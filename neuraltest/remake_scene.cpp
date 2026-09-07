// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_scene.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace neuraltest::remake {
namespace {
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool validCamera(const Camera& c) {
 return finite(c.position) && std::isfinite(c.fovY) && c.fovY > 1 && c.fovY < 179
  && std::isfinite(c.aspect) && c.aspect > 0 && std::isfinite(c.nearPlane)
  && std::isfinite(c.farPlane) && c.nearPlane > 0 && c.farPlane > c.nearPlane;
}
float tangent(const Camera& c) { return std::tan(c.fovY * 0.008726646259971648f); }
}
Result Validate(const Packet& p, std::uint64_t frame, const std::string& game, const Limits& limits) {
 if (p.version != 1) return {false, "schema"};
 if (p.frame != frame || p.game != game || p.game.empty() || p.game.size() > 64) return {false, "identity"};
 if (p.space != Space::World && p.space != Space::View && p.space != Space::PvrProjected) return {false, "space"};
 if (p.camera.provenance != Provenance::Unknown && p.camera.provenance != Provenance::Supplied
  && p.camera.provenance != Provenance::Analytic) return {false, "provenance"};
 if (p.truncated) return {false, "truncated"};
 if (p.meshes.empty() || p.meshes.size() > limits.meshes || p.omissions.size() > 64) return {false, "count-limit"};
 // Account actual packet-owned element storage, not allocator capacity. Ingestion
 // must check these same limits before allocation; this API accepts no files.
 std::size_t bytes = sizeof(Packet), vertices = 0, indices = 0;
 auto addBytes = [&](std::size_t n) {
  if (bytes > limits.bytes || n > limits.bytes - bytes) return false;
  bytes += n; return true;
 };
 if (!addBytes(p.game.size())) return {false, "byte-limit"};
 for (const auto& omission : p.omissions)
  if (omission.empty() || omission.size() > 256 || !addBytes(sizeof(std::string) + omission.size())) return {false, "omission-limit"};
 if (p.camera.provenance != Provenance::Unknown && !validCamera(p.camera)) return {false, "camera"};
 std::set<std::uint64_t> ids;
 for (const auto& m : p.meshes) {
  if (m.frame != frame) return {false, "mesh-frame"};
  if (!m.id || !ids.insert(m.id).second) return {false, "mesh-id"};
  if (m.topology != Topology::Triangles && m.topology != Topology::Strip) return {false, "topology"};
  if (m.vertices.size() > limits.vertices - vertices || m.indices.size() > limits.indices - indices) return {false, "count-limit"};
  vertices += m.vertices.size(); indices += m.indices.size();
  if (!addBytes(sizeof(Mesh))) return {false, "byte-limit"};
  // Division before multiplication prevents overflow with adversarial limits.
  if (m.vertices.size() > (limits.bytes - bytes) / sizeof(Vertex)
   || !addBytes(m.vertices.size() * sizeof(Vertex))) return {false, "byte-limit"};
  if (m.indices.size() > (limits.bytes - bytes) / sizeof(std::uint32_t)
   || !addBytes(m.indices.size() * sizeof(std::uint32_t))) return {false, "byte-limit"};
  if (m.vertices.size() < 3 || m.indices.size() < 3
   || (m.topology == Topology::Triangles && m.indices.size() % 3)) return {false, "topology"};
  for (auto index : m.indices) if (index >= m.vertices.size()) return {false, "index"};
  for (const auto& v : m.vertices) {
   if (!finite(v.position) || !std::isfinite(v.u) || !std::isfinite(v.v)) return {false, "nonfinite"};
   if (v.normal && (!finite(*v.normal) || std::abs(v.normal->x*v.normal->x + v.normal->y*v.normal->y + v.normal->z*v.normal->z - 1) > 0.001f)) return {false, "normal"};
  }
  if (m.transform) for (float n : *m.transform) if (!std::isfinite(n)) return {false, "transform"};
 }
 return {true, "valid-packet-not-scene-proof"};
}
Result ReadyForAdapter(const Packet& p, std::uint64_t frame, const std::string& game) {
 auto result = Validate(p, frame, game); if (!result.ok) return result;
 if (p.camera.provenance == Provenance::Unknown) return {false, "projection-unknown"};
 if (p.space != Space::World) return {false, "world-space-required"};
 if (!p.omissions.empty()) return {false, "incomplete-scene"};
 for (const auto& m : p.meshes) {
  if (!m.transform) return {false, "transform-unknown"};
  // M1 accepts baked world-space meshes with an explicit identity transform.
  // General transforms need inverse-transpose normal handling, not guessed math.
  constexpr std::array<float,12> identity{1,0,0,0, 0,1,0,0, 0,0,1,0};
  if (*m.transform != identity) return {false, "transform-unsupported"};
  if (m.texture.known) return {false, "textured-material-unsupported"};
  for (const auto& v : m.vertices) {
   if (!v.normal) return {false, "normal-unknown"};
   auto world = WorldPosition(m, v);
   if (!finite(world) || world.z - p.camera.position.z <= 0) return {false, "clip-unsupported"};
   auto s = Project(p.camera, world);
   if (!finite(s) || s.z < p.camera.nearPlane || s.z > p.camera.farPlane) return {false, "clip-unsupported"};
  }
 }
 return {true, "synthetic-untextured-adapter-ready"};
}
std::vector<std::uint32_t> Triangles(const Mesh& m) {
 std::vector<std::uint32_t> out;
 if (m.indices.size() > Limits{}.indices || m.vertices.size() > Limits{}.vertices) throw std::invalid_argument("topology bounds");
 if (m.topology != Topology::Triangles && m.topology != Topology::Strip) throw std::invalid_argument("topology");
 if (m.indices.size() < 3 || (m.topology == Topology::Triangles && m.indices.size()%3)) throw std::invalid_argument("topology");
 for (auto i : m.indices) if (i >= m.vertices.size()) throw std::invalid_argument("index");
 for (std::size_t i = 0; i + 2 < m.indices.size(); i += m.topology == Topology::Strip ? 1 : 3) {
  auto a=m.indices[i], b=m.indices[i+1], c=m.indices[i+2];
  if (m.topology == Topology::Strip && (i&1)) std::swap(a,b);
  if (a!=b && b!=c && a!=c) out.insert(out.end(), {a,b,c});
 }
 return out;
}
Vec3 WorldPosition(const Mesh& m, const Vertex& v) {
 if (!m.transform) throw std::invalid_argument("unknown transform");
 const auto& a=*m.transform; const auto p=v.position;
 return {a[0]*p.x+a[1]*p.y+a[2]*p.z+a[3], a[4]*p.x+a[5]*p.y+a[6]*p.z+a[7], a[8]*p.x+a[9]*p.y+a[10]*p.z+a[11]};
}
Vec3 Project(const Camera& c, Vec3 p) {
 if (c.provenance == Provenance::Unknown || !validCamera(c) || !finite(p)) throw std::invalid_argument("projection");
 p={p.x-c.position.x,p.y-c.position.y,p.z-c.position.z};
 if (p.z <= 0) throw std::invalid_argument("behind camera");
 return {0.5f + p.x/(2*p.z*tangent(c)*c.aspect),0.5f - p.y/(2*p.z*tangent(c)),p.z};
}
Vec3 Unproject(const Camera& c, Vec3 p) {
 if (c.provenance == Provenance::Unknown || !validCamera(c) || !finite(p) || p.z <= 0) throw std::invalid_argument("unprojection");
 return {(p.x-.5f)*2*p.z*tangent(c)*c.aspect+c.position.x,
  (.5f-p.y)*2*p.z*tangent(c)+c.position.y,p.z+c.position.z};
}
std::optional<float> DepthAt(const Packet& p, float x, float y) {
 if (!ReadyForAdapter(p,p.frame,p.game).ok) return {};
 std::optional<float> nearest;
 for (const auto& m:p.meshes) {
  auto indices=Triangles(m);
  for (std::size_t i=0;i<indices.size();i+=3) {
   const auto a=Project(p.camera,WorldPosition(m,m.vertices[indices[i]]));
   const auto b=Project(p.camera,WorldPosition(m,m.vertices[indices[i+1]]));
   const auto c=Project(p.camera,WorldPosition(m,m.vertices[indices[i+2]]));
   const float d=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
   if (std::abs(d)<1e-8f) continue;
   const float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/d;
   const float v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/d, w=1-u-v;
   if (u<0 || v<0 || w<0) continue;
   const float z=1/(u/a.z+v/b.z+w/c.z);
   if (!nearest || z<*nearest) nearest=z;
  }
 }
 return nearest;
}
Packet Synthetic(std::uint64_t frame,float cameraX) {
 Packet p; p.frame=frame; p.game="synthetic-overlap"; p.space=Space::World;
 p.camera.provenance=Provenance::Analytic; p.camera.position.x=cameraX;
 for (int n=1;n<=2;++n) {
  Mesh m; m.id=n; m.frame=frame;
  m.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  const float s=float(n);
  m.vertices={{{-s,-s,2*s},Vec3{0,0,-1},0,1},{{s,-s,2*s},Vec3{0,0,-1},1,1},{{0,s,2*s},Vec3{0,0,-1},.5f,0}};
  m.indices={0,1,2}; p.meshes.push_back(m);
 }
 return p;
}
}
