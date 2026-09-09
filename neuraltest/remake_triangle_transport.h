// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include <algorithm>
#include <stdexcept>

namespace neuraltest::remake {
// Diagnostic decomposition only: preserves triangle attributes, not game bones.
inline Packet TriangleBatches(const Packet& input) {
 Packet result=input;result.meshes.clear();
 for(const auto& mesh:input.meshes) {
  const auto indices=Triangles(mesh);
  for(std::size_t start=0;start<indices.size();start+=768) {
   if(mesh.id>UINT64_MAX/1024 || start/768>=1024 || result.meshes.size()>=128)
    throw std::invalid_argument("triangle batch bound");
   Mesh batch=mesh;batch.id=mesh.id*1024+start/768;
   batch.vertices.clear();batch.indices.clear();batch.topology=Topology::Triangles;
   for(auto j=start;j<std::min(start+768,indices.size());++j) {
    batch.indices.push_back(static_cast<std::uint32_t>(batch.vertices.size()));
    batch.vertices.push_back(mesh.vertices.at(indices[j]));
   }
   result.meshes.push_back(std::move(batch));
  }
 }
 return result;
}
inline std::array<float,12> TriangleAffine(const Vertex* old,const Vertex* next) {
 using V=std::array<double,3>;
 const auto point=[](Vec3 p)->V{return {p.x,p.y,p.z};};
 const auto sub=[](V a,V b)->V{return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};};
 const auto dot=[](V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
 const auto cross=[](V a,V b)->V{return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
 const auto basis=[&](const Vertex* v){
  std::array<V,3> b{sub(point(v[1].position),point(v[0].position)),sub(point(v[2].position),point(v[0].position)),{}};
  b[2]=cross(b[0],b[1]);const double length=std::sqrt(dot(b[2],b[2]));
  const double scale=std::sqrt(dot(b[0],b[0])*dot(b[1],b[1]));
  if(!std::isfinite(scale)||!scale||length<=scale*1e-8)throw std::invalid_argument("triangle affine degeneracy");
  for(auto& x:b[2])x/=length;return b;
 };
 const auto a=basis(old),b=basis(next);
 const double det=dot(a[0],cross(a[1],a[2]));
 std::array<V,3> inverse{cross(a[1],a[2]),cross(a[2],a[0]),cross(a[0],a[1])};
 for(auto& row:inverse)for(auto& value:row)value/=det;
 std::array<float,12> result{};
 for(int row=0;row<3;++row) {
  V linear{};for(int col=0;col<3;++col)for(int k=0;k<3;++k)linear[col]+=b[k][row]*inverse[k][col];
  for(int col=0;col<3;++col)result[row*4+col]=float(linear[col]);
  result[row*4+3]=float(point(next[0].position)[row]-dot(linear,point(old[0].position)));
 }
 for(float x:result)if(!std::isfinite(x))throw std::invalid_argument("triangle affine nonfinite");
 return result;
}
}
