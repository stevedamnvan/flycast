// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// D-241 candidate (experimental, default off; FLYCAST_REMAKE_CURVED_EXPORT=1,
// requires smoothed export normals): curved point-normal triangles on the
// exported geometry. Each triangle whose three smoothed vertex normals disagree
// by more than a small angle is replaced by four sub-triangles lying on the
// cubic PN patch defined by its corners and normals (Vlachos et al. 2001), so a
// flat facet becomes a rounded patch before the consumer sees it. Coplanar
// facets (equal normals) are left untouched; texture coordinates and colours
// interpolate linearly; nothing is added to the source or its certificate.
// The whole packet stays under the wire vertex bound or no triangle is curved
// that frame, reported rather than partially applied.
#include "remake_scene.h"
#include "remake_chunk_workers.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace flycast::rend::neural {
struct RemakeCurvedExportReport {
 bool applied=false; // False when no triangle qualified or the bound would be exceeded.
 std::size_t meshes=0,curvedTriangles=0,flatTriangles=0,verticesBefore=0,verticesAfter=0;
 const char* reason="";
};
namespace curved_detail {
inline remake::Vec3 Add(remake::Vec3 a,remake::Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline remake::Vec3 Sub(remake::Vec3 a,remake::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline remake::Vec3 Mul(remake::Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(remake::Vec3 a,remake::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline remake::Vec3 Normalize(remake::Vec3 a){const float l=std::sqrt(Dot(a,a));return l>1e-12f?Mul(a,1.f/l):remake::Vec3{0,0,1};}
inline bool Finite(remake::Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Patch {
 remake::Vec3 b300,b030,b003,b210,b120,b021,b012,b102,b201,b111;
 remake::Vec3 n200,n020,n002,n110,n011,n101;
 remake::Vec3 Position(float u,float v)const {
  const float w=1-u-v,w2=w*w,u2=u*u,v2=v*v;
  remake::Vec3 p=Mul(b300,w2*w);p=Add(p,Mul(b030,u2*u));p=Add(p,Mul(b003,v2*v));
  p=Add(p,Mul(b210,3*w2*u));p=Add(p,Mul(b120,3*w*u2));p=Add(p,Mul(b201,3*w2*v));
  p=Add(p,Mul(b021,3*u2*v));p=Add(p,Mul(b102,3*w*v2));p=Add(p,Mul(b012,3*u*v2));
  return Add(p,Mul(b111,6*w*u*v));
 }
 remake::Vec3 Normal(float u,float v)const {
  const float w=1-u-v;
  remake::Vec3 n=Mul(n200,w*w);n=Add(n,Mul(n020,u*u));n=Add(n,Mul(n002,v*v));
  n=Add(n,Mul(n110,w*u));n=Add(n,Mul(n011,u*v));n=Add(n,Mul(n101,w*v));return Normalize(n);
 }
};
// Corners P1 (w), P2 (u), P3 (v) with unit normals.
inline Patch MakePatch(remake::Vec3 p1,remake::Vec3 p2,remake::Vec3 p3,remake::Vec3 n1,remake::Vec3 n2,remake::Vec3 n3) {
 Patch q;q.b300=p1;q.b030=p2;q.b003=p3;
 const auto edge=[](remake::Vec3 a,remake::Vec3 b,remake::Vec3 na){const float w=Dot(Sub(b,a),na);return Mul(Sub(Add(Mul(a,2),b),Mul(na,w)),1.f/3);};
 q.b210=edge(p1,p2,n1);q.b120=edge(p2,p1,n2);q.b021=edge(p2,p3,n2);q.b012=edge(p3,p2,n3);q.b102=edge(p3,p1,n3);q.b201=edge(p1,p3,n1);
 const auto e=Mul(Add(Add(Add(q.b210,q.b120),Add(q.b021,q.b012)),Add(q.b102,q.b201)),1.f/6);
 const auto v=Mul(Add(Add(p1,p2),p3),1.f/3);
 q.b111=Add(e,Mul(Sub(e,v),.5f));
 const auto mid=[](remake::Vec3 a,remake::Vec3 b,remake::Vec3 na,remake::Vec3 nb){
  const auto d=Sub(b,a);const float dd=Dot(d,d);const float s=dd>1e-20f?2*Dot(d,Add(na,nb))/dd:0;
  return Normalize(Sub(Add(na,nb),Mul(d,s)));};
 q.n200=n1;q.n020=n2;q.n002=n3;q.n110=mid(p1,p2,n1,n2);q.n011=mid(p2,p3,n2,n3);q.n101=mid(p3,p1,n3,n1);
 return q;
}
inline std::uint32_t LerpColor(std::uint32_t a,std::uint32_t b,std::uint32_t c,float wa,float wb,float wc) {
 std::uint32_t out=0;
 for(unsigned i=0;i<4;++i) {
  const float value=((a>>(8*i))&255)*wa+((b>>(8*i))&255)*wb+((c>>(8*i))&255)*wc;
  out|=std::uint32_t(std::clamp(int(value+.5f),0,255))<<(8*i);
 }
 return out;
}
}
// Normals that disagree by more than 20 degrees mark a curved facet, and only
// facets near the silhouette are curved: at least one vertex normal within
// about 60 degrees of grazing to the camera ray (the camera sits at the
// origin of the camera-relative packet). Interior facets already shade
// smoothly from the smoothed normals and would only add vertices.
constexpr float RemakeCurvedExportCosine=0.9397f;
constexpr float RemakeCurvedExportGrazing=0.5f;
inline bool RemakeTriangleIsCurved(const remake::Vertex& a,const remake::Vertex& b,const remake::Vertex& c) {
 using namespace curved_detail;
 if(!a.normal||!b.normal||!c.normal||!Finite(*a.normal)||!Finite(*b.normal)||!Finite(*c.normal))return false;
 if(!Finite(a.position)||!Finite(b.position)||!Finite(c.position))return false;
 const auto na=Normalize(*a.normal),nb=Normalize(*b.normal),nc=Normalize(*c.normal);
 if(Dot(na,nb)>=RemakeCurvedExportCosine&&Dot(nb,nc)>=RemakeCurvedExportCosine&&Dot(na,nc)>=RemakeCurvedExportCosine)return false;
 const auto grazing=[](const remake::Vertex& v,remake::Vec3 n){return std::abs(Dot(n,Normalize(v.position)))<RemakeCurvedExportGrazing;};
 return grazing(a,na)||grazing(b,nb)||grazing(c,nc);
}
// Curves the expanded triangle lists of every opaque or cutout mesh in place.
// Blended meshes and meshes whose indices are not a plain expanded list are
// left as they are. Deterministic for identical input.
inline RemakeCurvedExportReport CurveRemakePacket(remake::Packet& packet,std::size_t vertexBound=remake::Limits{}.vertices,RemakeChunkWorkers* workers=nullptr) {
 using namespace curved_detail;
 RemakeCurvedExportReport report;
 std::vector<bool> eligible(packet.meshes.size(),false);
 std::size_t after=0;
 for(std::size_t m=0;m<packet.meshes.size();++m) {
  const auto& mesh=packet.meshes[m];report.verticesBefore+=mesh.vertices.size();
  bool plain=!mesh.sourceAlphaBlend&&mesh.topology==remake::Topology::Triangles&&mesh.indices.size()==mesh.vertices.size()&&mesh.vertices.size()%3==0;
  for(std::size_t i=0;plain&&i<mesh.indices.size();++i)plain=mesh.indices[i]==i;
  if(!plain){after+=mesh.vertices.size();continue;}
  std::size_t curved=0;
  for(std::size_t i=0;i+2<mesh.vertices.size();i+=3)curved+=RemakeTriangleIsCurved(mesh.vertices[i],mesh.vertices[i+1],mesh.vertices[i+2]);
  eligible[m]=curved>0;report.curvedTriangles+=curved;report.flatTriangles+=mesh.vertices.size()/3-curved;
  after+=mesh.vertices.size()+curved*9;
 }
 report.verticesAfter=report.verticesBefore;
 if(report.curvedTriangles==0){report.reason="no-curved-triangles";return report;}
 if(after>vertexBound){report.reason="vertex-bound";return report;}
 // Meshes are independent: the chunk workers take them round-robin; the
 // per-mesh result is identical whichever thread produced it.
 const auto curveMesh=[&](std::size_t m) {
  auto& mesh=packet.meshes[m];std::vector<remake::Vertex> out;out.reserve(mesh.vertices.size()*4);
  for(std::size_t i=0;i+2<mesh.vertices.size();i+=3) {
   const auto& a=mesh.vertices[i];const auto& b=mesh.vertices[i+1];const auto& c=mesh.vertices[i+2];
   if(!RemakeTriangleIsCurved(a,b,c)){out.push_back(a);out.push_back(b);out.push_back(c);continue;}
   const Patch patch=MakePatch(a.position,b.position,c.position,Normalize(*a.normal),Normalize(*b.normal),Normalize(*c.normal));
   const auto at=[&](float u,float v){
    remake::Vertex x;const float w=1-u-v;
    x.position=patch.Position(u,v);x.normal=patch.Normal(u,v);
    x.u=a.u*w+b.u*u+c.u*v;x.v=a.v*w+b.v*u+c.v*v;x.publicColor=LerpColor(a.publicColor,b.publicColor,c.publicColor,w,u,v);
    return x;};
   // Corners keep their exact source attributes; edge midpoints lie on the patch.
   const auto A=a,B=b,C=c;auto AB=at(.5f,0),BC=at(.5f,.5f),CA=at(0,.5f);
   const remake::Vertex tris[12]={A,AB,CA, AB,B,BC, CA,BC,C, AB,BC,CA};
   out.insert(out.end(),std::begin(tris),std::end(tris));
  }
  mesh.vertices=std::move(out);mesh.indices.resize(mesh.vertices.size());
  for(std::size_t i=0;i<mesh.indices.size();++i)mesh.indices[i]=std::uint32_t(i);
 };
 std::vector<std::size_t> work;for(std::size_t m=0;m<packet.meshes.size();++m)if(eligible[m])work.push_back(m);
 report.meshes=work.size();
 const unsigned chunks=workers?unsigned(std::min<std::size_t>(4,std::max<std::size_t>(1,work.size()))):1u;
 if(chunks>1)workers->Run(chunks,[&](unsigned index){for(std::size_t i=index;i<work.size();i+=chunks)curveMesh(work[i]);});
 else for(auto m:work)curveMesh(m);
 report.verticesAfter=after;report.applied=true;report.reason="applied";return report;
}
}
