// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <optional>
#include "hw/pvr/ta_ctx.h"
#include "source_observation.h"
namespace flycast::rend::neural {
struct PvrCapturedTexture {
 std::uint32_t upload=0, rtt=0;
 std::optional<std::uint32_t> palette;
};
struct PvrCapturedDraw {
 // A zero-count draw has no index references; its unused first value is retained.
 std::uint32_t list=0, ordinal=0;
 bool vertexRange=false; // Sorted source strips, not GPU index ranges.
 bool protectedOverlay=false; // Live source classification; never inferred from old archives.
 PolyParam state{}; // Texture pointers stay null; resolve only against retained resources.
 std::optional<PvrCapturedTexture> texture, texture1;
};
struct PvrCapturedPass {
 std::uint32_t op=0, pt=0, tr=0, mvo=0, sortedTr=0;
 bool autosort=false, zClear=false;
};
struct PvrDecodedPacket {
 std::uint64_t frame=0;
 ProducerIdentity sourceProducer;
 // Optional live-only SQ provenance. Disk packet versions do not contain it.
 std::vector<SourceVertexObservation> sourceVertices;
 // Live source renderer state; old decoded disk formats do not imply a value.
 std::optional<std::uint8_t> sourceAlphaReference;
 std::string game, gitSha;
 std::array<float,16> viewport{};
 std::array<std::uint32_t,2> framebufferSize{};
 bool clearFramebuffer=false;
 std::vector<Vertex> vertices;
 std::vector<std::uint32_t> indices;
 std::vector<PvrCapturedDraw> draws;
 std::vector<PvrCapturedPass> passes;
 bool sortedOrderCaptured=false; // v1 omitted this data; v2 retains it.
 std::vector<SortedTriangle> sortedTriangles;
 std::vector<std::string> omissions;
 std::uint32_t modifierTriangles=0, unusedNonfinite=0;
 // This remains a projected packet, never a recovered world/camera scene.
};
struct PvrSourceCoverage {
 std::size_t components=0,completeVertices=0,commonOriginVertices=0,completeDraws=0,partialDraws=0;
};
// Diagnostic coverage only, not camera or reconstruction acceptance.
inline PvrSourceCoverage MeasurePvrSourceCoverage(const PvrDecodedPacket& packet) {
 PvrSourceCoverage result;
 std::vector<unsigned char> coverage(packet.vertices.size(),0),seen(packet.vertices.size(),0);
 for(const auto& source:packet.sourceVertices) {
  const auto vertex=source.copy.decodedVertex;
  if(vertex<seen.size()&&seen[vertex]<2)++seen[vertex];
 }
 for(const auto& source:packet.sourceVertices) {
  const auto vertex=source.copy.decodedVertex;
  if(vertex>=coverage.size())continue;
  if(seen[vertex]!=1)continue;
  const auto& xyz=source.copy.xyzTransforms;
  unsigned components=0;for(const auto& transform:xyz)components+=transform&&transform->serial!=0;
  result.components+=components;
  if(components!=3)continue;
  ++result.completeVertices;
  const auto& first=*xyz[0];
  const auto same=[&](const SourceTransform& other) {
   return first.serial==other.serial&&first.pc==other.pc
    &&first.input==other.input&&first.matrix==other.matrix&&first.output==other.output;
  };
  if(same(*xyz[1])&&same(*xyz[2]))coverage[vertex]=1;
 }
 for(auto value:coverage)result.commonOriginVertices+=value!=0;
 for(const auto& draw:packet.draws) {
  if(!draw.state.count)continue;
  const std::uint64_t end=static_cast<std::uint64_t>(draw.state.first)+draw.state.count;
  const auto bound=draw.vertexRange?packet.vertices.size():packet.indices.size();
  if(end>bound)continue;
  std::size_t matched=0,total=0;
  for(std::uint64_t i=draw.state.first;i<end;++i) {
   const auto vertex=draw.vertexRange?static_cast<std::uint32_t>(i):packet.indices[i];
   if(vertex==UINT32_MAX)continue;
   ++total;if(vertex<coverage.size())matched+=coverage[vertex]!=0;
  }
  if(total&&matched==total)++result.completeDraws;
  else if(matched)++result.partialDraws;
 }
 return result;
}
// Bounded decoder, not renderer activation. Leaves output unchanged on failure.
// Caller owns the render context and texture-cache lifetime throughout the copy.
// Copies metadata only, clears raw texture pointers; no camera reconstruction.
bool SnapshotPvrScenePacket(const rend_context&, const std::array<float,16>&,
 std::uint64_t, const std::string&, PvrDecodedPacket&, std::string& error);
// Same owned render-thread boundary only; validates both texture slots.
bool PvrSnapshotTextureBindingsMatch(const rend_context&, const PvrDecodedPacket&);
bool WritePvrSourceWitness(const std::filesystem::path&,const PvrDecodedPacket&,std::string& error);
bool ReadPvrScenePacket(const std::filesystem::path&, std::uint64_t expectedFrame,
 const std::string& expectedGame, PvrDecodedPacket&, std::string& error);
// Developer capture only. No pointers or texture contents are serialized.
bool WritePvrScenePacket(const std::filesystem::path&, const rend_context&,
 const std::array<float,16>& viewport, std::uint64_t frame,
 const std::string& game, std::string& error);
}
