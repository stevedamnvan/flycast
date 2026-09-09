// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_cutout.h"
#include <wrl/client.h>
#include <array>
#include <cmath>
namespace neuraltest {
bool RunLegacyCutoutFixture(std::string& error) {
 using Microsoft::WRL::ComPtr;
 struct Window {HWND value=CreateWindowExW(0,L"STATIC",L"Cutout fixture",WS_POPUP,0,0,8,1,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
  ~Window(){if(value)DestroyWindow(value);}} window;
 const auto fail=[&](const char* why){error=why;return false;};
 if(!window.value)return fail("cutout hidden window");
 ComPtr<IDirect3D9Ex> api;ComPtr<IDirect3DDevice9Ex> device;
 if(FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION,api.GetAddressOf())))return fail("cutout D3D9 creation");
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
 pp.hDeviceWindow=window.value;pp.BackBufferWidth=8;pp.BackBufferHeight=1;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
 if(FAILED(api->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window.value,D3DCREATE_HARDWARE_VERTEXPROCESSING,
  &pp,nullptr,device.GetAddressOf())))return fail("cutout D3D9 device");
 ComPtr<IDirect3DSurface9> target,readback,depth;ComPtr<IDirect3DTexture9> texture;
 if(FAILED(device->CreateRenderTarget(8,1,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,target.GetAddressOf(),nullptr))
  ||FAILED(device->CreateOffscreenPlainSurface(8,1,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,readback.GetAddressOf(),nullptr))
  ||FAILED(device->CreateTexture(8,1,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,texture.GetAddressOf(),nullptr))
  ||FAILED(device->CreateDepthStencilSurface(8,1,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,depth.GetAddressOf(),nullptr)))return fail("cutout fixture resources");
 const std::array<unsigned,8> alphas{0,1,63,127,128,129,254,255};
 ComPtr<IDirect3DPixelShader9> shader,wrongShader;
 if(FAILED(remake::CreateLegacyCutoutShader(device.Get(),shader.GetAddressOf(),false,&error)))return false;
 if(FAILED(remake::CreateLegacyCutoutShader(device.Get(),wrongShader.GetAddressOf(),true,&error)))return false;
 D3DLOCKED_RECT map{};
 if(FAILED(texture->LockRect(0,&map,nullptr,0)))return fail("cutout texture map");
 for(unsigned i=0;i<8;++i)static_cast<DWORD*>(map.pBits)[i]=(alphas[i]<<24)|0xffffff;
 texture->UnlockRect(0);
 struct Vertex {float x,y,z,w;DWORD color;float u,v;};
 if(FAILED(device->SetRenderTarget(0,target.Get()))||FAILED(device->SetDepthStencilSurface(depth.Get()))
  ||FAILED(device->SetRenderState(D3DRS_ZENABLE,TRUE))||FAILED(device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE))
  ||FAILED(device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL))||FAILED(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE))
  ||FAILED(device->SetRenderState(D3DRS_LIGHTING,FALSE))||FAILED(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1))
  ||FAILED(device->SetTexture(0,texture.Get()))
  ||FAILED(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1))
  ||FAILED(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE))
  ||FAILED(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT))
  ||FAILED(device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT)))return fail("cutout fixture pipeline");
 unsigned wrongOpaquePixels=0,wrongRoundingCases=0;
 for(bool wrong:{false,true})for(unsigned flags=0;flags<4;++flags)for(unsigned vertexAlpha:{128u,255u})for(unsigned threshold:{0u,1u,64u,128u,255u}) {
  remake::Mesh mesh;mesh.sourceTsp=(3u<<6)|((flags&1)?1u<<20:0)|((flags&2)?1u<<19:0);
  mesh.sourceAlphaReference=static_cast<std::uint8_t>(threshold);
  const DWORD color=(vertexAlpha<<24)|0xffffff;
  const Vertex vertices[]={{-.5f,-.5f,0,1,color,0,0},{7.5f,-.5f,0,1,color,1,0},
   {-.5f,.5f,0,1,color,0,1},{7.5f,.5f,0,1,color,1,1}};
  if(FAILED(remake::ApplyLegacyAlpha(device.Get(),mesh,wrong?wrongShader.Get():shader.Get()))
   ||FAILED(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE))
   ||FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0))
   ||FAILED(device->BeginScene()))return fail("cutout fixture begin");
  const auto draw=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));
  // A green surface behind the cutout may appear only through rejected pixels.
  const Vertex behind[]={{-.5f,-.5f,.5f,1,0xff00ff00,0,0},{7.5f,-.5f,.5f,1,0xff00ff00,1,0},
   {-.5f,.5f,.5f,1,0xff00ff00,0,1},{7.5f,.5f,.5f,1,0xff00ff00,1,1}};
  remake::Mesh opaque;opaque.sourceTsp=3u<<6;
  const bool backgroundOk=SUCCEEDED(remake::ApplyLegacyAlpha(device.Get(),opaque))
   &&SUCCEEDED(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE))
   &&SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,behind,sizeof(Vertex)));
  const auto end=device->EndScene();
  if(FAILED(draw)||!backgroundOk||FAILED(end)||FAILED(device->GetRenderTargetData(target.Get(),readback.Get()))
   ||FAILED(readback->LockRect(&map,nullptr,D3DLOCK_READONLY)))return fail("cutout fixture readback");
  bool correct=true;
  for(unsigned i=0;i<8;++i) {
   const unsigned a=(flags&1)?vertexAlpha:255,b=(flags&2)?255:alphas[i];
   const unsigned rounded=static_cast<unsigned>(std::floor(double(a)*b/255+.5));
   const bool visible=rounded>=threshold;
   const bool actual=(static_cast<const DWORD*>(map.pBits)[i]&0xffffff)==0xffffff;
   if(actual!=visible&&correct)error="cutout alpha truth flags="+std::to_string(flags)
    +" vertex="+std::to_string(vertexAlpha)+" threshold="+std::to_string(threshold)
    +" pixel="+std::to_string(i)+" rgba="+std::to_string(static_cast<const DWORD*>(map.pBits)[i])
    +" expected-visible="+std::to_string(visible);
   correct&=actual==visible&&((static_cast<const DWORD*>(map.pBits)[i]&0xffffff)==(visible?0xffffffu:0x00ff00u));wrongOpaquePixels+=!visible;
  }
  readback->UnlockRect();if(wrong){wrongRoundingCases+=!correct;}else if(!correct)return false;
 }
 // Execute disabled-alpha negative after the final enabled state, not just an
 // analytic prediction. Every texel must become visible, including alpha zero.
 remake::Mesh opaque;opaque.sourceTsp=3u<<6;
 const Vertex quad[]={{-.5f,-.5f,0,1,0xffffffff,0,0},{7.5f,-.5f,0,1,0xffffffff,1,0},
  {-.5f,.5f,0,1,0xffffffff,0,1},{7.5f,.5f,0,1,0xffffffff,1,1}};
 if(FAILED(remake::ApplyLegacyAlpha(device.Get(),opaque))
  ||FAILED(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE))
  ||FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0))
  ||FAILED(device->BeginScene()))return fail("cutout negative begin");
 const auto draw=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));const auto end=device->EndScene();
 if(FAILED(draw)||FAILED(end)||FAILED(device->GetRenderTargetData(target.Get(),readback.Get()))
  ||FAILED(readback->LockRect(&map,nullptr,D3DLOCK_READONLY)))return fail("cutout negative readback");
 bool opaqueVisible=true;for(unsigned i=0;i<8;++i)opaqueVisible&=(static_cast<const DWORD*>(map.pBits)[i]&0xffffff)==0xffffff;
 readback->UnlockRect();if(!opaqueVisible||!wrongOpaquePixels||!wrongRoundingCases)return fail("cutout negative controls");
 error.clear();return true;
}
}
