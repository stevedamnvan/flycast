// SPDX-License-Identifier: GPL-2.0-or-later
#include "performance_tracker.h"
#include "version.h"

#include <dxgi1_4.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <locale>
#include <utility>

namespace flycast::rend::neural {
namespace {

double Percentile(std::vector<double> values, double fraction)
{
	if (values.empty()) return 0.;
	std::sort(values.begin(), values.end());
	const auto index = static_cast<std::size_t>((values.size() - 1) * fraction + .5);
	return values[(std::min)(index, values.size() - 1)];
}

std::string Json(const std::string& value)
{
	std::string result;
	for (const char c : value)
	{
		if (c == '\\' || c == '"') result.push_back('\\');
		if (c == '\n') result += "\\n";
		else if (static_cast<unsigned char>(c) >= 0x20) result.push_back(c);
	}
	return result;
}

std::pair<std::uint64_t, std::uint64_t> QueryVram(ID3D11Device *device)
{
	if (!device) return {};
	ComPtr<IDXGIDevice> dxgiDevice;
	ComPtr<IDXGIAdapter> adapter;
	ComPtr<IDXGIAdapter3> adapter3;
	DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
	if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice),
		reinterpret_cast<void **>(&dxgiDevice.get())))
		|| FAILED(dxgiDevice->GetAdapter(&adapter.get()))
		|| FAILED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),
			reinterpret_cast<void **>(&adapter3.get())))
		|| FAILED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory)))
		return {};
	return {memory.CurrentUsage, memory.Budget};
}

} // namespace

void PerformanceTracker::Reset()
{
	for (auto& slot : ring_)
	{
		slot.disjoint.reset();
		for (auto& point : slot.points) point.reset();
		slot = {};
	}
	device_.reset();
	activeSlot_ = RingSize;
	lastEndedSlot_ = RingSize;
	nextSequence_ = 1;
	samples_.clear();
	stageStats_ = {};
	ringBusy_ = 0;
	lastPresent_ = {};
	lastPresentIntervalMs_ = 0.;
	warmupRemaining_ = 0;
	initialVramUsage_ = 0;
	written_ = false;
}

bool PerformanceTracker::CreateQueries(ID3D11Device *device)
{
	D3D11_QUERY_DESC disjointDesc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
	D3D11_QUERY_DESC timestampDesc{D3D11_QUERY_TIMESTAMP, 0};
	for (auto& slot : ring_)
	{
		if (FAILED(device->CreateQuery(&disjointDesc, &slot.disjoint.get()))) return false;
		for (auto& point : slot.points)
			if (FAILED(device->CreateQuery(&timestampDesc, &point.get()))) return false;
	}
	return true;
}

void PerformanceTracker::Configure(ID3D11Device *device,
	const std::filesystem::path& root, std::uint32_t warmupFrames,
	std::uint32_t sampleFrames, std::string gameId, std::string api,
	std::string renderer, int neuralMode, int failureInjection,
	std::uint32_t failureInjectionCount, std::uint32_t failureInjectionAfter)
{
	sampleFrames = (std::min)(sampleFrames, 10000u);
	if (root == root_ && warmupFrames == warmupFrames_
		&& sampleFrames == targetSamples_ && gameId == gameId_
		&& api == api_ && renderer == renderer_ && neuralMode == neuralMode_
		&& failureInjection == failureInjection_
		&& failureInjectionCount == failureInjectionCount_
		&& failureInjectionAfter == failureInjectionAfter_)
		return;
	Reset();
	root_ = root;
	warmupFrames_ = warmupFrames;
	warmupRemaining_ = warmupFrames;
	targetSamples_ = sampleFrames;
	gameId_ = std::move(gameId);
	api_ = std::move(api);
	renderer_ = std::move(renderer);
	neuralMode_ = neuralMode;
	failureInjection_ = failureInjection;
	failureInjectionCount_ = failureInjectionCount;
	failureInjectionAfter_ = failureInjectionAfter;
	if (root_.empty() || targetSamples_ == 0 || !device) return;
	device->AddRef();
	device_.reset(device);
	if (!CreateQueries(device))
	{
		Reset();
		root_.clear();
		targetSamples_ = 0;
	}
}

void PerformanceTracker::ResolveAvailable(ID3D11DeviceContext *context)
{
	for (auto& slot : ring_)
	{
		if (!slot.pending) continue;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
		if (context->GetData(slot.disjoint, &disjoint, sizeof(disjoint),
			D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
			continue;
		std::array<UINT64, PointCount> timestamps{};
		bool ready = true;
		for (std::size_t i = 0; i < PointCount; ++i)
		{
			if (!slot.marked[i]) continue;
			if (context->GetData(slot.points[i], &timestamps[i], sizeof(timestamps[i]),
				D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
			{
				ready = false;
				break;
			}
		}
		if (!ready) continue;
		slot.pending = false;
		if (disjoint.Disjoint || disjoint.Frequency == 0) continue;
		auto duration = [&](GpuTimingPoint begin, GpuTimingPoint end) {
			const auto b = static_cast<std::size_t>(begin);
			const auto e = static_cast<std::size_t>(end);
			if (!slot.marked[b] || !slot.marked[e] || timestamps[e] < timestamps[b]) return 0.;
			return static_cast<double>(timestamps[e] - timestamps[b]) * 1000.
				/ static_cast<double>(disjoint.Frequency);
		};
		Sample sample;
		sample.pvrMs = duration(GpuTimingPoint::PvrBegin, GpuTimingPoint::PvrEnd);
		sample.guidanceMs = duration(GpuTimingPoint::GuidanceBegin, GpuTimingPoint::GuidanceEnd);
		sample.evaluateMs = duration(GpuTimingPoint::EvaluateBegin, GpuTimingPoint::EvaluateEnd);
		sample.compositeMs = duration(GpuTimingPoint::CompositeBegin, GpuTimingPoint::CompositeEnd);
		sample.totalGpuMs = duration(GpuTimingPoint::PvrBegin, GpuTimingPoint::CompositeEnd);
		sample.presentIntervalMs = slot.presentIntervalMs;
		sample.presented = slot.presented;
		sample.sequence = slot.sequence;
		sample.sourceFrameId = slot.sourceFrameId;
		sample.acceptedFrameId = slot.acceptedFrameId;
		sample.outputFrameId = slot.outputFrameId;
		sample.rendererResourceObjects = slot.rendererResourceObjects;
		sample.backendResourceObjects = slot.backendResourceObjects;
		if (samples_.size() < targetSamples_) samples_.push_back(sample);
	}
	if (samples_.size() >= targetSamples_ && !written_) WriteReport();
}

void PerformanceTracker::BeginFrame(ID3D11DeviceContext *context)
{
	activeSlot_ = RingSize;
	if (!Enabled() || !context) return;
	ResolveAvailable(context);
	if (!Enabled()) return;
	if (warmupRemaining_ != 0)
	{
		--warmupRemaining_;
		return;
	}
	if (initialVramUsage_ == 0)
		initialVramUsage_ = QueryVram(device_).first;
	for (std::size_t i = 0; i < RingSize; ++i)
	{
		if (ring_[i].pending) continue;
		activeSlot_ = i;
		auto& slot = ring_[i];
		slot.marked.fill(false);
		slot.presented = false;
		slot.sequence = nextSequence_++;
		slot.sourceFrameId = 0;
		slot.acceptedFrameId = 0;
		slot.outputFrameId = 0;
		slot.presentIntervalMs = 0.;
		context->Begin(slot.disjoint);
		Mark(context, GpuTimingPoint::PvrBegin);
		return;
	}
	++ringBusy_;
}

void PerformanceTracker::RecordEvaluation(std::uint64_t frameId, bool accepted) noexcept
{
	if (activeSlot_ < RingSize && accepted)
		ring_[activeSlot_].acceptedFrameId = frameId;
}

void PerformanceTracker::StagePresentation(std::uint64_t sourceFrameId,
	std::uint64_t outputFrameId) noexcept
{
	if (activeSlot_ >= RingSize) return;
	ring_[activeSlot_].sourceFrameId = sourceFrameId;
	ring_[activeSlot_].outputFrameId = outputFrameId;
}

void PerformanceTracker::Mark(ID3D11DeviceContext *context, GpuTimingPoint point)
{
	if (activeSlot_ >= RingSize || !context) return;
	const auto index = static_cast<std::size_t>(point);
	if (index >= PointCount) return;
	context->End(ring_[activeSlot_].points[index]);
	ring_[activeSlot_].marked[index] = true;
}

void PerformanceTracker::EndFrame(ID3D11DeviceContext *context, const StageStats& stats,
	std::uint32_t rendererResourceObjects)
{
	stageStats_ = stats;
	if (activeSlot_ >= RingSize || !context) return;
	ring_[activeSlot_].rendererResourceObjects = rendererResourceObjects;
	ring_[activeSlot_].backendResourceObjects = stats.backendResourceObjects;
	context->End(ring_[activeSlot_].disjoint);
	ring_[activeSlot_].pending = true;
	lastEndedSlot_ = activeSlot_;
	activeSlot_ = RingSize;
}

void PerformanceTracker::RecordPresent() noexcept
{
	const auto now = std::chrono::steady_clock::now();
	if (lastPresent_ != std::chrono::steady_clock::time_point{})
		lastPresentIntervalMs_ = std::chrono::duration<double, std::milli>(now - lastPresent_).count();
	lastPresent_ = now;
	if (lastEndedSlot_ < RingSize && ring_[lastEndedSlot_].pending)
	{
		ring_[lastEndedSlot_].presented = true;
		ring_[lastEndedSlot_].presentIntervalMs = lastPresentIntervalMs_;
		lastEndedSlot_ = RingSize;
	}
}

void PerformanceTracker::WriteReport()
{
	std::error_code ec;
	std::filesystem::create_directories(root_, ec);
	if (ec) return;
	std::sort(samples_.begin(), samples_.end(), [](const Sample& a, const Sample& b) {
		return a.sequence < b.sequence;
	});
	std::vector<double> pvr, guidance, evaluate, composite, total, present;
	PresentationCadence cadence;
	for (const auto& sample : samples_)
	{
		pvr.push_back(sample.pvrMs); guidance.push_back(sample.guidanceMs);
		evaluate.push_back(sample.evaluateMs); composite.push_back(sample.compositeMs);
		total.push_back(sample.totalGpuMs);
		if (sample.presentIntervalMs > 0.) present.push_back(sample.presentIntervalMs);
		cadence.Observe(sample.sourceFrameId, sample.acceptedFrameId,
			sample.outputFrameId, sample.presented);
	}
	const auto& cadenceStats = cadence.Stats();
	std::uint32_t initialResourceObjects = 0;
	std::uint32_t finalResourceObjects = 0;
	std::uint32_t minimumResourceObjects = 0;
	std::uint32_t maximumResourceObjects = 0;
	if (!samples_.empty())
	{
		auto totalObjects = [](const Sample& sample) {
			return sample.rendererResourceObjects + sample.backendResourceObjects;
		};
		initialResourceObjects = totalObjects(samples_.front());
		finalResourceObjects = totalObjects(samples_.back());
		minimumResourceObjects = initialResourceObjects;
		maximumResourceObjects = initialResourceObjects;
		for (const auto& sample : samples_)
		{
			const auto total = totalObjects(sample);
			minimumResourceObjects = (std::min)(minimumResourceObjects, total);
			maximumResourceObjects = (std::max)(maximumResourceObjects, total);
		}
	}
	const auto resourceGrowth = static_cast<std::int64_t>(finalResourceObjects)
		- static_cast<std::int64_t>(initialResourceObjects);
	const auto [usage, budget] = QueryVram(device_);
	const auto growth = static_cast<std::int64_t>(usage)
		- static_cast<std::int64_t>(initialVramUsage_);
	const bool evaluateAvailable = api_ == "d3d11" && stageStats_.submissions != 0;
	const char *evaluateScope = api_ != "d3d11"
		? "unavailable-d3d11on12-cross-queue"
		: neuralMode_ == 0
			? "not-applicable-neural-off"
			: stageStats_.submissions == 0
				? "not-observed-no-accepted-submission"
				: "native-d3d11-context";
	std::ofstream report(root_ / "performance.json");
	report.imbue(std::locale::classic());
	report << std::fixed << std::setprecision(6)
		<< "{\n  \"schema\": 2,\n  \"git_sha\": \"" << GIT_HASH
		<< "\",\n  \"game_id\": \"" << Json(gameId_)
		<< "\",\n  \"api\": \"" << Json(api_)
		<< "\",\n  \"renderer\": \"" << Json(renderer_)
		<< "\",\n  \"neural_mode\": " << neuralMode_
		<< ",\n  \"failure_injection\": " << failureInjection_
		<< ",\n  \"failure_injection_count\": " << failureInjectionCount_
		<< ",\n  \"failure_injection_after_accepted\": " << failureInjectionAfter_
		<< ",\n  \"synchronous_capture_enabled\": false"
		<< ",\n  \"query_policy\": \"D3D11_ASYNC_GETDATA_DONOTFLUSH\""
		<< ",\n  \"stage_evaluate_gpu_available\": " << (evaluateAvailable ? "true" : "false")
		<< ",\n  \"stage_evaluate_scope\": \"" << evaluateScope << "\""
		<< ",\n  \"sample_count\": " << samples_.size()
		<< ",\n  \"ring_busy_count\": " << ringBusy_
		<< ",\n  \"presentation_cadence\": {\"observed_presents\": "
		<< cadenceStats.observedPresents
		<< ", \"missing_presents\": " << cadenceStats.missingPresents
		<< ", \"accepted_evaluations\": " << cadenceStats.acceptedEvaluations
		<< ", \"neural_presents\": " << cadenceStats.neuralPresents
		<< ", \"native_presents\": " << cadenceStats.nativePresents
		<< ", \"accepted_not_presented\": " << cadenceStats.acceptedNotPresented
		<< ", \"frame_identity_mismatches\": " << cadenceStats.frameIdentityMismatches
		<< ", \"source_frame_repeats\": " << cadenceStats.sourceFrameRepeats
		<< ", \"source_frame_gaps\": " << cadenceStats.sourceFrameGaps
		<< ", \"output_frame_repeats\": " << cadenceStats.outputFrameRepeats
		<< ", \"native_neural_alternations\": " << cadenceStats.nativeNeuralAlternations
		<< ", \"latency_samples\": " << cadenceStats.latencySamples
		<< ", \"latency_frames_mean\": "
		<< (cadenceStats.latencySamples == 0 ? 0. :
			static_cast<double>(cadenceStats.latencyFramesTotal) /
			static_cast<double>(cadenceStats.latencySamples))
		<< ", \"latency_frames_max\": " << cadenceStats.latencyFramesMax << "}"
		<< ",\n  \"stage_counts\": {\"submissions\": " << stageStats_.submissions
		<< ", \"busy\": " << stageStats_.busySkips << ", \"fallbacks\": "
		<< stageStats_.fallbacks << ", \"resets\": " << stageStats_.resets
		<< ", \"hold_entries\": " << stageStats_.holdEntries
		<< ", \"create_failures\": " << stageStats_.createFailures
		<< ", \"evaluate_failures\": " << stageStats_.evaluateFailures
		<< ", \"device_removed\": " << stageStats_.deviceRemovedStatuses
		<< ", \"runtime_unavailable\": " << stageStats_.runtimeUnavailableStatuses << "}"
		<< ",\n  \"vram\": {\"initial_usage_bytes\": " << initialVramUsage_
		<< ", \"final_usage_bytes\": " << usage << ", \"growth_bytes\": " << growth
		<< ", \"budget_bytes\": " << budget << "}"
		<< ",\n  \"resource_objects\": {\"scope\": \"flycast-owned-neural-gpu-objects\""
		<< ", \"initial\": " << initialResourceObjects
		<< ", \"minimum\": " << minimumResourceObjects
		<< ", \"maximum\": " << maximumResourceObjects
		<< ", \"final\": " << finalResourceObjects
		<< ", \"growth\": " << resourceGrowth
		<< ", \"renderer_final\": " << samples_.back().rendererResourceObjects
		<< ", \"backend_final\": " << samples_.back().backendResourceObjects << "}"
		<< ",\n  \"percentiles_ms\": {";
	auto summary = [&](const char *name, const std::vector<double>& values, bool first) {
		report << (first ? "\n    " : ",\n    ") << '"' << name << "\": {\"p50\": "
			<< Percentile(values, .50) << ", \"p95\": " << Percentile(values, .95)
			<< ", \"p99\": " << Percentile(values, .99) << '}';
	};
	summary("base_pvr_gpu", pvr, true);
	summary("guidance_gpu", guidance, false);
	if (evaluateAvailable)
		summary("stage_evaluate_gpu", evaluate, false);
	else
		report << ",\n    \"stage_evaluate_gpu\": null";
	summary("overlay_and_present_blit_gpu", composite, false);
	summary("frame_gpu_timestamp_span", total, false);
	summary("present_interval_cpu", present, false);
	report << "\n  },\n  \"samples_ms\": [";
	for (std::size_t i = 0; i < samples_.size(); ++i)
	{
		const auto& s = samples_[i];
		if (i != 0) report << ',';
		report << "\n    {\"pvr\": " << s.pvrMs << ", \"guidance\": " << s.guidanceMs
			<< ", \"evaluate\": ";
		if (evaluateAvailable) report << s.evaluateMs;
		else report << "null";
		report << ", \"composite\": " << s.compositeMs << ", \"total\": " << s.totalGpuMs
			<< ", \"present_interval\": " << s.presentIntervalMs
			<< ", \"presented\": " << (s.presented ? "true" : "false")
			<< ", \"source_frame_id\": " << s.sourceFrameId
			<< ", \"accepted_frame_id\": " << s.acceptedFrameId
			<< ", \"output_frame_id\": " << s.outputFrameId << '}';
	}
	report << "\n  ]\n}\n";
	if (!report) return;
	std::ofstream complete(root_ / "performance-complete.json");
	complete.imbue(std::locale::classic());
	complete << "{\n  \"schema\": 2,\n  \"git_sha\": \"" << GIT_HASH
		<< "\",\n  \"sample_count\": " << samples_.size()
		<< ",\n  \"status\": \"complete\"\n}\n";
	if (complete) written_ = true;
}

} // namespace flycast::rend::neural
