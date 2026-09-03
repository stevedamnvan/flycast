// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_stage.h"
#include "presentation_cadence.h"
#include <d3d11.h>
#include "windows/comptr.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace flycast::rend::neural {

enum class GpuTimingPoint : std::uint8_t {
	PvrBegin,
	PvrEnd,
	GuidanceBegin,
	GuidanceEnd,
	EvaluateBegin,
	EvaluateEnd,
	CompositeBegin,
	CompositeEnd,
	Count,
};

// Asynchronous production telemetry. GetData always uses DONOTFLUSH and this
// class never performs a readback, wait, Flush, or finish-style operation.
class PerformanceTracker {
public:
	void Configure(ID3D11Device *device, const std::filesystem::path& root,
		std::uint32_t warmupFrames, std::uint32_t sampleFrames,
		std::string gameId, std::string api, std::string renderer, int neuralMode,
		int failureInjection, std::uint32_t failureInjectionCount,
		std::uint32_t failureInjectionAfter);
	void BeginFrame(ID3D11DeviceContext *context);
	void Mark(ID3D11DeviceContext *context, GpuTimingPoint point);
	void RecordEvaluation(std::uint64_t frameId, bool accepted) noexcept;
	void StagePresentation(std::uint64_t sourceFrameId,
		std::uint64_t outputFrameId) noexcept;
	void EndFrame(ID3D11DeviceContext *context, const StageStats& stats,
		std::uint32_t rendererResourceObjects);
	void RecordPresent() noexcept;
	bool Enabled() const noexcept { return !root_.empty() && targetSamples_ != 0 && !written_; }

private:
	static constexpr std::size_t RingSize = 12;
	static constexpr std::size_t PointCount = static_cast<std::size_t>(GpuTimingPoint::Count);
	struct Slot {
		ComPtr<ID3D11Query> disjoint;
		std::array<ComPtr<ID3D11Query>, PointCount> points;
		std::array<bool, PointCount> marked{};
		bool pending = false;
		bool presented = false;
		std::uint64_t sequence = 0;
		std::uint64_t sourceFrameId = 0;
		std::uint64_t acceptedFrameId = 0;
		std::uint64_t outputFrameId = 0;
		std::uint32_t rendererResourceObjects = 0;
		std::uint32_t backendResourceObjects = 0;
		double presentIntervalMs = 0.;
	};
	struct Sample {
		double pvrMs = 0.;
		double guidanceMs = 0.;
		double evaluateMs = 0.;
		double compositeMs = 0.;
		double totalGpuMs = 0.;
		double presentIntervalMs = 0.;
		bool presented = false;
		std::uint64_t sequence = 0;
		std::uint64_t sourceFrameId = 0;
		std::uint64_t acceptedFrameId = 0;
		std::uint64_t outputFrameId = 0;
		std::uint32_t rendererResourceObjects = 0;
		std::uint32_t backendResourceObjects = 0;
	};
	struct BackendEvaluateSample {
		std::uint64_t frameId = 0;
		double milliseconds = 0.;
	};

	void Reset();
	bool CreateQueries(ID3D11Device *device);
	void ResolveAvailable(ID3D11DeviceContext *context);
	void WriteReport();

	ComPtr<ID3D11Device> device_;
	std::array<Slot, RingSize> ring_;
	std::filesystem::path root_;
	std::uint32_t warmupFrames_ = 0;
	std::uint32_t warmupRemaining_ = 0;
	std::uint32_t targetSamples_ = 0;
	std::uint32_t ringBusy_ = 0;
	std::size_t activeSlot_ = RingSize;
	std::size_t lastEndedSlot_ = RingSize;
	std::uint64_t nextSequence_ = 1;
	std::vector<Sample> samples_;
	std::vector<BackendEvaluateSample> backendEvaluateSamples_;
	std::uint64_t lastBackendEvaluateSample_ = 0;
	StageStats stageStats_{};
	std::string gameId_;
	std::string api_;
	std::string renderer_;
	int neuralMode_ = 0;
	int failureInjection_ = 0;
	std::uint32_t failureInjectionCount_ = 0;
	std::uint32_t failureInjectionAfter_ = 0;
	std::uint64_t initialVramUsage_ = 0;
	std::chrono::steady_clock::time_point lastPresent_{};
	double lastPresentIntervalMs_ = 0.;
	bool written_ = false;
};

} // namespace flycast::rend::neural
