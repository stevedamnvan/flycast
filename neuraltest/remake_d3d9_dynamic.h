// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d9.h>
#include <cstring>
#include <cmath>
#include <remix_c.h>

namespace neuraltest::remake {
// Isolated compatibility fixture. No Flycast production renderer integration.
class DynamicD3D9Fixture {
 IDirect3DDevice9Ex* device_;
 IDirect3DVertexBuffer9* vertices_=nullptr;
 IDirect3DTexture9* texture_=nullptr;
 remixapi_Interface api_;
 remixapi_LightHandle light_=nullptr;
 bool failed_=false;
 struct Vertex {float x,y,z,nx,ny,nz;DWORD color;float u,v;};
 static constexpr DWORD fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1;
public:
 DynamicD3D9Fixture(IDirect3DDevice9Ex* device,remixapi_Interface api):device_(device),api_(api){}
 DynamicD3D9Fixture(const DynamicD3D9Fixture&)=delete;
 DynamicD3D9Fixture& operator=(const DynamicD3D9Fixture&)=delete;
 ~DynamicD3D9Fixture(){if(vertices_){device_->SetStreamSource(0,nullptr,0,0);vertices_->Release();}if(texture_){device_->SetTexture(0,nullptr);texture_->Release();}if(light_)api_.DestroyLight(light_);}
 HRESULT Draw(float progress,bool frozen=false) {
  if(failed_)return E_FAIL;
  if(!device_||!std::isfinite(progress)||progress<0||progress>1||!api_.CreateLight||!api_.DestroyLight||!api_.SetupCamera||!api_.DrawLightInstance)return E_INVALIDARG;
  const auto result=DrawInternal(progress,frozen);
  if(FAILED(result))failed_=true;
  return result;
 }
private:
 HRESULT DrawInternal(float progress,bool frozen) {
  if(!vertices_) {
   const auto hr=device_->CreateVertexBuffer(sizeof(Vertex)*6,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,fvf,D3DPOOL_DEFAULT,&vertices_,nullptr);
   if(FAILED(hr))return hr;
   auto textureStatus=device_->CreateTexture(2,2,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture_,nullptr);
   if(FAILED(textureStatus))return textureStatus;
   D3DLOCKED_RECT rect{};textureStatus=texture_->LockRect(0,&rect,nullptr,D3DLOCK_DISCARD);
   if(FAILED(textureStatus))return textureStatus;
   for(int y=0;y<2;++y)for(int x=0;x<2;++x)reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch)[x]=0xffffffff;
   if(FAILED(textureStatus=texture_->UnlockRect(0)))return textureStatus;
   remixapi_LightInfoDistantEXT distant{};distant.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
   distant.direction={0,0,1};distant.angularDiameterDegrees=.5f;distant.volumetricRadianceScale=1;
   remixapi_LightInfo light{};light.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;light.pNext=&distant;
   light.hash=0xfc067d39;light.radiance={.03f,.03f,.03f};
   if(api_.CreateLight(&light,&light_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
  }
  Vertex data[6]{};
  for(int mesh=0;mesh<2;++mesh) {
   const float s=float(mesh+1);
   const DWORD a=frozen?0xff808080:D3DCOLOR_XRGB(80+int(80*progress),120,80);
   const DWORD b=frozen?0xff808080:D3DCOLOR_XRGB(80,80+int(80*progress),120);
   const DWORD c=frozen?0xff808080:D3DCOLOR_XRGB(120,80,80+int(80*progress));
   data[mesh*3]={-s,-s,2*s,0,0,-1,a,0,1};
   data[mesh*3+1]={s,-s,2*s,0,0,-1,b,1,1};
   data[mesh*3+2]={progress*.5f,s,2*s,0,0,-1,c,.5f,0};
  }
  void* mapped=nullptr;auto hr=vertices_->Lock(0,sizeof(data),&mapped,D3DLOCK_DISCARD);
  if(FAILED(hr))return hr;std::memcpy(mapped,data,sizeof(data));
  if(FAILED(hr=vertices_->Unlock()))return hr;
  D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
  D3DMATRIX projection{};projection._11=.75f;projection._22=1;
  projection._33=100.f/99.9f;projection._34=1;projection._43=-10.f/99.9f;
  remixapi_CameraInfoParameterizedEXT cameraParameters{};
  cameraParameters.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT;
  cameraParameters.forward={0,0,1};cameraParameters.up={0,1,0};cameraParameters.right={1,0,0};
  cameraParameters.fovYInDegrees=90;cameraParameters.aspect=4.f/3;
  cameraParameters.nearPlane=.1f;cameraParameters.farPlane=100;
  remixapi_CameraInfo camera{};camera.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;
  camera.pNext=&cameraParameters;camera.type=REMIXAPI_CAMERA_TYPE_WORLD;
  if(api_.SetupCamera(&camera)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
  if(FAILED(device_->SetTransform(D3DTS_WORLD,&identity))||FAILED(device_->SetTransform(D3DTS_VIEW,&identity))||
     FAILED(device_->SetTransform(D3DTS_PROJECTION,&projection))||FAILED(device_->SetFVF(fvf))||
     FAILED(device_->SetStreamSource(0,vertices_,0,sizeof(Vertex)))||FAILED(device_->SetTexture(0,texture_))||
     FAILED(device_->SetRenderState(D3DRS_LIGHTING,FALSE))||FAILED(device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE))||
     FAILED(device_->SetRenderState(D3DRS_ZENABLE,TRUE))||FAILED(device_->SetRenderState(D3DRS_ZWRITEENABLE,TRUE))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE)))return E_FAIL;
  if(FAILED(hr=device_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0)))return hr;
  if(FAILED(hr=device_->BeginScene()))return hr;
  const auto drawn=device_->DrawPrimitive(D3DPT_TRIANGLELIST,0,2);
  const auto ended=device_->EndScene();
  if(FAILED(drawn))return drawn;if(FAILED(ended))return ended;
  return api_.DrawLightInstance(light_)==REMIXAPI_ERROR_CODE_SUCCESS?S_OK:E_FAIL;
 }
};
}
