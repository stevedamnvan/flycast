// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_stage.h"

#include <cstdint>
#include <string>

namespace flycast::rend::neural {

// Render-thread data copied across a mutex boundary for developer UI. The UI
// never reads NeuralStage or renderer-owned state directly.
struct LiveStatus {
	bool rendererAvailable = false;
	bool active = false;
	bool d3d11On12 = false;
	bool conservativeBypass = false;
	bool overlayProtection = false;
	bool debugViewActive = false;
	NeuralMode mode = NeuralMode::Off;
	Api api = Api::D3D11;
	SubmitStatus lastSubmit = SubmitStatus::Disabled;
	StageStats stage{};
	std::string reason = "no live DirectX 11 renderer";
	std::uint32_t renderWidth = 0;
	std::uint32_t renderHeight = 0;
	std::uint32_t outputWidth = 0;
	std::uint32_t outputHeight = 0;
	std::uint32_t overlayDraws = 0;
	std::uint32_t debugView = 0;
	std::uint64_t sourceFrameId = 0;
	std::uint64_t presentedOutputFrameId = 0;
	std::uint64_t generation = 0;
};

void PublishLiveStatus(LiveStatus status);
LiveStatus GetLiveStatus();
void ResetLiveStatus();

const char *NeuralModeName(NeuralMode mode) noexcept;
const char *SubmitStatusName(SubmitStatus status) noexcept;
const char *ApiName(Api api) noexcept;

} // namespace flycast::rend::neural
