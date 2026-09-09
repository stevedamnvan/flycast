// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_remix_adapter.h"
#include <iostream>
#include <cstdint>
#include <fstream>
#include <chrono>

using namespace neuraltest::remake;
namespace {
struct Calls {
 int materials=0, meshes=0, lights=0, cameras=0, draws=0, lightDraws=0;
 int freedMaterials=0, freedMeshes=0, freedLights=0, failMesh=0;
 int failMaterial=0;
 bool materialFailureReturnsHandle=false;
 bool materialHandlesByHash=false;
 std::wstring expectedPath;
 const wchar_t* observedPath=nullptr;
 bool valid=true, failDraw=false, distinctMaterials=false;
 float cameraX=0;
 float expectedLightZ=1;
 bool skinning=false;
 float expectedApex=0;
 float expectedRadiance=3;
 std::uint32_t expectedColor=0xffffffffu;
 float expectedFov=90,expectedAspect=1,expectedNear=.1f,expectedFar=100;
 Vec3 expectedRight{1,0,0},expectedUp{0,1,0},expectedForward{0,0,1};
} calls;
constexpr auto ok=REMIXAPI_ERROR_CODE_SUCCESS;
auto failure() { return static_cast<remixapi_ErrorCode>(1); }
remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* p,remixapi_MaterialHandle* out) {
 ++calls.materials; auto* o=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(p->pNext);
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO && o;
 calls.valid &= calls.expectedPath.empty()?p->albedoTexture==nullptr:
  p->albedoTexture && std::wstring(p->albedoTexture)==calls.expectedPath;
 if(p->albedoTexture)calls.observedPath=p->albedoTexture;
 calls.valid &= o->roughnessConstant==(calls.distinctMaterials?(calls.materials==1?.3f:.6f):.8f);
 calls.valid &= o->alphaTestType==7 && o->opacityConstant==1;
 if(calls.distinctMaterials) calls.valid &= o->albedoConstant.x==(calls.materials==1?.2f:.9f);
 *out=reinterpret_cast<remixapi_MaterialHandle>(std::uintptr_t(calls.materialHandlesByHash?p->hash:calls.materials));
 if(calls.failMaterial==calls.materials) {
  if(!calls.materialFailureReturnsHandle)*out=nullptr;
  return failure();
 }
 return ok;
}
remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo* p,remixapi_MeshHandle* out) {
 ++calls.meshes; if(calls.failMesh==calls.meshes) return failure();
 const auto& s=p->surfaces_values[0];
 calls.valid &= bool(s.skinning_hasvalue)==calls.skinning;
 if(calls.skinning)calls.valid &= s.skinning_value.bonesPerVertex==1
  && s.skinning_value.blendWeights_count==3&&s.skinning_value.blendIndices_count==3
  && s.skinning_value.blendWeights_values[2]==1&&s.skinning_value.blendIndices_values[2]==2;
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_MESH_INFO && p->surfaces_count==1
  && s.vertices_count==3 && s.indices_count==3 && s.indices_values[2]==2
  && s.material==reinterpret_cast<remixapi_MaterialHandle>(std::uintptr_t(p->hash))
  && s.vertices_values[0].position[2]==float(p->hash*2) && s.vertices_values[0].normal[2]==-1
  && s.vertices_values[0].color==calls.expectedColor && s.vertices_values[0]._pad6==0;
 *out=reinterpret_cast<remixapi_MeshHandle>(std::uintptr_t(calls.meshes)); return ok;
}
remixapi_ErrorCode REMIXAPI_CALL light(const remixapi_LightInfo* p,remixapi_LightHandle* out) {
 ++calls.lights; const auto* d=static_cast<const remixapi_LightInfoDistantEXT*>(p->pNext);
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_LIGHT_INFO && d && d->direction.x==0 && d->direction.y==0 && d->direction.z==calls.expectedLightZ && p->radiance.x==calls.expectedRadiance;
 *out=reinterpret_cast<remixapi_LightHandle>(1); return ok;
}
remixapi_ErrorCode REMIXAPI_CALL camera(const remixapi_CameraInfo* p) {
 ++calls.cameras; const auto* c=static_cast<const remixapi_CameraInfoParameterizedEXT*>(p->pNext);
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_CAMERA_INFO && c && c->fovYInDegrees==calls.expectedFov
  && c->aspect==calls.expectedAspect && c->nearPlane==calls.expectedNear && c->farPlane==calls.expectedFar
  && c->forward.x==calls.expectedForward.x && c->forward.y==calls.expectedForward.y && c->forward.z==calls.expectedForward.z
  && c->up.x==calls.expectedUp.x && c->up.y==calls.expectedUp.y && c->up.z==calls.expectedUp.z
  && c->right.x==calls.expectedRight.x && c->right.y==calls.expectedRight.y && c->right.z==calls.expectedRight.z;
 calls.cameraX=c->position.x; return ok;
}
remixapi_ErrorCode REMIXAPI_CALL draw(const remixapi_InstanceInfo* p) {
 ++calls.draws;
 if(calls.skinning) {
  const auto* b=static_cast<const remixapi_InstanceInfoBoneTransformsEXT*>(p->pNext);
  calls.valid &= b&&b->sType==REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT
   && b->boneTransforms_count==3&&b->boneTransforms_values[2].matrix[0][3]==calls.expectedApex;
 }
 calls.valid &= calls.cameras>=1 && p->mesh && p->transform.matrix[0][0]==1 && p->transform.matrix[0][3]==0;
 return calls.failDraw?failure():ok;
}
remixapi_ErrorCode REMIXAPI_CALL drawLight(remixapi_LightHandle) {++calls.lightDraws; return ok;}
remixapi_ErrorCode REMIXAPI_CALL freeMaterial(remixapi_MaterialHandle) {++calls.freedMaterials; return ok;}
remixapi_ErrorCode REMIXAPI_CALL freeMesh(remixapi_MeshHandle) {++calls.freedMeshes; return ok;}
remixapi_ErrorCode REMIXAPI_CALL freeLight(remixapi_LightHandle) {++calls.freedLights; return ok;}
remixapi_Interface interface() {
 remixapi_Interface a{}; a.CreateMaterial=material; a.DestroyMaterial=freeMaterial;
 a.CreateMesh=mesh; a.DestroyMesh=freeMesh; a.CreateLight=light; a.DestroyLight=freeLight;
 a.SetupCamera=camera; a.DrawInstance=draw; a.DrawLightInstance=drawLight; return a;
}
}
int main() {
 auto counts=TestSceneContract();
 auto expect=[&](bool v,const char* name) {++(v?counts.passed:counts.failed); std::cout<<(v?"PASS ":"FAIL ")<<name<<'\n';};
 {
  auto p=Synthetic();p.space=Space::SampledAnchor;p.camera.provenance=Provenance::Supplied;
  p.omissions={"sample coverage only","game clips unknown"};
  expect(!ReadyForAdapter(p,p.frame,p.game).ok,"ordinary adapter rejects sampled diagnostic");
  calls={};RemixScene scene(interface());
  expect(scene.SubmitDiagnostic(p,p.frame,p.game,false).reason=="diagnostic-clips-not-declared"
   && calls.materials==0,"diagnostic clips require explicit declaration");
  expect(scene.SubmitDiagnostic(p,p.frame,p.game,true).reason=="diagnostic-api-submitted-not-rendered-or-presented"
   && scene.IsDiagnostic() && scene.RetainedOmissions()==p.omissions,"diagnostic submission retains limitations");
  auto camera=p.camera;camera.position.x=.1f;
  expect(scene.Redraw(camera).ok && scene.IsDiagnostic() && scene.RetainedOmissions()==p.omissions,
   "diagnostic redraw retains provenance and omissions");
  camera.provenance=Provenance::Analytic;const int before=calls.cameras;
  expect(!scene.Redraw(camera).ok && calls.cameras==before,"diagnostic redraw cannot silently change provenance");
  auto bad=p;bad.omissions.clear();
  expect(!ReadyForDiagnosticAdapter(bad,bad.frame,bad.game,true).ok,"diagnostic limitations cannot be erased");
  bad=p;bad.meshes[0].vertices[0].normal.reset();
  expect(ReadyForDiagnosticAdapter(bad,bad.frame,bad.game,true).reason=="normal-unknown","diagnostic preserves normal safety");
  bad=p;bad.camera.farPlane=1;
  expect(ReadyForDiagnosticAdapter(bad,bad.frame,bad.game,true).reason=="clip-unsupported","diagnostic excludes invalid clips");
 }
 {
  const auto dir=std::filesystem::temp_directory_path()/
   ("flycast-dds-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if(!std::filesystem::create_directory(dir))return 1;
  const auto path=dir/"synthetic.dds";
  std::array<std::uint32_t,39> words{};
  words[0]=0x20534444;words[1]=124;words[2]=0x100f;words[3]=words[4]=1;
  words[5]=4;words[7]=1;words[19]=32;words[20]=4;words[21]=0x30315844;
  words[27]=0x1000;words[32]=28;words[33]=3;words[35]=1;words[37]=0xff0000ff;
  auto write=[&](std::size_t bytes) {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(words.data()),bytes);};
  write(152);
  calls={};calls.expectedPath=path.wstring();
  {
   auto p=Synthetic();for(auto& m:p.meshes){m.material->sourceDds=path;m.material->sourceColorExperiment=true;}
   RemixScene scene(interface());
   expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid,"valid DDS path reaches material API");
   for(auto& m:p.meshes)m.material->sourceDds.clear();
   expect(calls.observedPath&&std::wstring(calls.observedPath)==calls.expectedPath,"adapter owns path after caller mutation");
  }
  auto p=Synthetic();p.meshes[0].material->sourceDds=path;p.meshes[0].material->sourceColorExperiment=true;
  {
   auto bound=p;auto& mesh=bound.meshes[0];
   mesh.texture={9,3,5,7,true};mesh.material->sourceTexture=mesh.texture;
   expect(ReadyForAdapter(bound,bound.frame,bound.game).ok,"explicit source texture identity accepted");
   {
    calls={};calls.expectedPath=path.wstring();
    // Both synthetic surfaces use the explicit file for the mock path check.
    bound.meshes[1].material->sourceDds=path;
    bound.meshes[1].material->sourceColorExperiment=true;
    RemixScene submitted(interface());
    expect(submitted.Submit(bound,bound.frame,bound.game).ok && calls.valid
     && calls.materials==2 && calls.draws==2,"bound source texture reaches public API");
   }
   for(int field=0;field<5;++field) {
    auto bad=bound;auto& identity=*bad.meshes[0].material->sourceTexture;
    if(field==0)identity.id++;
    if(field==1)identity.generation++;
    if(field==2)identity.paletteGeneration++;
    if(field==3)identity.rttGeneration++;
    if(field==4)identity.known=false;
    calls={};RemixScene rejected(interface());
    expect(rejected.Submit(bad,bad.frame,bad.game).reason=="source-texture-identity"
     && calls.materials==0 && calls.draws==0,"stale source texture makes no API calls");
   }
   auto bad=bound;bad.meshes[0].material->sourceDds.clear();
   expect(ReadyForAdapter(bad,bad.frame,bad.game).reason=="source-texture-identity","identity alone cannot replace asset");
   bad=bound;bad.meshes[0].texture.known=false;
   expect(ReadyForAdapter(bad,bad.frame,bad.game).reason=="source-texture-identity","unowned source binding rejects");
   bad=bound;bad.omissions.push_back("unknown domain");
   expect(ReadyForAdapter(bad,bad.frame,bad.game).reason=="incomplete-scene","source binding cannot waive omissions");
  }
  words[32]=29;write(152);
  expect(ReadyForAdapter(p,p.frame,p.game).reason=="source-texture-contract","unexpected sRGB DDS rejects");
  words[32]=28;write(151);
  expect(ReadyForAdapter(p,p.frame,p.game).reason=="source-texture-contract","truncated DDS rejects");
  write(156);
  expect(ReadyForAdapter(p,p.frame,p.game).reason=="source-texture-contract","trailing DDS payload rejects");
  for(unsigned index:{2u,5u,20u,27u,28u,36u}) {
   const auto saved=words[index];words[index]^=1;write(152);
   expect(ReadyForAdapter(p,p.frame,p.game).reason=="source-texture-contract","unsupported DDS flags pitch caps or alpha reject");
   words[index]=saved;
  }
  std::filesystem::remove(path);std::filesystem::remove(dir);
  expect(ReadyForAdapter(p,p.frame,p.game).reason=="source-texture-contract","missing DDS rejects before API");
 }
 for(int failed=1;failed<=2;++failed)for(bool returned:{false,true}) {
  calls={};calls.failMaterial=failed;calls.materialFailureReturnsHandle=returned;
  {
   RemixScene scene(interface());auto p=Synthetic();
   expect(scene.Submit(p,p.frame,p.game).reason=="create-material"&&calls.cameras==0&&calls.draws==0,
    "material creation failure cannot draw partial scene");
  }
  expect(calls.freedMaterials==failed-1+int(returned)&&calls.freedMeshes==failed-1,
   "material failure releases every returned handle");
 }
 {
  calls={};calls.distinctMaterials=true;auto p=Synthetic();
  p.meshes[0].material->albedo.x=.2f;p.meshes[0].material->roughness=.3f;
  p.meshes[1].material->albedo.x=.9f;p.meshes[1].material->roughness=.6f;
  RemixScene scene(interface());
  expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid&&calls.materials==2,"distinct mesh material parameters reach public API");
 }
 {
  calls={};calls.expectedColor=0x12345678u;
  auto p=Synthetic();for(auto& m:p.meshes)for(auto& v:m.vertices)v.publicColor=calls.expectedColor;
  RemixScene scene(interface());
  expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid,"nonwhite raw public color transported without substitution");
 }
 {
  calls={};calls.expectedRight={0,1,0};calls.expectedUp={-1,0,0};
  RemixScene scene(interface());auto p=Synthetic();p.camera.right=calls.expectedRight;p.camera.up=calls.expectedUp;
  expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid&&calls.cameras==1,"explicit rotated axes reach public camera ABI");
  p.camera.forward={0,1,0};
  expect(!scene.Redraw(p.camera).ok&&calls.cameras==1,"invalid rotated axes make no API call");
 }
 calls={};
 {
  RemixScene scene(interface());auto p=Synthetic();
  expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid,"recovered camera fixture starts with synthetic scene");
  auto c=p.camera;
  c.position={-4.402202805233795f,-1.1753900732667373f,4.5792354100240775f};
  c.right=calls.expectedRight={.9999993807275268f,0,.0011504740232752663f};
  c.up=calls.expectedUp={.000018860498003155206f,-.9998656571636007f,-.01639366691737305f};
  c.forward=calls.expectedForward={.0011503193527460098f,.01639367640018463f,-.9998649954795837f};
  c.fovY=calls.expectedFov=45.99033235267587f;c.aspect=calls.expectedAspect=1.2266665409901234f;
  // Only tests ABI transport. Scene and clip range are synthetic, not gameplay.
  expect(scene.Redraw(c).ok&&calls.valid&&calls.cameras==2&&calls.cameraX==c.position.x,
   "recovered lens and proper basis reach public ABI without aspect substitution");
 }
 calls={};
 {
  RemixScene scene(interface()); auto p=Synthetic(8,.5f);
  auto r=scene.Submit(p,8,p.game);
  expect(r.ok&&r.reason=="api-submitted-not-rendered-or-presented"&&calls.valid&&calls.draws==2
   &&calls.lightDraws==1&&calls.cameraX==.5f,"actual public ABI receives synthetic scene");
  expect(calls.freedMeshes==0,"resources retained through caller consumption");
  expect(scene.Submit(p,8,p.game).reason=="single-use-adapter","duplicate submission blocked");
  auto moved=p.camera; moved.position.x=.25f;
  expect(scene.Redraw(moved).ok && calls.materials==2 && calls.meshes==2 && calls.lights==1
   && calls.cameras==2 && calls.draws==4 && calls.cameraX==.25f,"moving camera reuses scene resources");
  moved.provenance=Provenance::Unknown;
  expect(!scene.Redraw(moved).ok && calls.cameras==2,"invalid redraw camera has no API calls");
  moved=p.camera; calls.failDraw=true;
  expect(!scene.Redraw(moved).ok,"failed redraw discards frame");
  const int previousDraws=calls.draws;
  expect(scene.Redraw(moved).reason=="no-complete-scene" && calls.draws==previousDraws,"failed redraw cannot silently resume");
 }
 expect(calls.freedMeshes==2&&calls.freedMaterials==2&&calls.freedLights==1,"scoped resource release");
 calls={};
 { RemixScene scene(interface()); auto p=Synthetic();
  expect(scene.Redraw(p.camera).reason=="no-complete-scene" && calls.cameras==0,"redraw before submit makes no calls");
  bool sequence=scene.Submit(p,p.frame,p.game).ok;
  for(int i=1;i<120;i++) {p.camera.position.x=float(i)/238.f;sequence &= scene.Redraw(p.camera).ok;}
  expect(sequence && calls.cameras==120 && calls.draws==240 && calls.materials==2
   && calls.meshes==2 && calls.lights==1 && calls.freedMeshes==0,"120 frames retain fixed resource count");
 }
 expect(calls.freedMeshes==2&&calls.freedMaterials==2&&calls.freedLights==1,"120 frame scene released exactly once");
 calls={}; calls.failMesh=2;
 { RemixScene scene(interface()); auto p=Synthetic(); expect(scene.Submit(p,7,p.game).reason=="create-mesh","injected mesh create failure"); }
 expect(calls.freedMeshes==1&&calls.freedMaterials==2&&calls.draws==0,"partial creation cleans resources without drawing");
 calls={}; calls.failDraw=true;
 { RemixScene scene(interface()); auto p=Synthetic(); expect(scene.Submit(p,7,p.game).reason=="draw-instance-discard-frame","submission failure requires frame discard"); }
 expect(calls.freedMeshes==2&&calls.freedLights==1&&calls.lightDraws==0,"failed submission cleanup");
 calls={};
 { RemixScene scene(interface()); auto p=Synthetic();
  p.meshes.back().topology=Topology::Strip; p.meshes.back().indices={0,0,1};
  expect(scene.Submit(p,7,p.game).reason=="empty-triangulation"&&calls.materials==0
   &&calls.meshes==0&&calls.cameras==0,"degenerate later mesh rejected before any API resource creation"); }
 { RemixScene scene(interface()); auto p=Synthetic(); p.camera.provenance=Provenance::Unknown;
  expect(scene.Submit(p,7,p.game).reason=="projection-unknown"&&calls.materials==0,"unknown projection makes zero API calls"); }
 { auto a=interface(); a.CreateMesh=nullptr; RemixScene scene(a); auto p=Synthetic();
  expect(scene.Submit(p,7,p.game).reason=="incomplete-public-interface"&&calls.materials==0,"missing entry point makes zero API calls"); }
 calls={}; calls.expectedLightZ=-1;
 { RemixScene scene(interface(),false,true); auto p=Synthetic();
  expect(scene.Submit(p,p.frame,p.game).ok && calls.valid && calls.lights==1,
   "explicit reversed diagnostic light preserves scene and radiance"); }
 calls={};calls.skinning=true;
 { RemixScene scene(interface(),false,false,true);auto p=Synthetic();
  expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid,"public synthetic skinning creation");
  calls.expectedApex=.5f;
  expect(scene.RedrawSyntheticSkinning(p.camera,.5f).ok&&calls.valid&&calls.meshes==2&&calls.materials==2,
   "bone deformation retains meshes and materials");
  const auto before=calls.draws;
  expect(!scene.RedrawSyntheticSkinning(p.camera,1.f).ok&&calls.draws==before,"out-of-bound deformation rejects before API");
 }
 calls={};calls.skinning=true;calls.expectedRadiance=.03f;
 { RemixScene scene(interface(),false,false,true,true);auto p=Synthetic();
  expect(scene.Submit(p,p.frame,p.game).ok&&calls.valid,"dim affine fixture creation");
  expect(scene.RedrawSyntheticAffine(p.camera).ok&&calls.valid&&calls.meshes==2,
   "affine redraw retains scene resources");
 }
 calls={};
 { RemixScene scene(interface());auto p=Synthetic();
  expect(!scene.RedrawSyntheticAffine(p.camera).ok&&calls.draws==0,"affine rejects non-skinning adapter");
 }
 calls={};calls.materialHandlesByHash=true;
 { RemixScene scene(interface());auto p=Synthetic();
  expect(scene.Submit(p,p.frame,p.game).ok,"material replacement fixture");
  auto invalid=p.camera;invalid.fovY=0;
  expect(!scene.RedrawSyntheticMaterial(invalid,true).ok&&calls.materials==2&&calls.freedMaterials==0,
   "invalid replacement camera changes no materials");
  expect(scene.RedrawSyntheticMaterial(p.camera,true).ok&&calls.meshes==2&&calls.materials==4&&calls.valid,
   "replacement reuses meshes and stable material handles");
 }
 expect(calls.freedMaterials==4,"replacement material lifetimes released once");
 for(bool returnHandle:{false,true}) {
  calls={};calls.materialHandlesByHash=true;
  { RemixScene scene(interface());auto p=Synthetic();scene.Submit(p,p.frame,p.game);
   calls.failMaterial=3;calls.materialFailureReturnsHandle=returnHandle;
   expect(!scene.RedrawSyntheticMaterial(p.camera,true).ok&&calls.draws==2,
    "failed replacement cannot draw partial frame");
   expect(!scene.Redraw(p.camera).ok,"failed replacement cannot resume stale scene");
  }
  expect(calls.freedMaterials==(returnHandle?3:2),"replacement failure owns only live returned handles");
 }
 std::cout<<"remake-sdk-contract passed="<<counts.passed<<" failed="<<counts.failed
  <<" runtime_loaded=false gpu_rendered=false presented=false\n";
 return counts.failed?1:0;
}
