// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

namespace flycast::rend::neural {

struct Dlss5HookComponents
{
	bool reshadeHostLoaded = false;
	bool interceptorLoaded = false;
	bool neuralRuntimeLoaded = false;
	bool dlssRuntimeLoaded = false;
};

enum class Dlss5HookReadiness
{
	Disabled,
	MissingRoute,
	MissingComponents,
	ComponentsPresent,
	ContractEvaluated,
};

enum class Dlss5HookRoute : std::uint8_t
{
	None,
	D3D11External,
	D3D11On12,
};

enum class Dlss5RebuildReason : std::uint8_t
{
	None,
	ComponentsReadyTransition,
	RetryAfterCreateFailure,
};

struct Dlss5HookAssessment
{
	Dlss5HookReadiness readiness = Dlss5HookReadiness::Disabled;
	bool componentsPresent = false;
	const char *message = "DLSS 5 hook experiment disabled";
};

Dlss5HookComponents DetectDlss5HookComponents() noexcept;
Dlss5HookAssessment AssessDlss5Hook(bool requested, Dlss5HookRoute route,
	const Dlss5HookComponents& components, bool contractEvaluated) noexcept;
const char *Dlss5HookRouteName(Dlss5HookRoute route) noexcept;
const char *Dlss5HookReadinessName(Dlss5HookReadiness readiness) noexcept;
const char *Dlss5RebuildReasonName(Dlss5RebuildReason reason) noexcept;

// Arms only on a not-ready -> ready transition, then waits for a configurable
// number of successful public-contract evaluations. Feature recreation retries
// are bounded independently from normal NGX creation.
class Dlss5CompatibilityRebuildPolicy final
{
public:
	void Configure(std::uint32_t graceEvaluations, std::uint32_t maxAttempts) noexcept;
	void Reset() noexcept;
	void Observe(Dlss5HookReadiness readiness, std::uint64_t successfulEvaluations) noexcept;
	bool ConsumeReleaseRequest() noexcept;
	bool BeginCreateAttempt() noexcept;
	void CompleteCreateAttempt(bool success) noexcept;

	bool RecreatePending() const noexcept { return recreatePending_; }
	bool RetryAvailable() const noexcept { return recreatePending_ && attempts_ < maxAttempts_; }
	std::uint64_t Attempts() const noexcept { return attempts_; }
	std::uint64_t SuccessfulRebuilds() const noexcept { return successfulRebuilds_; }
	std::uint64_t Failures() const noexcept { return failures_; }
	Dlss5RebuildReason LastReason() const noexcept { return lastReason_; }

private:
	std::uint32_t graceEvaluations_ = 300;
	std::uint32_t maxAttempts_ = 2;
	std::uint64_t readySinceEvaluation_ = 0;
	std::uint64_t attempts_ = 0;
	std::uint64_t successfulRebuilds_ = 0;
	std::uint64_t failures_ = 0;
	Dlss5RebuildReason lastReason_ = Dlss5RebuildReason::None;
	bool ready_ = false;
	bool armed_ = false;
	bool releaseRequested_ = false;
	bool recreatePending_ = false;
};

} // namespace flycast::rend::neural
