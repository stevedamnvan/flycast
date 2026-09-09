// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include "remake_legacy_contract.h"
#include "remake_cutout.h"
#include "remake_scene_lighting.h"
#include <d3d9.h>
#include <remix_c.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace neuraltest::remake {
// Bounded opaque diagnostic packet uploader, NOT full PVR shading or a backend.
class D3D9PacketScene {
 struct Vertex {float x,y,z,nx,ny,nz;DWORD color;float u,v;};
 struct Resource {IDirect3DVertexBuffer9* vb=nullptr;IDirect3DTexture9* texture=nullptr;std::vector<std::uint32_t> indices;};
 IDirect3DDevice9Ex* device_;
 remixapi_Interface api_;
 std::vector<Resource> resources_;
 Packet initial_;
 Packet previous_;
 remixapi_LightHandle light_=nullptr;
 IDirect3DPixelShader9* cutoutShader_=nullptr;
 bool failed_=false,ready_=false;
 bool refreshResources_=false;
 bool allowSkippedSources_=false;
 bool omitCutoutsControl_=false;
 float sceneLightRadiance_=3;
 bool anchoredLight_=false;
 AnchoredSceneLight anchoredLightDirection_; // Survives material resource rebuilds.
 std::vector<std::vector<unsigned char>> textureBytes_;
 void ReleaseResources() {
  if(!resources_.empty()){device_->SetTexture(0,nullptr);device_->SetStreamSource(0,nullptr,0,0);}
  for(auto& r:resources_){if(r.vb)r.vb->Release();if(r.texture)r.texture->Release();}
  resources_.clear();textureBytes_.clear();
  if(light_){api_.DestroyLight(light_);light_=nullptr;}
  ready_=false;
 }
 static std::vector<unsigned char> ReadTexture(const Mesh::Material& material) {
  if(!material.sourceDdsBytes.empty()) {
   if(!material.sourceDds.empty() || !ValidSourceDdsBytes(material.sourceDdsBytes))throw std::runtime_error("owned texture contract");
   return material.sourceDdsBytes;
  }
  const auto& path=material.sourceDds;
  const auto count=std::filesystem::file_size(path);
  if(count<148||count>64*1024*1024)throw std::runtime_error("texture size");
  std::vector<unsigned char> bytes(static_cast<std::size_t>(count));
  std::ifstream stream(path,std::ios::binary);
  if(!stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("texture read");
  if(!ValidSourceDdsBytes(bytes))throw std::runtime_error("file texture contract");
  return bytes;
 }
 static constexpr DWORD fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1;
 static DWORD word(const std::vector<unsigned char>& bytes,std::size_t offset){DWORD v;std::memcpy(&v,bytes.data()+offset,4);return v;}
 HRESULT Texture(const std::vector<unsigned char>& data,IDirect3DTexture9** output) {
  if(word(data,0)!=0x20534444||word(data,128)!=28)return E_INVALIDARG;
  const auto width=word(data,16),height=word(data,12),levels=word(data,28);
  if(!width||!height||width>4096||height>4096||!levels||levels>13)return E_INVALIDARG;
  auto hr=device_->CreateTexture(width,height,levels,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,output,nullptr);
  if(FAILED(hr))return hr;
  std::size_t offset=148;
  for(DWORD level=0;level<levels;++level) {
   const auto w=std::max<DWORD>(1,width>>level),h=std::max<DWORD>(1,height>>level);
   if(offset+std::size_t(w)*h*4>data.size())return E_INVALIDARG;
   D3DLOCKED_RECT rect{};hr=(*output)->LockRect(level,&rect,nullptr,0);if(FAILED(hr))return hr;
   for(DWORD y=0;y<h;++y)for(DWORD x=0;x<w;++x) {
    const auto* src=data.data()+offset+(y*w+x)*4;
    auto* dst=static_cast<unsigned char*>(rect.pBits)+y*rect.Pitch+x*4;
    dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=src[3];
   }
   hr=(*output)->UnlockRect(level);if(FAILED(hr))return hr;
   offset+=std::size_t(w)*h*4;
  }
  return offset==data.size()?S_OK:E_INVALIDARG;
 }
 HRESULT DrawInternal(const Packet& packet) {
  if(!device_||!api_.CreateLight||!api_.DestroyLight||!api_.DrawLightInstance||!ReadyForDiagnosticAdapter(packet,packet.frame,packet.game,true).ok)return E_INVALIDARG;
  const auto fixedDirection=anchoredLight_?anchoredLightDirection_.Select(packet):std::optional<Vec3>{};
  if(anchoredLight_&&!fixedDirection)return E_INVALIDARG;
  for(const auto& mesh:packet.meshes)if(!LegacySamplingSupported(mesh)||(mesh.material->sourceDds.empty()&&mesh.material->sourceDdsBytes.empty()))return E_INVALIDARG;
  for(const auto& mesh:packet.meshes)if(mesh.sourceAlphaReference&&!cutoutShader_)
   if(FAILED(CreateLegacyCutoutShader(device_,&cutoutShader_)))return E_FAIL;
  if(ready_ && packet.frame!=previous_.frame && !DiagnosticContinuation(previous_,packet)) {
   if(!allowSkippedSources_||!AsyncSourceContinuation(previous_,packet))return E_INVALIDARG;
   std::cout<<"async_source_gap previous="<<previous_.frame<<" current="<<packet.frame
    <<" uploader_resources_reset=true runtime_temporal_reset_proven=false\n";
   ReleaseResources(); // Reset our correspondence only, not an undocumented runtime history API.
  }
  if(ready_ && refreshResources_) {
   bool compatible=packet.game==initial_.game && packet.meshes.size()==resources_.size();
   for(std::size_t i=0;compatible&&i<packet.meshes.size();++i)
    compatible=LegacyResourceCompatible(packet.meshes[i],initial_.meshes[i]) && ReadTexture(*packet.meshes[i].material)==textureBytes_[i];
   if(!compatible) {
    std::cout<<"live_source_resource_refresh frame="<<packet.frame<<" draws="<<packet.meshes.size()<<" temporal_identity_proven=false\n";
    // Diagnostic policy: discard/recreate incompatible resources, never freeze
    // the first packet. Any failure poisons this uploader; caller must not Present.
    ReleaseResources();
   }
  }
  if(!ready_) {
   initial_=packet;resources_.resize(packet.meshes.size());
   for(const auto& mesh:packet.meshes)textureBytes_.push_back(ReadTexture(*mesh.material));
   for(std::size_t i=0;i<resources_.size();++i) {
    auto& r=resources_[i];r.indices=Triangles(packet.meshes[i]);if(r.indices.empty())return E_INVALIDARG;
    auto hr=device_->CreateVertexBuffer(UINT(r.indices.size()*sizeof(Vertex)),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,fvf,D3DPOOL_DEFAULT,&r.vb,nullptr);
    if(FAILED(hr))return hr;if(FAILED(hr=Texture(textureBytes_[i],&r.texture)))return hr;
   }
   remixapi_LightInfoDistantEXT distant{};distant.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
   // Old prepared artifacts use a reflected anchor. The live-derived packet is
   // already camera-relative (+Z forward), so use an explicitly labeled headlight
   // along that camera direction. This is supplied diagnostic light, not game light.
   Vec3 direction=refreshResources_?packet.camera.forward:Vec3{0,0,-1};
   if(anchoredLight_) {
    direction=*fixedDirection;
   }
   distant.direction={direction.x,direction.y,direction.z};
   std::cout<<"scene_light_created frame="<<packet.frame<<" anchored="<<anchoredLight_
    <<" direction="<<direction.x<<','<<direction.y<<','<<direction.z<<'\n';
   distant.angularDiameterDegrees=.5f;distant.volumetricRadianceScale=1;
   remixapi_LightInfo light{};light.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;light.pNext=&distant;
   light.hash=0xfc067d40;light.radiance={sceneLightRadiance_,sceneLightRadiance_,sceneLightRadiance_};
   if(api_.CreateLight(&light,&light_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
   ready_=true;
  }
  if(packet.game!=initial_.game||packet.meshes.size()!=resources_.size())return E_INVALIDARG;
  for(std::size_t i=0;i<resources_.size();++i) {
   const auto& m=packet.meshes[i];const auto& old=initial_.meshes[i];
   if(!LegacyResourceCompatible(m,old))return E_INVALIDARG;
   // Capture directories may differ; resource reuse requires exact source bytes.
   if(ReadTexture(*m.material)!=textureBytes_[i])return E_INVALIDARG;
  }
  const auto& c=packet.camera;
  D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
  D3DMATRIX view{};view._44=1;
  const Vec3 axes[]={c.right,c.up,c.forward};
  for(int col=0;col<3;++col){view.m[0][col]=axes[col].x;view.m[1][col]=axes[col].y;view.m[2][col]=axes[col].z;
   view.m[3][col]=-(axes[col].x*c.position.x+axes[col].y*c.position.y+axes[col].z*c.position.z);}
  D3DMATRIX projection{};projection._22=1/std::tan(c.fovY*3.14159265358979323846f/360);
  projection._11=projection._22/c.aspect;projection._33=c.farPlane/(c.farPlane-c.nearPlane);
  projection._34=1;projection._43=-c.nearPlane*projection._33;
  if(FAILED(device_->SetTransform(D3DTS_WORLD,&identity))||FAILED(device_->SetTransform(D3DTS_VIEW,&view))||FAILED(device_->SetTransform(D3DTS_PROJECTION,&projection))||
     FAILED(device_->SetFVF(fvf))||FAILED(device_->SetRenderState(D3DRS_LIGHTING,FALSE))||FAILED(device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE))||
     FAILED(device_->SetRenderState(D3DRS_ZENABLE,TRUE))||FAILED(device_->SetRenderState(D3DRS_ZWRITEENABLE,TRUE))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE))||FAILED(device_->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE))||FAILED(device_->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1))||
     FAILED(device_->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE)))return E_FAIL;
  auto hr=device_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0);if(FAILED(hr))return hr;
  if(FAILED(hr=device_->BeginScene()))return hr;
  for(std::size_t i=0;i<resources_.size()&&SUCCEEDED(hr);++i) {
   const auto& mesh=packet.meshes[i];auto& r=resources_[i];void* mapped=nullptr;
   if(omitCutoutsControl_&&mesh.sourceAlphaReference)continue;
   hr=r.vb->Lock(0,UINT(r.indices.size()*sizeof(Vertex)),&mapped,D3DLOCK_DISCARD);if(FAILED(hr))break;
   auto* output=static_cast<Vertex*>(mapped);
   for(std::size_t j=0;j<r.indices.size();++j){const auto& v=mesh.vertices[r.indices[j]];output[j]={v.position.x,v.position.y,v.position.z,v.normal->x,v.normal->y,v.normal->z,v.publicColor,v.u,v.v};}
   hr=r.vb->Unlock();if(FAILED(hr))break;
   const auto tsp=*mesh.sourceTsp;
   if(FAILED(ApplyLegacyAlpha(device_,mesh,cutoutShader_))){hr=E_FAIL;break;}
   const auto address=[](bool clamp,bool mirror){return clamp?D3DTADDRESS_CLAMP:mirror?D3DTADDRESS_MIRROR:D3DTADDRESS_WRAP;};
   if(FAILED(device_->SetTexture(0,r.texture))||FAILED(device_->SetStreamSource(0,r.vb,0,sizeof(Vertex)))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_ADDRESSU,address(tsp&(1<<16),tsp&(1<<18))))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_ADDRESSV,address(tsp&(1<<15),tsp&(1<<17))))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_MINFILTER,(tsp&(1<<13))?D3DTEXF_LINEAR:D3DTEXF_POINT))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_MAGFILTER,(tsp&(1<<13))?D3DTEXF_LINEAR:D3DTEXF_POINT))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_POINT))||FAILED(device_->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE))) {hr=E_FAIL;break;}
   hr=device_->DrawPrimitive(D3DPT_TRIANGLELIST,0,UINT(r.indices.size()/3));
  }
  const auto ended=device_->EndScene();if(FAILED(hr))return hr;if(FAILED(ended))return ended;
  if(api_.DrawLightInstance(light_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
  previous_=packet; // Diagnostic draw only; harness aborts if the following Present fails.
  return S_OK;
 }
public:
 D3D9PacketScene(IDirect3DDevice9Ex* device,remixapi_Interface api,bool refreshResources=false,bool allowSkippedSources=false,bool omitCutoutsControl=false,float sceneLightRadiance=3,bool anchoredLight=false):device_(device),api_(api),refreshResources_(refreshResources),allowSkippedSources_(allowSkippedSources),omitCutoutsControl_(omitCutoutsControl),sceneLightRadiance_(sceneLightRadiance),anchoredLight_(anchoredLight){
  failed_=!std::isfinite(sceneLightRadiance_)||sceneLightRadiance_<0||sceneLightRadiance_>30;
 }
 D3D9PacketScene(const D3D9PacketScene&)=delete;D3D9PacketScene& operator=(const D3D9PacketScene&)=delete;
 ~D3D9PacketScene(){ReleaseResources();if(device_)device_->SetPixelShader(nullptr);if(cutoutShader_)cutoutShader_->Release();}
 HRESULT Draw(const Packet& p){if(failed_)return E_FAIL;try{const auto hr=DrawInternal(p);if(FAILED(hr))failed_=true;return hr;}catch(const std::exception&){failed_=true;return E_FAIL;}}
};
}
