// SPDX-License-Identifier: GPL-2.0-or-later
#include "neural_stage.h"

#include <chrono>

namespace flycast::rend::neural {

namespace {
std::uint64_t MonotonicMilliseconds() noexcept
{
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

SubmitStatus NeuralStage::TrySubmit(const NeuralFrame& frame) noexcept
{
	if (config_.mode == NeuralMode::Off)
		return SubmitStatus::Disabled;
	if (frame.source != FrameSource::Geometry)
	{
		if (!hasFrame_ || frame.source != lastSource_)
			history_.Discontinuity();
		lastSource_ = frame.source;
		hasFrame_ = true;
		++stats_.fallbacks;
		return SubmitStatus::Unsupported;
	}
	if (!recovery_.CanEvaluate(MonotonicMilliseconds()))
		return SubmitStatus::Holding;
	if (hasEvaluated_ && frame.frameId == history_.EvaluatedFrameId() && output_)
		return SubmitStatus::Submitted;
	if (recovery_.ConsumeResumeReset())
	{
		history_.Discontinuity();
		++stats_.resets;
	}
	if (!hasFrame_ || frame.source != lastSource_ ||
		frame.historyGeneration != lastHistoryGeneration_ || frame.resetHistory)
	{
		history_.Discontinuity();
		++stats_.resets;
	}
	lastSource_ = frame.source;
	lastHistoryGeneration_ = frame.historyGeneration;
	hasFrame_ = true;
	if (recreateRequested_)
	{
		recreateRequested_ = false;
		history_.Discontinuity();
		++stats_.resets;
	}
	if (config_.mode == NeuralMode::Passthrough)
	{
		output_ = frame.color;
		history_.Evaluated(frame.frameId);
		hasEvaluated_ = true;
		recovery_.RecordSuccess(frame.frameId);
		++stats_.submissions;
		return SubmitStatus::Submitted;
	}
	// Phase 0 skeleton. Backends replace this explicit unsupported result before
	// presentation integration is enabled.
	++stats_.fallbacks;
	return SubmitStatus::Unsupported;
}

void NeuralStage::Shutdown() noexcept
{
	output_ = {};
	history_ = {};
	recovery_ = {};
	lastHistoryGeneration_ = 0;
	lastSource_ = FrameSource::Geometry;
	hasFrame_ = false;
	hasEvaluated_ = false;
	recreateRequested_ = false;
}

} // namespace flycast::rend::neural
