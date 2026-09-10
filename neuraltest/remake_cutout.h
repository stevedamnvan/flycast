// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstring>
namespace neuraltest::remake {
inline HRESULT CreateLegacyCutoutShader(IDirect3DDevice9* device,IDirect3DPixelShader9** output,bool wrongRounding=false,std::string* diagnostic=nullptr) {
 if(!device||!output)return E_INVALIDARG;
 const char* source=R"(
sampler2D image : register(s0);
float4 controls : register(c0);
float4 main(float4 vertex : COLOR0,float2 uv : TEXCOORD0) : COLOR0 {
 float4 texel=tex2D(image,uv);
 float alpha=saturate((controls.x!=0?vertex.a:1)*(controls.y!=0?1:texel.a));
#if WRONG_ROUNDING
 clip(alpha*255-controls.z);
#else
 clip(floor(alpha*255+.5)-controls.z);
#endif
 return float4(vertex.rgb*texel.rgb,1);
})";
 const D3D_SHADER_MACRO defines[]={{"WRONG_ROUNDING",wrongRounding?"1":"0"},{nullptr,nullptr}};
 ID3DBlob* code=nullptr;ID3DBlob* errors=nullptr;
 auto hr=D3DCompile(source,std::strlen(source),"flycast-source-cutout",defines,nullptr,"main","ps_3_0",
  0,0,&code,&errors); // Legacy ps_3_0 sampler syntax is intentional.
 if(SUCCEEDED(hr))hr=device->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()),output);
 if(FAILED(hr)&&diagnostic)*diagnostic=errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"cutout CreatePixelShader HRESULT="+std::to_string(hr);
 if(errors)errors->Release();if(code)code->Release();return hr;
}
// ShadInstr3 only, matching LegacySamplingSupported. Alpha inputs are selected
// independently; disabled vertex/texture alpha means one, not zero.
// opaqueAlphaOne (D-220 diagnostic A/B only): a mesh that neither blends nor
// cuts out emits alpha one instead of the texture alpha, so the consumer cannot
// read an opaque surface as translucent. Off by default; not a source truth.
inline HRESULT ApplyLegacyAlpha(IDirect3DDevice9* device,const Mesh& mesh,IDirect3DPixelShader9* shader=nullptr,bool opaqueAlphaOne=false) {
 if(!device||!mesh.sourceTsp)return E_INVALIDARG;
 const auto tsp=*mesh.sourceTsp;const bool cutout=mesh.sourceAlphaReference.has_value();
 const DWORD opaqueAlpha=(opaqueAlphaOne&&!cutout&&!mesh.sourceAlphaBlend)?D3DTA_TFACTOR:D3DTA_TEXTURE;
 if(cutout&&!shader)return E_INVALIDARG;
 const DWORD texture=(tsp&(1u<<19))?D3DTA_TFACTOR:D3DTA_TEXTURE;
 const DWORD vertex=(tsp&(1u<<20))?D3DTA_DIFFUSE:D3DTA_TFACTOR;
 if(FAILED(device->SetRenderState(D3DRS_TEXTUREFACTOR,0xffffffff))
  ||FAILED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,mesh.sourceAlphaBlend))
  ||FAILED(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA))
  ||FAILED(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA))
  ||FAILED(device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD))
  ||FAILED(device->SetRenderState(D3DRS_ZWRITEENABLE,!mesh.sourceAlphaBlend))
  ||FAILED(device->SetRenderState(D3DRS_ALPHATESTENABLE,cutout))
  ||FAILED(device->SetRenderState(D3DRS_ALPHAREF,mesh.sourceAlphaReference.value_or(0)))
  ||FAILED(device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL))
  ||FAILED(device->SetTextureStageState(0,D3DTSS_ALPHAOP,(cutout||mesh.sourceAlphaBlend)?D3DTOP_MODULATE:D3DTOP_SELECTARG1))
  ||FAILED(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,(cutout||mesh.sourceAlphaBlend)?texture:opaqueAlpha))
  ||FAILED(device->SetTextureStageState(0,D3DTSS_ALPHAARG2,vertex)))return E_FAIL;
 if(FAILED(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE))
  ||FAILED(device->SetPixelShader(cutout?shader:nullptr)))return E_FAIL;
 if(cutout) {
  const float controls[]={float((tsp>>20)&1),float((tsp>>19)&1),float(*mesh.sourceAlphaReference),0};
  if(FAILED(device->SetPixelShaderConstantF(0,controls,1)))return E_FAIL;
 }
 return S_OK;
}
}
namespace neuraltest {bool RunLegacyCutoutFixture(std::string& error);}
