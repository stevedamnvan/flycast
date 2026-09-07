// SPDX-License-Identifier: GPL-2.0-or-later
#include "pvr_scene_capture.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/TexCache.h"
#include "version.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <locale>
#include <sstream>

namespace flycast::rend::neural {
namespace {
std::uint32_t Bits(const float& value) {
 std::uint32_t result; static_assert(sizeof(result)==sizeof(value));
 std::memcpy(&result,&value,sizeof(result)); return result;
}
void String(std::ostream& out,const std::string& value) {
 const char* hex="0123456789abcdef"; out << '"';
 for (unsigned char c:value) {
  if(c=='"'||c=='\\') out << '\\' << c;
  else if(c<32) out << "\\u00" << hex[c>>4] << hex[c&15];
  else out << c;
 }
 out << '"';
}
void Texture(std::ostream& out,const BaseTextureCacheData* texture) {
 if(!texture) {out << "null";return;}
 out << "{\"upload_generation\":" << texture->Updates << ",\"palette_hash\":";
 // This cache field is assigned only for paletted textures. Do not serialize
 // an indeterminate value for ordinary RGB textures as generation evidence.
 if(texture->tcw.PixelFmt==PixelPal4||texture->tcw.PixelFmt==PixelPal8) out<<texture->palette_hash;
 else out<<"null";
 out << ",\"rtt_generation\":" << texture->rttGeneration << '}';
}
}
bool WritePvrScenePacket(const std::filesystem::path& path,const rend_context& ctx,
 const std::array<float,16>& viewport,std::uint64_t frame,
 const std::string& game,std::string& error) {
 // Bounds checked before building any output. At most 32 MiB serialized text.
 const auto draws=ctx.global_param_op.size()+ctx.global_param_pt.size()+ctx.global_param_tr.size();
 if(ctx.isRTT||frame==0||game.empty()||game.size()>256||ctx.verts.size()>65536
  ||ctx.idx.size()>262144||draws>8192||ctx.render_passes.size()>MAX_PASSES) {
  error="pvr-packet-identity-or-bound";return false;
 }
 for(float v:viewport) if(!std::isfinite(v)) {error="pvr-packet-viewport";return false;}
 std::size_t nonfinitePositions=0;
 for(const auto& v:ctx.verts)
  if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) ++nonfinitePositions;
 for(auto index:ctx.idx) if(index!=0xffffffffu&&index>=ctx.verts.size()) {
  error="pvr-packet-index";return false;
 }
 std::size_t nonfiniteReferences=0;
 for(auto index:ctx.idx) if(index!=0xffffffffu) {
  const auto& v=ctx.verts[index];
  if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) ++nonfiniteReferences;
 }
 for(const auto* list:{&ctx.global_param_op,&ctx.global_param_pt,&ctx.global_param_tr})
  for(const auto& p:*list) if(p.first>ctx.idx.size()||p.count>ctx.idx.size()-p.first) {
   error="pvr-packet-draw-range";return false;
  }
 std::ostringstream out;out.imbue(std::locale::classic());
 out << "{\"schema\":\"flycast-pvr-scene-v1\",\"git_sha\":\"" << GIT_HASH
  << "\",\"frame_id\":" << frame << ",\"game_id\":";String(out,game);
 out << ",\"coordinate_space\":\"pvr-projected\",\"camera_provenance\":\"unknown\","
  "\"world_transform_provenance\":\"unknown\",\"normal_provenance\":\"unknown-for-dreamcast\","
  "\"depth_semantics\":\"raw-PVR-z-before-production-log-depth-shader\","
  "\"float_encoding\":\"IEEE754-binary32-unsigned-bits\","
  "\"viewport_layout\":\"column-major\",\"viewport_bits\":[";
 for(size_t i=0;i<viewport.size();++i) {if(i)out<<',';out<<Bits(viewport[i]);}
 out << "],\"nonfinite_position_count\":" << nonfinitePositions
  << ",\"nonfinite_index_reference_count\":" << nonfiniteReferences
  << ",\"clear_framebuffer\":" << (ctx.clearFramebuffer?"true":"false")
  << ",\"framebuffer_size\":[" << ctx.framebufferWidth << ',' << ctx.framebufferHeight << ']'
  << ",\"omissions\":[\"texture-pixels\",\"fog-and-global-register-state\","
  "\"retained-framebuffer-pixels\",\"offscreen-culled-geometry\",\"game-camera-and-lights\","
  "\"modifier-volume-geometry\",\"sorted-translucency-resolve-order\",\"Naomi2-matrices-and-lights\"],"
  "\"modifier_triangle_count\":" << ctx.modtrig.size()
  << ",\"naomi2_matrix_count\":" << ctx.matrices.size()
  << ",\"vertex_layout\":[\"x_bits\",\"y_bits\",\"z_bits\",\"u_bits\",\"v_bits\","
  "\"color_rgba_bytes\",\"offset_rgba_bytes\",\"u1_bits\",\"v1_bits\","
  "\"color1_rgba_bytes\",\"offset1_rgba_bytes\"],\"vertices\":[";
 for(size_t i=0;i<ctx.verts.size();++i) {
  const auto& v=ctx.verts[i];if(i)out<<',';
  out << '[' << Bits(v.x)<<','<<Bits(v.y)<<','<<Bits(v.z)<<','<<Bits(v.u)<<','<<Bits(v.v);
  auto color=[&](const u8* c){out<<",["<<unsigned(c[0])<<','<<unsigned(c[1])<<','<<unsigned(c[2])<<','<<unsigned(c[3])<<']';};
  color(v.col);color(v.spc);out<<','<<Bits(v.u1)<<','<<Bits(v.v1);color(v.col1);color(v.spc1);out<<']';
 }
 out << "],\"indices\":[";
 for(size_t i=0;i<ctx.idx.size();++i){if(i)out<<',';out<<ctx.idx[i];}
 out << "],\"draws\":[";bool comma=false;unsigned listId=0;
 for(const auto* list:{&ctx.global_param_op,&ctx.global_param_pt,&ctx.global_param_tr}) {
  unsigned ordinal=0;
  for(const auto& p:*list) {
   if(comma)out<<',';comma=true;
   out << "{\"list\":"<<listId<<",\"ordinal\":"<<ordinal++<<",\"first\":"<<p.first
    <<",\"count\":"<<p.count<<",\"tsp\":"<<p.tsp.full<<",\"tcw\":"<<p.tcw.full
    <<",\"pcw\":"<<p.pcw.full<<",\"isp\":"<<p.isp.full<<",\"tileclip\":"<<p.tileclip
    <<",\"tsp1\":"<<p.tsp1.full<<",\"tcw1\":"<<p.tcw1.full
    <<",\"naomi2\":"<<(p.isNaomi2()?"true":"false")<<",\"texture\":";Texture(out,p.texture);
   out<<",\"texture1\":";Texture(out,p.texture1);out<<'}';
  }
  ++listId;
 }
 out << "],\"passes\":[";
 for(size_t i=0;i<ctx.render_passes.size();++i) {
  const auto& p=ctx.render_passes[i];if(i)out<<',';
  out<<"{\"op\":"<<p.op_count<<",\"pt\":"<<p.pt_count<<",\"tr\":"<<p.tr_count
   <<",\"mvo\":"<<p.mvo_count<<",\"sorted_tr\":"<<p.sorted_tr_count
   <<",\"autosort\":"<<(p.autosort?"true":"false")<<",\"z_clear\":"<<(p.z_clear?"true":"false")<<'}';
 }
 out<<"]}\n";const auto text=out.str();
 if(text.size()>32*1024*1024) {error="pvr-packet-byte-bound";return false;}
 std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(text.data(),text.size());file.close();
 if(!file) {error="pvr-packet-write";return false;}return true;
}
}
