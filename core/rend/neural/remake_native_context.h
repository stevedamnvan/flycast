// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d11_1.h>
#include <memory>
#include "windows/comptr.h"
namespace flycast::rend::neural {
// A separate D3D11.1 state object isolates all pipeline bindings. It does not
// undo resource writes: replay must still use owned output/depth resources.
class NativeEffectContextScope {
 ComPtr<ID3D11DeviceContext1> context;
 ComPtr<ID3DDeviceContextState> previous,scratch;
 NativeEffectContextScope()=default;
public:
 NativeEffectContextScope(const NativeEffectContextScope&)=delete;
 NativeEffectContextScope& operator=(const NativeEffectContextScope&)=delete;
 static std::unique_ptr<NativeEffectContextScope> Enter(ID3D11DeviceContext* input) {
  if(!input||input->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return {};
  auto result=std::unique_ptr<NativeEffectContextScope>(new NativeEffectContextScope);
  if(FAILED(input->QueryInterface(__uuidof(ID3D11DeviceContext1),
   reinterpret_cast<void**>(&result->context.get()))))return {};
  ComPtr<ID3D11Device> device;input->GetDevice(&device.get());
  ComPtr<ID3D11Device1> device1;
  if(FAILED(device->QueryInterface(__uuidof(ID3D11Device1),
   reinterpret_cast<void**>(&device1.get()))))return {};
  const auto level=device->GetFeatureLevel();
  if(FAILED(device1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,
   __uuidof(ID3D11Device),nullptr,&result->scratch.get())))return {};
  result->context->SwapDeviceContextState(result->scratch,&result->previous.get());
  return result;
 }
 ~NativeEffectContextScope(){
  if(previous)context->SwapDeviceContextState(previous,nullptr);
 }
};
}
