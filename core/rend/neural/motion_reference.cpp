// SPDX-License-Identifier: GPL-2.0-or-later
#include "motion_reference.h"

#include <algorithm>
#include <array>
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

bool StructurallyCompatible(const DrawRecord& a, const DrawRecord& b) noexcept
{
	return a.list == b.list && a.pass == b.pass && a.stateSig == b.stateSig &&
		a.texId == b.texId && a.texId2 == b.texId2 && a.uvSig == b.uvSig &&
		a.stripCount == b.stripCount && a.textureGeneration == b.textureGeneration &&
		a.paletteGeneration == b.paletteGeneration && a.rttGeneration == b.rttGeneration;
}

bool SameStructuralIdentity(const DrawRecord& a, const DrawRecord& b) noexcept
{
	return a.list == b.list && a.pass == b.pass && a.stateSig == b.stateSig &&
		a.texId == b.texId && a.texId2 == b.texId2 &&
		a.vertexCount == b.vertexCount && a.indexCount == b.indexCount &&
		a.stripCount == b.stripCount && a.uvSig == b.uvSig &&
		a.topologySig == b.topologySig && a.blend == b.blend && a.flags == b.flags;
}

float SimilarityScore(const DrawRecord& previous, const DrawRecord& current) noexcept
{
	if (previous.list != current.list || previous.pass != current.pass)
		return -1.f;
	if (previous.texId != current.texId || previous.texId2 != current.texId2)
		return -1.f;
	float score = 0.f;
	score += previous.stateSig == current.stateSig ? 3.f : 0.f;
	score += 3.f; // texture identity was required above
	score += previous.uvSig == current.uvSig ? 2.f : 0.f;
	score += previous.topologySig == current.topologySig ? 2.f : 0.f;
	if (previous.textureGeneration != current.textureGeneration ||
		previous.paletteGeneration != current.paletteGeneration ||
		previous.rttGeneration != current.rttGeneration)
		return -1.f;
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

constexpr std::size_t OrdinalWordCount = 65536 / 64;
constexpr std::size_t AssignmentLimit = 8;

float AssignmentCost(const DrawRecord& previous, const DrawRecord& current) noexcept
{
	if (previous.textureGeneration != current.textureGeneration ||
		previous.paletteGeneration != current.paletteGeneration ||
		previous.rttGeneration != current.rttGeneration)
		return 2048.f;
	const float dx = previous.centroid[0] - current.centroid[0];
	const float dy = previous.centroid[1] - current.centroid[1];
	const float previousWidth = static_cast<float>(previous.bboxMax[0] - previous.bboxMin[0]);
	const float previousHeight = static_cast<float>(previous.bboxMax[1] - previous.bboxMin[1]);
	const float currentWidth = static_cast<float>(current.bboxMax[0] - current.bboxMin[0]);
	const float currentHeight = static_cast<float>(current.bboxMax[1] - current.bboxMin[1]);
	const float position = std::sqrt(dx * dx + dy * dy) * .05f;
	const float scale = (std::abs(previousWidth - currentWidth) +
		std::abs(previousHeight - currentHeight)) * .05f;
	const float depth = (std::abs(previous.zMin - current.zMin) +
		std::abs(previous.zMax - current.zMax)) * 10.f;
	const float order = std::abs(static_cast<int>(previous.ordinal) -
		static_cast<int>(current.ordinal)) * .05f;
	return position + scale + depth + order;
}

void MinimumCostAssignment(const float costs[AssignmentLimit][AssignmentLimit],
	std::size_t rows, std::size_t columns, std::int8_t *assignment) noexcept
{
	const std::size_t size = std::max(rows, columns);
	float u[AssignmentLimit + 1]{};
	float v[AssignmentLimit + 1]{};
	std::size_t p[AssignmentLimit + 1]{};
	std::size_t way[AssignmentLimit + 1]{};
	for (std::size_t i = 1; i <= size; ++i)
	{
		p[0] = i;
		std::size_t j0 = 0;
		float minv[AssignmentLimit + 1];
		bool used[AssignmentLimit + 1]{};
		std::fill(std::begin(minv), std::end(minv), std::numeric_limits<float>::infinity());
		do
		{
			used[j0] = true;
			const std::size_t i0 = p[j0];
			float delta = std::numeric_limits<float>::infinity();
			std::size_t j1 = 0;
			for (std::size_t j = 1; j <= size; ++j)
			{
				if (used[j]) continue;
				const float value = i0 <= rows && j <= columns ? costs[i0 - 1][j - 1]
					: (i0 <= rows ? 1024.f : 0.f);
				const float current = value - u[i0] - v[j];
				if (current < minv[j]) { minv[j] = current; way[j] = j0; }
				if (minv[j] < delta) { delta = minv[j]; j1 = j; }
			}
			for (std::size_t j = 0; j <= size; ++j)
				if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
				else minv[j] -= delta;
			j0 = j1;
		} while (p[j0] != 0);
		do
		{
			const std::size_t j1 = way[j0];
			p[j0] = p[j1];
			j0 = j1;
		} while (j0 != 0);
	}
	std::fill(assignment, assignment + rows, static_cast<std::int8_t>(-1));
	for (std::size_t j = 1; j <= size; ++j)
		if (p[j] != 0 && p[j] <= rows && j <= columns)
			assignment[p[j] - 1] = static_cast<std::int8_t>(j - 1);
}

bool PreviousOrdinalUsed(const std::array<std::uint64_t, OrdinalWordCount>& used,
	std::uint16_t ordinal) noexcept
{
	return (used[ordinal / 64] & (std::uint64_t{1} << (ordinal % 64))) != 0;
}

void MarkPreviousOrdinalUsed(std::array<std::uint64_t, OrdinalWordCount>& used,
	std::uint16_t ordinal) noexcept
{
	used[ordinal / 64] |= std::uint64_t{1} << (ordinal % 64);
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
	HashValue(hash, d.texId); HashValue(hash, d.texId2);
	HashValue(hash, d.textureGeneration); HashValue(hash, d.paletteGeneration);
	HashValue(hash, d.rttGeneration); HashValue(hash, d.firstVertex); HashValue(hash, d.vertexCount);
	HashValue(hash, d.firstIndex); HashValue(hash, d.indexCount); HashValue(hash, d.stripCount);
	HashValue(hash, d.uvSig); HashValue(hash, d.topologySig); HashValue(hash, d.ordinal);
	HashValue(hash, d.blend); HashValue(hash, d.flags);
	HashValue(hash, FloatBits(d.zMin)); HashValue(hash, FloatBits(d.zMax));
	HashValue(hash, FloatBits(d.centroid[0])); HashValue(hash, FloatBits(d.centroid[1]));
	HashValue(hash, static_cast<std::uint16_t>(d.n2Mv));
	HashValue(hash, static_cast<std::uint16_t>(d.n2Proj));
	for (auto value : d.bboxMin) HashValue(hash, static_cast<std::uint16_t>(value));
	for (auto value : d.bboxMax) HashValue(hash, static_cast<std::uint16_t>(value));
	return hash;
}

std::uint64_t DrawStructuralSignature(const DrawRecord& d) noexcept
{
	std::uint64_t hash = 1469598103934665603ull;
	HashValue(hash, d.list); HashValue(hash, d.pass); HashValue(hash, d.stateSig);
	HashValue(hash, d.texId); HashValue(hash, d.texId2);
	HashValue(hash, d.vertexCount); HashValue(hash, d.indexCount); HashValue(hash, d.stripCount);
	HashValue(hash, d.uvSig); HashValue(hash, d.topologySig);
	HashValue(hash, d.blend); HashValue(hash, d.flags);
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
	MatchDrawsInto(previous, current, matches.data(), matches.size());
	return matches;
}

void MatchDrawsInto(ArrayView<DrawRecord> previous, ArrayView<DrawRecord> current,
	DrawMatch *matches, std::size_t outputCapacity) noexcept
{
	const std::size_t count = std::min({current.size, outputCapacity, std::size_t{8192}});
	std::fill(matches, matches + count, DrawMatch{});
	std::array<std::uint64_t, OrdinalWordCount> consumed{};
	std::array<std::uint8_t, 8192> bucketed{};
	// Exact structural buckets use a true minimum-cost one-to-one assignment.
	// Large repeated buckets are intentionally ambiguous rather than generating
	// confident particle/HUD matches.
	for (std::size_t ci = 0; ci < count; ++ci)
	{
		if (bucketed[ci]) continue;
		std::size_t currentIndices[AssignmentLimit]{};
		std::size_t previousIndices[AssignmentLimit]{};
		std::size_t currentCount = 0;
		std::size_t previousCount = 0;
		bool oversized = false;
		for (std::size_t cj = ci; cj < count; ++cj)
			if (!bucketed[cj] && !IsReactive(current.data[cj]) &&
				SameStructuralIdentity(current.data[cj], current.data[ci]))
			{
				bucketed[cj] = 1;
				if (currentCount < AssignmentLimit) currentIndices[currentCount++] = cj;
				else oversized = true;
			}
		for (std::size_t pi = 0; pi < previous.size; ++pi)
			if (SameStructuralIdentity(previous.data[pi], current.data[ci]))
			{
				if (previousCount < AssignmentLimit) previousIndices[previousCount++] = pi;
				else oversized = true;
			}
		if (IsReactive(current.data[ci]))
		{
			matches[ci].reason = static_cast<std::uint8_t>(MatchReason::Reactive);
			continue;
		}
		if (oversized)
		{
			for (std::size_t cj = ci; cj < count; ++cj)
				if (SameStructuralIdentity(current.data[cj], current.data[ci]))
					matches[cj].reason = static_cast<std::uint8_t>(MatchReason::Ambiguous);
			continue;
		}
		if (currentCount == 0 || previousCount == 0) continue;
		float costs[AssignmentLimit][AssignmentLimit]{};
		for (std::size_t row = 0; row < currentCount; ++row)
			for (std::size_t column = 0; column < previousCount; ++column)
				costs[row][column] = AssignmentCost(previous.data[previousIndices[column]],
					current.data[currentIndices[row]]);
		std::int8_t assignment[AssignmentLimit]{};
		MinimumCostAssignment(costs, currentCount, previousCount, assignment);
		for (std::size_t row = 0; row < currentCount; ++row)
		{
			if (assignment[row] < 0) continue;
			const auto column = static_cast<std::size_t>(assignment[row]);
			const float assignedCost = costs[row][column];
			if (assignedCost >= 1024.f) continue;
			const auto& prior = previous.data[previousIndices[column]];
			if (PreviousOrdinalUsed(consumed, prior.ordinal)) continue;
			float second = std::numeric_limits<float>::infinity();
			for (std::size_t other = 0; other < previousCount; ++other)
				if (other != column && costs[row][other] < second) second = costs[row][other];
			const float confidence = assignedCost == 0.f && !std::isfinite(second) ? 1.f :
				std::clamp(.95f - assignedCost * .02f -
					(std::isfinite(second) ? std::max(0.f, 1.f - (second - assignedCost)) * .2f : 0.f),
					.5f, .95f);
			matches[currentIndices[row]] = MakeMatch(prior, confidence, 1, MatchReason::Exact);
			matches[currentIndices[row]].bestCost = assignedCost;
			matches[currentIndices[row]].secondBestCost = second;
			MarkPreviousOrdinalUsed(consumed, prior.ordinal);
		}
	}
	std::array<std::uint8_t, 8192> structuralBucketed{};
	for (std::size_t ci = 0; ci < count; ++ci)
	{
		if (matches[ci].confidence != 0.f || matches[ci].reason != 0
			|| structuralBucketed[ci]) continue;
		std::size_t currentIndices[AssignmentLimit]{};
		std::size_t previousIndices[AssignmentLimit]{};
		std::size_t currentCount = 0;
		std::size_t previousCount = 0;
		bool oversized = false;
		for (std::size_t cj = ci; cj < count; ++cj)
			if (matches[cj].confidence == 0.f && matches[cj].reason == 0
				&& StructurallyCompatible(current.data[cj], current.data[ci]))
			{
				structuralBucketed[cj] = 1;
				if (currentCount < AssignmentLimit) currentIndices[currentCount++] = cj;
				else oversized = true;
			}
		for (std::size_t pi = 0; pi < previous.size; ++pi)
			if (!PreviousOrdinalUsed(consumed, previous.data[pi].ordinal)
				&& StructurallyCompatible(previous.data[pi], current.data[ci]))
			{
				if (previousCount < AssignmentLimit) previousIndices[previousCount++] = pi;
				else oversized = true;
			}
		if (oversized)
		{
			for (std::size_t row = 0; row < currentCount; ++row)
				matches[currentIndices[row]].reason = static_cast<std::uint8_t>(
					MatchReason::Ambiguous);
			continue;
		}
		if (currentCount == 0 || previousCount == 0) continue;
		float costs[AssignmentLimit][AssignmentLimit]{};
		for (std::size_t row = 0; row < currentCount; ++row)
			for (std::size_t column = 0; column < previousCount; ++column)
			{
				const auto& prior = previous.data[previousIndices[column]];
				const auto& candidate = current.data[currentIndices[row]];
				costs[row][column] = AssignmentCost(prior, candidate)
					+ std::abs(static_cast<int>(prior.vertexCount)
						- static_cast<int>(candidate.vertexCount)) * .02f
					+ std::abs(static_cast<int>(prior.indexCount)
						- static_cast<int>(candidate.indexCount)) * .01f;
			}
		std::int8_t assignment[AssignmentLimit]{};
		MinimumCostAssignment(costs, currentCount, previousCount, assignment);
		for (std::size_t row = 0; row < currentCount; ++row)
		{
			if (assignment[row] < 0) continue;
			const auto column = static_cast<std::size_t>(assignment[row]);
			const float assignedCost = costs[row][column];
			if (assignedCost >= 1024.f) continue;
			const auto& prior = previous.data[previousIndices[column]];
			float second = std::numeric_limits<float>::infinity();
			for (std::size_t other = 0; other < previousCount; ++other)
				if (other != column && costs[row][other] < second) second = costs[row][other];
			const float confidence = std::clamp(.8f - assignedCost * .02f -
				(std::isfinite(second) ? std::max(0.f, 1.f - (second - assignedCost)) * .2f : 0.f),
				.5f, .8f);
			matches[currentIndices[row]] = MakeMatch(prior, confidence, 2,
				MatchReason::Structural);
			matches[currentIndices[row]].bestCost = assignedCost;
			matches[currentIndices[row]].secondBestCost = second;
			MarkPreviousOrdinalUsed(consumed, prior.ordinal);
		}
	}
	for (std::size_t ci = 0; ci < count; ++ci)
	{
		if (matches[ci].confidence != 0.f || matches[ci].reason != 0)
			continue;
		float bestScore = 7.f;
		float secondScore = -1.f;
		std::size_t best = previous.size;
		for (std::size_t pi = 0; pi < previous.size; ++pi)
		{
			if (PreviousOrdinalUsed(consumed, previous.data[pi].ordinal)) continue;
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
			MarkPreviousOrdinalUsed(consumed, previous.data[best].ordinal);
		}
		else if (best != previous.size)
			matches[ci].reason = static_cast<std::uint8_t>(MatchReason::Ambiguous);
	}
}

float StripCoverage(const DrawRecord& previous, const DrawRecord& current) noexcept
{
	if (previous.list != current.list || previous.pass != current.pass ||
		previous.stateSig != current.stateSig || previous.texId != current.texId ||
		previous.texId2 != current.texId2 ||
		previous.uvSig != current.uvSig || previous.stripCount == 0 || current.stripCount == 0)
		return 0.f;
	const int left = std::max<int>(previous.bboxMin[0], current.bboxMin[0]);
	const int top = std::max<int>(previous.bboxMin[1], current.bboxMin[1]);
	const int right = std::min<int>(previous.bboxMax[0], current.bboxMax[0]);
	const int bottom = std::min<int>(previous.bboxMax[1], current.bboxMax[1]);
	const int currentWidth = std::max(0, static_cast<int>(current.bboxMax[0]) - current.bboxMin[0]);
	const int currentHeight = std::max(0, static_cast<int>(current.bboxMax[1]) - current.bboxMin[1]);
	const int currentArea = currentWidth * currentHeight;
	if (currentArea == 0) return 0.f;
	const int overlapArea = std::max(0, right - left) * std::max(0, bottom - top);
	return static_cast<float>(overlapArea) / static_cast<float>(currentArea);
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

namespace {

void Transform4(const float *matrix, const float input[4], float output[4]) noexcept
{
	for (int row = 0; row < 4; ++row)
		output[row] = matrix[row] * input[0] + matrix[4 + row] * input[1] +
			matrix[8 + row] * input[2] + matrix[12 + row] * input[3];
}

} // namespace

Point2 ProjectNaomi2(const float *modelView, const float *projection,
	const float *ndc, Point3 position) noexcept
{
	const float input[4] = {position.x, position.y, position.z, 1.f};
	float view[4]{};
	float clip[4]{};
	Transform4(modelView, input, view);
	Transform4(projection, view, clip);
	if (std::abs(clip[3]) <= std::numeric_limits<float>::epsilon()) return {};
	const float projected[4] = {clip[0] / clip[3], clip[1] / clip[3],
		1.f / clip[3], 1.f};
	float output[4]{};
	Transform4(ndc, projected, output);
	return {output[0], output[1]};
}

MotionTrust ClassifyMotion(const DrawMatch& match, Point2 motion,
	bool resetHistory, bool truncated) noexcept
{
	MotionTrust result;
	result.motion = motion;
	result.confidence = match.confidence;
	const float magnitudeSquared = motion.x * motion.x + motion.y * motion.y;
	const bool reactive = match.reason == static_cast<std::uint8_t>(MatchReason::Reactive);
	result.trusted = !resetHistory && !truncated && !reactive &&
		match.confidence >= .5f && magnitudeSquared <= 128.f * 128.f;
	result.biasCurrentColor = !result.trusted;
	if (!result.trusted)
		result.motion = {};
	return result;
}

bool IsSceneCut(std::uint64_t matchedArea, std::uint64_t totalArea, float minimumRatio) noexcept
{
	return totalArea == 0 || static_cast<double>(matchedArea) /
		static_cast<double>(totalArea) < minimumRatio;
}

float InvertLegacyDepth(float encodedDepth, bool divPosZ) noexcept
{
	const float scaled = std::exp2(encodedDepth * 34.f) - 1.f;
	if (scaled <= std::numeric_limits<float>::epsilon())
		return 0.f;
	return divPosZ ? 100000.f / scaled : scaled / 100000.f;
}

Rect ComputeContentRect(std::uint32_t outputWidth, std::uint32_t outputHeight,
	float renderAspect, bool integerScale, std::uint32_t renderResolution) noexcept
{
	Rect result{0, 0, static_cast<std::int32_t>(outputWidth),
		static_cast<std::int32_t>(outputHeight)};
	if (outputWidth == 0 || outputHeight == 0 || renderAspect <= 0.f)
		return {};
	if (integerScale)
	{
		if (renderResolution == 0) return result;
		const int framebufferHeight = static_cast<int>(renderResolution);
		const int framebufferWidth = static_cast<int>(renderAspect * framebufferHeight);
		int scale = std::min(static_cast<int>(outputWidth) / framebufferWidth,
			static_cast<int>(outputHeight) / framebufferHeight);
		if (scale == 0)
		{
			scale = std::max(framebufferWidth / static_cast<int>(outputWidth),
				framebufferHeight / static_cast<int>(outputHeight)) + 1;
			result.x = (static_cast<int>(outputWidth) - framebufferWidth / scale) / 2;
			result.y = (static_cast<int>(outputHeight) - framebufferHeight / scale) / 2;
		}
		else
		{
			result.x = (static_cast<int>(outputWidth) - framebufferWidth * scale) / 2;
			result.y = (static_cast<int>(outputHeight) - framebufferHeight * scale) / 2;
		}
		result.width = static_cast<std::int32_t>(outputWidth) - 2 * result.x;
		result.height = static_cast<std::int32_t>(outputHeight) - 2 * result.y;
		return result;
	}
	const float screenAspect = static_cast<float>(outputWidth) / outputHeight;
	if (renderAspect > screenAspect)
	{
		result.y = static_cast<std::int32_t>(std::lround(outputHeight *
			(1.f - screenAspect / renderAspect) / 2.f));
		result.height -= 2 * result.y;
	}
	else
	{
		result.x = static_cast<std::int32_t>(std::lround(outputWidth *
			(1.f - renderAspect / screenAspect) / 2.f));
		result.width -= 2 * result.x;
	}
	return result;
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
