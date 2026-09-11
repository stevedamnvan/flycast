// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Frozen pre-optimization algorithm for bit-exact differential tests.
namespace flycast::rend::neural {
inline void ReferenceSmoothRemakeViewNormals(RemakeViewMesh& mesh,int level) {
 constexpr float creaseCosine=0.5f; // 60 degrees.
 std::vector<std::pair<std::uint64_t,std::uint32_t>> order;order.reserve(mesh.vertices.size());
 if(level>=2) {
  // Exact attribute identity, hashed; collisions only merge vertices whose
  // 24 bytes of position/uv/colour agree, which is the weld condition itself.
  std::vector<std::pair<std::array<std::uint32_t,6>,std::uint32_t>> keyed;keyed.reserve(mesh.vertices.size());
  for(std::uint32_t i=0;i<mesh.vertices.size();++i) {
   const auto& v=mesh.vertices[i].source;std::array<std::uint32_t,6> key{};
   std::memcpy(&key[0],&v.x,4);std::memcpy(&key[1],&v.y,4);std::memcpy(&key[2],&v.z,4);
   std::memcpy(&key[3],&v.u,4);std::memcpy(&key[4],&v.v,4);std::memcpy(&key[5],v.col,4);
   keyed.emplace_back(key,i);
  }
  std::sort(keyed.begin(),keyed.end());
  std::uint64_t group=0;
  for(std::size_t i=0;i<keyed.size();++i){if(i&&keyed[i].first!=keyed[i-1].first)++group;order.emplace_back(group,keyed[i].second);}
 } else {
  for(std::uint32_t i=0;i<mesh.vertices.size();++i)order.emplace_back(mesh.vertices[i].sourceVertex,i);
 }
 std::sort(order.begin(),order.end());
 std::vector<std::array<float,3>> smoothed(mesh.vertices.size());
 for(std::size_t begin=0;begin<order.size();) {
  std::size_t end=begin;while(end<order.size()&&order[end].first==order[begin].first)++end;
  for(std::size_t i=begin;i<end;++i) {
   const auto& n=mesh.vertices[order[i].second].normal;float sx=0,sy=0,sz=0;
   for(std::size_t j=begin;j<end;++j) {
    const auto& m=mesh.vertices[order[j].second].normal;
    if(n[0]*m[0]+n[1]*m[1]+n[2]*m[2]<creaseCosine)continue;
    sx+=m[0];sy+=m[1];sz+=m[2];
   }
   const float length=std::sqrt(sx*sx+sy*sy+sz*sz);
   smoothed[order[i].second]=length>1e-12f?std::array<float,3>{sx/length,sy/length,sz/length}:n;
  }
  begin=end;
 }
 for(std::uint32_t i=0;i<mesh.vertices.size();++i)mesh.vertices[i].normal=smoothed[i];
}
}
