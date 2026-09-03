// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_frame.h"

#include <cstdint>
#include <vector>

namespace flycast::rend::neural {

enum DrawFlags : std::uint8_t {
	DrawReactive = 1u << 0,
	DrawRtt = 1u << 1,
	DrawNaomi2 = 1u << 2,
	DrawDegenerate = 1u << 3,
	DrawAdditive = 1u << 4,
	DrawScreenAligned = 1u << 5,
};

enum class MatchReason : std::uint8_t {
	None,
	Exact,
	Structural,
	Similarity,
	Reactive,
	Ambiguous,
};

struct Point2 {
	float x = 0.f;
	float y = 0.f;
};

struct Point3 {
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
};

struct RasterSize {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

struct MotionTrust {
	Point2 motion{};
	float confidence = 0.f;
	bool trusted = false;
	bool biasCurrentColor = true;
};

struct SimilarityTransform {
	float scaleCos = 1.f;
	float scaleSin = 0.f;
	float translateX = 0.f;
	float translateY = 0.f;
	bool valid = false;

	Point2 Apply(Point2 point) const noexcept;
};

std::uint64_t DrawSignature(const DrawRecord& draw) noexcept;
std::uint64_t DrawStructuralSignature(const DrawRecord& draw) noexcept;
bool IsReactive(const DrawRecord& draw) noexcept;
std::vector<DrawMatch> MatchDraws(ArrayView<DrawRecord> previous,
	ArrayView<DrawRecord> current);
void MatchDrawsInto(ArrayView<DrawRecord> previous, ArrayView<DrawRecord> current,
	DrawMatch *output, std::size_t outputCapacity) noexcept;
float StripCoverage(const DrawRecord& previous, const DrawRecord& current) noexcept;
SimilarityTransform FitSimilarity(ArrayView<Point2> previous,
	ArrayView<Point2> current) noexcept;
Point2 ProjectNaomi2(const float *modelView, const float *projection,
	const float *ndc, Point3 position) noexcept;
MotionTrust ClassifyMotion(const DrawMatch& match, Point2 motion,
	bool resetHistory, bool truncated) noexcept;
bool IsSceneCut(std::uint64_t matchedArea, std::uint64_t totalArea,
	float minimumRatio = .35f) noexcept;
bool IsHighConfidenceOverlay(const DrawRecord& draw, std::size_t drawCount,
	std::uint32_t renderWidth, std::uint32_t renderHeight,
	std::uint8_t stableAcceptedFrames, std::uint16_t textureUseCount) noexcept;
bool IsPredominantly2DFrame(ArrayView<DrawRecord> draws,
	std::uint32_t renderWidth, std::uint32_t renderHeight) noexcept;
bool UpdateConservativeBypass(bool candidate, bool active,
	std::uint8_t& enterStreak, std::uint8_t& exitStreak,
	std::uint8_t threshold = 3) noexcept;
float InvertLegacyDepth(float encodedDepth, bool divPosZ) noexcept;
Rect ComputeContentRect(std::uint32_t outputWidth, std::uint32_t outputHeight,
	float renderAspect, bool integerScale, std::uint32_t renderResolution) noexcept;
RasterSize ComputeMatchOutputRasterSize(std::uint32_t outputWidth,
	std::uint32_t outputHeight, float renderAspect, bool rotate) noexcept;
bool UsesMatchOutputRaster(int neuralMode) noexcept;
float Halton(std::uint32_t index, std::uint32_t base) noexcept;
Point2 HaltonJitter(std::uint64_t frameId, std::uint32_t phaseCount) noexcept;
std::uint32_t JitterPhaseCount(std::uint32_t renderWidth, std::uint32_t outputWidth) noexcept;
std::size_t NextHistorySafeRingSlot(std::size_t currentSlot, std::size_t acceptedSlot,
	std::size_t ringSize, bool hasAcceptedHistory) noexcept;

class HistoryTracker final {
public:
	void Discontinuity() noexcept;
	void Skipped() noexcept;
	void Evaluated(std::uint64_t frameId) noexcept;
	bool ConsumeReset() noexcept;
	std::uint64_t EvaluatedFrameId() const noexcept { return evaluatedFrameId_; }
	std::uint32_t Generation() const noexcept { return generation_; }
	std::uint32_t ConsecutiveSkips() const noexcept { return consecutiveSkips_; }

private:
	std::uint64_t evaluatedFrameId_ = 0;
	std::uint32_t generation_ = 0;
	std::uint32_t consecutiveSkips_ = 0;
	bool resetPending_ = true;
};

} // namespace flycast::rend::neural
