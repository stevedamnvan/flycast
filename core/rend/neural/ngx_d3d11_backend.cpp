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

NgxCallResult EvaluateLeaf(ID3D11DeviceContext *context, NVSDK_NGX_Handle *handle,
	NVSDK_NGX_Parameter *parameters, NVSDK_NGX_D3D11_DLSS_Eval_Params *evaluate) noexcept
{
	NGX_SEH_CALL(NGX_D3D11_EVALUATE_DLSS_EXT(context, handle, parameters, evaluate));
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
		if (!CreateOutputRing(config.outputWidth, config.outputHeight))
		{
			++stats_.createFailures;
			return BackendEvalStatus::RecoverableFailure;
		}
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
		const bool dlaa = config_.mode == NeuralMode::Dlaa || config_.mode == NeuralMode::DlaaHook;
		if (dlaa && (frame.renderWidth != config_.outputWidth || frame.renderHeight != config_.outputHeight))
			return Unsupported("DLAA requires equal render and output dimensions");
		if (!feature_)
		{
			NVSDK_NGX_DLSS_Create_Params create{};
			create.Feature.InWidth = frame.renderWidth;
			create.Feature.InHeight = frame.renderHeight;
			create.Feature.InTargetWidth = config_.outputWidth;
			create.Feature.InTargetHeight = config_.outputHeight;
			create.Feature.InPerfQualityValue = Quality(config_.mode);
			create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
			create.InEnableOutputSubrects = false;
			const auto call = CreateLeaf(context_, &feature_, parameters_, &create);
			Record(call);
			if (call.exceptionCode != 0 || NVSDK_NGX_FAILED(call.result))
			{
				++stats_.createFailures;
				return Unsupported(call.exceptionCode ? "NGX DLSS create raised an exception"
					: "NGX DLSS feature creation failed");
			}
		}

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
		const auto call = EvaluateLeaf(context_, feature_, parameters_, &evaluate);
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
		reason_[0] = 0;
		return BackendEvalStatus::Success;
	}

	void ResetHistory() noexcept override { resetRequested_ = true; }
	BackendStats GetStats() const noexcept override { return stats_; }
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
	}

private:
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

	StageConfig config_{};
	BackendStats stats_{};
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
	char reason_[192]{};
};

} // namespace

std::unique_ptr<INeuralBackend> CreateNgxD3D11Backend()
{
	return std::make_unique<NgxD3D11Backend>();
}

} // namespace flycast::rend::neural
