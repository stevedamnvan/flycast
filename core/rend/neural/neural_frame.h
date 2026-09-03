// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace flycast::rend::neural {

enum class TextureApi : std::uint8_t { None, D3D11, D3D12 };

struct Rect {
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::int32_t width = 0;
	std::int32_t height = 0;
};

// Non-owning API-neutral texture handle. The stage validates the API before
// interpreting resource/view pointers.
struct TextureRef {
	TextureApi api = TextureApi::None;
	void *resource = nullptr;
	void *view = nullptr;
	std::uint32_t format = 0;

	explicit operator bool() const noexcept { return resource != nullptr; }
};

template<typename T>
struct ArrayView {
	const T *data = nullptr;
	std::size_t size = 0;

	const T *begin() const noexcept { return data; }
	const T *end() const noexcept { return data + size; }
	bool empty() const noexcept { return size == 0; }
};

enum class FrameSource : std::uint8_t {
	Geometry,
	FramebufferDirect,
	RenderToTexture,
};

struct DrawRecord {
	std::uint16_t list = 0;
	std::uint16_t pass = 0;
	std::uint32_t stateSig = 0;
	std::uint32_t texId = 0;
	std::uint32_t texId2 = 0;
	std::uint32_t textureGeneration = 0;
	std::uint32_t paletteGeneration = 0;
	std::uint32_t rttGeneration = 0;
	std::uint32_t firstVertex = 0;
	std::uint32_t vertexCount = 0;
	std::uint32_t firstIndex = 0;
	std::uint32_t indexCount = 0;
	std::uint16_t stripCount = 0;
	std::uint32_t uvSig = 0;
	std::uint32_t topologySig = 0;
	float centroid[2]{};
	float zMin = 0.f;
	float zMax = 0.f;
	std::int16_t bboxMin[2]{};
	std::int16_t bboxMax[2]{};
	std::uint16_t ordinal = 0;
	std::uint8_t blend = 0;
	std::uint8_t flags = 0;
	std::int16_t n2Mv = -1;
	std::int16_t n2Proj = -1;
};

struct DrawMatch {
	std::uint16_t prevOrdinal = 0;
	float confidence = 0.f;
	float bestCost = 0.f;
	float secondBestCost = 0.f;
	float fitResidual = 0.f;
	std::uint8_t tier = 0;
	std::uint8_t reason = 0;
	float rigid[4]{};
};

struct OverlayDrawDiagnostic {
	DrawRecord draw;
	std::uint8_t stableAcceptedFrames = 0;
	std::uint16_t textureUseCount = 0;
	bool classified = false;
};

struct PreviousPosition {
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
	// One only when exact index-position correspondence or a bounded, low-residual
	// reindex fit makes the accepted-frame position authoritative. Zero covers
	// new, ambiguous, conflicted, rejected-fit, and Naomi 2 vertices.
	float valid = 0.f;
};

struct CorrespondenceStats {
	std::uint32_t opaqueDraws = 0;
	std::uint32_t punchThroughDraws = 0;
	std::uint32_t translucentDraws = 0;
	std::uint32_t trustedDrawsBeforeSceneCut = 0;
	std::uint32_t candidateDrawsBeforePositionValidation = 0;
	std::uint32_t candidateTier1Draws = 0;
	std::uint32_t candidateTier2Draws = 0;
	std::uint32_t candidateTier3Draws = 0;
	std::uint32_t trustedPreviousVertices = 0;
	std::uint32_t reactiveDraws = 0;
	std::uint32_t ambiguousDraws = 0;
	std::uint32_t unmatchedDraws = 0;
	std::uint64_t matchedAreaBeforeSceneCut = 0;
	std::uint64_t candidateAreaBeforePositionValidation = 0;
	std::uint64_t totalAreaForSceneCut = 0;
	std::uint64_t matchedOpaqueAreaBeforeSceneCut = 0;
	std::uint64_t totalOpaqueAreaForSceneCut = 0;
};

struct NeuralFrame {
	TextureRef color;
	TextureRef depth;
	TextureRef motion;
	TextureRef mask;
	TextureRef confidence;
	TextureRef drawId;
	std::uint32_t renderWidth = 0;
	std::uint32_t renderHeight = 0;
	std::uint32_t outputWidth = 0;
	std::uint32_t outputHeight = 0;
	Rect contentRect;
	float jitterX = 0.f;
	float jitterY = 0.f;
	std::uint64_t frameId = 0;
	std::uint32_t historyGeneration = 0;
	std::uint32_t historyAge = 0;
	std::uint32_t skippedFrameCount = 0;
	bool historyValid = false;
	bool resetHistory = false;
	bool sceneCut = false;
	bool truncated = false;
	bool predominantly2D = false;
	FrameSource source = FrameSource::Geometry;
	ArrayView<DrawRecord> draws;
	ArrayView<DrawMatch> matches;
	CorrespondenceStats correspondence;
};

} // namespace flycast::rend::neural
