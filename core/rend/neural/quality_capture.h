// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_frame.h"
#include <d3d11.h>
#include "windows/comptr.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace flycast::rend::neural {

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
	std::uint64_t frameId = 0;
	std::uint32_t historyGeneration = 0;
	std::uint32_t historyAge = 0;
	std::uint32_t skippedFrameCount = 0;
	std::uint32_t renderWidth = 0;
	std::uint32_t renderHeight = 0;
	std::uint32_t outputWidth = 0;
	std::uint32_t outputHeight = 0;
	std::uint32_t drawCount = 0;
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
	std::string gameId;
	std::string submitStatus;
	std::string profile = "unassigned";
	std::string externalRecommendation = "user-controlled";
	QualityGpuTimings gpuTimings{};
	std::vector<OverlayDrawDiagnostic> overlayDraws;
};

struct QualityCaptureTextures {
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
		std::uint32_t limit);
	bool WantsFrame() const noexcept;
	bool CapturesCurrentFrame() const noexcept;
	bool Capture(ID3D11Device *device, ID3D11DeviceContext *context,
		const QualityCaptureMetadata& metadata, const QualityCaptureTextures& textures,
		std::string& error);
	std::uint32_t CapturedCount() const noexcept { return captured_; }

private:
	std::filesystem::path root_;
	std::uint32_t skip_ = 0;
	std::uint32_t limit_ = 0;
	std::uint32_t seen_ = 0;
	std::uint32_t captured_ = 0;
	std::uint64_t previousFrameId_ = 0;
	RgbaImage previousFinal_;
	RgbaImage previousSource_;
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
