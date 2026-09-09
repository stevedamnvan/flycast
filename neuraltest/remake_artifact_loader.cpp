// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_artifact_loader.h"
#include "../core/deps/json/json.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace neuraltest::remake {
namespace {
using Json=nlohmann::json;
void need(bool ok,const char* reason) {if(!ok)throw std::invalid_argument(reason);}
std::string read(const std::filesystem::path& p,std::size_t bound) {
 const auto size=std::filesystem::file_size(p);need(size>0 && size<=bound,"artifact file bound");
 std::string data(size,'\0');std::ifstream in(p,std::ios::binary);
 need(bool(in.read(data.data(),data.size())) && in.peek()==EOF,"artifact read");return data;
}
std::uint64_t number(const Json& j) {need(j.is_number_unsigned(),"unsigned field");return j.get<std::uint64_t>();}
float real(const Json& j) {need(j.is_number(),"numeric field");auto v=j.get<float>();need(std::isfinite(v),"finite field");return v;}
Vec3 vector(const Json& j) {need(j.is_array()&&j.size()==3,"vector shape");return {real(j[0]),real(j[1]),real(j[2])};}
std::string sha(const std::string& bytes) {
 unsigned char digest[32];
 need(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,(PUCHAR)bytes.data(),ULONG(bytes.size()),digest,32)>=0,"SHA256");
 const char* hex="0123456789abcdef";std::string result;
 for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
}
Packet LoadDiagnosticArtifact(const std::filesystem::path& path,const std::filesystem::path& assets,float clipNear,float clipFar) {
 need(path.is_absolute()&&assets.is_absolute(),"absolute artifact paths required");
 need(std::isfinite(clipNear)&&std::isfinite(clipFar)&&clipNear>0&&clipFar>clipNear,"explicit diagnostic clips");
 auto json=Json::parse(read(path,32*1024*1024),[](int depth,Json::parse_event_t,Json&) {
  need(depth<=32,"JSON depth");return true;
 });
 need(json.at("schema")=="flycast-prepared-remake-scene-v1"
  && json.at("coordinate_space")=="reflected-selected-source-anchor"
  && json.at("renderable_by_remix_adapter")==false,"diagnostic artifact schema");
 need(json.at("material_semantic")=="source-color-not-physical-albedo","source color semantic");
 Packet p;p.space=Space::SampledAnchor;p.frame=number(json.at("frame_id"));p.game=json.at("game_id").get<std::string>();
 p.diagnosticOrigin=vector(json.at("fixed_origin"));
 need(p.game.size()<=64,"game length");
 p.sourceGitSha=json.at("git_sha").get<std::string>();
 need(!p.sourceGitSha.empty()&&p.sourceGitSha.size()<=64,"source SHA length");
 const auto& camera=json.at("camera");need(camera.at("nearPlane").is_null()&&camera.at("farPlane").is_null()
  &&camera.at("accepted_game_camera")==false,"source game camera not recovered");
 p.camera.provenance=Provenance::Supplied;p.camera.position=vector(camera.at("position"));
 p.camera.right=vector(camera.at("right"));p.camera.up=vector(camera.at("up"));p.camera.forward=vector(camera.at("forward"));
 p.camera.fovY=real(camera.at("fovY"));p.camera.aspect=real(camera.at("aspect"));p.camera.nearPlane=clipNear;p.camera.farPlane=clipFar;
 const auto& omissions=json.at("omissions");need(omissions.is_array()&&omissions.size()<=64,"omission bound");
 for(const auto& omission:omissions){auto s=omission.get<std::string>();need(s.size()<=4096,"omission length");p.omissions.push_back(s);}
 const auto& meshes=json.at("meshes");need(meshes.is_array()&&!meshes.empty()&&meshes.size()<=128,"mesh bound");
 std::size_t vertices=0,indices=0,assetBytes=0;
 for(const auto& mesh:meshes) {
  const auto& vs=mesh.at("vertices");const auto& is=mesh.at("indices");
  need(vs.is_array()&&is.is_array()&&vs.size()<=65536-vertices&&is.size()<=262144-indices,"aggregate geometry bound");
  vertices+=vs.size();indices+=is.size();
 }
 need(vertices*sizeof(Vertex)+indices*sizeof(std::uint32_t)<=Limits{}.bytes,"geometry byte bound");
 for(const auto& input:meshes) {
  Mesh m;const auto sourceDraw=number(input.at("source_draw"));need(sourceDraw<UINT32_MAX,"draw identity bound");
  m.id=sourceDraw+1;m.frame=p.frame;
  m.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  const auto& bindings=input.at("source_bindings");need(bindings.is_array()&&bindings.size()==1,"single source texture required");
  const auto& b=bindings[0];need(number(b.at("slot"))==0,"primary source texture required");
  need(number(b.at("draw"))==sourceDraw,"material draw ownership");
  const auto aid=number(b.at("asset"));const auto& asset=json.at("source_assets").at(std::to_string(aid));
  const auto name="asset-"+std::to_string(aid)+".dds";need(asset.at("file")==name,"fixed asset filename");
  need(asset.at("semantic")=="source-color-not-physical-albedo","asset semantic");
  auto file=assets/name;need(file.native().size()<=4096,"asset path bound");
  auto bytes=read(file,64*1024*1024);need(bytes.size()<=64*1024*1024-assetBytes,"aggregate asset bytes");assetBytes+=bytes.size();
  need(number(asset.at("bytes"))==bytes.size()&&asset.at("sha256")==sha(bytes)&&b.at("dds_sha256")==asset.at("sha256"),"asset hash mismatch");
  need(b.at("palette_hash").is_null(),"palette identity import unsupported");
  m.texture={aid,number(b.at("upload_generation")),0,number(b.at("rtt_generation")),true};
  m.material=Mesh::Material{};m.material->albedo={1,1,1};m.material->sourceColorExperiment=true;
  m.material->sourceDds=file;m.material->sourceTexture=m.texture;
  for(const auto& v:input.at("vertices")) {
   need(v.at("normal_provenance")=="geometry-derived-flat","normal provenance");Vertex vertex;
   vertex.position=vector(v.at("position"));vertex.normal=vector(v.at("normal"));
   const auto& raw=v.at("original_vertex");need(raw.is_array()&&raw.size()==11,"source vertex layout");
   auto bits=number(raw[3]);need(bits<=UINT32_MAX,"UV bits");auto word=std::uint32_t(bits);std::memcpy(&vertex.u,&word,4);
   bits=number(raw[4]);need(bits<=UINT32_MAX,"UV bits");word=std::uint32_t(bits);std::memcpy(&vertex.v,&word,4);
   need(raw[5].is_array()&&raw[5].size()==4,"BGRA shape");std::array<std::uint8_t,4> color;
   for(int i=0;i<4;++i){auto value=number(raw[5][i]);need(value<=255,"BGRA byte");color[i]=std::uint8_t(value);}
   vertex.publicColor=PublicColorFromBgra(color);m.vertices.push_back(vertex);
  }
  for(const auto& index:input.at("indices")){auto n=number(index);need(n<m.vertices.size(),"mesh index");m.indices.push_back(std::uint32_t(n));}
  p.meshes.push_back(std::move(m));
 }
 const auto valid=ReadyForDiagnosticAdapter(p,p.frame,p.game,true);need(valid.ok,valid.reason.c_str());return p;
}
}
