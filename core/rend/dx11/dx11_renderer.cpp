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
#include "dx11_renderer.h"
#include "dx11context.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"
#include "ui/gui.h"
#include "rend/sorter.h"
#ifdef FLYCAST_ENABLE_NEURAL
#include "rend/neural/quality_profile.h"
#include "version.h"
#endif

#include <chrono>
#include <memory>
#include <thread>

void os_VideoRoutingTermDX();

const D3D11_INPUT_ELEMENT_DESC MainLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, nx),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
#ifdef FLYCAST_ENABLE_NEURAL
const D3D11_INPUT_ELEMENT_DESC NeuralLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, nx), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
#endif
const D3D11_INPUT_ELEMENT_DESC ModVolLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(ModTriangle, x0), D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

bool DX11Renderer::Init()
{
	NOTICE_LOG(RENDERER, "DX11 renderer initializing");
	device = DX11Context::Instance()->getDevice();
	deviceContext = DX11Context::Instance()->getDeviceContext();
	if (!device || !deviceContext)
	{
		WARN_LOG(RENDERER, "Null device or device context. Aborting");
		return false;
	}

	shaders = &DX11Context::Instance()->getShaders();
	samplers = &DX11Context::Instance()->getSamplers();
	bool success = (bool)shaders->getVertexShader(true, true);
	ComPtr<ID3DBlob> blob = shaders->getVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(MainLayout, std::size(MainLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &mainInputLayout.get()));
#ifdef FLYCAST_ENABLE_NEURAL
	blob = shaders->getNeuralVertexShaderBlob();
	success = success && blob && SUCCEEDED(device->CreateInputLayout(NeuralLayout,
		std::size(NeuralLayout), blob->GetBufferPointer(), blob->GetBufferSize(),
		&neuralInputLayout.get()));
#endif
	blob = shaders->getMVVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(ModVolLayout, std::size(ModVolLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &modVolInputLayout.get()));

	// Constants buffers
	{
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(VertexConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &vtxConstants.get()));

		desc.ByteWidth = sizeof(PixelConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlConstants.get()));

		desc.ByteWidth = sizeof(PixelPolyConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlPolyConstants.get()));
	}

	// Rasterizer state
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = true;
		desc.ScissorEnable = true;
		desc.DepthClipEnable = false;
		device->CreateRasterizerState(&desc, &rasterCullNone.get());
		desc.CullMode = D3D11_CULL_FRONT;
		device->CreateRasterizerState(&desc, &rasterCullFront.get());
		desc.CullMode = D3D11_CULL_BACK;
		device->CreateRasterizerState(&desc, &rasterCullBack.get());
	}
	// Palette texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 32;
		desc.Height = 32;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &paletteTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(paletteTexture, &viewDesc, &paletteTextureView.get());
	}
	// Fog texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 128;
		desc.Height = 2;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &fogTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(fogTexture, &viewDesc, &fogTextureView.get());
	}
	// White texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 8;
		desc.Height = 8;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &whiteTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(whiteTexture, &viewDesc, &whiteTextureView.get());

		u32 texData[8 * 8];
		memset(texData, 0xff, sizeof(texData));
		deviceContext->UpdateSubresource(whiteTexture, 0, nullptr, texData, 8 * sizeof(u32), 8 * sizeof(u32) * 8);
	}

	quad = std::make_unique<Quad>();
	quad->init(device, deviceContext, shaders);
	n2Helper.init(device, deviceContext);

	updateFogTable = true;

	if (!success)
	{
		WARN_LOG(RENDERER, "DirectX 11 renderer initialization failed");
		Term();
	}
	frameRendered = false;

#ifdef FLYCAST_ENABLE_NEURAL
	flycast::rend::neural::StageConfig neuralConfig;
	neuralConfig.mode = flycast::rend::neural::NeuralMode::Passthrough;
	neuralConfig.api = flycast::rend::neural::Api::D3D11;
	neuralStage = flycast::rend::neural::NeuralStage(neuralConfig);
#endif

	return success;
}

void DX11Renderer::Term()
{
	NOTICE_LOG(RENDERER, "DX11 renderer terminating");
#ifdef FLYCAST_ENABLE_NEURAL
	neuralStage.Shutdown();
	neuralInstrumentation.SetEnabled(false);
	releaseNeuralResources();
	neuralInputLayout.reset();
#endif
#ifdef VIDEO_ROUTING
	os_VideoRoutingTermDX();
#endif
	n2Helper.term();
	vtxConstants.reset();
	pxlConstants.reset();
	fbTex.reset();
	fbTextureView.reset();
	fbRenderTarget.reset();
	fbScaledRenderTarget.reset();
	fbScaledTextureView.reset();
	fbScaledTexture.reset();
	quad.reset();
	deviceContext.reset();
	device.reset();
	vrStagingTexture.reset();
	vrStagingTextureSRV.reset();
	vrScaledTexture.reset();
	vrScaledRenderTarget.reset();
}

void DX11Renderer::createDepthTexAndView(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11DepthStencilView>& view, int width, int height, DXGI_FORMAT format, UINT bindFlags)
{
	view.reset();
	texture.reset();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | bindFlags;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture.get());
	if (FAILED(hr))
		WARN_LOG(RENDERER, "Depth/stencil creation failed");

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc{};
	viewDesc.Format = format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_D24_UNORM_S8_UINT;
	viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = device->CreateDepthStencilView(texture, &viewDesc, &view.get());
	if (FAILED(hr))
		WARN_LOG(RENDERER, "Depth/stencil view creation failed");
}

void DX11Renderer::createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height)
{
	texture.reset();
	renderTarget.reset();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.MipLevels = 1;

	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Framebuffer texture creation failed");
		return;
	}

	hr = device->CreateRenderTargetView(texture, nullptr, &renderTarget.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Framebuffer render target creation failed");
		return;
	}
	FLOAT black[4] = { 0.f, 0.f, 0.f, 0.f };
	deviceContext->ClearRenderTargetView(renderTarget, black);
}

void DX11Renderer::resize(int w, int h)
{
	if (width == (u32)w && height == (u32)h)
		return;
	width = w;
	height = h;
#ifdef FLYCAST_ENABLE_NEURAL
	if (neuralInstrumentation.IsEnabled())
		neuralInstrumentation.Discontinuity();
#endif

	// Create framebuffer texture
	{
		fbTextureView.reset();
		createTexAndRenderTarget(fbTex, fbRenderTarget, width, height);

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(fbTex, &viewDesc, &fbTextureView.get());
	}

	// Create depth stencil texture
	createDepthTexAndView(depthTex, depthTexView, width, height);

	frameRendered = false;
	frameRenderedOnce = false;
}

bool DX11Renderer::ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize)
{
	if (minSize <= currentSize && buffer)
		return true;
	if (currentSize == 0)
		currentSize = minSize;
	else
		while (currentSize < minSize)
			currentSize *= 2;
	buffer.reset();
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = currentSize;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &buffer.get()));
}

BaseTextureCacheData *DX11Renderer::GetTexture(TSP tsp, TCW tcw, int area)
{
	//lookup texture
	DX11Texture* tf = texCache.getTextureCacheData(tsp, tcw, area);

	//update if needed
	if (tf->NeedsUpdate())
	{
		if (!tf->Update())
			tf = nullptr;
	}
	else if (tf->IsCustomTextureAvailable())
	{
		texCache.DeleteLater(tf->texture);
		tf->texture.reset();
		// FIXME textureView
		tf->loadCustomTexture();
	}
	return tf;
}

void DX11Renderer::Process(TA_context* ctx)
{
	rendContext = &ctx->rend;
	if (resetTextureCache) {
		texCache.Clear();
		resetTextureCache = false;
	}
	texCache.Cleanup();
	if (!ctx->rend.isRTT && ctx->rend.swapInterval > 0)
		DX11Context::Instance()->setSwapInterval(ctx->rend.swapInterval);

	ta_parse(ctx, true);
}

void DX11Renderer::resetContextState()
{
	// Reset device context state. Very much needed for libretro where current state is unknown.
	deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
	deviceContext->PSSetShader(nullptr, nullptr, 0);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->VSSetShader(nullptr, nullptr, 0);
	ID3D11ShaderResourceView *nullview[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] {};
	deviceContext->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->SetPredication(nullptr, false);
	deviceContext->SOSetTargets(0, nullptr, nullptr);
}

void DX11Renderer::configVertexShader()
{
	matrices.CalcMatrices(rendContext, rendContext->framebufferWidth, rendContext->framebufferHeight);
	setBaseScissor();

	if (rendContext->isRTT)
	{
		prepareRttRenderTarget(rendContext->fb_W_SOF1 & VRAM_MASK);
	}
	else
	{
		D3D11_VIEWPORT vp{};
		vp.Width = (FLOAT)rendContext->framebufferWidth;
		vp.Height = (FLOAT)rendContext->framebufferHeight;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		deviceContext->RSSetViewports(1, &vp);
	}
	VertexConstants constant{};
	memcpy(&constant.transMatrix, &matrices.GetNormalMatrix(), sizeof(constant.transMatrix));
	constant.leftPlane[0] = 1;
	constant.leftPlane[3] = 1;
	constant.rightPlane[0] = -1;
	constant.rightPlane[3] = 1;
	constant.topPlane[1] = 1;
	constant.topPlane[3] = 1;
	constant.bottomPlane[1] = -1;
	constant.bottomPlane[3] = 1;
#ifdef FLYCAST_ENABLE_NEURAL
	constant.neuralRenderSize[0] = static_cast<float>(width);
	constant.neuralRenderSize[1] = static_cast<float>(height);
#endif
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vtxConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &constant, sizeof(constant));
	deviceContext->Unmap(vtxConstants, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &vtxConstants.get());
}

void DX11Renderer::uploadGeometryBuffers()
{
	setFirstProvokingVertex(*rendContext);

	size_t size = rendContext->verts.size() * sizeof(decltype(*rendContext->verts.data()));
	bool rc = ensureBufferSize(vertexBuffer, D3D11_BIND_VERTEX_BUFFER, vertexBufferSize, size);
	verify(rc);
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, rendContext->verts.data(), size);
	deviceContext->Unmap(vertexBuffer, 0);

	size = rendContext->idx.size() * sizeof(decltype(*rendContext->idx.data()));
	rc = ensureBufferSize(indexBuffer, D3D11_BIND_INDEX_BUFFER, indexBufferSize, size);
	verify(rc);
	deviceContext->Map(indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, rendContext->idx.data(), size);
	deviceContext->Unmap(indexBuffer, 0);

	if (config::ModifierVolumes && !rendContext->modtrig.empty())
	{
		const ModTriangle *data = &rendContext->modtrig[0];
		size = rendContext->modtrig.size() * sizeof(decltype(rendContext->modtrig[0]));
		rc = ensureBufferSize(modvolBuffer, D3D11_BIND_VERTEX_BUFFER, modvolBufferSize, size);
		verify(rc);
		deviceContext->Map(modvolBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, data, size);
		deviceContext->Unmap(modvolBuffer, 0);
	}
    unsigned int stride = sizeof(Vertex);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void DX11Renderer::setupPixelShaderConstants()
{
	PixelConstants pixelConstants;
	// VERT and RAM fog color constants
	FOG_COL_VERT.getRGBColor(pixelConstants.fog_col_vert);
	FOG_COL_RAM.getRGBColor(pixelConstants.fog_col_ram);

	// Fog density
	pixelConstants.fogDensity = FOG_DENSITY.get() * config::ExtraDepthScale;
	// Shadow scale
	pixelConstants.shadowScale = FPU_SHAD_SCALE.scale_factor / 256.f;

	// Color clamping
	rendContext->fog_clamp_min.getRGBAColor(pixelConstants.colorClampMin);
	rendContext->fog_clamp_max.getRGBAColor(pixelConstants.colorClampMax);

	// Punch-through alpha ref
	pixelConstants.alphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

	// Dithering
	dithering = config::EmulateFramebuffer && rendContext->fb_W_CTRL.fb_dither && rendContext->fb_W_CTRL.fb_packmode <= 3;
	if (dithering)
	{
		switch (rendContext->fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			pixelConstants.ditherDivisor[0] = pixelConstants.ditherDivisor[1] = pixelConstants.ditherDivisor[2] = 2.f;
		break;
		case 1: // 565 RGB 16 bit
			pixelConstants.ditherDivisor[0] = pixelConstants.ditherDivisor[2] = 2.f;
			pixelConstants.ditherDivisor[1] = 4.f;
			break;
		case 2: // 4444 ARGB 16 bit
			pixelConstants.ditherDivisor[0] = pixelConstants.ditherDivisor[1] = pixelConstants.ditherDivisor[2] = 1.f;
			break;
		default:
			break;
		}
		pixelConstants.ditherDivisor[3] = 1.f;
	}

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(pxlConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &pixelConstants, sizeof(pixelConstants));
	deviceContext->Unmap(pxlConstants, 0);
	ID3D11Buffer *buffers[] { pxlConstants, pxlPolyConstants };
	deviceContext->PSSetConstantBuffers(0, std::size(buffers), buffers);
}

bool DX11Renderer::Render()
{
	resetContextState();
	bool is_rtt = rendContext->isRTT;
	if (!is_rtt)
	{
#ifdef FLYCAST_ENABLE_NEURAL
		if (!config::EmulateFramebuffer)
			beginNeuralPerformanceFrame();
#endif
		resize(rendContext->framebufferWidth, rendContext->framebufferHeight);
		deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), depthTexView);
		deviceContext->ClearDepthStencilView(depthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
		if (rendContext->clearFramebuffer)
		{
			float colors[4];
			VO_BORDER_COL.getRGBColor(colors);
			colors[3] = 1.f;
			deviceContext->ClearRenderTargetView(fbRenderTarget, colors);
		}
	}
	configVertexShader();

	deviceContext->IASetInputLayout(mainInputLayout);

	n2Helper.resetCache();
	uploadGeometryBuffers();

	updateFogTexture();
	updatePaletteTexture();

	setupPixelShaderConstants();

	drawStrips();
#ifdef FLYCAST_ENABLE_NEURAL
	if (!is_rtt)
		markNeuralPvrEnd();
#endif

	if (is_rtt)
	{
		readRttRenderTarget(rendContext->fb_W_SOF1 & VRAM_MASK);
	}
	else if (config::EmulateFramebuffer)
	{
		writeFramebufferToVRAM();
	}
	else
	{
		aspectRatio = getOutputFramebufferAspectRatio();
#ifdef FLYCAST_ENABLE_NEURAL
		submitNeuralFrame();
#endif
#ifndef LIBRETRO
		deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);
		displayFramebuffer();
#ifdef FLYCAST_ENABLE_NEURAL
		endNeuralPerformanceFrame();
#endif
		drawOSD();
		renderVideoRouting();
		DX11Context::Instance()->setFrameRendered();
#else
		ID3D11RenderTargetView *nullView = nullptr;
		deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
		DX11Context::Instance()->presentFrame(fbTextureView, width, height);
#endif
		frameRendered = true;
		frameRenderedOnce = true;
		clearLastFrame = false;
	}

	return !is_rtt;
}

#ifdef FLYCAST_ENABLE_NEURAL
flycast::rend::neural::Rect DX11Renderer::getNeuralContentRect() const
{
	int outputWidth = settings.display.width;
	int outputHeight = settings.display.height;
	float renderAspect = aspectRatio;
	if (config::Rotate90)
	{
		std::swap(outputWidth, outputHeight);
		renderAspect = 1.f / renderAspect;
	}
	return flycast::rend::neural::ComputeContentRect(outputWidth, outputHeight,
		renderAspect, config::NeuralMatchOutputResolution
			&& flycast::rend::neural::UsesMatchOutputRaster(activeNeuralMode)
			? false : config::IntegerScale.get(),
		config::RenderResolution);
}

bool DX11Renderer::ensureNeuralResources()
{
	if (neuralDepthTextures[0] && neuralColor.textures[0] && neuralOverlayMask.textures[0]
		&& neuralDepthWidth == width && neuralDepthHeight == height)
		return true;
	releaseNeuralResources();
	DX11Context *context = DX11Context::Instance();
	const bool useD3D11On12 = activeNeuralSurface && context->isD3D11On12();
	auto createWrappedTexture = [&](const D3D12_RESOURCE_DESC& resourceDesc,
		D3D12_RESOURCE_STATES initialState, D3D12_RESOURCE_STATES outState,
		const D3D11_RESOURCE_FLAGS& flags, ComPtr<ID3D12Resource>& resource,
		ComPtr<ID3D11Texture2D>& texture) {
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		HRESULT result = context->getD3D12Device()->CreateCommittedResource(&heap,
			D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, nullptr,
			__uuidof(ID3D12Resource), reinterpret_cast<void **>(&resource.get()));
		if (SUCCEEDED(result))
			result = context->getD3D11On12Device()->CreateWrappedResource(resource, &flags,
				initialState, outState, __uuidof(ID3D11Texture2D),
				reinterpret_cast<void **>(&texture.get()));
		return result;
	};
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
	viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = 1;
	for (std::size_t i = 0; i < NeuralExportRingSize; ++i)
	{
		HRESULT result;
		if (useD3D11On12)
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Width = width;
			resourceDesc.Height = height;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			D3D11_RESOURCE_FLAGS flags{};
			flags.BindFlags = desc.BindFlags;
			result = createWrappedTexture(resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, flags,
				neuralDepthD3D12Resources[i], neuralDepthTextures[i]);
		}
		else
			result = device->CreateTexture2D(&desc, nullptr, &neuralDepthTextures[i].get());
		if (SUCCEEDED(result))
			result = device->CreateDepthStencilView(neuralDepthTextures[i], &depthDesc,
				&neuralDepthTargets[i].get());
		if (SUCCEEDED(result))
			result = device->CreateShaderResourceView(neuralDepthTextures[i], &viewDesc,
				&neuralDepthViews[i].get());
		if (FAILED(result))
		{
			WARN_LOG(RENDERER, "Neural R32 depth ring creation failed at slot %d: %x",
				static_cast<int>(i), result);
			releaseNeuralResources();
			return false;
		}
	}
	auto createTargetRing = [&](NeuralTargetRing& ring, DXGI_FORMAT format, const char *name,
		bool allowWrapped = true) {
		D3D11_TEXTURE2D_DESC targetDesc{};
		targetDesc.Width = width;
		targetDesc.Height = height;
		targetDesc.MipLevels = 1;
		targetDesc.ArraySize = 1;
		targetDesc.Format = format;
		targetDesc.SampleDesc.Count = 1;
		targetDesc.Usage = D3D11_USAGE_DEFAULT;
		targetDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		for (std::size_t i = 0; i < NeuralExportRingSize; ++i)
		{
			HRESULT result;
			if (useD3D11On12 && allowWrapped)
			{
				D3D12_RESOURCE_DESC resourceDesc{};
				resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				resourceDesc.Width = width;
				resourceDesc.Height = height;
				resourceDesc.DepthOrArraySize = 1;
				resourceDesc.MipLevels = 1;
				resourceDesc.Format = format;
				resourceDesc.SampleDesc.Count = 1;
				resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				D3D11_RESOURCE_FLAGS flags{};
				flags.BindFlags = targetDesc.BindFlags;
				result = createWrappedTexture(resourceDesc,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, flags,
					ring.d3d12Resources[i], ring.textures[i]);
			}
			else
				result = device->CreateTexture2D(&targetDesc, nullptr, &ring.textures[i].get());
			if (SUCCEEDED(result))
				result = device->CreateRenderTargetView(ring.textures[i], nullptr, &ring.targets[i].get());
			if (SUCCEEDED(result))
				result = device->CreateShaderResourceView(ring.textures[i], nullptr, &ring.views[i].get());
			if (FAILED(result))
			{
				WARN_LOG(RENDERER, "Neural %s ring creation failed at slot %d: %x",
					name, static_cast<int>(i), result);
				return false;
			}
		}
		return true;
	};
	if (!createTargetRing(neuralColor, DXGI_FORMAT_R8G8B8A8_UNORM, "RGBA color")
		|| !createTargetRing(neuralMotion, DXGI_FORMAT_R16G16_FLOAT, "motion")
		|| !createTargetRing(neuralMask, DXGI_FORMAT_R8_UNORM, "bias mask")
		|| !createTargetRing(neuralResolvedMask, DXGI_FORMAT_R8_UNORM, "resolved bias mask")
		|| !createTargetRing(neuralConfidence, DXGI_FORMAT_R8_UNORM, "confidence")
		|| !createTargetRing(neuralDrawId, DXGI_FORMAT_R16_UINT, "draw ID")
		|| !createTargetRing(neuralPreviousDrawId, DXGI_FORMAT_R16_UINT,
			"expected previous draw ID")
		|| !createTargetRing(neuralOverlayMask, DXGI_FORMAT_R8_UNORM,
			"overlay classification", false))
	{
		releaseNeuralResources();
		return false;
	}
	neuralDepthWidth = width;
	neuralDepthHeight = height;
	return true;
}

void DX11Renderer::releaseNeuralResources() noexcept
{
	releaseNeuralPresentation();
	releaseNeuralInputs();
	releaseNeuralHistory();
	neuralPresentationView.reset();
	neuralQualityCapturePublicView.reset();
	neuralQualityCapturePublicSlot = NeuralExportRingSize;
	neuralCaptureOnlyPublicView.reset();
	neuralCaptureOnlyPublicSlot = NeuralExportRingSize;
	auto releaseTargetRing = [](NeuralTargetRing& ring) {
		for (auto& view : ring.views) view.reset();
		for (auto& target : ring.targets) target.reset();
		for (auto& texture : ring.textures) texture.reset();
		for (auto& resource : ring.d3d12Resources) resource.reset();
	};
	releaseTargetRing(neuralColor);
	releaseTargetRing(neuralMotion);
	releaseTargetRing(neuralMask);
	releaseTargetRing(neuralResolvedMask);
	releaseTargetRing(neuralConfidence);
	releaseTargetRing(neuralDrawId);
	releaseTargetRing(neuralPreviousDrawId);
	releaseTargetRing(neuralOverlayMask);
	for (auto& view : neuralDepthViews) view.reset();
	for (auto& target : neuralDepthTargets) target.reset();
	for (auto& texture : neuralDepthTextures) texture.reset();
	for (auto& resource : neuralDepthD3D12Resources) resource.reset();
	for (auto& view : neuralOutputWrappedViews) view.reset();
	for (auto& texture : neuralOutputWrappedTextures) texture.reset();
	for (auto& resource : neuralOutputD3D12Resources) resource.reset();
	neuralDepthWidth = neuralDepthHeight = 0;
	neuralExportSlot = 0;
	neuralAcceptedGuidanceSlot = 0;
	hasNeuralAcceptedGuidance = false;
	neuralExportActive = false;
	neuralReactiveCoverageActive = false;
	neuralPreviousPositionBuffer.reset();
	neuralPreviousPositionBufferSize = 0;
}

bool DX11Renderer::renderNeuralExports()
{
	acquireNeuralInputs();
	ID3D11UnorderedAccessView *nullUavs[2]{};
	deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(
		D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr,
		1, static_cast<UINT>(std::size(nullUavs)), nullUavs, nullptr);
	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);
	deviceContext->OMSetRenderTargets(1, &neuralColor.targets[neuralExportSlot].get(), nullptr);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	const float black[4]{};
	deviceContext->ClearRenderTargetView(neuralColor.targets[neuralExportSlot], black);
	quad->draw(fbTextureView, samplers->getSampler(false));
	deviceContext->PSSetShaderResources(0, 1, &nullView);

	ID3D11RenderTargetView *targets[] = {
		neuralMotion.targets[neuralExportSlot].get(),
		neuralMask.targets[neuralExportSlot].get(),
		neuralConfidence.targets[neuralExportSlot].get(),
		neuralDrawId.targets[neuralExportSlot].get(),
		neuralPreviousDrawId.targets[neuralExportSlot].get(),
		neuralOverlayMask.targets[neuralExportSlot].get(),
	};
	deviceContext->OMSetRenderTargets(static_cast<UINT>(std::size(targets)), targets,
		neuralDepthTargets[neuralExportSlot]);
	const float masked[4] = {1.f, 1.f, 1.f, 1.f};
	deviceContext->ClearRenderTargetView(neuralMotion.targets[neuralExportSlot], black);
	deviceContext->ClearRenderTargetView(neuralMask.targets[neuralExportSlot], masked);
	deviceContext->ClearRenderTargetView(neuralConfidence.targets[neuralExportSlot], black);
	deviceContext->ClearRenderTargetView(neuralDrawId.targets[neuralExportSlot], black);
	deviceContext->ClearRenderTargetView(neuralPreviousDrawId.targets[neuralExportSlot], black);
	deviceContext->ClearRenderTargetView(neuralOverlayMask.targets[neuralExportSlot], black);
	deviceContext->ClearDepthStencilView(neuralDepthTargets[neuralExportSlot], D3D11_CLEAR_DEPTH, 0.f, 0);
	const auto previousPositions = neuralInstrumentation.PreviousPositions();
	const u32 previousPositionBytes = static_cast<u32>(previousPositions.size
		* sizeof(flycast::rend::neural::PreviousPosition));
	if (previousPositionBytes == 0 || !ensureBufferSize(neuralPreviousPositionBuffer,
		D3D11_BIND_VERTEX_BUFFER, neuralPreviousPositionBufferSize, previousPositionBytes))
	{
		WARN_LOG(RENDERER, "Neural previous-position stream allocation failed");
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		releaseNeuralInputs();
		return false;
	}
	D3D11_MAPPED_SUBRESOURCE previousMapped{};
	if (FAILED(deviceContext->Map(neuralPreviousPositionBuffer, 0,
		D3D11_MAP_WRITE_DISCARD, 0, &previousMapped)))
	{
		WARN_LOG(RENDERER, "Neural previous-position stream upload failed");
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		releaseNeuralInputs();
		return false;
	}
	memcpy(previousMapped.pData, previousPositions.data, previousPositionBytes);
	deviceContext->Unmap(neuralPreviousPositionBuffer, 0);
	configVertexShader();
	setupPixelShaderConstants();
	deviceContext->IASetInputLayout(neuralInputLayout);
	ID3D11Buffer *vertexBuffers[] = {vertexBuffer.get(), neuralPreviousPositionBuffer.get()};
	const unsigned int strides[] = {sizeof(Vertex), sizeof(flycast::rend::neural::PreviousPosition)};
	const unsigned int offsets[] = {0, 0};
	deviceContext->IASetVertexBuffers(0, static_cast<UINT>(std::size(vertexBuffers)),
		vertexBuffers, strides, offsets);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	neuralExportActive = true;
	RenderPass previousPass{};
	for (const RenderPass& currentPass : rendContext->render_passes)
	{
		drawList<ListType_Opaque, false>(rendContext->global_param_op,
			previousPass.op_count, currentPass.op_count - previousPass.op_count);
		drawList<ListType_Punch_Through, false>(rendContext->global_param_pt,
			previousPass.pt_count, currentPass.pt_count - previousPass.pt_count);
		previousPass = currentPass;
	}
	if (!renderNeuralReactiveCoverage())
	{
		neuralExportActive = false;
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		releaseNeuralInputs();
		return false;
	}
	const int overlayPolicy = std::clamp(config::NeuralOverlayPolicy.get(), 0, 2);
	if (overlayPolicy == 1)
	{
		const float protectedFrame[4] = {1.f, 1.f, 1.f, 1.f};
		deviceContext->ClearRenderTargetView(neuralOverlayMask.targets[neuralExportSlot],
			protectedFrame);
	}
	if (activeNeuralMode == static_cast<int>(flycast::rend::neural::NeuralMode::Dlss5Experimental)
		&& config::NeuralDlss5EvidenceCapture.get()
		&& !config::NeuralDlss5EvidencePreserveMask.get())
	{
		const float zero[4]{};
		deviceContext->ClearRenderTargetView(neuralMask.targets[neuralExportSlot], zero);
	}
	neuralExportActive = false;
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	if (!renderNeuralDisocclusion())
	{
		releaseNeuralInputs();
		return false;
	}
	releaseNeuralInputs();
	return true;
}

bool DX11Renderer::renderNeuralReactiveCoverage()
{
	ID3D11RenderTargetView *targets[] = {
		nullptr,
		neuralMask.targets[neuralExportSlot].get(),
		nullptr,
		nullptr,
		nullptr,
		neuralOverlayMask.targets[neuralExportSlot].get(),
	};
	deviceContext->OMSetRenderTargets(static_cast<UINT>(std::size(targets)), targets, nullptr);
	neuralReactiveCoverageActive = true;
	RenderPass previousPass{};
	for (const RenderPass& currentPass : rendContext->render_passes)
	{
		if (currentPass.sorted_tr_count > previousPass.sorted_tr_count)
			// Sorted PolyParam offsets are vertices, not indices. Replay the same
			// submitted triangle list as scene color, without authoritative depth.
			drawSorted(previousPass.sorted_tr_count,
				currentPass.sorted_tr_count - previousPass.sorted_tr_count, false);
		else
			drawList<ListType_Translucent, false>(rendContext->global_param_tr,
				previousPass.tr_count, currentPass.tr_count - previousPass.tr_count);
		previousPass = currentPass;
	}
	neuralReactiveCoverageActive = false;
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	return true;
}

bool DX11Renderer::mergeNeuralReactiveCoverage(ID3D11ShaderResourceView *coverageView)
{
	if (!coverageView)
		return true;
	ID3D11RenderTargetView *target = neuralMask.targets[neuralExportSlot].get();
	deviceContext->OMSetRenderTargets(1, &target, nullptr);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	const auto& shader = shaders->getNeuralReactiveCoveragePixelShader();
	if (!shader)
	{
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		return false;
	}
	quad->drawCustom(shader, &coverageView, 1);
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	return true;
}

bool DX11Renderer::renderNeuralDisocclusion()
{
	if (!hasNeuralAcceptedGuidance
		|| (activeNeuralMode == static_cast<int>(flycast::rend::neural::NeuralMode::Dlss5Experimental)
			&& config::NeuralDlss5EvidenceCapture.get()
			&& !config::NeuralDlss5EvidencePreserveMask.get()))
	{
		deviceContext->CopyResource(neuralResolvedMask.textures[neuralExportSlot],
			neuralMask.textures[neuralExportSlot]);
		return true;
	}
	ID3D11RenderTargetView *target = neuralResolvedMask.targets[neuralExportSlot];
	deviceContext->OMSetRenderTargets(1, &target, nullptr);
	const float masked[4] = {1.f, 1.f, 1.f, 1.f};
	deviceContext->ClearRenderTargetView(target, masked);
	acquireNeuralHistory();
	ID3D11ShaderResourceView *views[] = {
		neuralMask.views[neuralExportSlot],
		neuralDepthViews[neuralExportSlot],
		neuralMotion.views[neuralExportSlot],
		neuralConfidence.views[neuralExportSlot],
		neuralPreviousDrawId.views[neuralExportSlot],
		neuralDepthViews[neuralAcceptedGuidanceSlot],
		neuralDrawId.views[neuralAcceptedGuidanceSlot],
	};
	const auto& shader = shaders->getNeuralDisocclusionPixelShader();
	if (!shader)
	{
		releaseNeuralHistory();
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		return false;
	}
	quad->drawCustom(shader, views, static_cast<UINT>(std::size(views)));
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	releaseNeuralHistory();
	return true;
}

flycast::rend::neural::TextureRef DX11Renderer::getNeuralTexture(
	std::array<ComPtr<ID3D11Texture2D>, 3>& textures,
	std::array<ComPtr<ID3D11ShaderResourceView>, 3>& views,
	std::array<ComPtr<ID3D12Resource>, 3>& d3d12Resources, DXGI_FORMAT format)
{
	if (activeNeuralSurface && d3d12Resources[neuralExportSlot])
		return {flycast::rend::neural::TextureApi::D3D12,
			d3d12Resources[neuralExportSlot].get(), nullptr, static_cast<std::uint32_t>(format)};
	return {flycast::rend::neural::TextureApi::D3D11, textures[neuralExportSlot].get(),
		views[neuralExportSlot].get(), static_cast<std::uint32_t>(format)};
}

void DX11Renderer::acquireNeuralInputs()
{
	if (!activeNeuralSurface || neuralInputsAcquired)
		return;
	ID3D11Resource *resources[] = {
		neuralColor.textures[neuralExportSlot],
		neuralDepthTextures[neuralExportSlot],
		neuralMotion.textures[neuralExportSlot],
		neuralMask.textures[neuralExportSlot],
		neuralResolvedMask.textures[neuralExportSlot],
		neuralConfidence.textures[neuralExportSlot],
		neuralDrawId.textures[neuralExportSlot],
		neuralPreviousDrawId.textures[neuralExportSlot],
	};
	DX11Context::Instance()->AcquireWrappedResources(resources,
		static_cast<UINT>(std::size(resources)));
	neuralInputsAcquired = true;
}

void DX11Renderer::releaseNeuralInputs()
{
	if (!neuralInputsAcquired)
		return;
	ID3D11Resource *resources[] = {
		neuralColor.textures[neuralExportSlot],
		neuralDepthTextures[neuralExportSlot],
		neuralMotion.textures[neuralExportSlot],
		neuralMask.textures[neuralExportSlot],
		neuralResolvedMask.textures[neuralExportSlot],
		neuralConfidence.textures[neuralExportSlot],
		neuralDrawId.textures[neuralExportSlot],
		neuralPreviousDrawId.textures[neuralExportSlot],
	};
	DX11Context::Instance()->ReleaseWrappedResources(resources,
		static_cast<UINT>(std::size(resources)));
	neuralInputsAcquired = false;
}

void DX11Renderer::acquireNeuralHistory()
{
	if (!activeNeuralSurface || neuralHistoryAcquired || !hasNeuralAcceptedGuidance)
		return;
	ID3D11Resource *resources[] = {
		neuralDepthTextures[neuralAcceptedGuidanceSlot],
		neuralDrawId.textures[neuralAcceptedGuidanceSlot],
	};
	DX11Context::Instance()->AcquireWrappedResources(resources,
		static_cast<UINT>(std::size(resources)));
	neuralHistoryAcquired = true;
}

void DX11Renderer::releaseNeuralHistory()
{
	if (!neuralHistoryAcquired)
		return;
	ID3D11Resource *resources[] = {
		neuralDepthTextures[neuralAcceptedGuidanceSlot],
		neuralDrawId.textures[neuralAcceptedGuidanceSlot],
	};
	DX11Context::Instance()->ReleaseWrappedResources(resources,
		static_cast<UINT>(std::size(resources)));
	neuralHistoryAcquired = false;
}

void DX11Renderer::releaseNeuralPresentation()
{
	if (!neuralPresentationAcquired)
	{
		pendingNeuralPresentationFrameId = 0;
		return;
	}
	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);
	ID3D11Resource *resource = neuralOutputWrappedTextures[neuralPresentationSlot];
	DX11Context::Instance()->ReleaseWrappedResources(&resource, 1);
	neuralPresentationAcquired = false;
	neuralPresentationView.reset();
	pendingNeuralPresentationFrameId = 0;
}

bool DX11Renderer::ensureNeuralOutputWrapped(ID3D12Resource *resource, std::size_t& slot)
{
	if (!activeNeuralSurface || !resource || !DX11Context::Instance()->isD3D11On12())
		return false;
	slot = NeuralExportRingSize;
	for (std::size_t i = 0; i < NeuralExportRingSize; ++i)
	{
		if (neuralOutputD3D12Resources[i].get() == resource)
		{
			slot = i;
			break;
		}
		if (slot == NeuralExportRingSize && !neuralOutputD3D12Resources[i])
			slot = i;
	}
	if (slot == NeuralExportRingSize)
		return false;
	if (!neuralOutputD3D12Resources[slot])
	{
		resource->AddRef();
		neuralOutputD3D12Resources[slot].reset(resource);
		D3D11_RESOURCE_FLAGS flags{};
		flags.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		const auto state = static_cast<D3D12_RESOURCE_STATES>(
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			| D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		HRESULT result = DX11Context::Instance()->getD3D11On12Device()->CreateWrappedResource(
			resource, &flags, state, state, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void **>(&neuralOutputWrappedTextures[slot].get()));
		if (SUCCEEDED(result))
			result = device->CreateShaderResourceView(neuralOutputWrappedTextures[slot], nullptr,
				&neuralOutputWrappedViews[slot].get());
		if (FAILED(result))
		{
			WARN_LOG(RENDERER, "Neural D3D12 output wrapping failed: %x", result);
			neuralOutputWrappedViews[slot].reset();
			neuralOutputWrappedTextures[slot].reset();
			neuralOutputD3D12Resources[slot].reset();
			return false;
		}
	}
	return true;
}

bool DX11Renderer::wrapNeuralOutput(ID3D12Resource *resource, std::uint64_t frameId)
{
	releaseNeuralPresentation();
	std::size_t slot = NeuralExportRingSize;
	if (!ensureNeuralOutputWrapped(resource, slot))
		return false;
	ID3D11Resource *wrapped = neuralOutputWrappedTextures[slot];
	DX11Context::Instance()->AcquireWrappedResources(&wrapped, 1);
	neuralPresentationSlot = slot;
	neuralPresentationAcquired = true;
	neuralPresentationView = neuralOutputWrappedViews[slot];
	pendingNeuralPresentationFrameId = frameId;
	++neuralWrappedOutputCount;
	if (neuralWrappedOutputCount == 1)
		NOTICE_LOG(RENDERER,
			"DLSS 5 candidate public-output ready: frame=%llu route=d3d11on12 resource=%p; external mutation unconfirmed",
			static_cast<unsigned long long>(frameId), resource);
	return true;
}

bool DX11Renderer::retainNeuralOutputForCapture(ID3D12Resource *resource)
{
	neuralCaptureOnlyPublicView.reset();
	neuralCaptureOnlyPublicSlot = NeuralExportRingSize;
	std::size_t slot = NeuralExportRingSize;
	if (!ensureNeuralOutputWrapped(resource, slot))
		return false;
	neuralCaptureOnlyPublicView = neuralOutputWrappedViews[slot];
	neuralCaptureOnlyPublicSlot = slot;
	return true;
}

void DX11Renderer::logNeuralConsumerStatus(
	flycast::rend::neural::SubmitStatus status) noexcept
{
	using namespace flycast::rend::neural;
	if (activeNeuralMode != static_cast<int>(NeuralMode::Dlss5Experimental))
		return;
	const auto stats = neuralStage.GetStats();
	if (stats.evidenceCaptures != loggedEvidenceCaptures
		|| stats.evidenceCaptureFailures != loggedEvidenceCaptureFailures)
	{
		loggedEvidenceCaptures = stats.evidenceCaptures;
		loggedEvidenceCaptureFailures = stats.evidenceCaptureFailures;
		NOTICE_LOG(RENDERER,
			"DLSS 5 developer evidence: git_sha=%s captures=%llu failures=%llu frame=%llu "
			"color_fnv64=%016llX depth_fnv64=%016llX motion_fnv64=%016llX mask_fnv64=%016llX "
			"returned_fnv64=%016llX marked_fnv64=%016llX wait_us=%llu "
			"marker=32x32-magenta-cyan marker_presentation=%s; synchronous developer mode",
			GIT_HASH, static_cast<unsigned long long>(stats.evidenceCaptures),
			static_cast<unsigned long long>(stats.evidenceCaptureFailures),
			static_cast<unsigned long long>(stats.evidenceFrameId),
			static_cast<unsigned long long>(stats.evidenceInputHash),
			static_cast<unsigned long long>(stats.evidenceDepthHash),
			static_cast<unsigned long long>(stats.evidenceMotionHash),
			static_cast<unsigned long long>(stats.evidenceMaskHash),
			static_cast<unsigned long long>(stats.evidenceOutputHash),
			static_cast<unsigned long long>(stats.evidenceMarkedOutputHash),
			static_cast<unsigned long long>(stats.evidenceWaitMicroseconds),
			config::NeuralDlss5EvidencePresentMarker.get() ? "present" : "restored");
	}
	if (stats.dlss5Route == loggedDlss5Route
		&& stats.dlss5Readiness == loggedDlss5Readiness
		&& stats.compatibilityRebuildAttempts == loggedCompatibilityRebuildAttempts
		&& stats.dlss5ContractEvaluated == loggedDlss5ContractEvaluated)
		return;
	loggedDlss5Route = stats.dlss5Route;
	loggedDlss5Readiness = stats.dlss5Readiness;
	loggedCompatibilityRebuildAttempts = stats.compatibilityRebuildAttempts;
	loggedDlss5ContractEvaluated = stats.dlss5ContractEvaluated;
	NOTICE_LOG(RENDERER,
		"DLSS 5 consumer status: submit=%u route=%s readiness=%s contract_evaluated=%d "
		"rebuilds=%llu attempts=%llu failures=%llu rebuild_reason=%s detail=%s",
		static_cast<unsigned>(status), Dlss5HookRouteName(stats.dlss5Route),
		Dlss5HookReadinessName(stats.dlss5Readiness), stats.dlss5ContractEvaluated ? 1 : 0,
		static_cast<unsigned long long>(stats.compatibilityRebuilds),
		static_cast<unsigned long long>(stats.compatibilityRebuildAttempts),
		static_cast<unsigned long long>(stats.compatibilityRebuildFailures),
		Dlss5RebuildReasonName(stats.compatibilityRebuildReason), neuralStage.GetStatusReason());
}

void DX11Renderer::submitNeuralFrame()
{
	using namespace flycast::rend::neural;
	neuralQualityCapturePending = false;
	currentNeuralSourceFrameId = 0;
	releaseNeuralPresentation();
	neuralPresentationView.reset();
	if (!syncNeuralMode()) return;
	neuralQualityCapture.Configure(config::NeuralCaptureDirectory.get(),
		static_cast<std::uint32_t>(std::max(0, config::NeuralCaptureSkip.get())),
		static_cast<std::uint32_t>(std::clamp(config::NeuralCaptureFrames.get(), 0, 240)));
	const auto contentRect = getNeuralContentRect();
	neuralInstrumentation.SetOverlayGameId(settings.content.gameId);
	const auto& capturedFrame = neuralInstrumentation.CaptureGeometry(*rendContext, {}, {}, width, height,
		static_cast<std::uint32_t>(std::max(0, contentRect.width)),
		static_cast<std::uint32_t>(std::max(0, contentRect.height)), contentRect, {});
	currentNeuralSourceFrameId = capturedFrame.frameId;
	if (loggedNeuralRenderWidth != capturedFrame.renderWidth
		|| loggedNeuralRenderHeight != capturedFrame.renderHeight
		|| loggedNeuralOutputWidth != capturedFrame.outputWidth
		|| loggedNeuralOutputHeight != capturedFrame.outputHeight)
	{
		loggedNeuralRenderWidth = capturedFrame.renderWidth;
		loggedNeuralRenderHeight = capturedFrame.renderHeight;
		loggedNeuralOutputWidth = capturedFrame.outputWidth;
		loggedNeuralOutputHeight = capturedFrame.outputHeight;
		NOTICE_LOG(RENDERER,
			"Neural raster contract: input=%ux%u output=%ux%u content=(%d,%d %dx%d) match=%d",
			capturedFrame.renderWidth, capturedFrame.renderHeight,
			capturedFrame.outputWidth, capturedFrame.outputHeight,
			contentRect.x, contentRect.y, contentRect.width, contentRect.height,
			config::NeuralMatchOutputResolution && UsesMatchOutputRaster(activeNeuralMode) ? 1 : 0);
	}
	const auto qualityProfile = ResolveQualityProfile(config::NeuralQualityProfile.get(),
		config::NeuralStyleFamily.get());
	if (loggedQualityProfile != config::NeuralQualityProfile.get()
		|| loggedStyleFamily != config::NeuralStyleFamily.get())
	{
		loggedQualityProfile = config::NeuralQualityProfile.get();
		loggedStyleFamily = config::NeuralStyleFamily.get();
		NOTICE_LOG(RENDERER,
			"Neural quality profile: game=%s profile=%s style=%s external_recommendation=%s configuration_write=none",
			settings.content.gameId.c_str(), qualityProfile.name, qualityProfile.styleName,
			qualityProfile.externalRecommendation.c_str());
	}
	const bool bypass2DCandidate = activeNeuralMode == static_cast<int>(NeuralMode::Dlss5Experimental)
		&& (capturedFrame.predominantly2D || qualityProfile.bypassGenerative);
	const bool bypass2D = UpdateConservativeBypass(bypass2DCandidate,
		neural2DBypassActive, neural2DBypassEnterStreak, neural2DBypassExitStreak);
	if (bypass2D != neural2DBypassActive)
	{
		if (bypass2D)
			neuralInstrumentation.Discontinuity();
		neural2DBypassActive = bypass2D;
		NOTICE_LOG(RENDERER,
			"DLSS 5 conservative 2D/menu bypass: game=%s active=%d draws=%u",
			settings.content.gameId.c_str(), bypass2D ? 1 : 0,
			static_cast<unsigned>(capturedFrame.draws.size));
	}
	if (bypass2D)
		return;
	const int overlayPolicy = std::clamp(config::NeuralOverlayPolicy.get(), 0, 2);
	if (loggedOverlayPolicy != overlayPolicy || loggedOverlayGameId != settings.content.gameId)
	{
		loggedOverlayPolicy = overlayPolicy;
		loggedOverlayGameId = settings.content.gameId;
		const char *policyName = overlayPolicy == 0 ? "auto-high-confidence"
			: overlayPolicy == 1 ? "protect-full-frame" : "disabled";
		NOTICE_LOG(RENDERER,
			"Neural game overlay policy: game=%s policy=%s profile=%s per-title=%d",
			settings.content.gameId.c_str(), policyName,
			OverlayProfileName(capturedFrame.overlayProfile),
			(overlayPolicy != 0 || capturedFrame.overlayProfile != OverlayProfile::None) ? 1 : 0);
	}
	const bool overlayActive = overlayPolicy == 1
		|| (overlayPolicy == 0 && neuralInstrumentation.OverlayDrawCount() != 0);
	if (overlayActive != loggedOverlayActive)
	{
		loggedOverlayActive = overlayActive;
		NOTICE_LOG(RENDERER, "Neural protected overlay state: game=%s active=%d draws=%u",
			settings.content.gameId.c_str(), overlayActive ? 1 : 0,
			static_cast<unsigned>(neuralInstrumentation.OverlayDrawCount()));
	}
	if (!ensureNeuralResources()) return;
	neuralExportSlot = NextHistorySafeRingSlot(neuralExportSlot,
		neuralAcceptedGuidanceSlot, NeuralExportRingSize, hasNeuralAcceptedGuidance);
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::GuidanceBegin);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::GuidanceBegin);
	if (!renderNeuralExports()) return;
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::GuidanceEnd);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::GuidanceEnd);
	TextureRef color{TextureApi::D3D11, fbTex.get(), fbTextureView.get(),
		static_cast<std::uint32_t>(DXGI_FORMAT_B8G8R8A8_UNORM)};
	TextureRef depth{};
	TextureRef motion{};
	TextureRef mask{};
	TextureRef confidence{};
	TextureRef drawId{};
	if (neuralColor.textures[0])
	{
		color = getNeuralTexture(neuralColor.textures, neuralColor.views,
			neuralColor.d3d12Resources, DXGI_FORMAT_R8G8B8A8_UNORM);
		depth = getNeuralTexture(neuralDepthTextures, neuralDepthViews,
			neuralDepthD3D12Resources, DXGI_FORMAT_R32_FLOAT);
		motion = getNeuralTexture(neuralMotion.textures, neuralMotion.views,
			neuralMotion.d3d12Resources, DXGI_FORMAT_R16G16_FLOAT);
		mask = getNeuralTexture(neuralResolvedMask.textures, neuralResolvedMask.views,
			neuralResolvedMask.d3d12Resources, DXGI_FORMAT_R8_UNORM);
		confidence = getNeuralTexture(neuralConfidence.textures, neuralConfidence.views,
			neuralConfidence.d3d12Resources, DXGI_FORMAT_R8_UNORM);
		drawId = getNeuralTexture(neuralDrawId.textures, neuralDrawId.views,
			neuralDrawId.d3d12Resources, DXGI_FORMAT_R16_UINT);
	}
	const auto& frame = neuralInstrumentation.AttachTextures(color, depth, motion, mask, confidence, drawId);
	neuralQualityCaptureMetadata = {};
	neuralQualityCaptureMetadata.frameId = frame.frameId;
	neuralQualityCaptureMetadata.historyGeneration = frame.historyGeneration;
	neuralQualityCaptureMetadata.historyAge = frame.historyAge;
	neuralQualityCaptureMetadata.skippedFrameCount = frame.skippedFrameCount;
	neuralQualityCaptureMetadata.renderWidth = frame.renderWidth;
	neuralQualityCaptureMetadata.renderHeight = frame.renderHeight;
	neuralQualityCaptureMetadata.outputWidth = frame.outputWidth;
	neuralQualityCaptureMetadata.outputHeight = frame.outputHeight;
	neuralQualityCaptureMetadata.screenWidth = frame.screenWidth;
	neuralQualityCaptureMetadata.screenHeight = frame.screenHeight;
	neuralQualityCaptureMetadata.drawCount = static_cast<std::uint32_t>(frame.draws.size);
	neuralQualityCaptureMetadata.correspondence = frame.correspondence;
	neuralQualityCaptureMetadata.contentRect = frame.contentRect;
	neuralQualityCaptureMetadata.historyValid = frame.historyValid;
	neuralQualityCaptureMetadata.resetHistory = frame.resetHistory;
	neuralQualityCaptureMetadata.sceneCut = frame.sceneCut;
	neuralQualityCaptureMetadata.truncated = frame.truncated;
	neuralQualityCaptureMetadata.predominantly2D = frame.predominantly2D;
	neuralQualityCaptureMetadata.d3d11On12 = activeNeuralSurface;
	neuralQualityCaptureMetadata.oitRenderer = IsOitRenderer();
	neuralQualityCaptureMetadata.neuralMode = activeNeuralMode;
	neuralQualityCaptureMetadata.dlssPreset = activeNeuralPreset;
	neuralQualityCaptureMetadata.overlayPolicy = overlayPolicy;
	neuralQualityCaptureMetadata.overlayProfile = frame.overlayProfile;
	neuralQualityCaptureMetadata.gameId = settings.content.gameId;
	neuralQualityCaptureMetadata.profile = std::string(qualityProfile.name) + " / "
		+ qualityProfile.styleName;
	neuralQualityCaptureMetadata.externalRecommendation =
		qualityProfile.externalRecommendation;
	if (neuralQualityCapture.CapturesCurrentFrame())
		neuralQualityCaptureMetadata.overlayDraws = neuralInstrumentation.CaptureOverlayDiagnostics();
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::EvaluateBegin);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::EvaluateBegin);
	const auto status = neuralStage.TrySubmit(frame);
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::EvaluateEnd);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::EvaluateEnd);
	neuralPerformance.RecordEvaluation(frame.frameId, status == SubmitStatus::Submitted);
	neuralQualityCaptureMetadata.evaluationAccepted = status == SubmitStatus::Submitted;
	neuralQualityCaptureMetadata.submitStatus = neuralStage.GetStatusReason();
	logNeuralConsumerStatus(status);
	if (status == SubmitStatus::Submitted)
	{
		neuralInstrumentation.MarkEvaluated(frame.frameId);
		neuralAcceptedGuidanceSlot = neuralExportSlot;
		hasNeuralAcceptedGuidance = true;
		const auto stats = neuralStage.GetStats();
		neuralQualityCaptureMetadata.externalContractEvaluated =
			stats.dlss5Readiness == Dlss5HookReadiness::ContractEvaluated;
		if (activeNeuralMode == static_cast<int>(NeuralMode::Dlss5Experimental)
			&& stats.dlss5Readiness != Dlss5HookReadiness::ContractEvaluated)
		{
			neuralQualityCapturePending = neuralQualityCapture.WantsFrame();
			if (neuralQualityCapturePending && activeNeuralSurface
				&& config::NeuralDlss5EvidenceCapture.get()
				&& !config::NeuralDlss5EvidencePresentMarker.get())
			{
				const auto output = neuralStage.GetOutput();
				if (output.api == TextureApi::D3D12 && output.resource)
					retainNeuralOutputForCapture(
						static_cast<ID3D12Resource *>(output.resource));
			}
			return;
		}
		const auto output = neuralStage.GetOutput();
		if (output.api == TextureApi::D3D11 && output.view)
		{
			auto *view = static_cast<ID3D11ShaderResourceView *>(output.view);
			view->AddRef();
			neuralPresentationView.reset(view);
			pendingNeuralPresentationFrameId = frame.frameId;
		}
		else if (output.api == TextureApi::D3D12 && output.resource)
			wrapNeuralOutput(static_cast<ID3D12Resource *>(output.resource), frame.frameId);
	}
	neuralQualityCapturePending = neuralQualityCapture.WantsFrame();
}

void DX11Renderer::captureNeuralQualityFrame()
{
	if (!neuralQualityCapturePending)
		return;
	neuralQualityCapturePending = false;
	bool captureOutputAcquired = false;
	if (activeNeuralSurface && neuralQualityCapturePublicView
		&& neuralQualityCapturePublicSlot < NeuralExportRingSize)
	{
		ID3D11Resource *resource = neuralOutputWrappedTextures[neuralQualityCapturePublicSlot];
		DX11Context::Instance()->AcquireWrappedResources(&resource, 1);
		captureOutputAcquired = true;
	}
	acquireNeuralInputs();
	ID3D11Texture2D *publicOutput = nullptr;
	// Passthrough presents the source texture; it is not a public-NGX result and
	// must not be labeled as public DLAA in a quality package.
	if (activeNeuralMode != static_cast<int>(flycast::rend::neural::NeuralMode::Passthrough)
		&& neuralQualityCapturePublicView)
	{
		ID3D11Resource *resource = nullptr;
		neuralQualityCapturePublicView->GetResource(&resource);
		if (resource)
		{
			resource->QueryInterface(__uuidof(ID3D11Texture2D),
				reinterpret_cast<void **>(&publicOutput));
			resource->Release();
		}
	}
	ID3D11Texture2D *finalComposite = nullptr;
	ID3D11Resource *finalResource = nullptr;
	DX11Context::Instance()->getRenderTarget()->GetResource(&finalResource);
	if (finalResource)
	{
		finalResource->QueryInterface(__uuidof(ID3D11Texture2D),
			reinterpret_cast<void **>(&finalComposite));
		finalResource->Release();
	}
	flycast::rend::neural::QualityCaptureTextures textures;
	textures.nativeColor = fbTex;
	textures.sourceColor = neuralColor.textures[neuralExportSlot];
	textures.depth = neuralDepthTextures[neuralExportSlot];
	textures.motion = neuralMotion.textures[neuralExportSlot];
	textures.biasMask = neuralResolvedMask.textures[neuralExportSlot];
	textures.confidence = neuralConfidence.textures[neuralExportSlot];
	textures.drawId = neuralDrawId.textures[neuralExportSlot];
	textures.overlay = neuralOverlayMask.textures[neuralExportSlot];
	textures.publicOutput = publicOutput;
	textures.finalComposite = finalComposite;
	std::string error;
	const auto beforeCount = neuralQualityCapture.CapturedCount();
	const bool captured = neuralQualityCapture.Capture(device, deviceContext,
		neuralQualityCaptureMetadata, textures, error);
	const auto afterCount = neuralQualityCapture.CapturedCount();
	if (publicOutput) publicOutput->Release();
	if (finalComposite) finalComposite->Release();
	releaseNeuralInputs();
	if (captureOutputAcquired)
	{
		ID3D11Resource *resource = neuralOutputWrappedTextures[neuralQualityCapturePublicSlot];
		DX11Context::Instance()->ReleaseWrappedResources(&resource, 1);
	}
	neuralQualityCapturePublicView.reset();
	neuralQualityCapturePublicSlot = NeuralExportRingSize;
	neuralCaptureOnlyPublicView.reset();
	neuralCaptureOnlyPublicSlot = NeuralExportRingSize;
	if (!captured)
		WARN_LOG(RENDERER, "Neural quality capture failed: %s", error.c_str());
	else if (afterCount != beforeCount)
		NOTICE_LOG(RENDERER,
			"Neural quality capture: game=%s frame=%llu captured=%u submit=%s synchronous-developer-only",
			settings.content.gameId.c_str(),
			static_cast<unsigned long long>(neuralQualityCaptureMetadata.frameId),
			afterCount, neuralQualityCaptureMetadata.submitStatus.c_str());
}

void DX11Renderer::beginNeuralPerformanceFrame()
{
	const bool synchronousCapture = !config::NeuralCaptureDirectory.get().empty()
		&& config::NeuralCaptureFrames.get() > 0;
	neuralQualityCapture.Configure(config::NeuralCaptureDirectory.get(),
		static_cast<std::uint32_t>(std::max(0, config::NeuralCaptureSkip.get())),
		static_cast<std::uint32_t>(std::clamp(config::NeuralCaptureFrames.get(), 0, 240)));
	neuralQualityCaptureGpuTimer.Configure(device, synchronousCapture);
	neuralQualityCaptureGpuTimer.BeginFrame(deviceContext,
		neuralQualityCapture.CapturesCurrentFrame());
	neuralPerformance.Configure(device,
		synchronousCapture ? std::filesystem::path{} :
			std::filesystem::path(config::NeuralPerformanceDirectory.get()),
		static_cast<std::uint32_t>(std::max(0, config::NeuralPerformanceWarmup.get())),
		static_cast<std::uint32_t>(std::clamp(config::NeuralPerformanceFrames.get(), 0, 10000)),
		settings.content.gameId,
		DX11Context::Instance()->isD3D11On12() ? "d3d11on12" : "d3d11",
		IsOitRenderer() ? "dx11-oit" : "dx11", config::NeuralMode.get(),
		std::clamp(config::NeuralFailureInjection.get(), 0, 5),
		static_cast<std::uint32_t>(std::clamp(
			config::NeuralFailureInjectionCount.get(), 0, 10000)),
		static_cast<std::uint32_t>(std::clamp(
			config::NeuralFailureInjectionAfter.get(), 0, 10000)));
	neuralPerformance.BeginFrame(deviceContext);
}

void DX11Renderer::markNeuralPvrEnd()
{
	neuralPerformance.Mark(deviceContext,
		flycast::rend::neural::GpuTimingPoint::PvrEnd);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		flycast::rend::neural::CaptureGpuTimingPoint::PvrEnd);
}

void DX11Renderer::endNeuralPerformanceFrame()
{
	neuralPerformance.EndFrame(deviceContext, neuralStage.GetStats(),
		neuralResourceObjectCount());
}

std::uint32_t DX11Renderer::neuralResourceObjectCount() const noexcept
{
	std::uint32_t count = neuralPreviousPositionBuffer ? 1u : 0u;
	auto countArray = [&count](const auto& objects) {
		for (const auto& object : objects) count += object ? 1u : 0u;
	};
	auto countTargetRing = [&countArray](const NeuralTargetRing& ring) {
		countArray(ring.textures);
		countArray(ring.targets);
		countArray(ring.views);
		countArray(ring.d3d12Resources);
	};
	countArray(neuralDepthTextures);
	countArray(neuralDepthTargets);
	countArray(neuralDepthViews);
	countArray(neuralDepthD3D12Resources);
	countTargetRing(neuralColor);
	countTargetRing(neuralMotion);
	countTargetRing(neuralMask);
	countTargetRing(neuralResolvedMask);
	countTargetRing(neuralConfidence);
	countTargetRing(neuralDrawId);
	countTargetRing(neuralPreviousDrawId);
	countTargetRing(neuralOverlayMask);
	countArray(neuralOutputD3D12Resources);
	countArray(neuralOutputWrappedTextures);
	countArray(neuralOutputWrappedViews);
	return count;
}

bool DX11Renderer::syncNeuralMode()
{
	using namespace flycast::rend::neural;
	const int requestedMode = std::clamp(config::NeuralMode.get(), 0, 8);
	if (requestedMode == static_cast<int>(NeuralMode::Dlss5Experimental)
		&& config::NeuralDlss5EvidenceCapture.get())
	{
		const auto delayMs = static_cast<std::uint64_t>(
			std::clamp(config::NeuralDlss5EvidenceStartDelayMs.get(), 0, 30000));
		if (delayMs != 0)
		{
			if (neuralEvidenceArmDeadlineMs == 0)
			{
				neuralEvidenceArmDeadlineMs = GetTickCount64() + delayMs;
				NOTICE_LOG(RENDERER,
					"DLSS 5 developer evidence arming delayed by %llu ms; native presentation retained",
					static_cast<unsigned long long>(delayMs));
			}
			if (GetTickCount64() < neuralEvidenceArmDeadlineMs)
				return false;
		}
	}
	else
		neuralEvidenceArmDeadlineMs = 0;
	const bool surfaceRequested = config::NeuralD3D12Surface.get();
	const bool requestedSurface = surfaceRequested && DX11Context::Instance()->isD3D11On12();
	const int configuredPreset = config::NeuralDlssPreset.get();
	const int requestedPreset = configuredPreset == 10 || configuredPreset == 11
		? configuredPreset : 0;
	const int requestedFailureInjection = std::clamp(config::NeuralFailureInjection.get(), 0, 5);
	const int requestedFailureInjectionCount = requestedFailureInjection == 0 ? 0
		: std::clamp(config::NeuralFailureInjectionCount.get(), 0, 10000);
	const int requestedFailureInjectionAfter = requestedFailureInjection == 0 ? 0
		: std::clamp(config::NeuralFailureInjectionAfter.get(), 0, 10000);
	const bool synchronousCapture = !config::NeuralCaptureDirectory.get().empty()
		&& config::NeuralCaptureFrames.get() > 0;
	const bool requestedGpuTiming = synchronousCapture
		|| (!config::NeuralPerformanceDirectory.get().empty()
			&& config::NeuralPerformanceFrames.get() > 0);
	if (requestedMode != activeNeuralMode || requestedSurface != activeNeuralSurface
		|| requestedPreset != activeNeuralPreset
		|| requestedFailureInjection != activeNeuralFailureInjection
		|| requestedFailureInjectionCount != activeNeuralFailureInjectionCount
		|| requestedFailureInjectionAfter != activeNeuralFailureInjectionAfter
		|| requestedGpuTiming != activeNeuralGpuTiming)
	{
		releaseNeuralResources();
		neuralPresentationView.reset();
		activeNeuralMode = requestedMode;
		activeNeuralPreset = requestedPreset;
		activeNeuralFailureInjection = requestedFailureInjection;
		activeNeuralFailureInjectionCount = requestedFailureInjectionCount;
		activeNeuralFailureInjectionAfter = requestedFailureInjectionAfter;
		activeNeuralSurface = requestedSurface;
		activeNeuralGpuTiming = requestedGpuTiming;
		neuralInstrumentation.SetEnabled(requestedMode != 0);
		neuralStage.Shutdown();
		StageConfig stageConfig;
		stageConfig.mode = static_cast<NeuralMode>(requestedMode);
		stageConfig.api = requestedSurface ? Api::D3D12 : Api::D3D11;
		stageConfig.dlssPreset = static_cast<std::uint32_t>(requestedPreset);
		stageConfig.failureInjection = static_cast<FailureInjection>(requestedFailureInjection);
		stageConfig.failureInjectionCount = static_cast<std::uint32_t>(
			requestedFailureInjectionCount);
		stageConfig.failureInjectionAfter = static_cast<std::uint32_t>(
			requestedFailureInjectionAfter);
		stageConfig.performanceGpuTiming = requestedGpuTiming;
		stageConfig.hookCompatibility = stageConfig.mode == NeuralMode::DlaaHook
			|| stageConfig.mode == NeuralMode::Dlss5Experimental;
		if (stageConfig.mode == NeuralMode::Dlss5Experimental)
		{
			stageConfig.dlss5Route = requestedSurface ? Dlss5HookRoute::D3D11On12
				: Dlss5HookRoute::D3D11External;
			stageConfig.dlss5RebuildGraceEvaluations = static_cast<std::uint32_t>(
				std::max(0, config::NeuralDlss5RebuildGraceEvaluations.get()));
			stageConfig.dlss5RebuildMaxAttempts = static_cast<std::uint32_t>(
				std::clamp(config::NeuralDlss5RebuildMaxAttempts.get(), 0, 4));
			stageConfig.dlss5EvidenceCapture = config::NeuralDlss5EvidenceCapture.get();
			stageConfig.dlss5EvidenceCaptureFrames = static_cast<std::uint32_t>(
				std::clamp(config::NeuralDlss5EvidenceCaptureFrames.get(), 1, 480));
			stageConfig.dlss5EvidenceStartFrame = static_cast<std::uint64_t>(
				std::max(0, config::NeuralDlss5EvidenceStartFrame.get()));
			stageConfig.dlss5EvidencePresentMarker =
				config::NeuralDlss5EvidencePresentMarker.get();
		}
		neuralStage = NeuralStage(stageConfig);
		NOTICE_LOG(RENDERER, "Public DLSS preset: %s (%d); external Neural Rendering model selection is independent",
			requestedPreset == 10 ? "J" : requestedPreset == 11 ? "K" : "Auto",
			requestedPreset);
		if (stageConfig.api == Api::D3D11)
			neuralStage.SetGraphicsDevice(stageConfig.api, device.get(), deviceContext.get());
		else
			neuralStage.SetGraphicsDevice(stageConfig.api,
				DX11Context::Instance()->getD3D12Device(),
				DX11Context::Instance()->getD3D12Queue());
		if (surfaceRequested && !requestedSurface)
			WARN_LOG(RENDERER, "D3D11On12 neural surface was requested after native D3D11 initialization; restart the renderer to activate it");
		if (requestedMode == 0)
			releaseNeuralResources();
	}
	return neuralInstrumentation.IsEnabled();
}

void DX11Renderer::submitNeuralFramebuffer()
{
	using namespace flycast::rend::neural;
	releaseNeuralPresentation();
	neuralPresentationView.reset();
	if (!syncNeuralMode()) return;
	const TextureRef color{TextureApi::D3D11, fbTex.get(), fbTextureView.get(),
		static_cast<std::uint32_t>(DXGI_FORMAT_B8G8R8A8_UNORM)};
	const auto contentRect = getNeuralContentRect();
	const auto& frame = neuralInstrumentation.CaptureSource(FrameSource::FramebufferDirect,
		color, width, height,
		static_cast<std::uint32_t>(std::max(0, contentRect.width)),
		static_cast<std::uint32_t>(std::max(0, contentRect.height)), contentRect);
	const auto status = neuralStage.TrySubmit(frame);
	logNeuralConsumerStatus(status);
	if (status == SubmitStatus::Submitted)
	{
		neuralInstrumentation.MarkEvaluated(frame.frameId);
		const auto stats = neuralStage.GetStats();
		if (activeNeuralMode == static_cast<int>(NeuralMode::Dlss5Experimental)
			&& stats.dlss5Readiness != Dlss5HookReadiness::ContractEvaluated)
			return;
		const auto output = neuralStage.GetOutput();
		if (output.api == TextureApi::D3D11 && output.view)
		{
			auto *view = static_cast<ID3D11ShaderResourceView *>(output.view);
			view->AddRef();
			neuralPresentationView.reset(view);
		}
		else if (output.api == TextureApi::D3D12 && output.resource)
			wrapNeuralOutput(static_cast<ID3D12Resource *>(output.resource), frame.frameId);
	}
}
#endif

void DX11Renderer::displayFramebuffer()
{
#ifndef LIBRETRO
#ifdef FLYCAST_ENABLE_NEURAL
	neuralPerformance.Mark(deviceContext,
		flycast::rend::neural::GpuTimingPoint::CompositeBegin);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		flycast::rend::neural::CaptureGpuTimingPoint::CompositeBegin);
#endif
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)settings.display.width;
	vp.Height = (FLOAT)settings.display.height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);

	const D3D11_RECT r = { 0, 0, settings.display.width, settings.display.height };
	deviceContext->RSSetScissorRects(1, &r);
	float colors[4];
	VO_BORDER_COL.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(DX11Context::Instance()->getRenderTarget(), colors);

	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);
	shiftX *=  2.f / width;
	shiftY *=  -2.f / height;

	int outwidth = settings.display.width;
	int outheight = settings.display.height;
	float renderAR = aspectRatio;
	if (config::Rotate90) {
		std::swap(outwidth, outheight);
		std::swap(shiftX, shiftY);
		renderAR = 1 / renderAR;
	}
	
	int dy = 0;
	int dx = 0;
	// handles the rotation on its own, so never pass config::Rotate90
	getWindowboxDimensions(outwidth, outheight, renderAR, dx, dy, false);
	
	float x = (float)dx;
	float y = (float)dy;
	float w = (float)(outwidth - 2 * dx);
	float h = (float)(outheight - 2 * dy);

	// Normalize
	x = x * 2.f / outwidth - 1.f;
	w *= 2.f / outwidth;
	y = y * 2.f / outheight - 1.f;
	h *= 2.f / outheight;
	// Shift
	x += shiftX;
	y += shiftY;
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	ComPtr<ID3D11ShaderResourceView>& presentationView =
#ifdef FLYCAST_ENABLE_NEURAL
		neuralPresentationView ? neuralPresentationView : fbTextureView;
#else
		fbTextureView;
#endif
	const bool queuedNeuralOutput =
#ifdef FLYCAST_ENABLE_NEURAL
		neuralPresentationAcquired;
	const auto queuedNeuralFrameId = pendingNeuralPresentationFrameId;
	const auto displayedNeuralFrameId = neuralPresentationView
		? pendingNeuralPresentationFrameId : 0;
#else
		false;
#endif
	quad->draw(presentationView, samplers->getSampler(config::LinearInterpolation), nullptr,
		x, y, w, h, config::Rotate90);
#ifdef FLYCAST_ENABLE_NEURAL
	if (neuralPresentationView && config::NeuralOverlayPolicy.get() != 2
		&& neuralOverlayMask.views[neuralExportSlot])
	{
		const auto& shader = shaders->getNeuralOverlayCompositePixelShader();
		if (shader)
		{
			deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
			ID3D11ShaderResourceView *views[] = {
				fbTextureView.get(), neuralOverlayMask.views[neuralExportSlot].get()
			};
			quad->drawCustom(shader, views, static_cast<UINT>(std::size(views)),
				samplers->getSampler(config::LinearInterpolation), x, y, w, h,
				config::Rotate90);
		}
	}
	neuralPerformance.Mark(deviceContext,
		flycast::rend::neural::GpuTimingPoint::CompositeEnd);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		flycast::rend::neural::CaptureGpuTimingPoint::CompositeEnd);
	neuralPerformance.StagePresentation(currentNeuralSourceFrameId,
		displayedNeuralFrameId);
	neuralQualityCapturePublicView.reset();
	neuralQualityCapturePublicSlot = NeuralExportRingSize;
	if (neuralQualityCapturePending && neuralPresentationView)
	{
		neuralQualityCapturePublicView = neuralPresentationView;
		if (neuralPresentationAcquired)
			neuralQualityCapturePublicSlot = neuralPresentationSlot;
	}
	else if (neuralQualityCapturePending && neuralCaptureOnlyPublicView)
	{
		neuralQualityCapturePublicView = neuralCaptureOnlyPublicView;
		neuralQualityCapturePublicSlot = neuralCaptureOnlyPublicSlot;
	}
	if (queuedNeuralOutput)
	{
		++neuralAcceptedBlitCount;
		DX11Context::Instance()->QueueNeuralOutputPresent(queuedNeuralFrameId);
		if (neuralAcceptedBlitCount == 1)
			NOTICE_LOG(RENDERER,
				"DLSS 5 candidate public-output blit queued: frame=%llu route=d3d11on12; external mutation unconfirmed",
				static_cast<unsigned long long>(queuedNeuralFrameId));
		pendingNeuralPresentationFrameId = 0;
	}
	releaseNeuralPresentation();
	const bool capturedGpuTiming = neuralQualityCaptureGpuTimer.EndAndResolve(deviceContext,
		neuralQualityCaptureMetadata.gpuTimings);
	if (!neuralQualityCaptureMetadata.evaluationAccepted || activeNeuralSurface)
		neuralQualityCaptureMetadata.gpuTimings.evaluateAvailable = false;
	if (activeNeuralSurface && capturedGpuTiming
		&& neuralQualityCaptureMetadata.evaluationAccepted)
	{
		const auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(500);
		while (std::chrono::steady_clock::now() < deadline)
		{
			neuralStage.PollCompletedGpuTiming();
			const auto timingStats = neuralStage.GetStats();
			if (timingStats.evaluateGpuFrameId == neuralQualityCaptureMetadata.frameId)
			{
				neuralQualityCaptureMetadata.gpuTimings.evaluateAvailable = true;
				neuralQualityCaptureMetadata.gpuTimings.evaluateMs = timingStats.evaluateGpuMs;
				neuralQualityCaptureMetadata.gpuTimings.available = true;
				break;
			}
			std::this_thread::yield();
		}
	}
	captureNeuralQualityFrame();
#endif
#endif
}

void DX11Renderer::setCullMode(int mode)
{
	ComPtr<ID3D11RasterizerState> rasterizer;
	switch (mode)
	{
	case 0:
	case 1:
	default:
		rasterizer = rasterCullNone;
		break;
	case 2:
		rasterizer = rasterCullFront;
		break;
	case 3:
		rasterizer = rasterCullBack;
		break;
	}
	deviceContext->RSSetState(rasterizer);
}

TileClipping DX11Renderer::setTileClip(u32 tileclip, Rect& clip_rect)
{
	TileClipping clipmode = matrices.getTileClip(tileclip, clip_rect);
	if (clipmode == TileClipping::Outside) {
		RECT rect { clip_rect.origin.x, clip_rect.origin.y,
			clip_rect.origin.x + clip_rect.size.x, clip_rect.origin.y + clip_rect.size.y };
		deviceContext->RSSetScissorRects(1, &rect);
	}
	else {
		deviceContext->RSSetScissorRects(1, &scissorRect);
	}
	return clipmode;
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::setRenderState(const PolyParam *gp, u32 neuralOrdinalOverride)
{
	PixelPolyConstants constants{};
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		constants.trilinearAlpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			constants.trilinearAlpha = 1.f - constants.trilinearAlpha;
	}
	else
		constants.trilinearAlpha = 1.f;

#ifdef FLYCAST_ENABLE_NEURAL
	if (neuralExportActive)
	{
		std::size_t ordinal = 0;
		if constexpr (Type == ListType_Opaque)
			ordinal = static_cast<std::size_t>(gp - rendContext->global_param_op.data());
		else if constexpr (Type == ListType_Punch_Through)
			ordinal = rendContext->global_param_op.size()
				+ static_cast<std::size_t>(gp - rendContext->global_param_pt.data());
		else
			ordinal = rendContext->global_param_op.size() + rendContext->global_param_pt.size()
				+ static_cast<std::size_t>(gp - rendContext->global_param_tr.data());
		if (neuralOrdinalOverride != ~0u)
			ordinal = neuralOrdinalOverride;
		constants.neuralDrawId = static_cast<std::uint32_t>(ordinal + 1);
		const int overlayPolicy = std::clamp(config::NeuralOverlayPolicy.get(), 0, 2);
		constants.neuralOverlayMask = overlayPolicy == 0
			&& neuralInstrumentation.IsOverlayOrdinal(ordinal) ? 1.f : 0.f;
		if (neuralReactiveCoverageActive)
		{
			constants.neuralConfidence = 0.f;
			constants.neuralBiasMask = 1.f;
			constants.neuralPreviousDrawId = 0;
		}
		else
		{
			const auto *match = neuralInstrumentation.MatchForOrdinal(ordinal);
			constants.neuralConfidence = match ? match->confidence : 0.f;
			constants.neuralBiasMask = constants.neuralConfidence >= .5f ? 0.f : 1.f;
			constants.neuralPreviousDrawId = match && constants.neuralConfidence >= .5f
				? static_cast<std::uint32_t>(match->prevOrdinal + 1) : 0;
		}
	}
#endif

	bool color_clamp = gp->tsp.ColorClamp && (rendContext->fog_clamp_min.full != 0 || rendContext->fog_clamp_max.full != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	Rect clip_rect;
	TileClipping clipmode = setTileClip(gp->tileclip, clip_rect);
	DX11Texture *texture = (DX11Texture *)gp->texture;
	int gpuPalette = texture == nullptr || !texture->gpuPalette ? 0
			: gp->tsp.FilterMode + 1;
	if (gpuPalette != 0)
	{
		if (config::TextureFiltering == 1)
			gpuPalette = 1; // force nearest
		else if (config::TextureFiltering == 2)
			gpuPalette = 2; // force linear
	}

	ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(gp->pcw.Gouraud,
		gp->isNaomi2()
#ifdef FLYCAST_ENABLE_NEURAL
		, neuralExportActive
#endif
		);
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
	ComPtr<ID3D11PixelShader> pixelShader = shaders->getShader(
			gp->pcw.Texture,
			gp->tsp.UseAlpha,
			gp->tsp.IgnoreTexA || gp->tcw.PixelFmt == Pixel565,
			gp->tsp.ShadInstr,
			gp->pcw.Offset,
			fog_ctrl,
			gp->tcw.PixelFmt == PixelBumpMap,
			color_clamp,
			constants.trilinearAlpha != 1.f,
			gpuPalette,
			gp->pcw.Gouraud,
			Type == ListType_Punch_Through,
			clipmode == TileClipping::Inside,
			dithering
#ifdef FLYCAST_ENABLE_NEURAL
			, neuralExportActive
#endif
			);
	deviceContext->PSSetShader(pixelShader, nullptr, 0);

	if (gpuPalette != 0)
	{
		if (gp->tcw.PixelFmt == PixelPal4)
			constants.paletteIndex = (float)(gp->tcw.PalSelect << 4);
		else
			constants.paletteIndex = (float)((gp->tcw.PalSelect >> 4) << 8);
	}

	if (clipmode == TileClipping::Inside)
	{
		constants.clipTest[0] = (float)clip_rect.origin.x;
		constants.clipTest[1] = (float)clip_rect.origin.y;
		constants.clipTest[2] = (float)clip_rect.bottomRight().x;
		constants.clipTest[3] = (float)clip_rect.bottomRight().y;
	}
	if (constants.trilinearAlpha != 1.f || gpuPalette != 0 || clipmode == TileClipping::Inside
#ifdef FLYCAST_ENABLE_NEURAL
		|| neuralExportActive
#endif
		)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(pxlPolyConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, &constants, sizeof(constants));
		deviceContext->Unmap(pxlPolyConstants, 0);
	}

	if (texture != nullptr)
	{
        deviceContext->PSSetShaderResources(0, 1, &texture->textureView.get());
		bool linearFiltering;
		if (gpuPalette != 0)
			linearFiltering = false;
		else if (config::TextureFiltering == 0)
			linearFiltering = gp->tsp.FilterMode != 0;
		else if (config::TextureFiltering == 1)
			linearFiltering = false;
		else
			linearFiltering = true;
        auto sampler = samplers->getSampler(linearFiltering, gp->tsp.ClampU, gp->tsp.ClampV, gp->tsp.FlipU, gp->tsp.FlipV, Type == ListType_Punch_Through);
        deviceContext->PSSetSamplers(0, 1, &sampler.get());
	}

	// Apparently punch-through polys support blending, or at least some combinations
#ifdef FLYCAST_ENABLE_NEURAL
	if (neuralExportActive)
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	else
#endif
		deviceContext->OMSetBlendState(blendStates.getState(true, gp->tsp.SrcInstr, gp->tsp.DstInstr), nullptr, 0xffffffff);

	setCullMode(gp->isp.CullMode);

	//set Z mode, only if required
	int zfunc;
	if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
		zfunc = 6; // GEQ
	else
		zfunc = gp->isp.DepthMode;

	bool zwriteEnable;
	if (SortingEnabled /* && !config::PerStripSorting */)
		zwriteEnable = false;
	else
	{
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (Type == ListType_Punch_Through)
			zwriteEnable = true;
		else
			zwriteEnable = !gp->isp.ZWriteDis;
	}
	const u32 stencil = (gp->pcw.Shadow != 0) ? 0x80 : 0;
#ifdef FLYCAST_ENABLE_NEURAL
	if (neuralReactiveCoverageActive)
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(false, false, 7, false), 0);
	else
#endif
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, zwriteEnable,
			zfunc, config::ModifierVolumes), stencil);

	if (gp->isNaomi2())
		n2Helper.setConstants(*gp, 0, *rendContext); // poly number only used in OIT
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::drawList(const std::vector<PolyParam>& gply, int first, int count)
{
	if (count == 0)
		return;
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const PolyParam* params = &gply[first];

	while (count-- > 0)
	{
		if (params->count > 2)
		{
			if ((Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled)) && params->isp.DepthMode == 0)
			{
				// depthFunc = never
				params++;
				continue;
			}
			setRenderState<Type, SortingEnabled>(params);
			deviceContext->DrawIndexed(params->count, params->first, 0);
		}

		params++;
	}
}

void DX11Renderer::drawSorted(int first, int count, bool multipass)
{
	if (count == 0)
		return;
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	int end = first + count;
	for (int p = first; p < end; p++)
	{
		const PolyParam* params = &rendContext->global_param_tr[rendContext->sortedTriangles[p].polyIndex];
		// Sorted guidance records follow the three original list ranges. The
		// override is consumed only during neural export, never native drawing.
		const auto ordinal = static_cast<u32>(rendContext->global_param_op.size()
			+ rendContext->global_param_pt.size() + rendContext->global_param_tr.size() + p);
		setRenderState<ListType_Translucent, true>(params, ordinal);
		deviceContext->DrawIndexed(rendContext->sortedTriangles[p].count, rendContext->sortedTriangles[p].first, 0);
	}
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

		ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(true, settings.platform.isNaomi2());
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		ComPtr<ID3D11PixelShader> pixelShader = shaders->getShader(
				false,
				false,
				false,
				0,
				false,
				2,
				false,
				false,
				false,
				false,
				true,
				false,
				false,
				false);
		deviceContext->PSSetShader(pixelShader, nullptr, 0);

		// Enable depth test, enable depth write, >=, disable stencil
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, true, 6, false), 0);
		deviceContext->RSSetScissorRects(1, &scissorRect);

		for (int p = first; p < end; p++)
		{
			const PolyParam* params = &rendContext->global_param_tr[rendContext->sortedTriangles[p].polyIndex];
			if (!params->isp.ZWriteDis)
			{
				setCullMode(params->isp.CullMode);
				deviceContext->DrawIndexed(rendContext->sortedTriangles[p].count, rendContext->sortedTriangles[p].first, 0);
			}
		}
	}
}

void DX11Renderer::drawModVols(int first, int count)
{
	if (count == 0 || rendContext->modtrig.empty() || !config::ModifierVolumes)
		return;

	deviceContext->IASetInputLayout(modVolInputLayout);
    unsigned int stride = 3 * sizeof(float);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &modvolBuffer.get(), &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

	deviceContext->PSSetShader(shaders->getModVolShader(), nullptr, 0);

	setCullMode(0);

	const ModifierVolumeParam *params = &rendContext->global_param_mvo[first];

	int mod_base = -1;
	int curMVMat = -1;
	int curProjMat = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		const ModifierVolumeParam& param = params[cmv];

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (param.isNaomi2() && (param.mvMatrix != curMVMat || param.projMatrix != curProjMat))
		{
			curMVMat = param.mvMatrix;
			curProjMat = param.projMatrix;
			n2Helper.setConstants(rendContext->matrices[param.mvMatrix].mat, rendContext->matrices[param.projMatrix].mat);
		}
		deviceContext->VSSetShader(shaders->getMVVertexShader(param.isNaomi2()), nullptr, 0);
		if (!param.isp.VolumeLast && mv_mode > 0)
			// OR'ing (open volume or quad)
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Or), 2);
		else
			// XOR'ing (closed volume)
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Xor), 0);

		Rect clip_rect;
		setTileClip(param.tileclip, clip_rect);
		// TODO inside clipping

		if (param.count > 0)
		{
			setCullMode(param.isp.CullMode);
			deviceContext->Draw(param.count * 3, param.first * 3);
		}

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(mv_mode == 1 ? DepthStencilStates::Inclusion : DepthStencilStates::Exclusion), 1);
			deviceContext->Draw((param.first + param.count - mod_base) * 3, mod_base * 3);
			mod_base = -1;
		}
	}
	//disable culling
	setCullMode(0);
	//enable color writes
	deviceContext->OMSetBlendState(blendStates.getState(true, 4, 5), nullptr, 0xffffffff);
	deviceContext->RSSetScissorRects(1, &scissorRect);

	//black out any stencil with '1'
	//only pixels that are Modvol enabled, and in area 1
	deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Final), 0x81);

	deviceContext->IASetInputLayout(mainInputLayout);
    stride = sizeof(Vertex);
    offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Use the background poly as a quad
	deviceContext->VSSetShader(shaders->getMVVertexShader(false), nullptr, 0);
	deviceContext->DrawIndexed(4, 0, 0);
}

void DX11Renderer::drawStrips()
{
	RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < (int)rendContext->render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = rendContext->render_passes[render_pass];
        u32 op_count = current_pass.op_count - previous_pass.op_count;
        u32 pt_count = current_pass.pt_count - previous_pass.pt_count;
        u32 tr_count = current_pass.tr_count - previous_pass.tr_count;
        u32 mvo_count = current_pass.mvo_count - previous_pass.mvo_count;
        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
        		op_count, pt_count, tr_count, mvo_count, current_pass.autosort);

		drawList<ListType_Opaque, false>(rendContext->global_param_op, previous_pass.op_count, op_count);

		drawList<ListType_Punch_Through, false>(rendContext->global_param_pt, previous_pass.pt_count, pt_count);

		drawModVols(previous_pass.mvo_count, mvo_count);

		if (current_pass.autosort)
		{
			if (!config::PerStripSorting)
				drawSorted(previous_pass.sorted_tr_count, current_pass.sorted_tr_count - previous_pass.sorted_tr_count, render_pass < (int)rendContext->render_passes.size() - 1);
			else
				drawList<ListType_Translucent, true>(rendContext->global_param_tr, previous_pass.tr_count, tr_count);
		}
		else
		{
			drawList<ListType_Translucent, false>(rendContext->global_param_tr, previous_pass.tr_count, tr_count);
		}
		previous_pass = current_pass;
    }
}

bool DX11Renderer::RenderLastFrame()
{
	if (!frameRenderedOnce || clearLastFrame)
		return false;
	displayFramebuffer();
	return true;
}

void DX11Renderer::RenderFramebuffer(const FramebufferInfo& info)
{
	rendContext = nullptr;
	PixelBuffer<u32> pb;
	int width;
	int height;

	if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
	{
		// Video output disabled
		width = height = 1;
		pb.init(width, height, false);
		u8 *p = (u8 *)pb.data(0, 0);
		p[0] = info.vo_border_col._blue;
		p[1] = info.vo_border_col._green;
		p[2] = info.vo_border_col._red;
		p[3] = 255;
	}
	else
	{
		ReadFramebuffer<BGRAPacker>(info, pb, width, height);
	}

	if (dcfbTexture)
	{
		D3D11_TEXTURE2D_DESC desc;
		dcfbTexture->GetDesc(&desc);
		if ((int)desc.Width != width || (int)desc.Height != height)
		{
			dcfbTexture.reset();
			dcfbTextureView.reset();
		}
	}
	if (!dcfbTexture)
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;

		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &dcfbTexture.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture creation failed");
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(dcfbTexture, &viewDesc, &dcfbTextureView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture view creation failed");
	}
	deviceContext->UpdateSubresource(dcfbTexture, 0, nullptr, pb.data(), width * sizeof(u32), width * sizeof(u32) * height);

#ifndef LIBRETRO
	ID3D11ShaderResourceView *nullResView = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullResView);
	resize(width, height);
	deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), nullptr);
	float colors[4];
	info.vo_border_col.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(fbRenderTarget, colors);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)this->width;
	vp.Height = (FLOAT)this->height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	const D3D11_RECT r = { 0, 0, (LONG)this->width, (LONG)this->height };
	deviceContext->RSSetScissorRects(1, &r);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);

	quad->draw(dcfbTextureView, samplers->getSampler(true));

	aspectRatio = getDCFramebufferAspectRatio();
#ifdef FLYCAST_ENABLE_NEURAL
	submitNeuralFramebuffer();
#endif

	deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);
	displayFramebuffer();
	drawOSD();
	renderVideoRouting();
	DX11Context::Instance()->setFrameRendered();
#else
	ID3D11RenderTargetView *nullView = nullptr;
	deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
	DX11Context::Instance()->presentFrame(dcfbTextureView, width, height);
#endif
	frameRendered = true;
	frameRenderedOnce = true;
	clearLastFrame = false;
}

void DX11Renderer::setBaseScissor()
{
	Rect scissor = matrices.getBaseScissor();
	scissorRect.left = scissor.origin.x;
	scissorRect.top = scissor.origin.y;
	// DX11 scissor bottom right pixel is clipped
	scissorRect.right = scissor.origin.x + scissor.size.x;
	scissorRect.bottom = scissor.origin.y + scissor.size.y;
	if (rendContext->isRTT) {
		scissorEnable = true;
	}
	else
	{
		if (scissor.origin.x != 0 || scissor.origin.y != 0
				|| scissor.size.x != (int)rendContext->framebufferWidth
				|| scissor.size.y != (int)rendContext->framebufferHeight)
			scissorEnable = true;
		else
			scissorEnable = false;
	}
	deviceContext->RSSetScissorRects(1, &scissorRect);
}

void DX11Renderer::prepareRttRenderTarget(u32 texAddress)
{
	u32 fbw = rendContext->framebufferWidth;
	u32 fbh = rendContext->framebufferHeight;
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x",
			rendContext->fb_W_CTRL.fb_packmode, rendContext->fb_W_LINESTRIDE * 8, fbw, fbh, texAddress);

	createTexAndRenderTarget(rttTexture, rttRenderTarget, fbw, fbh);
	createDepthTexAndView(rttDepthTex, rttDepthTexView, fbw, fbh);
	deviceContext->ClearDepthStencilView(rttDepthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
	deviceContext->OMSetRenderTargets(1, &rttRenderTarget.get(), rttDepthTexView);

	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)fbw;
	vp.Height = (FLOAT)fbh;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	setRTTSize(fbw, fbh);
}

void DX11Renderer::readRttRenderTarget(u32 texAddress)
{
	u32 w = rendContext->framebufferWidth;
	u32 h = rendContext->framebufferHeight;
	if (config::RenderToTextureBuffer)
	{
		D3D11_TEXTURE2D_DESC desc;
		rttTexture->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		ComPtr<ID3D11Texture2D> stagingTex;
		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "Staging RTT texture creation failed");
			return;
		}
		deviceContext->CopyResource(stagingTex, rttTexture);

		PixelBuffer<u32> tmp_buf;
		tmp_buf.init(w, h);
		u8 *p = (u8 *)tmp_buf.data();

		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "Failed to map staging RTT texture");
			return;
		}
		if (w * sizeof(u32) == mappedSubres.RowPitch) {
			memcpy(p, mappedSubres.pData, w * h * sizeof(u32));
		}
		else
		{
			u8 *src = (u8 *)mappedSubres.pData;
			for (u32 y = 0; y < h; y++)
			{
				memcpy(p, src, w * sizeof(u32));
				p += w * sizeof(u32);
				src += mappedSubres.RowPitch;
			}
		}
		deviceContext->Unmap(stagingTex, 0);

		u16 *dst = (u16 *)&vram[texAddress];
		WriteTextureToVRam<2, 1, 0, 3>(w, h, (u8 *)tmp_buf.data(), dst, rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, rendContext->fbClip);
	}
	else
	{
		//memset(&vram[gl.rtt.texAddress], 0, size);
		int wpo2, hpo2;
		getPvrFramebufferSize(*rendContext, wpo2, hpo2);
		if (wpo2 <= 1024 && hpo2 <= 1024)
		{
			DX11Texture* texture = texCache.getRTTexture(texAddress, rendContext->fb_W_CTRL.fb_packmode, wpo2, hpo2);

			texture->texture = rttTexture;
			rttTexture.reset();
			rttRenderTarget.reset();
			texture->textureView.reset();
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(texture->texture, &viewDesc, &texture->textureView.get());

			texture->MarkRenderToTextureUpdate();
			texture->unprotectVRam();
		}
	}
}

void DX11Renderer::updatePaletteTexture()
{
	if (updatePalette)
	{
		updatePalette = false;
		deviceContext->UpdateSubresource(paletteTexture, 0, nullptr, palette32_ram, 32 * sizeof(u32), 32 * sizeof(u32) * 32);
	}
    deviceContext->PSSetShaderResources(1, 1, &paletteTextureView.get());
    deviceContext->PSSetSamplers(1, 1, &samplers->getSampler(false).get());
}

void DX11Renderer::updateFogTexture()
{
	if (!config::Fog)
		return;
	if (updateFogTable)
	{
		updateFogTable = false;
		u8 temp_tex_buffer[256];
		MakeFogTexture(temp_tex_buffer);

		deviceContext->UpdateSubresource(fogTexture, 0, nullptr, temp_tex_buffer, 128, 128 * 2);
	}
    deviceContext->PSSetShaderResources(2, 1, &fogTextureView.get());
    deviceContext->PSSetSamplers(2, 1, &samplers->getSampler(true).get());
}

void DX11Renderer::drawOSD()
{
#ifndef LIBRETRO
	DX11Context::Instance()->setOverlay(true);
	gui_display_osd();
	DX11Context::Instance()->setOverlay(false);
#endif
}

void DX11Renderer::writeFramebufferToVRAM()
{
	u32 width = rendContext->globClip.x;
	u32 height = rendContext->globClip.y;
	glm::ivec2 scaledSize;
	Rect finalClip;
	getWriteFBToVramParams(*rendContext, scaledSize, finalClip);

	ComPtr<ID3D11Texture2D> fbTexture = fbTex;

	if (scaledSize.x != (int)width || scaledSize.y != (int)height)
	{
		const u32 scaledW = scaledSize.x;
		const u32 scaledH = scaledSize.y;

		if (fbScaledTexture)
		{
			D3D11_TEXTURE2D_DESC desc;
			fbScaledTexture->GetDesc(&desc);
			if (desc.Width != scaledW || desc.Height != scaledH)
			{
				fbScaledTexture.reset();
				fbScaledTextureView.reset();
				fbScaledRenderTarget.reset();
			}
		}
		if (!fbScaledTexture)
		{
			createTexAndRenderTarget(fbScaledTexture, fbScaledRenderTarget, scaledW, scaledH);

			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(fbScaledTexture, &viewDesc, &fbScaledTextureView.get());
		}
		deviceContext->OMSetRenderTargets(1, &fbScaledRenderTarget.get(), nullptr);
		D3D11_VIEWPORT vp{};
		vp.Width = (FLOAT)scaledW;
		vp.Height = (FLOAT)scaledH;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		deviceContext->RSSetViewports(1, &vp);
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
		quad->draw(fbTextureView, samplers->getSampler(true));

		width = scaledW;
		height = scaledH;
		fbTexture = fbScaledTexture;
	}
	u32 texAddress = rendContext->fb_W_SOF1 & VRAM_MASK; // TODO SCALER_CTL.interlace, SCALER_CTL.fieldselect
	u32 linestride = rendContext->fb_W_LINESTRIDE * 8;

	D3D11_TEXTURE2D_DESC desc;
	fbTexture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ComPtr<ID3D11Texture2D> stagingTex;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Staging RTT texture creation failed");
		return;
	}
	deviceContext->CopyResource(stagingTex, fbTexture);

	PixelBuffer<u32> tmp_buf;
	tmp_buf.init(width, height);
	u8 *p = (u8 *)tmp_buf.data();

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Failed to map staging RTT texture");
		return;
	}
	if (width * sizeof(u32) == mappedSubres.RowPitch)
		memcpy(p, mappedSubres.pData, width * height * sizeof(u32));
	else
	{
		u8 *src = (u8 *)mappedSubres.pData;
		for (u32 y = 0; y < height; y++)
		{
			memcpy(p, src, width * sizeof(u32));
			p += width * sizeof(u32);
			src += mappedSubres.RowPitch;
		}
	}
	deviceContext->Unmap(stagingTex, 0);

	WriteFramebuffer<2, 1, 0, 3>(width, height, (u8 *)tmp_buf.data(), texAddress, rendContext->fb_W_CTRL, linestride, finalClip);
}

bool DX11Renderer::GetLastFrame(std::vector<u8>& data, int& width, int& height)
{
	if (!frameRenderedOnce)
		return false;

	if (width != 0) {
		height = width / aspectRatio;
	}
	else if (height != 0) {
		width = aspectRatio * height;
	}
	else
	{
		width = this->width;
		height = this->height;
		if (config::Rotate90)
			std::swap(width, height);
		// We need square pixels for PNG
		int w = aspectRatio * height;
		if (width > w)
			height = width / aspectRatio;
		else
			width = w;
	}

	ComPtr<ID3D11Texture2D> dstTex;
	ComPtr<ID3D11RenderTargetView> dstRenderTarget;
	createTexAndRenderTarget(dstTex, dstRenderTarget, width, height);

	ID3D11ShaderResourceView *nullResView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullResView);
	deviceContext->OMSetRenderTargets(1, &dstRenderTarget.get(), nullptr);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	const D3D11_RECT r = { 0, 0, (LONG)width, (LONG)height };
	deviceContext->RSSetScissorRects(1, &r);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);

	quad->draw(fbTextureView, samplers->getSampler(true), nullptr, -1.f, -1.f, 2.f, 2.f, config::Rotate90);

#ifndef LIBRETRO
	deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);
#else
	ID3D11RenderTargetView *nullView = nullptr;
	deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
#endif
	D3D11_TEXTURE2D_DESC desc;
	dstTex->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ComPtr<ID3D11Texture2D> stagingTex;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Staging screenshot texture creation failed");
		return false;
	}
	deviceContext->CopyResource(stagingTex, dstTex);

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Failed to map staging screenshot texture");
		return false;
	}
	const u8* const src = (const u8 *)mappedSubres.pData;
	for (int y = 0; y < height; y++)
	{
		const u8 *p = src + y * mappedSubres.RowPitch;
		for (int x = 0; x < width; x++, p += 4)
		{
			data.push_back(p[2]);
			data.push_back(p[1]);
			data.push_back(p[0]);
		}
	}
	deviceContext->Unmap(stagingTex, 0);

	return true;
}

void DX11Renderer::renderVideoRouting()
{
#ifdef VIDEO_ROUTING
	if (config::VideoRouting)
	{
		extern void os_VideoRoutingPublishFrameTexture(ID3D11Texture2D* pTexture);
		
		ID3D11RenderTargetView* pRenderTargetView = DX11Context::Instance()->getRenderTarget().get();

		// Backbuffer texture would be different after resizing, fetching new address everytime
		ID3D11Resource* pResource = nullptr;
		pRenderTargetView->GetResource(&pResource);
		ID3D11Texture2D* backBufferTexture = nullptr;
		pResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&backBufferTexture);		
		
		if (config::VideoRoutingScale)
		{
			D3D11_TEXTURE2D_DESC bbDesc = {};
			backBufferTexture->GetDesc(&bbDesc);
			D3D11_TEXTURE2D_DESC vrsDesc = {};
			if (vrStagingTexture)
				vrStagingTexture->GetDesc(&vrsDesc);

			// Window resized?
			if (!vrStagingTexture || bbDesc.Width != vrsDesc.Width || bbDesc.Height != vrsDesc.Height)
			{
				vrStagingTexture.reset();
				vrStagingTextureSRV.reset();

				D3D11_TEXTURE2D_DESC srvDesc = bbDesc;
				srvDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				srvDesc.Usage = D3D11_USAGE_DEFAULT;
				device->CreateTexture2D(&srvDesc, nullptr, &vrStagingTexture.get());

				D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipLevels = 1;
				
				device->CreateShaderResourceView(vrStagingTexture.get(), &viewDesc, &vrStagingTextureSRV.get());
			}

			// Scale down value changed?
			D3D11_TEXTURE2D_DESC vrscDesc = {};
			if (vrScaledTexture)
				vrScaledTexture->GetDesc(&vrscDesc);
			int targetWidth = config::VideoRoutingVRes * settings.display.width / settings.display.height;
			if (!vrScaledTexture || (int)vrscDesc.Height != config::VideoRoutingVRes)
			{

				vrScaledTexture.reset();
				vrScaledRenderTarget.reset();
				createTexAndRenderTarget(vrScaledTexture, vrScaledRenderTarget, targetWidth, config::VideoRoutingVRes);
			}
			D3D11_VIEWPORT scaledViewPort{};
			scaledViewPort.Width = targetWidth;
			scaledViewPort.Height = config::VideoRoutingVRes;
			scaledViewPort.MinDepth = 0.f;
			scaledViewPort.MaxDepth = 1.f;

			deviceContext->OMSetRenderTargets(1, &vrScaledRenderTarget.get(), nullptr);
			deviceContext->RSSetViewports(1, &scaledViewPort);
			deviceContext->CopyResource(vrStagingTexture.get(), backBufferTexture);
			quad->draw(vrStagingTextureSRV, samplers->getSampler(true));
			os_VideoRoutingPublishFrameTexture(vrScaledTexture);

			deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);

		} else {
			os_VideoRoutingPublishFrameTexture(backBufferTexture);
		}

		backBufferTexture->Release();
		pResource->Release();
	}
	else
	{
		os_VideoRoutingTermDX();
	}
#endif
}

Renderer *rend_DirectX11()
{
	return new DX11Renderer();
}
