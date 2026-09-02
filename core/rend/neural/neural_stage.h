// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_frame.h"
#include "motion_reference.h"
#include "recovery_controller.h"

#include <cstdint>

namespace flycast::rend::neural {

enum class NeuralMode : std::uint8_t {
	Off,
	Passthrough,
	Dlaa,
	DlaaHook,
	SrQuality,
	SrBalanced,
	SrPerformance,
	SrUltraPerformance,
	Dlss5Experimental,
};

enum class Api : std::uint8_t { D3D11, D3D12 };

struct StageConfig {
	NeuralMode mode = NeuralMode::Off;
	Api api = Api::D3D11;
	std::uint32_t outputWidth = 0;
	std::uint32_t outputHeight = 0;
	Rect contentRect;
	bool hookCompatibility = false;
};

enum class SubmitStatus : std::uint8_t {
	Submitted,
	Busy,
	Holding,
	Unsupported,
	RecoverableFailure,
	Disabled,
	DeviceRemoved,
};

struct StageStats {
	double exportGpuMs = 0.;
	double evaluateGpuMs = 0.;
	double totalGpuMs = 0.;
	std::uint64_t submissions = 0;
	std::uint64_t busySkips = 0;
	std::uint64_t holdEntries = 0;
	std::uint64_t fallbacks = 0;
	std::uint64_t resets = 0;
	std::uint64_t createFailures = 0;
	std::uint64_t evaluateFailures = 0;
	std::int32_t lastNgxResult = 0;
	std::uint32_t lastExceptionCode = 0;
};

class NeuralStage final {
public:
	NeuralStage() = default;
	explicit NeuralStage(const StageConfig& config) : config_(config) {}

	SubmitStatus TrySubmit(const NeuralFrame& frame) noexcept;
	StageStats GetStats() const noexcept { return stats_; }
	TextureRef GetOutput() const noexcept { return output_; }
	void RequestRecreate() noexcept { recreateRequested_ = true; }
	void NotifyHostPresent() noexcept { recovery_.OnHostPresent(); }
	void Shutdown() noexcept;

private:
	StageConfig config_{};
	StageStats stats_{};
	TextureRef output_{};
	HistoryTracker history_{};
	RecoveryController recovery_{};
	std::uint32_t lastHistoryGeneration_ = 0;
	FrameSource lastSource_ = FrameSource::Geometry;
	bool hasFrame_ = false;
	bool hasEvaluated_ = false;
	bool recreateRequested_ = false;
};

} // namespace flycast::rend::neural
