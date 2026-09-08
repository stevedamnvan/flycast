// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_remix_adapter.h"
#include <iostream>
#include <cstdint>

using namespace neuraltest::remake;
namespace {
struct Calls {
 int materials=0, meshes=0, lights=0, cameras=0, draws=0, lightDraws=0;
 int freedMaterials=0, freedMeshes=0, freedLights=0, failMesh=0;
 bool valid=true, failDraw=false;
 float cameraX=0;
} calls;
constexpr auto ok=REMIXAPI_ERROR_CODE_SUCCESS;
auto failure() { return static_cast<remixapi_ErrorCode>(1); }
remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* p,remixapi_MaterialHandle* out) {
 ++calls.materials; auto* o=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(p->pNext);
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO && o && o->roughnessConstant==.8f && !p->albedoTexture;
 *out=reinterpret_cast<remixapi_MaterialHandle>(1); return ok;
}
remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo* p,remixapi_MeshHandle* out) {
 ++calls.meshes; if(calls.failMesh==calls.meshes) return failure();
 const auto& s=p->surfaces_values[0];
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_MESH_INFO && p->surfaces_count==1
  && s.vertices_count==3 && s.indices_count==3 && s.indices_values[2]==2 && s.material
  && s.vertices_values[0].position[2]==float(p->hash*2) && s.vertices_values[0].normal[2]==-1
  && s.vertices_values[0].color==0xffffffff && s.vertices_values[0]._pad6==0;
 *out=reinterpret_cast<remixapi_MeshHandle>(std::uintptr_t(calls.meshes)); return ok;
}
remixapi_ErrorCode REMIXAPI_CALL light(const remixapi_LightInfo* p,remixapi_LightHandle* out) {
 ++calls.lights; const auto* d=static_cast<const remixapi_LightInfoDistantEXT*>(p->pNext);
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_LIGHT_INFO && d && d->direction.z==1 && p->radiance.x==3;
 *out=reinterpret_cast<remixapi_LightHandle>(1); return ok;
}
remixapi_ErrorCode REMIXAPI_CALL camera(const remixapi_CameraInfo* p) {
 ++calls.cameras; const auto* c=static_cast<const remixapi_CameraInfoParameterizedEXT*>(p->pNext);
 calls.valid &= p->sType==REMIXAPI_STRUCT_TYPE_CAMERA_INFO && c && c->fovYInDegrees==90
  && c->forward.z==1 && c->up.y==1 && c->right.x==1;
 calls.cameraX=c->position.x; return ok;
}
remixapi_ErrorCode REMIXAPI_CALL draw(const remixapi_InstanceInfo* p) {
 ++calls.draws;
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
  RemixScene scene(interface()); auto p=Synthetic(8,.5f);
  auto r=scene.Submit(p,8,p.game);
  expect(r.ok&&r.reason=="api-submitted-not-rendered-or-presented"&&calls.valid&&calls.draws==2
   &&calls.lightDraws==1&&calls.cameraX==.5f,"actual public ABI receives synthetic scene");
  expect(calls.freedMeshes==0,"resources retained through caller consumption");
  expect(scene.Submit(p,8,p.game).reason=="single-use-adapter","duplicate submission blocked");
  auto moved=p.camera; moved.position.x=.25f;
  expect(scene.Redraw(moved).ok && calls.materials==1 && calls.meshes==2 && calls.lights==1
   && calls.cameras==2 && calls.draws==4 && calls.cameraX==.25f,"moving camera reuses scene resources");
  moved.provenance=Provenance::Unknown;
  expect(!scene.Redraw(moved).ok && calls.cameras==2,"invalid redraw camera has no API calls");
  moved=p.camera; calls.failDraw=true;
  expect(!scene.Redraw(moved).ok,"failed redraw discards frame");
  const int previousDraws=calls.draws;
  expect(scene.Redraw(moved).reason=="no-complete-scene" && calls.draws==previousDraws,"failed redraw cannot silently resume");
 }
 expect(calls.freedMeshes==2&&calls.freedMaterials==1&&calls.freedLights==1,"scoped resource release");
 calls={};
 { RemixScene scene(interface()); auto p=Synthetic();
  expect(scene.Redraw(p.camera).reason=="no-complete-scene" && calls.cameras==0,"redraw before submit makes no calls");
  bool sequence=scene.Submit(p,p.frame,p.game).ok;
  for(int i=1;i<120;i++) {p.camera.position.x=float(i)/238.f;sequence &= scene.Redraw(p.camera).ok;}
  expect(sequence && calls.cameras==120 && calls.draws==240 && calls.materials==1
   && calls.meshes==2 && calls.lights==1 && calls.freedMeshes==0,"120 frames retain fixed resource count");
 }
 expect(calls.freedMeshes==2&&calls.freedMaterials==1&&calls.freedLights==1,"120 frame scene released exactly once");
 calls={}; calls.failMesh=2;
 { RemixScene scene(interface()); auto p=Synthetic(); expect(scene.Submit(p,7,p.game).reason=="create-mesh","injected mesh create failure"); }
 expect(calls.freedMeshes==1&&calls.freedMaterials==1&&calls.draws==0,"partial creation cleans resources without drawing");
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
 std::cout<<"remake-sdk-contract passed="<<counts.passed<<" failed="<<counts.failed
  <<" runtime_loaded=false gpu_rendered=false presented=false\n";
 return counts.failed?1:0;
}
