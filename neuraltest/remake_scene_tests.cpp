// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_scene.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace neuraltest::remake {
TestCounts TestSceneContract() {
 TestCounts counts;
 auto expect=[&](bool ok,const char* name) { ++(ok?counts.passed:counts.failed); std::cout<<(ok?"PASS ":"FAIL ")<<"remake "<<name<<'\n'; };
 auto near=[](float a,float b) {return std::abs(a-b)<1e-6f;};
 auto p=Synthetic();
 expect(Validate(p,7,"synthetic-overlap").ok,"bounded analytic packet");
 expect(ReadyForAdapter(p,7,p.game).ok,"synthetic adapter eligibility");
 const auto projected=Project(p.camera,{-1,-1,2});
 expect(near(projected.x,.25f)&&near(projected.y,.75f)&&near(projected.z,2),"independent pinhole golden coordinate");
 const auto world=Unproject(p.camera,{.25f,.75f,2});
 expect(near(world.x,-1)&&near(world.y,-1)&&near(world.z,2),"independent unprojection golden coordinate");
 auto depth=DepthAt(p,.5f,.5f);
 expect(depth&&near(*depth,2),"near primitive wins overlap");
 std::reverse(p.meshes.begin(),p.meshes.end());
 auto reversed=DepthAt(p,.5f,.5f);
 expect(reversed&&depth&&near(*reversed,*depth),"reversed submission retains depth ordering");
 expect(!DepthAt(p,.05f,.05f),"background has no geometry");
 auto moved=Synthetic(8,.5f);
 auto screen=Project(moved.camera,{-1,-1,2});
 expect(near(screen.x,.125f)&&near(screen.y,.75f),"known camera motion analytic displacement");
 expect(!near(Project(Synthetic().camera,{-1,-1,2}).x,.125f),"wrong camera fails moving golden");
 auto wrong=Unproject(p.camera,{.25f,.75f,.5f});
 expect(!near(wrong.x,-1)&&!near(wrong.z,2),"reciprocal depth misused as linear fails golden");
 auto wrongOrder=Synthetic();
 std::reverse(wrongOrder.meshes.begin(),wrongOrder.meshes.end());
 // Deliberately broken painter: choose the first submitted triangle's depth.
 const auto wrongDepth=Project(wrongOrder.camera,wrongOrder.meshes.front().vertices.front().position).z;
 expect(!near(wrongDepth,2),"wrong farther-first depth fails golden");
 auto reject=[&](Packet q,const char* reason,const char* name) {auto r=Validate(q,7,"synthetic-overlap"); expect(!r.ok&&r.reason==reason,name);};
 p=Synthetic(); auto q=p; q.version=2; reject(q,"schema","schema negative");
 q=p; q.frame=8; reject(q,"identity","frame mismatch negative");
 q=p; q.game="other"; reject(q,"identity","game mismatch negative");
 q=p; q.meshes[0].frame=8; reject(q,"mesh-frame","mesh frame mismatch negative");
 q=p; q.meshes[1].id=q.meshes[0].id; reject(q,"mesh-id","duplicate mesh ID negative");
 q=p; q.truncated=true; reject(q,"truncated","truncated capture negative");
 q=p; q.meshes[0].indices[0]=3; reject(q,"index","invalid index negative");
 q=p; q.meshes[0].vertices[0].position.x=std::numeric_limits<float>::quiet_NaN(); reject(q,"nonfinite","NaN negative");
 q=p; q.meshes[0].vertices[0].u=std::numeric_limits<float>::infinity(); reject(q,"nonfinite","infinite UV negative");
 q=p; (*q.meshes[0].transform)[3]=std::numeric_limits<float>::infinity(); reject(q,"transform","infinite transform negative");
 q=p; q.camera.nearPlane=0; reject(q,"camera","invalid camera negative");
 q=p; q.meshes[0].indices.pop_back(); reject(q,"topology","incomplete triangle negative");
 Limits l; l.vertices=5; expect(Validate(p,7,p.game,l).reason=="count-limit","aggregate vertex bound");
 l=Limits{}; l.indices=5; expect(Validate(p,7,p.game,l).reason=="count-limit","aggregate index bound");
 l=Limits{}; l.bytes=sizeof(Packet); expect(Validate(p,7,p.game,l).reason=="byte-limit","byte bound");
 l=Limits{}; l.meshes=1; expect(Validate(p,7,p.game,l).reason=="count-limit","mesh bound");
 q=p; q.camera.provenance=Provenance::Unknown;
 expect(Validate(q,7,q.game).ok && ReadyForAdapter(q,7,q.game).reason=="projection-unknown","unknown projection retained but not submitted");
 q=p; q.space=Space::PvrProjected;
 expect(Validate(q,7,q.game).ok && ReadyForAdapter(q,7,q.game).reason=="world-space-required","PVR data not relabeled world space");
 q=p; q.omissions={"off-screen geometry unavailable"};
 expect(Validate(q,7,q.game).ok && ReadyForAdapter(q,7,q.game).reason=="incomplete-scene","omissions explicit and fail closed");
 q=p; q.meshes[0].transform.reset(); expect(ReadyForAdapter(q,7,q.game).reason=="transform-unknown","unknown transform not identity");
 q=p; q.meshes[0].vertices[0].normal.reset(); expect(ReadyForAdapter(q,7,q.game).reason=="normal-unknown","unknown normal not fabricated");
 q=p; q.meshes[0].texture={31,2,3,4,true};
 expect(Validate(q,7,q.game).ok && ReadyForAdapter(q,7,q.game).reason=="textured-material-unsupported" && q.meshes[0].texture.rttGeneration==4,"texture generations retained without fake material");
 q=p; q.meshes[0].vertices[0].position.z=-1; expect(ReadyForAdapter(q,7,q.game).reason=="clip-unsupported","behind-camera rejection does not throw");
 Mesh strip=p.meshes[0]; strip.topology=Topology::Strip;
 strip.vertices.resize(8); strip.indices={0,1,2,3,3,4,4,5,6,7};
 expect(Triangles(strip)==std::vector<std::uint32_t>({0,1,2,2,1,3,4,5,6,6,5,7}),"strip parity survives degenerate restart sequence");
 auto normal=p.meshes[0]; expect(Triangles(normal)==normal.indices,"triangle list unchanged");
 bool threw=false; normal.indices[0]=999; try {Triangles(normal);} catch(const std::exception&) {threw=true;}
 expect(threw,"standalone topology conversion rejects invalid index");
 std::vector<float> reference;
 auto snapshot=[](const Packet& packet) {
  std::vector<float> result;
  for(const auto& mesh:packet.meshes) for(auto index:Triangles(mesh)) {
   const auto v=Project(packet.camera,WorldPosition(mesh,mesh.vertices[index]));
   result.insert(result.end(),{v.x,v.y,v.z});
  }
  return result;
 };
 reference=snapshot(Synthetic()); bool repeatable=true;
 for(int i=0;i<5;++i) repeatable &= snapshot(Synthetic())==reference;
 expect(repeatable&&reference.size()==18,"five independently generated projected packets exact");
 expect(snapshot(Synthetic(8,.5f))!=reference,"wrong-camera packet differs from deterministic reference");
 q=p; q.camera.provenance=Provenance::Supplied;
 expect(ReadyForAdapter(q,7,q.game).ok&&q.camera.provenance==Provenance::Supplied,"supplied camera provenance not upgraded to analytic");
 q=p; (*q.meshes[0].transform)[3]=1;
 expect(ReadyForAdapter(q,7,q.game).reason=="transform-unsupported","unsupported transform fails before normal fabrication");
 q=p; q.meshes[0].vertices[0].normal=Vec3{0,0,0}; reject(q,"normal","zero normal negative");
 q=p; q.space=static_cast<Space>(99); reject(q,"space","invalid coordinate enum negative");
 q=p; q.camera.provenance=static_cast<Provenance>(99); reject(q,"provenance","invalid provenance enum negative");
 l=Limits{}; l.vertices=0;
 expect(Validate(p,7,p.game,l).reason=="count-limit","zero vertex budget fails without unsigned underflow");
 return counts;
}
}
