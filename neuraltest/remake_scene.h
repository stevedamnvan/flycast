// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_scene.h"
namespace neuraltest::remake {
// Compatibility names for developer fixtures. Live code owns the shared scene
// contract; production providers no longer need to include a harness type.
using namespace flycast::rend::neural::remake;
TestCounts TestSceneContract();
}
