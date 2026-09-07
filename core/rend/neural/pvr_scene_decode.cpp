// SPDX-License-Identifier: GPL-2.0-or-later
#include "pvr_scene_capture.h"
#include "json/json.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace flycast::rend::neural {
namespace {
using Json=nlohmann::json;
void Require(bool condition,const char* reason) {if(!condition)throw std::runtime_error(reason);}
std::uint64_t U64(const Json& j) {
 Require(j.is_number_unsigned(),"pvr-decode-unsigned-integer");return j.get<std::uint64_t>();
}
std::uint32_t U32(const Json& j) {
 const auto value=U64(j);Require(value<=UINT32_MAX,"pvr-decode-u32-range");return static_cast<std::uint32_t>(value);
}
bool Bool(const Json& j) {Require(j.is_boolean(),"pvr-decode-boolean");return j.get<bool>();}
std::string Text(const Json& j) {Require(j.is_string(),"pvr-decode-string");return j.get<std::string>();}
void Array(const Json& j,std::size_t max) {Require(j.is_array()&&j.size()<=max,"pvr-decode-array-bound");}
void ExactArray(const Json& j,std::size_t n) {Array(j,n);Require(j.size()==n,"pvr-decode-array-size");}
float Float(const Json& j) {const auto bits=U32(j);float v;std::memcpy(&v,&bits,4);return v;}
bool Finite(const Vertex& v) {return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
std::optional<PvrCapturedTexture> Texture(const Json& j,TCW tcw) {
 if(j.is_null())return {};
 Require(j.is_object(),"pvr-decode-texture");
 PvrCapturedTexture t;t.upload=U32(j.at("upload_generation"));t.rtt=U32(j.at("rtt_generation"));
 const auto& pal=j.at("palette_hash");
 const bool paletted=tcw.PixelFmt==PixelPal4||tcw.PixelFmt==PixelPal8;
 Require(paletted!=pal.is_null(),"pvr-decode-palette-applicability");
 if(paletted)t.palette=U32(pal);return t;
}
}
bool ReadPvrScenePacket(const std::filesystem::path& path,std::uint64_t expectedFrame,
 const std::string& expectedGame,PvrDecodedPacket& output,std::string& error) {
 try {
  Require(expectedFrame!=0&&!expectedGame.empty()&&expectedGame.size()<=256,"pvr-decode-expected-identity");
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  Require(bool(file),"pvr-decode-open");const auto size=file.tellg();
  Require(size>0&&size<=32*1024*1024,"pvr-decode-byte-bound");
  std::string text(static_cast<std::size_t>(size),'\0');file.seekg(0);file.read(text.data(),size);
  Require(bool(file)&&file.peek()==std::char_traits<char>::eof(),"pvr-decode-read-size");
  // Bound DOM construction itself, not only the decoded vectors afterward.
  std::size_t events=0;std::vector<std::set<std::string>> keys;
  const auto root=Json::parse(text,[&](int depth,Json::parse_event_t event,Json& value) {
   Require(depth<=12&&++events<=2000000,"pvr-decode-parser-budget");
   if(event==Json::parse_event_t::object_start)keys.emplace_back();
   if(event==Json::parse_event_t::key) {
    Require(!keys.empty()&&keys.back().insert(value.get<std::string>()).second,"pvr-decode-duplicate-key");
   }
   if(event==Json::parse_event_t::object_end)keys.pop_back();
   if(value.is_string())Require(value.get_ref<const std::string&>().size()<=256,"pvr-decode-string-bound");
   return true;
  });
  Require(root.is_object(),"pvr-decode-root");
  Require(root.at("schema")=="flycast-pvr-scene-v1"
   &&root.at("coordinate_space")=="pvr-projected"
   &&root.at("camera_provenance")=="unknown"
   &&root.at("world_transform_provenance")=="unknown"
   &&root.at("normal_provenance")=="unknown-for-dreamcast"
   &&root.at("depth_semantics")=="raw-PVR-z-before-production-log-depth-shader"
   &&root.at("float_encoding")=="IEEE754-binary32-unsigned-bits"
   &&root.at("viewport_layout")=="column-major","pvr-decode-provenance");
  PvrDecodedPacket packet;packet.frame=U64(root.at("frame_id"));packet.game=Text(root.at("game_id"));
  Require(packet.frame==expectedFrame&&packet.game==expectedGame,"pvr-decode-frame-game-mismatch");
  packet.gitSha=Text(root.at("git_sha"));Require(!packet.gitSha.empty(),"pvr-decode-git-sha");
  Require(U32(root.at("naomi2_matrix_count"))==0,"pvr-decode-naomi2-unsupported");
  packet.modifierTriangles=U32(root.at("modifier_triangle_count"));
  const Json layout={"x_bits","y_bits","z_bits","u_bits","v_bits","color_rgba_bytes",
   "offset_rgba_bytes","u1_bits","v1_bits","color1_rgba_bytes","offset1_rgba_bytes"};
  Require(root.at("vertex_layout")==layout,"pvr-decode-vertex-layout");
  const auto& omissions=root.at("omissions");Array(omissions,32);
  for(const auto& omission:omissions)packet.omissions.push_back(Text(omission));
  const std::set<std::string> required={"texture-pixels","fog-and-global-register-state",
   "retained-framebuffer-pixels","offscreen-culled-geometry","game-camera-and-lights",
   "modifier-volume-geometry","sorted-translucency-resolve-order","Naomi2-matrices-and-lights"};
  Require(std::set<std::string>(packet.omissions.begin(),packet.omissions.end())==required
   &&packet.omissions.size()==required.size(),"pvr-decode-omissions");
  ExactArray(root.at("viewport_bits"),16);
  for(size_t i=0;i<16;++i) {
   packet.viewport[i]=Float(root.at("viewport_bits")[i]);
   Require(std::isfinite(packet.viewport[i]),"pvr-decode-viewport");
  }
  ExactArray(root.at("framebuffer_size"),2);
  for(size_t i=0;i<2;++i)packet.framebufferSize[i]=U32(root.at("framebuffer_size")[i]);
  packet.clearFramebuffer=Bool(root.at("clear_framebuffer"));
  const auto& vertices=root.at("vertices");Array(vertices,65536);
  packet.vertices.resize(vertices.size());
  for(size_t i=0;i<vertices.size();++i) {
   const auto& a=vertices[i];ExactArray(a,11);auto& v=packet.vertices[i];
   v.x=Float(a[0]);v.y=Float(a[1]);v.z=Float(a[2]);v.u=Float(a[3]);v.v=Float(a[4]);
   auto color=[&](const Json& j,u8* c) {ExactArray(j,4);for(size_t k=0;k<4;++k) {
    const auto n=U32(j[k]);Require(n<=255,"pvr-decode-color-range");c[k]=static_cast<u8>(n);
   }};
   color(a[5],v.col);color(a[6],v.spc);v.u1=Float(a[7]);v.v1=Float(a[8]);color(a[9],v.col1);color(a[10],v.spc1);
   if(!Finite(v))++packet.unusedNonfinite;
  }
  Require(packet.unusedNonfinite==U32(root.at("nonfinite_position_count")),"pvr-decode-nonfinite-accounting");
  const auto& indices=root.at("indices");Array(indices,262144);packet.indices.reserve(indices.size());
  for(const auto& j:indices) {
   const auto index=U32(j);Require(index==UINT32_MAX||index<packet.vertices.size(),"pvr-decode-index-range");
   if(index!=UINT32_MAX)Require(Finite(packet.vertices[index]),"pvr-decode-referenced-nonfinite");
   packet.indices.push_back(index);
  }
  Require(U32(root.at("nonfinite_index_reference_count"))==0,"pvr-decode-nonfinite-accounting");
  const auto& draws=root.at("draws");Array(draws,8192);packet.draws.reserve(draws.size());
  std::array<std::uint32_t,3> ordinals{};
  for(const auto& j:draws) {
   PvrCapturedDraw draw;draw.state.init();draw.list=U32(j.at("list"));draw.ordinal=U32(j.at("ordinal"));
   Require(draw.list<3&&draw.ordinal==ordinals[draw.list]++,"pvr-decode-draw-ordinal");
   Require(!Bool(j.at("naomi2")),"pvr-decode-naomi2-unsupported");auto& p=draw.state;
   p.first=U32(j.at("first"));p.count=U32(j.at("count"));
   Require(p.first<=packet.indices.size()&&p.count<=packet.indices.size()-p.first,"pvr-decode-draw-range");
   p.tsp.full=U32(j.at("tsp"));p.tcw.full=U32(j.at("tcw"));p.pcw.full=U32(j.at("pcw"));
   p.isp.full=U32(j.at("isp"));p.tileclip=U32(j.at("tileclip"));
   p.tsp1.full=U32(j.at("tsp1"));p.tcw1.full=U32(j.at("tcw1"));
   draw.texture=Texture(j.at("texture"),p.tcw);draw.texture1=Texture(j.at("texture1"),p.tcw1);
   packet.draws.push_back(std::move(draw));
  }
  const auto& passes=root.at("passes");Array(passes,MAX_PASSES);PvrCapturedPass previous;
  for(const auto& j:passes) {
   PvrCapturedPass p;p.op=U32(j.at("op"));p.pt=U32(j.at("pt"));p.tr=U32(j.at("tr"));
   p.mvo=U32(j.at("mvo"));p.sortedTr=U32(j.at("sorted_tr"));
   p.autosort=Bool(j.at("autosort"));p.zClear=Bool(j.at("z_clear"));
   Require(p.op>=previous.op&&p.pt>=previous.pt&&p.tr>=previous.tr&&p.mvo>=previous.mvo
    &&p.sortedTr>=previous.sortedTr&&p.op<=ordinals[0]&&p.pt<=ordinals[1]&&p.tr<=ordinals[2],"pvr-decode-pass-range");
   packet.passes.push_back(p);previous=p;
  }
  Require(previous.op==ordinals[0]&&previous.pt==ordinals[1]&&previous.tr==ordinals[2],"pvr-decode-pass-coverage");
  output=std::move(packet);error.clear();return true;
 } catch(const std::exception& e) {error=e.what();return false;}
}
}
