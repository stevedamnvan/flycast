// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_frame.h"
#include "dlss5_hook.h"

#include <cstdint>
#include <memory>

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

enum class FailureInjection : std::uint8_t {
	None,
	FeatureCreate,
	Evaluate,
	OutputBusy,
	DeviceRemoved,
};

struct StageConfig {
	NeuralMode mode = NeuralMode::Off;
	Api api = Api::D3D11;
	std::uint32_t outputWidth = 0;
	std::uint32_t outputHeight = 0;
	Rect contentRect;
	bool hookCompatibility = false;
	// Flycast's PVR depth clears to zero and increases toward the camera. This
	// is independently exercised by neuraltest's production-shader fixture.
	bool depthInverted = true;
	// Public DLSS render-preset hint: 0 Auto, 10 J, 11 K.
	std::uint32_t dlssPreset = 0;
	Dlss5HookRoute dlss5Route = Dlss5HookRoute::None;
	std::uint32_t dlss5RebuildGraceEvaluations = 300;
	std::uint32_t dlss5RebuildMaxAttempts = 2;
	bool dlss5EvidenceCapture = false;
	std::uint32_t dlss5EvidenceCaptureFrames = 1;
	// Developer-only deterministic failure controls. Production defaults never
	// enter these paths and no real device state is modified.
	FailureInjection failureInjection = FailureInjection::None;
	std::uint32_t failureInjectionCount = 0;
	std::uint32_t failureInjectionAfter = 0;
};

enum class BackendEvalStatus : std::uint8_t {
	Success,
	Busy,
	Unsupported,
	RecoverableFailure,
	DeviceRemoved,
};

struct BackendStats {
	// Live Flycast-owned GPU resource/view/query/command objects in the backend.
	// Borrowed device/context/queue pointers and NGX-owned internals are excluded.
	std::uint32_t liveResourceObjects = 0;
	std::uint64_t createFailures = 0;
	std::uint64_t evaluateFailures = 0;
	std::int32_t lastResult = 0;
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

class INeuralBackend
{
public:
	virtual ~INeuralBackend() = default;
	virtual BackendEvalStatus Initialize(const StageConfig& config, void *device,
		void *context) noexcept = 0;
	virtual BackendEvalStatus Evaluate(const NeuralFrame& frame) noexcept = 0;
	virtual void ResetHistory() noexcept = 0;
	virtual BackendStats GetStats() const noexcept = 0;
	virtual TextureRef GetOutput() const noexcept = 0;
	virtual const char *GetStatusReason() const noexcept = 0;
	virtual void Shutdown() noexcept = 0;
};

std::unique_ptr<INeuralBackend> CreateNeuralBackend(NeuralMode mode, Api api);

} // namespace flycast::rend::neural
