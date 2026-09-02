// SPDX-License-Identifier: GPL-2.0-or-later
#include "motion_reference.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace flycast::rend::neural {
namespace {

template<typename T>
void HashValue(std::uint64_t& hash, T value) noexcept
{
	for (std::size_t i = 0; i < sizeof(T); ++i)
	{
		hash ^= static_cast<std::uint8_t>(value & 0xffu);
		hash *= 1099511628211ull;
		value >>= 8;
	}
}

bool ExactCompatible(const DrawRecord& a, const DrawRecord& b) noexcept
{
	return a.list == b.list && a.pass == b.pass && a.stateSig == b.stateSig &&
		a.texId == b.texId && a.vertexCount == b.vertexCount && a.indexCount == b.indexCount &&
		a.stripCount == b.stripCount && a.uvSig == b.uvSig && a.geomSig == b.geomSig &&
		std::abs(static_cast<int>(a.ordinal) - static_cast<int>(b.ordinal)) <= 2;
}

bool StructurallyCompatible(const DrawRecord& a, const DrawRecord& b) noexcept
{
	return a.list == b.list && a.pass == b.pass && a.stateSig == b.stateSig &&
		a.texId == b.texId && a.uvSig == b.uvSig && a.stripCount == b.stripCount;
}

float SimilarityScore(const DrawRecord& previous, const DrawRecord& current) noexcept
{
	if (previous.list != current.list || previous.pass != current.pass)
		return -1.f;
	float score = 0.f;
	score += previous.stateSig == current.stateSig ? 3.f : 0.f;
	score += previous.texId == current.texId ? 3.f : 0.f;
	score += previous.uvSig == current.uvSig ? 2.f : 0.f;
	score += previous.geomSig == current.geomSig ? 2.f : 0.f;
	const auto vertexDelta = std::abs(static_cast<int>(previous.vertexCount) -
		static_cast<int>(current.vertexCount));
	score += vertexDelta == 0 ? 2.f : (vertexDelta <= 4 ? 1.f : 0.f);
	const auto ordinalDelta = std::abs(static_cast<int>(previous.ordinal) -
		static_cast<int>(current.ordinal));
	score += ordinalDelta <= 2 ? 1.f : 0.f;
	return score;
}

DrawMatch MakeMatch(const DrawRecord& previous, float confidence, std::uint8_t tier,
	MatchReason reason) noexcept
{
	DrawMatch result{};
	result.prevOrdinal = previous.ordinal;
	result.confidence = confidence;
	result.tier = tier;
	result.reason = static_cast<std::uint8_t>(reason);
	result.rigid[0] = 1.f;
	return result;
}

std::uint32_t FloatBits(float value) noexcept
{
	std::uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

} // namespace

Point2 SimilarityTransform::Apply(Point2 point) const noexcept
{
	return {scaleCos * point.x - scaleSin * point.y + translateX,
		scaleSin * point.x + scaleCos * point.y + translateY};
}

std::uint64_t DrawSignature(const DrawRecord& d) noexcept
{
	std::uint64_t hash = 1469598103934665603ull;
	HashValue(hash, d.list); HashValue(hash, d.pass); HashValue(hash, d.stateSig);
	HashValue(hash, d.texId); HashValue(hash, d.firstVertex); HashValue(hash, d.vertexCount);
	HashValue(hash, d.firstIndex); HashValue(hash, d.indexCount); HashValue(hash, d.stripCount);
	HashValue(hash, d.uvSig); HashValue(hash, d.geomSig); HashValue(hash, d.ordinal);
	HashValue(hash, d.blend); HashValue(hash, d.flags);
	HashValue(hash, FloatBits(d.zMin)); HashValue(hash, FloatBits(d.zMax));
	HashValue(hash, static_cast<std::uint16_t>(d.n2Mv));
	HashValue(hash, static_cast<std::uint16_t>(d.n2Proj));
	for (auto value : d.bboxMin) HashValue(hash, static_cast<std::uint16_t>(value));
	for (auto value : d.bboxMax) HashValue(hash, static_cast<std::uint16_t>(value));
	return hash;
}

bool IsReactive(const DrawRecord& draw) noexcept
{
	const bool explicitlyReactive = (draw.flags & (DrawReactive | DrawAdditive | DrawDegenerate)) != 0;
	const auto width = std::max(0, static_cast<int>(draw.bboxMax[0]) - draw.bboxMin[0]);
	const auto height = std::max(0, static_cast<int>(draw.bboxMax[1]) - draw.bboxMin[1]);
	const bool smallTranslucent = draw.list == 2 && width * height <= 32 * 32;
	return explicitlyReactive || smallTranslucent;
}

std::vector<DrawMatch> MatchDraws(ArrayView<DrawRecord> previous, ArrayView<DrawRecord> current)
{
	std::vector<DrawMatch> matches(current.size);
	std::vector<bool> consumed(previous.size, false);
	for (std::size_t ci = 0; ci < current.size; ++ci)
	{
		const auto& draw = current.data[ci];
		if (IsReactive(draw))
		{
			matches[ci].reason = static_cast<std::uint8_t>(MatchReason::Reactive);
			continue;
		}
		for (std::size_t pi = 0; pi < previous.size; ++pi)
			if (!consumed[pi] && ExactCompatible(previous.data[pi], draw))
			{
				matches[ci] = MakeMatch(previous.data[pi], 1.f, 1, MatchReason::Exact);
				consumed[pi] = true;
				break;
			}
	}
	for (std::size_t ci = 0; ci < current.size; ++ci)
	{
		if (matches[ci].confidence != 0.f ||
			matches[ci].reason == static_cast<std::uint8_t>(MatchReason::Reactive))
			continue;
		for (std::size_t pi = 0; pi < previous.size; ++pi)
			if (!consumed[pi] && StructurallyCompatible(previous.data[pi], current.data[ci]))
			{
				matches[ci] = MakeMatch(previous.data[pi], .8f, 2, MatchReason::Structural);
				consumed[pi] = true;
				break;
			}
	}
	for (std::size_t ci = 0; ci < current.size; ++ci)
	{
		if (matches[ci].confidence != 0.f ||
			matches[ci].reason == static_cast<std::uint8_t>(MatchReason::Reactive))
			continue;
		float bestScore = 7.f;
		float secondScore = -1.f;
		std::size_t best = previous.size;
		for (std::size_t pi = 0; pi < previous.size; ++pi)
		{
			if (consumed[pi]) continue;
			const float score = SimilarityScore(previous.data[pi], current.data[ci]);
			if (score > bestScore)
			{
				secondScore = bestScore;
				bestScore = score;
				best = pi;
			}
			else if (score > secondScore)
				secondScore = score;
		}
		if (best != previous.size && bestScore - secondScore >= 1.f)
		{
			matches[ci] = MakeMatch(previous.data[best], .5f, 3, MatchReason::Similarity);
			consumed[best] = true;
		}
		else if (best != previous.size)
			matches[ci].reason = static_cast<std::uint8_t>(MatchReason::Ambiguous);
	}
	return matches;
}

SimilarityTransform FitSimilarity(ArrayView<Point2> previous, ArrayView<Point2> current) noexcept
{
	SimilarityTransform transform;
	if (previous.size != current.size || previous.size < 2)
		return transform;
	Point2 prevCenter{};
	Point2 currCenter{};
	for (std::size_t i = 0; i < previous.size; ++i)
	{
		prevCenter.x += previous.data[i].x; prevCenter.y += previous.data[i].y;
		currCenter.x += current.data[i].x; currCenter.y += current.data[i].y;
	}
	const float inverseCount = 1.f / static_cast<float>(previous.size);
	prevCenter.x *= inverseCount; prevCenter.y *= inverseCount;
	currCenter.x *= inverseCount; currCenter.y *= inverseCount;
	float dot = 0.f;
	float cross = 0.f;
	float currentNorm = 0.f;
	for (std::size_t i = 0; i < previous.size; ++i)
	{
		const float cx = current.data[i].x - currCenter.x;
		const float cy = current.data[i].y - currCenter.y;
		const float px = previous.data[i].x - prevCenter.x;
		const float py = previous.data[i].y - prevCenter.y;
		dot += cx * px + cy * py;
		cross += cx * py - cy * px;
		currentNorm += cx * cx + cy * cy;
	}
	if (currentNorm <= std::numeric_limits<float>::epsilon())
		return transform;
	transform.scaleCos = dot / currentNorm;
	transform.scaleSin = cross / currentNorm;
	transform.translateX = prevCenter.x - transform.scaleCos * currCenter.x +
		transform.scaleSin * currCenter.y;
	transform.translateY = prevCenter.y - transform.scaleSin * currCenter.x -
		transform.scaleCos * currCenter.y;
	transform.valid = true;
	return transform;
}

bool IsSceneCut(std::uint64_t matchedArea, std::uint64_t totalArea, float minimumRatio) noexcept
{
	return totalArea == 0 || static_cast<double>(matchedArea) /
		static_cast<double>(totalArea) < minimumRatio;
}

float Halton(std::uint32_t index, std::uint32_t base) noexcept
{
	float result = 0.f;
	float fraction = 1.f;
	while (index != 0)
	{
		fraction /= static_cast<float>(base);
		result += fraction * static_cast<float>(index % base);
		index /= base;
	}
	return result;
}

Point2 HaltonJitter(std::uint64_t frameId, std::uint32_t phaseCount) noexcept
{
	if (phaseCount == 0) return {};
	const auto phase = static_cast<std::uint32_t>(frameId % phaseCount) + 1u;
	return {Halton(phase, 2) - .5f, Halton(phase, 3) - .5f};
}

std::uint32_t JitterPhaseCount(std::uint32_t renderWidth, std::uint32_t outputWidth) noexcept
{
	if (renderWidth == 0 || outputWidth == 0) return 8;
	const double ratio = static_cast<double>(outputWidth) / renderWidth;
	return std::max(1u, static_cast<std::uint32_t>(std::lround(8. * ratio * ratio)));
}

void HistoryTracker::Discontinuity() noexcept
{
	++generation_;
	resetPending_ = true;
	consecutiveSkips_ = 0;
}

void HistoryTracker::Skipped() noexcept
{
	++consecutiveSkips_;
	resetPending_ = true;
}

void HistoryTracker::Evaluated(std::uint64_t frameId) noexcept
{
	evaluatedFrameId_ = frameId;
	consecutiveSkips_ = 0;
}

bool HistoryTracker::ConsumeReset() noexcept
{
	const bool value = resetPending_;
	resetPending_ = false;
	return value;
}

} // namespace flycast::rend::neural
