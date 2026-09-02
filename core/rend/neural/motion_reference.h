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

struct SimilarityTransform {
	float scaleCos = 1.f;
	float scaleSin = 0.f;
	float translateX = 0.f;
	float translateY = 0.f;
	bool valid = false;

	Point2 Apply(Point2 point) const noexcept;
};

std::uint64_t DrawSignature(const DrawRecord& draw) noexcept;
bool IsReactive(const DrawRecord& draw) noexcept;
std::vector<DrawMatch> MatchDraws(ArrayView<DrawRecord> previous,
	ArrayView<DrawRecord> current);
SimilarityTransform FitSimilarity(ArrayView<Point2> previous,
	ArrayView<Point2> current) noexcept;
bool IsSceneCut(std::uint64_t matchedArea, std::uint64_t totalArea,
	float minimumRatio = .35f) noexcept;
float Halton(std::uint32_t index, std::uint32_t base) noexcept;
Point2 HaltonJitter(std::uint64_t frameId, std::uint32_t phaseCount) noexcept;
std::uint32_t JitterPhaseCount(std::uint32_t renderWidth, std::uint32_t outputWidth) noexcept;

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
