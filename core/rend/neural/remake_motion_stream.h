// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_temporal_scene.h"
#include "motion_reference.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace flycast::rend::neural {
struct RemakeMotionVertex {
 remake::Vec3 currentScreen,previousScreen; // Unjittered render pixels and view Z.
 float confidence=0;
 std::uint32_t currentDraw=0,previousDraw=0;
};
struct RemakeMotionStream {
 std::vector<RemakeMotionVertex> vertices;
 std::vector<std::uint32_t> indices;
 std::uint32_t trustedDraws=0,reactiveDraws=0,ambiguousDraws=0,trustedVertices=0;
 float maximumMotion=0;
};
namespace remake_motion_detail {
inline void hash(std::uint32_t& h,std::uint32_t value) {
 for(unsigned i=0;i<4;++i){h^=(value>>(8*i))&255;h*=16777619u;}
}
inline std::uint32_t bits(float x){std::uint32_t out;std::memcpy(&out,&x,4);return out;}
inline std::uint32_t generation(std::uint64_t x){return std::uint32_t(x)^std::uint32_t(x>>32);}
inline bool exact(const RemakeTemporalMesh& a,const RemakeTemporalMesh& b) {
 if(a.texture.known!=b.texture.known||a.texture.id!=b.texture.id||a.texture.generation!=b.texture.generation
  ||a.texture.paletteGeneration!=b.texture.paletteGeneration||a.texture.rttGeneration!=b.texture.rttGeneration
  ||a.sourceTsp!=b.sourceTsp||a.alphaReference!=b.alphaReference||a.alphaBlend!=b.alphaBlend
  ||a.indices!=b.indices||a.vertices.size()!=b.vertices.size())return false;
 for(std::size_t i=0;i<a.vertices.size();++i)
  if(a.vertices[i].u!=b.vertices[i].u||a.vertices[i].v!=b.vertices[i].v||a.vertices[i].publicColor!=b.vertices[i].publicColor)return false;
 return true;
}
inline bool records(const RemakeTemporalScene& scene,std::vector<DrawRecord>& out,
 std::vector<std::vector<remake::Vec3>>& screens) {
 if(scene.meshes.empty()||scene.meshes.size()>128)return false;
 std::size_t vertices=0,indices=0;
 for(std::size_t ordinal=0;ordinal<scene.meshes.size();++ordinal) {
  const auto& mesh=scene.meshes[ordinal];
  if(mesh.vertices.size()<3||mesh.vertices.size()>65536-vertices||mesh.indices.size()<3
   ||mesh.indices.size()>262144-indices||mesh.indices.size()%3||(mesh.id>>32)>65535)return false;
  vertices+=mesh.vertices.size();indices+=mesh.indices.size();
  DrawRecord d;d.ordinal=std::uint16_t(ordinal);d.list=std::uint16_t(mesh.id>>32);d.stripCount=1;
  d.vertexCount=std::uint32_t(mesh.vertices.size());d.indexCount=std::uint32_t(mesh.indices.size());
  d.texId=std::uint32_t(mesh.texture.id);d.texId2=std::uint32_t(mesh.texture.id>>32);
  d.textureGeneration=generation(mesh.texture.generation);d.paletteGeneration=generation(mesh.texture.paletteGeneration);
  d.rttGeneration=generation(mesh.texture.rttGeneration);d.stateSig=2166136261u;
  hash(d.stateSig,mesh.sourceTsp);hash(d.stateSig,mesh.alphaReference?unsigned(*mesh.alphaReference):256u);
  hash(d.stateSig,mesh.alphaBlend);hash(d.stateSig,mesh.texture.known);
  d.flags=DrawTriangleList|((mesh.alphaBlend||mesh.alphaReference)?DrawReactive:0);
  d.uvSig=d.topologySig=2166136261u;d.zMin=1;d.zMax=0;
  float lo[2]={32767,32767},hi[2]={-32768,-32768};double sum[2]{};
  std::vector<remake::Vec3> projected;projected.reserve(mesh.vertices.size());
  for(const auto& v:mesh.vertices) {
   if(!std::isfinite(v.u)||!std::isfinite(v.v))return false;
   auto p=remake::Project(scene.camera,v.position);p.x*=640;p.y*=480;
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
   projected.push_back(p);hash(d.uvSig,bits(v.u));hash(d.uvSig,bits(v.v));
   // Assignment pose hints cover the content rectangle; actual positions stay unclamped.
   const float xy[]={std::clamp(p.x,0.f,640.f),std::clamp(p.y,0.f,480.f)};
   for(unsigned k=0;k<2;++k){lo[k]=(std::min)(lo[k],xy[k]);hi[k]=(std::max)(hi[k],xy[k]);sum[k]+=xy[k];}
   const float n=scene.camera.nearPlane,f=scene.camera.farPlane;
   const float z=1-(f/(f-n)-n*f/((f-n)*p.z));d.zMin=(std::min)(d.zMin,z);d.zMax=(std::max)(d.zMax,z);
  }
  for(auto i:mesh.indices){if(i>=mesh.vertices.size())return false;hash(d.topologySig,i);}
  for(unsigned k=0;k<2;++k) {
   d.centroid[k]=float(sum[k]/mesh.vertices.size());
   d.bboxMin[k]=std::int16_t(std::clamp(std::floor(lo[k]),-32768.f,32767.f));
   d.bboxMax[k]=std::int16_t(std::clamp(std::ceil(hi[k]),-32768.f,32767.f));
  }
  out.push_back(d);screens.push_back(std::move(projected));
 }
 return true;
}
}
// Previous must be the compatible last successful evaluation, not last publication.
// This produces geometry candidates only; returned-depth/disocclusion and GPU
// interpolation still decide per-pixel trust before neural history is enabled.
inline bool BuildRemakeMotionStream(const RemakeTemporalScene* previous,const RemakeTemporalScene& current,
 RemakeMotionStream& output,std::string& error) {
 try {
  if(previous&&!CompatibleRemakeTemporalReference(*previous,current))previous=nullptr;
  std::vector<DrawRecord> before,now;std::vector<std::vector<remake::Vec3>> oldScreen,newScreen;
  if(!remake_motion_detail::records(current,now,newScreen)
   ||(previous&&!remake_motion_detail::records(*previous,before,oldScreen))) {error="remake-motion-source-bound";return false;}
  const auto matches=MatchDraws({before.data(),before.size()},{now.data(),now.size()});
  RemakeMotionStream result;
  for(std::size_t i=0;i<now.size();++i) {
   const auto& match=matches[i];const auto prior=match.prevOrdinal;const auto& mesh=current.meshes[i];
   bool trusted=previous&&match.tier==1&&match.confidence>=.5f&&prior<before.size()
    &&(!std::isfinite(match.secondBestCost)||match.secondBestCost-match.bestCost>=1.f)
    &&remake_motion_detail::exact(previous->meshes[prior],mesh)&&!IsReactive(now[i]);
   if(trusted)for(std::size_t v=0;v<mesh.vertices.size();++v) {
    const auto a=oldScreen[prior][v],b=newScreen[i][v];
    if(!ClassifyMotion(match,{a.x-b.x,a.y-b.y},false,false).trusted){trusted=false;break;}
   }
   result.trustedDraws+=trusted;result.reactiveDraws+=!trusted;
   result.ambiguousDraws+=match.reason==std::uint8_t(MatchReason::Ambiguous)
    ||(std::isfinite(match.secondBestCost)&&match.secondBestCost-match.bestCost<1.f&&match.tier!=0);
   const auto base=std::uint32_t(result.vertices.size());
   for(std::size_t v=0;v<mesh.vertices.size();++v) {
    const auto p=newScreen[i][v];const auto q=trusted?oldScreen[prior][v]:p;
    result.vertices.push_back({p,q,trusted?match.confidence:0.f,std::uint32_t(i+1),trusted?std::uint32_t(prior+1):0u});
    if(trusted){++result.trustedVertices;result.maximumMotion=(std::max)(result.maximumMotion,std::hypot(q.x-p.x,q.y-p.y));}
   }
   for(auto index:mesh.indices)result.indices.push_back(base+index);
  }
  output=std::move(result);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
}
