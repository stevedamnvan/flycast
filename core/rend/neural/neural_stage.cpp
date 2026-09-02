// SPDX-License-Identifier: GPL-2.0-or-later
#include "neural_stage.h"

namespace flycast::rend::neural {

SubmitStatus NeuralStage::TrySubmit(const NeuralFrame& frame) noexcept
{
	if (config_.mode == NeuralMode::Off)
		return SubmitStatus::Disabled;
	if (frame.source != FrameSource::Geometry)
	{
		++stats_.fallbacks;
		return SubmitStatus::Unsupported;
	}
	if (recreateRequested_)
	{
		recreateRequested_ = false;
		++stats_.resets;
	}
	// Phase 0 skeleton. Backends replace this explicit unsupported result before
	// presentation integration is enabled.
	++stats_.fallbacks;
	return SubmitStatus::Unsupported;
}

void NeuralStage::Shutdown() noexcept
{
	output_ = {};
	recreateRequested_ = false;
}

} // namespace flycast::rend::neural
