// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "pvr_scene_capture.h"
#include "remake_effect_identity.h"
#include <d3d11.h>
#include "windows/comptr.h"
#include <cstdlib>
#include <cstring>

namespace flycast::rend::neural {
inline bool RemakeEffectEvidenceRequested() {
 const auto* value=std::getenv("FLYCAST_REMAKE_EFFECT_IDENTITY");
 return value&&std::strcmp(value,"1")==0;
}
inline bool RemakeNativeEffectsRequested() {
 const auto* value=std::getenv("FLYCAST_REMAKE_NATIVE_EFFECTS");
 return value&&std::strcmp(value,"1")==0;
}
// Copy-based first integration. Original depth rejection, fragment ordering,
// colors, modifiers and blend selection are retained, not reconstructed from
// the final native image. Multi-pass and non-autosorted sources are not covered.
class RemakeOitEffects {
 static bool SameDevice(ID3D11Device* a,ID3D11Device* b) {
  if(!a||!b)return false;
  ComPtr<IUnknown> ai,bi;
  return SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&ai.get())))
   &&SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&bi.get())))&&ai==bi;
 }
 ProducerIdentity producer;
 std::uint64_t logicalBytes=0;
 std::uint32_t resolverLayers=0, resolverVariant=0;
 ComPtr<ID3D11Buffer> pixels,parameters,constants;
 ComPtr<ID3D11Texture2D> pointers;
 ComPtr<ID3D11Texture2D> evidenceNativeBackground;
 ComPtr<ID3D11UnorderedAccessView> pixelView;
 ComPtr<ID3D11ShaderResourceView> parameterView;
 ComPtr<ID3D11PixelShader> resolve;
 ComPtr<ID3D11VertexShader> vertex;
public:
 // Six owned data objects: three buffers, pointer texture, UAV and SRV.
 // Resolver shaders are borrowed from the native cache, not newly allocated.
 std::uint32_t ObjectCount()const noexcept {
  return (pixels?1u:0u)+(parameters?1u:0u)+(constants?1u:0u)
   +(pointers?1u:0u)+(pixelView?1u:0u)+(parameterView?1u:0u)+(evidenceNativeBackground?1u:0u);
 }
 std::uint64_t LogicalBytes()const noexcept {return logicalBytes;}
 // Developer-only native resolver input, retained at the same source boundary.
 bool RetainNativeBackgroundForEvidence(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* input) {
  if(!device||!context||!input||!pixels||evidenceNativeBackground||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
  ComPtr<ID3D11Device> contextOwner,inputOwner,pixelOwner;
  context->GetDevice(&contextOwner.get());input->GetDevice(&inputOwner.get());pixels->GetDevice(&pixelOwner.get());
  ComPtr<ID3D11DeviceContext> factory;device->GetImmediateContext(&factory.get());
  ComPtr<ID3D11Device> factoryOwner;if(!factory)return false;factory->GetDevice(&factoryOwner.get());
  if(!SameDevice(contextOwner,factoryOwner)
   ||(!SameDevice(inputOwner,device)&&!SameDevice(inputOwner,contextOwner))
   ||(!SameDevice(pixelOwner,device)&&!SameDevice(pixelOwner,contextOwner)))return false;
  D3D11_TEXTURE2D_DESC desc{};input->GetDesc(&desc);
  if(desc.Width<640||desc.Height<480||desc.MipLevels!=1||desc.ArraySize!=1||desc.SampleDesc.Count!=1
   ||(desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM))return false;
  desc.Width=640;desc.Height=480;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=desc.MiscFlags=0;
  desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  if(FAILED(device->CreateTexture2D(&desc,nullptr,&evidenceNativeBackground.get())))return false;
  const D3D11_BOX box{0,0,0,640,480,1};context->CopySubresourceRegion(evidenceNativeBackground,0,0,0,0,input,0,&box);
  logicalBytes+=640u*480*4;return true;
 }
 ID3D11Texture2D* NativeBackgroundForEvidence(const ProducerIdentity& source)const {
  return Matches(source)?static_cast<ID3D11Texture2D*>(evidenceNativeBackground):nullptr;
 }
 bool Matches(const ProducerIdentity& source)const noexcept {
  return producer.Available()&&source.Available()&&source.epoch==producer.epoch
   &&source.ordinal==producer.ordinal&&source.cycle==producer.cycle;
 }
 static std::shared_ptr<RemakeOitEffects> Capture(ID3D11Device* device,
  ID3D11DeviceContext* context,const ProducerIdentity& producer,
  ID3D11Buffer* pixels,ID3D11Texture2D* pointers,ID3D11Buffer* parameters,
  ID3D11Buffer* constants,ID3D11PixelShader* resolve,ID3D11VertexShader* vertex,std::string* error=nullptr,
  std::uint32_t layers=0,std::uint32_t variant=0) {
  const auto fail=[&](const std::string& why)->std::shared_ptr<RemakeOitEffects>{if(error)*error=why;return {};};
  if(!device||!context||!producer.Available()||!pixels||!pointers||!parameters
   ||!constants||!resolve||!vertex||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
   return fail("missing-input constants="+std::to_string(constants!=nullptr)+" producer="+std::to_string(producer.Available()));
  ComPtr<ID3D11DeviceContext> factoryContext;
  ComPtr<ID3D11Device> contextOwner,factoryOwner;
  device->GetImmediateContext(&factoryContext.get());
  if(!factoryContext)return fail("missing-factory-context");
  context->GetDevice(&contextOwner.get());factoryContext->GetDevice(&factoryOwner.get());
  if(!SameDevice(contextOwner,factoryOwner))return fail("context-factory-owner");
  // Shader handles come from this renderer's already-used native resolver.
  // The installed shader interceptor does not report the resource device
  // identity through shader GetDevice (live probe as, resource4). Do not apply
  // that query to shaders; validate all copied data and the factory/context.
  ID3D11DeviceChild* resources[]={pixels,pointers,parameters,constants,context};
  unsigned resourceIndex=0;
  for(auto* resource : resources) {
   // Supplied interception may expose a factory facade and its context's
   // device as distinct COM identities. Both must be tied to this context;
   // merely sharing an adapter or D3D12 device is not sufficient.
   ComPtr<ID3D11Device> owner;resource->GetDevice(&owner.get());
   if(!SameDevice(owner,device)&&!SameDevice(owner,contextOwner))
    return fail("device-owner resource="+std::to_string(resourceIndex));
   ++resourceIndex;
  }
  D3D11_BUFFER_DESC pd{},td{},cd{};
  pixels->GetDesc(&pd);parameters->GetDesc(&td);constants->GetDesc(&cd);
  D3D11_TEXTURE2D_DESC pointerDesc{};pointers->GetDesc(&pointerDesc);
  if(!pd.ByteWidth||pd.ByteWidth>512u*1024*1024||pd.StructureByteStride!=16
   ||!td.ByteWidth||td.ByteWidth>8192*8||!cd.ByteWidth||cd.ByteWidth>4096
   ||pointerDesc.Width<640||pointerDesc.Height<480||pointerDesc.Width>4096||pointerDesc.Height>4096
   ||pointerDesc.Format!=DXGI_FORMAT_R32_UINT||pointerDesc.MipLevels!=1
   ||pointerDesc.ArraySize!=1||pointerDesc.SampleDesc.Count!=1)
   return fail("resource-bound pixels="+std::to_string(pd.ByteWidth)+" stride="+std::to_string(pd.StructureByteStride)
    +" parameters="+std::to_string(td.ByteWidth)+" constants="+std::to_string(cd.ByteWidth));
  auto owned=std::make_shared<RemakeOitEffects>();
  owned->resolverLayers=layers;owned->resolverVariant=variant;
  owned->producer=producer;resolve->AddRef();vertex->AddRef();
  owned->resolve.reset(resolve);owned->vertex.reset(vertex);
  pd.Usage=td.Usage=cd.Usage=D3D11_USAGE_DEFAULT;
  pd.CPUAccessFlags=td.CPUAccessFlags=cd.CPUAccessFlags=0;
  // Native OIT allocation grows for RTTs and never shrinks. Retain only the
  // exact main content rectangle, not the larger backing allocation.
  pointerDesc.Width=640;pointerDesc.Height=480;
  owned->logicalBytes=std::uint64_t(pd.ByteWidth)+td.ByteWidth+cd.ByteWidth+640u*480*4;
  HRESULT hr=device->CreateBuffer(&pd,nullptr,&owned->pixels.get());
  if(FAILED(hr))return fail("create-pixels hr="+std::to_string(hr));
  hr=device->CreateBuffer(&td,nullptr,&owned->parameters.get());
  if(FAILED(hr))return fail("create-parameters hr="+std::to_string(hr));
  hr=device->CreateBuffer(&cd,nullptr,&owned->constants.get());
  if(FAILED(hr))return fail("create-constants hr="+std::to_string(hr));
  hr=device->CreateTexture2D(&pointerDesc,nullptr,&owned->pointers.get());
  if(FAILED(hr))return fail("create-pointers hr="+std::to_string(hr));
  D3D11_UNORDERED_ACCESS_VIEW_DESC uv{};uv.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
  uv.Buffer.NumElements=pd.ByteWidth/16;
  hr=device->CreateUnorderedAccessView(owned->pixels,&uv,&owned->pixelView.get());
  if(FAILED(hr))return fail("create-pixel-view hr="+std::to_string(hr));
  hr=device->CreateShaderResourceView(owned->parameters,nullptr,&owned->parameterView.get());
  if(FAILED(hr))return fail("create-parameter-view hr="+std::to_string(hr));
  context->CopyResource(owned->pixels,pixels);context->CopyResource(owned->parameters,parameters);
  context->CopyResource(owned->constants,constants);
  const D3D11_BOX content{0,0,0,640,480,1};
  context->CopySubresourceRegion(owned->pointers,0,0,0,0,pointers,0,&content);
  return owned;
 }
 // Explicit synchronous evidence API. Never called by Capture/Compose or the
 // ordinary render loop. Large staging copies are not performance evidence.
 bool ReadIdentityForEvidence(ID3D11Device* device,ID3D11DeviceContext* context,
  const ProducerIdentity& source,std::vector<std::uint32_t>& identity,std::string& error)const {
  identity.clear();
  const auto fail=[&](const char* why){error=why;return false;};
  if(!device||!context||!Matches(source)||!resolverLayers||resolverLayers>256||resolverVariant>1
   ||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return fail("effect-evidence-contract");
  ComPtr<ID3D11DeviceContext> factory;device->GetImmediateContext(&factory.get());
  ComPtr<ID3D11Device> owner,factoryOwner,pixelOwner;
  context->GetDevice(&owner.get());if(!factory)return fail("effect-evidence-factory");
  factory->GetDevice(&factoryOwner.get());pixels->GetDevice(&pixelOwner.get());
  if(!SameDevice(owner,factoryOwner)||(!SameDevice(pixelOwner,owner)&&!SameDevice(pixelOwner,device)))
   return fail("effect-evidence-device");
  auto readBuffer=[&](ID3D11Buffer* buffer,void* destination,std::size_t bytes){
   D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
   if(desc.ByteWidth!=bytes)return false;
   desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   desc.MiscFlags=0;desc.StructureByteStride=0;
   ComPtr<ID3D11Buffer> staging;if(FAILED(device->CreateBuffer(&desc,nullptr,&staging.get())))return false;
   context->CopyResource(staging,buffer);D3D11_MAPPED_SUBRESOURCE map{};
   if(FAILED(context->Map(staging,0,D3D11_MAP_READ,0,&map)))return false;
   std::memcpy(destination,map.pData,bytes);context->Unmap(staging,0);return true;
  };
  D3D11_BUFFER_DESC pd{},td{},cd{};pixels->GetDesc(&pd);parameters->GetDesc(&td);constants->GetDesc(&cd);
  if(pd.ByteWidth%sizeof(EffectIdentityPixel)||td.ByteWidth%sizeof(EffectIdentityPoly)||cd.ByteWidth%4)
   return fail("effect-evidence-buffer-layout");
  std::vector<EffectIdentityPixel> px(pd.ByteWidth/sizeof(EffectIdentityPixel));
  std::vector<EffectIdentityPoly> pp(td.ByteWidth/sizeof(EffectIdentityPoly));
  std::vector<std::uint32_t> state(cd.ByteWidth/4),heads(640*480);
  if(!readBuffer(pixels,px.data(),pd.ByteWidth)||!readBuffer(parameters,pp.data(),td.ByteWidth)
   ||!readBuffer(constants,state.data(),cd.ByteWidth))return fail("effect-evidence-buffer-read");
  D3D11_TEXTURE2D_DESC desc{};pointers->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;
  desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
  ComPtr<ID3D11Texture2D> staging;
  if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging.get())))return fail("effect-evidence-pointer-create");
  context->CopyResource(staging,pointers);D3D11_MAPPED_SUBRESOURCE map{};
  if(FAILED(context->Map(staging,0,D3D11_MAP_READ,0,&map)))return fail("effect-evidence-pointer-read");
  for(unsigned y=0;y<480;++y)std::memcpy(heads.data()+y*640,
   static_cast<const unsigned char*>(map.pData)+y*map.RowPitch,640*4);
  context->Unmap(staging,0);
  // Version of Flycast's resolver contract plus the selected dithering variant.
  state.insert(state.begin(),{1,resolverVariant});
  return CanonicalEffectIdentity({source.epoch,source.ordinal,source.cycle},heads,px,pp,state,
   resolverLayers,identity,error);
 }
 bool Compose(ID3D11Device* device,ID3D11DeviceContext* immediate,
  const ProducerIdentity& source,ID3D11Texture2D* background,
  ComPtr<ID3D11Texture2D>& output,ComPtr<ID3D11ShaderResourceView>& outputView)const {
  if(!Matches(source)||!device||!immediate||!background||immediate->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
  ComPtr<ID3D11DeviceContext> factoryContext;
  ComPtr<ID3D11Device> contextOwner,factoryOwner;
  device->GetImmediateContext(&factoryContext.get());if(!factoryContext)return false;
  immediate->GetDevice(&contextOwner.get());factoryContext->GetDevice(&factoryOwner.get());
  if(!SameDevice(contextOwner,factoryOwner))return false;
  ID3D11DeviceChild* resources[]={pixels,background,immediate};
  for(auto* resource : resources) {
   ComPtr<ID3D11Device> owner;resource->GetDevice(&owner.get());
   if(!SameDevice(owner,device)&&!SameDevice(owner,contextOwner))return false;
  }
  D3D11_TEXTURE2D_DESC desc{};background->GetDesc(&desc);
  if(desc.Width!=640||desc.Height!=480||desc.MipLevels!=1||desc.ArraySize!=1
   ||desc.SampleDesc.Count!=1||(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM
    &&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM))return false;
  ComPtr<ID3D11Texture2D> color,workingPointers;
  ComPtr<ID3D11ShaderResourceView> colorView,backgroundView;
  ComPtr<ID3D11RenderTargetView> target;
  ComPtr<ID3D11UnorderedAccessView> pointerView;
  desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=desc.MiscFlags=0;
  desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  if(FAILED(device->CreateTexture2D(&desc,nullptr,&color.get()))
   ||FAILED(device->CreateShaderResourceView(color,nullptr,&colorView.get()))
   ||FAILED(device->CreateRenderTargetView(color,nullptr,&target.get()))
   ||FAILED(device->CreateShaderResourceView(background,nullptr,&backgroundView.get())))return false;
  pointers->GetDesc(&desc);
  if(FAILED(device->CreateTexture2D(&desc,nullptr,&workingPointers.get()))
   ||FAILED(device->CreateUnorderedAccessView(workingPointers,nullptr,&pointerView.get())))return false;
  ComPtr<ID3D11DeviceContext> commands;
  ComPtr<ID3D11CommandList> list;
  ComPtr<ID3D11SamplerState> sampler;
  D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
  sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
  ComPtr<ID3D11RasterizerState> raster;
  D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
  if(FAILED(device->CreateDeferredContext(0,&commands.get()))
   ||FAILED(device->CreateSamplerState(&sd,&sampler.get()))
   ||FAILED(device->CreateRasterizerState(&rd,&raster.get())))return false;
  commands->CopyResource(workingPointers,pointers);
  ID3D11UnorderedAccessView* uavs[]={pixelView,pointerView};
  commands->OMSetRenderTargetsAndUnorderedAccessViews(1,&target.get(),nullptr,2,2,uavs,nullptr);
  commands->PSSetShaderResources(0,1,&backgroundView.get());
  ID3D11ShaderResourceView* params=parameterView;
  ID3D11Buffer* globals=constants;
  commands->PSSetShaderResources(5,1,&params);
  commands->PSSetConstantBuffers(0,1,&globals);
  commands->PSSetSamplers(0,1,&sampler.get());
  commands->PSSetShader(resolve,nullptr,0);commands->VSSetShader(vertex,nullptr,0);
  commands->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  D3D11_VIEWPORT viewport{0,0,640,480,0,1};commands->RSSetViewports(1,&viewport);
  commands->RSSetState(raster);
  commands->Draw(4,0);
  if(FAILED(commands->FinishCommandList(FALSE,&list.get())))return false;
  immediate->ExecuteCommandList(list,TRUE); // Preserve caller's complete pipeline state.
  output=std::move(color);outputView=std::move(colorView);return true;
 }
};
struct RemakeEffectsResources {
 std::uint32_t snapshots=0,objects=0;
 std::uint64_t logicalBytes=0;
};
template<std::size_t N>
RemakeEffectsResources CountRemakeEffects(const std::array<const RemakeOitEffects*,N>& owners)noexcept {
 RemakeEffectsResources result;
 for(std::size_t i=0;i<N;++i) {
  if(!owners[i])continue;
  bool duplicate=false;for(std::size_t j=0;j<i;++j)duplicate|=owners[j]==owners[i];
  if(duplicate)continue;
  ++result.snapshots;result.objects+=owners[i]->ObjectCount();result.logicalBytes+=owners[i]->LogicalBytes();
 }
 return result;
}
}
