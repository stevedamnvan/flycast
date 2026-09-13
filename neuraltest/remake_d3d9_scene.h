// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include "remake_legacy_contract.h"
#include "remake_legacy_reuse.h"
#include "remake_cutout.h"
#include "remake_scene_lighting.h"
#include <d3d9.h>
#include <remix_c.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>
#include <type_traits>

namespace neuraltest::remake {
// Immutable shared texture source bytes. A Referenced mesh (D-212) carries no
// bytes; the owner of the channel session resolves its identity to storage that
// is never mutated, so the same pointer proves the same bytes without a compare.
using TextureSourceBytes=std::shared_ptr<const std::vector<unsigned char>>;
using TextureReferenceResolver=std::function<TextureSourceBytes(const Mesh&)>;
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
 remixapi_LightHandle fillLight_=nullptr;
 IDirect3DPixelShader9* cutoutShader_=nullptr;
 // D-220 diagnostic A/B: opaque meshes emit alpha one (env FLYCAST_REMAKE_OPAQUE_ALPHA_ONE=1).
 const bool opaqueAlphaOne_=[]{wchar_t v[2]{};return GetEnvironmentVariableW(L"FLYCAST_REMAKE_OPAQUE_ALPHA_ONE",v,2)==1&&v[0]==L'1';}();
 bool failed_=false,ready_=false;
 bool refreshResources_=false;
 const bool selectiveRefresh_=[]{wchar_t v[2]{};return GetEnvironmentVariableW(L"FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH",v,2)==1&&v[0]==L'1';}();
 bool allowSkippedSources_=false;
 bool omitCutoutsControl_=false;
 float sceneLightRadiance_=3;
 bool anchoredLight_=false;
 bool templeLightRig_=false;
 std::optional<Vec3> authoredDirection_;
 std::optional<SceneFill> sceneFill_;
 AnchoredSceneLight anchoredLightDirection_; // Survives material resource rebuilds.
 AnchoredSceneLight anchoredFillDirection_;
 TextureReferenceResolver resolveReference_;
 std::vector<TextureSourceBytes> textureBytes_; // Source bytes uploaded per resource slot.
 void ReleaseResources() {
  if(!resources_.empty()){device_->SetTexture(0,nullptr);device_->SetStreamSource(0,nullptr,0,0);}
  for(auto& r:resources_){if(r.vb)r.vb->Release();if(r.texture)r.texture->Release();}
  resources_.clear();textureBytes_.clear();
  if(light_){api_.DestroyLight(light_);light_=nullptr;}
  if(fillLight_){api_.DestroyLight(fillLight_);fillLight_=nullptr;}
  ready_=false;
 }
 // Referenced mesh bytes from the session owner; null when the mesh is not a
 // resolvable reference (missing, ambiguous with owned bytes/path, or invalid).
 TextureSourceBytes ResolveReference(const Mesh& mesh) const {
  if(mesh.textureWire!=TextureWire::Referenced||!resolveReference_||!mesh.material
   ||!mesh.material->sourceDds.empty()||!mesh.material->sourceDdsBytes.empty())return nullptr;
  auto bytes=resolveReference_(mesh);
  return bytes&&ValidSourceDdsBytes(*bytes)?bytes:nullptr;
 }
 static bool TextureSourceDeclared(const Mesh& mesh) {
  return mesh.textureWire==TextureWire::Referenced||!mesh.material->sourceDds.empty()||!mesh.material->sourceDdsBytes.empty();
 }
 static void CheckOwnedTexture(const Mesh::Material& material) {
  if(!material.sourceDds.empty() || !ValidSourceDdsBytes(material.sourceDdsBytes))throw std::runtime_error("owned texture contract");
 }
 static std::vector<unsigned char> ReadTextureFile(const std::filesystem::path& path) {
  const auto count=std::filesystem::file_size(path);
  if(count<148||count>64*1024*1024)throw std::runtime_error("texture size");
  std::vector<unsigned char> bytes(static_cast<std::size_t>(count));
  std::ifstream stream(path,std::ios::binary);
  if(!stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("texture read");
  if(!ValidSourceDdsBytes(bytes))throw std::runtime_error("file texture contract");
  return bytes;
 }
 // Source bytes to upload for a mesh. Owned bytes are copied once here (upload
 // only); a reference shares the owner's storage without any copy.
 TextureSourceBytes ReadTexture(const Mesh& mesh) const {
  if(mesh.textureWire==TextureWire::Referenced) {
   auto bytes=ResolveReference(mesh);
   if(!bytes)throw std::runtime_error("texture reference contract");
   return bytes;
  }
  const auto& material=*mesh.material;
  if(!material.sourceDdsBytes.empty()) {
   CheckOwnedTexture(material);
   return std::make_shared<const std::vector<unsigned char>>(material.sourceDdsBytes);
  }
  return std::make_shared<const std::vector<unsigned char>>(ReadTextureFile(material.sourceDds));
 }
 // Exact-bytes check against the slot's uploaded source without copying. A
 // reference compares storage identity first; equal bytes behind a different
 // storage adopt that storage so later frames stay a pointer compare.
 bool SameTexture(const Mesh& mesh,TextureSourceBytes& uploaded) const {
  if(mesh.textureWire==TextureWire::Referenced) {
   auto bytes=ResolveReference(mesh);
   if(!bytes)throw std::runtime_error("texture reference contract");
   if(bytes==uploaded)return true;
   if(*bytes!=*uploaded)return false;
   uploaded=std::move(bytes);return true;
  }
  const auto& material=*mesh.material;
  if(!material.sourceDdsBytes.empty()) {CheckOwnedTexture(material);return material.sourceDdsBytes==*uploaded;}
  return ReadTextureFile(material.sourceDds)==*uploaded;
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
 // Build the replacement collection transactionally. Retained COM references
 // belong to both collections until commit, so any failure leaves old ownership intact.
 HRESULT RefreshMatchingResources(const Packet& packet) {
  struct Pending {
   std::vector<Resource> resources;
   ~Pending(){for(auto& r:resources){if(r.vb)r.vb->Release();if(r.texture)r.texture->Release();}}
  } pending;
  Packet nextInitial=packet;
  std::vector<TextureSourceBytes> nextBytes(packet.meshes.size());
  pending.resources.resize(packet.meshes.size());
  const auto mapping=LegacyReuseMapping(packet.meshes.size(),resources_.size(),[&](std::size_t i,std::size_t j){
   return LegacyResourceCompatible(packet.meshes[i],initial_.meshes[j])&&SameTexture(packet.meshes[i],textureBytes_[j]);
  });
  std::size_t retained=0;
  for(std::size_t i=0;i<packet.meshes.size();++i) {
   auto& next=pending.resources[i];const auto old=mapping[i];
   if(old<resources_.size()) {
    next.indices=resources_[old].indices;
    nextBytes[i]=textureBytes_[old];
    next.vb=resources_[old].vb;next.vb->AddRef();
    next.texture=resources_[old].texture;next.texture->AddRef();++retained;
   } else {
    nextBytes[i]=ReadTexture(packet.meshes[i]);next.indices=Triangles(packet.meshes[i]);
    if(next.indices.empty())return E_INVALIDARG;
    auto hr=device_->CreateVertexBuffer(UINT(next.indices.size()*sizeof(Vertex)),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,fvf,D3DPOOL_DEFAULT,&next.vb,nullptr);
    if(FAILED(hr))return hr;
    if(FAILED(hr=Texture(*nextBytes[i],&next.texture)))return hr;
   }
  }
  if(FAILED(device_->SetTexture(0,nullptr))||FAILED(device_->SetStreamSource(0,nullptr,0,0)))return E_FAIL;
  static_assert(std::is_nothrow_move_assignable_v<Packet>,"Resource commit requires nonthrowing packet transfer");
  resources_.swap(pending.resources);textureBytes_.swap(nextBytes);
  initial_=std::move(nextInitial);
  std::cout<<"selective_source_resource_refresh frame="<<packet.frame<<" replaced="<<resources_.size()-retained
   <<" retained="<<retained<<" retired="<<pending.resources.size()-retained
   <<" anchored_lights_retained=true temporal_identity_proven=false\n";
  return S_OK;
 }
 HRESULT DrawInternal(const Packet& packet) {
  // Host elapsed scopes include driver waits; these are not GPU timestamps.
  using Clock=std::chrono::steady_clock;
  auto checkpoint=Clock::now();
  const auto elapsed=[&](){const auto now=Clock::now();const double ms=std::chrono::duration<double,std::milli>(now-checkpoint).count();checkpoint=now;return ms;};
  double uploadMs=0,stateMs=0,primitiveMs=0;

  if(!device_||!api_.CreateLight||!api_.DestroyLight||!api_.DrawLightInstance||!ReadyForDiagnosticAdapter(packet,packet.frame,packet.game,true).ok)return E_INVALIDARG;
  const auto fixedDirection=anchoredLight_?anchoredLightDirection_.Select(packet,
   templeLightRig_?std::optional<Vec3>{TempleLightDirection(packet.camera,false)}:authoredDirection_):std::optional<Vec3>{};
  if(anchoredLight_&&!fixedDirection)return E_INVALIDARG;
  const auto fillDirection=templeLightRig_?anchoredFillDirection_.Select(packet,TempleLightDirection(packet.camera,true)):
   sceneFill_?anchoredFillDirection_.Select(packet,sceneFill_->direction):std::optional<Vec3>{};
  if((templeLightRig_||sceneFill_)&&!fillDirection)return E_INVALIDARG;
  for(const auto& mesh:packet.meshes)if(!LegacySamplingSupported(mesh)||!TextureSourceDeclared(mesh)
   ||(mesh.textureWire==TextureWire::Referenced&&!ResolveReference(mesh)))return E_INVALIDARG;
  for(const auto& mesh:packet.meshes)if(mesh.sourceAlphaReference&&!cutoutShader_)
   if(FAILED(CreateLegacyCutoutShader(device_,&cutoutShader_)))return E_FAIL;
  const double validationMs=elapsed();
  if(ready_ && packet.frame!=previous_.frame && !DiagnosticContinuation(previous_,packet)) {
   const bool regenerated=allowSkippedSources_&&AnchorGenerationChange(previous_,packet);
   if(!regenerated&&(!allowSkippedSources_||!AsyncSourceContinuation(previous_,packet)))return E_INVALIDARG;
   if(regenerated) {
    const auto o=*packet.diagnosticOrigin;
    std::cout<<"anchor_generation_change previous="<<previous_.frame<<" current="<<packet.frame
     <<" origin="<<o.x<<','<<o.y<<','<<o.z<<" uploader_resources_reset=true light_reanchored="<<anchoredLight_
     <<" runtime_temporal_reset_proven=false\n";
   } else
    std::cout<<"async_source_gap previous="<<previous_.frame<<" current="<<packet.frame
     <<" uploader_resources_reset=true runtime_temporal_reset_proven=false\n";
   ReleaseResources(); // Reset our correspondence only, not an undocumented runtime history API.
  }
  if(ready_ && refreshResources_) {
   bool compatible=packet.game==initial_.game && packet.meshes.size()==resources_.size();
   for(std::size_t i=0;compatible&&i<packet.meshes.size();++i)
    compatible=LegacyResourceCompatible(packet.meshes[i],initial_.meshes[i]) && SameTexture(packet.meshes[i],textureBytes_[i]);
   if(!compatible) {
    if(selectiveRefresh_&&anchoredLight_&&packet.game==initial_.game) {
     const auto refreshed=RefreshMatchingResources(packet);if(FAILED(refreshed))return refreshed;
    } else {
    std::cout<<"live_source_resource_refresh frame="<<packet.frame<<" draws="<<packet.meshes.size()<<" temporal_identity_proven=false\n";
    // Diagnostic policy: discard/recreate incompatible resources, never freeze
    // the first packet. Any failure poisons this uploader; caller must not Present.
    ReleaseResources();
    }
   }
  }
  if(!ready_) {
   initial_=packet;resources_.resize(packet.meshes.size());
   for(const auto& mesh:packet.meshes)textureBytes_.push_back(ReadTexture(mesh));
   for(std::size_t i=0;i<resources_.size();++i) {
    auto& r=resources_[i];r.indices=Triangles(packet.meshes[i]);if(r.indices.empty())return E_INVALIDARG;
    auto hr=device_->CreateVertexBuffer(UINT(r.indices.size()*sizeof(Vertex)),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,fvf,D3DPOOL_DEFAULT,&r.vb,nullptr);
    if(FAILED(hr))return hr;if(FAILED(hr=Texture(*textureBytes_[i],&r.texture)))return hr;
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
   if(templeLightRig_) {
    light.radiance={sceneLightRadiance_,sceneLightRadiance_*.88f,sceneLightRadiance_*.72f};
    distant.angularDiameterDegrees=3.f;
   }
   if(api_.CreateLight(&light,&light_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
   if(templeLightRig_||sceneFill_) {
    distant.direction={fillDirection->x,fillDirection->y,fillDirection->z};
    distant.angularDiameterDegrees=12.f;
    light.hash=0xfc067d41;
    light.radiance=sceneFill_?remixapi_Float3D{sceneFill_->radiance,sceneFill_->radiance,sceneFill_->radiance}:
     remixapi_Float3D{sceneLightRadiance_*.20f,sceneLightRadiance_*.26f,sceneLightRadiance_*.35f};
    if(api_.CreateLight(&light,&fillLight_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
    std::cout<<"scene_fill_created temple_rig="<<templeLightRig_<<" authored=true world_lighting_proven=false fill_direction="
     <<fillDirection->x<<','<<fillDirection->y<<','<<fillDirection->z<<" radiance="<<light.radiance.x<<','<<light.radiance.y<<','<<light.radiance.z<<'\n';
   }
   ready_=true;
  }
  if(packet.game!=initial_.game||packet.meshes.size()!=resources_.size())return E_INVALIDARG;
  for(std::size_t i=0;i<resources_.size();++i) {
   const auto& m=packet.meshes[i];const auto& old=initial_.meshes[i];
   if(!LegacyResourceCompatible(m,old))return E_INVALIDARG;
   // Capture directories may differ; resource reuse requires exact source bytes.
   if(!SameTexture(m,textureBytes_[i]))return E_INVALIDARG;
  }
  const double resourcesMs=elapsed();
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
  const double setupMs=elapsed();
  for(std::size_t i=0;i<resources_.size()&&SUCCEEDED(hr);++i) {
   const auto& mesh=packet.meshes[i];auto& r=resources_[i];void* mapped=nullptr;
   if(omitCutoutsControl_&&mesh.sourceAlphaReference)continue;
   hr=r.vb->Lock(0,UINT(r.indices.size()*sizeof(Vertex)),&mapped,D3DLOCK_DISCARD);if(FAILED(hr))break;
   auto* output=static_cast<Vertex*>(mapped);
   for(std::size_t j=0;j<r.indices.size();++j){const auto& v=mesh.vertices[r.indices[j]];output[j]={v.position.x,v.position.y,v.position.z,v.normal->x,v.normal->y,v.normal->z,v.publicColor,v.u,v.v};}
   hr=r.vb->Unlock();if(FAILED(hr))break;
   uploadMs+=elapsed();
   const auto tsp=*mesh.sourceTsp;
   if(FAILED(ApplyLegacyAlpha(device_,mesh,cutoutShader_,opaqueAlphaOne_))){hr=E_FAIL;break;}
   const auto address=[](bool clamp,bool mirror){return clamp?D3DTADDRESS_CLAMP:mirror?D3DTADDRESS_MIRROR:D3DTADDRESS_WRAP;};
   if(FAILED(device_->SetTexture(0,r.texture))||FAILED(device_->SetStreamSource(0,r.vb,0,sizeof(Vertex)))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_ADDRESSU,address(tsp&(1<<16),tsp&(1<<18))))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_ADDRESSV,address(tsp&(1<<15),tsp&(1<<17))))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_MINFILTER,(tsp&(1<<13))?D3DTEXF_LINEAR:D3DTEXF_POINT))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_MAGFILTER,(tsp&(1<<13))?D3DTEXF_LINEAR:D3DTEXF_POINT))||
      FAILED(device_->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_POINT))||FAILED(device_->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE))) {hr=E_FAIL;break;}
   stateMs+=elapsed();
   hr=device_->DrawPrimitive(D3DPT_TRIANGLELIST,0,UINT(r.indices.size()/3));
   primitiveMs+=elapsed();
  }
  const auto ended=device_->EndScene();if(FAILED(hr))return hr;if(FAILED(ended))return ended;
  if(api_.DrawLightInstance(light_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
  if(fillLight_&&api_.DrawLightInstance(fillLight_)!=REMIXAPI_ERROR_CODE_SUCCESS)return E_FAIL;
  const double finishMs=elapsed();
  previous_=packet; // Diagnostic draw only; harness aborts if the following Present fails.
  const double retainedCopyMs=elapsed();
  std::cout<<"legacy_draw_cpu frame="<<packet.frame<<" meshes="<<packet.meshes.size()
   <<" validation_ms="<<validationMs<<" resources_ms="<<resourcesMs<<" setup_ms="<<setupMs
   <<" upload_ms="<<uploadMs<<" state_ms="<<stateMs<<" primitive_ms="<<primitiveMs
   <<" finish_ms="<<finishMs<<" retained_copy_ms="<<retainedCopyMs<<'\n';
  return S_OK;
 }
public:
 D3D9PacketScene(IDirect3DDevice9Ex* device,remixapi_Interface api,bool refreshResources=false,bool allowSkippedSources=false,bool omitCutoutsControl=false,float sceneLightRadiance=3,bool anchoredLight=false,TextureReferenceResolver resolveReference={},bool templeLightRig=false,std::optional<Vec3> authoredDirection={},std::optional<SceneFill> sceneFill={}):device_(device),api_(api),refreshResources_(refreshResources),allowSkippedSources_(allowSkippedSources),omitCutoutsControl_(omitCutoutsControl),sceneLightRadiance_(sceneLightRadiance),anchoredLight_(anchoredLight),templeLightRig_(templeLightRig),authoredDirection_(authoredDirection),sceneFill_(sceneFill),resolveReference_(std::move(resolveReference)){
  failed_=!std::isfinite(sceneLightRadiance_)||sceneLightRadiance_<0||sceneLightRadiance_>30||(templeLightRig_&&!anchoredLight_)||((authoredDirection_||sceneFill_)&&(!anchoredLight_||templeLightRig_));
 }
 D3D9PacketScene(const D3D9PacketScene&)=delete;D3D9PacketScene& operator=(const D3D9PacketScene&)=delete;
 ~D3D9PacketScene(){ReleaseResources();if(device_)device_->SetPixelShader(nullptr);if(cutoutShader_)cutoutShader_->Release();}
 HRESULT Draw(const Packet& p){if(failed_)return E_FAIL;try{const auto hr=DrawInternal(p);if(FAILED(hr))failed_=true;return hr;}catch(const std::exception&){failed_=true;return E_FAIL;}}
};
}
