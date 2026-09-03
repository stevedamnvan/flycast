// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "neural_frame.h"

#include <d3d11.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace flycast::rend::neural {

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

} // namespace flycast::rend::neural
