// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include "remake_motion_shader.h"
#include "rend/neural/remake_motion_stream.h"

namespace flycast::rend::neural {
// Isolated deferred-context work preserves the caller's graphics state. Output
// ownership is explicit: retaining this object retains the corresponding draw IDs.
struct RemakeRasterOutput {
 std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>,4> textures;
 std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>,4> views;
};
class RemakeMotionRaster {
 template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
 Ptr<ID3D11Device> device;
 Ptr<ID3D11DeviceContext> context;
 Ptr<ID3D11VertexShader> vs;
 Ptr<ID3D11PixelShader> ps;
 Ptr<ID3D11InputLayout> layout;
 Ptr<ID3D11RasterizerState> raster;
 Ptr<ID3D11DepthStencilState> depthState;
public:
 unsigned OwnedObjects()const {
  return unsigned(bool(context))+unsigned(bool(vs))+unsigned(bool(ps))
   +unsigned(bool(layout))+unsigned(bool(raster))+unsigned(bool(depthState));
 }
 bool Initialize(ID3D11Device* incoming,decltype(&D3DCompile) compile,std::string& error) {
  if(device.Get()==incoming&&vs&&ps)return true;
  *this=RemakeMotionRaster{};
  if(!incoming||!compile){error="remake-raster-device-or-compiler";return false;}
  Ptr<ID3DBlob> vertex,pixel,diagnostics;
  auto shader=[&](const char* source,const char* target,Ptr<ID3DBlob>& result) {
   diagnostics.Reset();
   const auto hr=compile(source,std::strlen(source),"remake-motion",nullptr,nullptr,"main",target,
    D3DCOMPILE_ENABLE_STRICTNESS,0,result.GetAddressOf(),diagnostics.GetAddressOf());
   if(FAILED(hr))error=diagnostics?std::string(static_cast<const char*>(diagnostics->GetBufferPointer()),diagnostics->GetBufferSize()):"remake-raster-compile";
   return SUCCEEDED(hr);
  };
  if(!shader(RemakeMotionVertexShader,"vs_5_0",vertex)||!shader(RemakeMotionPixelShader,"ps_5_0",pixel))return false;
  D3D11_INPUT_ELEMENT_DESC elements[]={
   {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
   {"POSITION",1,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
   {"TEXCOORD",0,DXGI_FORMAT_R32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
   {"TEXCOORD",1,DXGI_FORMAT_R32_UINT,0,28,D3D11_INPUT_PER_VERTEX_DATA,0},
   {"TEXCOORD",2,DXGI_FORMAT_R32_UINT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
  static_assert(sizeof(RemakeMotionVertex)==36,"motion stream layout");
  D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
  D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
  if(FAILED(incoming->CreateDeferredContext(0,context.GetAddressOf()))
   ||FAILED(incoming->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,vs.GetAddressOf()))
   ||FAILED(incoming->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,ps.GetAddressOf()))
   ||FAILED(incoming->CreateInputLayout(elements,5,vertex->GetBufferPointer(),vertex->GetBufferSize(),layout.GetAddressOf()))
   ||FAILED(incoming->CreateRasterizerState(&rd,raster.GetAddressOf()))
   ||FAILED(incoming->CreateDepthStencilState(&dd,depthState.GetAddressOf()))) {
   error="remake-raster-pipeline-create";return false;
  }
  device=incoming;error.clear();return true;
 }
 bool Render(ID3D11DeviceContext* immediate,const RemakeMotionStream& stream,
  const std::vector<float>& currentDepth,const std::vector<float>& previousDepth,
  ID3D11ShaderResourceView* previousDrawIds,float nearPlane,float farPlane,
  float absoluteTolerance,float relativeTolerance,RemakeRasterOutput& output,std::string& error) {
  auto fail=[&](const char* why){error=why;return false;};
  if(!device||!immediate||stream.vertices.empty()||stream.vertices.size()>65536
   ||stream.indices.empty()||stream.indices.size()>262144||stream.indices.size()%3
   ||currentDepth.size()!=640*480||previousDepth.size()!=640*480
   ||!std::isfinite(nearPlane)||!std::isfinite(farPlane)||nearPlane<=0||farPlane<=nearPlane
   ||!std::isfinite(absoluteTolerance)||!std::isfinite(relativeTolerance)
   ||absoluteTolerance<0||relativeTolerance<0)return fail("remake-raster-input-bound");
  Ptr<ID3D11Device> owner;immediate->GetDevice(owner.GetAddressOf());
  if(owner.Get()!=device.Get())return fail("remake-raster-wrong-device");
  if(previousDrawIds) {
   owner.Reset();previousDrawIds->GetDevice(owner.GetAddressOf());
   if(owner.Get()!=device.Get())return fail("remake-raster-previous-wrong-device");
  }
  for(auto i:stream.indices)if(i>=stream.vertices.size())return fail("remake-raster-index");
  for(const auto& v:stream.vertices)
   if(!std::isfinite(v.currentScreen.x)||!std::isfinite(v.currentScreen.y)||!std::isfinite(v.currentScreen.z)
    ||!std::isfinite(v.previousScreen.x)||!std::isfinite(v.previousScreen.y)||!std::isfinite(v.previousScreen.z)
    ||v.currentScreen.z<=0||v.previousScreen.z<=0||!std::isfinite(v.confidence)
    ||v.currentDraw>128||v.previousDraw>128)return fail("remake-raster-vertex");
  for(std::size_t i=0;i<currentDepth.size();++i)
   if(!std::isfinite(currentDepth[i])||currentDepth[i]<0||currentDepth[i]>1
    ||!std::isfinite(previousDepth[i])||previousDepth[i]<0||previousDepth[i]>1)return fail("remake-raster-depth");
  auto texture=[&](DXGI_FORMAT format,UINT flags,const void* data,UINT pitch,
   Ptr<ID3D11Texture2D>& tex,Ptr<ID3D11ShaderResourceView>* view) {
   D3D11_TEXTURE2D_DESC d{};d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
   d.Format=format;d.BindFlags=flags;d.Usage=D3D11_USAGE_DEFAULT;
   D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;initial.SysMemPitch=pitch;
   return SUCCEEDED(device->CreateTexture2D(&d,data?&initial:nullptr,tex.GetAddressOf()))
    &&(!view||SUCCEEDED(device->CreateShaderResourceView(tex.Get(),nullptr,view->GetAddressOf())));
  };
  RemakeRasterOutput result;std::array<Ptr<ID3D11RenderTargetView>,4> targets;
  constexpr DXGI_FORMAT formats[]={DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R16_UINT,DXGI_FORMAT_R8_UNORM};
  for(unsigned i=0;i<4;++i)
   if(!texture(formats[i],D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,nullptr,0,result.textures[i],&result.views[i])
    ||FAILED(device->CreateRenderTargetView(result.textures[i].Get(),nullptr,targets[i].GetAddressOf())))return fail("remake-raster-target");
  Ptr<ID3D11Texture2D> depth,current,previous;Ptr<ID3D11ShaderResourceView> currentView,previousView;
  Ptr<ID3D11DepthStencilView> dsv;
  if(!texture(DXGI_FORMAT_D32_FLOAT,D3D11_BIND_DEPTH_STENCIL,nullptr,0,depth,nullptr)
   ||FAILED(device->CreateDepthStencilView(depth.Get(),nullptr,dsv.GetAddressOf()))
   ||!texture(DXGI_FORMAT_R32_FLOAT,D3D11_BIND_SHADER_RESOURCE,currentDepth.data(),640*4,current,&currentView)
   ||!texture(DXGI_FORMAT_R32_FLOAT,D3D11_BIND_SHADER_RESOURCE,previousDepth.data(),640*4,previous,&previousView))return fail("remake-raster-depth-create");
  auto buffer=[&](UINT flags,const void* data,UINT bytes,Ptr<ID3D11Buffer>& b) {
   D3D11_BUFFER_DESC d{};d.ByteWidth=bytes;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=flags;
   D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;
   return SUCCEEDED(device->CreateBuffer(&d,&initial,b.GetAddressOf()));
  };
  const float constants[]={640,480,nearPlane,farPlane,absoluteTolerance,relativeTolerance,0,0};
  Ptr<ID3D11Buffer> vertices,indices,contract;
  if(!buffer(D3D11_BIND_VERTEX_BUFFER,stream.vertices.data(),UINT(stream.vertices.size()*sizeof(RemakeMotionVertex)),vertices)
   ||!buffer(D3D11_BIND_INDEX_BUFFER,stream.indices.data(),UINT(stream.indices.size()*4),indices)
   ||!buffer(D3D11_BIND_CONSTANT_BUFFER,constants,sizeof(constants),contract))return fail("remake-raster-buffer");
  context->ClearState();
  const float zero[4]{},one[4]={1,1,1,1};
  for(unsigned i=0;i<4;++i)context->ClearRenderTargetView(targets[i].Get(),i==3?one:zero);
  context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
  ID3D11RenderTargetView* rt[]={targets[0].Get(),targets[1].Get(),targets[2].Get(),targets[3].Get()};
  context->OMSetRenderTargets(4,rt,dsv.Get());context->OMSetDepthStencilState(depthState.Get(),0);
  context->RSSetState(raster.Get());const D3D11_VIEWPORT viewport={0,0,640,480,0,1};context->RSSetViewports(1,&viewport);
  ID3D11Buffer* vb=vertices.Get();UINT stride=sizeof(RemakeMotionVertex),offset=0;
  context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetIndexBuffer(indices.Get(),DXGI_FORMAT_R32_UINT,0);
  context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
  ID3D11Buffer* cb=contract.Get();context->VSSetConstantBuffers(0,1,&cb);context->PSSetConstantBuffers(0,1,&cb);
  ID3D11ShaderResourceView* srvs[]={currentView.Get(),previousView.Get(),previousDrawIds};context->PSSetShaderResources(0,3,srvs);
  context->DrawIndexed(UINT(stream.indices.size()),0,0);
  Ptr<ID3D11CommandList> commands;
  if(FAILED(context->FinishCommandList(FALSE,commands.GetAddressOf())))return fail("remake-raster-command-list");
  immediate->ExecuteCommandList(commands.Get(),TRUE);
  output=std::move(result);error.clear();return true;
 }
};
}
