// SPDX-License-Identifier: GPL-2.0-or-later
#include "neural_backend.h"
#include "dlss5_hook.h"
#include "version.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>

#include "windows/comptr.h"

#include <array>
#include <cstdio>

namespace flycast::rend::neural {
namespace {

struct NgxCallResult
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_FAIL_PlatformError;
	std::uint32_t exceptionCode = 0;
};

#define NGX_SEH_CALL(body) \
	NgxCallResult call; \
	__try { call.result = (body); } \
	__except (EXCEPTION_EXECUTE_HANDLER) { call.exceptionCode = GetExceptionCode(); } \
	return call

NgxCallResult InitLeaf(const wchar_t *path, ID3D12Device *device,
	const NVSDK_NGX_FeatureCommonInfo *featureInfo) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D12_Init_with_ProjectID(
		"7d5f2a1c-3b8e-4c6a-9f0d-2e4b6c8a1d3f", NVSDK_NGX_ENGINE_TYPE_CUSTOM,
		GIT_VERSION, path, device, featureInfo, NVSDK_NGX_Version_API));
}

NgxCallResult CapabilityLeaf(NVSDK_NGX_Parameter **parameters, int *available,
	int *needsDriver, unsigned int *major, unsigned int *minor, int *featureInitResult) noexcept
{
	NgxCallResult call;
	__try
	{
		call.result = NVSDK_NGX_D3D12_GetCapabilityParameters(parameters);
		if (NVSDK_NGX_SUCCEED(call.result))
		{
			NVSDK_NGX_Parameter_GetI(*parameters, NVSDK_NGX_Parameter_SuperSampling_Available, available);
			NVSDK_NGX_Parameter_GetI(*parameters, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, needsDriver);
			NVSDK_NGX_Parameter_GetUI(*parameters, NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, major);
			NVSDK_NGX_Parameter_GetUI(*parameters, NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, minor);
			NVSDK_NGX_Parameter_GetI(*parameters, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,
				featureInitResult);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		call.exceptionCode = GetExceptionCode();
	}
	return call;
}

NgxCallResult OptimalSettingsLeaf(NVSDK_NGX_Parameter *parameters,
	unsigned int outputWidth, unsigned int outputHeight, NVSDK_NGX_PerfQuality_Value quality,
	unsigned int *optimalWidth, unsigned int *optimalHeight, unsigned int *maxWidth,
	unsigned int *maxHeight, unsigned int *minWidth, unsigned int *minHeight,
	float *sharpness) noexcept
{
	NGX_SEH_CALL(NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters, outputWidth, outputHeight, quality,
		optimalWidth, optimalHeight, maxWidth, maxHeight, minWidth, minHeight, sharpness));
}

NgxCallResult CreateLeaf(ID3D12GraphicsCommandList *list, NVSDK_NGX_Handle **handle,
	NVSDK_NGX_Parameter *parameters, NVSDK_NGX_DLSS_Create_Params *create) noexcept
{
	NGX_SEH_CALL(NGX_D3D12_CREATE_DLSS_EXT(list, 1, 1, handle, parameters, create));
}

NgxCallResult EvaluateLeaf(ID3D12GraphicsCommandList *list, NVSDK_NGX_Handle *handle,
	NVSDK_NGX_Parameter *parameters, NVSDK_NGX_D3D12_DLSS_Eval_Params *evaluate) noexcept
{
	NGX_SEH_CALL(NGX_D3D12_EVALUATE_DLSS_EXT(list, handle, parameters, evaluate));
}

NgxCallResult ReleaseLeaf(NVSDK_NGX_Handle *handle) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D12_ReleaseFeature(handle));
}

NgxCallResult DestroyParametersLeaf(NVSDK_NGX_Parameter *parameters) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D12_DestroyParameters(parameters));
}

NgxCallResult ShutdownLeaf(ID3D12Device *device) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D12_Shutdown1(device));
}

#undef NGX_SEH_CALL

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource *resource,
	D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	return barrier;
}

class NgxD3D12Backend final : public INeuralBackend
{
public:
	~NgxD3D12Backend() override { Shutdown(); }

	BackendEvalStatus Initialize(const StageConfig& config, void *device,
		void *context) noexcept override
	{
		Shutdown();
		config_ = config;
		rebuildPolicy_.Configure(config.dlss5RebuildGraceEvaluations,
			config.dlss5RebuildMaxAttempts);
		stats_.dlss5Route = config.dlss5Route;
		device_ = static_cast<ID3D12Device *>(device);
		queue_ = static_cast<ID3D12CommandQueue *>(context);
		if (!device_ || !queue_)
			return Unsupported("D3D12 device or direct queue is unavailable");
		if (config.outputWidth == 0 || config.outputHeight == 0)
			return Unsupported("neural output dimensions are zero");
		wchar_t appPath[MAX_PATH]{};
		if (GetTempPathW(static_cast<DWORD>(std::size(appPath)), appPath) == 0)
			return Unsupported("no writable NGX application-data path");
		wchar_t featurePath[32768]{};
		const DWORD pathLength = GetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH",
			featurePath, static_cast<DWORD>(std::size(featurePath)));
		const wchar_t *featurePaths[] = {featurePath};
		NVSDK_NGX_FeatureCommonInfo featureInfo{};
		if (pathLength > 0 && pathLength < std::size(featurePath))
		{
			featureInfo.PathListInfo.Path = featurePaths;
			featureInfo.PathListInfo.Length = 1;
		}
		auto call = InitLeaf(appPath, device_, featureInfo.PathListInfo.Length ? &featureInfo : nullptr);
		Record(call);
		if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
		{
			++stats_.createFailures;
			return Unsupported(call.exceptionCode ? "NGX D3D12 initialization raised an exception"
				: "NGX D3D12 initialization failed");
		}
		initialized_ = true;
		int available = 0;
		int needsDriver = 0;
		unsigned int driverMajor = 0;
		unsigned int driverMinor = 0;
		int featureInitResult = 0;
		call = CapabilityLeaf(&parameters_, &available, &needsDriver, &driverMajor, &driverMinor,
			&featureInitResult);
		Record(call);
		if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result) || !available)
		{
			++stats_.createFailures;
			if (needsDriver)
			{
				std::snprintf(reason_, sizeof(reason_), "NGX DLSS requires driver %u.%u or newer",
					driverMajor, driverMinor);
				return BackendEvalStatus::Unsupported;
			}
			if (call.exceptionCode)
				return Unsupported("NGX D3D12 capability query raised an exception");
			std::snprintf(reason_, sizeof(reason_),
				"NGX Super Sampling unavailable (availability=%d, driver-update=%d, feature-init=0x%08X)",
				available, needsDriver, static_cast<unsigned int>(featureInitResult));
			return BackendEvalStatus::Unsupported;
		}
		if (IsSr(config_.mode))
		{
			float sharpness = 0.f;
			call = OptimalSettingsLeaf(parameters_, config_.outputWidth, config_.outputHeight,
				Quality(config_.mode), &optimalWidth_, &optimalHeight_, &maxWidth_, &maxHeight_,
				&minWidth_, &minHeight_, &sharpness);
			Record(call);
			if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result)
				|| optimalWidth_ == 0 || optimalHeight_ == 0)
			{
				++stats_.createFailures;
				return Unsupported(call.exceptionCode ? "NGX D3D12 optimal-settings query raised an exception"
					: "NGX D3D12 optimal-settings query failed");
			}
		}
		if (!CreateResources())
		{
			++stats_.createFailures;
			return BackendEvalStatus::RecoverableFailure;
		}
		RefreshDlss5HookStatus();
		return BackendEvalStatus::Success;
	}

	BackendEvalStatus Evaluate(const NeuralFrame& frame) noexcept override
	{
		if (!initialized_ || !parameters_)
			return Unsupported("NGX D3D12 session is not initialized");
		if (!frame.color || !frame.depth || !frame.motion || !frame.mask)
			return Unsupported("neural frame is missing a required DLSS input texture");
		if (frame.color.api != TextureApi::D3D12 || frame.depth.api != TextureApi::D3D12
			|| frame.motion.api != TextureApi::D3D12 || frame.mask.api != TextureApi::D3D12)
			return Unsupported("neural frame texture API does not match D3D12 backend");
		const bool dlaa = IsDlaa(config_.mode);
		if (dlaa && (frame.renderWidth != config_.outputWidth || frame.renderHeight != config_.outputHeight))
			return Unsupported("DLAA requires equal render and output dimensions");
		if (IsSr(config_.mode)
			&& (frame.renderWidth != optimalWidth_ || frame.renderHeight != optimalHeight_))
		{
			std::snprintf(reason_, sizeof(reason_),
				"SR input %ux%u does not match NGX optimal %ux%u for output %ux%u",
				frame.renderWidth, frame.renderHeight, optimalWidth_, optimalHeight_,
				config_.outputWidth, config_.outputHeight);
			return BackendEvalStatus::Unsupported;
		}

		RefreshDlss5HookStatus();
		if (rebuildPolicy_.ConsumeReleaseRequest())
			rebuildReleasePending_ = true;
		if (rebuildReleasePending_)
		{
			if (!AllSubmittedWorkComplete())
				return UnsupportedOrBusy("DLSS 5 compatibility rebuild awaiting asynchronous retirement");
			if (feature_)
			{
				const auto release = ReleaseLeaf(feature_);
				Record(release);
				if (release.exceptionCode != 0 || NVSDK_NGX_FAILED(release.result))
				{
					if (rebuildPolicy_.BeginCreateAttempt())
						rebuildPolicy_.CompleteCreateAttempt(false);
					SyncRebuildStats();
					if (!rebuildPolicy_.RetryAvailable())
					{
						compatibilityRebuildExhausted_ = true;
						rebuildReleasePending_ = false;
						return Unsupported("DLSS 5 compatibility release retry limit reached");
					}
					return DeviceOrFailure("DLSS 5 compatibility feature release failed; bounded retry pending");
				}
				feature_ = nullptr;
			}
			rebuildReleasePending_ = false;
			resetRequested_ = true;
		}
		if (compatibilityRebuildExhausted_)
			return Unsupported("DLSS 5 compatibility rebuild retry limit reached");

		const std::size_t slot = (outputSlot_ + 1) % output_.size();
		if (slotFence_[slot] != 0 && fence_->GetCompletedValue() < slotFence_[slot])
			return BackendEvalStatus::Busy;
		if (FAILED(allocator_[slot]->Reset()) || FAILED(list_[slot]->Reset(allocator_[slot], nullptr)))
			return DeviceOrFailure("D3D12 neural command-list reset failed");
		const D3D12_RESOURCE_STATES previousOutputState = outputState_[slot];
		if (previousOutputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			const auto barrier = Transition(output_[slot], previousOutputState,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			list_[slot]->ResourceBarrier(1, &barrier);
		}
		if (!feature_)
		{
			const bool compatibilityCreate = rebuildPolicy_.RecreatePending();
			if (compatibilityCreate && !rebuildPolicy_.BeginCreateAttempt())
				return Unsupported("DLSS 5 compatibility rebuild retry limit reached");
			NVSDK_NGX_DLSS_Create_Params create{};
			create.Feature.InWidth = frame.renderWidth;
			create.Feature.InHeight = frame.renderHeight;
			create.Feature.InTargetWidth = config_.outputWidth;
			create.Feature.InTargetHeight = config_.outputHeight;
			create.Feature.InPerfQualityValue = Quality(config_.mode);
			create.InFeatureCreateFlags = IsSr(config_.mode)
				? NVSDK_NGX_DLSS_Feature_Flags_MVLowRes
				: NVSDK_NGX_DLSS_Feature_Flags_None;
			create.InEnableOutputSubrects = false;
			const auto call = CreateLeaf(list_[slot], &feature_, parameters_, &create);
			Record(call);
			if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
			{
				++stats_.createFailures;
				if (compatibilityCreate)
				{
					rebuildPolicy_.CompleteCreateAttempt(false);
					SyncRebuildStats();
					Poison(slot);
					if (rebuildPolicy_.RetryAvailable())
						return DeviceOrFailure("DLSS 5 compatibility recreate failed; bounded retry pending");
					compatibilityRebuildExhausted_ = true;
					return Unsupported("DLSS 5 compatibility recreate retry limit reached");
				}
				Poison(slot);
				return Unsupported(call.exceptionCode ? "NGX D3D12 create raised an exception"
					: "NGX D3D12 feature creation failed");
			}
			if (compatibilityCreate)
			{
				rebuildPolicy_.CompleteCreateAttempt(true);
				SyncRebuildStats();
			}
		}
		NVSDK_NGX_D3D12_DLSS_Eval_Params evaluate{};
		evaluate.Feature.pInColor = static_cast<ID3D12Resource *>(frame.color.resource);
		evaluate.Feature.pInOutput = output_[slot];
		evaluate.Feature.InSharpness = 0.f;
		evaluate.pInDepth = static_cast<ID3D12Resource *>(frame.depth.resource);
		evaluate.pInMotionVectors = static_cast<ID3D12Resource *>(frame.motion.resource);
		evaluate.pInBiasCurrentColorMask = static_cast<ID3D12Resource *>(frame.mask.resource);
		evaluate.InJitterOffsetX = config_.hookCompatibility ? 0.f : frame.jitterX;
		evaluate.InJitterOffsetY = config_.hookCompatibility ? 0.f : frame.jitterY;
		evaluate.InRenderSubrectDimensions = {frame.renderWidth, frame.renderHeight};
		evaluate.InReset = resetRequested_ || frame.resetHistory ? 1 : 0;
		evaluate.InMVScaleX = 1.f;
		evaluate.InMVScaleY = 1.f;
		evaluate.InPreExposure = 1.f;
		evaluate.InExposureScale = 1.f;
		const auto call = EvaluateLeaf(list_[slot], feature_, parameters_, &evaluate);
		Record(call);
		if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
		{
			++stats_.evaluateFailures;
			Poison(slot);
			return DeviceOrFailure(call.exceptionCode ? "NGX D3D12 evaluate raised an exception"
				: "NGX D3D12 evaluate failed");
		}
		const auto barrier = Transition(output_[slot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		list_[slot]->ResourceBarrier(1, &barrier);
		outputState_[slot] = static_cast<D3D12_RESOURCE_STATES>(
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		if (FAILED(list_[slot]->Close()))
		{
			outputState_[slot] = previousOutputState;
			Poison(slot);
			return DeviceOrFailure("D3D12 neural command-list close failed");
		}
		ID3D12CommandList *lists[] = {list_[slot]};
		queue_->ExecuteCommandLists(1, lists);
		const std::uint64_t value = nextFence_++;
		if (FAILED(queue_->Signal(fence_, value)))
			return DeviceOrFailure("D3D12 neural fence signal failed");
		slotFence_[slot] = value;
		outputSlot_ = slot;
		resetRequested_ = false;
		++successfulEvaluations_;
		if (config_.mode == NeuralMode::Dlss5Experimental)
			RefreshDlss5HookStatus(true);
		else
			reason_[0] = 0;
		return BackendEvalStatus::Success;
	}

	void ResetHistory() noexcept override { resetRequested_ = true; }
	BackendStats GetStats() const noexcept override { return stats_; }
	TextureRef GetOutput() const noexcept override
	{
		if (!output_[outputSlot_]) return {};
		return {TextureApi::D3D12, static_cast<ID3D12Resource *>(output_[outputSlot_]), nullptr,
			static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM)};
	}
	const char *GetStatusReason() const noexcept override
	{
		return reason_[0] ? reason_ : "ready";
	}

	void Shutdown() noexcept override
	{
		WaitForSubmittedWork();
		if (feature_)
		{
			Record(ReleaseLeaf(feature_));
			feature_ = nullptr;
		}
		if (parameters_)
		{
			Record(DestroyParametersLeaf(parameters_));
			parameters_ = nullptr;
		}
		if (initialized_)
		{
			Record(ShutdownLeaf(device_));
			initialized_ = false;
		}
		for (auto& list : list_) list.reset();
		for (auto& allocator : allocator_) allocator.reset();
		for (auto& output : output_) output.reset();
		fence_.reset();
		queue_ = nullptr;
		device_ = nullptr;
		outputSlot_ = 0;
		slotFence_.fill(0);
		outputState_.fill(D3D12_RESOURCE_STATE_COMMON);
		resetRequested_ = true;
		successfulEvaluations_ = 0;
		rebuildReleasePending_ = false;
		compatibilityRebuildExhausted_ = false;
		rebuildPolicy_.Reset();
		SyncRebuildStats();
		stats_.dlss5ContractEvaluated = false;
		stats_.dlss5Readiness = Dlss5HookReadiness::Disabled;
		stats_.dlss5Route = Dlss5HookRoute::None;
		stats_.dlss5Components = {};
	}

private:
	BackendEvalStatus Unsupported(const char *reason) noexcept
	{
		std::snprintf(reason_, sizeof(reason_), "%s", reason);
		return BackendEvalStatus::Unsupported;
	}

	BackendEvalStatus UnsupportedOrBusy(const char *reason) noexcept
	{
		std::snprintf(reason_, sizeof(reason_), "%s", reason);
		return BackendEvalStatus::Busy;
	}

	BackendEvalStatus DeviceOrFailure(const char *reason) noexcept
	{
		std::snprintf(reason_, sizeof(reason_), "%s", reason);
		return device_ && FAILED(device_->GetDeviceRemovedReason())
			? BackendEvalStatus::DeviceRemoved : BackendEvalStatus::RecoverableFailure;
	}

	void Record(const NgxCallResult& call) noexcept
	{
		stats_.lastResult = static_cast<std::int32_t>(call.result);
		if (call.exceptionCode) stats_.lastExceptionCode = call.exceptionCode;
	}

	bool AllSubmittedWorkComplete() const noexcept
	{
		if (!fence_) return true;
		const auto completed = fence_->GetCompletedValue();
		for (const auto value : slotFence_)
			if (value != 0 && completed < value) return false;
		return true;
	}

	void RefreshDlss5HookStatus(bool contractEvaluated = false) noexcept
	{
		if (config_.mode != NeuralMode::Dlss5Experimental) return;
		stats_.dlss5Components = DetectDlss5HookComponents();
		if (contractEvaluated)
		{
			const auto ready = AssessDlss5Hook(true, config_.dlss5Route,
				stats_.dlss5Components, false);
			if (ready.componentsPresent) stats_.dlss5ContractEvaluated = true;
		}
		const auto assessment = AssessDlss5Hook(true, config_.dlss5Route, stats_.dlss5Components,
			stats_.dlss5ContractEvaluated);
		stats_.dlss5Route = config_.dlss5Route;
		stats_.dlss5Readiness = assessment.readiness;
		rebuildPolicy_.Observe(assessment.readiness, successfulEvaluations_);
		SyncRebuildStats();
		std::snprintf(reason_, sizeof(reason_), "%s", assessment.message);
	}

	void SyncRebuildStats() noexcept
	{
		stats_.compatibilityRebuilds = rebuildPolicy_.SuccessfulRebuilds();
		stats_.compatibilityRebuildAttempts = rebuildPolicy_.Attempts();
		stats_.compatibilityRebuildFailures = rebuildPolicy_.Failures();
		stats_.compatibilityRebuildReason = rebuildPolicy_.LastReason();
	}

	bool CreateCommandSlot(std::size_t slot) noexcept
	{
		return SUCCEEDED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			__uuidof(ID3D12CommandAllocator), reinterpret_cast<void **>(&allocator_[slot].get())))
			&& SUCCEEDED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				allocator_[slot], nullptr, __uuidof(ID3D12GraphicsCommandList),
				reinterpret_cast<void **>(&list_[slot].get())))
			&& SUCCEEDED(list_[slot]->Close());
	}

	bool CreateResources() noexcept
	{
		if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
			reinterpret_cast<void **>(&fence_.get()))))
			return false;
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = config_.outputWidth;
		desc.Height = config_.outputHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		for (std::size_t i = 0; i < output_.size(); ++i)
		{
			if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COMMON, nullptr, __uuidof(ID3D12Resource),
				reinterpret_cast<void **>(&output_[i].get()))) || !CreateCommandSlot(i))
			{
				std::snprintf(reason_, sizeof(reason_), "D3D12 neural resource ring creation failed");
				return false;
			}
			outputState_[i] = D3D12_RESOURCE_STATE_COMMON;
		}
		return true;
	}

	void Poison(std::size_t slot) noexcept
	{
		list_[slot].reset();
		allocator_[slot].reset();
		if (!CreateCommandSlot(slot))
			std::snprintf(reason_, sizeof(reason_), "failed to replace poisoned D3D12 command slot");
	}

	void WaitForSubmittedWork() noexcept
	{
		if (!queue_ || !fence_) return;
		std::uint64_t target = 0;
		for (const auto value : slotFence_) if (value > target) target = value;
		if (target == 0 || fence_->GetCompletedValue() >= target) return;
		HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!eventHandle) return;
		if (SUCCEEDED(fence_->SetEventOnCompletion(target, eventHandle)))
			WaitForSingleObject(eventHandle, 30000);
		CloseHandle(eventHandle);
	}

	static NVSDK_NGX_PerfQuality_Value Quality(NeuralMode mode) noexcept
	{
		switch (mode)
		{
		case NeuralMode::SrQuality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
		case NeuralMode::SrBalanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
		case NeuralMode::SrPerformance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
		case NeuralMode::SrUltraPerformance: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
		default: return NVSDK_NGX_PerfQuality_Value_DLAA;
		}
	}

	static bool IsDlaa(NeuralMode mode) noexcept
	{
		return mode == NeuralMode::Dlaa || mode == NeuralMode::DlaaHook
			|| mode == NeuralMode::Dlss5Experimental;
	}

	static bool IsSr(NeuralMode mode) noexcept
	{
		return mode == NeuralMode::SrQuality || mode == NeuralMode::SrBalanced
			|| mode == NeuralMode::SrPerformance || mode == NeuralMode::SrUltraPerformance;
	}

	StageConfig config_{};
	BackendStats stats_{};
	ID3D12Device *device_ = nullptr;
	ID3D12CommandQueue *queue_ = nullptr;
	NVSDK_NGX_Parameter *parameters_ = nullptr;
	NVSDK_NGX_Handle *feature_ = nullptr;
	ComPtr<ID3D12Fence> fence_;
	std::array<ComPtr<ID3D12CommandAllocator>, 3> allocator_;
	std::array<ComPtr<ID3D12GraphicsCommandList>, 3> list_;
	std::array<ComPtr<ID3D12Resource>, 3> output_;
	std::array<D3D12_RESOURCE_STATES, 3> outputState_{};
	std::array<std::uint64_t, 3> slotFence_{};
	std::uint64_t nextFence_ = 1;
	std::size_t outputSlot_ = 0;
	bool initialized_ = false;
	bool resetRequested_ = true;
	Dlss5CompatibilityRebuildPolicy rebuildPolicy_;
	bool rebuildReleasePending_ = false;
	bool compatibilityRebuildExhausted_ = false;
	std::uint64_t successfulEvaluations_ = 0;
	unsigned int optimalWidth_ = 0;
	unsigned int optimalHeight_ = 0;
	unsigned int maxWidth_ = 0;
	unsigned int maxHeight_ = 0;
	unsigned int minWidth_ = 0;
	unsigned int minHeight_ = 0;
	char reason_[192]{};
};

} // namespace

std::unique_ptr<INeuralBackend> CreateNgxD3D12Backend()
{
	return std::make_unique<NgxD3D12Backend>();
}

} // namespace flycast::rend::neural
