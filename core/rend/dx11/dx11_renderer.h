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
#pragma once
#include "types.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta_ctx.h"
#include <d3d11.h>
#include "dx11context.h"
#include "rend/transform_matrix.h"
#include "dx11_quad.h"
#include "dx11_texture.h"
#include "dx11_shaders.h"
#include "dx11_renderstate.h"
#include "dx11_naomi2.h"
#ifdef FLYCAST_ENABLE_NEURAL
#include "rend/neural/instrumentation.h"
#include "rend/neural/neural_stage.h"
#include <array>
#endif
#ifndef LIBRETRO
#include "dx11_driver.h"
#endif

struct DX11Renderer : public Renderer
{
	bool Init() override;
	void Term() override;
	void Process(TA_context* ctx) override;
	bool Render() override;
	void RenderFramebuffer(const FramebufferInfo& info) override;

	bool Present() override
	{
		if (!frameRendered || clearLastFrame)
			return false;
		frameRendered = false;
#ifndef LIBRETRO
		imguiDriver->setFrameRendered();
#else
		DX11Context::Instance()->present();
#endif
		return true;
	}

	bool RenderLastFrame() override;
	void ResetNeuralHistory() override
	{
#ifdef FLYCAST_ENABLE_NEURAL
		neuralInstrumentation.Discontinuity();
#endif
	}
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw, int area) override;
	bool GetLastFrame(std::vector<u8>& data, int& width, int& height) override;
#ifdef FLYCAST_ENABLE_NEURAL
	void SetNeuralInstrumentationEnabled(bool enabled) noexcept
	{
		neuralInstrumentation.SetEnabled(enabled);
	}
#endif

protected:
	struct VertexConstants
	{
	    float transMatrix[4][4];
	    float leftPlane[4];
	    float topPlane[4];
		float rightPlane[4];
		float bottomPlane[4];
		float neuralRenderSize[2];
		float neuralPadding[2];
	};

	struct PixelConstants
	{
		float colorClampMin[4];
		float colorClampMax[4];
		float fog_col_vert[4];
		float fog_col_ram[4];
		float ditherDivisor[4];
		float fogDensity;
		float shadowScale;
		float alphaTestValue;
	};

	struct PixelPolyConstants
	{
		float clipTest[4];
		float paletteIndex;
		float trilinearAlpha;
		float neuralConfidence;
		std::uint32_t neuralDrawId;
		float neuralBiasMask;
		float neuralPadding[2];
	};

	virtual void resize(int w, int h);
	bool ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize);
	void createDepthTexAndView(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11DepthStencilView>& view, int width, int height, DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT, UINT bindFlags = 0);
	void createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height);
	void configVertexShader();
	void uploadGeometryBuffers();
	void setupPixelShaderConstants();
	void updateFogTexture();
	void updatePaletteTexture();
	void readRttRenderTarget(u32 texAddress);
	void displayFramebuffer();
	void setCullMode(int mode);
	virtual void setRTTSize(int width, int height) {}
	void writeFramebufferToVRAM();
	void renderVideoRouting();
	void resetContextState();
	void drawOSD();
	TileClipping setTileClip(u32 val, Rect& rect);
#ifdef FLYCAST_ENABLE_NEURAL
	void submitNeuralFrame();
	void submitNeuralFramebuffer();
	bool syncNeuralMode();
	bool ensureNeuralResources();
	bool renderNeuralExports();
	void releaseNeuralResources() noexcept;
	void logNeuralConsumerStatus(flycast::rend::neural::SubmitStatus status) noexcept;
	flycast::rend::neural::Rect getNeuralContentRect() const;
	flycast::rend::neural::TextureRef getNeuralTexture(
		std::array<ComPtr<ID3D11Texture2D>, 3>& textures,
		std::array<ComPtr<ID3D11ShaderResourceView>, 3>& views,
		std::array<ComPtr<ID3D12Resource>, 3>& d3d12Resources,
		DXGI_FORMAT format);
	bool wrapNeuralOutput(ID3D12Resource *resource, std::uint64_t frameId);
	void acquireNeuralInputs();
	void releaseNeuralInputs();
	void releaseNeuralPresentation();
#endif

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11Texture2D> depthTex;
	ComPtr<ID3D11DepthStencilView> depthTexView;
	ComPtr<ID3D11InputLayout> mainInputLayout;
	ComPtr<ID3D11InputLayout> neuralInputLayout;
	ComPtr<ID3D11InputLayout> modVolInputLayout;
	ComPtr<ID3D11Buffer> pxlPolyConstants;
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> modvolBuffer;
	ComPtr<ID3D11RenderTargetView> fbRenderTarget;
	ComPtr<ID3D11RenderTargetView> rttRenderTarget;
	ComPtr<ID3D11ShaderResourceView> fbTextureView;

	BlendStates blendStates;
	DepthStencilStates depthStencilStates;
	Samplers *samplers;
	TransformMatrix matrices{ true };
	D3D11_RECT scissorRect{};
	u32 width = 0;
	u32 height = 0;
	bool frameRendered = false;
	bool frameRenderedOnce = false;
	Naomi2Helper n2Helper;
	float aspectRatio = 4.f / 3.f;
	bool dithering = false;
	rend_context *rendContext;
#ifdef FLYCAST_ENABLE_NEURAL
	flycast::rend::neural::NeuralInstrumentation neuralInstrumentation;
	flycast::rend::neural::NeuralStage neuralStage;
	static constexpr std::size_t NeuralExportRingSize = 3;
	struct NeuralTargetRing
	{
		std::array<ComPtr<ID3D11Texture2D>, NeuralExportRingSize> textures;
		std::array<ComPtr<ID3D11RenderTargetView>, NeuralExportRingSize> targets;
		std::array<ComPtr<ID3D11ShaderResourceView>, NeuralExportRingSize> views;
		std::array<ComPtr<ID3D12Resource>, NeuralExportRingSize> d3d12Resources;
	};
	std::array<ComPtr<ID3D11Texture2D>, NeuralExportRingSize> neuralDepthTextures;
	std::array<ComPtr<ID3D11DepthStencilView>, NeuralExportRingSize> neuralDepthTargets;
	std::array<ComPtr<ID3D11ShaderResourceView>, NeuralExportRingSize> neuralDepthViews;
	std::array<ComPtr<ID3D12Resource>, NeuralExportRingSize> neuralDepthD3D12Resources;
	NeuralTargetRing neuralColor;
	NeuralTargetRing neuralMotion;
	NeuralTargetRing neuralMask;
	NeuralTargetRing neuralConfidence;
	NeuralTargetRing neuralDrawId;
	std::uint32_t neuralDepthWidth = 0;
	std::uint32_t neuralDepthHeight = 0;
	std::size_t neuralExportSlot = 0;
	int activeNeuralMode = -1;
	bool activeNeuralSurface = false;
	bool neuralExportActive = false;
	ComPtr<ID3D11Buffer> neuralPreviousPositionBuffer;
	u32 neuralPreviousPositionBufferSize = 0;
	ComPtr<ID3D11ShaderResourceView> neuralPresentationView;
	std::array<ComPtr<ID3D12Resource>, NeuralExportRingSize> neuralOutputD3D12Resources;
	std::array<ComPtr<ID3D11Texture2D>, NeuralExportRingSize> neuralOutputWrappedTextures;
	std::array<ComPtr<ID3D11ShaderResourceView>, NeuralExportRingSize> neuralOutputWrappedViews;
	bool neuralInputsAcquired = false;
	bool neuralPresentationAcquired = false;
	std::size_t neuralPresentationSlot = 0;
	std::uint64_t pendingNeuralPresentationFrameId = 0;
	std::uint64_t neuralWrappedOutputCount = 0;
	std::uint64_t neuralAcceptedBlitCount = 0;
	flycast::rend::neural::Dlss5HookRoute loggedDlss5Route =
		flycast::rend::neural::Dlss5HookRoute::None;
	flycast::rend::neural::Dlss5HookReadiness loggedDlss5Readiness =
		flycast::rend::neural::Dlss5HookReadiness::Disabled;
	std::uint64_t loggedCompatibilityRebuildAttempts = 0;
	bool loggedDlss5ContractEvaluated = false;
	std::uint64_t loggedEvidenceCaptures = 0;
	std::uint64_t loggedEvidenceCaptureFailures = 0;
	std::uint64_t neuralEvidenceArmDeadlineMs = 0;
#endif

private:
	void prepareRttRenderTarget(u32 texAddress);
	void setBaseScissor();
	void drawStrips();
	template <u32 Type, bool SortingEnabled>
	void drawList(const std::vector<PolyParam>& gply, int first, int count);
	template <u32 Type, bool SortingEnabled>
	void setRenderState(const PolyParam *gp);
	void drawSorted(int first, int count, bool multipass);
	void drawModVols(int first, int count);

	u32 vertexBufferSize = 0;
	u32 modvolBufferSize = 0;
	u32 indexBufferSize = 0;

	ComPtr<ID3D11Texture2D> fbTex;
	ComPtr<ID3D11Texture2D> dcfbTexture;
	ComPtr<ID3D11ShaderResourceView> dcfbTextureView;
	ComPtr<ID3D11Texture2D> paletteTexture;
	ComPtr<ID3D11ShaderResourceView> paletteTextureView;
	ComPtr<ID3D11Texture2D> fogTexture;
	ComPtr<ID3D11ShaderResourceView> fogTextureView;
	ComPtr<ID3D11Texture2D> rttTexture;
	ComPtr<ID3D11Texture2D> rttDepthTex;
	ComPtr<ID3D11DepthStencilView> rttDepthTexView;
	ComPtr<ID3D11Texture2D> whiteTexture;
	ComPtr<ID3D11ShaderResourceView> whiteTextureView;
	ComPtr<ID3D11Texture2D> fbScaledTexture;
	ComPtr<ID3D11ShaderResourceView> fbScaledTextureView;
	ComPtr<ID3D11RenderTargetView> fbScaledRenderTarget;
	ComPtr<ID3D11Texture2D> vrStagingTexture;
	ComPtr<ID3D11ShaderResourceView> vrStagingTextureSRV;
	ComPtr<ID3D11Texture2D> vrScaledTexture;
	ComPtr<ID3D11RenderTargetView> vrScaledRenderTarget;

	ComPtr<ID3D11RasterizerState> rasterCullNone, rasterCullFront, rasterCullBack;

	DX11TextureCache texCache;
	DX11Shaders *shaders;
	std::unique_ptr<Quad> quad;
	ComPtr<ID3D11Buffer> vtxConstants;
	ComPtr<ID3D11Buffer> pxlConstants;
	bool scissorEnable = false;
};
