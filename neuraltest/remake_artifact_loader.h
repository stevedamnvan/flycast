// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
namespace neuraltest::remake {
// Explicit caller clip interval; source JSON retains unknown game clips.
Packet LoadDiagnosticArtifact(const std::filesystem::path& artifact,
 const std::filesystem::path& assetDirectory,float nearPlane,float farPlane);
}
