// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "motion_reference.h"
#include "neural_frame.h"

#include <array>
#include <cstdint>

struct rend_context;

namespace flycast::rend::neural {

class NeuralInstrumentation final {
public:
	static constexpr std::size_t MaxDraws = 8192;

	void SetEnabled(bool enabled) noexcept;
	bool IsEnabled() const noexcept { return enabled_; }
	void Discontinuity() noexcept;
	const NeuralFrame& CaptureGeometry(const ::rend_context& context, TextureRef color,
		TextureRef depth, std::uint32_t renderWidth, std::uint32_t renderHeight,
		std::uint32_t outputWidth, std::uint32_t outputHeight, Rect contentRect,
		Point2 jitter) noexcept;

	bool Truncated() const noexcept { return truncated_; }
	std::uint64_t DrawSnapshotHash() const noexcept { return drawSnapshotHash_; }
	std::uint32_t HistoryGeneration() const noexcept { return historyGeneration_; }

private:
	using DrawBuffer = std::array<DrawRecord, MaxDraws>;
	using MatchBuffer = std::array<DrawMatch, MaxDraws>;

	DrawBuffer drawBuffers_[2]{};
	MatchBuffer matchBuffer_{};
	std::size_t drawCounts_[2]{};
	std::uint32_t currentBuffer_ = 0;
	std::uint64_t frameId_ = 0;
	std::uint32_t historyGeneration_ = 0;
	std::uint64_t drawSnapshotHash_ = 0;
	bool enabled_ = false;
	bool truncated_ = false;
	bool resetPending_ = true;
	NeuralFrame frame_{};
};

} // namespace flycast::rend::neural
