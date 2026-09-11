// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_native_resource.h"
#include <array>
#include <d3d11_1.h>
#include <memory>
namespace flycast::rend::neural {
// Source-owned DrawIndexed state. Caller owns producer qualification, output
// color/depth snapshots and state restoration. Capture is not publication.
struct NativeEffectDraw {
 ComPtr<ID3D11Device> owner;
 ComPtr<ID3D11InputLayout> layout;
 ComPtr<ID3D11VertexShader> vs;
 ComPtr<ID3D11PixelShader> ps;
 ComPtr<ID3D11BlendState> blend;
 ComPtr<ID3D11DepthStencilState> depth;
 ComPtr<ID3D11RasterizerState> raster;
 ComPtr<ID3D11Buffer> indices;
 DXGI_FORMAT indexFormat=DXGI_FORMAT_UNKNOWN;UINT indexOffset=0;
 D3D11_PRIMITIVE_TOPOLOGY topology=D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
 std::array<ComPtr<ID3D11Buffer>,32> vertices;
 std::array<UINT,32> strides{},offsets{};
 std::array<D3D11_VIEWPORT,16> viewports{};
 std::array<D3D11_RECT,16> scissors{};
 UINT viewportCount=16,scissorCount=16,stencilRef=0,sampleMask=0;
 FLOAT blendFactor[4]{};
 UINT count=0,start=0;INT base=0;
 bool sharedGeometry=false,sharedViews=false;
 std::array<ComPtr<ID3D11Buffer>,14> vsConstants;
 std::array<ComPtr<ID3D11ShaderResourceView>,128> vsViews;
 std::array<ComPtr<ID3D11SamplerState>,16> vsSamplers;
 std::array<ComPtr<ID3D11Buffer>,14> psConstants;
 std::array<ComPtr<ID3D11ShaderResourceView>,128> psViews;
 std::array<ComPtr<ID3D11SamplerState>,16> psSamplers;
 // Newly allocated snapshot resources only. Shared shader/state/sampler
 // references are owned by the renderer and must not be counted twice.
 std::uint32_t OwnedObjects()const noexcept {
  std::uint32_t count=0;
  if(!sharedGeometry){count=indices?1u:0u;for(const auto& item:vertices)count+=item?1u:0u;}
  for(const auto& item:vsConstants)count+=item?1u:0u;
  for(const auto& item:psConstants)count+=item?1u:0u;
  if(!sharedViews)for(const auto& item:vsViews)count+=item?2u:0u; // copied texture + new view
  if(!sharedViews)for(const auto& item:psViews)count+=item?2u:0u;
  return count;
 }
 static std::unique_ptr<NativeEffectDraw> Capture(ID3D11Device* device,
  ID3D11DeviceContext* context,UINT count,UINT start,INT base,NativeGeometryCopies* geometry=nullptr,NativeViewCopies* views=nullptr) {
  if(!device||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return {};
  ComPtr<ID3D11Device> contextOwner;context->GetDevice(&contextOwner.get());
  if(contextOwner.get()!=device)return {};
  // Native DX11 draw path has no tessellation/geometry shaders, predication
  // or stream output. Fail instead of retaining only part of such a pipeline.
  ComPtr<ID3D11GeometryShader> gs;context->GSGetShader(&gs.get(),nullptr,nullptr);
  ComPtr<ID3D11HullShader> hs;context->HSGetShader(&hs.get(),nullptr,nullptr);
  ComPtr<ID3D11DomainShader> ds;context->DSGetShader(&ds.get(),nullptr,nullptr);
  ComPtr<ID3D11Predicate> predicate;BOOL predicateValue;
  context->GetPredication(&predicate.get(),&predicateValue);
  bool streamOutput=false;
  ID3D11Buffer* so[4]{};context->SOGetTargets(4,so);
  for(auto* buffer:so)if(buffer){streamOutput=true;buffer->Release();}
  if(gs||hs||ds||predicate||streamOutput)return {};
  // Pixel UAV side effects cannot be reproduced by a color/depth-only replay.
  const UINT uavSlots=device->GetFeatureLevel()>=D3D_FEATURE_LEVEL_11_1?64:8;
  ID3D11UnorderedAccessView* uavs[64]{};
  context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,uavSlots,uavs);
  bool hasUav=false;for(auto* uav:uavs)if(uav){hasUav=true;uav->Release();}
  if(hasUav)return {};
  ComPtr<ID3D11DeviceContext1> context1;
  context->QueryInterface(__uuidof(ID3D11DeviceContext1),reinterpret_cast<void**>(&context1.get()));
  auto result=std::make_unique<NativeEffectDraw>();auto& r=*result;
  r.sharedGeometry=geometry!=nullptr;r.sharedViews=views!=nullptr;r.owner=contextOwner;r.count=count;r.start=start;r.base=base;
  UINT vsInstances=0,psInstances=0;
  context->VSGetShader(&r.vs.get(),nullptr,&vsInstances);
  context->PSGetShader(&r.ps.get(),nullptr,&psInstances);
  if(vsInstances||psInstances||!r.vs||!r.ps)return {};
  context->IAGetInputLayout(&r.layout.get());
  context->IAGetPrimitiveTopology(&r.topology);
  ComPtr<ID3D11Buffer> index;
  context->IAGetIndexBuffer(&index.get(),&r.indexFormat,&r.indexOffset);
  r.indices=geometry?geometry->Get(device,context,index):CopyNativeEffectBuffer(device,context,index);if(!r.indices)return {};
  for(UINT i=0;i<32;++i){
   ComPtr<ID3D11Buffer> source;
   context->IAGetVertexBuffers(i,1,&source.get(),&r.strides[i],&r.offsets[i]);
   if(source){r.vertices[i]=geometry?geometry->Get(device,context,source):CopyNativeEffectBuffer(device,context,source);if(!r.vertices[i])return {};}
  }
  for(UINT i=0;i<14;++i){
   ComPtr<ID3D11Buffer> source;context->VSGetConstantBuffers(i,1,&source.get());
   if(source){
    if(context1){
     UINT first=0,num=0;ComPtr<ID3D11Buffer> ranged;
     context1->VSGetConstantBuffers1(i,1,&ranged.get(),&first,&num);
     D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
     if(first||num<desc.ByteWidth/16)return {};
    }
    r.vsConstants[i]=CopyNativeEffectBuffer(device,context,source);if(!r.vsConstants[i])return {};
   }
  }
  for(UINT i=0;i<128;++i){
   ComPtr<ID3D11ShaderResourceView> source;context->VSGetShaderResources(i,1,&source.get());
   if(source){r.vsViews[i]=views?views->Get(device,context,source):CopyNativeEffectView(device,context,source);if(!r.vsViews[i])return {};}
  }
  for(UINT i=0;i<16;++i)context->VSGetSamplers(i,1,&r.vsSamplers[i].get());
  for(UINT i=0;i<14;++i){
   ComPtr<ID3D11Buffer> source;context->PSGetConstantBuffers(i,1,&source.get());
   if(source){
    if(context1){
     UINT first=0,num=0;ComPtr<ID3D11Buffer> ranged;
     context1->PSGetConstantBuffers1(i,1,&ranged.get(),&first,&num);
     D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
     if(first||num<desc.ByteWidth/16)return {};
    }
    r.psConstants[i]=CopyNativeEffectBuffer(device,context,source);if(!r.psConstants[i])return {};
   }
  }
  for(UINT i=0;i<128;++i){
   ComPtr<ID3D11ShaderResourceView> source;context->PSGetShaderResources(i,1,&source.get());
   if(source){r.psViews[i]=views?views->Get(device,context,source):CopyNativeEffectView(device,context,source);if(!r.psViews[i])return {};}
  }
  for(UINT i=0;i<16;++i)context->PSGetSamplers(i,1,&r.psSamplers[i].get());
  context->OMGetBlendState(&r.blend.get(),r.blendFactor,&r.sampleMask);
  context->OMGetDepthStencilState(&r.depth.get(),&r.stencilRef);
  context->RSGetState(&r.raster.get());
  context->RSGetViewports(&r.viewportCount,r.viewports.data());
  context->RSGetScissorRects(&r.scissorCount,r.scissors.data());
  return result;
 }
 // Output/depth binding and restoration are required at the snapshot owner.
 bool Replay(ID3D11DeviceContext* context,bool suppressColor=false)const {
  if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
  ComPtr<ID3D11Device> device;context->GetDevice(&device.get());
  if(static_cast<ID3D11Device*>(device)!=static_cast<ID3D11Device*>(owner))return false;
  context->GSSetShader(nullptr,nullptr,0);context->HSSetShader(nullptr,nullptr,0);
  context->DSSetShader(nullptr,nullptr,0);context->SetPredication(nullptr,FALSE);
  context->SOSetTargets(0,nullptr,nullptr);
  context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);
  context->IASetInputLayout(layout);context->IASetPrimitiveTopology(topology);
  context->IASetIndexBuffer(indices,indexFormat,indexOffset);
  // Submit every slot, including nulls, in one call per binding class.
  // Raw arrays borrow the snapshot's ownership for this immediate submission.
  std::array<ID3D11Buffer*,32> vertexBindings{};
  for(UINT i=0;i<32;++i)vertexBindings[i]=vertices[i];
  context->IASetVertexBuffers(0,32,vertexBindings.data(),strides.data(),offsets.data());
  std::array<ID3D11Buffer*,14> vsBuffers{},psBuffers{};
  for(UINT i=0;i<14;++i){vsBuffers[i]=vsConstants[i];psBuffers[i]=psConstants[i];}
  context->VSSetConstantBuffers(0,14,vsBuffers.data());context->PSSetConstantBuffers(0,14,psBuffers.data());
  std::array<ID3D11ShaderResourceView*,128> vertexViews{},pixelViews{};
  for(UINT i=0;i<128;++i){vertexViews[i]=vsViews[i];pixelViews[i]=psViews[i];}
  context->VSSetShaderResources(0,128,vertexViews.data());context->PSSetShaderResources(0,128,pixelViews.data());
  std::array<ID3D11SamplerState*,16> vertexSamplers{},pixelSamplers{};
  for(UINT i=0;i<16;++i){vertexSamplers[i]=vsSamplers[i];pixelSamplers[i]=psSamplers[i];}
  context->VSSetSamplers(0,16,vertexSamplers.data());context->PSSetSamplers(0,16,pixelSamplers.data());
  ComPtr<ID3D11BlendState> excludedBlend;
  if(suppressColor){
   D3D11_BLEND_DESC desc{};
   if(blend)blend->GetDesc(&desc);
   else for(auto& target:desc.RenderTarget){
    target.SrcBlend=target.SrcBlendAlpha=D3D11_BLEND_ONE;
    target.DestBlend=target.DestBlendAlpha=D3D11_BLEND_ZERO;
    target.BlendOp=target.BlendOpAlpha=D3D11_BLEND_OP_ADD;
   }
   // Preserve depth/stencil and draw execution while excluding color ownership.
   for(auto& target:desc.RenderTarget)target.RenderTargetWriteMask=0;
   if(FAILED(device->CreateBlendState(&desc,&excludedBlend.get())))return false;
  }
  context->OMSetBlendState(suppressColor?static_cast<ID3D11BlendState*>(excludedBlend):static_cast<ID3D11BlendState*>(blend),blendFactor,sampleMask);
  context->OMSetDepthStencilState(depth,stencilRef);context->RSSetState(raster);
  context->RSSetViewports(viewportCount,viewports.data());context->RSSetScissorRects(scissorCount,scissors.data());
  context->DrawIndexed(count,start,base);return true;
 }
};
}
