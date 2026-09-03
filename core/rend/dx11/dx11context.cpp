/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "dx11context.h"
#ifndef LIBRETRO
#include "rend/osd.h"
#ifdef USE_SDL
#include "sdl/sdl.h"
#endif
#include "hw/pvr/Renderer_if.h"
#include "emulator.h"
#include "dx11_driver.h"
#include "imgui_impl_dx11.h"
#include <dxgi1_6.h>
#ifdef TARGET_UWP
#include <windows.h>
#include <gamingdeviceinformation.h>
#endif

#include "nowide/stackstring.hpp"

void DX11Context::Create(void *window, void *display) {
	new DX11Context(window, display);
}

DX11Context::DX11Context(void *window, void *display)
	: GraphicsContext(window, display)
{
	if (!init())
		throw FlycastException("DX11 initialization failed");
}

DX11Context::~DX11Context() {
	term();
}

bool DX11Context::init(bool keepCurrentWindow)
{
	NOTICE_LOG(RENDERER, "DX11 Context initializing");
#ifdef USE_SDL
	if (!keepCurrentWindow && !sdl_recreate_window(0))
		return false;
#endif
#ifdef TARGET_UWP
	GAMING_DEVICE_MODEL_INFORMATION info {};
	GetGamingDeviceModelInformation(&info);
	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId != GAMING_DEVICE_DEVICE_ID_NONE)
	{
		Windows::Graphics::Display::Core::HdmiDisplayInformation^ dispInfo = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
		Windows::Graphics::Display::Core::HdmiDisplayMode^ displayMode = dispInfo->GetCurrentDisplayMode();
		NOTICE_LOG(RENDERER, "HDMI resolution: %d x %d", displayMode->ResolutionWidthInRawPixels, displayMode->ResolutionHeightInRawPixels);
		settings.display.width = displayMode->ResolutionWidthInRawPixels;
		settings.display.height = displayMode->ResolutionHeightInRawPixels;
		settings.display.uiScale = settings.display.width / 1920.0f * 1.4f;
	}
#endif

	// Use high performance GPU on Windows 10 (1803 or later)
	ComPtr<IDXGIFactory1> dxgiFactory;
	ComPtr<IDXGIFactory6> dxgiFactory6;
	ComPtr<IDXGIAdapter> dxgiAdapter;
	HRESULT hr;
	allowTearing = false;
	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&dxgiFactory.get());
	if (SUCCEEDED(hr))
	{
		dxgiFactory.as(dxgiFactory6);
		if (dxgiFactory6) 
		{
			dxgiFactory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter), (void **)&dxgiAdapter.get());
			UINT tearing;
			if (SUCCEEDED(dxgiFactory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing,
			                                                 sizeof(tearing))) && tearing != 0)
				allowTearing = true;
			dxgiFactory6.reset();
		}
	}
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
#ifdef FLYCAST_ENABLE_NEURAL
	if (config::NeuralD3D12Surface.get())
	{
		hr = D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_11_0,
			__uuidof(ID3D12Device), reinterpret_cast<void **>(&d3d12Device.get()));
		if (SUCCEEDED(hr))
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc{};
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			hr = d3d12Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
				reinterpret_cast<void **>(&d3d12Queue.get()));
		}
		if (SUCCEEDED(hr))
		{
			IUnknown *queues[] = {d3d12Queue.get()};
			hr = D3D11On12CreateDevice(d3d12Device, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
				featureLevels, ARRAYSIZE(featureLevels), queues, 1, 0, &pDevice.get(),
				&pDeviceContext.get(), &featureLevel);
			if (SUCCEEDED(hr)) pDevice.as(d3d11On12Device);
		}
		if (FAILED(hr) || !d3d11On12Device)
		{
			WARN_LOG(RENDERER, "D3D11On12 device creation failed: %x; falling back to native D3D11", hr);
			d3d11On12Device.reset();
			d3d12Queue.reset();
			d3d12Device.reset();
			pDeviceContext.reset();
			pDevice.reset();
		}
		else
			NOTICE_LOG(RENDERER, "D3D11 renderer is using the D3D11On12 neural surface");
	}
	if (!pDevice)
#endif
	hr = D3D11CreateDevice(
	    dxgiAdapter.get(), // High performance GPU, or fallback to use the default adapter.
	    dxgiAdapter.get() == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN, // D3D_DRIVER_TYPE_UNKNOWN is required when providing an adapter.
	    nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, // | D3D11_CREATE_DEVICE_DEBUG,
	    featureLevels,
	    ARRAYSIZE(featureLevels),
	    D3D11_SDK_VERSION,
	    &pDevice.get(),
	    &featureLevel,
	    &pDeviceContext.get());
	if (!pDevice || FAILED(hr)) {
		WARN_LOG(RENDERER, "D3D11 device creation failed: %x", hr);
		return false;
	}

	ComPtr<IDXGIDevice2> dxgiDevice;
	pDevice.as(dxgiDevice);

	bool refreshFactoryFromD3D11 = true;
#ifdef FLYCAST_ENABLE_NEURAL
	refreshFactoryFromD3D11 = !d3d12Queue || !dxgiAdapter;
#endif
	if (refreshFactoryFromD3D11)
	{
		dxgiAdapter.reset();
		dxgiDevice->GetAdapter(&dxgiAdapter.get());
		dxgiFactory.reset();
		dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), (void **)&dxgiFactory.get());
	}
	DXGI_ADAPTER_DESC desc;
	dxgiAdapter->GetDesc(&desc);
	nowide::stackstring wdesc;
	wdesc.convert(desc.Description);
	adapterDesc = wdesc.get();
	adapterVersion = std::to_string(desc.Revision);
	vendorId = desc.VendorId;

	ComPtr<IDXGIFactory2> dxgiFactory2;
	dxgiFactory.as(dxgiFactory2);

	if (dxgiFactory2)
	{
		// DX 11.1
		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.SampleDesc.Count = 1;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		if (allowTearing)
			desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

#ifdef TARGET_UWP
		desc.Width = settings.display.width;
		desc.Height = settings.display.height;
		hr = dxgiFactory2->CreateSwapChainForCoreWindow(pDevice, (IUnknown *)window, &desc, nullptr, &swapchain1.get());
#else
		IUnknown *swapchainDevice = pDevice;
#ifdef FLYCAST_ENABLE_NEURAL
		if (d3d12Queue) swapchainDevice = d3d12Queue;
#endif
		hr = dxgiFactory2->CreateSwapChainForHwnd(swapchainDevice, (HWND)window, &desc, nullptr, nullptr, &swapchain1.get());
#endif
		if (SUCCEEDED(hr))
			swapchain1.as(swapchain);
	}
	else
	{
		// DX 11.0
		swapchain1.reset();
#ifdef TARGET_UWP
		return false;
#endif
		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferCount = 2;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.OutputWindow = (HWND)window;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.BufferCount = 2;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		if (allowTearing)
			desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		IUnknown *swapchainDevice = pDevice;
#ifdef FLYCAST_ENABLE_NEURAL
		if (d3d12Queue) swapchainDevice = d3d12Queue;
#endif
		hr = dxgiFactory->CreateSwapChain(swapchainDevice, &desc, &swapchain.get());
	}
	if (FAILED(hr)) {
		WARN_LOG(RENDERER, "D3D11 swap chain creation failed: %x", hr);
		pDevice.reset();
		return false;
	}

#ifndef TARGET_UWP
	// Prevent DXGI from monitoring our message queue for ALT+Enter
	dxgiFactory->MakeWindowAssociation((HWND)window, DXGI_MWA_NO_WINDOW_CHANGES);
#endif
	D3D11_FEATURE_DATA_SHADER_CACHE cacheSupport{};
	if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_SHADER_CACHE, &cacheSupport, (UINT)sizeof(cacheSupport))))
	{
		_hasShaderCache = cacheSupport.SupportFlags & D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE;
		if (!_hasShaderCache)
			NOTICE_LOG(RENDERER, "No system-provided shader cache");
	}

	imguiDriver = std::unique_ptr<ImGuiDriver>(new DX11Driver(pDevice, pDeviceContext));
	resize();
	shaders.init(pDevice, &D3DCompile);
	overlay.init(pDevice, pDeviceContext, &shaders, &samplers);
	bool success = checkTextureSupport();
	if (!success)
		term();
	return success;
}

void DX11Context::term()
{
	NOTICE_LOG(RENDERER, "DX11 Context terminating");
	overlay.term();
	samplers.term();
	shaders.term();
	imguiDriver.reset();
#ifdef FLYCAST_ENABLE_NEURAL
	releaseWrappedBackBuffer();
	for (auto& view : wrappedBackBufferViews) view.reset();
	for (auto& buffer : wrappedBackBuffers) buffer.reset();
	swapchain3.reset();
	pendingNeuralOutputFrameId = 0;
	pendingNeuralOutputPresent = false;
	neuralEvidenceBackBufferAttempts = 0;
#endif
	renderTargetView.reset();
	swapchain1.reset();
	swapchain.reset();
	if (pDeviceContext)
	{
		pDeviceContext->ClearState();
		pDeviceContext->Flush();
	}
	pDeviceContext.reset();
	pDevice.reset();
#ifdef FLYCAST_ENABLE_NEURAL
	d3d11On12Device.reset();
	d3d12Queue.reset();
	d3d12Device.reset();
#endif
	d3dcompiler = nullptr;
}

void DX11Context::Present()
{
	if (!frameRendered)
		return;
	frameRendered = false;
#ifdef FLYCAST_ENABLE_NEURAL
	if (pendingNeuralOutputPresent && config::NeuralDlss5EvidenceCapture.get())
		captureNeuralEvidenceBackBuffer(pendingNeuralOutputFrameId);
	releaseWrappedBackBuffer();
#endif
	bool swapOnVSync = !settings.input.fastForwardMode && config::VSync;
	HRESULT hr;
	if (!swapchain) {
		hr = DXGI_ERROR_DEVICE_RESET;
	}
	else if (swapOnVSync) {
		int swapInterval = std::clamp((int)(settings.display.refreshRate / 60.f * gameSwapInterval), 1, 4);
		hr = swapchain->Present(swapInterval, 0);
	}
	else {
		hr = swapchain->Present(0, allowTearing ? DXGI_PRESENT_ALLOW_TEARING : DXGI_PRESENT_DO_NOT_WAIT);
	}
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
		WARN_LOG(RENDERER, "Present failed: device removed/reset");
#ifdef FLYCAST_ENABLE_NEURAL
		if (d3d12Device)
			WARN_LOG(RENDERER, "D3D12 device removed reason: %x",
				d3d12Device->GetDeviceRemovedReason());
#endif
		handleDeviceLost();
	}
	else if (hr != DXGI_ERROR_WAS_STILL_DRAWING && FAILED(hr)) {
		WARN_LOG(RENDERER, "Present failed %x", hr);
	}
#ifdef FLYCAST_ENABLE_NEURAL
	if (SUCCEEDED(hr) && pendingNeuralOutputPresent)
	{
		++neuralOutputPresentCount;
		const auto evidenceLimit = static_cast<std::uint64_t>(
			std::clamp(config::NeuralDlss5EvidenceCaptureFrames.get(), 1, 480));
		if (neuralOutputPresentCount == 1
			|| (config::NeuralDlss5EvidenceCapture.get() && neuralOutputPresentCount <= evidenceLimit))
			NOTICE_LOG(RENDERER,
				"DLSS 5 candidate public-output present completed: frame=%llu route=d3d11on12; external mutation unconfirmed",
				static_cast<unsigned long long>(pendingNeuralOutputFrameId));
		pendingNeuralOutputFrameId = 0;
		pendingNeuralOutputPresent = false;
	}
	else if (FAILED(hr) && hr != DXGI_ERROR_WAS_STILL_DRAWING)
	{
		pendingNeuralOutputFrameId = 0;
		pendingNeuralOutputPresent = false;
	}
	if (hr != DXGI_ERROR_DEVICE_REMOVED && hr != DXGI_ERROR_DEVICE_RESET)
		acquireWrappedBackBuffer();
#endif
}

void DX11Context::EndImGuiFrame()
{
	if (pDevice && pDeviceContext && renderTargetView)
	{
#ifdef FLYCAST_ENABLE_NEURAL
		acquireWrappedBackBuffer();
#endif
		if (overlayOnly) {
			overlay.draw(settings.display.width, settings.display.height, config::FloatVMUs, true);
		}
		else
		{
			pDeviceContext->OMSetRenderTargets(1, &renderTargetView.get(), nullptr);
			const FLOAT black[4] { 0.f, 0.f, 0.f, 1.f };
			pDeviceContext->ClearRenderTargetView(renderTargetView, black);
			if (renderer != nullptr)
				renderer->RenderLastFrame();
		}
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	frameRendered = true;
}

void DX11Context::resize()
{
	if (!pDevice)
		return;
	if (swapchain)
	{
		ID3D11RenderTargetView *nullRTV = nullptr;
		pDeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
#ifdef FLYCAST_ENABLE_NEURAL
		releaseWrappedBackBuffer();
		for (auto& view : wrappedBackBufferViews) view.reset();
		for (auto& buffer : wrappedBackBuffers) buffer.reset();
		swapchain3.reset();
		pendingNeuralOutputFrameId = 0;
		pendingNeuralOutputPresent = false;
		neuralEvidenceBackBufferAttempts = 0;
#endif
		renderTargetView.reset();
#ifdef TARGET_UWP
		HRESULT hr = swapchain->ResizeBuffers(2, settings.display.width, settings.display.height, DXGI_FORMAT_R8G8B8A8_UNORM, allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
#else
		HRESULT hr = swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | (allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			handleDeviceLost();
		    return;
		}
#endif
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "ResizeBuffers failed: %x", hr);
			return;
		}

		// Create a render target view
		ComPtr<ID3D11Texture2D> backBuffer;
		bool wrappedTargetsCreated = false;
#ifdef FLYCAST_ENABLE_NEURAL
		if (d3d11On12Device)
		{
			hr = swapchain1.as(swapchain3);
			for (std::size_t i = 0; SUCCEEDED(hr) && i < On12BackBufferCount; ++i)
			{
				ComPtr<ID3D12Resource> backBuffer12;
				hr = swapchain->GetBuffer(static_cast<UINT>(i), __uuidof(ID3D12Resource),
					reinterpret_cast<void **>(&backBuffer12.get()));
				if (FAILED(hr)) break;
				D3D11_RESOURCE_FLAGS flags{};
				flags.BindFlags = D3D11_BIND_RENDER_TARGET;
				hr = d3d11On12Device->CreateWrappedResource(backBuffer12, &flags,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT,
					__uuidof(ID3D11Texture2D),
					reinterpret_cast<void **>(&wrappedBackBuffers[i].get()));
				if (SUCCEEDED(hr))
					hr = pDevice->CreateRenderTargetView(wrappedBackBuffers[i], nullptr,
						&wrappedBackBufferViews[i].get());
			}
			wrappedTargetsCreated = SUCCEEDED(hr);
		}
		else
#endif
			hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "swapChain->GetBuffer() failed: %x", hr);
			return;
		}

		if (!wrappedTargetsCreated)
			hr = pDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView.get());
		if (FAILED(hr) || (!wrappedTargetsCreated && !renderTargetView))
		{
			WARN_LOG(RENDERER, "CreateRenderTargetView failed: %x", hr);
			return;
		}
#ifdef FLYCAST_ENABLE_NEURAL
		acquireWrappedBackBuffer();
#endif
		pDeviceContext->OMSetRenderTargets(1, &renderTargetView.get(), nullptr);

		if (swapchain1)
		{
			DXGI_SWAP_CHAIN_DESC1 desc;
			swapchain1->GetDesc1(&desc);
#ifndef TARGET_UWP
			settings.display.width = desc.Width;
			settings.display.height = desc.Height;
#endif
			NOTICE_LOG(RENDERER, "Swapchain resized: %d x %d", desc.Width, desc.Height);
		}
		else
		{
			DXGI_SWAP_CHAIN_DESC desc;
			swapchain->GetDesc(&desc);
			settings.display.width = desc.BufferDesc.Width;
			settings.display.height = desc.BufferDesc.Height;
			NOTICE_LOG(RENDERER, "Swapchain resized: %d x %d", desc.BufferDesc.Width, desc.BufferDesc.Height);
		}
	}
	// TODO minimized window
}

#ifdef FLYCAST_ENABLE_NEURAL
void DX11Context::AcquireWrappedResources(ID3D11Resource *const *resources, UINT count) noexcept
{
	if (d3d11On12Device && resources && count != 0)
		d3d11On12Device->AcquireWrappedResources(resources, count);
}

void DX11Context::ReleaseWrappedResources(ID3D11Resource *const *resources, UINT count) noexcept
{
	if (d3d11On12Device && resources && count != 0)
	{
		d3d11On12Device->ReleaseWrappedResources(resources, count);
		pDeviceContext->Flush();
	}
}

void DX11Context::acquireWrappedBackBuffer() noexcept
{
	if (d3d11On12Device && swapchain3 && !wrappedBackBufferAcquired)
	{
		wrappedBackBufferIndex = swapchain3->GetCurrentBackBufferIndex();
		if (wrappedBackBufferIndex >= wrappedBackBuffers.size()
			|| !wrappedBackBuffers[wrappedBackBufferIndex])
			return;
		ID3D11Resource *resource = wrappedBackBuffers[wrappedBackBufferIndex];
		AcquireWrappedResources(&resource, 1);
		renderTargetView = wrappedBackBufferViews[wrappedBackBufferIndex];
		wrappedBackBufferAcquired = true;
	}
}

void DX11Context::releaseWrappedBackBuffer() noexcept
{
	if (d3d11On12Device && wrappedBackBufferAcquired
		&& wrappedBackBufferIndex < wrappedBackBuffers.size()
		&& wrappedBackBuffers[wrappedBackBufferIndex])
	{
		ID3D11RenderTargetView *nullTarget = nullptr;
		pDeviceContext->OMSetRenderTargets(1, &nullTarget, nullptr);
		ID3D11Resource *resource = wrappedBackBuffers[wrappedBackBufferIndex];
		ReleaseWrappedResources(&resource, 1);
		wrappedBackBufferAcquired = false;
	}
}

void DX11Context::captureNeuralEvidenceBackBuffer(std::uint64_t frameId) noexcept
{
	const auto captureLimit = static_cast<std::uint32_t>(
		std::clamp(config::NeuralDlss5EvidenceCaptureFrames.get(), 1, 480));
	if (neuralEvidenceBackBufferAttempts >= captureLimit || !renderTargetView)
		return;
	const auto captureNumber = ++neuralEvidenceBackBufferAttempts;
	ComPtr<ID3D11Resource> resource;
	renderTargetView->GetResource(&resource.get());
	ComPtr<ID3D11Texture2D> texture;
	resource.as(texture);
	if (!texture)
	{
		WARN_LOG(RENDERER, "DLSS 5 developer present evidence failed: no backbuffer texture");
		return;
	}
	D3D11_TEXTURE2D_DESC desc{};
	texture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;
	ComPtr<ID3D11Texture2D> staging;
	if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, &staging.get())))
	{
		WARN_LOG(RENDERER, "DLSS 5 developer present evidence failed: staging texture creation");
		return;
	}
	pDeviceContext->CopyResource(staging, texture);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(pDeviceContext->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
	{
		WARN_LOG(RENDERER, "DLSS 5 developer present evidence failed: staging map");
		return;
	}
	constexpr std::uint64_t offset = 14695981039346656037ull;
	constexpr std::uint64_t prime = 1099511628211ull;
	std::uint64_t hash = offset;
	std::uint32_t markerPixels = 0;
	const auto *bytes = static_cast<const std::uint8_t *>(mapped.pData);
	for (UINT y = 0; y < desc.Height; ++y)
	{
		const auto *row = bytes + static_cast<std::size_t>(y) * mapped.RowPitch;
		for (UINT x = 0; x < desc.Width * 4; ++x)
		{
			hash ^= row[x];
			hash *= prime;
		}
		if (y < 32)
		{
			for (UINT x = 0; x < std::min<UINT>(32, desc.Width); ++x)
			{
				const auto *pixel = row + x * 4;
				const bool cyan = ((x / 8) + (y / 8)) % 2 != 0;
				const auto nearByte = [](std::uint8_t value, std::uint8_t expected) {
					return value >= expected - std::min<std::uint8_t>(expected, 2)
						&& value <= expected + std::min<std::uint8_t>(static_cast<std::uint8_t>(255 - expected), 2);
				};
				if (nearByte(pixel[0], 255) && nearByte(pixel[1], cyan ? 255 : 0)
					&& nearByte(pixel[2], cyan ? 0 : 255))
					++markerPixels;
			}
		}
	}
	pDeviceContext->Unmap(staging, 0);
	NOTICE_LOG(RENDERER,
		"DLSS 5 developer present evidence: capture=%u frame=%llu backbuffer_fnv64=%016llX marker_pixels=%u/1024 size=%ux%u; synchronous developer mode",
		captureNumber, static_cast<unsigned long long>(frameId), static_cast<unsigned long long>(hash),
		markerPixels, desc.Width, desc.Height);
}
#endif

void DX11Context::handleDeviceLost()
{
	if (pDevice)
	{
		HRESULT hr = pDevice->GetDeviceRemovedReason();
		WARN_LOG(RENDERER, "Device removed reason: %x", hr);
	}
	rend_term_renderer();
	term();
	if (init(true))
	{
		rend_init_renderer();
	}
	else
	{
		Renderer* rend_norend(void);
		renderer = rend_norend();
		renderer->Init();
	}
}

const pD3DCompile DX11Context::getCompiler()
{
	if (d3dcompiler == nullptr)
	{
#ifndef TARGET_UWP
		if (!d3dcompilerLib.load("d3dcompiler_47.dll"))
		{
			if (!d3dcompilerLib.load("d3dcompiler_46.dll"))
			{
				WARN_LOG(RENDERER, "Neither d3dcompiler_47.dll or d3dcompiler_46.dll can be loaded");
				return D3DCompile;
			}
		}
		d3dcompiler = d3dcompilerLib.getFunc("D3DCompile", d3dcompiler);
#endif
		if (d3dcompiler == nullptr)
			d3dcompiler = D3DCompile;
	}
	return d3dcompiler;
}
#endif // !LIBRETRO

bool DX11Context::checkTextureSupport()
{
	const DXGI_FORMAT formats[] = { DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_A8_UNORM };
	const char * const fmtNames[] = { "B5G5R5A1", "B4G4R4A4", "B5G6R5", "B8G8R8A8", "A8" };
	const TextureType dcTexTypes[] = { TextureType::_5551, TextureType::_4444, TextureType::_565, TextureType::_8888, TextureType::_8 };
	UINT support;
	for (std::size_t i = 0; i < std::size(formats); i++)
	{
		supportedTexFormats[(int)dcTexTypes[i]] = false;
		pDevice->CheckFormatSupport(formats[i], &support);
		if ((support & (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)) != (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))
		{
			if (formats[i] == DXGI_FORMAT_B8G8R8A8_UNORM)
			{
				// Can't do much without this format
				ERROR_LOG(RENDERER, "Fatal: Format %s not supported", fmtNames[i]);
				return false;
			}
			WARN_LOG(RENDERER, "Format %s not supported", fmtNames[i]);
		}
		else
		{
			if ((support & D3D11_FORMAT_SUPPORT_MIP) == 0)
				WARN_LOG(RENDERER, "Format %s doesn't support mipmaps", fmtNames[i]);
			else if ((support & (D3D11_FORMAT_SUPPORT_MIP_AUTOGEN | D3D11_FORMAT_SUPPORT_RENDER_TARGET)) != (D3D11_FORMAT_SUPPORT_MIP_AUTOGEN | D3D11_FORMAT_SUPPORT_RENDER_TARGET))
				WARN_LOG(RENDERER, "Format %s doesn't support mipmap autogen", fmtNames[i]);
			else
				supportedTexFormats[(int)dcTexTypes[i]] = true;
		}
	}

	return true;
}
