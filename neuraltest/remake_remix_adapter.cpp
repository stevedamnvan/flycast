// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_remix_adapter.h"
#include <algorithm>

namespace neuraltest::remake {
RemixScene::~RemixScene() {
 for (auto m:meshes_) if (m && api_.DestroyMesh) api_.DestroyMesh(m);
 if (light_ && api_.DestroyLight) api_.DestroyLight(light_);
 for (auto m:materials_) if (m && api_.DestroyMaterial) api_.DestroyMaterial(m);
}
Result RemixScene::Submit(const Packet& p, std::uint64_t frame, const std::string& game) {
 return SubmitChecked(p,frame,game,false);
}
Result RemixScene::SubmitDiagnostic(const Packet& p,std::uint64_t frame,const std::string& game,bool clips) {
 if(!clips)return {false,"diagnostic-clips-not-declared"};
 return SubmitChecked(p,frame,game,true);
}
Result RemixScene::SubmitChecked(const Packet& p, std::uint64_t frame, const std::string& game,bool diagnostic) {
 if (attempted_) return {false,"single-use-adapter"};
 auto checked=diagnostic?ReadyForDiagnosticAdapter(p,frame,game,true):ReadyForAdapter(p,frame,game);
 if (!checked.ok) return checked;
 diagnostic_=diagnostic;
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
 vertices_.resize(p.meshes.size()); meshes_.reserve(p.meshes.size());
 materials_.reserve(p.meshes.size());
 texturePaths_.resize(p.meshes.size());
 for (std::size_t i=0;i<p.meshes.size();++i) {
 const auto& parameters=*p.meshes[i].material;
 remixapi_MaterialInfoOpaqueEXT opaque{};
 opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
 opaque.albedoConstant={parameters.albedo.x,parameters.albedo.y,parameters.albedo.z};
 opaque.opacityConstant=1; opaque.roughnessConstant=parameters.roughness;
 remixapi_MaterialInfo material{}; material.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
 material.pNext=&opaque; material.hash=p.meshes[i].id;
 texturePaths_[i]=parameters.sourceDds.wstring();
 material.albedoTexture=texturePaths_[i].empty()?nullptr:texturePaths_[i].c_str();
 remixapi_MaterialHandle materialHandle=nullptr;
 const auto materialResult=api_.CreateMaterial(&material,&materialHandle);
 if(materialHandle) materials_.push_back(materialHandle);
 if (materialResult!=REMIXAPI_ERROR_CODE_SUCCESS || !materialHandle)
  return {false,"create-material"};
  const auto& mesh=p.meshes[i]; auto& vertices=vertices_[i]; vertices.resize(mesh.vertices.size());
  for (std::size_t j=0;j<vertices.size();++j) {
   auto& out=vertices[j]; const auto& in=mesh.vertices[j];
   out.position[0]=in.position.x; out.position[1]=in.position.y; out.position[2]=in.position.z;
   out.normal[0]=in.normal->x; out.normal[1]=in.normal->y; out.normal[2]=in.normal->z;
   out.texcoord[0]=in.u; out.texcoord[1]=in.v; out.color=in.publicColor;
  }
  remixapi_MeshInfoSurfaceTriangles surface{};
  surface.vertices_values=vertices.data(); surface.vertices_count=vertices.size();
  surface.indices_values=indices_[i].data(); surface.indices_count=indices_[i].size(); surface.material=materialHandle;
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
 packet_=p;
 const auto drawn=DrawFrame(p.camera);
 ready_=drawn.ok;
 return drawn;
}
Result RemixScene::Redraw(const Camera& camera) {
 if(!ready_) return {false,"no-complete-scene"};
 auto check=packet_; check.camera=camera;
 const auto valid=diagnostic_?ReadyForDiagnosticAdapter(check,check.frame,check.game,true)
  :ReadyForAdapter(check,check.frame,check.game);
 if(!valid.ok) return valid;
 const auto drawn=DrawFrame(camera);
 ready_=drawn.ok;
 return drawn;
}
Result RemixScene::DrawFrame(const Camera& input) {
 remixapi_CameraInfoParameterizedEXT parameters{};
 parameters.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT;
 parameters.position={input.position.x,input.position.y,input.position.z};
 parameters.forward={input.forward.x,input.forward.y,input.forward.z};
 parameters.up={input.up.x,input.up.y,input.up.z};
 parameters.right={input.right.x,input.right.y,input.right.z};
 parameters.fovYInDegrees=input.fovY; parameters.aspect=input.aspect;
 parameters.nearPlane=input.nearPlane; parameters.farPlane=input.farPlane;
 remixapi_CameraInfo camera{}; camera.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;
 camera.type=REMIXAPI_CAMERA_TYPE_WORLD; camera.pNext=&parameters;
 if (api_.SetupCamera(&camera)!=REMIXAPI_ERROR_CODE_SUCCESS) return {false,"setup-camera-discard-frame"};
 for(std::size_t i=0;i<meshes_.size();++i) {
  remixapi_InstanceInfo instance{}; instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
  instance.mesh=meshes_[i]; instance.doubleSided=1;
  for(int row=0;row<3;++row) for(int col=0;col<4;++col)
   instance.transform.matrix[row][col]=(*packet_.meshes[i].transform)[row*4+col];
  if(api_.DrawInstance(&instance)!=REMIXAPI_ERROR_CODE_SUCCESS) return {false,"draw-instance-discard-frame"};
 }
 if(api_.DrawLightInstance(light_)!=REMIXAPI_ERROR_CODE_SUCCESS) return {false,"draw-light-discard-frame"};
 return {true,diagnostic_?"diagnostic-api-submitted-not-rendered-or-presented":"api-submitted-not-rendered-or-presented"};
}
}
