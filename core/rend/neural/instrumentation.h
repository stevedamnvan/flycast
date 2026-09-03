// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "motion_reference.h"
#include "neural_frame.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

struct rend_context;

namespace flycast::rend::neural {

struct Naomi2Transform {
	std::array<float, 16> modelView{};
	std::array<float, 16> projection{};
	bool valid = false;
};

class NeuralInstrumentation final {
public:
	static constexpr std::size_t MaxDraws = 8192;
	static constexpr std::size_t MaxHistoryVertices = 1024 * 1024;
	static constexpr std::size_t MaxHistoryIndices = 1024 * 1024;

	void SetEnabled(bool enabled) noexcept;
	bool IsEnabled() const noexcept { return enabled_; }
	void Discontinuity() noexcept;
	void SetOverlayGameId(std::string_view gameId) noexcept;
	const NeuralFrame& CaptureGeometry(const ::rend_context& context, TextureRef color,
		TextureRef depth, std::uint32_t renderWidth, std::uint32_t renderHeight,
		std::uint32_t outputWidth, std::uint32_t outputHeight, Rect contentRect,
		Point2 jitter) noexcept;
	const NeuralFrame& CaptureSource(FrameSource source, TextureRef color,
		std::uint32_t renderWidth, std::uint32_t renderHeight,
		std::uint32_t outputWidth, std::uint32_t outputHeight,
		Rect contentRect) noexcept;
	const NeuralFrame& AttachTextures(TextureRef color, TextureRef depth, TextureRef motion,
		TextureRef mask, TextureRef confidence, TextureRef drawId) noexcept;
	void MarkEvaluated(std::uint64_t frameId) noexcept;

	bool Truncated() const noexcept { return truncated_; }
	std::uint64_t DrawSnapshotHash() const noexcept { return drawSnapshotHash_; }
	std::uint32_t HistoryGeneration() const noexcept { return historyGeneration_; }
	ArrayView<PreviousPosition> PreviousPositions() const noexcept
	{
		return {previousPositions_.data(), previousPositions_.size()};
	}
	std::size_t TrustedPreviousVertexCount() const noexcept
	{
		return trustedPreviousVertexCount_;
	}
	const DrawMatch *MatchForOrdinal(std::size_t ordinal) const noexcept
	{
		return ordinal < drawCounts_[currentBuffer_] ? &matchBuffer_[ordinal] : nullptr;
	}
	const Naomi2Transform *PreviousNaomi2TransformForOrdinal(
		std::size_t ordinal) const noexcept;
	bool IsOverlayOrdinal(std::size_t ordinal) const noexcept
	{
		return ordinal < drawCounts_[currentBuffer_] && overlayBuffer_[ordinal] != 0;
	}
	std::size_t OverlayDrawCount() const noexcept { return overlayDrawCount_; }
	std::size_t CurrentDrawCount() const noexcept { return drawCounts_[currentBuffer_]; }
	// Allocates only when a bounded developer capture requests per-draw evidence.
	std::vector<OverlayDrawDiagnostic> CaptureOverlayDiagnostics() const;

private:
	using DrawBuffer = std::array<DrawRecord, MaxDraws>;
	using MatchBuffer = std::array<DrawMatch, MaxDraws>;
	void BeginSource(FrameSource source) noexcept;
	bool CapturePositionSnapshot(const ::rend_context& context) noexcept;
	void CaptureNaomi2Transforms(const ::rend_context& context) noexcept;
	void BuildPreviousPositions(const ::rend_context& context) noexcept;
	void FinalizeConfidence() noexcept;
	void ClassifyOverlays(std::uint32_t renderWidth, std::uint32_t renderHeight) noexcept;

	DrawBuffer drawBuffers_[2]{};
	MatchBuffer matchBuffer_{};
	std::array<std::uint8_t, MaxDraws> overlayBuffer_{};
	std::array<std::array<std::uint8_t, MaxDraws>, 2> overlayStability_{};
	std::size_t overlayDrawCount_ = 0;
	std::size_t drawCounts_[2]{};
	std::vector<PreviousPosition> positionBuffers_[2];
	std::vector<std::uint32_t> indexBuffers_[2];
	std::array<Naomi2Transform, MaxDraws> naomi2TransformBuffers_[2]{};
	std::vector<PreviousPosition> previousPositions_;
	std::size_t trustedPreviousVertexCount_ = 0;
	CorrespondenceStats correspondenceStats_{};
	std::uint32_t candidateDrawsBeforePositionValidation_ = 0;
	std::uint32_t candidateTierDraws_[3]{};
	std::uint64_t candidateAreaBeforePositionValidation_ = 0;
	std::uint32_t referenceBuffer_ = 0;
	std::uint32_t currentBuffer_ = 1;
	std::uint64_t frameId_ = 0;
	std::uint64_t capturedFrameId_ = 0;
	std::uint64_t lastAcceptedFrameId_ = 0;
	std::uint32_t historyAge_ = 0;
	std::uint32_t skippedFrameCount_ = 0;
	bool hasAcceptedFrame_ = false;
	bool sceneCut_ = false;
	std::uint32_t historyGeneration_ = 0;
	std::uint64_t drawSnapshotHash_ = 0;
	bool enabled_ = false;
	bool truncated_ = false;
	bool resetPending_ = true;
	bool hasCapturedFrame_ = false;
	bool hasSource_ = false;
	OverlayProfile overlayProfile_ = OverlayProfile::None;
	FrameSource lastSource_ = FrameSource::Geometry;
	NeuralFrame frame_{};
};

} // namespace flycast::rend::neural
