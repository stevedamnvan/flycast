// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_frame.h"
#include "producer_identity.h"
#include "remake_alpha_ownership.h"
#include "pvr_scene_capture.h"
#include "remake_view_scene.h"
#include "remake_view_transport.h"
#include "remake_live_channel.h"
#include <d3d11.h>
#include "windows/comptr.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <functional>
struct rend_context;

namespace flycast::rend::neural {
class RemakeOitEffects;
// Only called inside the existing bounded synchronous preview capture.
bool CaptureRemakeGuidance(const std::filesystem::path& root,ID3D11Device*,ID3D11DeviceContext*,
 const RemakeReturnedImage&,std::uint64_t current,std::uint64_t guidanceFrame,
 const std::array<ID3D11Texture2D*,6>&,std::string& error);

// Synchronous developer-only preview proof; caller bounds attempts and supplies
// receipt-matched original surfaces. Not external neural provenance.
bool CaptureRemakePreview(const std::filesystem::path& root, ID3D11Device*,
	ID3D11DeviceContext*, const RemakeReturnedImage&, std::uint64_t current,
	ID3D11Texture2D* original, ID3D11Texture2D* mask,
	ID3D11Texture2D* composite, ID3D11Texture2D* backbuffer, std::string& error,
	ID3D11Texture2D* evaluated = nullptr, const remake::Packet* scene = nullptr,
	std::uint64_t replayOriginalFrame = 0, ID3D11Texture2D* preEffects = nullptr,
	const RemakeOitEffects* effects = nullptr,const std::vector<AlphaEffectSelection>& alphaSelections = {},
	const std::string& sessionToken = {});

enum class CaptureGpuTimingPoint : std::uint8_t {
	PvrBegin,
	PvrEnd,
	GuidanceBegin,
	GuidanceEnd,
	EvaluateBegin,
	EvaluateEnd,
	CompositeBegin,
	CompositeEnd,
	Count,
};

struct QualityGpuTimings {
	bool available = false;
	bool pvrAvailable = false;
	bool guidanceAvailable = false;
	bool evaluateAvailable = false;
	bool compositeAvailable = false;
	bool totalAvailable = false;
	double pvrMs = 0.;
	double guidanceMs = 0.;
	double evaluateMs = 0.;
	double compositeMs = 0.;
	double totalMs = 0.;
};

struct QualityCaptureMetadata {
	ProducerIdentity producerIdentity;
	std::uint64_t frameId = 0;
	std::uint32_t historyGeneration = 0;
	std::uint32_t historyAge = 0;
	std::uint32_t skippedFrameCount = 0;
	std::uint32_t renderWidth = 0;
	std::uint32_t renderHeight = 0;
	std::uint32_t outputWidth = 0;
	std::uint32_t outputHeight = 0;
	std::uint32_t screenWidth = 0;
	std::uint32_t screenHeight = 0;
	std::uint32_t drawCount = 0;
	float jitterX = 0.f;
	float jitterY = 0.f;
	bool rasterJitterApplied = false;
	std::string rasterJitterReason = "not-selected";
	CorrespondenceStats correspondence{};
	Rect contentRect{};
	bool historyValid = false;
	bool resetHistory = false;
	bool sceneCut = false;
	bool truncated = false;
	bool predominantly2D = false;
	bool evaluationAccepted = false;
	bool externalContractEvaluated = false;
	bool externalOutputConfirmed = false;
	bool d3d11On12 = false;
	bool oitRenderer = false;
	int neuralMode = 0;
	int dlssPreset = 0;
	int overlayPolicy = 0;
	OverlayProfile overlayProfile = OverlayProfile::None;
	std::string gameId;
	std::string submitStatus;
	std::string remakeInput = "native-pvr";
	std::string profile = "unassigned";
	std::string externalRecommendation = "user-controlled";
	QualityGpuTimings gpuTimings{};
	std::vector<OverlayDrawDiagnostic> overlayDraws;
};

struct PvrReplayTextures {
	ComPtr<ID3D11Texture2D> priorFramebuffer;
	std::array<ComPtr<ID3D11Texture2D>,4> color; // decoded, wrong viewport, wrong depth, retained native buffers
};
struct QualityCaptureTextures {
	std::function<bool(const RemakeReturnedImage&, ComPtr<ID3D11Texture2D>&, std::string&)> remakeComposite;
	RemakeTextureReader remakeTextureReader;
	std::function<bool(const std::filesystem::path&, std::string&)> pvrMaterials;
	std::function<bool(const std::filesystem::path&, PvrReplayTextures&, std::string&)> pvrReplay;
	bool pvrPacketRequested = false;
	const rend_context *pvrContext = nullptr;
	std::array<float,16> pvrViewport{};
	ID3D11Texture2D *nativeColor = nullptr;
	ID3D11Texture2D *sourceColor = nullptr;
	ID3D11Texture2D *depth = nullptr;
	ID3D11Texture2D *motion = nullptr;
	ID3D11Texture2D *biasMask = nullptr;
	ID3D11Texture2D *confidence = nullptr;
	ID3D11Texture2D *drawId = nullptr;
	ID3D11Texture2D *overlay = nullptr;
	ID3D11Texture2D *publicOutput = nullptr;
	ID3D11Texture2D *finalComposite = nullptr;
};

// Deliberately synchronous and developer-only. This class is never used unless
// an explicit capture directory and positive frame limit are supplied.
class QualityCaptureWriter {
public:
	struct RgbaImage {
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::vector<std::uint8_t> pixels;
	};

	void Configure(const std::filesystem::path& root, std::uint32_t skip,
		std::uint32_t limit, bool lateOverlayProof = false, std::uint64_t startFrame = 0, std::uint64_t startProducer = 0);
	void SetSourceFrame(std::uint64_t frame,std::uint64_t producer = 0) noexcept { sourceFrame_ = frame; sourceProducer_ = producer; }
	bool WantsFrame() const noexcept;
	bool CapturesCurrentFrame() const noexcept;
	// Returns true once, immediately before the first retained frame. The
	// renderer uses this capture-only boundary to start every comparison lane
	// from the same accepted-history and raster-jitter phase.
	bool ConsumeCaptureStart() noexcept;
	bool Capture(ID3D11Device *device, ID3D11DeviceContext *context,
		const QualityCaptureMetadata& metadata, const QualityCaptureTextures& textures,
		std::string& error);
	bool CapturePresentedWithFlycastOverlays(ID3D11Device *device,
		ID3D11DeviceContext *context, ID3D11Texture2D *presented,
		std::uint64_t frameId, const Rect& contentRect, std::string& error);
	std::uint32_t CapturedCount() const noexcept { return captured_; }
	// Render-thread-only borrowed view; invalidated by Configure or next Capture.
	// A captured projected packet, not a recovered camera/world scene.
	const PvrDecodedPacket* CapturedPvrSnapshot(std::uint64_t frame) const noexcept {
		return pvrSnapshot_ && pvrSnapshot_->frame==frame ? &*pvrSnapshot_ : nullptr;
	}
	const RemakeViewScene* CapturedRemakeViewScene(std::uint64_t frame) const noexcept {
		return remakeView_ && remakeView_->frame==frame ? &*remakeView_ : nullptr;
	}
	const remake::Packet* CapturedRemakePacket(std::uint64_t frame) const noexcept {
		return remakePacket_ && remakePacket_->frame==frame ? &*remakePacket_ : nullptr;
	}
	const std::string& RemakePacketStatus() const noexcept {return remakePacketStatus_;}
	bool RemakeInputReplayed() const noexcept { return remakeInputReplayed_; }
	bool PrepareRemakeBeforeComposite(const PvrDecodedPacket&,const QualityCaptureMetadata&,const RemakeTextureReader&,
		const std::function<void()>& beforeExchange = {});
	const RemakeReturnedImage* ReturnedRemakeFrame(std::uint64_t frame) const noexcept {
		return remakeReturnedImage_&&remakeReturnedImage_->frame==frame ? &*remakeReturnedImage_ : nullptr;
	}

private:
	void ExchangeRemakePacket();
	bool remakePreparedBeforeComposite_=false;
	bool remakeInputReplayed_=false;
	std::uint64_t remakeReplayOriginalFrame_=0;
	RemakeLiveChannel remakeChannel_;
	std::optional<RemakeReturnedImage> remakeReturnedImage_;
	std::optional<RemakeViewScene> remakeView_;
	std::optional<remake::Packet> remakePacket_;
	std::string remakePacketStatus_="not-requested";
	std::optional<PvrDecodedPacket> pvrSnapshot_;
	std::filesystem::path root_;
	std::uint32_t skip_ = 0;
	std::uint32_t limit_ = 0;
	std::uint32_t seen_ = 0;
	std::uint64_t sourceFrame_ = 0, startFrame_ = 0;
	std::uint64_t sourceProducer_ = 0, startProducer_ = 0;
	std::uint32_t captured_ = 0;
	std::uint32_t lateOverlayCaptured_ = 0;
	std::uint64_t previousFrameId_ = 0;
	std::uint64_t pendingLateOverlayFrameId_ = 0;
	Rect pendingLateOverlayContentRect_{};
	bool lateOverlayProof_ = false;
	bool captureStartConsumed_ = false;
	RgbaImage previousFinal_;
	RgbaImage previousSource_;
	RgbaImage pendingPreFlycastOverlayFull_;
};

// Deliberately synchronous capture-only timing. It is activated only for a
// frame the bounded quality writer will retain and is never performance data.
class QualityCaptureGpuTimer {
public:
	void Configure(ID3D11Device *device, bool enabled);
	void BeginFrame(ID3D11DeviceContext *context, bool captureCurrentFrame);
	void Mark(ID3D11DeviceContext *context, CaptureGpuTimingPoint point);
	bool EndAndResolve(ID3D11DeviceContext *context, QualityGpuTimings& timings);

private:
	static constexpr std::size_t PointCount =
		static_cast<std::size_t>(CaptureGpuTimingPoint::Count);
	void Reset();
	ComPtr<ID3D11Device> device_;
	ComPtr<ID3D11Query> disjoint_;
	std::array<ComPtr<ID3D11Query>, PointCount> points_;
	std::array<bool, PointCount> marked_{};
	bool active_ = false;
};

} // namespace flycast::rend::neural
