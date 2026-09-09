// SPDX-License-Identifier: GPL-2.0-or-later
// Bounded diagnostic stdin/stdout bridge to the actual float projection code.
// This does not accept a scene for rendering or infer game clipping planes.
#include "remake_scene.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace neuraltest::remake;
int main(int argc,char** argv) {
 try {
  if(argc==2 && std::string(argv[1])=="--flat-normals") {
   unsigned vertices=0,indices=0;
   if(!(std::cin>>vertices>>indices) || vertices==0 || vertices>65536
     || indices==0 || indices>262144) throw std::runtime_error("mesh bounds");
   Mesh mesh;mesh.vertices.resize(vertices);mesh.indices.resize(indices);
   for(auto& v:mesh.vertices) if(!(std::cin>>v.position.x>>v.position.y>>v.position.z))
    throw std::runtime_error("mesh position");
   for(auto& i:mesh.indices) if(!(std::cin>>i))throw std::runtime_error("mesh index");
   std::cin>>std::ws;if(!std::cin.eof())throw std::runtime_error("trailing input");
   // Caller-supplied reconstructed coordinates, not an accepted game world.
   const auto flat=DeriveFlatNormals(mesh,Space::View);
   std::cout<<flat.mesh.vertices.size()<<' '<<flat.degenerateTriangles<<'\n'<<std::setprecision(9);
   for(std::size_t i=0;i<flat.mesh.vertices.size();i+=3) {
    const auto n=*flat.mesh.vertices[i].normal;
    std::cout<<n.x<<' '<<n.y<<' '<<n.z<<'\n';
   }
   return 0;
  }
  if(argc!=1)throw std::runtime_error("unknown mode");
  auto c=Synthetic().camera;
  auto read=[](Vec3& v) { return bool(std::cin>>v.x>>v.y>>v.z); };
  if(!read(c.position)||!read(c.right)||!read(c.up)||!read(c.forward)
    ||!(std::cin>>c.fovY>>c.aspect)) throw std::runtime_error("camera input");
  unsigned count=0;
  if(!(std::cin>>count)||count==0||count>65536) throw std::runtime_error("point bound");
  std::cout<<std::setprecision(9);
  for(unsigned i=0;i<count;++i) {
   Vec3 v;
   if(!read(v)) throw std::runtime_error("point input");
   auto p=Project(c,v);
   std::cout<<p.x<<' '<<p.y<<' '<<p.z<<'\n';
  }
  std::cin>>std::ws;
  if(!std::cin.eof()) throw std::runtime_error("trailing input");
  return 0;
 } catch(const std::exception& e) {
  std::cerr<<"projection rejected: "<<e.what()<<'\n';return 1;
 }
}
