// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_remix_adapter.h"
#include <algorithm>

namespace neuraltest::remake {
RemixScene::~RemixScene() {
 for (auto m:meshes_) if (m && api_.DestroyMesh) api_.DestroyMesh(m);
 if (light_ && api_.DestroyLight) api_.DestroyLight(light_);
 if (material_ && api_.DestroyMaterial) api_.DestroyMaterial(material_);
}
Result RemixScene::Submit(const Packet& p, std::uint64_t frame, const std::string& game) {
 if (attempted_) return {false,"single-use-adapter"};
 auto checked=ReadyForAdapter(p,frame,game); if (!checked.ok) return checked;
 if (!api_.CreateMaterial || !api_.DestroyMaterial || !api_.CreateMesh || !api_.DestroyMesh
  || !api_.CreateLight || !api_.DestroyLight || !api_.DrawLightInstance || !api_.SetupCamera || !api_.DrawInstance)
  return {false,"incomplete-public-interface"};
 attempted_=true;
 // Finish topology conversion before creating any external resources. A valid
 // strip can contain only degenerate breaks and therefore no drawable surface.
 indices_.resize(p.meshes.size());
 for (std::size_t i=0;i<p.meshes.size();++i) {
  indices_[i]=Triangles(p.meshes[i]);
  if (indices_[i].empty()) return {false,"empty-triangulation"};
 }
 // Explicit synthetic art direction, never inferred from game material/light.
 remixapi_MaterialInfoOpaqueEXT opaque{};
 opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
 opaque.albedoConstant={.7f,.7f,.7f}; opaque.opacityConstant=1; opaque.roughnessConstant=.8f;
 remixapi_MaterialInfo material{}; material.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
 material.pNext=&opaque; material.hash=0xFC067001;
 if (api_.CreateMaterial(&material,&material_)!=REMIXAPI_ERROR_CODE_SUCCESS || !material_)
  return {false,"create-material"};
 vertices_.resize(p.meshes.size()); meshes_.reserve(p.meshes.size());
 for (std::size_t i=0;i<p.meshes.size();++i) {
  const auto& mesh=p.meshes[i]; auto& vertices=vertices_[i]; vertices.resize(mesh.vertices.size());
  for (std::size_t j=0;j<vertices.size();++j) {
   auto& out=vertices[j]; const auto& in=mesh.vertices[j];
   out.position[0]=in.position.x; out.position[1]=in.position.y; out.position[2]=in.position.z;
   out.normal[0]=in.normal->x; out.normal[1]=in.normal->y; out.normal[2]=in.normal->z;
   out.texcoord[0]=in.u; out.texcoord[1]=in.v; out.color=0xffffffff;
  }
  remixapi_MeshInfoSurfaceTriangles surface{};
  surface.vertices_values=vertices.data(); surface.vertices_count=vertices.size();
  surface.indices_values=indices_[i].data(); surface.indices_count=indices_[i].size(); surface.material=material_;
  remixapi_MeshInfo info{}; info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;
  info.hash=mesh.id; info.surfaces_values=&surface; info.surfaces_count=1;
  remixapi_MeshHandle handle=nullptr;
  const auto status=api_.CreateMesh(&info,&handle);
  if(handle) meshes_.push_back(handle);
  if(status!=REMIXAPI_ERROR_CODE_SUCCESS || !handle) return {false,"create-mesh"};
 }
 remixapi_LightInfoDistantEXT distant{}; distant.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
 distant.direction={0,0,1}; distant.angularDiameterDegrees=.5f; distant.volumetricRadianceScale=1;
 remixapi_LightInfo light{}; light.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO; light.pNext=&distant;
 light.hash=0xFC067002; light.radiance={3,3,3};
 if (api_.CreateLight(&light,&light_)!=REMIXAPI_ERROR_CODE_SUCCESS || !light_) return {false,"create-light"};
 remixapi_CameraInfoParameterizedEXT parameters{};
 parameters.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT;
 parameters.position={p.camera.position.x,p.camera.position.y,p.camera.position.z};
 parameters.forward={0,0,1}; parameters.up={0,1,0}; parameters.right={1,0,0};
 parameters.fovYInDegrees=p.camera.fovY; parameters.aspect=p.camera.aspect;
 parameters.nearPlane=p.camera.nearPlane; parameters.farPlane=p.camera.farPlane;
 remixapi_CameraInfo camera{}; camera.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;
 camera.type=REMIXAPI_CAMERA_TYPE_WORLD; camera.pNext=&parameters;
 if (api_.SetupCamera(&camera)!=REMIXAPI_ERROR_CODE_SUCCESS) return {false,"setup-camera-discard-frame"};
 for(std::size_t i=0;i<meshes_.size();++i) {
  remixapi_InstanceInfo instance{}; instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
  instance.mesh=meshes_[i]; instance.doubleSided=1;
  for(int row=0;row<3;++row) for(int col=0;col<4;++col)
   instance.transform.matrix[row][col]=(*p.meshes[i].transform)[row*4+col];
  if(api_.DrawInstance(&instance)!=REMIXAPI_ERROR_CODE_SUCCESS) return {false,"draw-instance-discard-frame"};
 }
 if(api_.DrawLightInstance(light_)!=REMIXAPI_ERROR_CODE_SUCCESS) return {false,"draw-light-discard-frame"};
 return {true,"api-submitted-not-rendered-or-presented"};
}
}
