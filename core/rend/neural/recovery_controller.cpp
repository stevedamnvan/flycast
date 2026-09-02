// SPDX-License-Identifier: GPL-2.0-or-later
#include "recovery_controller.h"

#include <algorithm>

namespace flycast::rend::neural {

RecoveryController::RecoveryController() noexcept
{
	ClearFailures();
}

void RecoveryController::ClearFailures() noexcept
{
	failureFrames_.fill(EmptyFrame);
	nextFailure_ = 0;
}

void RecoveryController::SetReady() noexcept
{
	ClearFailures();
	state_ = RecoveryState::Ready;
	holdPresents_ = 0;
	resumeReset_ = false;
}

void RecoveryController::RecordSuccess(std::uint64_t) noexcept
{
	if (state_ == RecoveryState::Uninitialized)
		state_ = RecoveryState::Ready;
}

std::uint32_t RecoveryController::FailuresInWindow(std::uint64_t frameId) const noexcept
{
	const std::uint64_t first = frameId >= 59 ? frameId - 59 : 0;
	return static_cast<std::uint32_t>(std::count_if(failureFrames_.begin(), failureFrames_.end(),
		[first, frameId](std::uint64_t failed) {
			return failed != EmptyFrame && failed >= first && failed <= frameId;
		}));
}

void RecoveryController::RecordTransientFailure(std::uint64_t frameId,
	std::uint64_t monotonicMs) noexcept
{
	if (state_ == RecoveryState::FallbackHold || state_ == RecoveryState::DisabledForSession ||
		state_ == RecoveryState::DeviceRemoved)
		return;
	failureFrames_[nextFailure_] = frameId;
	nextFailure_ = (nextFailure_ + 1) % static_cast<std::uint32_t>(failureFrames_.size());
	if (FailuresInWindow(frameId) >= 3)
	{
		state_ = RecoveryState::FallbackHold;
		holdStartMs_ = monotonicMs;
		holdPresents_ = 0;
		++holdEntries_;
	}
}

void RecoveryController::OnHostPresent() noexcept
{
	if (state_ == RecoveryState::FallbackHold && holdPresents_ != ~std::uint32_t{0})
		++holdPresents_;
}

bool RecoveryController::CanEvaluate(std::uint64_t monotonicMs) noexcept
{
	if (state_ == RecoveryState::Ready || state_ == RecoveryState::Uninitialized)
		return true;
	if (state_ != RecoveryState::FallbackHold)
		return false;
	if (monotonicMs - holdStartMs_ < 1000 || holdPresents_ < 60)
		return false;
	ClearFailures();
	state_ = RecoveryState::Ready;
	resumeReset_ = true;
	holdPresents_ = 0;
	return true;
}

bool RecoveryController::ConsumeResumeReset() noexcept
{
	const bool value = resumeReset_;
	resumeReset_ = false;
	return value;
}

} // namespace flycast::rend::neural
