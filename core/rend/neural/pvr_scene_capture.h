// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
struct rend_context;
namespace flycast::rend::neural {
// Developer capture only. No pointers or texture contents are serialized.
bool WritePvrScenePacket(const std::filesystem::path&, const rend_context&,
 const std::array<float,16>& viewport, std::uint64_t frame,
 const std::string& game, std::string& error);
}
