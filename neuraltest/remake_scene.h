// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

namespace neuraltest::remake {
struct Vec3 { float x = 0, y = 0, z = 0; };
enum class Space { World, View, PvrProjected };
enum class Provenance { Unknown, Supplied, Analytic };
enum class Topology { Triangles, Strip };
struct Camera {
 Provenance provenance = Provenance::Unknown;
 // Explicit orthonormal source-space camera basis; no inferred game camera.
 Vec3 position;
 float fovY = 90, aspect = 1, nearPlane = 0.1f, farPlane = 100;
 Vec3 right{1,0,0}, up{0,1,0}, forward{0,0,1};
};
struct Vertex {
 Vec3 position; std::optional<Vec3> normal; float u = 0, v = 0;
 // Public HardcodedVertex uses B8G8R8A8_UNORM at reviewed Remix revision.
 std::uint32_t publicColor = 0xffffffffu;
};
constexpr std::uint32_t PublicColorFromBgra(std::array<std::uint8_t,4> bytes) {
 return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1])<<8)
  | (std::uint32_t(bytes[2])<<16) | (std::uint32_t(bytes[3])<<24);
}
struct TextureIdentity {
 std::uint64_t id = 0, generation = 0, paletteGeneration = 0, rttGeneration = 0;
 bool known = false;
};
struct Mesh {
 struct Material {
  Vec3 albedo{.7f,.7f,.7f}; float roughness=.8f;
  std::filesystem::path sourceDds;
  bool sourceColorExperiment=false; // Not physical albedo or full PVR shading.
 };
 std::optional<Material> material; // Explicit caller art direction, not inferred PBR.
 std::uint64_t id = 0, frame = 0;
 Topology topology = Topology::Triangles;
 TextureIdentity texture;
 // Row-major 3x4 object-to-world; absent means unknown, not identity.
 std::optional<std::array<float, 12>> transform;
 std::vector<Vertex> vertices;
 std::vector<std::uint32_t> indices;
};
struct Packet {
 std::uint32_t version = 1;
 std::uint64_t frame = 0;
 std::string game;
 Space space = Space::PvrProjected;
 Camera camera;
 bool truncated = false;
 std::vector<std::string> omissions;
 std::vector<Mesh> meshes;
};
struct Limits {
 std::size_t meshes = 128, vertices = 65536, indices = 262144, bytes = 8 * 1024 * 1024;
};
struct Result { bool ok; std::string reason; };
Result Validate(const Packet&, std::uint64_t expectedFrame, const std::string& expectedGame,
 const Limits& = {});
Result ReadyForAdapter(const Packet&, std::uint64_t expectedFrame, const std::string& expectedGame);
std::vector<std::uint32_t> Triangles(const Mesh&);
struct FlatNormalMesh {
 Mesh mesh;
 std::size_t degenerateTriangles = 0;
 // Source normals remain unknown; these describe the supplied coordinate mesh.
 const char* normalProvenance = "geometry-derived-flat";
};
// Explicit experiment only. Does not establish coordinate or camera truth.
FlatNormalMesh DeriveFlatNormals(const Mesh&, Space, const Limits& = {});
Vec3 WorldPosition(const Mesh&, const Vertex&);
// Synthetic normalized viewport: x right, y down; z remains linear view Z.
Vec3 Project(const Camera&, Vec3 world);
Vec3 Unproject(const Camera&, Vec3 viewport);
std::optional<float> DepthAt(const Packet&, float x, float y);
Packet Synthetic(std::uint64_t frame = 7, float cameraX = 0);
struct TestCounts { int passed = 0, failed = 0; };
TestCounts TestSceneContract();
}
