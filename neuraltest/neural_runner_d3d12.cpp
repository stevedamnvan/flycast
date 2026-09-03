// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/neural_stage.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_6.h>
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

constexpr D3D12_RESOURCE_STATES kShaderRead = static_cast<D3D12_RESOURCE_STATES>(
	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

std::string HrText(const char *operation, HRESULT hr)
{
	char text[96]{};
	std::snprintf(text, sizeof(text), "%s failed (HRESULT 0x%08lX)", operation,
		static_cast<unsigned long>(hr));
	return text;
}

std::uint16_t FloatToHalf(float value)
{
	std::uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	const std::uint32_t sign = (bits >> 16) & 0x8000u;
	int exponent = static_cast<int>((bits >> 23) & 0xffu) - 127 + 15;
	std::uint32_t mantissa = bits & 0x7fffffu;
	if (exponent <= 0) return static_cast<std::uint16_t>(sign);
	if (exponent >= 31) return static_cast<std::uint16_t>(sign | 0x7c00u);
	mantissa += 0x1000u;
	if ((mantissa & 0x800000u) != 0) { mantissa = 0; ++exponent; }
	if (exponent >= 31) return static_cast<std::uint16_t>(sign | 0x7c00u);
	return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10)
		| (mantissa >> 13));
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

bool ExecuteAndWait(ID3D12CommandQueue *queue, ID3D12GraphicsCommandList *list,
	ID3D12Fence *fence, HANDLE eventHandle, std::uint64_t value, std::string& error)
{
	HRESULT hr = list->Close();
	if (FAILED(hr))
	{
		error = HrText("close D3D12 harness command list", hr);
		return false;
	}
	ID3D12CommandList *lists[] = {list};
	queue->ExecuteCommandLists(1, lists);
	hr = queue->Signal(fence, value);
	if (FAILED(hr))
	{
		error = HrText("signal D3D12 harness fence", hr);
		return false;
	}
	if (fence->GetCompletedValue() < value)
	{
		hr = fence->SetEventOnCompletion(value, eventHandle);
		if (FAILED(hr))
		{
			error = HrText("set D3D12 harness completion event", hr);
			return false;
		}
		if (WaitForSingleObject(eventHandle, 30000) != WAIT_OBJECT_0)
		{
			error = "timed out draining D3D12 harness work";
			return false;
		}
	}
	return true;
}

bool SignalAndWait(ID3D12CommandQueue *queue, ID3D12Fence *fence, HANDLE eventHandle,
	std::uint64_t value, std::string& error)
{
	HRESULT hr = queue->Signal(fence, value);
	if (FAILED(hr))
	{
		error = HrText("signal D3D12 harness fence", hr);
		return false;
	}
	if (fence->GetCompletedValue() < value)
	{
		hr = fence->SetEventOnCompletion(value, eventHandle);
		if (FAILED(hr) || WaitForSingleObject(eventHandle, 30000) != WAIT_OBJECT_0)
		{
			error = FAILED(hr) ? HrText("set D3D12 harness completion event", hr)
				: "timed out draining D3D11On12 harness work";
			return false;
		}
	}
	return true;
}

bool ValidateD3D11On12Surface(ID3D12Device *device, ID3D12CommandQueue *queue,
	ID3D12Fence *fence, HANDLE eventHandle, std::uint64_t fenceValue, std::string& error)
{
	IUnknown *queues[] = {queue};
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	ComPtr<ID3D11Device> d3d11Device;
	ComPtr<ID3D11DeviceContext> d3d11Context;
	D3D_FEATURE_LEVEL selected{};
	HRESULT hr = D3D11On12CreateDevice(device, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		levels, static_cast<UINT>(std::size(levels)), queues, 1, 0,
		d3d11Device.GetAddressOf(), d3d11Context.GetAddressOf(), &selected);
	ComPtr<ID3D11On12Device> on12;
	if (SUCCEEDED(hr)) hr = d3d11Device.As(&on12);
	if (FAILED(hr))
	{
		error = HrText("create D3D11On12 harness surface", hr);
		return false;
	}
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = 4;
	desc.Height = 4;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	ComPtr<ID3D12Resource> resource;
	hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(resource.GetAddressOf()));
	D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET, 0, 0, 0};
	ComPtr<ID3D11Resource> wrapped;
	if (SUCCEEDED(hr)) hr = on12->CreateWrappedResource(resource.Get(), &flags,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE,
		IID_PPV_ARGS(wrapped.GetAddressOf()));
	ComPtr<ID3D11RenderTargetView> rtv;
	if (SUCCEEDED(hr)) hr = d3d11Device->CreateRenderTargetView(wrapped.Get(), nullptr,
		rtv.GetAddressOf());
	if (FAILED(hr))
	{
		error = HrText("create D3D11On12 wrapped render target", hr);
		return false;
	}
	ID3D11Resource *resources[] = {wrapped.Get()};
	on12->AcquireWrappedResources(resources, 1);
	const float color[] = {.25f, .5f, .75f, 1.f};
	d3d11Context->ClearRenderTargetView(rtv.Get(), color);
	on12->ReleaseWrappedResources(resources, 1);
	d3d11Context->Flush();
	return SignalAndWait(queue, fence, eventHandle, fenceValue, error);
}

std::string AdapterName(ID3D12Device *device)
{
	ComPtr<IDXGIFactory4> factory;
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
		return "unknown D3D12 adapter";
	const LUID target = device->GetAdapterLuid();
	for (UINT index = 0;; ++index)
	{
		ComPtr<IDXGIAdapter1> adapter;
		if (factory->EnumAdapters1(index, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
			break;
		DXGI_ADAPTER_DESC1 desc{};
		if (SUCCEEDED(adapter->GetDesc1(&desc)) && desc.AdapterLuid.HighPart == target.HighPart
			&& desc.AdapterLuid.LowPart == target.LowPart)
		{
			char name[256]{};
			WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name,
				static_cast<int>(sizeof(name)), nullptr, nullptr);
			return name;
		}
	}
	return "unknown D3D12 adapter";
}

bool CreateInput(ID3D12Device *device, ID3D12GraphicsCommandList *list,
	std::uint32_t width, std::uint32_t height, DXGI_FORMAT format,
	const void *data, std::uint32_t rowPitch, ComPtr<ID3D12Resource>& texture,
	std::vector<ComPtr<ID3D12Resource>>& uploads, std::string& error)
{
	D3D12_HEAP_PROPERTIES defaultHeap{};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	HRESULT hr = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(texture.GetAddressOf()));
	if (FAILED(hr))
	{
		error = HrText("create D3D12 neural input texture", hr);
		return false;
	}
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT rows = 0;
	UINT64 rowBytes = 0;
	UINT64 totalBytes = 0;
	device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &rowBytes, &totalBytes);
	D3D12_HEAP_PROPERTIES uploadHeap{};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC buffer{};
	buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	buffer.Width = totalBytes;
	buffer.Height = 1;
	buffer.DepthOrArraySize = 1;
	buffer.MipLevels = 1;
	buffer.SampleDesc.Count = 1;
	buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> upload;
	hr = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upload.GetAddressOf()));
	if (FAILED(hr))
	{
		error = HrText("create D3D12 neural upload buffer", hr);
		return false;
	}
	void *mapped = nullptr;
	D3D12_RANGE noRead{0, 0};
	hr = upload->Map(0, &noRead, &mapped);
	if (FAILED(hr))
	{
		error = HrText("map D3D12 neural upload buffer", hr);
		return false;
	}
	for (UINT y = 0; y < rows; ++y)
		std::memcpy(static_cast<std::uint8_t *>(mapped) + footprint.Offset
			+ static_cast<std::size_t>(y) * footprint.Footprint.RowPitch,
			static_cast<const std::uint8_t *>(data) + static_cast<std::size_t>(y) * rowPitch,
			static_cast<std::size_t>(rowBytes));
	upload->Unmap(0, nullptr);
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = texture.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destination.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = upload.Get();
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	source.PlacedFootprint = footprint;
	list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	const auto barrier = Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, kShaderRead);
	list->ResourceBarrier(1, &barrier);
	uploads.push_back(upload);
	return true;
}

bool ReadbackOutput(ID3D12Device *device, ID3D12CommandQueue *queue,
	ID3D12CommandAllocator *allocator, ID3D12GraphicsCommandList *list,
	ID3D12Fence *fence, HANDLE eventHandle, std::uint64_t fenceValue,
	const flycast::rend::neural::TextureRef& output, ComPtr<ID3D12Resource>& readback,
	Image& image, bool& black, std::string& error)
{
	auto *texture = static_cast<ID3D12Resource *>(output.resource);
	if (!texture)
	{
		error = "submitted stage returned no D3D12 output texture";
		return false;
	}
	const D3D12_RESOURCE_DESC desc = texture->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT rows = 0;
	UINT64 rowBytes = 0;
	UINT64 totalBytes = 0;
	device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &rowBytes, &totalBytes);
	if (!readback)
	{
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_READBACK;
		D3D12_RESOURCE_DESC buffer{};
		buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer.Width = totalBytes;
		buffer.Height = 1;
		buffer.DepthOrArraySize = 1;
		buffer.MipLevels = 1;
		buffer.SampleDesc.Count = 1;
		buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(readback.GetAddressOf()));
		if (FAILED(hr))
		{
			error = HrText("create D3D12 neural readback buffer", hr);
			return false;
		}
	}
	HRESULT hr = allocator->Reset();
	if (SUCCEEDED(hr)) hr = list->Reset(allocator, nullptr);
	if (FAILED(hr))
	{
		error = HrText("reset D3D12 harness readback list", hr);
		return false;
	}
	auto toCopy = Transition(texture, kShaderRead, D3D12_RESOURCE_STATE_COPY_SOURCE);
	list->ResourceBarrier(1, &toCopy);
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = readback.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destination.PlacedFootprint = footprint;
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = texture;
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	auto toRead = Transition(texture, D3D12_RESOURCE_STATE_COPY_SOURCE, kShaderRead);
	list->ResourceBarrier(1, &toRead);
	if (!ExecuteAndWait(queue, list, fence, eventHandle, fenceValue, error)) return false;
	void *mapped = nullptr;
	D3D12_RANGE range{0, static_cast<SIZE_T>(totalBytes)};
	hr = readback->Map(0, &range, &mapped);
	if (FAILED(hr))
	{
		error = HrText("map D3D12 neural readback buffer", hr);
		return false;
	}
	image.width = static_cast<std::uint32_t>(desc.Width);
	image.height = desc.Height;
	image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
	black = true;
	for (std::uint32_t y = 0; y < image.height; ++y)
	{
		const auto *sourceRow = static_cast<const std::uint8_t *>(mapped) + footprint.Offset
			+ static_cast<std::size_t>(y) * footprint.Footprint.RowPitch;
		std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * image.width * 4,
			sourceRow, image.width * 4);
		for (std::uint32_t x = 0; x < image.width && black; ++x)
			black = sourceRow[x * 4] == 0 && sourceRow[x * 4 + 1] == 0 && sourceRow[x * 4 + 2] == 0;
	}
	D3D12_RANGE noWrite{0, 0};
	readback->Unmap(0, &noWrite);
	return true;
}

} // namespace

bool RunLiveNeuralD3D12(const Image& input, const std::string& backend,
	const std::string& mode, std::uint32_t outputWidth, std::uint32_t outputHeight,
	bool disableNgx, bool warp, bool depthInverted, const Image *previousInput,
	float motionX, float motionY, std::uint32_t frames, std::uint32_t dlssPreset,
	NeuralRunResult& result, std::string& error)
{
	using namespace flycast::rend::neural;
	if (input.width == 0 || input.height == 0 || input.rgba.size()
		!= static_cast<std::size_t>(input.width) * input.height * 4)
	{
		error = "invalid RGBA input image";
		return false;
	}
	if (previousInput && (previousInput->width != input.width || previousInput->height != input.height
		|| previousInput->rgba.size() != input.rgba.size()))
	{
		error = "invalid previous RGBA input image";
		return false;
	}
	ComPtr<IDXGIFactory4> factory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()));
	ComPtr<IDXGIAdapter> warpAdapter;
	IUnknown *adapter = nullptr;
	if (SUCCEEDED(hr) && warp)
	{
		hr = factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf()));
		adapter = warpAdapter.Get();
	}
	ComPtr<ID3D12Device> device;
	if (SUCCEEDED(hr)) hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(device.GetAddressOf()));
	if (FAILED(hr))
	{
		error = HrText("create D3D12 harness device", hr);
		return false;
	}
	result.adapter = AdapterName(device.Get());
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> list;
	ComPtr<ID3D12Fence> fence;
	if (FAILED(hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue.GetAddressOf())))
		|| FAILED(hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(allocator.GetAddressOf())))
		|| FAILED(hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			allocator.Get(), nullptr, IID_PPV_ARGS(list.GetAddressOf())))
		|| FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(fence.GetAddressOf()))))
	{
		error = HrText("create D3D12 harness queue resources", hr);
		return false;
	}
	HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!eventHandle)
	{
		error = "create D3D12 harness event failed";
		return false;
	}
	std::uint64_t harnessFence = 1;
	if (!ValidateD3D11On12Surface(device.Get(), queue.Get(), fence.Get(), eventHandle,
		harnessFence++, error))
	{
		CloseHandle(eventHandle);
		return false;
	}
	result.surface = "d3d11on12-same-queue";
	const std::size_t pixels = static_cast<std::size_t>(input.width) * input.height;
	std::vector<float> depth(pixels, .5f);
	const std::uint32_t packedMotion = static_cast<std::uint32_t>(FloatToHalf(motionX))
		| (static_cast<std::uint32_t>(FloatToHalf(motionY)) << 16);
	std::vector<std::uint32_t> motion(pixels, packedMotion);
	std::vector<std::uint32_t> zeroMotion(pixels, 0);
	std::vector<std::uint8_t> mask(pixels, previousInput ? 0 : 255);
	ComPtr<ID3D12Resource> color, previousColor, depthTexture, motionTexture,
		zeroMotionTexture, maskTexture;
	std::vector<ComPtr<ID3D12Resource>> uploads;
	if (!CreateInput(device.Get(), list.Get(), input.width, input.height,
		DXGI_FORMAT_R8G8B8A8_UNORM, input.rgba.data(), input.width * 4, color, uploads, error)
		|| !CreateInput(device.Get(), list.Get(), input.width, input.height,
			DXGI_FORMAT_R32_FLOAT, depth.data(), input.width * 4, depthTexture, uploads, error)
		|| !CreateInput(device.Get(), list.Get(), input.width, input.height,
			DXGI_FORMAT_R16G16_FLOAT, motion.data(), input.width * 4, motionTexture, uploads, error)
		|| (previousInput && !CreateInput(device.Get(), list.Get(), input.width, input.height,
			DXGI_FORMAT_R8G8B8A8_UNORM, previousInput->rgba.data(), input.width * 4,
			previousColor, uploads, error))
		|| (previousInput && !CreateInput(device.Get(), list.Get(), input.width, input.height,
			DXGI_FORMAT_R16G16_FLOAT, zeroMotion.data(), input.width * 4,
			zeroMotionTexture, uploads, error))
		|| !CreateInput(device.Get(), list.Get(), input.width, input.height,
			DXGI_FORMAT_R8_UNORM, mask.data(), input.width, maskTexture, uploads, error)
		|| !ExecuteAndWait(queue.Get(), list.Get(), fence.Get(), eventHandle, harnessFence++, error))
	{
		CloseHandle(eventHandle);
		return false;
	}
	uploads.clear();
	if (disableNgx)
	{
		result.status = "unsupported";
		result.reason = "--no-ngx requested after D3D12 export texture validation";
		CloseHandle(eventHandle);
		return true;
	}

	StageConfig config;
	config.mode = backend == "dlss5-hook" ? NeuralMode::Dlss5Experimental
		: backend == "dlaa-hook" ? NeuralMode::DlaaHook
		: mode == "balanced" ? NeuralMode::SrBalanced
		: mode == "performance" ? NeuralMode::SrPerformance
		: mode == "ultra-performance" ? NeuralMode::SrUltraPerformance
		: backend == "sr" ? NeuralMode::SrQuality : NeuralMode::Dlaa;
	config.api = Api::D3D12;
	config.outputWidth = outputWidth;
	config.outputHeight = outputHeight;
	config.contentRect = {0, 0, static_cast<std::int32_t>(outputWidth),
		static_cast<std::int32_t>(outputHeight)};
	config.depthInverted = depthInverted;
	config.dlssPreset = dlssPreset;
	config.hookCompatibility = config.mode == NeuralMode::DlaaHook
		|| config.mode == NeuralMode::Dlss5Experimental;
	if (config.mode == NeuralMode::Dlss5Experimental)
		config.dlss5Route = Dlss5HookRoute::D3D11On12;
	NeuralStage stage(config);
	stage.SetGraphicsDevice(Api::D3D12, device.Get(), queue.Get());
	NeuralFrame frame;
	frame.source = FrameSource::Geometry;
	frame.color = {TextureApi::D3D12, color.Get(), nullptr,
		static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM)};
	frame.depth = {TextureApi::D3D12, depthTexture.Get(), nullptr,
		static_cast<std::uint32_t>(DXGI_FORMAT_R32_FLOAT)};
	frame.motion = {TextureApi::D3D12, motionTexture.Get(), nullptr,
		static_cast<std::uint32_t>(DXGI_FORMAT_R16G16_FLOAT)};
	frame.mask = {TextureApi::D3D12, maskTexture.Get(), nullptr,
		static_cast<std::uint32_t>(DXGI_FORMAT_R8_UNORM)};
	frame.renderWidth = input.width;
	frame.renderHeight = input.height;
	frame.outputWidth = outputWidth;
	frame.outputHeight = outputHeight;
	frame.contentRect = config.contentRect;
	SubmitStatus status = SubmitStatus::Disabled;
	ComPtr<ID3D12Resource> readback;
	Image previousOutput;
	std::uint64_t previousHash = 0;
	result.minTemporalPsnr = std::numeric_limits<double>::infinity();
	const std::uint32_t frameCount = frames == 0 ? 1 : frames;
	for (std::uint32_t i = 0; i < frameCount; ++i)
	{
		if (previousInput && i == 0)
		{
			frame.color = {TextureApi::D3D12, previousColor.Get(), nullptr,
				static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM)};
			frame.motion = {TextureApi::D3D12, zeroMotionTexture.Get(), nullptr,
				static_cast<std::uint32_t>(DXGI_FORMAT_R16G16_FLOAT)};
		}
		else
		{
			frame.color = {TextureApi::D3D12, color.Get(), nullptr,
				static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM)};
			frame.motion = {TextureApi::D3D12, motionTexture.Get(), nullptr,
				static_cast<std::uint32_t>(DXGI_FORMAT_R16G16_FLOAT)};
		}
		frame.frameId = i;
		frame.resetHistory = i == 0;
		frame.historyValid = i != 0;
		status = stage.TrySubmit(frame);
		if (status != SubmitStatus::Submitted) break;
		bool black = false;
		if (!ReadbackOutput(device.Get(), queue.Get(), allocator.Get(), list.Get(), fence.Get(),
			eventHandle, harnessFence++, stage.GetOutput(), readback, result.output, black, error))
		{
			CloseHandle(eventHandle);
			return false;
		}
		if (black) ++result.invalidFrames;
		const std::uint64_t outputHash = HashImage(result.output);
		if (i != 0)
		{
			if (outputHash != previousHash) ++result.outputChanges;
			std::uint32_t changedPixels = 0;
			std::uint8_t maxDelta = 0;
			const double psnr = ComputePsnr(previousOutput, result.output, changedPixels, maxDelta);
			result.maxTemporalChangedPixels = (std::max)(result.maxTemporalChangedPixels, changedPixels);
			result.maxTemporalDelta = (std::max)(result.maxTemporalDelta, maxDelta);
			result.minTemporalPsnr = (std::min)(result.minTemporalPsnr, psnr);
		}
		previousHash = outputHash;
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
	result.outputHash = result.output.rgba.empty() ? 0 : previousHash;
	if (frameCount < 2 || !std::isfinite(result.minTemporalPsnr)) result.minTemporalPsnr = 0.;
	stage.Shutdown();
	CloseHandle(eventHandle);
	return true;
}

} // namespace neuraltest
