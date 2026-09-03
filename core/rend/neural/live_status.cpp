// SPDX-License-Identifier: GPL-2.0-or-later
#include "live_status.h"

#include <mutex>
#include <utility>

namespace flycast::rend::neural {
namespace {

std::mutex statusMutex;
LiveStatus liveStatus;
std::uint64_t nextGeneration = 1;

} // namespace

void PublishLiveStatus(LiveStatus status)
{
	std::lock_guard<std::mutex> lock(statusMutex);
	status.generation = nextGeneration++;
	liveStatus = std::move(status);
}

LiveStatus GetLiveStatus()
{
	std::lock_guard<std::mutex> lock(statusMutex);
	return liveStatus;
}

void ResetLiveStatus()
{
	PublishLiveStatus({});
}

const char *NeuralModeName(NeuralMode mode) noexcept
{
	switch (mode)
	{
	case NeuralMode::Off: return "Off";
	case NeuralMode::Passthrough: return "Passthrough";
	case NeuralMode::Dlaa: return "DLAA";
	case NeuralMode::DlaaHook: return "DLAA Hook-Compatible";
	case NeuralMode::SrQuality: return "DLSS SR Quality";
	case NeuralMode::SrBalanced: return "DLSS SR Balanced";
	case NeuralMode::SrPerformance: return "DLSS SR Performance";
	case NeuralMode::SrUltraPerformance: return "DLSS SR Ultra Performance";
	case NeuralMode::Dlss5Experimental: return "DLSS 5 Experimental";
	default: return "Unknown";
	}
}

const char *SubmitStatusName(SubmitStatus status) noexcept
{
	switch (status)
	{
	case SubmitStatus::Submitted: return "Submitted";
	case SubmitStatus::Busy: return "Busy";
	case SubmitStatus::Holding: return "Fallback hold";
	case SubmitStatus::Unsupported: return "Unsupported";
	case SubmitStatus::RecoverableFailure: return "Recoverable failure";
	case SubmitStatus::Disabled: return "Disabled";
	case SubmitStatus::DeviceRemoved: return "Device removed";
	default: return "Unknown";
	}
}

const char *ApiName(Api api) noexcept
{
	return api == Api::D3D12 ? "D3D11On12 / D3D12" : "D3D11";
}

} // namespace flycast::rend::neural
