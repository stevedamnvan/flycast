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
static bool ValidateContext(const rend_context& ctx,
 const std::array<float,16>& viewport,std::uint64_t frame,
 const std::string& game,std::vector<bool>& vertexRanges,
 std::size_t& nonfinitePositions,std::size_t& nonfiniteReferences,std::string& error) {
 // Bounds checked before building any output. At most 32 MiB serialized text.
 const auto draws=ctx.global_param_op.size()+ctx.global_param_pt.size()+ctx.global_param_tr.size();
 if(ctx.isRTT||frame==0||game.empty()||game.size()>256||ctx.verts.size()>65536
  ||ctx.idx.size()>262144||draws>8192||ctx.render_passes.size()>MAX_PASSES||ctx.sortedTriangles.size()>262144) {
  error="pvr-packet-identity-or-bound";return false;
 }
 for(float v:viewport) if(!std::isfinite(v)) {error="pvr-packet-viewport";return false;}
 nonfinitePositions=0;
 for(const auto& v:ctx.verts)
  if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) ++nonfinitePositions;
 for(auto index:ctx.idx) if(index!=0xffffffffu&&index>=ctx.verts.size()) {
  error="pvr-packet-index";return false;
 }
 nonfiniteReferences=0;
 for(auto index:ctx.idx) if(index!=0xffffffffu) {
  const auto& v=ctx.verts[index];
  if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) ++nonfiniteReferences;
 }
 vertexRanges.assign(ctx.global_param_tr.size(),false);
 RenderPass previous{};
 size_t sortedEnd=0;
 for(const auto& pass:ctx.render_passes) {
  if(pass.op_count<previous.op_count||pass.op_count>ctx.global_param_op.size()
   ||pass.pt_count<previous.pt_count||pass.pt_count>ctx.global_param_pt.size()
   ||pass.tr_count<previous.tr_count||pass.tr_count>ctx.global_param_tr.size()
   ||pass.sorted_tr_count<previous.sorted_tr_count||pass.sorted_tr_count>ctx.sortedTriangles.size()
   ||pass.mvo_count<previous.mvo_count) {error="pvr-packet-pass-range";return false;}
  if(pass.sorted_tr_count>previous.sorted_tr_count) {
   if(!pass.autosort) {error="pvr-packet-sorted-pass";return false;}
   for(size_t i=previous.tr_count;i<pass.tr_count;++i)vertexRanges[i]=true;
   for(size_t i=previous.sorted_tr_count;i<pass.sorted_tr_count;++i) {
    const auto& s=ctx.sortedTriangles[i];
    if(s.polyIndex<previous.tr_count||s.polyIndex>=pass.tr_count||s.count%3!=0
     ||(s.count!=0&&s.first<sortedEnd)
     ||s.first>ctx.idx.size()||s.count>ctx.idx.size()-s.first) {
     error="pvr-packet-sorted-range";return false;
    }
    for(size_t j=s.first;j<size_t(s.first)+s.count;++j)if(ctx.idx[j]==UINT32_MAX) {
     error="pvr-packet-sorted-restart";return false;
    }
    if(s.count!=0)sortedEnd=size_t(s.first)+s.count;
   }
  }
  previous=pass;
 }
 if(previous.op_count!=ctx.global_param_op.size()||previous.pt_count!=ctx.global_param_pt.size()
  ||previous.tr_count!=ctx.global_param_tr.size()||previous.sorted_tr_count!=ctx.sortedTriangles.size()) {
  error="pvr-packet-pass-coverage";return false;
 }
 unsigned validationList=0;
 for(const auto* list:{&ctx.global_param_op,&ctx.global_param_pt,&ctx.global_param_tr}) {
  // Empty culled draws can retain a pre-compaction offset. Preserve it exactly;
  // it references no index. Every nonempty range must still be fully valid.
  for(size_t i=0;i<list->size();++i) {
   const auto& p=(*list)[i];
   const size_t limit=validationList==2&&vertexRanges[i]?ctx.verts.size():ctx.idx.size();
   if(p.count!=0&&(p.first>limit||p.count>limit-p.first)) {
    error="pvr-packet-draw-range";return false;
   }
  }
  ++validationList;
 }
 return true;
}
bool SnapshotPvrScenePacket(const rend_context& ctx,const std::array<float,16>& viewport,
 std::uint64_t frame,const std::string& game,PvrDecodedPacket& output,std::string& error) {
 std::vector<bool> ranges;std::size_t nonfinite=0,references=0;
 if(!ValidateContext(ctx,viewport,frame,game,ranges,nonfinite,references,error))return false;
 if(references||ctx.modtrig.size()>65536||ctx.framebufferWidth>4096||ctx.framebufferHeight>2160) {
  error="pvr-snapshot-unsupported-or-bound";return false;
 }
 PvrDecodedPacket result;result.frame=frame;result.game=game;result.gitSha=GIT_HASH;
#ifdef FLYCAST_ENABLE_NEURAL
 if (!ctx.sourceVertices.empty()) {
  if (!ctx.captureProducer.Available() || ctx.sourceVertices.size() > SourceObservationBatch::capacity) {
   error="pvr-snapshot-source-identity-or-bound";return false;
  }
  for (const auto& source : ctx.sourceVertices) {
   const auto& copy=source.copy;
   if (copy.decodedVertex>=ctx.verts.size() || copy.before!=copy.after ||
       std::memcmp(copy.after.data()+1,&ctx.verts[copy.decodedVertex].x,3*sizeof(float))) {
    error="pvr-snapshot-source-vertex-mismatch";return false;
   }
  }
  result.sourceProducer=ctx.captureProducer;
  result.sourceVertices=ctx.sourceVertices;
 }
#endif
 result.viewport=viewport;result.framebufferSize={ctx.framebufferWidth,ctx.framebufferHeight};
 result.clearFramebuffer=ctx.clearFramebuffer;result.vertices=ctx.verts;result.indices=ctx.idx;
 result.sortedTriangles=ctx.sortedTriangles;result.sortedOrderCaptured=true;
 result.unusedNonfinite=static_cast<std::uint32_t>(nonfinite);
 result.modifierTriangles=static_cast<std::uint32_t>(ctx.modtrig.size());
 result.omissions={"texture-pixels","fog-and-global-register-state","retained-framebuffer-pixels",
  "offscreen-culled-geometry","game-camera-and-lights","modifier-volume-geometry","Naomi2-matrices-and-lights"};
 const auto texture=[](const BaseTextureCacheData* t)->std::optional<PvrCapturedTexture> {
  if(!t)return std::nullopt;
  PvrCapturedTexture r;r.upload=t->Updates;r.rtt=t->rttGeneration;
  if(t->tcw.PixelFmt==PixelPal4||t->tcw.PixelFmt==PixelPal8)r.palette=t->palette_hash;
  return r;
 };
 unsigned listId=0;
 for(const auto* list:{&ctx.global_param_op,&ctx.global_param_pt,&ctx.global_param_tr}) {
  for(std::size_t i=0;i<list->size();++i) {
   const auto& p=(*list)[i];PvrCapturedDraw d;d.list=listId;d.ordinal=static_cast<std::uint32_t>(i);
   d.vertexRange=listId==2&&ranges[i];d.state=p;d.texture=texture(p.texture);d.texture1=texture(p.texture1);
   d.state.texture=nullptr;d.state.texture1=nullptr;result.draws.push_back(d);
  }
  ++listId;
 }
 for(const auto& p:ctx.render_passes)result.passes.push_back({p.op_count,p.pt_count,p.tr_count,p.mvo_count,p.sorted_tr_count,p.autosort,p.z_clear});
 output=std::move(result);error.clear();return true;
}
bool PvrSnapshotTextureBindingsMatch(const rend_context& ctx,const PvrDecodedPacket& packet) {
 const auto matches=[](const BaseTextureCacheData* live,const std::optional<PvrCapturedTexture>& saved) {
  if(!live)return !saved;
  if(!saved||saved->upload!=live->Updates||saved->rtt!=live->rttGeneration)return false;
  const bool paletted=live->tcw.PixelFmt==PixelPal4||live->tcw.PixelFmt==PixelPal8;
  return paletted ? saved->palette && *saved->palette==live->palette_hash : !saved->palette;
 };
 std::size_t index=0;unsigned listId=0;
 for(const auto* list:{&ctx.global_param_op,&ctx.global_param_pt,&ctx.global_param_tr}) {
  for(std::size_t ordinal=0;ordinal<list->size();++ordinal) {
   if(index>=packet.draws.size())return false;
   const auto& d=packet.draws[index++];const auto& live=(*list)[ordinal];
   if(d.list!=listId||d.ordinal!=ordinal||d.state.texture||d.state.texture1||
      d.state.tcw.full!=live.tcw.full||d.state.tcw1.full!=live.tcw1.full||
      !matches(live.texture,d.texture)||!matches(live.texture1,d.texture1))return false;
  }
  ++listId;
 }
 return index==packet.draws.size();
}
bool WritePvrScenePacket(const std::filesystem::path& path,const rend_context& ctx,
 const std::array<float,16>& viewport,std::uint64_t frame,const std::string& game,std::string& error) {
 std::vector<bool> vertexRanges;std::size_t nonfinitePositions=0,nonfiniteReferences=0;
 if(!ValidateContext(ctx,viewport,frame,game,vertexRanges,nonfinitePositions,nonfiniteReferences,error))return false;
 std::ostringstream out;out.imbue(std::locale::classic());
 out << "{\"schema\":\"flycast-pvr-scene-v2\",\"git_sha\":\"" << GIT_HASH
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
  "\"modifier-volume-geometry\",\"Naomi2-matrices-and-lights\"],"
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
   out << "{\"list\":"<<listId<<",\"ordinal\":"<<ordinal
    <<",\"range_space\":\""<<(listId==2&&vertexRanges[ordinal]?"vertices":"indices")<<"\",\"first\":"<<p.first
    <<",\"count\":"<<p.count<<",\"tsp\":"<<p.tsp.full<<",\"tcw\":"<<p.tcw.full
    <<",\"pcw\":"<<p.pcw.full<<",\"isp\":"<<p.isp.full<<",\"tileclip\":"<<p.tileclip
    <<",\"tsp1\":"<<p.tsp1.full<<",\"tcw1\":"<<p.tcw1.full
    <<",\"naomi2\":"<<(p.isNaomi2()?"true":"false")<<",\"texture\":";Texture(out,p.texture);
   out<<",\"texture1\":";Texture(out,p.texture1);out<<'}';
   ++ordinal;
  }
  ++listId;
 }
 out << "],\"sorted_triangles\":[";
 for(size_t i=0;i<ctx.sortedTriangles.size();++i) {
  const auto& s=ctx.sortedTriangles[i];if(i)out<<',';
  out<<"{\"poly_index\":"<<s.polyIndex<<",\"first\":"<<s.first<<",\"count\":"<<s.count<<'}';
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
