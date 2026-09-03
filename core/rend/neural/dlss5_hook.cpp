// SPDX-License-Identifier: GPL-2.0-or-later
#include "dlss5_hook.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace flycast::rend::neural {

Dlss5HookComponents DetectDlss5HookComponents() noexcept
{
#ifdef _WIN32
	auto moduleExports = [](const wchar_t *moduleName, const char *exportName) noexcept {
		const HMODULE module = GetModuleHandleW(moduleName);
		return module != nullptr && GetProcAddress(module, exportName) != nullptr;
	};
	return {
		moduleExports(L"dxgi.dll", "ReShadeRegisterAddon")
			|| moduleExports(L"d3d11.dll", "ReShadeRegisterAddon")
			|| moduleExports(L"d3d12.dll", "ReShadeRegisterAddon")
			|| moduleExports(L"ReShade64.dll", "ReShadeRegisterAddon"),
		GetModuleHandleW(L"renodx-dlss5.addon64") != nullptr,
		GetModuleHandleW(L"nvngx_dlssnr.dll") != nullptr,
		GetModuleHandleW(L"nvngx_dlss.dll") != nullptr,
	};
#else
	return {};
#endif
}

Dlss5HookAssessment AssessDlss5Hook(bool requested, Dlss5HookRoute route,
	const Dlss5HookComponents& components, bool contractEvaluated) noexcept
{
	if (!requested)
		return {};
	if (route == Dlss5HookRoute::None)
		return {Dlss5HookReadiness::MissingRoute, false,
			"DLSS 5 experiment has no selected external-consumer route"};
	if (!components.reshadeHostLoaded || !components.interceptorLoaded
		|| !components.neuralRuntimeLoaded || !components.dlssRuntimeLoaded)
		return {Dlss5HookReadiness::MissingComponents, false,
			"public NGX available; external DLSS 5 consumer components are incomplete"};
	if (!contractEvaluated)
		return {Dlss5HookReadiness::ComponentsPresent, true,
			"external DLSS 5 consumer components loaded; awaiting a public NGX evaluation"};
	return {Dlss5HookReadiness::ContractEvaluated, true,
		"public NGX contract evaluated with consumer components present; neural output remains unconfirmed"};
}

const char *Dlss5HookRouteName(Dlss5HookRoute route) noexcept
{
	switch (route)
	{
	case Dlss5HookRoute::D3D11External: return "d3d11-external-unclassified";
	case Dlss5HookRoute::D3D11On12: return "d3d11on12";
	default: return "none";
	}
}

const char *Dlss5HookReadinessName(Dlss5HookReadiness readiness) noexcept
{
	switch (readiness)
	{
	case Dlss5HookReadiness::MissingRoute: return "missing-route";
	case Dlss5HookReadiness::MissingComponents: return "missing-components";
	case Dlss5HookReadiness::ComponentsPresent: return "components-present";
	case Dlss5HookReadiness::ContractEvaluated: return "contract-evaluated";
	default: return "disabled";
	}
}

const char *Dlss5RebuildReasonName(Dlss5RebuildReason reason) noexcept
{
	switch (reason)
	{
	case Dlss5RebuildReason::ComponentsReadyTransition: return "components-ready-transition";
	case Dlss5RebuildReason::RetryAfterCreateFailure: return "retry-after-create-failure";
	default: return "none";
	}
}

void Dlss5CompatibilityRebuildPolicy::Configure(std::uint32_t graceEvaluations,
	std::uint32_t maxAttempts) noexcept
{
	graceEvaluations_ = graceEvaluations;
	maxAttempts_ = maxAttempts;
}

void Dlss5CompatibilityRebuildPolicy::Reset() noexcept
{
	readySinceEvaluation_ = 0;
	attempts_ = 0;
	successfulRebuilds_ = 0;
	failures_ = 0;
	lastReason_ = Dlss5RebuildReason::None;
	ready_ = false;
	armed_ = false;
	releaseRequested_ = false;
	recreatePending_ = false;
}

void Dlss5CompatibilityRebuildPolicy::Observe(Dlss5HookReadiness readiness,
	std::uint64_t successfulEvaluations) noexcept
{
	const bool ready = readiness == Dlss5HookReadiness::ComponentsPresent
		|| readiness == Dlss5HookReadiness::ContractEvaluated;
	if (!ready)
	{
		ready_ = false;
		armed_ = false;
		releaseRequested_ = false;
		return;
	}
	if (!ready_)
	{
		ready_ = true;
		armed_ = true;
		readySinceEvaluation_ = successfulEvaluations;
	}
	if (armed_ && !recreatePending_ && attempts_ < maxAttempts_
		&& successfulEvaluations - readySinceEvaluation_ >= graceEvaluations_)
	{
		releaseRequested_ = true;
		armed_ = false;
	}
}

bool Dlss5CompatibilityRebuildPolicy::ConsumeReleaseRequest() noexcept
{
	if (!releaseRequested_ || recreatePending_ || attempts_ >= maxAttempts_)
		return false;
	releaseRequested_ = false;
	recreatePending_ = true;
	lastReason_ = Dlss5RebuildReason::ComponentsReadyTransition;
	return true;
}

bool Dlss5CompatibilityRebuildPolicy::BeginCreateAttempt() noexcept
{
	if (!recreatePending_ || attempts_ >= maxAttempts_)
		return false;
	if (attempts_ != 0)
		lastReason_ = Dlss5RebuildReason::RetryAfterCreateFailure;
	++attempts_;
	return true;
}

void Dlss5CompatibilityRebuildPolicy::CompleteCreateAttempt(bool success) noexcept
{
	if (!recreatePending_)
		return;
	if (success)
	{
		++successfulRebuilds_;
		recreatePending_ = false;
		return;
	}
	++failures_;
	if (attempts_ >= maxAttempts_)
		recreatePending_ = false;
}

} // namespace flycast::rend::neural
