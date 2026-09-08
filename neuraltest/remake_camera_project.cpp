// SPDX-License-Identifier: GPL-2.0-or-later
// Bounded diagnostic stdin/stdout bridge to the actual float projection code.
// This does not accept a scene for rendering or infer game clipping planes.
#include "remake_scene.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace neuraltest::remake;
int main() {
 try {
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
