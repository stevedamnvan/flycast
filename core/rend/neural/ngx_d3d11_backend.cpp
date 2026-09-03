// SPDX-License-Identifier: GPL-2.0-or-later
#include "neural_backend.h"
#include "version.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>

#include "windows/comptr.h"

#include <array>
#include <cstdio>

namespace flycast::rend::neural {
namespace {

constexpr DWORD kInjectedNgxSehCode = 0xE0424E47u;

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

NgxCallResult InitLeaf(const wchar_t *path, ID3D11Device *device,
	const NVSDK_NGX_FeatureCommonInfo *featureInfo) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D11_Init_with_ProjectID(
		"7d5f2a1c-3b8e-4c6a-9f0d-2e4b6c8a1d3f", NVSDK_NGX_ENGINE_TYPE_CUSTOM,
		GIT_VERSION, path, device, featureInfo, NVSDK_NGX_Version_API));
}

NgxCallResult CapabilityLeaf(NVSDK_NGX_Parameter **parameters, int *available,
	int *needsDriver, unsigned int *major, unsigned int *minor, int *featureInitResult) noexcept
{
	NgxCallResult call;
	__try
	{
		call.result = NVSDK_NGX_D3D11_GetCapabilityParameters(parameters);
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

NgxCallResult CreateLeaf(ID3D11DeviceContext *context, NVSDK_NGX_Handle **handle,
	NVSDK_NGX_Parameter *parameters, NVSDK_NGX_DLSS_Create_Params *create) noexcept
{
	NGX_SEH_CALL(NGX_D3D11_CREATE_DLSS_EXT(context, handle, parameters, create));
}

NgxCallResult ApplyPresetLeaf(NVSDK_NGX_Parameter *parameters,
	std::uint32_t preset) noexcept
{
	NgxCallResult call;
	__try
	{
		parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, preset);
		parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, preset);
		parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, preset);
		parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, preset);
		parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, preset);
		call.result = NVSDK_NGX_Result_Success;
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

NgxCallResult EvaluateLeaf(ID3D11DeviceContext *context, NVSDK_NGX_Handle *handle,
	NVSDK_NGX_Parameter *parameters, NVSDK_NGX_D3D11_DLSS_Eval_Params *evaluate,
	bool injectException = false) noexcept
{
	NgxCallResult call;
	__try
	{
		if (injectException)
			RaiseException(kInjectedNgxSehCode, 0, 0, nullptr);
		call.result = NGX_D3D11_EVALUATE_DLSS_EXT(context, handle, parameters, evaluate);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		call.exceptionCode = GetExceptionCode();
	}
	return call;
}

NgxCallResult ReleaseLeaf(NVSDK_NGX_Handle *handle) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D11_ReleaseFeature(handle));
}

NgxCallResult DestroyParametersLeaf(NVSDK_NGX_Parameter *parameters) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D11_DestroyParameters(parameters));
}

NgxCallResult ShutdownLeaf(ID3D11Device *device) noexcept
{
	NGX_SEH_CALL(NVSDK_NGX_D3D11_Shutdown1(device));
}

#undef NGX_SEH_CALL

class NgxD3D11Backend final : public INeuralBackend
{
public:
	~NgxD3D11Backend() override { Shutdown(); }

	BackendEvalStatus Initialize(const StageConfig& config, void *device,
		void *context) noexcept override
	{
		Shutdown();
		config_ = config;
		injectedCount_ = 0;
		rebuildPolicy_.Configure(config.dlss5RebuildGraceEvaluations,
			config.dlss5RebuildMaxAttempts);
		stats_.dlss5Route = config.dlss5Route;
		device_ = static_cast<ID3D11Device *>(device);
		context_ = static_cast<ID3D11DeviceContext *>(context);
		if (!device_ || !context_)
			return Unsupported("D3D11 device or immediate context is unavailable");
		if (config.outputWidth == 0 || config.outputHeight == 0)
			return Unsupported("neural output dimensions are zero");
		wchar_t path[MAX_PATH]{};
		if (GetTempPathW(static_cast<DWORD>(std::size(path)), path) == 0)
			return Unsupported("no writable NGX application-data path");
		wchar_t featurePath[32768]{};
		const DWORD featurePathLength = GetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH",
			featurePath, static_cast<DWORD>(std::size(featurePath)));
		const wchar_t *featurePaths[] = {featurePath};
		NVSDK_NGX_FeatureCommonInfo featureInfo{};
		if (featurePathLength > 0 && featurePathLength < std::size(featurePath))
		{
			featureInfo.PathListInfo.Path = featurePaths;
			featureInfo.PathListInfo.Length = 1;
		}
		auto call = InitLeaf(path, device_, featureInfo.PathListInfo.Length ? &featureInfo : nullptr);
		Record(call);
		if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
		{
			++stats_.createFailures;
			return Unsupported(call.exceptionCode ? "NGX D3D11 initialization raised an exception"
				: "NGX D3D11 initialization failed");
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
				return Unsupported("NGX capability query raised an exception");
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
				return Unsupported(call.exceptionCode ? "NGX optimal-settings query raised an exception"
					: "NGX optimal-settings query failed");
			}
		}
		if (!CreateOutputRing(config.outputWidth, config.outputHeight))
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
			return Unsupported("NGX D3D11 session is not initialized");
		if (!frame.color || !frame.depth || !frame.motion || !frame.mask)
			return Unsupported("neural frame is missing a required DLSS input texture");
		if (frame.color.api != TextureApi::D3D11 || frame.depth.api != TextureApi::D3D11
			|| frame.motion.api != TextureApi::D3D11 || frame.mask.api != TextureApi::D3D11)
			return Unsupported("neural frame texture API does not match D3D11 backend");
		const bool dlaa = config_.mode == NeuralMode::Dlaa || config_.mode == NeuralMode::DlaaHook
			|| config_.mode == NeuralMode::Dlss5Experimental;
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
				return Busy("DLSS 5 compatibility rebuild awaiting asynchronous retirement");
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
		if (ConsumeInjection(FailureInjection::DeviceRemoved,
			"injected D3D11 device-removed status"))
			return BackendEvalStatus::DeviceRemoved;
		if (config_.failureInjection == FailureInjection::RuntimeUnavailable
			&& successfulEvaluations_ >= config_.failureInjectionAfter
			&& injectedCount_ < config_.failureInjectionCount)
		{
			if (!AllSubmittedWorkComplete())
				return Busy("injected D3D11 runtime retirement awaiting asynchronous work");
			ConsumeInjection(FailureInjection::RuntimeUnavailable,
				"injected D3D11 runtime unavailable");
			++stats_.runtimeUnavailableStatuses;
			Shutdown();
			return Unsupported("injected D3D11 runtime unavailable; session retired");
		}

		if (!feature_)
		{
			const bool compatibilityCreate = rebuildPolicy_.RecreatePending();
			if (compatibilityCreate && !rebuildPolicy_.BeginCreateAttempt())
				return Unsupported("DLSS 5 compatibility rebuild retry limit reached");
			NVSDK_NGX_DLSS_Create_Params create{};
			const auto presetCall = ApplyPresetLeaf(parameters_, config_.dlssPreset);
			Record(presetCall);
			if (presetCall.exceptionCode != 0 || NVSDK_NGX_FAILED(presetCall.result))
				return Unsupported("NGX public render-preset hint raised an exception");
			create.Feature.InWidth = frame.renderWidth;
			create.Feature.InHeight = frame.renderHeight;
			create.Feature.InTargetWidth = config_.outputWidth;
			create.Feature.InTargetHeight = config_.outputHeight;
			create.Feature.InPerfQualityValue = Quality(config_.mode);
			create.InFeatureCreateFlags = IsSr(config_.mode)
				? NVSDK_NGX_DLSS_Feature_Flags_MVLowRes
				: NVSDK_NGX_DLSS_Feature_Flags_None;
			if (config_.depthInverted)
				create.InFeatureCreateFlags = static_cast<NVSDK_NGX_DLSS_Feature_Flags>(
					create.InFeatureCreateFlags | NVSDK_NGX_DLSS_Feature_Flags_DepthInverted);
			create.InEnableOutputSubrects = false;
			if (ConsumeInjection(FailureInjection::FeatureCreate,
				"injected D3D11 feature-create failure"))
			{
				++stats_.createFailures;
				if (compatibilityCreate)
				{
					rebuildPolicy_.CompleteCreateAttempt(false);
					SyncRebuildStats();
					if (!rebuildPolicy_.RetryAvailable())
					{
						compatibilityRebuildExhausted_ = true;
						return Unsupported("injected compatibility recreate retry limit reached");
					}
				}
				return BackendEvalStatus::RecoverableFailure;
			}
			const auto call = CreateLeaf(context_, &feature_, parameters_, &create);
			Record(call);
			if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
			{
				++stats_.createFailures;
				if (compatibilityCreate)
				{
					rebuildPolicy_.CompleteCreateAttempt(false);
					SyncRebuildStats();
					if (rebuildPolicy_.RetryAvailable())
						return DeviceOrFailure("DLSS 5 compatibility recreate failed; bounded retry pending");
					compatibilityRebuildExhausted_ = true;
					return Unsupported("DLSS 5 compatibility recreate retry limit reached");
				}
				return Unsupported(call.exceptionCode ? "NGX DLSS create raised an exception"
					: "NGX DLSS feature creation failed");
			}
			if (compatibilityCreate)
			{
				rebuildPolicy_.CompleteCreateAttempt(true);
				SyncRebuildStats();
			}
		}

		if (ConsumeInjection(FailureInjection::OutputBusy,
			"injected D3D11 output-ring busy status"))
			return BackendEvalStatus::Busy;
		const std::size_t slot = (outputSlot_ + 1) % output_.size();
		if (slotSubmitted_[slot])
		{
			const HRESULT ready = context_->GetData(completion_[slot], nullptr, 0,
				D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (ready == S_FALSE)
				return BackendEvalStatus::Busy;
			if (FAILED(ready))
				return DeviceOrFailure("D3D11 output-ring completion query failed");
		}
		NVSDK_NGX_D3D11_DLSS_Eval_Params evaluate{};
		evaluate.Feature.pInColor = static_cast<ID3D11Resource *>(frame.color.resource);
		evaluate.Feature.pInOutput = output_[slot].get();
		evaluate.Feature.InSharpness = 0.f;
		evaluate.pInDepth = static_cast<ID3D11Resource *>(frame.depth.resource);
		evaluate.pInMotionVectors = static_cast<ID3D11Resource *>(frame.motion.resource);
		evaluate.pInBiasCurrentColorMask = static_cast<ID3D11Resource *>(frame.mask.resource);
		evaluate.InJitterOffsetX = config_.hookCompatibility ? 0.f : frame.jitterX;
		evaluate.InJitterOffsetY = config_.hookCompatibility ? 0.f : frame.jitterY;
		evaluate.InRenderSubrectDimensions = {frame.renderWidth, frame.renderHeight};
		evaluate.InReset = resetRequested_ || frame.resetHistory ? 1 : 0;
		evaluate.InMVScaleX = 1.f;
		evaluate.InMVScaleY = 1.f;
		evaluate.InPreExposure = 1.f;
		evaluate.InExposureScale = 1.f;
		if (ConsumeInjection(FailureInjection::Evaluate,
			"injected D3D11 evaluate failure"))
		{
			++stats_.evaluateFailures;
			return BackendEvalStatus::RecoverableFailure;
		}
		const bool injectException = ConsumeInjection(FailureInjection::SehException,
			"injected D3D11 NGX evaluate exception");
		const auto call = EvaluateLeaf(context_, feature_, parameters_, &evaluate,
			injectException);
		Record(call);
		if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
		{
			++stats_.evaluateFailures;
			return DeviceOrFailure(call.exceptionCode ? "NGX DLSS evaluate raised an exception"
				: "NGX DLSS evaluate failed");
		}
		context_->End(completion_[slot]);
		slotSubmitted_[slot] = true;
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
	BackendStats GetStats() const noexcept override
	{
		auto result = stats_;
		for (const auto& resource : output_) result.liveResourceObjects += resource ? 1u : 0u;
		for (const auto& view : outputViews_) result.liveResourceObjects += view ? 1u : 0u;
		for (const auto& query : completion_) result.liveResourceObjects += query ? 1u : 0u;
		return result;
	}
	TextureRef GetOutput() const noexcept override
	{
		if (!output_[outputSlot_]) return {};
		return {TextureApi::D3D11, static_cast<ID3D11Texture2D *>(output_[outputSlot_]),
			static_cast<ID3D11ShaderResourceView *>(outputViews_[outputSlot_]),
			static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM)};
	}
	const char *GetStatusReason() const noexcept override
	{
		return reason_[0] ? reason_ : "ready";
	}

	void Shutdown() noexcept override
	{
		for (auto& query : completion_) query.reset();
		for (auto& view : outputViews_) view.reset();
		for (auto& texture : output_) texture.reset();
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
		device_ = nullptr;
		context_ = nullptr;
		outputSlot_ = 0;
		slotSubmitted_.fill(false);
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
	bool ConsumeInjection(FailureInjection injection, const char *reason) noexcept
	{
		if (config_.failureInjection != injection
			|| successfulEvaluations_ < config_.failureInjectionAfter
			|| injectedCount_ >= config_.failureInjectionCount)
			return false;
		++injectedCount_;
		std::snprintf(reason_, sizeof(reason_), "%s (%u/%u)", reason, injectedCount_,
			config_.failureInjectionCount);
		return true;
	}

	BackendEvalStatus Busy(const char *reason) noexcept
	{
		std::snprintf(reason_, sizeof(reason_), "%s", reason);
		return BackendEvalStatus::Busy;
	}

	BackendEvalStatus Unsupported(const char *reason) noexcept
	{
		std::snprintf(reason_, sizeof(reason_), "%s", reason);
		return BackendEvalStatus::Unsupported;
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

	bool AllSubmittedWorkComplete() noexcept
	{
		for (std::size_t slot = 0; slot < slotSubmitted_.size(); ++slot)
		{
			if (!slotSubmitted_[slot]) continue;
			const HRESULT ready = context_->GetData(completion_[slot], nullptr, 0,
				D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (ready == S_FALSE) return false;
			if (FAILED(ready)) return false;
			slotSubmitted_[slot] = false;
		}
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
		const auto assessment = AssessDlss5Hook(true, config_.dlss5Route,
			stats_.dlss5Components, stats_.dlss5ContractEvaluated);
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

	bool CreateOutputRing(std::uint32_t width, std::uint32_t height) noexcept
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		D3D11_QUERY_DESC queryDesc{D3D11_QUERY_EVENT, 0};
		for (std::size_t i = 0; i < output_.size(); ++i)
		{
			if (FAILED(device_->CreateTexture2D(&desc, nullptr, &output_[i].get()))
				|| FAILED(device_->CreateShaderResourceView(output_[i], nullptr, &outputViews_[i].get()))
				|| FAILED(device_->CreateQuery(&queryDesc, &completion_[i].get())))
			{
				std::snprintf(reason_, sizeof(reason_), "D3D11 NGX output ring creation failed");
				return false;
			}
		}
		return true;
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

	static bool IsSr(NeuralMode mode) noexcept
	{
		return mode == NeuralMode::SrQuality || mode == NeuralMode::SrBalanced
			|| mode == NeuralMode::SrPerformance || mode == NeuralMode::SrUltraPerformance;
	}

	StageConfig config_{};
	BackendStats stats_{};
	std::uint32_t injectedCount_ = 0;
	ID3D11Device *device_ = nullptr;
	ID3D11DeviceContext *context_ = nullptr;
	NVSDK_NGX_Parameter *parameters_ = nullptr;
	NVSDK_NGX_Handle *feature_ = nullptr;
	std::array<ComPtr<ID3D11Texture2D>, 3> output_;
	std::array<ComPtr<ID3D11ShaderResourceView>, 3> outputViews_;
	std::array<ComPtr<ID3D11Query>, 3> completion_;
	std::array<bool, 3> slotSubmitted_{};
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

std::unique_ptr<INeuralBackend> CreateNgxD3D11Backend()
{
	return std::make_unique<NgxD3D11Backend>();
}

} // namespace flycast::rend::neural
