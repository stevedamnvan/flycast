// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
#include <memory>

namespace flycast::rend::neural {
struct RemakeTemporalMesh {
 std::uint64_t id=0; // Draw ordinal hint, not correspondence identity.
 remake::TextureIdentity texture;
 std::uint32_t sourceTsp=0;
 std::optional<std::uint8_t> alphaReference;
 bool alphaBlend=false;
 std::vector<remake::Vertex> vertices;
 std::vector<std::uint32_t> indices;
};
struct RemakeTemporalScene {
 RemakeChannelReceipt receipt;
 ProducerIdentity producer;
 std::uint64_t frame=0;
 std::string game,sourceSha;
 remake::Camera camera;
 remake::Vec3 fixedOrigin;
 std::vector<RemakeTemporalMesh> meshes;
 bool Matches(const RemakeReturnedImage& image)const {
  return frame&&frame==image.frame&&producer.Available()
   &&producer.epoch==image.producer.epoch&&producer.ordinal==image.producer.ordinal&&producer.cycle==image.producer.cycle
   &&receipt.sequence&&receipt.digest&&receipt.bytes&&receipt.sequence==image.source.sequence
   &&receipt.digest==image.source.digest&&receipt.bytes==image.source.bytes
   &&camera.nearPlane==image.nearPlane&&camera.farPlane==image.farPlane;
 }
};
inline std::shared_ptr<RemakeTemporalScene> CaptureRemakeTemporalScene(const remake::Packet& packet,std::string& error) {
 const auto fail=[&](const char* why)->std::shared_ptr<RemakeTemporalScene>{error=why;return {};};
 if(packet.diagnosticEmbeddingProvenance!="diagnostic-camera-embedded-anchor-not-world-reconstruction"
  ||!packet.diagnosticOrigin||!packet.producer.Available()||packet.meshes.empty()||packet.meshes.size()>128)
  return fail("temporal-source-scope");
 const auto valid=remake::ReadyForDiagnosticAdapter(packet,packet.frame,packet.game,true);
 if(!valid.ok){error=valid.reason;return {};}
 const auto origin=*packet.diagnosticOrigin;
 if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z))return fail("temporal-source-origin");
 std::size_t vertices=0,indices=0;
 for(const auto& mesh:packet.meshes) {
  if(!mesh.sourceTsp||mesh.topology!=remake::Topology::Triangles||mesh.vertices.size()>65536-vertices
   ||mesh.indices.size()>262144-indices)return fail("temporal-source-bound-or-topology");
  vertices+=mesh.vertices.size();indices+=mesh.indices.size();
 }
 auto owned=std::make_shared<RemakeTemporalScene>();
 owned->producer=packet.producer;owned->frame=packet.frame;owned->game=packet.game;owned->sourceSha=packet.sourceGitSha;
 owned->camera=packet.camera;owned->fixedOrigin=origin;owned->meshes.reserve(packet.meshes.size());
 for(const auto& mesh:packet.meshes) {
  RemakeTemporalMesh m;m.id=mesh.id;m.texture=mesh.texture;m.sourceTsp=*mesh.sourceTsp;
  m.alphaReference=mesh.sourceAlphaReference;m.alphaBlend=mesh.sourceAlphaBlend;
  m.vertices=mesh.vertices;m.indices=mesh.indices;owned->meshes.push_back(std::move(m));
 }
 error.clear();return owned;
}
// Reference data for subsequent correspondence. This does not enable NGX history.
inline bool CompatibleRemakeTemporalReference(const RemakeTemporalScene& previous,const RemakeTemporalScene& next) {
 return previous.producer.epoch==next.producer.epoch&&next.producer.ordinal>previous.producer.ordinal
  &&next.producer.cycle>=previous.producer.cycle&&next.frame>previous.frame&&next.frame-previous.frame<=8
  &&next.game==previous.game&&next.sourceSha==previous.sourceSha
  &&next.fixedOrigin.x==previous.fixedOrigin.x&&next.fixedOrigin.y==previous.fixedOrigin.y&&next.fixedOrigin.z==previous.fixedOrigin.z
  &&next.camera.fovY==previous.camera.fovY&&next.camera.aspect==previous.camera.aspect
  &&next.camera.nearPlane==previous.camera.nearPlane&&next.camera.farPlane==previous.camera.farPlane;
}
class RemakeTemporalHistory {
 std::shared_ptr<const RemakeTemporalScene> accepted;
 std::vector<float> acceptedDepth;
 std::vector<unsigned char> acceptedColor;
public:
 void Reset(){accepted.reset();acceptedDepth.clear();acceptedColor.clear();}
 const RemakeTemporalScene* Last()const{return accepted.get();}
 std::shared_ptr<const RemakeTemporalScene> Shared()const{return accepted;} // Immutable; safe to hand to a worker.
 const std::vector<float>& Depth()const{return acceptedDepth;}
 const std::vector<unsigned char>& Color()const{return acceptedColor;}
 bool CanReproject(const RemakeTemporalScene& next)const {
  return accepted&&CompatibleRemakeTemporalReference(*accepted,next);
 }
 bool Accept(std::shared_ptr<const RemakeTemporalScene> scene,const RemakeReturnedImage& image,bool evaluated,bool retainColor=true) {
  if(!evaluated||!scene||!scene->Matches(image)||image.width!=640||image.height!=480
   ||image.projectionDepth.size()!=640*480)return false;
  for(float z:image.projectionDepth)if(!std::isfinite(z)||z<0||z>1)return false;
  if(accepted&&(scene->producer.epoch!=accepted->producer.epoch||scene->frame<=accepted->frame
   ||scene->producer.ordinal<=accepted->producer.ordinal||scene->producer.cycle<accepted->producer.cycle
   ||scene->receipt.sequence<=accepted->receipt.sequence))return false;
  auto depth=image.projectionDepth;
  auto color=retainColor?image.bgra:std::vector<unsigned char>{};
  accepted=std::move(scene);acceptedDepth=std::move(depth);acceptedColor=std::move(color);return true;
 }
};
}
