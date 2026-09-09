// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
#include "windows/comptr.h"
#include <d3d11.h>

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
 ComPtr<ID3D11Texture2D> color,mask;
 ComPtr<ID3D11ShaderResourceView> colorView,maskView;
};
// Copy immutable original-frame native color/mask before publishing its receipt.
// No CPU readback, flush or wait. Caller owns wrapped-resource acquire/release.
inline bool CaptureRemakeOverlay(ID3D11Device* device,ID3D11DeviceContext* context,
 ID3D11Texture2D* color,ID3D11Texture2D* mask,std::uint64_t frame,
 const ProducerIdentity& producer,RemakeOverlaySnapshot& output) {
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
 if(FAILED(device->CreateTexture2D(&c,nullptr,&snapshot.color.get()))
  ||FAILED(device->CreateTexture2D(&m,nullptr,&snapshot.mask.get()))
  ||FAILED(device->CreateShaderResourceView(snapshot.color,nullptr,&snapshot.colorView.get()))
  ||FAILED(device->CreateShaderResourceView(snapshot.mask,nullptr,&snapshot.maskView.get())))return false;
 context->CopyResource(snapshot.color,color);context->CopyResource(snapshot.mask,mask);
 output=std::move(snapshot);return true;
}
}
