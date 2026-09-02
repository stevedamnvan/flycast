// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>

namespace flycast::rend::neural {

enum class RecoveryState : std::uint8_t {
	Uninitialized,
	Ready,
	FallbackHold,
	CreateBackoff,
	ReinitPending,
	DisabledForSession,
	DeviceRemoved,
};

class RecoveryController final {
public:
	RecoveryController() noexcept;
	void SetReady() noexcept;
	void RecordSuccess(std::uint64_t frameId) noexcept;
	void RecordTransientFailure(std::uint64_t frameId, std::uint64_t monotonicMs) noexcept;
	void OnHostPresent() noexcept;
	bool CanEvaluate(std::uint64_t monotonicMs) noexcept;
	bool ConsumeResumeReset() noexcept;
	void DisableForSession() noexcept { state_ = RecoveryState::DisabledForSession; }
	void DeviceRemoved() noexcept { state_ = RecoveryState::DeviceRemoved; }

	RecoveryState State() const noexcept { return state_; }
	std::uint64_t HoldEntries() const noexcept { return holdEntries_; }
	std::uint32_t FailuresInWindow(std::uint64_t frameId) const noexcept;

private:
	static constexpr std::uint64_t EmptyFrame = ~std::uint64_t{0};
	std::array<std::uint64_t, 60> failureFrames_{};
	std::uint32_t nextFailure_ = 0;
	RecoveryState state_ = RecoveryState::Uninitialized;
	std::uint64_t holdStartMs_ = 0;
	std::uint64_t holdEntries_ = 0;
	std::uint32_t holdPresents_ = 0;
	bool resumeReset_ = false;

	void ClearFailures() noexcept;
};

} // namespace flycast::rend::neural
