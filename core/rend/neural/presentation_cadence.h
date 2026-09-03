// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>

namespace flycast::rend::neural {

struct PresentationCadenceStats {
	std::uint64_t observedPresents = 0;
	std::uint64_t missingPresents = 0;
	std::uint64_t acceptedEvaluations = 0;
	std::uint64_t neuralPresents = 0;
	std::uint64_t nativePresents = 0;
	std::uint64_t acceptedNotPresented = 0;
	std::uint64_t frameIdentityMismatches = 0;
	std::uint64_t sourceFrameRepeats = 0;
	std::uint64_t sourceFrameGaps = 0;
	std::uint64_t outputFrameRepeats = 0;
	std::uint64_t nativeNeuralAlternations = 0;
	std::uint64_t latencySamples = 0;
	std::uint64_t latencyFramesTotal = 0;
	std::uint64_t latencyFramesMax = 0;
};

// Pure presentation accounting kept separate from GPU-query collection so its
// repeat, drop, identity, and latency rules remain deterministic and testable.
class PresentationCadence final {
public:
	void Observe(std::uint64_t sourceFrameId, std::uint64_t acceptedFrameId,
		std::uint64_t outputFrameId, bool presented) noexcept
	{
		if (acceptedFrameId != 0)
			++stats_.acceptedEvaluations;
		if (!presented)
		{
			++stats_.missingPresents;
			if (acceptedFrameId != 0)
				++stats_.acceptedNotPresented;
			return;
		}

		++stats_.observedPresents;
		const bool neural = outputFrameId != 0;
		if (neural)
			++stats_.neuralPresents;
		else
			++stats_.nativePresents;
		if (hasPresentation_ && neural != previousNeural_)
			++stats_.nativeNeuralAlternations;
		hasPresentation_ = true;
		previousNeural_ = neural;

		if (sourceFrameId != 0 && previousSourceFrameId_ != 0)
		{
			if (sourceFrameId == previousSourceFrameId_)
				++stats_.sourceFrameRepeats;
			else if (sourceFrameId > previousSourceFrameId_ + 1)
				stats_.sourceFrameGaps += sourceFrameId - previousSourceFrameId_ - 1;
		}
		if (sourceFrameId != 0)
			previousSourceFrameId_ = sourceFrameId;

		if (acceptedFrameId != 0 && outputFrameId != acceptedFrameId)
			++stats_.acceptedNotPresented;
		if (!neural)
		{
			previousOutputFrameId_ = 0;
			return;
		}
		if (previousOutputFrameId_ == outputFrameId)
			++stats_.outputFrameRepeats;
		previousOutputFrameId_ = outputFrameId;
		if (sourceFrameId == 0 || outputFrameId > sourceFrameId)
		{
			++stats_.frameIdentityMismatches;
			return;
		}
		const auto latency = sourceFrameId - outputFrameId;
		++stats_.latencySamples;
		stats_.latencyFramesTotal += latency;
		stats_.latencyFramesMax = (std::max)(stats_.latencyFramesMax, latency);
		if (acceptedFrameId != 0 && outputFrameId != acceptedFrameId)
			++stats_.frameIdentityMismatches;
	}

	const PresentationCadenceStats& Stats() const noexcept { return stats_; }

private:
	PresentationCadenceStats stats_{};
	std::uint64_t previousSourceFrameId_ = 0;
	std::uint64_t previousOutputFrameId_ = 0;
	bool hasPresentation_ = false;
	bool previousNeural_ = false;
};

} // namespace flycast::rend::neural
