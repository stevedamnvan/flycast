// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d11.h>
#include "windows/comptr.h"

namespace flycast::rend::neural {
// Copy on the source immediate context before subsequent native writes. No
// pointer-key cache here: constants may change between consecutive draws.
inline bool NativeCaptureContextMatches(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11Resource* source) noexcept {
 if(!device||!context||!source||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
 ComPtr<ID3D11Device> contextDevice,sourceDevice;
 context->GetDevice(&contextDevice.get());source->GetDevice(&sourceDevice.get());
 return contextDevice.get()==device&&sourceDevice.get()==device;
}
inline ComPtr<ID3D11Buffer> CopyNativeEffectBuffer(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11Buffer* source) {
 ComPtr<ID3D11Buffer> copy;
 if(!NativeCaptureContextMatches(device,context,source))return copy;
 D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
 // Replay binds vertex/index/constants only. Reject special buffer semantics.
 const UINT allowed=D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER|D3D11_BIND_CONSTANT_BUFFER;
 if(!desc.ByteWidth||!desc.BindFlags||(desc.BindFlags&~allowed)||desc.MiscFlags)return copy;
 desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
 if(FAILED(device->CreateBuffer(&desc,nullptr,&copy.get())))return {};
 context->CopyResource(copy,source);return copy;
}
inline ComPtr<ID3D11Texture2D> CopyNativeEffectTexture(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11Texture2D* source) {
 ComPtr<ID3D11Texture2D> copy;
 if(!NativeCaptureContextMatches(device,context,source))return copy;
 D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
 // Preserve every mip, array slice, format and sample. Shared handles and
 // mip-generation ownership belong to the live source, not this snapshot.
 const UINT allowedMisc=D3D11_RESOURCE_MISC_GENERATE_MIPS|D3D11_RESOURCE_MISC_TEXTURECUBE;
 if(desc.MiscFlags&~allowedMisc)return copy;
 desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
 desc.MiscFlags&=D3D11_RESOURCE_MISC_TEXTURECUBE;
 if(FAILED(device->CreateTexture2D(&desc,nullptr,&copy.get())))return {};
 context->CopyResource(copy,source);return copy;
}
// Return an owning view with the exact original interpretation. Buffer/3D
// views are unsupported here and must not silently become empty bindings.
inline ComPtr<ID3D11ShaderResourceView> CopyNativeEffectView(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11ShaderResourceView* source) {
 if(!source)return {};
 ComPtr<ID3D11Resource> resource;source->GetResource(&resource.get());
 if(!NativeCaptureContextMatches(device,context,resource))return {};
 ComPtr<ID3D11Texture2D> texture;
 if(FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D),
  reinterpret_cast<void**>(&texture.get()))))return {};
 auto owned=CopyNativeEffectTexture(device,context,texture);
 if(!owned)return {};
 D3D11_SHADER_RESOURCE_VIEW_DESC desc{};source->GetDesc(&desc);
 ComPtr<ID3D11ShaderResourceView> view;
 if(FAILED(device->CreateShaderResourceView(owned,&desc,&view.get())))return {};
 return view;
}

}
