// SPDX-License-Identifier: GPL-2.0-or-later
#include "live_status.h"

#include <mutex>
#include <iomanip>
#include <sstream>
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

const char *DlssPresetName(int preset) noexcept
{
	return preset == 10 ? "J" : preset == 11 ? "K" : "Auto";
}

std::string FormatLiveStatusOverlay(const LiveStatus& status, float fps)
{
	std::ostringstream text;
	text.imbue(std::locale::classic());
	const char *profileName = status.qualityProfile == 1 ? "Enhanced"
		: status.qualityProfile == 2 ? "Photoreal"
		: status.qualityProfile == 3 ? "Uncanny" : "Faithful";
	const char *apiName = status.api == Api::D3D12 ? "D3D11On12" : "D3D11";
	text << "Neural: " << NeuralModeName(status.mode) << " | " << profileName
		<< " | Preset " << DlssPresetName(status.dlssPreset);
	if (status.rendererAvailable)
		text << " | " << apiName;
	text << '\n';
	if (!status.rendererAvailable)
		text << "Renderer unavailable";
	else
	{
		if (status.mode == NeuralMode::Dlss5Experimental)
			text << "Public contract: " << SubmitStatusName(status.lastSubmit)
				<< " | External unverified";
		else
			text << SubmitStatusName(status.lastSubmit);
		if (status.conservativeBypass)
			text << " | 2D bypass";
		else if (status.overlayProtection)
			text << " | HUD protected";
		text << " | " << status.renderWidth << 'x' << status.renderHeight << " -> "
			<< status.outputWidth << 'x' << status.outputHeight;
		if (status.rasterJitterApplied)
			text << " | J " << std::fixed << std::setprecision(3)
				<< status.rasterJitterX << ',' << status.rasterJitterY;
	}
	text << '\n';
	if (fps >= 0.f && fps < 9999.f)
		text << std::fixed << std::setprecision(1) << "FPS " << fps << " | Frame "
			<< (fps > 0.f ? 1000.f / fps : 0.f) << " ms";
	else
		text << "FPS unavailable";
	text << '\n' << "Accepted " << status.stage.submissions << " | Busy "
		<< status.stage.busySkips << " | Fallback " << status.stage.fallbacks
		<< " | Drops n/a";
	return text.str();
}

} // namespace flycast::rend::neural
