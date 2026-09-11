// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_native_draw.h"
#include "remake_native_context.h"
#include "producer_identity.h"
#include "remake_alpha_ownership.h"
#include <vector>
#include <chrono>
namespace flycast::rend::neural {
// Prototype single-pass native-background proof. Not wired to returned output.
class NativeEffectSnapshot {
 ProducerIdentity producer{};
 ComPtr<ID3D11Device> owner;
 ComPtr<ID3D11Texture2D> background,depth;
 D3D11_RENDER_TARGET_VIEW_DESC colorView{};
 D3D11_DEPTH_STENCIL_VIEW_DESC depthView{};
 std::vector<std::unique_ptr<NativeEffectDraw>> draws;
 std::vector<std::uint32_t> drawOrdinals;
 std::vector<EffectIdentityPoly> nativeParameters;
 NativeGeometryCopies geometryCopies;bool stableGeometry=false;
 NativeViewCopies viewCopies;bool stableViews=false;
 double captureMs=0;
 bool valid=false,sealed=false;
public:
 static std::unique_ptr<NativeEffectSnapshot> Begin(ID3D11Device* device,
  ID3D11DeviceContext* context,const ProducerIdentity& source,
  ID3D11RenderTargetView* color,ID3D11DepthStencilView* depthTarget,
  const std::vector<EffectIdentityPoly>& parameters={},bool geometryStableForPass=false,bool viewsStableForPass=false) {
  if(!source.Available()||!color||!depthTarget)return {};
  const auto started=std::chrono::steady_clock::now();
  auto result=std::make_unique<NativeEffectSnapshot>();
  ComPtr<ID3D11Resource> colorResource,depthResource;
  color->GetResource(&colorResource.get());depthTarget->GetResource(&depthResource.get());
  ComPtr<ID3D11Texture2D> colorTexture,depthTexture;
  if(FAILED(colorResource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&colorTexture.get())))
   ||FAILED(depthResource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&depthTexture.get()))))return {};
  result->background=CopyNativeEffectTexture(device,context,colorTexture);
  result->depth=CopyNativeEffectTexture(device,context,depthTexture);
  if(!result->background||!result->depth)return {};
  color->GetDesc(&result->colorView);depthTarget->GetDesc(&result->depthView);
  context->GetDevice(&result->owner.get());result->producer=source;result->stableGeometry=geometryStableForPass;result->stableViews=viewsStableForPass;result->nativeParameters=parameters;result->valid=true;
  result->captureMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
  return result;
 }
 double CaptureMilliseconds()const noexcept{return captureMs;}
 D3D11_TEXTURE2D_DESC RasterDescription()const noexcept {
  D3D11_TEXTURE2D_DESC desc{};if(background)background->GetDesc(&desc);return desc;
 }
 bool Append(ID3D11DeviceContext* context,UINT count,UINT start,INT base,std::uint32_t ordinal=UINT32_MAX){
  if(!valid||sealed)return false;
  const auto started=std::chrono::steady_clock::now();
  auto draw=NativeEffectDraw::Capture(owner,context,count,start,base,stableGeometry?&geometryCopies:nullptr,stableViews?&viewCopies:nullptr);
  captureMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
  if(!draw){valid=false;draws.clear();return false;}
  draws.push_back(std::move(draw));drawOrdinals.push_back(ordinal);return true;
 }
 bool Seal(std::size_t expectedDraws){
  if(!valid||sealed||draws.size()!=expectedDraws){valid=false;return false;}
  sealed=true;return true;
 }
 std::uint32_t OwnedObjects()const noexcept {
  std::uint32_t count=(background?1u:0u)+(depth?1u:0u)+static_cast<std::uint32_t>(geometryCopies.copies.size())+2u*static_cast<std::uint32_t>(viewCopies.copies.size());
  for(const auto& draw:draws)count+=draw->OwnedObjects();
  return count;
 }
 bool Matches(const ProducerIdentity& source)const noexcept {
  return valid&&sealed&&source.Available()&&source.epoch==producer.epoch
   &&source.ordinal==producer.ordinal&&source.cycle==producer.cycle;
 }
 ComPtr<ID3D11Texture2D> ReplayNative(ID3D11DeviceContext* context,
  const ProducerIdentity& source)const {
  return Compose(context,source,background);
 }
 // Returned color must have the same raster layout as this source. Never
 // resize/reinterpret it implicitly or write into the caller-owned image.
 ComPtr<ID3D11Texture2D> Compose(ID3D11DeviceContext* context,
  const ProducerIdentity& source,ID3D11Texture2D* input,
  const std::vector<AlphaEffectSelection>& selected={})const {
  if(!Matches(source)||!NativeCaptureContextMatches(owner,context,input))return {};
  std::vector<AlphaEffectSelection> exclusions;
  if(!selected.empty()){
   if(!PlanAlphaEffectExclusion(producer,source,nativeParameters,selected,exclusions))return {};
   for(auto ordinal:drawOrdinals)if(ordinal>=nativeParameters.size())return {};
  }
  D3D11_TEXTURE2D_DESC expected{},actual{};background->GetDesc(&expected);input->GetDesc(&actual);
  if(actual.Width!=expected.Width||actual.Height!=expected.Height||actual.Format!=expected.Format
   ||actual.MipLevels!=expected.MipLevels||actual.ArraySize!=expected.ArraySize
   ||actual.SampleDesc.Count!=expected.SampleDesc.Count||actual.SampleDesc.Quality!=expected.SampleDesc.Quality)return {};
  expected.Usage=D3D11_USAGE_DEFAULT;expected.CPUAccessFlags=0;expected.MiscFlags=0;
  expected.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  ComPtr<ID3D11Texture2D> color;
  if(FAILED(owner->CreateTexture2D(&expected,nullptr,&color.get())))return {};
  context->CopyResource(color,input);
  auto workingDepth=CopyNativeEffectTexture(owner,context,depth);
  if(!color||!workingDepth)return {};
  ComPtr<ID3D11RenderTargetView> target;ComPtr<ID3D11DepthStencilView> depthTarget;
  if(FAILED(owner->CreateRenderTargetView(color,&colorView,&target.get()))
   ||FAILED(owner->CreateDepthStencilView(workingDepth,&depthView,&depthTarget.get())))return {};
  auto scope=NativeEffectContextScope::Enter(context);if(!scope)return {};
  context->OMSetRenderTargets(1,&target.get(),depthTarget);
  for(std::size_t i=0;i<draws.size();++i){
   bool exclude=false;for(const auto& item:exclusions)exclude|=item.ordinal==drawOrdinals[i];
   if(!draws[i]->Replay(context,exclude))return {};
  }
  return color;
 }
};
}
