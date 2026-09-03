// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_backend.h"
#include "motion_reference.h"
#include "recovery_controller.h"

#include <cstdint>
#include <memory>

namespace flycast::rend::neural {

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
	std::uint32_t backendResourceObjects = 0;
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
	std::uint64_t deviceRemovedStatuses = 0;
	std::int32_t lastNgxResult = 0;
	std::uint32_t lastExceptionCode = 0;
	std::uint64_t compatibilityRebuilds = 0;
	std::uint64_t compatibilityRebuildAttempts = 0;
	std::uint64_t compatibilityRebuildFailures = 0;
	Dlss5RebuildReason compatibilityRebuildReason = Dlss5RebuildReason::None;
	bool dlss5ContractEvaluated = false;
	Dlss5HookRoute dlss5Route = Dlss5HookRoute::None;
	Dlss5HookReadiness dlss5Readiness = Dlss5HookReadiness::Disabled;
	Dlss5HookComponents dlss5Components{};
	std::uint64_t evidenceFrameId = 0;
	std::uint64_t evidenceInputHash = 0;
	std::uint64_t evidenceDepthHash = 0;
	std::uint64_t evidenceMotionHash = 0;
	std::uint64_t evidenceMaskHash = 0;
	std::uint64_t evidenceOutputHash = 0;
	std::uint64_t evidenceMarkedOutputHash = 0;
	std::uint64_t evidenceWaitMicroseconds = 0;
	std::uint64_t evidenceCaptures = 0;
	std::uint64_t evidenceCaptureFailures = 0;
};

class NeuralStage final {
public:
	NeuralStage();
	explicit NeuralStage(const StageConfig& config);
	~NeuralStage();
	NeuralStage(NeuralStage&&) noexcept;
	NeuralStage& operator=(NeuralStage&&) noexcept;
	NeuralStage(const NeuralStage&) = delete;
	NeuralStage& operator=(const NeuralStage&) = delete;

	void SetGraphicsDevice(Api api, void *device, void *context) noexcept;
	SubmitStatus TrySubmit(const NeuralFrame& frame) noexcept;
	StageStats GetStats() const noexcept { return stats_; }
	TextureRef GetOutput() const noexcept { return output_; }
	void RequestRecreate() noexcept { recreateRequested_ = true; }
	void NotifyHostPresent() noexcept { recovery_.OnHostPresent(); }
	void Shutdown() noexcept;
	const char *GetStatusReason() const noexcept;

private:
	StageConfig config_{};
	StageStats stats_{};
	TextureRef output_{};
	HistoryTracker history_{};
	RecoveryController recovery_{};
	std::unique_ptr<INeuralBackend> backend_;
	void *device_ = nullptr;
	void *context_ = nullptr;
	std::uint32_t backendRenderWidth_ = 0;
	std::uint32_t backendRenderHeight_ = 0;
	bool backendInitialized_ = false;
	bool backendPermanentlyUnsupported_ = false;
	std::uint32_t lastHistoryGeneration_ = 0;
	FrameSource lastSource_ = FrameSource::Geometry;
	bool hasFrame_ = false;
	bool hasEvaluated_ = false;
	bool recreateRequested_ = false;
};

} // namespace flycast::rend::neural
