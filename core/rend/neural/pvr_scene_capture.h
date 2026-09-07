// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <optional>
#include "hw/pvr/ta_ctx.h"
namespace flycast::rend::neural {
struct PvrCapturedTexture {
 std::uint32_t upload=0, rtt=0;
 std::optional<std::uint32_t> palette;
};
struct PvrCapturedDraw {
 std::uint32_t list=0, ordinal=0;
 PolyParam state{}; // Texture pointers stay null; resolve only against retained resources.
 std::optional<PvrCapturedTexture> texture, texture1;
};
struct PvrCapturedPass {
 std::uint32_t op=0, pt=0, tr=0, mvo=0, sortedTr=0;
 bool autosort=false, zClear=false;
};
struct PvrDecodedPacket {
 std::uint64_t frame=0;
 std::string game, gitSha;
 std::array<float,16> viewport{};
 std::array<std::uint32_t,2> framebufferSize{};
 bool clearFramebuffer=false;
 std::vector<Vertex> vertices;
 std::vector<std::uint32_t> indices;
 std::vector<PvrCapturedDraw> draws;
 std::vector<PvrCapturedPass> passes;
 std::vector<std::string> omissions;
 std::uint32_t modifierTriangles=0, unusedNonfinite=0;
 // This remains a projected packet, never a recovered world/camera scene.
};
// Bounded decoder, not renderer activation. Leaves output unchanged on failure.
bool ReadPvrScenePacket(const std::filesystem::path&, std::uint64_t expectedFrame,
 const std::string& expectedGame, PvrDecodedPacket&, std::string& error);
// Developer capture only. No pointers or texture contents are serialized.
bool WritePvrScenePacket(const std::filesystem::path&, const rend_context&,
 const std::array<float,16>& viewport, std::uint64_t frame,
 const std::string& game, std::string& error);
}
