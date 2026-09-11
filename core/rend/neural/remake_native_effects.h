// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_native_draw.h"
#include "remake_native_context.h"
#include "producer_identity.h"
#include <vector>
namespace flycast::rend::neural {
// Prototype single-pass native-background proof. Not wired to returned output.
class NativeEffectSnapshot {
 ProducerIdentity producer{};
 ComPtr<ID3D11Device> owner;
 ComPtr<ID3D11Texture2D> background,depth;
 D3D11_RENDER_TARGET_VIEW_DESC colorView{};
 D3D11_DEPTH_STENCIL_VIEW_DESC depthView{};
 std::vector<std::unique_ptr<NativeEffectDraw>> draws;
 bool valid=false,sealed=false;
public:
 static std::unique_ptr<NativeEffectSnapshot> Begin(ID3D11Device* device,
  ID3D11DeviceContext* context,const ProducerIdentity& source,
  ID3D11RenderTargetView* color,ID3D11DepthStencilView* depthTarget) {
  if(!source.Available()||!color||!depthTarget)return {};
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
  context->GetDevice(&result->owner.get());result->producer=source;result->valid=true;
  return result;
 }
 bool Append(ID3D11DeviceContext* context,UINT count,UINT start,INT base){
  if(!valid||sealed)return false;
  auto draw=NativeEffectDraw::Capture(owner,context,count,start,base);
  if(!draw){valid=false;draws.clear();return false;}
  draws.push_back(std::move(draw));return true;
 }
 bool Seal(std::size_t expectedDraws){
  if(!valid||sealed||draws.size()!=expectedDraws){valid=false;return false;}
  sealed=true;return true;
 }
 bool Matches(const ProducerIdentity& source)const noexcept {
  return valid&&sealed&&source.Available()&&source.epoch==producer.epoch
   &&source.ordinal==producer.ordinal&&source.cycle==producer.cycle;
 }
 ComPtr<ID3D11Texture2D> ReplayNative(ID3D11DeviceContext* context,
  const ProducerIdentity& source)const {
  if(!Matches(source))return {};
  auto color=CopyNativeEffectTexture(owner,context,background);
  auto workingDepth=CopyNativeEffectTexture(owner,context,depth);
  if(!color||!workingDepth)return {};
  ComPtr<ID3D11RenderTargetView> target;ComPtr<ID3D11DepthStencilView> depthTarget;
  if(FAILED(owner->CreateRenderTargetView(color,&colorView,&target.get()))
   ||FAILED(owner->CreateDepthStencilView(workingDepth,&depthView,&depthTarget.get())))return {};
  auto scope=NativeEffectContextScope::Enter(context);if(!scope)return {};
  context->OMSetRenderTargets(1,&target.get(),depthTarget);
  for(const auto& draw:draws)if(!draw->Replay(context))return {};
  return color;
 }
};
}
