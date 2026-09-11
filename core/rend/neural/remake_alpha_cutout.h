// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// D-240 hair option 1: promoted alpha surfaces (D-183) whose sampled texture
// alpha is a cutout, not a gradient, travel to the consumer as alpha-tested
// cutouts instead of blended translucent material. The decision uses only the
// draw's own texture bytes and its texture-coordinate footprint; no material
// identity is inferred from ordinal, position or any external configuration.
// Draws that do not qualify keep native composition exactly as with the
// promotion disabled. Experimental, opt-in (FLYCAST_REMAKE_ALPHA_CUTOUT=1 with
// alpha ownership on); not a default and not a source-truth claim.
#include "remake_scene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
namespace flycast::rend::neural {
struct RemakeAlphaPlane { unsigned width=0,height=0; std::vector<std::uint8_t> alpha; };
// Mip 0 alpha of the owned RGBA8 DDS layout (148-byte DX10 header). Rejects
// anything that is not that layout; never guesses a format.
inline bool DecodeRemakeDdsAlphaPlane(const std::vector<unsigned char>& dds,RemakeAlphaPlane& out) {
 const auto word=[&](std::size_t offset){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(dds[offset+i])<<(8*i);return v;};
 if(dds.size()<148||word(0)!=0x20534444||word(4)!=124||word(84)!=0x30315844||word(128)!=28)return false;
 const auto height=word(12),width=word(16);
 if(!width||!height||width>4096||height>4096||dds.size()<148+std::size_t(width)*height*4)return false;
 RemakeAlphaPlane plane;plane.width=width;plane.height=height;plane.alpha.resize(std::size_t(width)*height);
 for(std::size_t i=0;i<plane.alpha.size();++i)plane.alpha[i]=dds[148+i*4+3];
 out=std::move(plane);return true;
}
struct RemakeAlphaCutoutStatistics {
 bool decoded=false,wholeTexture=false;
 std::size_t texels=0,opaque=0,clear=0,mid=0; // opaque >= 240, clear <= 16, mid between.
 double Opaque()const{return texels?double(opaque)/double(texels):0;}
 double Clear()const{return texels?double(clear)/double(texels):0;}
 double Mid()const{return texels?double(mid)/double(texels):0;}
};
// Alpha inside the mesh's texture-coordinate footprint. A footprint that
// leaves one tile (wrapping or mirroring) measures the whole texture.
inline RemakeAlphaCutoutStatistics MeasureRemakeAlphaCutout(const RemakeAlphaPlane& plane,const remake::Mesh& mesh) {
 RemakeAlphaCutoutStatistics s;
 if(!plane.width||!plane.height||plane.alpha.size()!=std::size_t(plane.width)*plane.height||mesh.vertices.empty())return s;
 float u0=1e30f,u1=-1e30f,v0=1e30f,v1=-1e30f;
 for(const auto& v:mesh.vertices) {
  if(!std::isfinite(v.u)||!std::isfinite(v.v))return s;
  u0=std::min(u0,v.u);u1=std::max(u1,v.u);v0=std::min(v0,v.v);v1=std::max(v1,v.v);
 }
 unsigned x0=0,x1=plane.width,y0=0,y1=plane.height;
 if(u0>=0&&u1<=1&&v0>=0&&v1<=1) {
  x0=unsigned(std::floor(u0*plane.width));x1=std::min(plane.width,unsigned(std::ceil(u1*plane.width)));
  y0=unsigned(std::floor(v0*plane.height));y1=std::min(plane.height,unsigned(std::ceil(v1*plane.height)));
  if(x1<=x0)x1=std::min(plane.width,x0+1);if(y1<=y0)y1=std::min(plane.height,y0+1);
 }else s.wholeTexture=true;
 for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x) {
  const auto a=plane.alpha[std::size_t(y)*plane.width+x];++s.texels;
  if(a>=240)++s.opaque;else if(a<=16)++s.clear;else ++s.mid;
 }
 s.decoded=s.texels>0;return s;
}
// Alpha reference for a promoted cutout (the shader clips alpha*255 below it).
constexpr std::uint8_t RemakeAlphaCutoutReference=128;
// A cutout has both fully opaque and fully clear texels in the footprint and at
// most half of its texels on the edge between them (16-level source alpha
// strands). Gradients, uniform translucency and undecodable textures do not
// qualify and stay native.
inline bool RemakeAlphaCutoutQualifies(const RemakeAlphaCutoutStatistics& s) {
 return s.decoded&&s.texels>=4&&s.Opaque()>=.02&&s.Clear()>=.02&&s.Mid()<=.5;
}
// Alpha planes by texture identity for meshes whose bytes travel by reference
// (D-212). Bounded; a miss keeps the draw native and is counted, never guessed.
class RemakeAlphaPlaneCache {
 std::map<std::array<std::uint64_t,4>,std::shared_ptr<const RemakeAlphaPlane>> planes_;
 static std::array<std::uint64_t,4> Key(const remake::TextureIdentity& t){return {t.id,t.generation,t.paletteGeneration,t.rttGeneration};}
public:
 static constexpr std::size_t Bound=256;
 std::shared_ptr<const RemakeAlphaPlane> Find(const remake::TextureIdentity& identity)const {
  const auto found=planes_.find(Key(identity));return found==planes_.end()?nullptr:found->second;
 }
 std::shared_ptr<const RemakeAlphaPlane> Remember(const remake::TextureIdentity& identity,const std::vector<unsigned char>& dds) {
  if(auto held=Find(identity))return held;
  RemakeAlphaPlane plane;if(!DecodeRemakeDdsAlphaPlane(dds,plane))return nullptr;
  if(planes_.size()>=Bound)planes_.clear();
  auto shared=std::make_shared<const RemakeAlphaPlane>(std::move(plane));planes_.emplace(Key(identity),shared);return shared;
 }
 std::size_t Size()const{return planes_.size();}
 void Clear(){planes_.clear();}
};
// A surface whose vertex alpha (TSP UseAlpha, bit 20) makes it translucent is
// not a cutout whatever its texture alpha: it stays native.
inline bool RemakeVertexAlphaOpaque(const remake::Mesh& mesh) {
 if(!mesh.sourceTsp||!((*mesh.sourceTsp>>20)&1))return true;
 for(const auto& v:mesh.vertices)if((v.publicColor>>24)<250)return false;
 return true;
}
// A draw whose every vertex sits at one camera depth is a screen-aligned
// sprite or message (ring-out text, flashes), not scene geometry: it stays
// native. Relative spread below one part in ten thousand counts as constant.
inline bool RemakeDrawHasDepthExtent(const remake::Mesh& mesh) {
 if(mesh.vertices.empty())return false;
 float lo=mesh.vertices[0].position.z,hi=lo;
 for(const auto& v:mesh.vertices){if(!std::isfinite(v.position.z))return false;lo=std::min(lo,v.position.z);hi=std::max(hi,v.position.z);}
 return hi-lo>1e-4f*std::max(std::abs(hi),1e-6f);
}
struct RemakeAlphaCutoutPromotion {
 unsigned promoted=0,keptNative=0,undecoded=0; // Per packet; keptNative meshes are removed from the packet.
 std::vector<std::uint64_t> promotedIds;
};
// Rewrites the built packet in place: qualifying promoted alpha meshes become
// cutouts (blend off, reference set); the others are removed so their native
// composition is untouched. Opaque and already-cutout meshes are never changed.
inline RemakeAlphaCutoutPromotion PromoteRemakeAlphaCutouts(remake::Packet& packet,RemakeAlphaPlaneCache& cache) {
 RemakeAlphaCutoutPromotion result;std::vector<remake::Mesh> kept;kept.reserve(packet.meshes.size());
 for(auto& mesh:packet.meshes) {
  if(!mesh.sourceAlphaBlend||mesh.sourceAlphaReference){kept.push_back(std::move(mesh));continue;}
  std::shared_ptr<const RemakeAlphaPlane> plane;
  if(mesh.material&&!mesh.material->sourceDdsBytes.empty()&&mesh.texture.known)plane=cache.Remember(mesh.texture,mesh.material->sourceDdsBytes);
  else if(mesh.texture.known)plane=cache.Find(mesh.texture);
  if(!plane){++result.undecoded;++result.keptNative;continue;}
  const auto stats=MeasureRemakeAlphaCutout(*plane,mesh);
  if(!RemakeAlphaCutoutQualifies(stats)||!RemakeVertexAlphaOpaque(mesh)||!RemakeDrawHasDepthExtent(mesh)){++result.keptNative;continue;}
  mesh.sourceAlphaBlend=false;mesh.sourceAlphaReference=RemakeAlphaCutoutReference;
  ++result.promoted;result.promotedIds.push_back(mesh.id);kept.push_back(std::move(mesh));
 }
 packet.meshes=std::move(kept);return result;
}
}
