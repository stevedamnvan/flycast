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
#include "version.h"
#ifdef FLYCAST_ENABLE_NEURAL
#include "rend/neural/pvr_material_capture.h"
#include "rend/neural/live_status.h"
#include "rend/neural/quality_profile.h"
#include "rend/neural/pvr_scene_capture.h"
#include "rend/neural/remake_neural_input.h"
#include "rend/neural/remake_input_replay.h"
#endif

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <thread>

void os_VideoRoutingTermDX();

#ifdef FLYCAST_ENABLE_NEURAL
namespace {
// Bounded, opt-in elapsed CPU diagnostics. Includes driver blocking; not GPU time.
class RemakeCpuScope {
	const char* label;
	std::uint64_t frame;
	bool enabled;
	std::chrono::steady_clock::time_point start;
public:
	RemakeCpuScope(const char* label, std::uint64_t frame, unsigned& count)
		: label(label), frame(frame), enabled(false) {
		const char* value=std::getenv("FLYCAST_REMAKE_CPU_TIMING");
		enabled=value&&std::strcmp(value,"1")==0&&count<600;
		if(enabled){++count;start=std::chrono::steady_clock::now();}
	}
	void End(){if(enabled){report();enabled=false;}}
	~RemakeCpuScope(){End();}
private:
	void report()const {
		const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
		NOTICE_LOG(RENDERER,"Remake CPU scope: frame=%llu stage=%s elapsed_ms=%.6f includes_driver_wait=true diagnostic=true",
			(unsigned long long)frame,label,ms);
	}
};
}
#endif

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
	publishNeuralStatus(flycast::rend::neural::SubmitStatus::Disabled,
		"neural rendering is off");
#endif

	return success;
}

void DX11Renderer::Term()
{
	NOTICE_LOG(RENDERER, "DX11 renderer terminating");
#ifdef FLYCAST_ENABLE_NEURAL
	remakeFeedWorker.Stop();remakeReturnWorker.Stop();
	for(auto& pooled:remakeOwnedOutputs){pooled.view.reset();pooled.texture.reset();}
	remakePaletteUpload.reset();
	neuralStage.Shutdown();
	neuralInstrumentation.SetEnabled(false);
	releaseNeuralResources();
	flycast::rend::neural::ResetLiveStatus();
	neuralInputLayout.reset();
#endif
#ifdef VIDEO_ROUTING
	os_VideoRoutingTermDX();
#endif
	n2Helper.term();
	vtxConstants.reset();
	pxlConstants.reset();
	nativeParityStagingTexture.reset();
	nativeParitySeenFrames = 0;
	nativeParityCapturedFrames = 0;
	nativeParityCaptureComplete = false;
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

void DX11Renderer::configVertexShader(float rasterJitterX, float rasterJitterY, const float *capturedViewport)
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
	memcpy(&constant.transMatrix, capturedViewport ? capturedViewport : &matrices.GetNormalMatrix()[0][0], sizeof(constant.transMatrix));
	constant.leftPlane[0] = 1;
	constant.leftPlane[3] = 1;
	constant.rightPlane[0] = -1;
	constant.rightPlane[3] = 1;
	constant.topPlane[1] = 1;
	constant.topPlane[3] = 1;
	constant.bottomPlane[1] = -1;
	constant.bottomPlane[3] = 1;
	constant.neuralRenderSize[0] = static_cast<float>(width);
	constant.neuralRenderSize[1] = static_cast<float>(height);
	constant.neuralRasterJitter[0] = rasterJitterX;
	constant.neuralRasterJitter[1] = rasterJitterY;
#ifdef FLYCAST_ENABLE_NEURAL
	if (pvrReplayBase && !pvrReplayNativeVertexValid)
	{
		pvrReplayNativeVertexConstants = constant;
		pvrReplayNativeVertexValid = true;
	}
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
	// Unused fog alpha/dither fields and the aligned tail are still uploaded.
	// Initialize them rather than exposing stack/allocation residue to captures.
	PixelConstants pixelConstants{};
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
#ifdef FLYCAST_ENABLE_NEURAL
	remakeSourceAlphaReference.reset();
	if(pixelConstants.colorClampMin[3]==0&&pixelConstants.colorClampMax[3]==1)
		remakeSourceAlphaReference=static_cast<std::uint8_t>(std::lround(pixelConstants.alphaTestValue*255));
#endif

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
#ifdef FLYCAST_ENABLE_NEURAL
	materialShaderGlobals = {};
	if (config::NeuralCapturePvrMaterials.get()) {
	// Diagnostic representation comparison only; padding is never serialized.
	std::array<unsigned char,sizeof(PixelConstants)> beforeSnapshot;
	std::memcpy(beforeSnapshot.data(), &pixelConstants, sizeof(pixelConstants));
	materialShaderGlobals.fogEnabled = config::Fog.get();
	for (unsigned i=0;i<3;++i) {
		materialShaderGlobals.fogVertex[i]=pixelConstants.fog_col_vert[i];
		materialShaderGlobals.fogRam[i]=pixelConstants.fog_col_ram[i];
	}
	for (unsigned i=0;i<4;++i) {
		materialShaderGlobals.clampMin[i]=pixelConstants.colorClampMin[i];
		materialShaderGlobals.clampMax[i]=pixelConstants.colorClampMax[i];
	}
	materialShaderGlobals.fogDensity=pixelConstants.fogDensity;
	materialShaderGlobals.alphaReference=pixelConstants.alphaTestValue;
	materialShaderGlobals.shadowScale=pixelConstants.shadowScale;
	materialShaderGlobals.sourceBytesUnchanged = std::memcmp(beforeSnapshot.data(),
		&pixelConstants, sizeof(pixelConstants)) == 0;
	materialShaderGlobals.valid=materialShaderGlobals.sourceBytesUnchanged;
	}
#endif
	ID3D11Buffer *buffers[] { pxlConstants, pxlPolyConstants };
	deviceContext->PSSetConstantBuffers(0, std::size(buffers), buffers);
}

bool DX11Renderer::Render()
{
#ifdef FLYCAST_ENABLE_NEURAL
	materialShaderGlobals.valid=false;
#endif
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
#ifdef FLYCAST_ENABLE_NEURAL
	if (!is_rtt && !config::EmulateFramebuffer)
		retainPvrReplayBase();
#endif
	configVertexShader();

	deviceContext->IASetInputLayout(mainInputLayout);

	n2Helper.resetCache();
	uploadGeometryBuffers();

	updateFogTexture();
	updatePaletteTexture();

	setupPixelShaderConstants();
#ifdef FLYCAST_ENABLE_NEURAL
	if (!is_rtt && !config::EmulateFramebuffer && pvrReplayBase)
	{
		pvrReplayPixelConstants.reset();
		D3D11_BUFFER_DESC desc{}; pxlConstants->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_DEFAULT; desc.CPUAccessFlags = 0;
		if (SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pvrReplayPixelConstants.get())))
			deviceContext->CopyResource(pvrReplayPixelConstants, pxlConstants);
	}
#endif

	drawStrips();
	if (!is_rtt && !config::EmulateFramebuffer)
		captureNativeParityFrame();
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
#ifdef FLYCAST_ENABLE_NEURAL
		captureNeuralLateOverlayFrame();
#endif
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

void DX11Renderer::captureNativeParityFrame()
{
	const std::filesystem::path root(config::NativeParityCaptureDirectory.get());
	const std::uint32_t requested = static_cast<std::uint32_t>(
		std::clamp(config::NativeParityCaptureFrames.get(), 0, 240));
	if (root.empty() || requested == 0 || nativeParityCaptureComplete || !fbTex)
		return;

	const std::uint32_t seen = nativeParitySeenFrames++;
	const std::uint32_t skip = static_cast<std::uint32_t>(
		std::max(0, config::NativeParityCaptureSkip.get()));
	if (seen < skip)
		return;

	D3D11_TEXTURE2D_DESC sourceDesc{};
	fbTex->GetDesc(&sourceDesc);
	if (sourceDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || sourceDesc.SampleDesc.Count != 1)
	{
		WARN_LOG(RENDERER, "Native parity capture requires single-sample BGRA8 PVR color");
		nativeParityCaptureComplete = true;
		return;
	}

	bool createStaging = !nativeParityStagingTexture;
	if (!createStaging)
	{
		D3D11_TEXTURE2D_DESC stagingDesc{};
		nativeParityStagingTexture->GetDesc(&stagingDesc);
		createStaging = stagingDesc.Width != sourceDesc.Width
			|| stagingDesc.Height != sourceDesc.Height
			|| stagingDesc.Format != sourceDesc.Format;
	}
	if (createStaging)
	{
		nativeParityStagingTexture.reset();
		D3D11_TEXTURE2D_DESC stagingDesc = sourceDesc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags = 0;
		if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr,
			&nativeParityStagingTexture.get())))
		{
			WARN_LOG(RENDERER, "Native parity capture could not create staging texture");
			nativeParityCaptureComplete = true;
			return;
		}
	}

	deviceContext->CopyResource(nativeParityStagingTexture, fbTex);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(deviceContext->Map(nativeParityStagingTexture, 0, D3D11_MAP_READ, 0, &mapped)))
	{
		WARN_LOG(RENDERER, "Native parity capture could not map staging texture");
		nativeParityCaptureComplete = true;
		return;
	}

	const std::size_t rowBytes = static_cast<std::size_t>(sourceDesc.Width) * 4;
	std::vector<std::uint8_t> bytes(rowBytes * sourceDesc.Height);
	for (std::uint32_t y = 0; y < sourceDesc.Height; ++y)
		std::memcpy(bytes.data() + rowBytes * y,
			static_cast<const std::uint8_t *>(mapped.pData) + mapped.RowPitch * y,
			rowBytes);
	deviceContext->Unmap(nativeParityStagingTexture, 0);

	std::uint64_t hash = 14695981039346656037ull;
	for (const std::uint8_t value : bytes)
	{
		hash ^= value;
		hash *= 1099511628211ull;
	}

	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec)
	{
		WARN_LOG(RENDERER, "Native parity capture could not create output directory: %s",
			ec.message().c_str());
		nativeParityCaptureComplete = true;
		return;
	}

	std::ostringstream stem;
	stem << "frame-" << std::setfill('0') << std::setw(6) << nativeParityCapturedFrames;
	std::ofstream raw(root / (stem.str() + ".bgra8"), std::ios::binary);
	raw.write(reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	raw.close();
	std::ofstream metadata(root / (stem.str() + ".json"));
	metadata.imbue(std::locale::classic());
	metadata << "{\n"
		<< "  \"schema\": 1,\n"
		<< "  \"capture_point\": \"pvr-scene-color-before-neural-and-overlays\",\n"
		<< "  \"format\": \"BGRA8_UNORM\",\n"
		<< "  \"renderer\": \"" << (IsOitRenderer() ? "dx11-oit" : "dx11") << "\",\n"
		<< "  \"capture_index\": " << nativeParityCapturedFrames << ",\n"
		<< "  \"source_frame_index\": " << seen << ",\n"
		<< "  \"width\": " << sourceDesc.Width << ",\n"
		<< "  \"height\": " << sourceDesc.Height << ",\n"
		<< "  \"row_bytes\": " << rowBytes << ",\n"
		<< "  \"byte_count\": " << bytes.size() << ",\n"
		<< "  \"fnv64\": \"" << std::hex << std::uppercase << std::setfill('0')
		<< std::setw(16) << hash << "\",\n"
		<< "  \"git_sha\": \"" << GIT_HASH << "\",\n"
		<< "  \"synchronous_developer_capture\": true\n"
		<< "}\n";
	metadata.close();

	if (!raw || !metadata)
	{
		WARN_LOG(RENDERER, "Native parity capture failed to write frame %u",
			nativeParityCapturedFrames);
		nativeParityCaptureComplete = true;
		return;
	}

	++nativeParityCapturedFrames;
	if (nativeParityCapturedFrames == requested)
	{
		bool neuralCompiled = false;
		int neuralMode = 0;
		bool instrumentationEnabled = false;
		std::size_t drawRecords = 0;
		std::size_t previousPositions = 0;
		bool inputLayoutAllocated = false;
		bool exportResourcesAllocated = false;
		bool d3d11On12Surface = false;
		std::uint64_t guidanceReplays = 0;
		std::uint32_t backendResourceObjects = 0;
#ifdef FLYCAST_ENABLE_NEURAL
		neuralCompiled = true;
		neuralMode = config::NeuralMode.get();
		instrumentationEnabled = neuralInstrumentation.IsEnabled();
		drawRecords = neuralInstrumentation.CurrentDrawCount();
		previousPositions = neuralInstrumentation.PreviousPositions().size;
		inputLayoutAllocated = neuralInputLayout != nullptr;
		d3d11On12Surface = DX11Context::Instance()->isD3D11On12();
		exportResourcesAllocated = neuralColor.textures[0] != nullptr
			|| neuralDepthTextures[0] != nullptr || neuralMotion.textures[0] != nullptr
			|| neuralMask.textures[0] != nullptr || neuralPreviousPositionBuffer != nullptr;
		guidanceReplays = neuralGuidanceReplayCount;
		backendResourceObjects = neuralStage.GetStats().backendResourceObjects;
#endif
		std::ofstream complete(root / "native-parity-capture-complete.json");
		complete.imbue(std::locale::classic());
		complete << "{\n"
			<< "  \"schema\": 1,\n"
			<< "  \"capture_point\": \"pvr-scene-color-before-neural-and-overlays\",\n"
			<< "  \"renderer\": \"" << (IsOitRenderer() ? "dx11-oit" : "dx11") << "\",\n"
			<< "  \"d3d11on12_surface\": "
			<< (d3d11On12Surface ? "true" : "false") << ",\n"
			<< "  \"captured_frames\": " << nativeParityCapturedFrames << ",\n"
			<< "  \"skipped_frames\": " << skip << ",\n"
			<< "  \"git_sha\": \"" << GIT_HASH << "\",\n"
			<< "  \"neural_compiled\": " << (neuralCompiled ? "true" : "false") << ",\n"
			<< "  \"neural_mode\": " << neuralMode << ",\n"
			<< "  \"neural_instrumentation_enabled\": "
			<< (instrumentationEnabled ? "true" : "false") << ",\n"
			<< "  \"neural_draw_records\": " << drawRecords << ",\n"
			<< "  \"neural_previous_positions\": " << previousPositions << ",\n"
			<< "  \"neural_input_layout_allocated\": "
			<< (inputLayoutAllocated ? "true" : "false") << ",\n"
			<< "  \"neural_export_resources_allocated\": "
			<< (exportResourcesAllocated ? "true" : "false") << ",\n"
			<< "  \"neural_guidance_replays\": " << guidanceReplays << ",\n"
			<< "  \"neural_backend_resource_objects\": " << backendResourceObjects << ",\n"
			<< "  \"synchronous_developer_capture\": true,\n"
			<< "  \"performance_eligible\": false\n"
			<< "}\n";
		nativeParityCaptureComplete = true;
		NOTICE_LOG(RENDERER, "Native parity capture complete: %u BGRA8 PVR frames at %s",
			nativeParityCapturedFrames, root.string().c_str());
	}
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
	createDepthTexAndView(neuralSceneDepthTexture, neuralSceneDepthTarget,
		static_cast<int>(width), static_cast<int>(height));
	if (!neuralSceneDepthTexture || !neuralSceneDepthTarget)
	{
		WARN_LOG(RENDERER, "Neural scene-replay depth creation failed");
		releaseNeuralResources();
		return false;
	}
	D3D11_TEXTURE2D_DESC retainedDesc{};
	retainedDesc.Width = width;
	retainedDesc.Height = height;
	retainedDesc.MipLevels = 1;
	retainedDesc.ArraySize = 1;
	retainedDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	retainedDesc.SampleDesc.Count = 1;
	retainedDesc.Usage = D3D11_USAGE_DEFAULT;
	retainedDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	HRESULT retainedResult = device->CreateTexture2D(&retainedDesc, nullptr,
		&neuralRetainedSceneTexture.get());
	if (SUCCEEDED(retainedResult))
		retainedResult = device->CreateRenderTargetView(neuralRetainedSceneTexture,
			nullptr, &neuralRetainedSceneTarget.get());
	if (SUCCEEDED(retainedResult))
		retainedResult = device->CreateShaderResourceView(neuralRetainedSceneTexture,
			nullptr, &neuralRetainedSceneView.get());
	if (FAILED(retainedResult))
	{
		WARN_LOG(RENDERER, "Neural retained-scene resource creation failed: %x",
			retainedResult);
		releaseNeuralResources();
		return false;
	}
	neuralRetainedSceneValid = false;
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
	remakeSessionRenewalRequested=true;
	remakeAsyncTextures.Reset();remakeAsyncChannel.Close();resetRemakeAsyncFrames();
	remakeAsyncStopped=true;
	pvrReplayBase.reset();
	pvrReplayPixelConstants.reset();
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
	neuralSceneDepthTarget.reset();
	neuralSceneDepthTexture.reset();
	neuralRetainedSceneView.reset();
	neuralRetainedSceneTarget.reset();
	neuralRetainedSceneTexture.reset();
	neuralRetainedSceneValid = false;
	for (auto& view : neuralOutputWrappedViews) view.reset();
	for (auto& texture : neuralOutputWrappedTextures) texture.reset();
	for (auto& resource : neuralOutputD3D12Resources) resource.reset();
	neuralDepthWidth = neuralDepthHeight = 0;
	neuralExportSlot = 0;
	neuralAcceptedGuidanceSlot = 0;
	hasNeuralAcceptedGuidance = false;
	currentNeuralGuidanceFrameId = 0;
	lastPresentedNeuralFrameId = 0;
	neuralExportActive = false;
	neuralReactiveCoverageActive = false;
	neuralPreviousPositionBuffer.reset();
	neuralPreviousPositionBufferSize = 0;
}

void DX11Renderer::retainPvrReplayBase()
{
	pvrReplayNativeVertexValid = false;
	pvrReplayBase.reset();
	pvrReplayPixelConstants.reset();
	if (!config::NeuralCapturePvrReplay.get() || !config::NeuralCapturePvrPacket.get()
		|| !neuralQualityCapture.CapturesCurrentFrame() || IsOitRenderer()
		|| DX11Context::Instance()->isD3D11On12() || config::NeuralMode.get() != 1 || !fbTex)
		return;
	D3D11_TEXTURE2D_DESC desc{}; fbTex->GetDesc(&desc);
	desc.BindFlags = 0; desc.MiscFlags = 0; desc.CPUAccessFlags = 0; desc.Usage = D3D11_USAGE_DEFAULT;
	if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &pvrReplayBase.get())))
		deviceContext->CopyResource(pvrReplayBase, fbTex);
}

bool DX11Renderer::replayPvrPacket(const std::filesystem::path& path,
	flycast::rend::neural::PvrReplayTextures& result, std::string& error)
{
	using namespace flycast::rend::neural;
	if (!pvrReplayBase || !pvrReplayPixelConstants || !rendContext || rendContext->isRTT || IsOitRenderer()
		|| activeNeuralSurface || activeNeuralMode != 1 || neuralExportActive)
	{ error = "pvr-replay-unsupported-or-missing-base"; return false; }
	PvrDecodedPacket packet;
	if (!ReadPvrScenePacket(path, neuralQualityCaptureMetadata.frameId,
		settings.content.gameId, packet, error)) return false;
	if (std::memcmp(packet.viewport.data(), &pvrReplayNativeVertexConstants.transMatrix, sizeof(pvrReplayNativeVertexConstants.transMatrix)) != 0)
	{ error = "pvr-replay-captured-versus-native-viewport-mismatch"; return false; }
	if (packet.vertices.empty() || packet.indices.empty()
		|| packet.framebufferSize[0] != rendContext->framebufferWidth
		|| packet.framebufferSize[1] != rendContext->framebufferHeight
		|| packet.clearFramebuffer != rendContext->clearFramebuffer
		|| packet.draws.size() != rendContext->global_param_op.size() + rendContext->global_param_pt.size() + rendContext->global_param_tr.size()
		|| packet.passes.size() != rendContext->render_passes.size()
		|| packet.modifierTriangles != rendContext->modtrig.size()
		|| packet.framebufferSize[0] > 4096 || packet.framebufferSize[1] > 2160
		|| rendContext->modtrig.size() > 65536 || rendContext->global_param_mvo.size() > 8192
		|| rendContext->global_param_mvo_tr.size() > 8192 || rendContext->sortedTriangles.size() > 262144)
	{ error = "pvr-replay-size"; return false; }
	// Same-frame resource-backed experiment. Global pixel state, modifiers and
	// sorted order are retained explicitly; this is not standalone scene replay.
	rend_context replay = *rendContext;
	replay.captureProducer = {}; // Reconstructed replay is not a new live submission.
	replay.verts = packet.vertices; replay.idx = packet.indices;
	if (packet.sortedOrderCaptured)
	{
		if (packet.sortedTriangles.size() != rendContext->sortedTriangles.size())
		{ error = "pvr-replay-sorted-state-mismatch"; return false; }
		for (size_t i = 0; i < packet.sortedTriangles.size(); ++i)
		{
			const auto& saved = packet.sortedTriangles[i]; const auto& live = rendContext->sortedTriangles[i];
			if (saved.polyIndex != live.polyIndex || saved.first != live.first || saved.count != live.count)
			{ error = "pvr-replay-sorted-state-mismatch"; return false; }
		}
		replay.sortedTriangles = packet.sortedTriangles;
	}
	for (size_t i = 0; i < packet.passes.size(); ++i)
	{
		const auto& saved = packet.passes[i]; auto& live = replay.render_passes[i];
		if (saved.op != live.op_count || saved.pt != live.pt_count || saved.tr != live.tr_count
			|| saved.mvo != live.mvo_count || saved.sortedTr != live.sorted_tr_count
			|| saved.autosort != live.autosort || saved.zClear != live.z_clear)
		{ error = "pvr-replay-pass-state-mismatch"; return false; }
		live.op_count = saved.op; live.pt_count = saved.pt; live.tr_count = saved.tr;
		live.mvo_count = saved.mvo; live.sorted_tr_count = saved.sortedTr;
		live.autosort = saved.autosort; live.z_clear = saved.zClear;
	}
	auto textureMatches = [](const std::optional<PvrCapturedTexture>& saved, const BaseTextureCacheData* live) {
		if (!saved) return live == nullptr;
		if (!live || saved->upload != live->Updates || saved->rtt != live->rttGeneration) return false;
		return !saved->palette || *saved->palette == live->palette_hash;
	};
	for (const auto& draw : packet.draws)
	{
		auto& list = draw.list == 0 ? replay.global_param_op : draw.list == 1 ? replay.global_param_pt : replay.global_param_tr;
		if (draw.ordinal >= list.size()) { error = "pvr-replay-draw-count"; return false; }
		auto& live = list[draw.ordinal]; const auto& saved = draw.state;
		if (live.first != saved.first || live.count != saved.count || live.tcw.full != saved.tcw.full
			|| live.tsp.full != saved.tsp.full || live.pcw.full != saved.pcw.full || live.isp.full != saved.isp.full
			|| live.tcw1.full != saved.tcw1.full || live.tsp1.full != saved.tsp1.full || live.tileclip != saved.tileclip
			|| !textureMatches(draw.texture, live.texture) || !textureMatches(draw.texture1, live.texture1))
		{ error = "pvr-replay-retained-resource-state-mismatch"; return false; }
		auto replacement = saved; replacement.texture = live.texture; replacement.texture1 = live.texture1;
		replacement.zvZ = live.zvZ; live = replacement;
	}
	D3D11_TEXTURE2D_DESC colorDesc{}; fbTex->GetDesc(&colorDesc);
	D3D11_TEXTURE2D_DESC depthDesc{}; depthTex->GetDesc(&depthDesc);
	ComPtr<ID3D11Texture2D> depth;
	ComPtr<ID3D11DepthStencilView> depthView;
	if (FAILED(device->CreateTexture2D(&depthDesc, nullptr, &depth.get()))
		|| FAILED(device->CreateDepthStencilView(depth, nullptr, &depthView.get())))
	{ error = "pvr-replay-depth-allocation"; return false; }
	auto buffer = [&](const void *data, UINT bytes, UINT bind, ComPtr<ID3D11Buffer>& out) {
		D3D11_BUFFER_DESC desc{}; desc.ByteWidth = bytes; desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = bind;
		D3D11_SUBRESOURCE_DATA initial{}; initial.pSysMem = data;
		return SUCCEEDED(device->CreateBuffer(&desc, &initial, &out.get()));
	};
	ComPtr<ID3D11Buffer> replayIndices;
	if (!buffer(replay.idx.data(), static_cast<UINT>(replay.idx.size() * sizeof(u32)), D3D11_BIND_INDEX_BUFFER, replayIndices))
	{ error = "pvr-replay-index-allocation"; return false; }
	const auto originalVertices = vertexBuffer, originalIndices = indexBuffer;
	ComPtr<ID3D11Buffer> originalPixelConstants;
	deviceContext->PSGetConstantBuffers(0, 1, &originalPixelConstants.get());
	auto *originalContext = rendContext;
	ComPtr<ID3D11RenderTargetView> originalTarget; ComPtr<ID3D11DepthStencilView> originalDepth;
	deviceContext->OMGetRenderTargets(1, &originalTarget.get(), &originalDepth.get());
	D3D11_VIEWPORT originalViewport[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
	UINT viewportCount = std::size(originalViewport); deviceContext->RSGetViewports(&viewportCount, originalViewport);
	struct Restore { std::function<void()> fn; ~Restore() { fn(); } } restore{[&] {
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		rendContext = originalContext; vertexBuffer = originalVertices; indexBuffer = originalIndices;
		configVertexShader();
		deviceContext->PSSetConstantBuffers(0, 1, &originalPixelConstants.get());
		const UINT stride = sizeof(Vertex), offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
		deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		deviceContext->OMSetRenderTargets(1, &originalTarget.get(), originalDepth);
		deviceContext->RSSetViewports(viewportCount, originalViewport);
	}};
	rendContext = &replay; indexBuffer = replayIndices;
	for (size_t lane = 0; lane < result.color.size(); ++lane)
	{
		ComPtr<ID3D11RenderTargetView> target;
		if (FAILED(device->CreateTexture2D(&colorDesc, nullptr, &result.color[lane].get()))
			|| FAILED(device->CreateRenderTargetView(result.color[lane], nullptr, &target.get())))
		{ error = "pvr-replay-color-allocation"; return false; }
		if (lane == 2) for (auto& v : replay.verts) if (std::isfinite(v.z) && v.z > 0) v.z = 1.f / v.z;
		ComPtr<ID3D11Buffer> vertices;
		if (!buffer(replay.verts.data(), static_cast<UINT>(replay.verts.size() * sizeof(Vertex)), D3D11_BIND_VERTEX_BUFFER, vertices))
		{ error = "pvr-replay-vertex-allocation"; return false; }
		vertexBuffer = vertices;
		if (lane == 3)
		{
			rendContext = originalContext; vertexBuffer = originalVertices; indexBuffer = originalIndices;
		}
		resetContextState();
		deviceContext->CopyResource(result.color[lane], pvrReplayBase);
		deviceContext->OMSetRenderTargets(1, &target.get(), depthView);
		deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
		auto viewport = packet.viewport;
		if (lane == 1) viewport[12] += .05f; // Deliberately wrong camera/viewport assumption.
		configVertexShader(0.f, 0.f, viewport.data());
		auto capturedConstants = pvrReplayNativeVertexConstants;
		std::memcpy(&capturedConstants.transMatrix, viewport.data(), sizeof(capturedConstants.transMatrix));
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(deviceContext->Map(vtxConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{ error = "pvr-replay-vertex-constants-map"; return false; }
		std::memcpy(mapped.pData, &capturedConstants, sizeof(capturedConstants));
		deviceContext->Unmap(vtxConstants, 0);
		ID3D11Buffer *pixelBuffers[] = {pvrReplayPixelConstants, pxlPolyConstants};
		deviceContext->PSSetConstantBuffers(0, 2, pixelBuffers);
		deviceContext->PSSetShaderResources(1, 1, &paletteTextureView.get());
		deviceContext->PSSetSamplers(1, 1, &samplers->getSampler(false).get());
		deviceContext->PSSetShaderResources(2, 1, &fogTextureView.get());
		deviceContext->PSSetSamplers(2, 1, &samplers->getSampler(true).get());
		deviceContext->IASetInputLayout(mainInputLayout);
		const UINT stride = sizeof(Vertex), offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
		deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		n2Helper.resetCache(); drawStrips();
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	}
	result.priorFramebuffer = pvrReplayBase;
	return true;
}

bool DX11Renderer::prepareNeuralSceneColorTarget(ID3D11RenderTargetView *target)
{
	deviceContext->OMSetRenderTargets(1, &target, nullptr);
	if (rendContext->clearFramebuffer)
	{
		float clearColor[4];
		VO_BORDER_COL.getRGBColor(clearColor);
		clearColor[3] = 1.f;
		deviceContext->ClearRenderTargetView(target, clearColor);
	}
	else
	{
		if (!neuralRetainedSceneValid || !neuralRetainedSceneView)
			return false;
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
		quad->draw(neuralRetainedSceneView, samplers->getSampler(false));
		ID3D11ShaderResourceView *nullView = nullptr;
		deviceContext->PSSetShaderResources(0, 1, &nullView);
	}
	return true;
}

bool DX11Renderer::renderNeuralSceneColor(float rasterJitterX, float rasterJitterY)
{
	ID3D11RenderTargetView *target = neuralColor.targets[neuralExportSlot].get();
	if (!prepareNeuralSceneColorTarget(target))
		return false;
	deviceContext->OMSetRenderTargets(1, &target, neuralSceneDepthTarget);
	deviceContext->ClearDepthStencilView(neuralSceneDepthTarget,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
	configVertexShader(rasterJitterX, rasterJitterY);
	setupPixelShaderConstants();
	deviceContext->IASetInputLayout(mainInputLayout);
	ID3D11Buffer *buffer = vertexBuffer.get();
	const unsigned int stride = sizeof(Vertex);
	const unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	n2Helper.resetCache();
	drawStrips();
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	return true;
}

bool DX11Renderer::updateNeuralRetainedScene()
{
	if (!neuralRetainedSceneTarget || !fbTextureView)
	{
		neuralRetainedSceneValid = false;
		return false;
	}
	ID3D11RenderTargetView *target = neuralRetainedSceneTarget.get();
	deviceContext->OMSetRenderTargets(1, &target, nullptr);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	quad->draw(fbTextureView, samplers->getSampler(false));
	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	neuralRetainedSceneValid = true;
	return true;
}

bool DX11Renderer::renderNeuralExports(float rasterJitterX, float rasterJitterY)
{
	++neuralGuidanceReplayCount;
	acquireNeuralInputs();
	ID3D11UnorderedAccessView *nullUavs[2]{};
	deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(
		D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr,
		2, static_cast<UINT>(std::size(nullUavs)), nullUavs, nullptr);
	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);
	const float black[4]{};
	if (rasterJitterX == 0.f && rasterJitterY == 0.f)
	{
		deviceContext->OMSetRenderTargets(1,
			&neuralColor.targets[neuralExportSlot].get(), nullptr);
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
		deviceContext->ClearRenderTargetView(neuralColor.targets[neuralExportSlot], black);
		quad->draw(fbTextureView, samplers->getSampler(false));
		deviceContext->PSSetShaderResources(0, 1, &nullView);
	}
	else if (!renderNeuralSceneColor(rasterJitterX, rasterJitterY))
	{
		releaseNeuralInputs();
		return false;
	}

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
	configVertexShader(rasterJitterX, rasterJitterY);
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
			"marker=32x32-magenta-cyan marker_origin=%s marker_presentation=%s; synchronous developer mode",
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
			config::NeuralDlss5EvidenceMarkerBottomRight.get() ? "bottom-right" : "top-left",
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
	currentNeuralGuidanceFrameId = 0;
	releaseNeuralPresentation();
	neuralPresentationView.reset();
	if (!syncNeuralMode()) return;
	neuralQualityCapture.Configure(config::NeuralCaptureDirectory.get(),
		static_cast<std::uint32_t>(std::max(0, config::NeuralCaptureSkip.get())),
		static_cast<std::uint32_t>(std::clamp(config::NeuralCaptureFrames.get(), 0, 300)),
		config::NeuralLateOverlayProof.get(), static_cast<std::uint64_t>(std::max(0, config::NeuralCaptureStartFrame.get())),
		static_cast<std::uint64_t>(std::max(0, config::NeuralCaptureStartProducer.get())));
	neuralQualityCapture.SetSourceFrame(neuralInstrumentation.NextFrameId(),rendContext?rendContext->captureProducer.ordinal:0);
	if (neuralQualityCapture.ConsumeCaptureStart())
	{
		neuralInstrumentation.Discontinuity();
		NOTICE_LOG(RENDERER,
			"Neural bounded capture start: temporal history reset before first retained frame");
	}
	const auto contentRect = getNeuralContentRect();
	neuralInstrumentation.SetOverlayGameId(settings.content.gameId);
	const auto& capturedFrame = neuralInstrumentation.CaptureGeometry(*rendContext, {}, {}, width, height,
		static_cast<std::uint32_t>(std::max(0, contentRect.width)),
		static_cast<std::uint32_t>(std::max(0, contentRect.height)), contentRect, {});
	const auto mode = static_cast<NeuralMode>(std::clamp(activeNeuralMode, 0, 8));
	const bool publicTemporalMode = mode == NeuralMode::Dlaa
		|| mode == NeuralMode::SrQuality || mode == NeuralMode::SrBalanced
		|| mode == NeuralMode::SrPerformance || mode == NeuralMode::SrUltraPerformance
		|| mode == NeuralMode::Dlss5Experimental;
	const int overlayPolicy = std::clamp(config::NeuralOverlayPolicy.get(), 0, 2);
	const bool overlayProtectionNeeded = overlayPolicy != 2
		&& neuralInstrumentation.OverlayDrawCount() != 0;
	const bool retainedSceneReady = neuralRetainedSceneValid
		&& neuralDepthWidth == width && neuralDepthHeight == height;
	const bool rasterJitterEligible = publicTemporalMode
		&& (rendContext->clearFramebuffer || retainedSceneReady)
		&& !capturedFrame.predominantly2D && !overlayProtectionNeeded;
	Point2 rasterJitter{};
	if (rasterJitterEligible)
	{
		const auto phases = JitterPhaseCount(capturedFrame.renderWidth,
			capturedFrame.outputWidth);
		rasterJitter = HaltonJitter(neuralInstrumentation.AcceptedEvaluationCount(), phases);
	}
	neuralInstrumentation.SetCurrentJitter(rasterJitter);
	if (!hasLoggedNeuralRasterJitter || loggedNeuralRasterJitter != rasterJitterEligible)
	{
		hasLoggedNeuralRasterJitter = true;
		loggedNeuralRasterJitter = rasterJitterEligible;
		NOTICE_LOG(RENDERER,
			"Neural production raster jitter: active=%d mode=%u renderer=%s clear=%d overlay_draws=%u predominantly_2d=%d motion_space=unjittered",
			rasterJitterEligible ? 1 : 0, static_cast<unsigned>(mode),
			IsOitRenderer() ? "dx11-oit" : "dx11",
			rendContext->clearFramebuffer ? 1 : 0,
			static_cast<unsigned>(neuralInstrumentation.OverlayDrawCount()),
			capturedFrame.predominantly2D ? 1 : 0);
	}
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
	{
		publishNeuralStatus(SubmitStatus::Disabled,
			"conservative 2D/menu/FMV bypass");
		return;
	}
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
	if (!ensureNeuralResources())
	{
		publishNeuralStatus(SubmitStatus::RecoverableFailure,
			"guidance resource allocation failed");
		return;
	}
	neuralExportSlot = NextHistorySafeRingSlot(neuralExportSlot,
		neuralAcceptedGuidanceSlot, NeuralExportRingSize, hasNeuralAcceptedGuidance);
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::GuidanceBegin);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::GuidanceBegin);
	if (!renderNeuralExports(capturedFrame.jitterX, capturedFrame.jitterY))
	{
		publishNeuralStatus(SubmitStatus::RecoverableFailure,
			"guidance export failed");
		return;
	}
	if (!updateNeuralRetainedScene())
		WARN_LOG(RENDERER, "Neural retained-scene update failed; next retained frame will use zero jitter");
	currentNeuralGuidanceFrameId = capturedFrame.frameId;
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
	auto frame = neuralInstrumentation.AttachTextures(color, depth, motion, mask, confidence, drawId);
	neuralQualityCaptureMetadata = {};
	neuralQualityCaptureMetadata.frameId = frame.frameId;
	neuralQualityCaptureMetadata.producerIdentity = rendContext->captureProducer;
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
	neuralQualityCaptureMetadata.jitterX = frame.jitterX;
	neuralQualityCaptureMetadata.jitterY = frame.jitterY;
	neuralQualityCaptureMetadata.rasterJitterApplied = rasterJitterEligible;
	neuralQualityCaptureMetadata.rasterJitterReason = rasterJitterEligible
		? "separate-neural-scene-replay"
		: !publicTemporalMode ? "mode-requires-zero-jitter"
		: !rendContext->clearFramebuffer && !retainedSceneReady ? "retained-base-unavailable"
		: capturedFrame.predominantly2D ? "predominantly-2d"
		: overlayProtectionNeeded ? "protected-overlay-present"
		: "conservative-disabled";
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
	prepareRemakeAsyncFeed();
	if(const auto* asyncNeural=std::getenv("FLYCAST_REMAKE_ASYNC_NEURAL");asyncNeural&&std::strcmp(asyncNeural,"1")==0) {
		evaluateRemakeAsync(frame);
		return;
	}
	prepareRemakeCapture();
	const bool remakeInputApplied = applyRemakeCaptureInput(frame);
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::EvaluateBegin);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::EvaluateBegin);
	const auto status = neuralStage.TrySubmit(frame);
	neuralPerformance.Mark(deviceContext, GpuTimingPoint::EvaluateEnd);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		CaptureGpuTimingPoint::EvaluateEnd);
	neuralPerformance.RecordEvaluation(frame.frameId, status == SubmitStatus::Submitted,
		frame.resetHistory);
	neuralQualityCaptureMetadata.evaluationAccepted = status == SubmitStatus::Submitted;
	neuralQualityCaptureMetadata.submitStatus = neuralStage.GetStatusReason();
	logNeuralConsumerStatus(status);
	publishNeuralStatus(status);
	if (status == SubmitStatus::Submitted)
	{
		neuralInstrumentation.MarkEvaluated(frame.frameId);
		neuralAcceptedGuidanceSlot = neuralExportSlot;
		hasNeuralAcceptedGuidance = true;
		if (remakeInputApplied) {
			// Diagnostic reset-only inputs cannot become native correspondence history.
			hasNeuralAcceptedGuidance = false;
			neuralInstrumentation.Discontinuity();
		}
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
	if (config::NeuralCapturePvrMaterials.get() && neuralQualityCapture.CapturesCurrentFrame())
		textures.pvrMaterials = [this](const std::filesystem::path& path, std::string& error) {
			if (!config::NeuralCapturePvrPacket.get() || config::NeuralCaptureFrames.get() > 30
				|| !rendContext || rendContext->isRTT || IsOitRenderer() || activeNeuralSurface
				|| activeNeuralMode != 1 || config::EmulateFramebuffer.get())
			{ error = "material-capture-requires-bounded-native-d3d11-normal-packet"; return false; }
			return flycast::rend::neural::WritePvrMaterials(path, device, deviceContext,
				*rendContext, paletteTexture, PAL_RAM_CTRL, config::TextureFiltering.get(),
				config::AnisotropicFiltering.get(), neuralQualityCaptureMetadata.frameId,
				settings.content.gameId, error, materialShaderGlobals);
		};
	if (config::NeuralCapturePvrPacket.get() && neuralQualityCapture.CapturesCurrentFrame())
	{
		textures.pvrContext = rendContext;
		textures.pvrPacketRequested = true;
		if(!IsOitRenderer() && !config::EmulateFramebuffer.get() && rendContext && !rendContext->isRTT) {
			textures.remakeTextureReader=[this,remaining=std::size_t(64*1024*1024)](const flycast::rend::neural::PvrCapturedDraw& draw,
				std::vector<unsigned char>& bytes,std::string& reason) mutable {
				return flycast::rend::neural::ReadRemakeViewTexture(device,deviceContext,*rendContext,draw,remaining,bytes,reason);
			};
		}
		if (config::NeuralCapturePvrReplay.get())
			textures.pvrReplay = [this](const std::filesystem::path& path,
				flycast::rend::neural::PvrReplayTextures& result, std::string& error) {
				return replayPvrPacket(path, result, error);
			};
		const auto& viewport = matrices.GetNormalMatrix();
		for (int column = 0; column < 4; ++column)
			for (int row = 0; row < 4; ++row)
				textures.pvrViewport[column * 4 + row] = viewport[column][row];
	}
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
	const auto* compositeTest=std::getenv("FLYCAST_REMAKE_COMPOSITE_TEST");
	if(compositeTest&&std::strcmp(compositeTest,"1")==0&&!IsOitRenderer()
		&& !activeNeuralSurface && rendContext && !rendContext->isRTT && !config::EmulateFramebuffer.get()) {
		textures.remakeComposite=[this](const flycast::rend::neural::RemakeReturnedImage& image,
			ComPtr<ID3D11Texture2D>& output,std::string& error) {
			if(image.frame!=currentNeuralSourceFrameId||image.frame!=currentNeuralGuidanceFrameId
				||image.producer.epoch!=neuralQualityCaptureMetadata.producerIdentity.epoch
				||image.producer.ordinal!=neuralQualityCaptureMetadata.producerIdentity.ordinal
				||image.producer.cycle!=neuralQualityCaptureMetadata.producerIdentity.cycle
				||image.width!=640||image.height!=480||image.bgra.size()!=640*480*4
				||!fbTextureView||!neuralOverlayMask.views[neuralExportSlot]) {
				error="return-composite-frame-or-input";return false;
			}
			D3D11_TEXTURE2D_DESC nativeDesc{};fbTex->GetDesc(&nativeDesc);
			if(nativeDesc.Width!=640||nativeDesc.Height!=480) {error="return-composite-native-size";return false;}
			const auto& shader=shaders->getNeuralOverlayCompositePixelShader();
			if(!shader){error="return-composite-shader";return false;}
			D3D11_TEXTURE2D_DESC desc{};desc.Width=640;desc.Height=480;desc.MipLevels=1;desc.ArraySize=1;
			desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
			desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=image.bgra.data();initial.SysMemPitch=640*4;
			ComPtr<ID3D11Texture2D> target;ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DeviceContext> deferred;
			if(FAILED(device->CreateTexture2D(&desc,&initial,&target.get()))
				||FAILED(device->CreateRenderTargetView(target,nullptr,&rtv.get()))
				||FAILED(device->CreateDeferredContext(0,&deferred.get()))) {error="return-composite-resources";return false;}
			// Isolated command list preserves the native target and all immediate state.
			Quad diagnosticQuad;diagnosticQuad.init(device,deferred,shaders);
			D3D11_VIEWPORT viewport{0,0,640,480,0,1};deferred->RSSetViewports(1,&viewport);
			deferred->OMSetRenderTargets(1,&rtv.get(),nullptr);
			deferred->OMSetBlendState(blendStates.getState(false),nullptr,0xffffffff);
			ID3D11ShaderResourceView* views[]={fbTextureView.get(),neuralOverlayMask.views[neuralExportSlot].get()};
			diagnosticQuad.drawCustom(shader,views,2,samplers->getSampler(false));
			ComPtr<ID3D11CommandList> commands;
			if(FAILED(deferred->FinishCommandList(FALSE,&commands.get()))){error="return-composite-command-list";return false;}
			deviceContext->ExecuteCommandList(commands,TRUE);output=std::move(target);return true;
		};
	}
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
	{
		if (textures.pvrPacketRequested) {
			const auto frame = neuralQualityCaptureMetadata.frameId;
			const auto* snapshot = neuralQualityCapture.CapturedPvrSnapshot(frame);
			NOTICE_LOG(RENDERER,"PVR live Remix packet: frame=%llu status=%s",
				static_cast<unsigned long long>(frame),neuralQualityCapture.RemakePacketStatus().c_str());
			if(const auto* scene=neuralQualityCapture.CapturedRemakeViewScene(frame)) {
				std::size_t triangles=0;for(const auto& mesh:scene->meshes)triangles+=mesh.vertices.size()/3;
				NOTICE_LOG(RENDERER,"PVR live view scene: frame=%llu meshes=%zu triangles=%zu omitted-draws=%zu projection-error=%g scope=%s",
					static_cast<unsigned long long>(frame),scene->meshes.size(),triangles,scene->omittedDraws,scene->maximumProjectionError,scene->scope);
			} else NOTICE_LOG(RENDERER,"PVR live view scene: frame=%llu unsupported; native unchanged",static_cast<unsigned long long>(frame));
			if (snapshot && !snapshot->sourceVertices.empty()) {
				const auto coverage=flycast::rend::neural::MeasurePvrSourceCoverage(*snapshot);
				NOTICE_LOG(RENDERER,"PVR transform coverage: frame=%llu complete-xyz=%zu common-origin=%zu complete-draws=%zu partial-draws=%zu diagnostic-only",
					static_cast<unsigned long long>(frame),coverage.completeVertices,coverage.commonOriginVertices,coverage.completeDraws,coverage.partialDraws);
				size_t stores=0,reads=0,producers=0,transformComponents=0;
				std::vector<u32> storePcs;
				for(const auto& vertex:snapshot->sourceVertices)
				{
					stores+=std::all_of(vertex.copy.xyzStorePc.begin(),vertex.copy.xyzStorePc.end(),[](u32 pc){return pc!=0;});
					reads+=std::all_of(vertex.copy.xyzSourceRam.begin(),vertex.copy.xyzSourceRam.end(),[](u32 address){return address!=0;});
					producers+=std::all_of(vertex.copy.xyzRamProducerPc.begin(),vertex.copy.xyzRamProducerPc.end(),[](u32 pc){return pc!=0;});
					for(const auto& transform:vertex.copy.xyzTransforms)transformComponents+=transform.has_value();
					for(auto pc:vertex.copy.xyzStorePc) if(pc && std::find(storePcs.begin(),storePcs.end(),pc)==storePcs.end() && storePcs.size()<32)
						storePcs.push_back(pc);
				}
				NOTICE_LOG(RENDERER, "PVR owned source snapshot: frame=%llu producer=%llu joined-vertices=%zu",
					static_cast<unsigned long long>(frame), static_cast<unsigned long long>(snapshot->sourceProducer.ordinal),
					snapshot->sourceVertices.size());
				NOTICE_LOG(RENDERER,"PVR executed XYZ stores: frame=%llu complete-vertices=%zu",
					static_cast<unsigned long long>(frame),stores);
				NOTICE_LOG(RENDERER,"PVR live RAM read linkage: frame=%llu complete-vertices=%zu",
					static_cast<unsigned long long>(frame),reads);
				NOTICE_LOG(RENDERER,"PVR observed RAM producers: frame=%llu complete-vertices=%zu diagnostic-only",
					static_cast<unsigned long long>(frame),producers);
				NOTICE_LOG(RENDERER,"PVR owned vertex transforms: frame=%llu components=%zu observed-dependency-only",
					static_cast<unsigned long long>(frame),transformComponents);
				for(auto pc:storePcs) NOTICE_LOG(RENDERER,"PVR observed XYZ store instruction: frame=%llu pc=%08x diagnostic-only",
					static_cast<unsigned long long>(frame),pc);
			}
			const bool valid = snapshot && textures.pvrContext
				&& snapshot->game == settings.content.gameId
				&& !neuralQualityCapture.CapturedPvrSnapshot(frame + 1)
				&& flycast::rend::neural::PvrSnapshotTextureBindingsMatch(*textures.pvrContext, *snapshot);
			bool wrongUploadRejected = false, wrongRttRejected = false;
			if (valid) {
				flycast::rend::neural::PvrDecodedPacket altered;
				altered.draws = snapshot->draws; // Validator reads bindings only; never alter live cache.
				for (auto& draw : altered.draws) if (draw.texture) {
					draw.texture->upload ^= 1u;
					wrongUploadRejected = !flycast::rend::neural::PvrSnapshotTextureBindingsMatch(*textures.pvrContext, altered);
					draw.texture->upload ^= 1u;
					draw.texture->rtt ^= 1u;
					wrongRttRejected = !flycast::rend::neural::PvrSnapshotTextureBindingsMatch(*textures.pvrContext, altered);
					break;
				}
			}
			NOTICE_LOG(RENDERER, "PVR owned snapshot: frame=%llu bindings=%s wrong-frame-rejected=%s wrong-upload-rejected=%s wrong-rtt-rejected=%s",
				static_cast<unsigned long long>(frame), valid ? "verified" : "FAILED",
				neuralQualityCapture.CapturedPvrSnapshot(frame + 1) ? "no" : "yes",
				wrongUploadRejected ? "yes" : "no-or-unavailable", wrongRttRejected ? "yes" : "no-or-unavailable");
		}
		NOTICE_LOG(RENDERER,
			"Neural quality capture: game=%s frame=%llu captured=%u submit=%s synchronous-developer-only",
			settings.content.gameId.c_str(),
			static_cast<unsigned long long>(neuralQualityCaptureMetadata.frameId),
			afterCount, neuralQualityCaptureMetadata.submitStatus.c_str());
	}
}

void DX11Renderer::captureNeuralLateOverlayFrame()
{
	if (!config::NeuralLateOverlayProof.get()
		|| config::NeuralCaptureDirectory.get().empty()
		|| config::NeuralCaptureFrames.get() <= 0
		|| currentNeuralSourceFrameId == 0)
		return;
	ID3D11Resource *resource = nullptr;
	DX11Context::Instance()->getRenderTarget()->GetResource(&resource);
	if (!resource)
		return;
	ID3D11Texture2D *backBuffer = nullptr;
	resource->QueryInterface(__uuidof(ID3D11Texture2D),
		reinterpret_cast<void **>(&backBuffer));
	resource->Release();
	if (!backBuffer)
		return;
	std::string error;
	const bool captured = neuralQualityCapture.CapturePresentedWithFlycastOverlays(
		device, deviceContext, backBuffer, currentNeuralSourceFrameId,
		getNeuralContentRect(), error);
	backBuffer->Release();
	if (!captured)
		WARN_LOG(RENDERER, "Neural late-overlay proof failed: %s", error.c_str());
}

void DX11Renderer::beginNeuralPerformanceFrame()
{
	const bool synchronousCapture = !config::NeuralCaptureDirectory.get().empty()
		&& config::NeuralCaptureFrames.get() > 0;
	neuralQualityCapture.Configure(config::NeuralCaptureDirectory.get(),
		static_cast<std::uint32_t>(std::max(0, config::NeuralCaptureSkip.get())),
		static_cast<std::uint32_t>(std::clamp(config::NeuralCaptureFrames.get(), 0, 300)),
		config::NeuralLateOverlayProof.get(), static_cast<std::uint64_t>(std::max(0, config::NeuralCaptureStartFrame.get())),
		static_cast<std::uint64_t>(std::max(0, config::NeuralCaptureStartProducer.get())));
	neuralQualityCapture.SetSourceFrame(neuralInstrumentation.NextFrameId(),rendContext?rendContext->captureProducer.ordinal:0);
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
		std::clamp(config::NeuralFailureInjection.get(), 0, 6),
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
	count += neuralSceneDepthTexture ? 1u : 0u;
	count += neuralSceneDepthTarget ? 1u : 0u;
	count += neuralRetainedSceneTexture ? 1u : 0u;
	count += neuralRetainedSceneTarget ? 1u : 0u;
	count += neuralRetainedSceneView ? 1u : 0u;
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
	const auto effects=flycast::rend::neural::CountRemakeEffects(std::array<const flycast::rend::neural::RemakeOitEffects*,7>{
		remakeCurrentEffects.get(),remakeAsyncOverlaySources[0].effects.get(),remakeAsyncOverlaySources[1].effects.get(),
		remakeAsyncOverlaySources[2].effects.get(),remakeAsyncOverlaySources[3].effects.get(),
		remakeAsyncAcceptedOverlay.effects.get(),remakeEvaluatedOverlay.effects.get()});
	count+=effects.objects;
	count+=remakeMotionRaster.OwnedObjects();
	countArray(remakeAcceptedRaster.textures);
	countArray(remakeAcceptedRaster.views);
	return count;
}

void DX11Renderer::publishNeuralStatus(
	flycast::rend::neural::SubmitStatus status, const char *reason)
{
	using namespace flycast::rend::neural;
	lastNeuralSubmitStatus = status;
	const std::string nextReason = reason ? reason : neuralStage.GetStatusReason();
	neuralLiveReason = nextReason;
	LiveStatus live;
	live.rendererAvailable = true;
	live.active = activeNeuralMode > 0 && neuralInstrumentation.IsEnabled();
	live.d3d11On12 = activeNeuralSurface;
	live.conservativeBypass = neural2DBypassActive;
	live.overlayProtection = loggedOverlayActive;
	live.debugViewActive = loggedNeuralDebugActive;
	live.mode = static_cast<NeuralMode>(std::clamp(activeNeuralMode, 0, 8));
	live.api = activeNeuralSurface ? Api::D3D12 : Api::D3D11;
	live.lastSubmit = status;
	live.stage = neuralStage.GetStats();
	live.reason = neuralLiveReason;
	live.renderWidth = loggedNeuralRenderWidth;
	live.renderHeight = loggedNeuralRenderHeight;
	live.outputWidth = loggedNeuralOutputWidth;
	live.outputHeight = loggedNeuralOutputHeight;
	live.overlayDraws = static_cast<std::uint32_t>(neuralInstrumentation.OverlayDrawCount());
	live.debugView = static_cast<std::uint32_t>(
		std::clamp(config::NeuralDebugView.get(), 0, 7));
	live.qualityProfile = std::clamp(config::NeuralQualityProfile.get(), 0, 3);
	live.dlssPreset = activeNeuralPreset;
	live.rasterJitterX = neuralQualityCaptureMetadata.jitterX;
	live.rasterJitterY = neuralQualityCaptureMetadata.jitterY;
	live.rasterJitterApplied = neuralQualityCaptureMetadata.rasterJitterApplied;
	live.sourceFrameId = currentNeuralSourceFrameId;
	live.remakeSessionRequested = !remakeAsyncToken.empty();
	live.remakeRestartRequired = remakeAsyncStopped;
	live.remakeChannelOpen = remakeAsyncChannel.IsOpen();
	live.remakeReturnedFrame = remakeAsyncReturned ? remakeAsyncReturned->frame : 0;
	live.presentedOutputFrameId = lastPresentedNeuralFrameId;
	PublishLiveStatus(std::move(live));
}

bool DX11Renderer::syncNeuralMode()
{
	using namespace flycast::rend::neural;
	const int requestedMode = std::clamp(config::NeuralMode.get(), 0, 8);
	if (requestedMode != 0 && !neuralInputLayout)
	{
		const ComPtr<ID3DBlob> blob = shaders->getNeuralVertexShaderBlob();
		if (!blob || FAILED(device->CreateInputLayout(NeuralLayout,
			std::size(NeuralLayout), blob->GetBufferPointer(), blob->GetBufferSize(),
			&neuralInputLayout.get())))
		{
			publishNeuralStatus(SubmitStatus::RecoverableFailure,
				"neural input-layout allocation failed");
			return false;
		}
	}
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
			{
				publishNeuralStatus(SubmitStatus::Disabled,
					"developer evidence capture arm delay");
				return false;
			}
		}
	}
	else
		neuralEvidenceArmDeadlineMs = 0;
	const bool surfaceRequested = config::NeuralD3D12Surface.get();
	const bool requestedSurface = surfaceRequested && DX11Context::Instance()->isD3D11On12();
	const int configuredPreset = config::NeuralDlssPreset.get();
	const int requestedPreset = configuredPreset == 10 || configuredPreset == 11
		? configuredPreset : 0;
	const int requestedFailureInjection = std::clamp(config::NeuralFailureInjection.get(), 0, 6);
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
			stageConfig.dlss5EvidenceMarkerBottomRight =
				config::NeuralDlss5EvidenceMarkerBottomRight.get();
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
		{
			releaseNeuralResources();
			neuralInputLayout.reset();
		}
		publishNeuralStatus(SubmitStatus::Disabled,
			requestedMode == 0 ? "neural rendering is off" : "waiting for a neural frame");
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
	publishNeuralStatus(status,
		"framebuffer-direct content uses native fallback unless explicitly supported");
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

#ifdef FLYCAST_ENABLE_NEURAL
flycast::rend::neural::RemakeDisplayDecision DX11Renderer::selectRemakePreview(bool permitted)
{
	using namespace flycast::rend::neural;
	const auto current=currentNeuralSourceFrameId;
	const auto* preview=std::getenv("FLYCAST_REMAKE_ASYNC_PRESENT");
	const auto* asyncNeural=std::getenv("FLYCAST_REMAKE_ASYNC_NEURAL");
	const bool evaluatedRequested=asyncNeural&&std::strcmp(asyncNeural,"1")==0;
	const bool enabled=preview&&std::strcmp(preview,"1")==0&&permitted&&!remakeAsyncStopped
		&&rendContext&&!rendContext->isRTT&&!config::EmulateFramebuffer.get()
		&&RemakeRendererAllowed(IsOitRenderer(),std::getenv("FLYCAST_REMAKE_ASYNC_OIT"))
		&&current&&currentNeuralGuidanceFrameId==current&&config::NeuralCaptureFrames.get()==0
		&&neuralQualityCaptureMetadata.renderWidth==640&&neuralQualityCaptureMetadata.renderHeight==480
		&&neuralQualityCaptureMetadata.outputWidth==640&&neuralQualityCaptureMetadata.outputHeight==480;
	try {
		std::uint64_t candidate=0;
		const auto& source=evaluatedRequested?remakeEvaluatedSource:remakeAsyncReturned;
		const auto& overlay=evaluatedRequested?remakeEvaluatedOverlay:remakeAsyncAcceptedOverlay;
		if(enabled&&source&&(!RemakeNativeEffectsRequested()||evaluatedRequested)
			&&(!evaluatedRequested||remakeEvaluatedView)&&overlay.identity.Matches(*source,current,rendContext->captureProducer)) {
			const auto& image=*source;candidate=image.frame;
			if(remakeCompositeFrame!=candidate||remakeCompositeEvaluated!=evaluatedRequested) {
				const auto& shader=shaders->getNeuralOverlayCompositePixelShader();
				if(!shader)throw std::runtime_error("overlay shader unavailable");
				D3D11_TEXTURE2D_DESC desc{};desc.Width=640;desc.Height=480;desc.MipLevels=desc.ArraySize=1;
				desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
				desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
				D3D11_SUBRESOURCE_DATA initial{image.bgra.data(),640*4,0};
				ComPtr<ID3D11Texture2D> target;ComPtr<ID3D11RenderTargetView> rtv;
				ComPtr<ID3D11ShaderResourceView> view;ComPtr<ID3D11DeviceContext> deferred;
				if(FAILED(device->CreateTexture2D(&desc,&initial,&target.get()))
					||FAILED(device->CreateRenderTargetView(target,nullptr,&rtv.get()))
					||FAILED(device->CreateShaderResourceView(target,nullptr,&view.get()))
					||FAILED(device->CreateDeferredContext(0,&deferred.get())))throw std::runtime_error("composite resources unavailable");
				Quad composite;composite.init(device,deferred,shaders);
				D3D11_VIEWPORT viewport{0,0,640,480,0,1};deferred->RSSetViewports(1,&viewport);
				deferred->OMSetRenderTargets(1,&rtv.get(),nullptr);
				deferred->OMSetBlendState(blendStates.getState(false),nullptr,0xffffffff);
				if(evaluatedRequested)composite.draw(remakeEvaluatedView,samplers->getSampler(false),nullptr);
				ID3D11ShaderResourceView* views[]={overlay.colorView,overlay.maskView};
				composite.drawCustom(shader,views,2,samplers->getSampler(false));
				ComPtr<ID3D11CommandList> commands;
				if(FAILED(deferred->FinishCommandList(FALSE,&commands.get())))throw std::runtime_error("composite command list failed");
				deviceContext->ExecuteCommandList(commands,TRUE);
				remakeCompositeTexture=std::move(target);remakeCompositeView=std::move(view);remakeCompositeFrame=candidate;
				remakeCompositeEvaluated=evaluatedRequested;
			}
		}
		const bool wasActive=remakePresentationPolicy.Active(),wasFailed=remakePresentationPolicy.Failed();
		const auto* compareStart=std::getenv("FLYCAST_REMAKE_COMPARE_START_FRAME");
		const auto* captureRoot=std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE");
		const bool boundedCapture=captureRoot&&*captureRoot
			&&RemakeMovingCaptureEnabled(std::getenv("FLYCAST_REMAKE_MOVING_CAPTURE"));
		const bool captureBoundary=compareStart&&current
			&&RemakeComparisonEligible(compareStart,current,boundedCapture)
			&&!RemakeComparisonEligible(compareStart,current-1,boundedCapture);
		auto decision=remakePresentationPolicy.Choose(current,candidate,enabled,captureBoundary);
		if((wasActive&&!enabled)||(!wasFailed&&remakePresentationPolicy.Failed()))
			NOTICE_LOG(RENDERER,"Remake presentation stopped: current=%llu candidate=%llu enabled=%d permitted=%d guidance=%llu source=%llu producer=%llu latched=%d",
				(unsigned long long)current,(unsigned long long)candidate,enabled,permitted,
				(unsigned long long)currentNeuralGuidanceFrameId,(unsigned long long)(source?source->frame:0),
				(unsigned long long)(rendContext?rendContext->captureProducer.ordinal:0),remakePresentationPolicy.Failed());
		if(decision.kind==RemakeDisplayKind::HoldNative) {
			if(remakeWarmupNative.identity.frame!=decision.frame) {
				if(decision.frame!=current)throw std::runtime_error("warmup source unavailable");
				acquireNeuralInputs();
				const bool copied=CaptureRemakeOverlay(device,deviceContext,fbTex,neuralOverlayMask.textures[neuralExportSlot],current,rendContext->captureProducer,remakeWarmupNative);
				releaseNeuralInputs();
				if(!copied)throw std::runtime_error("warmup snapshot unavailable");
			}
			remakeDisplayedView=remakeWarmupNative.colorView;remakeDisplayedFrame=decision.frame;remakeDisplayedEvaluated=false;
		}else if(decision.kind==RemakeDisplayKind::Remake) {
			if(decision.frame==remakeCompositeFrame){remakeDisplayedView=remakeCompositeView;remakeDisplayedFrame=decision.frame;remakeDisplayedEvaluated=remakeCompositeEvaluated;}
			if(!remakeDisplayedView||remakeDisplayedFrame!=decision.frame)throw std::runtime_error("display source unavailable");
			remakeWarmupNative={};
		}else {remakeDisplayedView.reset();remakeWarmupNative={};remakeDisplayedEvaluated=false;}
		return decision;
	}catch(const std::exception& error) {
		remakePresentationPolicy.Fail();remakeDisplayedView.reset();remakeWarmupNative={};
		WARN_LOG(RENDERER,"Remake preview latched fallback: %s",error.what());
		return {RemakeDisplayKind::Fallback,current};
	}
}

void DX11Renderer::drainRemakeReturns(std::uint64_t currentFrame,const flycast::rend::neural::ProducerIdentity& producer)
{
	using namespace flycast::rend::neural;
	// D-213/D-214: the return worker received and prepared these in order; the
	// identity gate here is the one the synchronous path applied. Only one
	// image is pending at a time: an image not yet evaluated is never
	// overwritten by a later prepared one (it ages out after8 frames instead).
	for(;;) {
		if(remakeAsyncReturned&&remakeAsyncReturned->frame>remakeLastEvaluationAttempt)break;
		auto next=remakeReturnWorker.Next();
		if(!next)break;
		auto& prepared=*next;const auto& returned=prepared.returned;
		auto& overlay=remakeAsyncOverlaySources[returned.source.sequence%RemakeOverlaySlots];
		const bool accepted=returned.frame<=currentFrame&&currentFrame-returned.frame<=8
			&&returned.producer.epoch==producer.epoch
			&&overlay.colorView&&overlay.maskView&&overlay.identity.Matches(returned,currentFrame,producer)
			&&prepared.wellFormed;
		NOTICE_LOG(RENDERER,"Remake async return: source=%llu producer=%llu sequence=%llu current=%llu retained=%d presentation=false",
			(unsigned long long)returned.frame,(unsigned long long)returned.producer.ordinal,
			(unsigned long long)returned.source.sequence,(unsigned long long)currentFrame,accepted);
		if(!accepted)continue;
		NOTICE_LOG(RENDERER,"Remake async overlay retained: frame=%llu sequence=%llu original_native=true original_mask=true presentation=false",
			(unsigned long long)overlay.identity.frame,(unsigned long long)overlay.identity.receipt.sequence);
		if(const auto* timing=std::getenv("FLYCAST_REMAKE_CPU_TIMING");timing&&std::strcmp(timing,"1")==0&&remakeReturnTimingCount<600) {
			++remakeReturnTimingCount;
			NOTICE_LOG(RENDERER,"Remake CPU scope: frame=%llu stage=return-worker elapsed_ms=%.6f includes_driver_wait=false diagnostic=true",
				(unsigned long long)returned.frame,prepared.workerMs);
		}
		RemakePreparedReturn ready;ready.frame=returned.frame;ready.sequence=returned.source.sequence;
		ready.previousFrame=prepared.previousFrame;ready.temporal=prepared.temporal;ready.streamReady=prepared.streamReady;
		ready.inputReady=prepared.inputReady;ready.streamError=std::move(prepared.streamError);
		ready.stream=std::move(prepared.stream);ready.input=std::move(prepared.input);
		remakeAsyncReturned=std::move(prepared.returned);remakeAsyncAcceptedOverlay=std::move(overlay);overlay={};
		remakePreparedReturn=std::move(ready);
	}
}

void DX11Renderer::prepareRemakeAsyncFeed()
{
	try {
	using namespace flycast::rend::neural;
	const auto* requested=std::getenv("FLYCAST_REMAKE_ASYNC_CHANNEL");
	if(!requested||!*requested)return;
	if(remakeSessionRoot!=requested&&remakeAsyncToken!=requested) {
		if(const auto* root=std::getenv("FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT");root&&*root) {
			std::string error;
			if(!PrepareLockedRemakeInput(root,error)) {
				WARN_LOG(RENDERER,"Remake archive preparation failed: %s; native fallback",error.c_str());return;
			}
			NOTICE_LOG(RENDERER,"Remake archive index prepared before session request; validation remains per input");
		}
	}
	std::string token(requested);
	const auto* managed=std::getenv("FLYCAST_REMAKE_MANAGED_SESSION");
	if(managed&&std::strcmp(managed,"1")==0) {
		if(remakeSessionRoot!=requested||remakeSessionRenewalRequested) {
			std::string error;
			if(!RequestRemakeSession(requested,token,error))return;
			remakeSessionRoot=requested;remakeSessionRenewalRequested=false;
			NOTICE_LOG(RENDERER,"Remake fresh session requested: %s",token.c_str());
		} else token=remakeAsyncToken;
	}
	if(token!=remakeAsyncToken) {
		remakeAsyncTextures.Reset();remakeAsyncChannel.Close();resetRemakeAsyncFrames();
		remakeAsyncToken=token;remakeAsyncEpoch=0;remakeAsyncStopped=false;
	}
	if(remakeAsyncStopped||!RemakeRendererAllowed(IsOitRenderer(),std::getenv("FLYCAST_REMAKE_ASYNC_OIT"))
		||!rendContext||rendContext->isRTT||config::EmulateFramebuffer.get())return;
	const auto& metadata=neuralQualityCaptureMetadata;const auto producer=rendContext->captureProducer;
	const auto skip=[&](const char* stage,const std::string& reason) {
		const auto* diagnostics=std::getenv("FLYCAST_REMAKE_ASYNC_DIAGNOSTICS");
		if(diagnostics&&std::strcmp(diagnostics,"1")==0)
			NOTICE_LOG(RENDERER,"Remake async skip: frame=%llu producer=%llu stage=%s reason=%s",
				(unsigned long long)metadata.frameId,(unsigned long long)producer.ordinal,stage,reason.c_str());
	};
	// D-211: apply completed feed-worker results in source order before this
	// frame's return handling. History retirement and logging stay here.
	for(auto& fed:remakeFeedWorker.Drain()) {
		if(const auto* timing=std::getenv("FLYCAST_REMAKE_CPU_TIMING");timing&&std::strcmp(timing,"1")==0&&remakeFeedTimingCount<600) {
			++remakeFeedTimingCount;
			NOTICE_LOG(RENDERER,"Remake CPU scope: frame=%llu stage=feed-worker elapsed_ms=%.6f includes_driver_wait=false diagnostic=true",
				(unsigned long long)fed.frame,fed.workerMs);
			if(fed.packetMs>0)
				NOTICE_LOG(RENDERER,"Remake CPU scope: frame=%llu stage=feed-worker-packet-build elapsed_ms=%.6f includes_driver_wait=false diagnostic=true",
					(unsigned long long)fed.frame,fed.packetMs);
		}
		if(fed.viewCut) {
			// A large single-frame basis jump with continuing support is a view cut
			// inside the same arena. Keep the anchor and light; retire histories so
			// nothing reprojects across the cut.
			retireRemakeTemporalHistory();
			NOTICE_LOG(RENDERER,"Remake anchor view cut: source=%llu generation=%u rotation_from_last_deg=%.6g translation_from_last=%.6g frames_since_last=%llu shared_last=%u history_retired=true anchor_retained=true",
				(unsigned long long)fed.frame,fed.generation,fed.support.rotationFromLastDegrees,fed.support.translationFromLast,(unsigned long long)fed.support.framesSinceLast,unsigned(fed.support.sharedLast));
		}
		if(!fed.stage.empty()) {
			if(const auto* diagnostics=std::getenv("FLYCAST_REMAKE_ASYNC_DIAGNOSTICS");diagnostics&&std::strcmp(diagnostics,"1")==0)
				NOTICE_LOG(RENDERER,"Remake async skip: frame=%llu producer=%llu stage=%s reason=%s",
					(unsigned long long)fed.frame,(unsigned long long)fed.producer.ordinal,fed.stage.c_str(),fed.error.c_str());
			if(fed.stage=="camera-anchor"&&fed.support.rejected&&fed.error=="anchor-source-support-changed")
				NOTICE_LOG(RENDERER,"Remake anchor support report: source=%llu reference_producer=%llu points=%u reference=%u shared_reference=%u last_accepted=%u shared_last=%u rotation_from_reference_deg=%.6g translation_from_reference=%.6g rotation_from_last_deg=%.6g translation_from_last=%.6g bases=%u",
					(unsigned long long)fed.frame,(unsigned long long)fed.referenceOrdinal,
					unsigned(fed.support.points),unsigned(fed.support.reference),unsigned(fed.support.sharedReference),
					unsigned(fed.support.lastAccepted),unsigned(fed.support.sharedLast),
					fed.support.rotationFromReferenceDegrees,fed.support.translationFromReference,
					fed.support.rotationFromLastDegrees,fed.support.translationFromLast,unsigned(fed.support.bases));
			if(fed.supportChanged) {
				// Genuine source-view cut: the worker retired its fixed view; retire
				// temporal/raster history, pending returns and presentation carry-over
				// here, keeping the channel and helper.
				retireRemakeHistory();
				NOTICE_LOG(RENDERER,"Remake anchor support changed: in-session re-anchor generation=%u regenerated=%d history_reset=true presentation_retired=true channel_retained=true",
					fed.generation,fed.regenerated);
			}
			continue;
		}
		if(!fed.registeredTextures.empty()) {
			auto grown=remakeSentTextures?std::make_shared<RemakeSentTextureSet>(*remakeSentTextures):std::make_shared<RemakeSentTextureSet>();
			for(const auto& identity:fed.registeredTextures)
				grown->insert({identity.id,identity.generation,identity.paletteGeneration,identity.rttGeneration});
			remakeSentTextures=std::move(grown);remakeSentTextureBytes+=fed.registeredBytes;
		}
		if(fed.alphaOwnership)
			NOTICE_LOG(RENDERER,"Remake alpha ownership: source=%llu excluded_native_draws=%u source-qualified=true",
				(unsigned long long)fed.frame,unsigned(fed.overlay.alphaEffectSelections.size()));
		auto overlay=std::move(fed.overlay);
		if(fed.temporalScene)overlay.temporalScene=std::move(fed.temporalScene);
		remakeReturnWorker.RegisterScene(fed.receipt.sequence,overlay.temporalScene);
		if(fed.capturedPacket)overlay.captureScene=std::move(fed.capturedPacket);
		remakeAsyncOverlaySources[fed.receipt.sequence%RemakeOverlaySlots]=std::move(overlay);
		if(fed.anchored)
			NOTICE_LOG(RENDERER,"Remake observed camera: source=%llu reference_producer=%llu generation=%u origin=%.9g,%.9g,%.9g position=%.9g,%.9g,%.9g world_recovered=false projection_max_pixels=%.9g support_points=%u shared_reference=%u shared_last=%u rotation_from_last_deg=%.6g translation_from_last=%.6g frames_since_last=%llu bases=%u moving_points=%u lineage_basis=%d offscreen_accepted=%u offscreen_max_pixels=%.6g offscreen_max_effect_pixels=%.6g offscreen_max_tangential_pixels=%.6g offscreen_max_diagonals=%.6g",
				(unsigned long long)fed.frame,(unsigned long long)fed.referenceOrdinal,fed.generation,
				fed.origin.x,fed.origin.y,fed.origin.z,fed.cameraPosition.x,fed.cameraPosition.y,fed.cameraPosition.z,fed.projectionMaxPixels,
				unsigned(fed.support.points),unsigned(fed.support.sharedReference),unsigned(fed.support.sharedLast),
				fed.support.rotationFromLastDegrees,fed.support.translationFromLast,(unsigned long long)fed.support.framesSinceLast,
				unsigned(fed.support.bases),unsigned(fed.support.movingPoints),int(fed.support.lineageSelected),
				unsigned(fed.projection.offscreenAccepted),fed.projection.maxPixels,fed.projection.maxEffect,fed.projection.maxTangential,fed.projection.maxDiagonals);
		NOTICE_LOG(RENDERER,"Remake async publish: frame=%llu producer=%llu sequence=%llu bytes=%u digest=%llu capture=false wait=false presentation=false",
			(unsigned long long)fed.frame,(unsigned long long)fed.producer.ordinal,(unsigned long long)fed.receipt.sequence,
			fed.receipt.bytes,(unsigned long long)fed.receipt.digest);
	}
	if(!producer.Available()||metadata.predominantly2D||metadata.gameId!="T1401N"
		||metadata.renderWidth!=640||metadata.renderHeight!=480)return;
	if(const auto* start=std::getenv("FLYCAST_REMAKE_ASYNC_START_PRODUCER");start&&*start) {
		char* end=nullptr;const auto ordinal=std::strtoul(start,&end,10);
		if(*end||ordinal>10000000||producer.ordinal<ordinal)return;
	}
	static thread_local unsigned feedTimingCount=0;
	RemakeCpuScope feedTiming("scene-feed",metadata.frameId,feedTimingCount);
	if(remakeAsyncEpoch&&remakeAsyncEpoch!=producer.epoch) {
		remakeSessionRenewalRequested=true;
		remakeAsyncTextures.Reset();remakeAsyncChannel.Close();resetRemakeAsyncFrames();remakeAsyncStopped=true;
		WARN_LOG(RENDERER,"Remake async epoch changed: new consumer token required; native presentation retained");return;
	}
	remakeAsyncEpoch=producer.epoch;std::string error;
	if(!remakeAsyncChannel.IsOpen()&&!remakeAsyncChannel.OpenPublisher(token,error)) {
		if(error=="channel-publisher-already-claimed") {
			remakeAsyncStopped=true;
			WARN_LOG(RENDERER,"Remake session token already claimed: relaunch required; native fallback retained");
		}
		return;
	}
	// D-214: the return worker receives and prepares returned images itself;
	// a closed channel is handled here exactly as the synchronous receive did.
	remakeReturnWorker.Start();remakeReturnWorker.Attach(&remakeAsyncChannel);
	if(!remakeFrameBudgetConfigured) {
		remakeFrameBudgetConfigured=true;
		if(const auto* budget=std::getenv("FLYCAST_REMAKE_FRAME_BUDGET_MS");budget&&*budget) {
			char* end=nullptr;const double ms=std::strtod(budget,&end);
			if(!*end&&ms>0&&ms<1000)remakeFrameBudget.Configure(ms);
		}
		NOTICE_LOG(RENDERER,"Remake frame budget: budget_ms=%.3f enabled=%d policy=explicit-skip-or-defer-never-slower-emulation",
			remakeFrameBudget.BudgetMs(),remakeFrameBudget.Enabled());
	}
	remakeFrameBudget.BeginFrame();
	if(remakeFrameBudget.Enabled()&&++remakeFrameBudgetFrames%300==0)
		NOTICE_LOG(RENDERER,"Remake frame budget report: frame=%llu budget_ms=%.3f credit_ms=%.3f feed_estimate_ms=%.3f evaluate_estimate_ms=%.3f feed_runs=%llu feed_skips=%llu evaluate_runs=%llu evaluate_deferrals=%llu",
			(unsigned long long)metadata.frameId,remakeFrameBudget.BudgetMs(),remakeFrameBudget.Credit(),remakeFrameBudget.FeedEstimateMs(),remakeFrameBudget.EvaluateEstimateMs(),
			(unsigned long long)remakeFrameBudget.FeedRuns(),(unsigned long long)remakeFrameBudget.FeedSkips(),(unsigned long long)remakeFrameBudget.EvaluateRuns(),(unsigned long long)remakeFrameBudget.EvaluateDeferrals());
	drainRemakeReturns(metadata.frameId,producer);
	const auto received=remakeReturnWorker.ChannelClosed()?RemakeChannelResult::Closed:RemakeChannelResult::Empty;
	if(received==RemakeChannelResult::Closed) {
		remakeAsyncChannel.Close();remakeAsyncTextures.Reset();resetRemakeAsyncFrames();remakeAsyncStopped=true;
		if(managed&&std::strcmp(managed,"1")==0)remakeSessionRenewalRequested=true;
		return;
	}
	remakeAsyncChannel.ExpireReturns(metadata.frameId,producer,8);
	for(auto& source:remakeAsyncOverlaySources)
		if(source.identity.frame&&(source.identity.frame>metadata.frameId||metadata.frameId-source.identity.frame>8))source={};
	if(remakeAsyncReturned&&(remakeAsyncReturned->frame>metadata.frameId||metadata.frameId-remakeAsyncReturned->frame>8)) {
		remakeAsyncReturned.reset();remakeAsyncAcceptedOverlay={};
	}
	if(!remakeAsyncChannel.HasReturnCredit()){skip("credit","no-return-credit");return;}
	// D-216 render-thread budget: an explicit skip, never slower emulation.
	if(!remakeFrameBudget.AllowFeed()){remakeFrameBudget.CountFeedSkip();skip("feed","frame-budget");return;}
	struct FeedCost {
		RemakeFrameBudget& budget;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
		~FeedCost(){budget.RecordFeed(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
	} feedCost{remakeFrameBudget};
	remakeAsyncTextures.BeginFrame(metadata.frameId,producer.epoch);
	std::array<float,16> viewport{};const auto& matrix=matrices.GetNormalMatrix();
	for(int c=0;c<4;++c)for(int r=0;r<4;++r)viewport[c*4+r]=matrix[c][r];
	PvrDecodedPacket snapshot;RemakeViewScene scene;
	const auto* estimate=std::getenv("FLYCAST_REMAKE_ESTIMATE_UNTRACED");
	bool snapshotReady;
	{
		static thread_local unsigned count=0;
		RemakeCpuScope timing("source-snapshot",metadata.frameId,count);
		snapshotReady=SnapshotPvrScenePacket(*rendContext,viewport,metadata.frameId,metadata.gameId,snapshot,error);
	}
	if(!snapshotReady) {skip("snapshot",error);return;}
	snapshot.sourceAlphaReference=remakeSourceAlphaReference;
	// Match the concatenated OP/PT/TR ordinal used by native overlay-mask replay.
	// Exclude only when that same policy will restore the owned native overlay.
	unsigned protectedDraws=0;
	if(config::NeuralOverlayPolicy.get()==0)
		for(std::size_t ordinal=0;ordinal<snapshot.draws.size();++ordinal) {
			snapshot.draws[ordinal].protectedOverlay=neuralInstrumentation.IsOverlayOrdinal(ordinal);
			protectedDraws+=snapshot.draws[ordinal].protectedOverlay;
		}
	if(protectedDraws)NOTICE_LOG(RENDERER,"Remake world overlay exclusion: source=%llu protected_draws=%u policy=native-post-composite",
		(unsigned long long)metadata.frameId,protectedDraws);
	const auto* cutout=std::getenv("FLYCAST_REMAKE_PUNCH_THROUGH");
	const auto* alphaOption=std::getenv("FLYCAST_REMAKE_ALPHA_PREVIEW");
	const bool alphaPreview=alphaOption&&std::strcmp(alphaOption,"1")==0;
	const auto* alphaCombinedOption=std::getenv("FLYCAST_REMAKE_ALPHA_COMBINED");
	const bool alphaCombined=alphaCombinedOption&&std::strcmp(alphaCombinedOption,"1")==0;
	const auto* neuralOption=std::getenv("FLYCAST_REMAKE_ASYNC_NEURAL");
	if(alphaCombined&&(alphaPreview||!RemakeNativeEffectsRequested()||!neuralOption||std::strcmp(neuralOption,"1")!=0)) {
		skip("alpha-combined","requires-owned-effects-and-evaluation");return;
	}
	if(alphaPreview&&(RemakeNativeEffectsRequested()||(neuralOption&&std::strcmp(neuralOption,"1")==0))) {
		skip("alpha-preview","combined-ownership-not-implemented");return;
	}
	bool sceneReady;
	{
		static thread_local unsigned count=0;
		RemakeCpuScope timing("view-scene",metadata.frameId,count);
		sceneReady=BuildRemakeViewScene(snapshot,producer,metadata.frameId,scene,error,estimate&&std::strcmp(estimate,"1")==0,
			cutout&&std::strcmp(cutout,"1")==0,alphaPreview||alphaCombined);
	}
	if(!sceneReady) {skip("scene",error);return;}
	if(alphaPreview) {
		unsigned count=0;for(const auto& mesh:scene.meshes)count+=mesh.sourceAlphaBlend;
		NOTICE_LOG(RENDERER,"Remake alpha material preview: source=%llu meshes=%u combined=false native-effects=false",
			(unsigned long long)metadata.frameId,count);
	}
	if(cutout&&std::strcmp(cutout,"1")==0) {
		std::size_t count=0;for(const auto& mesh:scene.meshes)count+=mesh.sourceAlphaReference.has_value();
		NOTICE_LOG(RENDERER,"Remake cutout scene: source=%llu cutout_meshes=%u alpha_reference=%d scope=experimental",
			(unsigned long long)metadata.frameId,unsigned(count),snapshot.sourceAlphaReference?int(*snapshot.sourceAlphaReference):-1);
	}
	// Visit every required draw, even when an earlier texture is pending. Publish
	// no partial scene; a later frame uses its own geometry and current generations.
	// D-212: textures the consumer already holds this session travel by
	// reference. Disabled whenever a capture or locked archive is involved so
	// saved packets and their digests keep the full-texture lineage.
	const bool byReference=!(std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE")&&*std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE"))
		&&!(std::getenv("FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT")&&*std::getenv("FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT"));
	const auto alreadySent=[this](const remake::TextureIdentity& identity) {
		return remakeSentTextures&&remakeSentTextures->count({identity.id,identity.generation,identity.paletteGeneration,identity.rttGeneration})!=0;
	};
	const bool registerMore=(!remakeSentTextures||remakeSentTextures->size()<remake::Limits{}.textureReferences)
		&&remakeSentTextureBytes<remake::Limits{}.textureReferenceBytes;
	const RemakeTextureSent sent=byReference?RemakeTextureSent([&](const remake::TextureIdentity& identity){return alreadySent(identity);}):RemakeTextureSent{};
	// D-213: the render thread stages the texture bytes every unsent draw needs;
	// the packet itself is built on the feed worker from those bytes.
	bool ready=true;std::size_t remaining=64*1024*1024;
	std::map<std::pair<std::uint32_t,std::uint32_t>,std::vector<unsigned char>> staged;
	static thread_local unsigned stageTimingCount=0;
	RemakeCpuScope stageTiming("feed-texture-stage",metadata.frameId,stageTimingCount);
	for(const auto& mesh:scene.meshes) {
		std::vector<unsigned char> bytes;
		if(sent&&mesh.sourceDraw.texture&&alreadySent({mesh.sourceDraw.state.tcw.full,mesh.sourceDraw.texture->upload,
			mesh.sourceDraw.texture->palette.value_or(0),mesh.sourceDraw.texture->rtt,true}))continue;
		if(!ReadRemakeViewTexture(device,deviceContext,*rendContext,mesh.sourceDraw,remaining,bytes,error,&remakeAsyncTextures,paletteTexture,remakePaletteUpload.get())) {
			if(ready)skip("texture",error);ready=false;
		}else staged.emplace(std::make_pair(mesh.sourceDraw.list,mesh.sourceDraw.ordinal),std::move(bytes));
	}
	if(!ready)return;
	stageTiming.End();
	remake::Packet packet;
	const auto* anchorOption=std::getenv("FLYCAST_REMAKE_CAMERA_ANCHOR");
	const bool anchored=anchorOption&&std::strcmp(anchorOption,"1")==0;
	// The camera anchor is applied by the feed worker (D-211), off this thread.
	bool temporalRequested=false;
	const auto* temporalOption=std::getenv("FLYCAST_REMAKE_TEMPORAL_PREPARE");
	if(temporalOption&&std::strcmp(temporalOption,"1")==0) {
		const auto* locked=std::getenv("FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT");
		const auto* compareStart=std::getenv("FLYCAST_REMAKE_COMPARE_START_FRAME");
		const auto* capture=std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE");
		const bool boundedReplay=compareStart&&capture&&*capture&&RemakeEffectEvidenceRequested()
			&&RemakeMovingCaptureEnabled(std::getenv("FLYCAST_REMAKE_MOVING_CAPTURE"))
			&&RemakePreviewCaptureLimit(std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES"),"1")>0
			&&RemakeComparisonEligible(compareStart,10000000,true);
		if(!anchored||!neuralOption||std::strcmp(neuralOption,"1")!=0||((locked&&*locked)&&!boundedReplay)) {
			skip("temporal-source","requires-anchored-live-neural-source");return;
		}
		temporalRequested=true; // Captured by the feed worker from the anchored packet.
	}
	if(currentNeuralSourceFrameId!=metadata.frameId||currentNeuralGuidanceFrameId!=metadata.frameId){skip("guidance","frame-mismatch");return;}
	RemakeOverlaySnapshot overlay;
	if(RemakeNativeEffectsRequested()) {
		if(!remakeCurrentEffects||!remakeCurrentEffects->Matches(producer)) {
			skip("native-effects",remakeCurrentEffectsReason);return;
		}
	}
	bool copied;
	{
		static thread_local unsigned count=0;
		RemakeCpuScope timing("feed-overlay-copy",metadata.frameId,count);
		acquireNeuralInputs();
		copied=CaptureRemakeOverlay(device,deviceContext,fbTex,neuralOverlayMask.textures[neuralExportSlot],metadata.frameId,producer,overlay);
		releaseNeuralInputs();
	}
	if(!copied){skip("overlay","copy-failed");return;}
	if(RemakeNativeEffectsRequested())overlay.effects=remakeCurrentEffects;
	std::map<std::uint32_t,EffectIdentityPoly> alphaParams;
	if(alphaCombined) {
		// Bindings and list ranges are verified here against the live source;
		// the selection itself follows the clipped packet on the feed worker.
		if(!PvrSnapshotTextureBindingsMatch(*rendContext,snapshot)){skip("alpha-ownership","source-bindings-changed");return;}
		for(const auto& mesh:scene.meshes)if(mesh.sourceAlphaBlend) {
			const auto& draw=mesh.sourceDraw;
			if(draw.list!=2||draw.ordinal>=rendContext->global_param_tr.size()){skip("alpha-ownership","source-list-range");return;}
			const auto& pp=rendContext->global_param_tr[draw.ordinal];
			alphaParams[draw.ordinal]={(pp.tsp.full&0xffff00c0)|((pp.isp.full>>16)&0xe400)|((pp.pcw.full>>7)&1),pp.tsp1.full};
		}
	}
	// D-211: anchor, temporal capture, serialization and digest run on the feed
	// worker. A busy worker is an explicit native fallback for this source.
	remakeFeedWorker.Start();
	RemakeFeedJob job;job.frame=metadata.frameId;job.producer=producer;
	job.snapshot=std::move(snapshot);job.scene=std::move(scene);job.packet=std::move(packet);job.overlay=std::move(overlay);
	job.anchored=anchored;job.temporal=temporalRequested;job.managed=managed&&std::strcmp(managed,"1")==0;
	job.buildPacket=true;job.registerMore=registerMore;job.textures=std::move(staged);job.byReference=bool(sent);job.sent=remakeSentTextures;
	job.alphaOwnership=alphaCombined;job.alphaParams=std::move(alphaParams);
	if(const auto* capture=std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE");capture&&*capture)job.captureScene=true;
	job.publish=[this](const remake::Packet& source,RemakeChannelReceipt& receipt,std::string& why) {
		return remakeAsyncChannel.PublishForReturn(source,receipt,why);
	};
	if(!remakeFeedWorker.Dispatch(std::move(job)))skip("feed","worker-busy-native-fallback");
	} catch(const std::exception& error) {
		remakeAsyncChannel.Close();remakeAsyncTextures.Reset();resetRemakeAsyncFrames();remakeAsyncStopped=true;
		WARN_LOG(RENDERER,"Remake async feed stopped: %s; existing presentation retained",error.what());
	}
}

void DX11Renderer::prepareRemakeCapture()
{
	const auto* remakeToken=std::getenv("FLYCAST_REMAKE_CHANNEL");
	const auto* lockedInput=std::getenv("FLYCAST_REMAKE_LOCKED_INPUT_ROOT");
	const auto* inputTest=std::getenv("FLYCAST_REMAKE_INPUT_TEST");
	const bool requested=inputTest&&std::strcmp(inputTest,"1")==0;
	if(((remakeToken&&*remakeToken)||(lockedInput&&*lockedInput&&requested)) && (activeNeuralMode==1 || requested)
		&& !IsOitRenderer() && rendContext && !rendContext->isRTT && !config::EmulateFramebuffer.get()
		&& config::NeuralCapturePvrPacket.get()
		&& neuralQualityCapture.CapturesCurrentFrame()) {
		std::array<float,16> viewport{};const auto& matrix=matrices.GetNormalMatrix();
		for(int column=0;column<4;++column)for(int row=0;row<4;++row)viewport[column*4+row]=matrix[column][row];
		flycast::rend::neural::PvrDecodedPacket snapshot;std::string error;
		if(flycast::rend::neural::SnapshotPvrScenePacket(*rendContext,viewport,
			neuralQualityCaptureMetadata.frameId,neuralQualityCaptureMetadata.gameId,snapshot,error)) {
			const auto reader=[this,remaining=std::size_t(64*1024*1024)](const flycast::rend::neural::PvrCapturedDraw& draw,
				std::vector<unsigned char>& bytes,std::string& reason) mutable {
				return flycast::rend::neural::ReadRemakeViewTexture(device,deviceContext,*rendContext,draw,remaining,bytes,reason);
			};
			neuralQualityCapture.PrepareRemakeBeforeComposite(snapshot,neuralQualityCaptureMetadata,reader,[this] {
				// Developer-only exchange may block for another GPU process. Submit
				// pending producer work before waiting; not a completion/Present proof.
				deviceContext->Flush();
			});
		}
	}
}

void DX11Renderer::evaluateRemakeAsync(flycast::rend::neural::NeuralFrame frame)
{
	using namespace flycast::rend::neural;
	// Native input must not enter this stage while it is reserved for returned
	// scenes. Display uses only an owned accepted snapshot and its original HUD.
	releaseNeuralPresentation();neuralPresentationView.reset();
	if(!remakeAsyncStopped&&rendContext)drainRemakeReturns(frame.frameId,rendContext->captureProducer);
	if(!activeNeuralSurface||!RemakeRendererAllowed(IsOitRenderer(),std::getenv("FLYCAST_REMAKE_ASYNC_OIT"))
		||!rendContext||rendContext->isRTT||config::EmulateFramebuffer.get()
		||config::NeuralCaptureFrames.get()!=0||remakeAsyncStopped||!remakeAsyncReturned
		||frame.renderWidth!=640||frame.renderHeight!=480||frame.outputWidth!=640||frame.outputHeight!=480
		||(activeNeuralMode!=static_cast<int>(NeuralMode::Dlaa)
			&&activeNeuralMode!=static_cast<int>(NeuralMode::Dlss5Experimental)))return;
	const auto& returned=*remakeAsyncReturned;
	if(returned.frame<=remakeLastEvaluationAttempt)return; // Already attempted; nothing to budget.
	// D-216: defer the evaluation to a later frame when the budget cannot
	// cover its learned cost; the image stays pending until it ages out.
	if(!remakeFrameBudget.AllowEvaluate()){remakeFrameBudget.CountEvaluateDeferral();return;}
	struct EvaluateCost {
		RemakeFrameBudget& budget;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
		~EvaluateCost(){budget.RecordEvaluate(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
	} evaluateCost{remakeFrameBudget};
	static thread_local unsigned evaluateTimingCount=0;
	RemakeCpuScope evaluateTiming("returned-evaluate",frame.frameId,evaluateTimingCount);
	const auto* comparisonCapture=std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE");
	const bool boundedComparison=comparisonCapture&&*comparisonCapture
		&&RemakeMovingCaptureEnabled(std::getenv("FLYCAST_REMAKE_MOVING_CAPTURE"))
		&&RemakePreviewCaptureLimit(std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES"),"1")>0;
	if(!RemakeComparisonEligible(std::getenv("FLYCAST_REMAKE_COMPARE_START_FRAME"),returned.frame,boundedComparison))return;
	if(!RemakeComparisonBeforeEnd(std::getenv("FLYCAST_REMAKE_COMPARE_END_FRAME"),returned.frame,boundedComparison))return;
	if(RemakeNativeEffectsRequested()) {
		const auto* lockedRoot=std::getenv("FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT");
		const char* rejected=!remakeAsyncAcceptedOverlay.effects?"missing-effects"
			:!remakeAsyncAcceptedOverlay.effects->Matches(returned.producer)?"effect-source-mismatch"
			:lockedRoot&&*lockedRoot&&!RemakeEffectEvidenceRequested()?"locked-effects-replay-requires-identity":nullptr;
		if(rejected) {
			NOTICE_LOG(RENDERER,"Remake effects evaluation rejected: source=%llu reason=%s",(unsigned long long)returned.frame,rejected);
			return;
		}
	}
	if(returned.frame<=remakeLastEvaluationAttempt||!remakeAsyncAcceptedOverlay.identity.Matches(returned,frame.frameId,rendContext->captureProducer))return;
	remakeLastEvaluationAttempt=returned.frame;
	try {
		std::optional<RemakeReturnedImage> replay;std::uint64_t replayOriginalFrame=0;
		if(const auto* root=std::getenv("FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT");root&&*root) {
			std::string error;RemakeReturnedImage retained;std::filesystem::path matchedDirectory;
			if(!remakeAsyncAcceptedOverlay.captureScene||!ReadLockedRemakeInput(root,*remakeAsyncAcceptedOverlay.captureScene,retained,replayOriginalFrame,error,&matchedDirectory)) {
				NOTICE_LOG(RENDERER,"Remake async locked input rejected: source=%llu reason=%s",(unsigned long long)returned.frame,error.c_str());return;
			}
			if(RemakeNativeEffectsRequested()) {
				if(remakeEffectReplayAttempts>=RemakeEffectEvidenceLimit()) {
					NOTICE_LOG(RENDERER,"Remake effect replay rejected: evidence-attempt-bound");return;
				}
				++remakeEffectReplayAttempts;
				std::vector<std::uint32_t> identity;
				std::ifstream evidence(matchedDirectory/"native-effect-identity.bin",std::ios::binary);
				if(!remakeAsyncAcceptedOverlay.effects->ReadIdentityForEvidence(device,deviceContext,returned.producer,identity,error)
					||!MatchEffectIdentity(evidence,identity,error)) {
					NOTICE_LOG(RENDERER,"Remake effect replay rejected: source=%llu reason=%s",(unsigned long long)returned.frame,error.c_str());return;
				}
				NOTICE_LOG(RENDERER,"Remake effect replay matched: source=%llu original=%llu synchronous=true performance_eligible=false",
					(unsigned long long)returned.frame,(unsigned long long)replayOriginalFrame);
				if(!remakeAsyncAcceptedOverlay.alphaEffectSelections.empty()||std::filesystem::exists(matchedDirectory/"native-alpha-exclusions.bin")) {
					std::ifstream selected(matchedDirectory/"native-alpha-exclusions.bin",std::ios::binary);
					if(!MatchEffectIdentity(selected,AlphaEffectSelectionIdentity(remakeAsyncAcceptedOverlay.alphaEffectSelections),error)) {
						NOTICE_LOG(RENDERER,"Remake alpha selection replay rejected: %s",error.c_str());return;
					}
				}
			}
			// Temporal replay must keep exact original source-frame identity. The
			// older image-only experiment permits remapping, but that must never
			// manufacture geometry history for a differently numbered source.
			if(!RemakeTemporalReplayFrameMatches(bool(remakeAsyncAcceptedOverlay.temporalScene),replayOriginalFrame,returned.frame)) {
				NOTICE_LOG(RENDERER,"Remake temporal replay rejected: original source frame differs");return;
			}
			// Scene/producer/input hashes were checked above. Current receipt owns
			// the matching original HUD; pixels are explicitly labeled retained replay.
			retained.source=returned.source;replay=std::move(retained);
			NOTICE_LOG(RENDERER,"Remake async locked input accepted: source=%llu original=%llu input_origin=locked-replay-not-live",
				(unsigned long long)returned.frame,(unsigned long long)replayOriginalFrame);
		}
		const auto& source=replay?*replay:returned;
		const auto temporal=remakeAsyncAcceptedOverlay.temporalScene;
		const auto* rasterRequest=std::getenv("FLYCAST_REMAKE_TEMPORAL_RASTER");
		const bool rasterRequested=rasterRequest&&std::strcmp(rasterRequest,"1")==0;
		if(rasterRequested&&!temporal)return;
		RemakeMotionStream stream;
		RemakeRasterOutput rasterOutput;
		bool rasterHistory=false;
		if(temporal&&!temporal->Matches(source)) {
			NOTICE_LOG(RENDERER,"Remake temporal source rejected: receipt mismatch");return;
		}
		if(const auto* comparison=std::getenv("FLYCAST_REMAKE_COMPARE_REMIX_ONLY");comparison) {
			// Explicit diagnostic lane. Never submit or advance accepted neural
			// history; retain the same source effects and late overlay ownership.
			if(std::strcmp(comparison,"1")!=0||!boundedComparison||!RemakeNativeEffectsRequested())return;
			if(source.bgra.size()!=640*480*4)return;
			D3D11_TEXTURE2D_DESC desc{};desc.Width=640;desc.Height=480;
			desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA data{source.bgra.data(),640*4,0};
			ComPtr<ID3D11Texture2D> raw,composed;ComPtr<ID3D11ShaderResourceView> composedView;
			if(FAILED(device->CreateTexture2D(&desc,&data,&raw.get()))
				||!remakeAsyncAcceptedOverlay.effects->Compose(device,deviceContext,source.producer,raw,composed,composedView,
					remakeAsyncAcceptedOverlay.alphaEffectSelections))return;
			remakeEvaluatedSource=source;remakeEvaluatedOverlay=remakeAsyncAcceptedOverlay;
			remakeEvaluatedOverlay.replayOriginalFrame=replayOriginalFrame;
			remakeEvaluatedTexture=std::move(composed);remakeEvaluatedView=std::move(composedView);
			remakePreEffectTexture=std::move(raw);
			NOTICE_LOG(RENDERER,"Remake comparison output owned: source=%llu sequence=%llu lane=remix-only neural_submitted=false history_advanced=false",
				(unsigned long long)source.frame,(unsigned long long)source.source.sequence);
			return;
		}
		// D-213: use the worker-prepared stream/input only for this exact source
		// and only when the accepted history it was built against is unchanged.
		const bool prepared=!replay&&remakePreparedReturn&&remakePreparedReturn->frame==source.frame
			&&remakePreparedReturn->sequence==source.source.sequence;
		const auto* lastAccepted=remakeTemporalHistory.Last();
		const bool preparedStream=prepared&&remakePreparedReturn->temporal&&bool(temporal)
			&&remakePreparedReturn->previousFrame==(lastAccepted?lastAccepted->frame:0);
		if(temporal) {
			std::string error;
			const auto* previous=remakeTemporalHistory.CanReproject(*temporal)?remakeTemporalHistory.Last():nullptr;
			bool streamReady;
			if(preparedStream){stream=std::move(remakePreparedReturn->stream);streamReady=remakePreparedReturn->streamReady;error=remakePreparedReturn->streamError;}
			else {
				static thread_local unsigned count=0;
				RemakeCpuScope timing("evaluate-motion-stream",frame.frameId,count);
				streamReady=BuildRemakeMotionStream(previous,*temporal,stream,error);
			}
			NOTICE_LOG(RENDERER,"Remake return preparation: source=%llu stream=%s input=%s",(unsigned long long)source.frame,
				preparedStream?"worker":prepared?"rebuilt-history-advanced":"render-thread",prepared&&remakePreparedReturn->inputReady?"worker":"render-thread");
			if(!streamReady) {
				NOTICE_LOG(RENDERER,"Remake geometry motion rejected: %s",error.c_str());return;
			}
			NOTICE_LOG(RENDERER,"Remake geometry motion: source=%llu trusted_draws=%u reactive_draws=%u ambiguous_draws=%u trusted_vertices=%u max_pixels=%.9g gpu_guidance=false",
				(unsigned long long)source.frame,stream.trustedDraws,stream.reactiveDraws,stream.ambiguousDraws,stream.trustedVertices,stream.maximumMotion);
		}
		RemakeNeuralInput input;
		{
			static thread_local unsigned count=0;
			RemakeCpuScope timing("evaluate-input-upload",frame.frameId,count);
			if(prepared&&remakePreparedReturn->inputReady)input=std::move(remakePreparedReturn->input);
			else if(!BuildRemakeNeuralInput(source,source.frame,source.producer,input))return;
			if(prepared)remakePreparedReturn.reset();
			if(!uploadRemakeInput(input))return;
		}
		if(rasterRequested) {
			std::string error;
			const auto* previous=remakeTemporalHistory.Last();
			rasterHistory=previous&&remakeTemporalHistory.CanReproject(*temporal)
				&&remakeAcceptedRasterFrame==previous->frame&&remakeAcceptedRaster.views[2];
			const auto& previousDepth=rasterHistory?remakeTemporalHistory.Depth():source.projectionDepth;
			const char* colorSetting=std::getenv("FLYCAST_REMAKE_COLOR_CONSISTENCY");
			const bool colorCheck=colorSetting&&std::strcmp(colorSetting,"1")==0;
			const auto& previousColor=rasterHistory?remakeTemporalHistory.Color():source.bgra;
			bool rasterReady;
			{
				static thread_local unsigned count=0;
				RemakeCpuScope timing("evaluate-raster",frame.frameId,count);
				rasterReady=remakeMotionRaster.Initialize(device,DX11Context::Instance()->getCompiler(),error)
					&&remakeMotionRaster.Render(deviceContext,stream,source.projectionDepth,previousDepth,
						rasterHistory?remakeAcceptedRaster.views[2].Get():nullptr,source.nearPlane,source.farPlane,
						.001f,.0001f,rasterOutput,error,colorCheck?&source.bgra:nullptr,colorCheck?&previousColor:nullptr);
			}
			if(!rasterReady) {
				NOTICE_LOG(RENDERER,"Remake GPU guidance rejected: source=%llu reason=%s",
					(unsigned long long)source.frame,error.c_str());return;
			}
			acquireNeuralInputs();
			deviceContext->CopyResource(neuralMotion.textures[neuralExportSlot],rasterOutput.textures[0].Get());
			deviceContext->CopyResource(neuralConfidence.textures[neuralExportSlot],rasterOutput.textures[1].Get());
			deviceContext->CopyResource(neuralDrawId.textures[neuralExportSlot],rasterOutput.textures[2].Get());
			deviceContext->CopyResource(neuralResolvedMask.textures[neuralExportSlot],rasterOutput.textures[3].Get());
			releaseNeuralInputs();
			NOTICE_LOG(RENDERER,"Remake GPU guidance: source=%llu previous=%llu history=%d color_consistency=%d color_threshold_sdr=8/255 scope=projected-depth-experiment",
				(unsigned long long)source.frame,(unsigned long long)(rasterHistory?previous->frame:0),rasterHistory,colorCheck);
		}
		frame.frameId=source.frame;frame.jitterX=frame.jitterY=0;
		frame.historyValid=false;frame.resetHistory=true;frame.historyAge=0;frame.skippedFrameCount=0;
		if(rasterHistory) {
			frame.historyValid=true;frame.resetHistory=false;frame.historyAge=1;
			frame.skippedFrameCount=unsigned(source.frame-remakeTemporalHistory.Last()->frame-1);
		}
		frame.draws={};frame.matches={};frame.correspondence={};frame.predominantly2D=false;
		neuralPerformance.Mark(deviceContext,GpuTimingPoint::EvaluateBegin);
		const auto status=[&] {
			static thread_local unsigned count=0;
			RemakeCpuScope timing("evaluate-submit",frame.frameId,count);
			return neuralStage.TrySubmit(frame);
		}();
		{
			static thread_local unsigned count=0;
			RemakeCpuScope timing("evaluate-status-marks",frame.frameId,count);
			logNeuralConsumerStatus(status);
			neuralPerformance.Mark(deviceContext,GpuTimingPoint::EvaluateEnd);
			neuralPerformance.RecordEvaluation(source.frame,status==SubmitStatus::Submitted,frame.resetHistory);
		}
		NOTICE_LOG(RENDERER,"Remake async neural evaluation: source=%llu current=%llu sequence=%llu accepted=%d reset=%d motion=%s bias=%s displayed=false",
			(unsigned long long)source.frame,(unsigned long long)currentNeuralSourceFrameId,(unsigned long long)source.source.sequence,status==SubmitStatus::Submitted,
			frame.resetHistory,rasterRequested?"returned-geometry":"zero",rasterRequested?"returned-depth-consistency":"one");
		if(status!=SubmitStatus::Submitted)return;
		// Returned-scene evaluation is never accepted native PVR correspondence.
		hasNeuralAcceptedGuidance=false;neuralInstrumentation.Discontinuity();
		if(temporal) {
			const auto* previous=remakeTemporalHistory.Last();const auto previousFrame=previous?previous->frame:0;
			const bool compatible=remakeTemporalHistory.CanReproject(*temporal);
			const char* colorSetting=std::getenv("FLYCAST_REMAKE_COLOR_CONSISTENCY");
			const bool retained=[&] {
				static thread_local unsigned count=0;
				RemakeCpuScope timing("evaluate-history-accept",frame.frameId,count);
				return remakeTemporalHistory.Accept(temporal,source,true,colorSetting&&std::strcmp(colorSetting,"1")==0);
			}();
			if(retained) {
				remakeAcceptedRaster=std::move(rasterOutput);
				remakeAcceptedRasterFrame=rasterRequested?source.frame:0;
			}
			NOTICE_LOG(RENDERER,"Remake temporal reference: source=%llu previous_evaluated=%llu compatible=%d retained=%d history_enabled=%d motion=%s",
				(unsigned long long)source.frame,(unsigned long long)previousFrame,compatible,retained,rasterHistory,rasterRequested?"returned-geometry":"zero");
		}
		// Submitted means this source was successfully evaluated by the public
		// backend. Retain that source history independently of external output
		// eligibility; hooks-disabled evaluation must not manufacture zero motion.
		// This does not authorize presenting or claiming an external neural result.
		if(activeNeuralMode==static_cast<int>(NeuralMode::Dlss5Experimental)
			&&neuralStage.GetStats().dlss5Readiness!=Dlss5HookReadiness::ContractEvaluated)return;
		static thread_local unsigned ownTimingCount=0,wrapTimingCount=0;
		RemakeCpuScope ownTiming("evaluate-output-own",frame.frameId,ownTimingCount);
		const auto output=neuralStage.GetOutput();
		bool wrapped;
		{
			RemakeCpuScope wrapTiming("evaluate-output-wrap",frame.frameId,wrapTimingCount);
			wrapped=output.api==TextureApi::D3D12&&output.resource&&wrapNeuralOutput(static_cast<ID3D12Resource*>(output.resource),source.frame);
		}
		if(!wrapped)return;
		// Owned copies come from a small ring (D-215): an evaluated output is read
		// by at most the next composite and display, and copies are ordered on the
		// immediate context, so the slot reused three evaluations later is free.
		D3D11_TEXTURE2D_DESC desc{};neuralOutputWrappedTextures[neuralPresentationSlot]->GetDesc(&desc);
		desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.CPUAccessFlags=0;desc.MiscFlags=0;
		auto& pooled=remakeOwnedOutputs[remakeOwnedOutputNext%remakeOwnedOutputs.size()];++remakeOwnedOutputNext;
		bool created=true;
		if(pooled.texture) {
			D3D11_TEXTURE2D_DESC have{};pooled.texture->GetDesc(&have);
			if(have.Width!=desc.Width||have.Height!=desc.Height||have.Format!=desc.Format){pooled.texture.reset();pooled.view.reset();}
		}
		if(!pooled.texture) {
			created=SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&pooled.texture.get()))
				&&SUCCEEDED(device->CreateShaderResourceView(pooled.texture,nullptr,&pooled.view.get()));
			if(!created){pooled.texture.reset();pooled.view.reset();}
		}
		ComPtr<ID3D11Texture2D> owned=pooled.texture;ComPtr<ID3D11ShaderResourceView> view=pooled.view;
		if(created)deviceContext->CopyResource(owned,neuralOutputWrappedTextures[neuralPresentationSlot]);
		releaseNeuralPresentation();neuralPresentationView.reset();
		if(!created)return;
		ComPtr<ID3D11Texture2D> preEffects;
		if(RemakeNativeEffectsRequested()) {
			if(const auto* capture=std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE");capture&&*capture)preEffects=owned;
			ComPtr<ID3D11Texture2D> composed;ComPtr<ID3D11ShaderResourceView> composedView;
			if(!remakeAsyncAcceptedOverlay.effects->Compose(device,deviceContext,source.producer,owned,composed,composedView,
				remakeAsyncAcceptedOverlay.alphaEffectSelections))return;
			owned=std::move(composed);view=std::move(composedView);
			NOTICE_LOG(RENDERER,"Remake source effects composited: source=%llu sequence=%llu scope=native-oit-single-pass-copy-experiment provenance=pending",
				(unsigned long long)source.frame,(unsigned long long)source.source.sequence);
		}
		remakeEvaluatedSource=source;remakeEvaluatedOverlay=remakeAsyncAcceptedOverlay;
		remakeEvaluatedOverlay.replayOriginalFrame=replayOriginalFrame;
		remakeEvaluatedTexture=std::move(owned);remakeEvaluatedView=std::move(view);
		remakePreEffectTexture=std::move(preEffects);
		NOTICE_LOG(RENDERER,"Remake evaluated output owned: source=%llu sequence=%llu external_mutation=unconfirmed",
			(unsigned long long)source.frame,(unsigned long long)source.source.sequence);
	}catch(const std::exception& e) {
		releaseNeuralPresentation();neuralPresentationView.reset();
		WARN_LOG(RENDERER,"Remake async neural snapshot failed: %s; native fallback",e.what());
	}
}

bool DX11Renderer::applyRemakeCaptureInput(flycast::rend::neural::NeuralFrame& frame)
{
	using namespace flycast::rend::neural;
	const auto* request=std::getenv("FLYCAST_REMAKE_INPUT_TEST");
	if(!request||std::strcmp(request,"1")!=0||!neuralQualityCapture.CapturesCurrentFrame())return false;
	neuralQualityCaptureMetadata.remakeInput="requested-native-fallback";
	if(IsOitRenderer()||!rendContext||rendContext->isRTT||config::EmulateFramebuffer.get()
		||frame.renderWidth!=640||frame.renderHeight!=480||frame.outputWidth!=640||frame.outputHeight!=480
		||frame.jitterX!=0||frame.jitterY!=0)return false;
	const auto* returned=neuralQualityCapture.ReturnedRemakeFrame(frame.frameId);
	RemakeNeuralInput input;
	if(!returned||!BuildRemakeNeuralInput(*returned,frame.frameId,rendContext->captureProducer,input)) {
		WARN_LOG(RENDERER,"Remake input rejected: frame=%llu status=%s",(unsigned long long)frame.frameId,neuralQualityCapture.RemakePacketStatus().c_str());
		return false;
	}
	if(!uploadRemakeInput(input))return false;
	frame.historyValid=false;frame.resetHistory=true;frame.historyAge=0;
	frame.correspondence={};
	neuralQualityCaptureMetadata.historyValid=false;neuralQualityCaptureMetadata.resetHistory=true;
	neuralQualityCaptureMetadata.historyAge=0;
	neuralQualityCaptureMetadata.correspondence={};
	neuralQualityCaptureMetadata.profile += " / Remix reset-only diagnostic";
	neuralQualityCaptureMetadata.remakeInput=neuralQualityCapture.RemakeInputReplayed()
		?"locked-replay-reset-only-inverted-projection-experiment":"returned-scene-reset-only-inverted-projection-experiment";
	NOTICE_LOG(RENDERER,"Remake neural input: frame=%llu source_sequence=%llu reset=1 motion=zero bias=one depth=inverted-projection",
		(unsigned long long)frame.frameId,(unsigned long long)returned->source.sequence);
	return true;
}

bool DX11Renderer::uploadRemakeInput(const flycast::rend::neural::RemakeNeuralInput& input)
{
	if(input.rgba.size()!=640*480*4||input.invertedDepth.size()!=640*480)return false;
	D3D11_TEXTURE2D_DESC desc{};desc.Width=640;desc.Height=480;desc.MipLevels=1;desc.ArraySize=1;
	desc.Format=DXGI_FORMAT_R32_FLOAT;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA data{};data.pSysMem=input.invertedDepth.data();data.SysMemPitch=640*sizeof(float);
	ComPtr<ID3D11Texture2D> upload;
	if(FAILED(device->CreateTexture2D(&desc,&data,&upload.get())))return false;
	acquireNeuralInputs();
	deviceContext->UpdateSubresource(neuralColor.textures[neuralExportSlot],0,nullptr,input.rgba.data(),640*4,0);
	deviceContext->CopyResource(neuralDepthTextures[neuralExportSlot],upload);
	const float zero[4]{};const float one[4]={1,1,1,1};
	deviceContext->ClearRenderTargetView(neuralMotion.targets[neuralExportSlot],zero);
	deviceContext->ClearRenderTargetView(neuralConfidence.targets[neuralExportSlot],zero);
	deviceContext->ClearRenderTargetView(neuralDrawId.targets[neuralExportSlot],zero);
	deviceContext->ClearRenderTargetView(neuralResolvedMask.targets[neuralExportSlot],one);
	releaseNeuralInputs();
	return true;
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
	bool neuralDebugActive = false;
#ifdef FLYCAST_ENABLE_NEURAL
	const int neuralDebugSelection = std::clamp(config::NeuralDebugView.get(), 0, 7);
	ID3D11ShaderResourceView *neuralDebugTexture = nullptr;
	if (activeNeuralMode > 0 && currentNeuralGuidanceFrameId != 0
		&& currentNeuralGuidanceFrameId == currentNeuralSourceFrameId
		&& neuralDepthWidth != 0 && neuralDepthHeight != 0)
	{
		switch (neuralDebugSelection)
		{
		case 1: neuralDebugTexture = neuralColor.views[neuralExportSlot]; break;
		case 2: neuralDebugTexture = neuralDepthViews[neuralExportSlot]; break;
		case 3: neuralDebugTexture = neuralMotion.views[neuralExportSlot]; break;
		case 4: neuralDebugTexture = neuralResolvedMask.views[neuralExportSlot]; break;
		case 5: neuralDebugTexture = neuralConfidence.views[neuralExportSlot]; break;
		case 6: neuralDebugTexture = neuralDrawId.views[neuralExportSlot]; break;
		case 7: neuralDebugTexture = neuralOverlayMask.views[neuralExportSlot]; break;
		default: break;
		}
	}
	const ComPtr<ID3D11PixelShader> *neuralDebugShader = nullptr;
	if (neuralDebugTexture)
	{
		const auto& shader = shaders->getNeuralDebugPixelShader(neuralDebugSelection);
		if (shader)
			neuralDebugShader = &shader;
	}
	neuralDebugActive = neuralDebugShader != nullptr;
	if (neuralDebugSelection != loggedNeuralDebugView
		|| neuralDebugActive != loggedNeuralDebugActive)
	{
		static const char *names[] = {"off", "source-color", "depth", "motion",
			"bias-mask", "confidence", "draw-id", "overlay-classification"};
		loggedNeuralDebugView = neuralDebugSelection;
		loggedNeuralDebugActive = neuralDebugActive;
		NOTICE_LOG(RENDERER,
			"Neural developer debug view: view=%s active=%d history_effect=none presentation_accounting=%s",
			names[neuralDebugSelection], neuralDebugActive ? 1 : 0,
			neuralDebugActive ? "native" : "normal");
	}
#endif
#ifdef FLYCAST_ENABLE_NEURAL
	const auto remakeDecision=selectRemakePreview(!neuralDebugActive);
	const bool remakePreviewDraw=remakeDecision.kind!=flycast::rend::neural::RemakeDisplayKind::Fallback&&remakeDisplayedView;
	const bool queuedPublicOutput=neuralPresentationAcquired&&!neuralDebugActive&&!remakePreviewDraw;
	const auto queuedNeuralFrameId = pendingNeuralPresentationFrameId;
	const auto displayedNeuralFrameId = neuralPresentationView && !neuralDebugActive&&!remakePreviewDraw
		? pendingNeuralPresentationFrameId : 0;
#endif
#ifdef FLYCAST_ENABLE_NEURAL
	if(remakePreviewDraw)
		quad->draw(remakeDisplayedView,samplers->getSampler(config::LinearInterpolation),nullptr,x,y,w,h,config::Rotate90);
	else if (neuralDebugActive)
	{
		acquireNeuralInputs();
		quad->drawCustom(*neuralDebugShader, &neuralDebugTexture, 1, {},
			x, y, w, h, config::Rotate90);
		releaseNeuralInputs();
	}
	else
#endif
		quad->draw(presentationView, samplers->getSampler(config::LinearInterpolation), nullptr,
			x, y, w, h, config::Rotate90);
#ifdef FLYCAST_ENABLE_NEURAL
	if (!remakePreviewDraw&&!neuralDebugActive && neuralPresentationView && config::NeuralOverlayPolicy.get() != 2
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
	const auto& previewSource=remakeDisplayedEvaluated?remakeEvaluatedSource:remakeAsyncReturned;
	const auto& previewOverlay=remakeDisplayedEvaluated?remakeEvaluatedOverlay:remakeAsyncAcceptedOverlay;
	bool previewStartAllowed=true;
	if(const auto* start=std::getenv("FLYCAST_REMAKE_PREVIEW_START_SOURCE");start&&*start) {
		char* end=nullptr;const auto frame=std::strtoul(start,&end,10);
		previewStartAllowed=!*end&&frame<=10000000&&remakeDecision.frame>=frame;
	}
	if(previewStartAllowed&&remakePreviewDraw&&remakeDecision.kind==flycast::rend::neural::RemakeDisplayKind::Remake
		&&previewSource&&remakeDecision.frame==previewSource->frame
		&&(!flycast::rend::neural::RemakeEffectEvidenceRequested()||remakePreviewCaptureAttempts<flycast::rend::neural::RemakeEffectEvidenceLimit())
		&&remakeDecision.frame!=remakePreviewLastCaptured&&remakePreviewCaptureAttempts<
			flycast::rend::neural::RemakePreviewCaptureLimit(std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES"),
				std::getenv("FLYCAST_REMAKE_MOVING_CAPTURE"))) {
		if(const auto* directory=std::getenv("FLYCAST_REMAKE_PREVIEW_CAPTURE");directory&&*directory) {
			++remakePreviewCaptureAttempts;remakePreviewLastCaptured=remakeDecision.frame;
			// Current-frame classification diagnostics are deliberately labeled
			// separately from the retained displayed source and only emitted in a
			// bounded explicit capture. They do not establish source HUD ownership.
			// Moving captures retain pixel/identity evidence, not hundreds of MB
			// of per-draw debugging text. Short diagnostic captures keep it.
			if(!flycast::rend::neural::RemakeMovingCaptureEnabled(std::getenv("FLYCAST_REMAKE_MOVING_CAPTURE")))
			for(const auto& item:neuralInstrumentation.CaptureOverlayDiagnostics()) {
				const auto& d=item.draw;
				if(d.bboxMin[1]>=0&&d.bboxMax[1]<=96)
					NOTICE_LOG(RENDERER,"Remake HUD draw diagnostic: current=%llu ordinal=%u list=%u texture=%u blend=%u flags=%u quads=%u bounds=%d,%d,%d,%d depth=%g,%g classified=%d stability=%u",
						(unsigned long long)currentNeuralSourceFrameId,d.ordinal,d.list,d.texId,d.blend,d.flags,d.screenAlignedPrimitiveCount,
						d.bboxMin[0],d.bboxMin[1],d.bboxMax[0],d.bboxMax[1],d.zMin,d.zMax,item.classified,item.stableAcceptedFrames);
			}
			ComPtr<ID3D11Resource> resource;ComPtr<ID3D11Texture2D> backbuffer;
			DX11Context::Instance()->getRenderTarget()->GetResource(&resource.get());
			if(resource)resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&backbuffer.get());
			std::string error;
			const bool captured=flycast::rend::neural::CaptureRemakePreview(directory,device,deviceContext,*previewSource,
				currentNeuralSourceFrameId,previewOverlay.color,previewOverlay.mask,
				remakeCompositeTexture,backbuffer,error,remakeDisplayedEvaluated?remakeEvaluatedTexture.get():nullptr,
				previewOverlay.captureScene.get(),previewOverlay.replayOriginalFrame,
				remakeDisplayedEvaluated?remakePreEffectTexture.get():nullptr,previewOverlay.effects.get(),previewOverlay.alphaEffectSelections,remakeAsyncToken);
			NOTICE_LOG(RENDERER,"Remake preview pixel capture: source=%llu current=%llu success=%d synchronous=true performance_eligible=false error=%s",
				(unsigned long long)remakeDecision.frame,(unsigned long long)currentNeuralSourceFrameId,captured,error.c_str());
			if(captured&&remakeDisplayedEvaluated&&remakeAcceptedRasterFrame) {
				std::array<ID3D11Texture2D*,6> guidance{};
				for(unsigned i=0;i<6;++i)guidance[i]=remakeAcceptedRaster.textures[i].Get();
				const bool recorded=flycast::rend::neural::CaptureRemakeGuidance(directory,device,deviceContext,*previewSource,
					currentNeuralSourceFrameId,remakeAcceptedRasterFrame,guidance,error);
				NOTICE_LOG(RENDERER,"Remake guidance capture: source=%llu success=%d synchronous=true error=%s",
					(unsigned long long)previewSource->frame,recorded,error.c_str());
			}
		}
	}
	neuralPerformance.Mark(deviceContext,
		flycast::rend::neural::GpuTimingPoint::CompositeEnd);
	neuralQualityCaptureGpuTimer.Mark(deviceContext,
		flycast::rend::neural::CaptureGpuTimingPoint::CompositeEnd);
	neuralPerformance.StagePresentation(currentNeuralSourceFrameId,
		remakePreviewDraw?remakeDecision.frame:displayedNeuralFrameId,
		!remakePreviewDraw?flycast::rend::neural::PresentationKind::Automatic:
		remakeDecision.kind==flycast::rend::neural::RemakeDisplayKind::Remake?
		(remakeDisplayedEvaluated?flycast::rend::neural::PresentationKind::RemakeEvaluated:flycast::rend::neural::PresentationKind::Remake):flycast::rend::neural::PresentationKind::HeldNative);
	DX11Context::Instance()->QueueRemakePreviewPresent(remakePreviewDraw?remakeDecision.frame:0,currentNeuralSourceFrameId,
		remakeDecision.kind==flycast::rend::neural::RemakeDisplayKind::HoldNative,remakeDisplayedEvaluated);
	if(remakePreviewDraw&&remakeDisplayedEvaluated)
		DX11Context::Instance()->QueueNeuralOutputPresent(remakeDecision.frame);
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
	if (queuedPublicOutput)
	{
		++neuralAcceptedBlitCount;
		DX11Context::Instance()->QueueNeuralOutputPresent(queuedNeuralFrameId);
		if (neuralAcceptedBlitCount == 1)
			NOTICE_LOG(RENDERER,
				"DLSS 5 candidate public-output blit queued: frame=%llu route=d3d11on12; external mutation unconfirmed",
				static_cast<unsigned long long>(queuedNeuralFrameId));
		pendingNeuralPresentationFrameId = 0;
	}
	lastPresentedNeuralFrameId = displayedNeuralFrameId;
	releaseNeuralPresentation();
	publishNeuralStatus(lastNeuralSubmitStatus,
		remakePreviewDraw?(remakeDisplayedEvaluated?"Evaluated Remix / original HUD; external DLSS5 mutation unconfirmed":"Raw Remix preview / original HUD; not evaluated by DLSS5"):
		neuralDebugActive ? "developer guidance debug view active; neural output not presented"
			: neuralLiveReason.c_str());
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
	std::size_t neuralOrdinal = flycast::rend::neural::NeuralInstrumentation::MaxDraws;
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
		neuralOrdinal = ordinal;
		constants.neuralDrawId = static_cast<std::uint32_t>(ordinal + 1);
		const int overlayPolicy = std::clamp(config::NeuralOverlayPolicy.get(), 0, 2);
		constants.neuralOverlayMask = overlayPolicy == 0
			&& neuralInstrumentation.IsOverlayOrdinal(ordinal) ? 1.f : 0.f;
		// Mode2 is alpha-aware coverage only for ordinary source-alpha HUD replay.
		// Other blend modes may contribute even with zero alpha.
		if(neuralReactiveCoverageActive && constants.neuralOverlayMask>0.f
			&& gp->tsp.SrcInstr==4 && gp->tsp.DstInstr==5)
			constants.neuralOverlayMask=2.f;
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
		deviceContext->OMSetBlendState(neuralReactiveCoverageActive
			? blendStates.getReactiveCoverageState() : blendStates.getState(false), nullptr, 0xffffffff);
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
	{
#ifdef FLYCAST_ENABLE_NEURAL
		const auto *previousTransform = neuralExportActive
			? neuralInstrumentation.PreviousNaomi2TransformForOrdinal(neuralOrdinal)
			: nullptr;
		n2Helper.setConstants(*gp, 0, *rendContext,
			previousTransform ? previousTransform->modelView.data() : nullptr,
			previousTransform ? previousTransform->projection.data() : nullptr);
#else
		n2Helper.setConstants(*gp, 0, *rendContext);
#endif
		// Polygon number is used only by the OIT renderer.
	}
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
#ifdef FLYCAST_ENABLE_NEURAL
	captureNeuralLateOverlayFrame();
#endif
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
#ifdef FLYCAST_ENABLE_NEURAL
		remakePaletteUpload.reset();
		const auto* remake=std::getenv("FLYCAST_REMAKE_ASYNC_NEURAL");
		if(remake&&remake[0]=='1'&&remake[1]=='\0')
			remakePaletteUpload=flycast::rend::neural::CaptureMaterialPalette(paletteTexture,palette32_ram,pal_hash_16,pal_hash_256);
#endif
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
