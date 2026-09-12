// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
#include "remake_oit_effects.h"
#include "remake_native_effects.h"
#include "remake_temporal_scene.h"
#include "remake_native_resource.h"
#include "windows/comptr.h"
#include <d3d11.h>
#include <memory>

namespace flycast::rend::neural {
struct RemakeOverlayIdentity {
 RemakeChannelReceipt receipt;
 std::uint64_t frame=0;
 ProducerIdentity producer;
 bool Matches(const RemakeReturnedImage& image,std::uint64_t currentFrame,
  const ProducerIdentity& current,std::uint64_t maxAge=8)const noexcept {
  return frame&&receipt.sequence&&receipt.bytes&&producer.Available()&&current.Available()
   &&current.ordinal>=producer.ordinal&&current.cycle>=producer.cycle
   &&frame<=currentFrame&&currentFrame-frame<=maxAge&&producer.epoch==current.epoch
   &&image.frame==frame&&image.producer.epoch==producer.epoch
   &&image.producer.ordinal==producer.ordinal&&image.producer.cycle==producer.cycle
   &&image.source.sequence==receipt.sequence&&image.source.digest==receipt.digest
   &&image.source.bytes==receipt.bytes;
 }
};
struct RemakeOverlaySnapshot {
 RemakeOverlayIdentity identity;
 // Only retained under explicit developer capture/replay, never ordinary play.
 std::shared_ptr<const remake::Packet> captureScene;
 // Explicit temporal preparation only; geometry/generations, never DDS payloads.
 std::shared_ptr<const RemakeTemporalScene> temporalScene;
 std::uint64_t replayOriginalFrame=0;
 std::shared_ptr<const RemakeOitEffects> effects;
 std::shared_ptr<const class NativeEffectSnapshot> normalEffects;
 std::vector<AlphaEffectSelection> alphaEffectSelections;
 bool EffectsMatch(const ProducerIdentity& source)const noexcept {
  if(bool(effects)==bool(normalEffects))return false;
  return effects?effects->Matches(source):normalEffects->Matches(source);
 }
 bool ComposeEffects(ID3D11Device* device,ID3D11DeviceContext* context,
  const ProducerIdentity& source,ID3D11Texture2D* input,
  ComPtr<ID3D11Texture2D>& output,ComPtr<ID3D11ShaderResourceView>& view)const {
  if(!EffectsMatch(source)||!device||!context)return false;
  if(effects)return effects->Compose(device,context,source,input,output,view,alphaEffectSelections);
  auto composed=normalEffects->Compose(context,source,input,alphaEffectSelections);
  if(!composed)return false;
  ComPtr<ID3D11Device> owner;composed->GetDevice(&owner.get());
  if(owner.get()!=device)return false;
  ComPtr<ID3D11ShaderResourceView> resultView;
  if(FAILED(device->CreateShaderResourceView(composed,nullptr,&resultView.get())))return false;
  output=std::move(composed);view=std::move(resultView);return true;
 }

 ComPtr<ID3D11Texture2D> color,mask;
 ComPtr<ID3D11ShaderResourceView> colorView,maskView;
 // LOG910: when the copies came from a pool, every copy of this snapshot
 // shares the lease and the textures retire to the pool only when the last
 // copy is gone, so a pooled texture is never handed out while still read.
 std::shared_ptr<void> lease;
};
// Copy immutable original-frame native color/mask before publishing its receipt.
// No CPU readback, flush or wait. Caller owns wrapped-resource acquire/release.
// With a pool serving the device the two textures are reused allocations.
inline bool CaptureRemakeOverlay(ID3D11Device* device,ID3D11DeviceContext* context,
 ID3D11Texture2D* color,ID3D11Texture2D* mask,std::uint64_t frame,
 const ProducerIdentity& producer,RemakeOverlaySnapshot& output,
 const std::shared_ptr<NativeResourcePool>& pool={}) {
 if(!device||!context||!color||!mask||!frame||!producer.Available()
  ||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
 D3D11_TEXTURE2D_DESC c{},m{};color->GetDesc(&c);mask->GetDesc(&m);
 if(!c.Width||!c.Height||c.Width>4096||c.Height>4096||c.Width!=m.Width||c.Height!=m.Height
  ||c.MipLevels!=1||m.MipLevels!=1||c.ArraySize!=1||m.ArraySize!=1
  ||c.SampleDesc.Count!=1||m.SampleDesc.Count!=1
  ||(c.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&c.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)
  ||(m.Format!=DXGI_FORMAT_R8_UNORM&&m.Format!=DXGI_FORMAT_R8G8B8A8_UNORM))return false;
 ComPtr<ID3D11Device> cd,md,xd;color->GetDevice(&cd.get());mask->GetDevice(&md.get());context->GetDevice(&xd.get());
 if(cd.get()!=device||md.get()!=device||xd.get()!=device)return false;
 RemakeOverlaySnapshot snapshot;snapshot.identity.frame=frame;snapshot.identity.producer=producer;
 for(auto* desc:{&c,&m}) {desc->Usage=D3D11_USAGE_DEFAULT;desc->BindFlags=D3D11_BIND_SHADER_RESOURCE;
  desc->CPUAccessFlags=0;desc->MiscFlags=0;}
 const bool pooled=pool&&pool->Serves(device);
 if(pooled){snapshot.color=pool->AcquireTexture(device,c);snapshot.mask=pool->AcquireTexture(device,m);}
 else if(FAILED(device->CreateTexture2D(&c,nullptr,&snapshot.color.get()))
  ||FAILED(device->CreateTexture2D(&m,nullptr,&snapshot.mask.get())))return false;
 if(!snapshot.color||!snapshot.mask
  ||FAILED(device->CreateShaderResourceView(snapshot.color,nullptr,&snapshot.colorView.get()))
  ||FAILED(device->CreateShaderResourceView(snapshot.mask,nullptr,&snapshot.maskView.get())))return false;
 if(pooled) {
  struct Lease {
   std::shared_ptr<NativeResourcePool> pool;ComPtr<ID3D11Texture2D> color,mask;
   ~Lease(){pool->Retire(color.get());pool->Retire(mask.get());}
  };
  auto lease=std::make_shared<Lease>();lease->pool=pool;lease->color=snapshot.color;lease->mask=snapshot.mask;
  snapshot.lease=std::move(lease);
 }
 context->CopyResource(snapshot.color,color);context->CopyResource(snapshot.mask,mask);
 output=std::move(snapshot);return true;
}
}
