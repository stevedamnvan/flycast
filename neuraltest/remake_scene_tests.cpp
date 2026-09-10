// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_scene.h"
#include "remake_legacy_contract.h"
#include "remake_scene_lighting.h"
#include "remake_runtime_budget.h"
#include "rend/neural/remake_presentation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace neuraltest::remake {
TestCounts TestSceneContract() {
 TestCounts counts;
 auto expect=[&](bool ok,const char* name) { ++(ok?counts.passed:counts.failed); std::cout<<(ok?"PASS ":"FAIL ")<<"remake "<<name<<'\n'; };
 using flycast::rend::neural::RemakeEffectCaptureBound;
 using flycast::rend::neural::RemakeComparisonBeforeEnd;
 using flycast::rend::neural::RemakePresentationPolicy;
 using flycast::rend::neural::RemakeDisplayKind;
 RemakePresentationPolicy boundary;
 expect(boundary.Choose(100,0,true,true).kind==RemakeDisplayKind::HoldNative,"capture boundary holds native before delayed result");
 const auto first=boundary.Choose(102,100,true);
 expect(first.kind==RemakeDisplayKind::Remake&&first.frame==100,"capture boundary retains first source without time reversal");
 boundary.Reset();boundary.Choose(100,0,true,true);
 expect(boundary.Choose(109,0,true).kind==RemakeDisplayKind::Fallback,"capture boundary keeps eight-frame timeout");
 expect(RemakeEffectCaptureBound(nullptr,false)==30,"effect capture legacy bound");
 expect(RemakeEffectCaptureBound("1",true)==300,"effect capture explicit extended bound");
 expect(RemakeEffectCaptureBound("1",false)==0&&RemakeEffectCaptureBound("10",true)==0,"effect capture unbounded and malformed rejected");
 expect(RemakeComparisonBeforeEnd("3000",3000,true)&&!RemakeComparisonBeforeEnd("3000",3001,true),"comparison end inclusive");
 expect(!RemakeComparisonBeforeEnd("3000",2000,false)&&!RemakeComparisonBeforeEnd("10000001",1,true)
  &&!RemakeComparisonBeforeEnd("x",1,true),"comparison end rejects invalid or unbounded use");
 auto near=[](float a,float b) {return std::abs(a-b)<1e-6f;};
 auto p=Synthetic();
 expect(RemakeWorkerFrameLimit(false,true,660)==660,"legacy helper frame bound unchanged");
 expect(!RemakeWorkerFrameLimit(false,true,661),"legacy oversized helper rejected");
 expect(RemakeWorkerFrameLimit(true,true,660)==10000,"session worker has explicit finite cap");
 expect(!RemakeWorkerFrameLimit(true,false,660),"worker requires returned scene route");
 expect(!RemakeWorkerFrameLimit(true,true,120),"worker rejects ambiguous short diagnostic request");
 {
  auto q=p;q.diagnosticEmbeddingProvenance="diagnostic-camera-embedded-anchor-not-world-reconstruction";
  q.diagnosticOrigin=Vec3{};q.producer={1,1,1};q.camera.forward={0,0,1};
  AnchoredSceneLight light;
  expect(light.Select(q).has_value(),"anchored light accepts qualified initial direction");
  q.camera.forward={1,0,0};
  expect(light.Select(q)->z==1,"anchored light ignores later camera rotation");
  auto bad=q;bad.producer.epoch++;
  expect(!light.Select(bad),"anchored light rejects different epoch");
  bad=q;bad.diagnosticEmbeddingProvenance="unknown";
  expect(!light.Select(bad),"anchored light rejects unanchored scope");
  bad=q;bad.game="other";
  expect(!light.Select(bad),"anchored light rejects different game");
  bad=q;bad.camera.forward={0,0,0};
  expect(!light.Select(bad),"anchored light rejects invalid direction");
  expect(light.Select(q)->z==1,"anchored light failures preserve original direction");
  auto regenerated=q;regenerated.diagnosticOrigin->x=1;regenerated.camera.forward={1,0,0};
  expect(light.Select(regenerated).has_value()&&light.Select(regenerated)->x==1&&light.Reanchors()==1,
   "anchored light re-anchors explicitly on a changed coordinate origin");
  regenerated.camera.forward={0,1,0};
  expect(light.Select(regenerated)->x==1&&light.Reanchors()==1,"anchored light keeps the re-anchored direction within a generation");
  bad=regenerated;bad.producer.epoch++;bad.diagnosticOrigin->x=2;
  expect(!light.Select(bad)&&light.Reanchors()==1,"anchored light still rejects an epoch change with a changed origin");
  auto a=q;a.frame=10;a.producer={1,10,100};a.game="T1401N";a.sourceGitSha="fixture";
  auto b=a;b.frame=11;b.producer={1,11,101};
  expect(!AnchorGenerationChange(a,b)&&DiagnosticContinuation(a,b),"unchanged origin is continuity, not a generation change");
  b.diagnosticOrigin->x=2;
  expect(AnchorGenerationChange(a,b)&&!DiagnosticContinuation(a,b)&&!AsyncSourceContinuation(a,b),
   "changed origin on a continuing producer chain is an explicit generation change");
  auto c=b;c.producer.epoch++;expect(!AnchorGenerationChange(a,c),"generation change requires the same epoch");
  c=b;c.producer.ordinal=10;expect(!AnchorGenerationChange(a,c),"generation change requires an advancing producer");
  c=b;c.diagnosticEmbeddingProvenance="unknown";expect(!AnchorGenerationChange(a,c),"generation change requires anchored scope");
  c=b;c.diagnosticOrigin->x=std::numeric_limits<float>::quiet_NaN();expect(!AnchorGenerationChange(a,c),"generation change requires a finite origin");
  c=b;c.sourceGitSha="other";expect(!AnchorGenerationChange(a,c),"generation change requires the same source build");
 }
 expect(RemakeRuntimeBudget(false,false,120)==30u,"ordinary runtime budget unchanged");
 expect(RemakeRuntimeBudget(false,true,120)==30u,"short returned-scene budget unchanged");
 expect(RemakeRuntimeBudget(false,true,121)==120u,"extended returned-scene budget unchanged");
 expect(RemakeRuntimeBudget(false,true,660)==120u,"performance budget not extended implicitly");
 expect(!RemakeRuntimeBudget(true,false,120),"capture budget rejects unsupported route");
 expect(RemakeRuntimeBudget(true,true,660)==300u,"explicit diagnostic budget remains bounded");
 for(const auto* value:{L"0",L"0.03",L"1",L"3",L"30"})
  expect(ParseSceneLightRadiance(value).has_value(),"bounded authored light accepts decimal");
 expect(ParseSceneLightRadiance(L"0.03")==.03f,"authored light preserves fractional value");
 for(const auto* value:{L"",L".",L"-1",L"30.01",L"nan",L"inf",L"1e2",L"1junk",L" 3",L"1.2.3",L"9999999999999"})
  expect(!ParseSceneLightRadiance(value),"invalid authored light rejects");
 {
  std::vector<unsigned char> bytes(152,0);
  auto word=[&](unsigned at,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes[at+i]=static_cast<unsigned char>(value>>(8*i));};
  word(0,0x20534444);word(4,124);word(8,0x100f);word(12,1);word(16,1);
  word(20,4);word(28,1);word(76,32);word(80,4);word(84,0x30315844);
  word(108,0x1000);word(128,28);word(132,3);word(140,1);
  bytes[148]=255;bytes[151]=255;
  expect(ValidSourceDdsBytes(bytes),"owned RGBA texture validates without a file");
  auto q=p;auto& material=*q.meshes[0].material;
  material.sourceColorExperiment=true;material.sourceDdsBytes=bytes;
  expect(ReadyForAdapter(q,q.frame,q.game).ok,"owned texture passes scene contract");
  bytes[148]=0;
  expect(material.sourceDdsBytes[148]==255,"owned texture does not borrow producer storage");
  auto copy=q;copy.meshes[0].material->sourceDdsBytes[148]=0;
  expect(material.sourceDdsBytes[148]==255,"packet texture copy survives independent mutation");
  material.sourceDds="ambiguous.dds";
  expect(ReadyForAdapter(q,q.frame,q.game).reason=="source-texture-contract","two texture sources reject");
  const auto before=material.sourceDdsBytes;
  expect(!OwnDiagnosticTextures(q).ok && material.sourceDds=="ambiguous.dds" && material.sourceDdsBytes==before,
   "failed ownership conversion preserves packet");
  material.sourceDds.clear();material.sourceDdsBytes[128]=29;
  expect(ReadyForAdapter(q,q.frame,q.game).reason=="source-texture-contract","owned wrong pixel format rejects");
  material.sourceDdsBytes=bytes;material.sourceDdsBytes.pop_back();
  expect(!ValidSourceDdsBytes(material.sourceDdsBytes),"owned truncated mip rejects");
  material.sourceDdsBytes=bytes;material.sourceDdsBytes.push_back(0);
  expect(!ValidSourceDdsBytes(material.sourceDdsBytes),"owned trailing bytes reject");
  material.sourceDdsBytes=bytes;Limits limits;limits.textureBytes=151;
  expect(Validate(q,q.frame,q.game,limits).reason=="texture-byte-limit","owned texture counts toward payload budget");
  q.meshes[1].material->sourceDdsBytes=bytes;limits.textureBytes=303;
  expect(Validate(q,q.frame,q.game,limits).reason=="texture-byte-limit","owned texture budget is aggregate across draws");
 }
 {
  auto base=p.meshes[0];base.sourceTsp=(3u<<6)|(1u<<13);
  expect(LegacySamplingSupported(base),"legacy linear modulation supported");
  for(std::uint8_t threshold:{0,128,255}) {
   auto cutout=base;cutout.sourceAlphaReference=threshold;
   expect(LegacySamplingSupported(cutout),"legacy cutout carries supported alpha shading");
   cutout.sourceTsp=*cutout.sourceTsp|(3u<<22);
   expect(!LegacySamplingSupported(cutout),"legacy cutout rejects fog-replaced alpha");
  }
  auto changed=base;changed.sourceTsp.reset();expect(!LegacySamplingSupported(changed),"legacy missing sampler rejected");
  changed=base;changed.sourceTsp=(3u<<6)|(2u<<13);expect(!LegacySamplingSupported(changed),"legacy trilinear rejected");
  changed=base;changed.sourceTsp=0;expect(!LegacySamplingSupported(changed),"legacy unsupported shading rejected");
  changed=base;changed.vertices[0].position.x+=1;changed.vertices[0].u+=.1f;changed.vertices[0].publicColor=0;
  expect(LegacyResourceCompatible(base,changed),"legacy changing attributes retain resource");
  changed=base;changed.texture.generation++;expect(!LegacyResourceCompatible(base,changed),"legacy texture revision rejected");
  changed=base;changed.texture.paletteGeneration++;expect(!LegacyResourceCompatible(base,changed),"legacy palette revision rejected");
  changed=base;changed.texture.rttGeneration++;expect(!LegacyResourceCompatible(base,changed),"legacy RTT revision rejected");
  changed=base;std::swap(changed.indices[0],changed.indices[1]);expect(!LegacyResourceCompatible(base,changed),"legacy reindex rejected");
  changed=base;changed.id++;expect(!LegacyResourceCompatible(base,changed),"legacy draw identity rejected");
 }
 {
  auto previous=p;previous.frame=1782;previous.sourceGitSha="fixture";previous.diagnosticOrigin=Vec3{1,2,3};
  auto next=previous;next.frame=1783;
  expect(DiagnosticContinuation(previous,next),"sequence consecutive source");
  auto changed=next;changed.frame=1784;expect(!DiagnosticContinuation(previous,changed),"sequence skipped frame rejected");
  changed=next;changed.game="other";expect(!DiagnosticContinuation(previous,changed),"sequence game rejected");
  changed=next;changed.sourceGitSha="other";expect(!DiagnosticContinuation(previous,changed),"sequence SHA rejected");
  changed=next;changed.diagnosticOrigin->x+=1;expect(!DiagnosticContinuation(previous,changed),"sequence origin rejected");
  changed=next;changed.diagnosticOrigin.reset();expect(!DiagnosticContinuation(previous,changed),"sequence missing origin rejected");
  changed=next;changed.diagnosticOrigin->x=std::numeric_limits<float>::quiet_NaN();expect(!DiagnosticContinuation(previous,changed),"sequence nonfinite origin rejected");
 }
 {
  auto m=p.meshes[0];m.vertices[0].publicColor=0x12345678;
  auto flat=DeriveFlatNormals(m,Space::World);
  expect(flat.mesh.vertices.size()==3 && flat.mesh.vertices[0].normal->z==1
   && flat.mesh.vertices[0].publicColor==0x12345678 && flat.mesh.vertices[0].v==1
   && m.vertices[0].normal->z==-1,"derived face preserves attributes and source");
  std::swap(m.indices[1],m.indices[2]);flat=DeriveFlatNormals(m,Space::World);
  expect(flat.mesh.vertices[0].normal->z==-1,"derived face winding reversal");
  for(auto& v:m.vertices) {const auto y=v.position.y;v.position.y=-v.position.z;v.position.z=y;}
  flat=DeriveFlatNormals(m,Space::View);
  expect(flat.mesh.vertices[0].normal->y==1,"derived face proper rotation");
  m.indices={0,0,1};flat=DeriveFlatNormals(m,Space::World);
  expect(flat.degenerateTriangles==1 && flat.mesh.vertices.empty(),"derived repeated index omission");
  m=p.meshes[0];m.vertices[2].position=m.vertices[0].position;
  flat=DeriveFlatNormals(m,Space::World);
  expect(flat.degenerateTriangles==1,"derived coincident position omission");
  auto rejected=[&](Mesh mesh,Space space,Limits limits) {
   try {DeriveFlatNormals(mesh,space,limits);return false;}catch(const std::invalid_argument&) {return true;}
  };
  expect(rejected(p.meshes[0],Space::PvrProjected,{}),"derived projected domain rejects");
  Limits tiny;tiny.vertices=2;
  expect(rejected(p.meshes[0],Space::World,tiny),"derived vertex budget rejects");
  tiny=Limits{};tiny.bytes=1;
  expect(rejected(p.meshes[0],Space::World,tiny),"derived expansion byte budget rejects");
  m=p.meshes[0];m.indices[0]=999;
  expect(rejected(m,Space::World,{}),"derived invalid index rejects");
  m=p.meshes[0];m.vertices[0].position.x=std::numeric_limits<float>::infinity();
  expect(rejected(m,Space::World,{}),"derived nonfinite rejects");
  m=p.meshes[0];m.topology=Topology::Strip;
  m.indices={0,1,2,2,1,0};flat=DeriveFlatNormals(m,Space::World);
  expect(flat.degenerateTriangles==2 && flat.mesh.vertices.size()==6
   && flat.mesh.vertices[0].normal->z==1 && flat.mesh.vertices[3].normal->z==1,
   "derived strip parity survives degenerate break");
  tiny=Limits{};tiny.vertices=6;
  expect(rejected(m,Space::World,tiny),"derived worst case expansion rejects before omissions");
 }
 {
  auto q=p;q.meshes[0].material.reset();
  expect(ReadyForAdapter(q,7,q.game).reason=="material-unknown","unknown material not synthetic default");
  q=p;q.meshes[0].material->roughness=-1;
  expect(ReadyForAdapter(q,7,q.game).reason=="material-parameters","invalid material rejects");
  q=p;q.meshes[0].material->sourceDds="relative.dds";
  expect(ReadyForAdapter(q,7,q.game).reason=="source-texture-contract","texture path requires explicit source experiment");
  q.meshes[0].material->sourceColorExperiment=true;
  expect(ReadyForAdapter(q,7,q.game).reason=="source-texture-contract","relative texture path rejects");
  q.meshes[0].material->sourceDds=std::string(4097,'x');
  expect(Validate(q,7,q.game).reason=="byte-limit","oversized owned texture path rejects before copy");
 }
 expect(PublicColorFromBgra({0,0,255,255})==0xffff0000u
  && PublicColorFromBgra({255,0,0,128})==0x800000ffu,
  "public BGRA red blue alpha independent word goldens");
 {
  // Recovered H lens/basis; clip planes remain synthetic, not game evidence.
  auto c=p.camera;
  c.position={-4.402202805233795f,-1.1753900732667373f,4.5792354100240775f};
  c.right={.9999993807275268f,0,.0011504740232752663f};
  c.up={.000018860498003155206f,-.9998656571636007f,-.01639366691737305f};
  c.forward={.0011503193527460098f,.01639367640018463f,-.9998649954795837f};
  c.fovY=45.99033235267587f;c.aspect=1.2266665409901234f;
  auto q=p;q.camera=c;
  expect(Validate(q,7,q.game).ok,"recovered camera float axes validate with synthetic clips");
  const auto s=Project(c,{0,0,0});
  // Independent calibrated matrix projection, not an Unproject-generated point.
  const float error=std::max(std::abs(s.x-1.417501739858114f)*640,
                             std::abs(s.y-.7815836510728279f)*480);
  std::cout<<"recovered camera golden pixel error "<<error<<'\n';
  expect(error<.001f&&near(s.z,4.602950096130371f),"recovered camera float projection golden");
  const auto w=Unproject(c,s);
  const float inverseError=std::max({std::abs(w.x),std::abs(w.y),std::abs(w.z)});
  std::cout<<"recovered camera inverse coordinate error "<<inverseError<<'\n';
  expect(inverseError<1e-6f,"recovered camera float inverse residual");
  c.aspect=4.f/3.f;
  expect(std::abs(Project(c,{0,0,0}).x-s.x)*640>.001f,"framebuffer aspect substitution fails recovered golden");
 }
 {
  auto rotated=p.camera; rotated.right={0,0,-1};rotated.forward={1,0,0};
  auto s=Project(rotated,{2,1,-1});auto w=Unproject(rotated,{.75f,.25f,2});
  expect(near(s.x,.75f)&&near(s.y,.25f)&&near(s.z,2),"rotated camera independent golden");
  expect(near(w.x,2)&&near(w.y,1)&&near(w.z,-1),"rotated camera inverse golden");
  auto invalid=p;invalid.camera.up={1,0,0};
  expect(!Validate(invalid,7,p.game).ok,"nonorthogonal camera axes reject");
  invalid=p;invalid.camera.right={-1,0,0};
  expect(!Validate(invalid,7,p.game).ok,"reflected camera basis rejects");
 }
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
