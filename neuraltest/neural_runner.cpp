// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/neural_stage.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

std::string HrText(const char *operation, HRESULT hr)
{
	char text[96]{};
	std::snprintf(text, sizeof(text), "%s failed (HRESULT 0x%08lX)", operation,
		static_cast<unsigned long>(hr));
	return text;
}

std::string AdapterName(ID3D11Device *device)
{
	ComPtr<IDXGIDevice> dxgiDevice;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC desc{};
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf())))
		&& SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))
		&& SUCCEEDED(adapter->GetDesc(&desc)))
	{
		char name[256]{};
		WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name,
			static_cast<int>(sizeof(name)), nullptr, nullptr);
		return name;
	}
	return "unknown D3D11 adapter";
}

bool CreateInput(ID3D11Device *device, std::uint32_t width, std::uint32_t height,
	DXGI_FORMAT format, const void *data, std::uint32_t rowPitch,
	ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11ShaderResourceView>& view,
	std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = data;
	initial.SysMemPitch = rowPitch;
	HRESULT hr = device->CreateTexture2D(&desc, &initial, texture.GetAddressOf());
	if (SUCCEEDED(hr))
		hr = device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf());
	if (FAILED(hr))
	{
		error = HrText("create neural input texture", hr);
		return false;
	}
	return true;
}

bool ReadbackOutput(ID3D11Device *device, ID3D11DeviceContext *context,
	const flycast::rend::neural::TextureRef& output, ComPtr<ID3D11Texture2D>& staging,
	Image& image, bool& black, std::string& error)
{
	auto *texture = static_cast<ID3D11Texture2D *>(output.resource);
	if (!texture)
	{
		error = "submitted stage returned no D3D11 output texture";
		return false;
	}
	D3D11_TEXTURE2D_DESC desc{};
	texture->GetDesc(&desc);
	if (!staging)
	{
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		const HRESULT hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
		if (FAILED(hr))
		{
			error = HrText("create neural readback texture", hr);
			return false;
		}
	}
	context->CopyResource(staging.Get(), texture);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	const HRESULT hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr))
	{
		error = HrText("map neural readback texture", hr);
		return false;
	}
	image.width = desc.Width;
	image.height = desc.Height;
	image.rgba.resize(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
	black = true;
	for (std::uint32_t y = 0; y < desc.Height; ++y)
	{
		const auto *source = static_cast<const std::uint8_t *>(mapped.pData)
			+ static_cast<std::size_t>(y) * mapped.RowPitch;
		auto *destination = image.rgba.data() + static_cast<std::size_t>(y) * desc.Width * 4;
		std::memcpy(destination, source, desc.Width * 4);
		for (std::uint32_t x = 0; x < desc.Width && black; ++x)
			black = source[x * 4] == 0 && source[x * 4 + 1] == 0 && source[x * 4 + 2] == 0;
	}
	context->Unmap(staging.Get(), 0);
	return true;
}

const char *StatusName(flycast::rend::neural::SubmitStatus status)
{
	using flycast::rend::neural::SubmitStatus;
	switch (status)
	{
	case SubmitStatus::Submitted: return "submitted";
	case SubmitStatus::Busy: return "busy";
	case SubmitStatus::Holding: return "holding";
	case SubmitStatus::Unsupported: return "unsupported";
	case SubmitStatus::RecoverableFailure: return "recoverable-failure";
	case SubmitStatus::Disabled: return "disabled";
	case SubmitStatus::DeviceRemoved: return "device-removed";
	}
	return "unknown";
}

} // namespace

bool RunLiveNeuralD3D11(const Image& input, const std::string& backend,
	const std::string& mode, std::uint32_t outputWidth, std::uint32_t outputHeight,
	bool disableNgx, bool warp, std::uint32_t frames, NeuralRunResult& result, std::string& error)
{
	using namespace flycast::rend::neural;
	if (input.width == 0 || input.height == 0 || input.rgba.size()
		!= static_cast<std::size_t>(input.width) * input.height * 4)
	{
		error = "invalid RGBA input image";
		return false;
	}
	UINT flags = 0;
#ifndef NDEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	D3D_FEATURE_LEVEL actual{};
	HRESULT hr = D3D11CreateDevice(nullptr, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
		nullptr, flags, requested, static_cast<UINT>(std::size(requested)), D3D11_SDK_VERSION,
		device.GetAddressOf(), &actual, context.GetAddressOf());
	if (FAILED(hr))
	{
		error = HrText("D3D11CreateDevice", hr);
		return false;
	}
	result.adapter = AdapterName(device.Get());
	result.surface = "native-d3d11";
	const std::size_t pixels = static_cast<std::size_t>(input.width) * input.height;
	std::vector<float> depth(pixels, .5f);
	std::vector<std::uint32_t> motion(pixels, 0);
	std::vector<std::uint8_t> mask(pixels, 255);
	ComPtr<ID3D11Texture2D> colorTexture, depthTexture, motionTexture, maskTexture;
	ComPtr<ID3D11ShaderResourceView> colorView, depthView, motionView, maskView;
	if (!CreateInput(device.Get(), input.width, input.height, DXGI_FORMAT_R8G8B8A8_UNORM,
		input.rgba.data(), input.width * 4, colorTexture, colorView, error)
		|| !CreateInput(device.Get(), input.width, input.height, DXGI_FORMAT_R32_FLOAT,
			depth.data(), input.width * 4, depthTexture, depthView, error)
		|| !CreateInput(device.Get(), input.width, input.height, DXGI_FORMAT_R16G16_FLOAT,
			motion.data(), input.width * 4, motionTexture, motionView, error)
		|| !CreateInput(device.Get(), input.width, input.height, DXGI_FORMAT_R8_UNORM,
			mask.data(), input.width, maskTexture, maskView, error))
		return false;
	if (disableNgx)
	{
		result.status = "unsupported";
		result.reason = "--no-ngx requested after D3D11 export texture validation";
		return true;
	}

	StageConfig config;
	config.mode = backend == "dlss5-hook" ? NeuralMode::Dlss5Experimental
		: backend == "dlaa-hook" ? NeuralMode::DlaaHook
		: mode == "balanced" ? NeuralMode::SrBalanced
		: mode == "performance" ? NeuralMode::SrPerformance
		: mode == "ultra-performance" ? NeuralMode::SrUltraPerformance
		: backend == "sr" ? NeuralMode::SrQuality : NeuralMode::Dlaa;
	config.api = Api::D3D11;
	config.outputWidth = outputWidth;
	config.outputHeight = outputHeight;
	config.contentRect = {0, 0, static_cast<std::int32_t>(outputWidth),
		static_cast<std::int32_t>(outputHeight)};
	config.hookCompatibility = config.mode == NeuralMode::DlaaHook
		|| config.mode == NeuralMode::Dlss5Experimental;
	if (config.mode == NeuralMode::Dlss5Experimental)
		config.dlss5Route = Dlss5HookRoute::D3D11External;
	NeuralStage stage(config);
	stage.SetGraphicsDevice(Api::D3D11, device.Get(), context.Get());
	NeuralFrame frame;
	frame.source = FrameSource::Geometry;
	frame.color = {TextureApi::D3D11, colorTexture.Get(), colorView.Get(),
		static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM)};
	frame.depth = {TextureApi::D3D11, depthTexture.Get(), depthView.Get(),
		static_cast<std::uint32_t>(DXGI_FORMAT_R32_FLOAT)};
	frame.motion = {TextureApi::D3D11, motionTexture.Get(), motionView.Get(),
		static_cast<std::uint32_t>(DXGI_FORMAT_R16G16_FLOAT)};
	frame.mask = {TextureApi::D3D11, maskTexture.Get(), maskView.Get(),
		static_cast<std::uint32_t>(DXGI_FORMAT_R8_UNORM)};
	frame.renderWidth = input.width;
	frame.renderHeight = input.height;
	frame.outputWidth = outputWidth;
	frame.outputHeight = outputHeight;
	frame.contentRect = config.contentRect;
	SubmitStatus status = SubmitStatus::Disabled;
	ComPtr<ID3D11Texture2D> staging;
	Image previousOutput;
	std::uint64_t previousOutputHash = 0;
	result.minTemporalPsnr = std::numeric_limits<double>::infinity();
	const std::uint32_t frameCount = frames == 0 ? 1 : frames;
	for (std::uint32_t i = 0; i < frameCount; ++i)
	{
		frame.frameId = i;
		frame.resetHistory = i == 0;
		frame.historyValid = i != 0;
		status = stage.TrySubmit(frame);
		if (status != SubmitStatus::Submitted)
			break;
		// Harness-only blocking readback validates every output and drains the headless
		// context. The emulator path never calls this helper or waits for completion.
		bool black = false;
		if (!ReadbackOutput(device.Get(), context.Get(), stage.GetOutput(), staging,
			result.output, black, error))
			return false;
		if (black) ++result.invalidFrames;
		const std::uint64_t outputHash = HashImage(result.output);
		if (i != 0)
		{
			if (outputHash != previousOutputHash) ++result.outputChanges;
			std::uint32_t changedPixels = 0;
			std::uint8_t maxDelta = 0;
			const double psnr = ComputePsnr(previousOutput, result.output, changedPixels, maxDelta);
			result.maxTemporalChangedPixels = (std::max)(result.maxTemporalChangedPixels, changedPixels);
			result.maxTemporalDelta = (std::max)(result.maxTemporalDelta, maxDelta);
			result.minTemporalPsnr = (std::min)(result.minTemporalPsnr, psnr);
		}
		previousOutputHash = outputHash;
		previousOutput = result.output;
	}
	result.status = StatusName(status);
	result.reason = stage.GetStatusReason();
	const auto stats = stage.GetStats();
	result.submissions = stats.submissions;
	result.busySkips = stats.busySkips;
	result.fallbacks = stats.fallbacks;
	result.lastNgxResult = stats.lastNgxResult;
	result.lastExceptionCode = stats.lastExceptionCode;
	result.compatibilityRebuilds = stats.compatibilityRebuilds;
	result.compatibilityRebuildAttempts = stats.compatibilityRebuildAttempts;
	result.compatibilityRebuildFailures = stats.compatibilityRebuildFailures;
	result.compatibilityRebuildReason = stats.compatibilityRebuildReason;
	result.dlss5ContractEvaluated = stats.dlss5ContractEvaluated;
	result.dlss5Route = stats.dlss5Route;
	result.dlss5Readiness = stats.dlss5Readiness;
	result.dlss5Components = stats.dlss5Components;
	result.outputHash = result.output.rgba.empty() ? 0 : previousOutputHash;
	if (frameCount < 2 || !std::isfinite(result.minTemporalPsnr)) result.minTemporalPsnr = 0.;
	if (status != SubmitStatus::Submitted)
		return true;
	return true;
}

} // namespace neuraltest
