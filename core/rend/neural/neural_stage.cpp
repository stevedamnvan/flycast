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

NeuralStage::NeuralStage() = default;
NeuralStage::NeuralStage(const StageConfig& config) : config_(config) {}
NeuralStage::~NeuralStage() { Shutdown(); }
NeuralStage::NeuralStage(NeuralStage&&) noexcept = default;
NeuralStage& NeuralStage::operator=(NeuralStage&&) noexcept = default;

void NeuralStage::SetGraphicsDevice(Api api, void *device, void *context) noexcept
{
	if (config_.api != api || device_ != device || context_ != context)
	{
		if (backend_) backend_->Shutdown();
		backend_.reset();
		backendInitialized_ = false;
		backendPermanentlyUnsupported_ = false;
		device_ = device;
		context_ = context;
		config_.api = api;
	}
}

const char *NeuralStage::GetStatusReason() const noexcept
{
	return backend_ ? backend_->GetStatusReason() : "not initialized";
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
		if (backend_) backend_->Shutdown();
		backendInitialized_ = false;
		backendPermanentlyUnsupported_ = false;
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
	const bool dimensionsChanged = backendRenderWidth_ != frame.renderWidth
		|| backendRenderHeight_ != frame.renderHeight
		|| config_.outputWidth != frame.outputWidth || config_.outputHeight != frame.outputHeight
		|| config_.contentRect.x != frame.contentRect.x || config_.contentRect.y != frame.contentRect.y
		|| config_.contentRect.width != frame.contentRect.width
		|| config_.contentRect.height != frame.contentRect.height;
	if (dimensionsChanged && backend_)
	{
		backend_->Shutdown();
		backendInitialized_ = false;
		backendPermanentlyUnsupported_ = false;
	}
	config_.outputWidth = frame.outputWidth;
	config_.outputHeight = frame.outputHeight;
	config_.contentRect = frame.contentRect;
	backendRenderWidth_ = frame.renderWidth;
	backendRenderHeight_ = frame.renderHeight;
	if (!backend_)
		backend_ = CreateNeuralBackend(config_.mode, config_.api);
	if (backendPermanentlyUnsupported_)
	{
		++stats_.fallbacks;
		return SubmitStatus::Unsupported;
	}
	if (!backendInitialized_)
	{
		const auto init = backend_->Initialize(config_, device_, context_);
		const auto backendStats = backend_->GetStats();
		stats_.createFailures = backendStats.createFailures;
		stats_.lastNgxResult = backendStats.lastResult;
		stats_.lastExceptionCode = backendStats.lastExceptionCode;
		stats_.compatibilityRebuilds = backendStats.compatibilityRebuilds;
		stats_.compatibilityRebuildAttempts = backendStats.compatibilityRebuildAttempts;
		stats_.compatibilityRebuildFailures = backendStats.compatibilityRebuildFailures;
		stats_.compatibilityRebuildReason = backendStats.compatibilityRebuildReason;
		stats_.dlss5ContractEvaluated = backendStats.dlss5ContractEvaluated;
		stats_.dlss5Route = backendStats.dlss5Route == Dlss5HookRoute::None
			&& config_.mode == NeuralMode::Dlss5Experimental
			? config_.dlss5Route : backendStats.dlss5Route;
		stats_.dlss5Readiness = backendStats.dlss5Readiness;
		stats_.dlss5Components = backendStats.dlss5Components;
		stats_.evidenceFrameId = backendStats.evidenceFrameId;
		stats_.evidenceInputHash = backendStats.evidenceInputHash;
		stats_.evidenceDepthHash = backendStats.evidenceDepthHash;
		stats_.evidenceMotionHash = backendStats.evidenceMotionHash;
		stats_.evidenceMaskHash = backendStats.evidenceMaskHash;
		stats_.evidenceOutputHash = backendStats.evidenceOutputHash;
		stats_.evidenceMarkedOutputHash = backendStats.evidenceMarkedOutputHash;
		stats_.evidenceWaitMicroseconds = backendStats.evidenceWaitMicroseconds;
		stats_.evidenceCaptures = backendStats.evidenceCaptures;
		stats_.evidenceCaptureFailures = backendStats.evidenceCaptureFailures;
		if (init != BackendEvalStatus::Success)
		{
			++stats_.fallbacks;
			if (init == BackendEvalStatus::DeviceRemoved)
			{
				++stats_.deviceRemovedStatuses;
				recovery_.DeviceRemoved();
			}
			if (init == BackendEvalStatus::Unsupported)
				backendPermanentlyUnsupported_ = true;
			else if (init == BackendEvalStatus::RecoverableFailure)
			{
				recovery_.RecordTransientFailure(frame.frameId, MonotonicMilliseconds());
				stats_.holdEntries = recovery_.HoldEntries();
			}
			return init == BackendEvalStatus::DeviceRemoved ? SubmitStatus::DeviceRemoved
				: init == BackendEvalStatus::RecoverableFailure ? SubmitStatus::RecoverableFailure
				: SubmitStatus::Unsupported;
		}
		backendInitialized_ = true;
	}
	if (frame.resetHistory)
		backend_->ResetHistory();
	const auto result = backend_->Evaluate(frame);
	const auto backendStats = backend_->GetStats();
	stats_.createFailures = backendStats.createFailures;
	stats_.evaluateFailures = backendStats.evaluateFailures;
	stats_.lastNgxResult = backendStats.lastResult;
	stats_.lastExceptionCode = backendStats.lastExceptionCode;
	stats_.compatibilityRebuilds = backendStats.compatibilityRebuilds;
	stats_.compatibilityRebuildAttempts = backendStats.compatibilityRebuildAttempts;
	stats_.compatibilityRebuildFailures = backendStats.compatibilityRebuildFailures;
	stats_.compatibilityRebuildReason = backendStats.compatibilityRebuildReason;
	stats_.dlss5ContractEvaluated = backendStats.dlss5ContractEvaluated;
	stats_.dlss5Route = backendStats.dlss5Route == Dlss5HookRoute::None
		&& config_.mode == NeuralMode::Dlss5Experimental
		? config_.dlss5Route : backendStats.dlss5Route;
	stats_.dlss5Readiness = backendStats.dlss5Readiness;
	stats_.dlss5Components = backendStats.dlss5Components;
	stats_.evidenceFrameId = backendStats.evidenceFrameId;
	stats_.evidenceInputHash = backendStats.evidenceInputHash;
	stats_.evidenceDepthHash = backendStats.evidenceDepthHash;
	stats_.evidenceMotionHash = backendStats.evidenceMotionHash;
	stats_.evidenceMaskHash = backendStats.evidenceMaskHash;
	stats_.evidenceOutputHash = backendStats.evidenceOutputHash;
	stats_.evidenceMarkedOutputHash = backendStats.evidenceMarkedOutputHash;
	stats_.evidenceWaitMicroseconds = backendStats.evidenceWaitMicroseconds;
	stats_.evidenceCaptures = backendStats.evidenceCaptures;
	stats_.evidenceCaptureFailures = backendStats.evidenceCaptureFailures;
	if (result == BackendEvalStatus::Success)
	{
		output_ = backend_->GetOutput();
		history_.Evaluated(frame.frameId);
		hasEvaluated_ = true;
		recovery_.RecordSuccess(frame.frameId);
		++stats_.submissions;
		return SubmitStatus::Submitted;
	}
	++stats_.fallbacks;
	if (result == BackendEvalStatus::Busy)
	{
		++stats_.busySkips;
		recovery_.RecordTransientFailure(frame.frameId, MonotonicMilliseconds());
		stats_.holdEntries = recovery_.HoldEntries();
		return SubmitStatus::Busy;
	}
	if (result == BackendEvalStatus::RecoverableFailure)
	{
		recovery_.RecordTransientFailure(frame.frameId, MonotonicMilliseconds());
		stats_.holdEntries = recovery_.HoldEntries();
		return SubmitStatus::RecoverableFailure;
	}
	if (result == BackendEvalStatus::DeviceRemoved)
	{
		++stats_.deviceRemovedStatuses;
		recovery_.DeviceRemoved();
	}
	return result == BackendEvalStatus::DeviceRemoved ? SubmitStatus::DeviceRemoved
		: SubmitStatus::Unsupported;
}

void NeuralStage::Shutdown() noexcept
{
	if (backend_) backend_->Shutdown();
	backend_.reset();
	output_ = {};
	history_ = {};
	recovery_ = {};
	lastHistoryGeneration_ = 0;
	lastSource_ = FrameSource::Geometry;
	hasFrame_ = false;
	hasEvaluated_ = false;
	recreateRequested_ = false;
	backendRenderWidth_ = backendRenderHeight_ = 0;
	backendInitialized_ = false;
	backendPermanentlyUnsupported_ = false;
}

} // namespace flycast::rend::neural
