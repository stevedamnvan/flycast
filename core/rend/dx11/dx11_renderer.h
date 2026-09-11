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
#include "rend/neural/live_status.h"
#include "rend/neural/neural_stage.h"
#include "rend/neural/performance_tracker.h"
#include "rend/neural/quality_capture.h"
#include "rend/neural/pvr_material_capture.h"
#include "rend/neural/remake_overlay_snapshot.h"
#include "rend/neural/remake_native_effects.h"
#include "rend/neural/remake_presentation.h"
#include "rend/neural/remake_neural_input.h"
#include "rend/neural/remake_camera_anchor.h"
#include "rend/neural/remake_feed_worker.h"
#include "rend/neural/remake_return_worker.h"
#include "rend/neural/remake_frame_budget.h"
#include "rend/neural/remake_motion_stream.h"
#include "remake_motion_raster.h"
#include <array>
#include <set>
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
#ifdef FLYCAST_ENABLE_NEURAL
		neuralPerformance.RecordPresent();
		neuralStage.NotifyHostPresent();
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
	virtual bool IsOitRenderer() const noexcept { return false; }

	struct VertexConstants
	{
	    float transMatrix[4][4];
	    float leftPlane[4];
	    float topPlane[4];
		float rightPlane[4];
		float bottomPlane[4];
		float neuralRenderSize[2];
		float neuralRasterJitter[2];
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
		float constantPadding;
	};
	static_assert(sizeof(PixelConstants)==96,"Pixel constants must fill the GPU allocation");

	struct PixelPolyConstants
	{
		float clipTest[4];
		float paletteIndex;
		float trilinearAlpha;
		float neuralConfidence;
		std::uint32_t neuralDrawId;
		float neuralBiasMask;
		std::uint32_t neuralPreviousDrawId;
		float neuralOverlayMask;
		float neuralPadding;
	};

	virtual void resize(int w, int h);
	bool ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize);
	void createDepthTexAndView(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11DepthStencilView>& view, int width, int height, DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT, UINT bindFlags = 0);
	void createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height);
	void configVertexShader(float rasterJitterX = 0.f, float rasterJitterY = 0.f, const float *capturedViewport = nullptr);
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
	void captureNativeParityFrame();
	TileClipping setTileClip(u32 val, Rect& rect);
#ifdef FLYCAST_ENABLE_NEURAL
	void submitNeuralFrame();
	void prepareRemakeCapture();
	void prepareRemakeAsyncFeed();
	// D-213/D-214: take prepared returns one at a time and apply the identity gate (feed start and evaluation start).
	void drainRemakeReturns(std::uint64_t currentFrame,const flycast::rend::neural::ProducerIdentity& producer);
	flycast::rend::neural::RemakeDisplayDecision selectRemakePreview(bool permitted);
	bool applyRemakeCaptureInput(flycast::rend::neural::NeuralFrame& frame);
	bool uploadRemakeInput(const flycast::rend::neural::RemakeNeuralInput& input,bool bracket=true);
	void evaluateRemakeAsync(flycast::rend::neural::NeuralFrame frame);
	flycast::rend::neural::MaterialShaderGlobals materialShaderGlobals;
	void submitNeuralFramebuffer();
	bool syncNeuralMode();
	bool ensureNeuralResources();
	bool renderNeuralExports(float rasterJitterX, float rasterJitterY);
	virtual bool renderNeuralSceneColor(float rasterJitterX, float rasterJitterY);
	bool prepareNeuralSceneColorTarget(ID3D11RenderTargetView *target);
	bool updateNeuralRetainedScene();
	bool renderNeuralDisocclusion();
	virtual bool renderNeuralReactiveCoverage();
	bool mergeNeuralReactiveCoverage(ID3D11ShaderResourceView *coverageView);
	void releaseNeuralResources() noexcept;
	void logNeuralConsumerStatus(flycast::rend::neural::SubmitStatus status) noexcept;
	flycast::rend::neural::Rect getNeuralContentRect() const;
	flycast::rend::neural::TextureRef getNeuralTexture(
		std::array<ComPtr<ID3D11Texture2D>, 3>& textures,
		std::array<ComPtr<ID3D11ShaderResourceView>, 3>& views,
		std::array<ComPtr<ID3D12Resource>, 3>& d3d12Resources,
		DXGI_FORMAT format);
	bool ensureNeuralOutputWrapped(ID3D12Resource *resource, std::size_t& slot);
	bool wrapNeuralOutput(ID3D12Resource *resource, std::uint64_t frameId);
	bool retainNeuralOutputForCapture(ID3D12Resource *resource);
	void acquireNeuralInputs();
	void releaseNeuralInputs();
	void acquireNeuralHistory();
	void releaseNeuralHistory();
	void releaseNeuralPresentation();
	void captureNeuralQualityFrame();
	void retainPvrReplayBase();
	bool replayPvrPacket(const std::filesystem::path&, flycast::rend::neural::PvrReplayTextures&, std::string&);
	void captureNeuralLateOverlayFrame();
	void beginNeuralPerformanceFrame();
	void markNeuralPvrEnd();
	void endNeuralPerformanceFrame();
	virtual std::uint32_t neuralResourceObjectCount() const noexcept;
	void publishNeuralStatus(flycast::rend::neural::SubmitStatus status,
		const char *reason = nullptr);
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
	ComPtr<ID3D11Texture2D> neuralSceneDepthTexture;
	ComPtr<ID3D11DepthStencilView> neuralSceneDepthTarget;
	ComPtr<ID3D11Texture2D> neuralRetainedSceneTexture;
	ComPtr<ID3D11RenderTargetView> neuralRetainedSceneTarget;
	ComPtr<ID3D11ShaderResourceView> neuralRetainedSceneView;
	bool neuralRetainedSceneValid = false;
	ComPtr<ID3D11Texture2D> pvrReplayBase;
	ComPtr<ID3D11Buffer> pvrReplayPixelConstants;
	VertexConstants pvrReplayNativeVertexConstants{};
	bool pvrReplayNativeVertexValid = false;
	NeuralTargetRing neuralColor;
	NeuralTargetRing neuralMotion;
	NeuralTargetRing neuralMask;
	NeuralTargetRing neuralResolvedMask;
	NeuralTargetRing neuralConfidence;
	NeuralTargetRing neuralDrawId;
	NeuralTargetRing neuralPreviousDrawId;
	NeuralTargetRing neuralOverlayMask;
	std::uint32_t neuralDepthWidth = 0;
	std::uint32_t neuralDepthHeight = 0;
	std::size_t neuralExportSlot = 0;
	int activeNeuralMode = -1;
	int activeNeuralPreset = -1;
	int activeNeuralFailureInjection = -1;
	int activeNeuralFailureInjectionCount = -1;
	int activeNeuralFailureInjectionAfter = -1;
	bool activeNeuralSurface = false;
	bool activeNeuralGpuTiming = false;
	bool neuralExportActive = false;
	std::uint64_t neuralGuidanceReplayCount = 0;
	bool neuralReactiveCoverageActive = false;
	bool loggedNeuralRasterJitter = false;
	bool hasLoggedNeuralRasterJitter = false;
	ComPtr<ID3D11Buffer> neuralPreviousPositionBuffer;
	u32 neuralPreviousPositionBufferSize = 0;
	ComPtr<ID3D11ShaderResourceView> neuralPresentationView;
	std::array<ComPtr<ID3D12Resource>, NeuralExportRingSize> neuralOutputD3D12Resources;
	std::array<ComPtr<ID3D11Texture2D>, NeuralExportRingSize> neuralOutputWrappedTextures;
	std::array<ComPtr<ID3D11ShaderResourceView>, NeuralExportRingSize> neuralOutputWrappedViews;
	bool neuralInputsAcquired = false;
	bool neuralHistoryAcquired = false;
	bool hasNeuralAcceptedGuidance = false;
	std::size_t neuralAcceptedGuidanceSlot = 0;
	bool neuralPresentationAcquired = false;
	std::size_t neuralPresentationSlot = 0;
	std::uint64_t pendingNeuralPresentationFrameId = 0;
	std::uint64_t currentNeuralSourceFrameId = 0;
	std::uint64_t currentNeuralGuidanceFrameId = 0;
	std::uint64_t lastPresentedNeuralFrameId = 0;
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
	int loggedOverlayPolicy = -1;
	int loggedQualityProfile = -1;
	int loggedStyleFamily = -1;
	int loggedNeuralDebugView = -1;
	bool loggedNeuralDebugActive = false;
	std::string loggedOverlayGameId;
	bool loggedOverlayActive = false;
	bool neural2DBypassActive = false;
	flycast::rend::neural::SubmitStatus lastNeuralSubmitStatus =
		flycast::rend::neural::SubmitStatus::Disabled;
	std::string neuralLiveReason = "off";
	std::uint8_t neural2DBypassEnterStreak = 0;
	std::uint8_t neural2DBypassExitStreak = 0;
	std::uint32_t loggedNeuralRenderWidth = 0;
	std::uint32_t loggedNeuralRenderHeight = 0;
	std::uint32_t loggedNeuralOutputWidth = 0;
	std::uint32_t loggedNeuralOutputHeight = 0;
	std::uint64_t neuralEvidenceArmDeadlineMs = 0;
	flycast::rend::neural::QualityCaptureWriter neuralQualityCapture;
	flycast::rend::neural::RemakeTextureCache remakeAsyncTextures;
	std::shared_ptr<const flycast::rend::neural::MaterialPaletteSnapshot> remakePaletteUpload;
	flycast::rend::neural::RemakeLiveChannel remakeAsyncChannel;
	flycast::rend::neural::RemakeFeedWorker remakeFeedWorker; // Owns the camera anchor (D-211).
	// Textures the live consumer holds for the open channel session (D-212):
	// keyed by texture identity, cleared with the channel, bounded like the consumer.
	std::shared_ptr<const flycast::rend::neural::RemakeSentTextureSet> remakeSentTextures;std::size_t remakeSentTextureBytes=0;
	// D-213 returned-image preparation off the render thread; the prepared
	// stream/input are consumed by the next evaluation of the same source.
	flycast::rend::neural::RemakeReturnWorker remakeReturnWorker;
	struct RemakePreparedReturn {
		std::uint64_t frame=0,sequence=0,previousFrame=0;bool temporal=false,streamReady=false,inputReady=false;
		std::string streamError;flycast::rend::neural::RemakeMotionStream stream;flycast::rend::neural::RemakeNeuralInput input;
	};
	std::optional<RemakePreparedReturn> remakePreparedReturn;
	flycast::rend::neural::RemakeFrameBudget remakeFrameBudget;bool remakeFrameBudgetConfigured=false; // D-216.
	std::uint64_t remakeFrameBudgetFrames=0;
	struct RemakeOwnedOutput { ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11ShaderResourceView> view; };
	std::array<RemakeOwnedOutput,3> remakeOwnedOutputs;std::size_t remakeOwnedOutputNext=0; // D-215 ring.
	ComPtr<ID3D11DeviceContext> remakeDisplayContext;
	std::unique_ptr<Quad> remakeDisplayQuad;
	ComPtr<ID3D11Texture2D> remakeDepthUpload; // D-217: persistent inverted-depth upload.
	unsigned remakeReturnTimingCount=0;
	unsigned remakeFeedTimingCount=0;
	flycast::rend::neural::RemakeTemporalHistory remakeTemporalHistory;
	flycast::rend::neural::RemakeMotionRaster remakeMotionRaster;
	flycast::rend::neural::RemakeRasterOutput remakeAcceptedRaster;
	std::uint64_t remakeAcceptedRasterFrame=0;
	std::optional<flycast::rend::neural::RemakeReturnedImage> remakeAsyncReturned;
	std::optional<flycast::rend::neural::RemakeReturnedImage> remakeEvaluatedSource;
	flycast::rend::neural::RemakeOverlaySnapshot remakeEvaluatedOverlay;
	ComPtr<ID3D11Texture2D> remakeEvaluatedTexture;
	ComPtr<ID3D11Texture2D> remakePreEffectTexture; // Explicit preview capture only.
	ComPtr<ID3D11ShaderResourceView> remakeEvaluatedView;
	std::uint64_t remakeLastEvaluationAttempt=0;
	std::chrono::steady_clock::time_point remakeFrameEndAt{};unsigned remakeFrameScopeCounts[8]{}; // Diagnostics only.
	std::optional<std::uint8_t> remakeSourceAlphaReference;
	// D-214: four slots by channel sequence. The channel keeps two sources in
	// flight, and the return worker may receive (releasing return credit) before
	// the render thread has taken the image, so the next publish must not land
	// on the slot that image still needs.
	static constexpr std::size_t RemakeOverlaySlots=4;
	std::array<flycast::rend::neural::RemakeOverlaySnapshot,RemakeOverlaySlots> remakeAsyncOverlaySources;
	flycast::rend::neural::RemakeOverlaySnapshot remakeAsyncAcceptedOverlay;
	flycast::rend::neural::RemakeOverlaySnapshot remakeWarmupNative;
	std::shared_ptr<const flycast::rend::neural::RemakeOitEffects> remakeCurrentEffects;
	std::string remakeCurrentEffectsReason="unsupported-renderer";
	std::unique_ptr<flycast::rend::neural::NativeEffectSnapshot> nativeEffectProof;
	unsigned nativeEffectProofAttempts=0,nativeEffectProofDraws=0;
	bool nativeEffectCapturePass=false;
	std::shared_ptr<const flycast::rend::neural::NativeEffectSnapshot> remakeCurrentNormalEffects;
	// LOG895: retired native-effect copies of identical shape are reused across
	// frames on this device; snapshots keep exclusive ownership while alive.
	std::shared_ptr<flycast::rend::neural::NativeResourcePool> remakeNativeResourcePool;
	void finishNativeEffectProof();

	flycast::rend::neural::RemakePresentationPolicy remakePresentationPolicy;
	ComPtr<ID3D11Texture2D> remakeCompositeTexture;
	ComPtr<ID3D11ShaderResourceView> remakeCompositeView,remakeDisplayedView;
	std::uint64_t remakeCompositeFrame=0,remakeDisplayedFrame=0;
	bool remakeCompositeEvaluated=false,remakeDisplayedEvaluated=false;
	unsigned remakePreviewCaptureAttempts=0;
	unsigned remakeEffectReplayAttempts=0;
	std::uint64_t remakePreviewLastCaptured=0;
	void resetRemakeAsyncFrames() {
		remakeFeedWorker.ResetAnchor();remakeReturnWorker.Attach(nullptr); // Re-attached by the next feed once the channel is open again.
		remakeSentTextures.reset();remakeSentTextureBytes=0;
		retireRemakeHistory();
	}
	// D-221: an in-session re-anchor no longer discards the returns in flight
	// (accepted evaluations of pre-cut sources, presented in source order); the
	// history reset is deferred to the first return of a post-cut source.
	bool remakeHistoryResetPending=false;std::uint64_t remakeHistoryResetAfterFrame=0;
	void retireRemakeHistoryKeepingReturns() {
		remakePreEffectTexture.reset();remakeCurrentEffects.reset();remakeCurrentNormalEffects.reset();
		remakeTemporalHistory.Reset();
		remakeAcceptedRaster={};remakeAcceptedRasterFrame=0;
		remakeEvaluatedSource.reset();remakeEvaluatedOverlay={};remakeEvaluatedTexture.reset();remakeEvaluatedView.reset();
		remakeWarmupNative={};remakePresentationPolicy.Reset();remakeCompositeTexture.reset();
		remakeCompositeView.reset();remakeDisplayedView.reset();remakeCompositeFrame=remakeDisplayedFrame=0;
		remakeCompositeEvaluated=remakeDisplayedEvaluated=false;
	}
	// Retire every cross-frame history and presentation carry-over without
	// touching the anchor, channel or texture cache: used at an in-session
	// anchor generation change so no pre-cut image is presented after the cut.
	void retireRemakeHistory() {
		remakePreEffectTexture.reset();
		remakeCurrentEffects.reset();remakeCurrentNormalEffects.reset();
		retireRemakeTemporalHistory();
	}
	// Retire temporal/raster history, pending returns and presentation carry-over
	// but keep the current frame's own native effects: used when the anchored
	// basis jumps inside a continuing arena, so nothing reprojects across a cut.
	void retireRemakeTemporalHistory() {
		remakeHistoryResetPending=false;
		remakeReturnWorker.Discard();remakePreparedReturn.reset();
		remakeTemporalHistory.Reset();
		// D-220: the motion raster holds no cross-frame history (its retained
		// output is remakeAcceptedRaster, reset here); destroying it made the first
		// evaluation after a re-anchor recompile its shaders (a 266 ms present).
		remakeAcceptedRaster={};remakeAcceptedRasterFrame=0;
		remakeAsyncReturned.reset();remakeAsyncOverlaySources={};remakeAsyncAcceptedOverlay={};
		remakeEvaluatedSource.reset();remakeEvaluatedOverlay={};remakeEvaluatedTexture.reset();remakeEvaluatedView.reset();remakeLastEvaluationAttempt=0;
		remakeWarmupNative={};remakePresentationPolicy.Reset();remakeCompositeTexture.reset();
		remakeCompositeView.reset();remakeDisplayedView.reset();remakeCompositeFrame=remakeDisplayedFrame=0;
		remakeCompositeEvaluated=remakeDisplayedEvaluated=false;
	}
	std::string remakeAsyncToken;
	std::string remakeSessionRoot;
	bool remakeSessionRenewalRequested=true;
	std::uint64_t remakeAsyncEpoch=0;
	bool remakeAsyncStopped=false;
	flycast::rend::neural::QualityCaptureGpuTimer neuralQualityCaptureGpuTimer;
	flycast::rend::neural::QualityCaptureMetadata neuralQualityCaptureMetadata;
	ComPtr<ID3D11ShaderResourceView> neuralQualityCapturePublicView;
	std::size_t neuralQualityCapturePublicSlot = NeuralExportRingSize;
	ComPtr<ID3D11ShaderResourceView> neuralCaptureOnlyPublicView;
	std::size_t neuralCaptureOnlyPublicSlot = NeuralExportRingSize;
	bool neuralQualityCapturePending = false;
	flycast::rend::neural::PerformanceTracker neuralPerformance;
#endif

private:
	void prepareRttRenderTarget(u32 texAddress);
	void setBaseScissor();
	void drawStrips();
	template <u32 Type, bool SortingEnabled>
	void drawList(const std::vector<PolyParam>& gply, int first, int count);
	template <u32 Type, bool SortingEnabled>
	void setRenderState(const PolyParam *gp, u32 neuralOrdinalOverride = ~0u);
	void drawSorted(int first, int count, bool multipass);
	void drawModVols(int first, int count);

	u32 vertexBufferSize = 0;
	u32 modvolBufferSize = 0;
	u32 indexBufferSize = 0;

	ComPtr<ID3D11Texture2D> fbTex;
	ComPtr<ID3D11Texture2D> nativeParityStagingTexture;
	std::uint32_t nativeParitySeenFrames = 0;
	std::uint32_t nativeParityCapturedFrames = 0;
	bool nativeParityCaptureComplete = false;
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
