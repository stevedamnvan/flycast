// SPDX-License-Identifier: GPL-2.0-or-later
#include "instrumentation.h"

#include "hw/pvr/ta_ctx.h"
#include "rend/TexCache.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace flycast::rend::neural {
namespace {

void Mix(std::uint64_t& hash, std::uint32_t value) noexcept
{
	for (int i = 0; i < 4; ++i)
	{
		hash ^= static_cast<std::uint8_t>(value & 0xff);
		hash *= 1099511628211ull;
		value >>= 8;
	}
}

std::uint32_t Fold(std::uint64_t hash) noexcept
{
	return static_cast<std::uint32_t>(hash ^ (hash >> 32));
}

std::int16_t QuantizeCoord(float value) noexcept
{
	return static_cast<std::int16_t>(std::clamp(std::lround(value),
		static_cast<long>(std::numeric_limits<std::int16_t>::min()),
		static_cast<long>(std::numeric_limits<std::int16_t>::max())));
}

std::uint32_t QuantizeFloat(float value, float scale) noexcept
{
	return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(value * scale)));
}

bool IsNondegenerateTriangle(const rend_context& context,
	const std::array<std::uint32_t, 3>& triangle) noexcept
{
	for (const auto index : triangle)
		if (index >= context.verts.size()) return false;
	const auto& a = context.verts[triangle[0]];
	const auto& b = context.verts[triangle[1]];
	const auto& c = context.verts[triangle[2]];
	const double area = (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y)
		- (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
	return std::isfinite(area) && std::abs(area) > .01;
}

bool IsAxisAlignedQuad(const rend_context& context,
	const std::array<std::uint32_t, 3>& first,
	const std::array<std::uint32_t, 3>& second) noexcept
{
	if (!IsNondegenerateTriangle(context, first)
		|| !IsNondegenerateTriangle(context, second)) return false;
	const std::array<std::uint32_t, 6> indices = {
		first[0], first[1], first[2], second[0], second[1], second[2]
	};
	float xMin = std::numeric_limits<float>::infinity();
	float yMin = std::numeric_limits<float>::infinity();
	float xMax = -std::numeric_limits<float>::infinity();
	float yMax = -std::numeric_limits<float>::infinity();
	for (const auto index : indices)
	{
		if (index >= context.verts.size()) return false;
		const auto& vertex = context.verts[index];
		xMin = std::min(xMin, vertex.x); yMin = std::min(yMin, vertex.y);
		xMax = std::max(xMax, vertex.x); yMax = std::max(yMax, vertex.y);
	}
	if (!std::isfinite(xMin) || !std::isfinite(yMin)
		|| !std::isfinite(xMax) || !std::isfinite(yMax)
		|| xMax - xMin <= .5f || yMax - yMin <= .5f)
		return false;
	std::array<std::uint8_t, 4> cornerUses{};
	for (const auto index : indices)
	{
		const auto& vertex = context.verts[index];
		const bool left = std::abs(vertex.x - xMin) <= .25f;
		const bool right = std::abs(vertex.x - xMax) <= .25f;
		const bool top = std::abs(vertex.y - yMin) <= .25f;
		const bool bottom = std::abs(vertex.y - yMax) <= .25f;
		if ((!left && !right) || (!top && !bottom)) return false;
		if (left && top) ++cornerUses[0];
		else if (left && bottom) ++cornerUses[1];
		else if (right && top) ++cornerUses[2];
		else if (right && bottom) ++cornerUses[3];
	}
	std::sort(cornerUses.begin(), cornerUses.end());
	return cornerUses == std::array<std::uint8_t, 4>{1, 1, 2, 2};
}

std::uint16_t CountScreenAlignedQuads(const rend_context& context,
	std::size_t begin, std::size_t end, bool triangleList) noexcept
{
	if (begin >= end || end > context.idx.size()) return 0;
	std::uint32_t count = 0;
	std::array<std::uint32_t, 3> pending{};
	bool hasPending = false;
	auto consume = [&](const std::array<std::uint32_t, 3>& triangle) {
		if (!IsNondegenerateTriangle(context, triangle)) return true;
		if (!hasPending)
		{
			pending = triangle;
			hasPending = true;
			return true;
		}
		if (!IsAxisAlignedQuad(context, pending, triangle)) return false;
		hasPending = false;
		++count;
		return true;
	};
	if (triangleList)
	{
		if ((end - begin) % 3 != 0) return 0;
		for (std::size_t group = begin; group < end; group += 3)
		{
			const std::array<std::uint32_t, 3> triangle = {
				context.idx[group], context.idx[group + 1], context.idx[group + 2]
			};
			if (!consume(triangle)) return 0;
		}
	}
	else
	{
		std::size_t segment = begin;
		for (std::size_t i = begin; i <= end; ++i)
		{
			const bool separator = i == end || context.idx[i] == ~std::uint32_t{0};
			if (!separator) continue;
			for (std::size_t vertex = segment + 2; vertex < i; ++vertex)
			{
				const std::array<std::uint32_t, 3> triangle = {
					context.idx[vertex - 2], context.idx[vertex - 1], context.idx[vertex]
				};
				if (!consume(triangle)) return 0;
			}
			if (hasPending) return 0;
			segment = i + 1;
		}
	}
	if (hasPending) return 0;
	return static_cast<std::uint16_t>(std::min<std::uint32_t>(
		count, std::numeric_limits<std::uint16_t>::max()));
}

bool UsesProfileOverlayContinuity(const DrawRecord& draw,
	OverlayProfile profile) noexcept
{
	return profile == OverlayProfile::SoulcaliburT1401nHudV1
		&& draw.list == 2 && draw.indexCount >= 4 && draw.bboxMin[1] >= 0
		&& draw.bboxMax[1] <= 96;
}

std::uint64_t OverlayContinuitySignature(const DrawRecord& draw,
	OverlayProfile profile) noexcept
{
	std::uint64_t hash = 1469598103934665603ull;
	Mix(hash, draw.list); Mix(hash, draw.pass); Mix(hash, draw.stateSig);
	Mix(hash, draw.texId); Mix(hash, draw.texId2);
	Mix(hash, draw.textureGeneration); Mix(hash, draw.paletteGeneration);
	Mix(hash, draw.rttGeneration); Mix(hash, draw.vertexCount); Mix(hash, draw.indexCount);
	Mix(hash, draw.screenAlignedPrimitiveCount); Mix(hash, draw.blend);
	// Generic single quads keep the older exact UV/topology continuity rule.
	// Proven batches, and the exact Soulcalibur top-HUD profile, may legitimately
	// rebuild atlas indices or animate glyph UVs while their screen bounds remain fixed.
	const bool soulcaliburTopBatch = UsesProfileOverlayContinuity(draw, profile);
	if (draw.screenAlignedPrimitiveCount <= 1 && !soulcaliburTopBatch)
	{
		Mix(hash, draw.uvSig);
		Mix(hash, draw.topologySig);
	}
	Mix(hash, draw.flags & (DrawRtt | DrawNaomi2 | DrawDegenerate
		| DrawScreenAligned | DrawTriangleList));
	Mix(hash, static_cast<std::uint16_t>(draw.bboxMin[0]));
	Mix(hash, static_cast<std::uint16_t>(draw.bboxMin[1]));
	Mix(hash, static_cast<std::uint16_t>(draw.bboxMax[0]));
	Mix(hash, static_cast<std::uint16_t>(draw.bboxMax[1]));
	return hash;
}

std::uint32_t PassFor(const rend_context& context, std::uint32_t index,
	std::uint16_t list) noexcept
{
	for (std::uint32_t pass = 0; pass < context.render_passes.size(); ++pass)
	{
		const auto& value = context.render_passes[pass];
		std::uint32_t end = 0;
		switch (list)
		{
		case ListType_Opaque: end = value.op_count; break;
		case ListType_Punch_Through: end = value.pt_count; break;
		case ListType_Translucent: end = value.tr_count; break;
		default: break;
		}
		if (index < end) return pass;
	}
	return context.render_passes.empty() ? 0 :
		static_cast<std::uint32_t>(context.render_passes.size() - 1);
}

DrawRecord MakeRecord(const rend_context& context, const PolyParam& poly,
	std::uint16_t list, std::uint32_t listIndex, std::uint16_t ordinal,
	bool triangleList = false) noexcept
{
	DrawRecord record{};
	record.list = list;
	record.pass = static_cast<std::uint16_t>(PassFor(context, listIndex, list));
	std::uint64_t state = 1469598103934665603ull;
	Mix(state, poly.pcw.full & 0x300ceu);
	Mix(state, poly.isp.full & 0xf4000000u);
	Mix(state, poly.tsp.full);
	Mix(state, poly.tileclip);
	Mix(state, poly.tsp1.full);
	record.stateSig = Fold(state);
	record.texId = poly.tcw.full;
	record.texId2 = poly.tcw1.full;
	if (poly.texture != nullptr)
	{
		record.textureGeneration = poly.texture->Updates;
		record.paletteGeneration = poly.texture->palette_hash;
		record.rttGeneration = poly.texture->rttGeneration;
		if (record.rttGeneration != 0) record.flags |= DrawRtt;
	}
	if (poly.texture1 != nullptr)
	{
		// Fold the second-volume resource revisions without confusing them with
		// either texture's immutable TCW identity.
		record.textureGeneration ^= poly.texture1->Updates * 0x9e3779b9u;
		record.paletteGeneration ^= poly.texture1->palette_hash * 0x85ebca6bu;
		record.rttGeneration ^= poly.texture1->rttGeneration * 0xc2b2ae35u;
		if (poly.texture1->rttGeneration != 0) record.flags |= DrawRtt;
	}
	record.firstIndex = poly.first;
	record.indexCount = poly.count;
	record.stripCount = poly.count == 0 ? 0 : 1;
	record.ordinal = ordinal;
	if (triangleList) record.flags |= DrawTriangleList;
	record.blend = static_cast<std::uint8_t>((poly.tsp.SrcInstr << 3) | poly.tsp.DstInstr);
	record.n2Mv = static_cast<std::int16_t>(poly.mvMatrix);
	record.n2Proj = static_cast<std::int16_t>(poly.projMatrix);
	if (poly.isNaomi2()) record.flags |= DrawNaomi2;
	if (list == ListType_Translucent && poly.count <= 6) record.flags |= DrawReactive;
	if (list == ListType_Translucent && poly.tsp.SrcInstr == 1 && poly.tsp.DstInstr == 1)
		record.flags |= DrawAdditive;

	std::uint64_t uvHash = 1469598103934665603ull;
	std::uint64_t topologyHash = 1469598103934665603ull;
	float zMin = std::numeric_limits<float>::infinity();
	float zMax = -std::numeric_limits<float>::infinity();
	float xMin = std::numeric_limits<float>::infinity();
	float yMin = std::numeric_limits<float>::infinity();
	float xMax = -std::numeric_limits<float>::infinity();
	float yMax = -std::numeric_limits<float>::infinity();
	std::uint32_t firstVertex = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t lastVertex = 0;
	float xSum = 0.f;
	float ySum = 0.f;
	std::uint32_t sampleCount = 0;
	const auto end = std::min<std::size_t>(context.idx.size(),
		static_cast<std::size_t>(poly.first) + poly.count);
	for (std::size_t i = poly.first; i < end; ++i)
	{
		const std::uint32_t vertexIndex = context.idx[i];
		if (vertexIndex >= context.verts.size()) continue;
		const auto& vertex = context.verts[vertexIndex];
		firstVertex = std::min(firstVertex, vertexIndex);
		lastVertex = std::max(lastVertex, vertexIndex);
		xMin = std::min(xMin, vertex.x); yMin = std::min(yMin, vertex.y);
		xMax = std::max(xMax, vertex.x); yMax = std::max(yMax, vertex.y);
		zMin = std::min(zMin, vertex.z); zMax = std::max(zMax, vertex.z);
		Mix(uvHash, QuantizeFloat(vertex.u, 4096.f));
		Mix(uvHash, QuantizeFloat(vertex.v, 4096.f));
		xSum += vertex.x;
		ySum += vertex.y;
		++sampleCount;
	}
	if (firstVertex == std::numeric_limits<std::uint32_t>::max())
	{
		record.flags |= DrawDegenerate;
		firstVertex = 0;
		zMin = zMax = xMin = yMin = xMax = yMax = 0.f;
	}
	record.firstVertex = firstVertex;
	record.vertexCount = sampleCount != 0 && lastVertex >= firstVertex
		? lastVertex - firstVertex + 1 : 0;
	record.uvSig = Fold(uvHash);
	for (std::size_t i = poly.first; i < end; ++i)
	{
		const std::uint32_t vertexIndex = context.idx[i];
		Mix(topologyHash, vertexIndex >= firstVertex ? vertexIndex - firstVertex : vertexIndex);
	}
	record.topologySig = Fold(topologyHash);
	if (sampleCount != 0)
	{
		record.centroid[0] = xSum / sampleCount;
		record.centroid[1] = ySum / sampleCount;
	}
	record.zMin = zMin;
	record.zMax = zMax;
	record.bboxMin[0] = QuantizeCoord(xMin);
	record.bboxMin[1] = QuantizeCoord(yMin);
	record.bboxMax[0] = QuantizeCoord(xMax);
	record.bboxMax[1] = QuantizeCoord(yMax);
	record.screenAlignedPrimitiveCount = CountScreenAlignedQuads(
		context, poly.first, end, triangleList);
	if (record.screenAlignedPrimitiveCount != 0) record.flags |= DrawScreenAligned;
	return record;
}

template<typename Container>
void AppendList(const rend_context& context, const Container& polys, std::uint16_t list,
	std::array<DrawRecord, NeuralInstrumentation::MaxDraws>& output, std::size_t& count,
	bool& truncated) noexcept
{
	for (std::uint32_t i = 0; i < polys.size(); ++i)
	{
		if (count == output.size())
		{
			truncated = true;
			return;
		}
		const auto ordinal = static_cast<std::uint16_t>(count);
		PolyParam poly = polys[i];
		if (list == ListType_Translucent)
		{
			const auto pass = PassFor(context, i, list);
			if (pass < context.render_passes.size()
				&& context.render_passes[pass].sorted_tr_count
					> (pass == 0 ? 0 : context.render_passes[pass - 1].sorted_tr_count))
				// Triangle sorting leaves this offset in vertex space. Retain the
				// ordinal as an empty placeholder; capture actual submissions below.
				poly.count = 0;
		}
		output[count++] = MakeRecord(context, poly, list, i, ordinal);
	}
}

void AppendSortedTranslucent(const rend_context& context,
	std::array<DrawRecord, NeuralInstrumentation::MaxDraws>& output,
	std::size_t& count, bool& truncated) noexcept
{
	std::size_t pass = 0;
	for (std::size_t i = 0; i < context.sortedTriangles.size(); ++i)
	{
		if (count == output.size()) { truncated = true; return; }
		const auto& sorted = context.sortedTriangles[i];
		if (sorted.polyIndex >= context.global_param_tr.size())
		{
			truncated = true;
			return;
		}
		while (pass + 1 < context.render_passes.size()
			&& i >= context.render_passes[pass].sorted_tr_count) ++pass;
		PolyParam poly = context.global_param_tr[sorted.polyIndex];
		poly.first = sorted.first;
		poly.count = sorted.count;
		auto record = MakeRecord(context, poly, ListType_Translucent,
			sorted.polyIndex, static_cast<std::uint16_t>(count), true);
		record.pass = static_cast<std::uint16_t>(pass);
		record.flags |= DrawTriangleList | DrawReactive;
		output[count++] = record;
	}
}

} // namespace

void NeuralInstrumentation::SetEnabled(bool enabled) noexcept
{
	if (enabled_ == enabled) return;
	enabled_ = enabled;
	Discontinuity();
	if (!enabled)
	{
		drawCounts_[0] = drawCounts_[1] = 0;
		positionBuffers_[0].clear();
		positionBuffers_[1].clear();
		indexBuffers_[0].clear();
		indexBuffers_[1].clear();
		naomi2TransformBuffers_[0].fill({});
		naomi2TransformBuffers_[1].fill({});
		previousPositions_.clear();
		trustedPreviousVertexCount_ = 0;
		referenceBuffer_ = 0;
		currentBuffer_ = 1;
		hasCapturedFrame_ = false;
		hasAcceptedFrame_ = false;
		historyAge_ = 0;
		skippedFrameCount_ = 0;
		sceneCut_ = false;
		overlayDrawCount_ = 0;
		overlayBuffer_.fill(0);
		overlayStability_[0].fill(0);
		overlayStability_[1].fill(0);
		hasSource_ = false;
		frame_ = {};
	}
}

void NeuralInstrumentation::SetOverlayGameId(std::string_view gameId) noexcept
{
	const auto profile = gameId == "T1401N"
		? OverlayProfile::SoulcaliburT1401nHudV1 : OverlayProfile::None;
	if (profile == overlayProfile_) return;
	overlayProfile_ = profile;
	// A title boundary is also a temporal correspondence boundary.
	Discontinuity();
	overlayStability_[0].fill(0);
	overlayStability_[1].fill(0);
}

void NeuralInstrumentation::ClassifyOverlays(std::uint32_t renderWidth,
	std::uint32_t renderHeight) noexcept
{
	overlayBuffer_.fill(0);
	overlayStability_[currentBuffer_].fill(0);
	overlayDrawCount_ = 0;
	const auto currentCount = drawCounts_[currentBuffer_];
	const auto referenceCount = drawCounts_[referenceBuffer_];
	struct Occurrence
	{
		enum : std::size_t { Capacity = 4 };
		std::uint16_t count = 0;
		std::array<std::size_t, Capacity> indices{};
		void Add(std::size_t index) noexcept
		{
			if (count < Capacity) indices[count] = index;
			if (count != std::numeric_limits<std::uint16_t>::max()) ++count;
		}
	};
	try
	{
		std::unordered_map<std::uint64_t, Occurrence> currentOccurrences;
		std::unordered_map<std::uint64_t, Occurrence> referenceOccurrences;
		std::unordered_map<std::uint32_t, std::uint16_t> textureOccurrences;
		currentOccurrences.reserve(currentCount);
		referenceOccurrences.reserve(referenceCount);
		textureOccurrences.reserve(currentCount);
		for (std::size_t i = 0; i < currentCount; ++i)
		{
			auto& occurrence = currentOccurrences[
				OverlayContinuitySignature(drawBuffers_[currentBuffer_][i], overlayProfile_)];
			occurrence.Add(i);
			const auto& draw = drawBuffers_[currentBuffer_][i];
			const auto texture = draw.texId;
			if (texture != 0 && draw.indexCount != 0
				&& (draw.flags & DrawDegenerate) == 0 && textureOccurrences[texture]
				!= std::numeric_limits<std::uint16_t>::max())
				++textureOccurrences[texture];
		}
		for (std::size_t i = 0; i < referenceCount; ++i)
		{
			auto& occurrence = referenceOccurrences[
				OverlayContinuitySignature(drawBuffers_[referenceBuffer_][i], overlayProfile_)];
			occurrence.Add(i);
		}
		for (std::size_t ci = 0; ci < currentCount; ++ci)
		{
			const auto& current = drawBuffers_[currentBuffer_][ci];
			const auto signature = OverlayContinuitySignature(current, overlayProfile_);
			const auto currentOccurrence = currentOccurrences.find(signature);
			const auto referenceOccurrence = referenceOccurrences.find(signature);
			std::uint8_t stability = 1;
			if (currentOccurrence != currentOccurrences.end()
				&& referenceOccurrence != referenceOccurrences.end())
			{
				const auto currentCountForSignature = currentOccurrence->second.count;
				const auto referenceCountForSignature = referenceOccurrence->second.count;
				std::size_t referenceIndex = NeuralInstrumentation::MaxDraws;
				if (currentCountForSignature == 1 && referenceCountForSignature == 1)
					referenceIndex = referenceOccurrence->second.indices[0];
				else if (UsesProfileOverlayContinuity(current, overlayProfile_)
					&& currentCountForSignature == referenceCountForSignature
					&& currentCountForSignature <= Occurrence::Capacity)
				{
					// The Soulcalibur name plates are submitted as two coincident
					// depth layers in sorted translucency. Pair this small, exact-profile
					// bucket one-to-one by depth; a changed or oversized bucket remains
					// untrusted rather than manufacturing continuity.
					auto currentIndices = currentOccurrence->second.indices;
					auto referenceIndices = referenceOccurrence->second.indices;
					const auto currentEnd = currentIndices.begin() + currentCountForSignature;
					const auto referenceEnd = referenceIndices.begin() + referenceCountForSignature;
					const auto currentLess = [this](std::size_t a, std::size_t b) {
						const auto& left = drawBuffers_[currentBuffer_][a];
						const auto& right = drawBuffers_[currentBuffer_][b];
						if (left.zMin != right.zMin) return left.zMin < right.zMin;
						if (left.zMax != right.zMax) return left.zMax < right.zMax;
						return left.ordinal < right.ordinal;
					};
					const auto referenceLess = [this](std::size_t a, std::size_t b) {
						const auto& left = drawBuffers_[referenceBuffer_][a];
						const auto& right = drawBuffers_[referenceBuffer_][b];
						if (left.zMin != right.zMin) return left.zMin < right.zMin;
						if (left.zMax != right.zMax) return left.zMax < right.zMax;
						return left.ordinal < right.ordinal;
					};
					std::sort(currentIndices.begin(), currentEnd, currentLess);
					std::sort(referenceIndices.begin(), referenceEnd, referenceLess);
					const auto position = std::find(currentIndices.begin(), currentEnd, ci);
					if (position != currentEnd)
						referenceIndex = referenceIndices[
							static_cast<std::size_t>(position - currentIndices.begin())];
				}
				if (referenceIndex != NeuralInstrumentation::MaxDraws)
				{
					const auto& previous = drawBuffers_[referenceBuffer_][referenceIndex];
					const bool stationary = std::abs(current.centroid[0] - previous.centroid[0]) <= .25f
						&& std::abs(current.centroid[1] - previous.centroid[1]) <= .25f
						&& current.bboxMin[0] == previous.bboxMin[0]
						&& current.bboxMin[1] == previous.bboxMin[1]
						&& current.bboxMax[0] == previous.bboxMax[0]
						&& current.bboxMax[1] == previous.bboxMax[1]
						&& std::abs(current.zMin - previous.zMin) <= .002f
						&& std::abs(current.zMax - previous.zMax) <= .002f;
					if (stationary)
						stability = static_cast<std::uint8_t>(std::min<int>(255,
							overlayStability_[referenceBuffer_][referenceIndex] + 1));
				}
			}
			overlayStability_[currentBuffer_][ci] = stability;
			const auto texture = textureOccurrences.find(current.texId);
			const std::uint16_t countedTextureUses = texture == textureOccurrences.end()
				? 0 : texture->second;
			const std::uint16_t textureUses = std::max<std::uint16_t>(
				current.screenAlignedPrimitiveCount, countedTextureUses);
			if (IsHighConfidenceOverlay(current, currentCount, renderWidth, renderHeight,
				stability, textureUses)
				|| IsTitleSpecificOverlay(current, currentCount, renderWidth, renderHeight,
					stability, overlayProfile_))
			{
				overlayBuffer_[ci] = 1;
				++overlayDrawCount_;
			}
		}
	}
	catch (...)
	{
		overlayBuffer_.fill(0);
		overlayStability_[currentBuffer_].fill(0);
		overlayDrawCount_ = 0;
	}
}

std::vector<OverlayDrawDiagnostic> NeuralInstrumentation::CaptureOverlayDiagnostics() const
{
	std::unordered_map<std::uint32_t, std::uint16_t> textureUses;
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		const auto& draw = drawBuffers_[currentBuffer_][i];
		if (draw.texId != 0 && draw.indexCount != 0 && (draw.flags & DrawDegenerate) == 0
			&& textureUses[draw.texId] != std::numeric_limits<std::uint16_t>::max())
			++textureUses[draw.texId];
	}
	std::vector<OverlayDrawDiagnostic> result;
	result.reserve(drawCounts_[currentBuffer_]);
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		const auto& draw = drawBuffers_[currentBuffer_][i];
		const auto counted = draw.texId == 0 ? std::uint16_t(0) : textureUses[draw.texId];
		result.push_back({draw, overlayStability_[currentBuffer_][i],
			std::max(draw.screenAlignedPrimitiveCount, counted), overlayBuffer_[i] != 0});
	}
	return result;
}

void NeuralInstrumentation::FinalizeConfidence() noexcept
{
	historyAge_ = hasAcceptedFrame_ ? static_cast<std::uint32_t>(std::min<std::uint64_t>(
		frameId_ - lastAcceptedFrameId_, std::numeric_limits<std::uint32_t>::max())) : 0;
	skippedFrameCount_ = historyAge_ > 0 ? historyAge_ - 1 : 0;
	sceneCut_ = false;
	correspondenceStats_ = {};
	correspondenceStats_.candidateDrawsBeforePositionValidation =
		candidateDrawsBeforePositionValidation_;
	correspondenceStats_.candidateTier1Draws = candidateTierDraws_[0];
	correspondenceStats_.candidateTier2Draws = candidateTierDraws_[1];
	correspondenceStats_.candidateTier3Draws = candidateTierDraws_[2];
	correspondenceStats_.candidateAreaBeforePositionValidation =
		candidateAreaBeforePositionValidation_;
	correspondenceStats_.trustedPreviousVertices = static_cast<std::uint32_t>(
		std::min<std::size_t>(trustedPreviousVertexCount_, std::numeric_limits<std::uint32_t>::max()));
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		const auto& draw = drawBuffers_[currentBuffer_][i];
		const auto& match = matchBuffer_[i];
		if (draw.list == ListType_Opaque) ++correspondenceStats_.opaqueDraws;
		else if (draw.list == ListType_Punch_Through) ++correspondenceStats_.punchThroughDraws;
		else if (draw.list == ListType_Translucent) ++correspondenceStats_.translucentDraws;
		if (match.reason == static_cast<std::uint8_t>(MatchReason::Reactive))
			++correspondenceStats_.reactiveDraws;
		else if (match.reason == static_cast<std::uint8_t>(MatchReason::Ambiguous))
			++correspondenceStats_.ambiguousDraws;
		else if (match.confidence == 0.f)
			++correspondenceStats_.unmatchedDraws;
		const auto width = std::max(0, static_cast<int>(draw.bboxMax[0]) - draw.bboxMin[0]);
		const auto height = std::max(0, static_cast<int>(draw.bboxMax[1]) - draw.bboxMin[1]);
		const auto area = static_cast<std::uint64_t>(width) * height;
		correspondenceStats_.totalAreaForSceneCut += area;
		if (draw.list != ListType_Translucent && !IsReactive(draw))
			correspondenceStats_.totalOpaqueAreaForSceneCut += area;
		if (match.confidence >= .5f)
		{
			++correspondenceStats_.trustedDrawsBeforeSceneCut;
			correspondenceStats_.matchedAreaBeforeSceneCut += area;
			if (draw.list != ListType_Translucent && !IsReactive(draw))
				correspondenceStats_.matchedOpaqueAreaBeforeSceneCut += area;
		}
	}
	if (resetPending_ || truncated_ || drawCounts_[referenceBuffer_] == 0)
		return;
	if (IsSceneCut(correspondenceStats_.matchedAreaBeforeSceneCut,
		correspondenceStats_.totalAreaForSceneCut))
	{
		sceneCut_ = true;
		Discontinuity();
		for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
			matchBuffer_[i].confidence = 0.f;
		for (auto& position : previousPositions_) position.valid = 0.f;
		trustedPreviousVertexCount_ = 0;
		return;
	}
	const float ageFactor = skippedFrameCount_ == 0 ? 1.f
		: skippedFrameCount_ == 1 ? .8f
		: skippedFrameCount_ == 2 ? .6f : 0.f;
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		auto& confidence = matchBuffer_[i].confidence;
		confidence *= ageFactor;
		if (confidence < .5f) confidence = 0.f;
	}
	// BuildPreviousPositions runs before final age/scene evidence is known. Keep
	// its diagnostic vertex count aligned with what the production shader can
	// actually trust after that final confidence decision.
	try
	{
		std::vector<std::uint8_t> trusted(previousPositions_.size(), 0);
		const auto& currentIndices = indexBuffers_[currentBuffer_];
		for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
		{
			if (matchBuffer_[i].confidence < .5f) continue;
			const auto& draw = drawBuffers_[currentBuffer_][i];
			for (std::uint32_t j = 0; j < draw.indexCount; ++j)
			{
				const auto offset = static_cast<std::size_t>(draw.firstIndex) + j;
				if (offset >= currentIndices.size()) break;
				const auto vertex = currentIndices[offset];
				if (vertex < previousPositions_.size()
					&& previousPositions_[vertex].valid == 1.f) trusted[vertex] = 1;
			}
		}
		trustedPreviousVertexCount_ = 0;
		for (std::size_t i = 0; i < previousPositions_.size(); ++i)
		{
			if (!trusted[i]) previousPositions_[i].valid = 0.f;
			else ++trustedPreviousVertexCount_;
		}
	}
	catch (...)
	{
		for (auto& position : previousPositions_) position.valid = 0.f;
		trustedPreviousVertexCount_ = 0;
		for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
			matchBuffer_[i].confidence = 0.f;
	}
}

bool NeuralInstrumentation::CapturePositionSnapshot(const rend_context& context) noexcept
{
	if (context.verts.size() > MaxHistoryVertices || context.idx.size() > MaxHistoryIndices)
		return false;
	try
	{
		auto& positions = positionBuffers_[currentBuffer_];
		positions.resize(context.verts.size());
		for (std::size_t i = 0; i < context.verts.size(); ++i)
		{
			positions[i].x = context.verts[i].x;
			positions[i].y = context.verts[i].y;
			positions[i].z = context.verts[i].z;
			positions[i].valid = 0.f;
		}
		indexBuffers_[currentBuffer_].assign(context.idx.begin(), context.idx.end());
		return true;
	}
	catch (...)
	{
		positionBuffers_[currentBuffer_].clear();
		indexBuffers_[currentBuffer_].clear();
		return false;
	}
}

void NeuralInstrumentation::CaptureNaomi2Transforms(const rend_context& context) noexcept
{
	auto& transforms = naomi2TransformBuffers_[currentBuffer_];
	transforms.fill({});
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		const auto& draw = drawBuffers_[currentBuffer_][i];
		if ((draw.flags & DrawNaomi2) == 0 || draw.n2Mv < 0 || draw.n2Proj < 0)
			continue;
		const auto mv = static_cast<std::size_t>(draw.n2Mv);
		const auto projection = static_cast<std::size_t>(draw.n2Proj);
		if (mv >= context.matrices.size() || projection >= context.matrices.size())
			continue;
		auto& transform = transforms[i];
		std::copy(std::begin(context.matrices[mv].mat),
			std::end(context.matrices[mv].mat), transform.modelView.begin());
		std::copy(std::begin(context.matrices[projection].mat),
			std::end(context.matrices[projection].mat), transform.projection.begin());
		transform.valid = true;
	}
}

const Naomi2Transform *NeuralInstrumentation::PreviousNaomi2TransformForOrdinal(
	std::size_t ordinal) const noexcept
{
	if (ordinal >= drawCounts_[currentBuffer_]) return nullptr;
	const auto& draw = drawBuffers_[currentBuffer_][ordinal];
	const auto& match = matchBuffer_[ordinal];
	if ((draw.flags & DrawNaomi2) == 0 || match.confidence < .5f
		|| match.prevOrdinal >= drawCounts_[referenceBuffer_])
		return nullptr;
	const auto& transform = naomi2TransformBuffers_[referenceBuffer_][match.prevOrdinal];
	return transform.valid ? &transform : nullptr;
}

void NeuralInstrumentation::BuildPreviousPositions(const rend_context& context) noexcept
{
	trustedPreviousVertexCount_ = 0;
	try
	{
		previousPositions_.resize(context.verts.size());
	}
	catch (...)
	{
		previousPositions_.clear();
		truncated_ = true;
		Discontinuity();
		return;
	}
	for (std::size_t i = 0; i < context.verts.size(); ++i)
	{
		previousPositions_[i].x = context.verts[i].x;
		previousPositions_[i].y = context.verts[i].y;
		previousPositions_[i].z = context.verts[i].z;
		previousPositions_[i].valid = 0.f;
	}
	if (resetPending_ || truncated_ || drawCounts_[referenceBuffer_] == 0)
		return;
	const auto& previousVertices = positionBuffers_[referenceBuffer_];
	const auto& previousIndices = indexBuffers_[referenceBuffer_];
	const auto& currentIndices = indexBuffers_[currentBuffer_];
	for (std::size_t ci = 0; ci < drawCounts_[currentBuffer_]; ++ci)
	{
		const auto& currentDraw = drawBuffers_[currentBuffer_][ci];
		auto& match = matchBuffer_[ci];
		if (match.tier == 0 || match.confidence < .5f ||
			match.prevOrdinal >= drawCounts_[referenceBuffer_])
			continue;
		const auto& previousDraw = drawBuffers_[referenceBuffer_][match.prevOrdinal];
		const bool naomi2 = (currentDraw.flags & DrawNaomi2) != 0;
		if (naomi2 && ((previousDraw.flags & DrawNaomi2) == 0
			|| !naomi2TransformBuffers_[currentBuffer_][ci].valid
			|| !naomi2TransformBuffers_[referenceBuffer_][match.prevOrdinal].valid))
		{
			match.confidence = 0.f;
			continue;
		}
		auto writeCandidate = [&](std::uint32_t currentVertex,
			const PreviousPosition& candidate) {
			if (currentVertex >= previousPositions_.size()) return;
			auto& output = previousPositions_[currentVertex];
			if (output.valid < 0.f) return;
			if (output.valid > 0.f && (output.x != candidate.x || output.y != candidate.y
				|| output.z != candidate.z))
			{
				output.valid = -1.f;
				--trustedPreviousVertexCount_;
				return;
			}
			if (output.valid == 0.f)
			{
				output = candidate;
				output.valid = 1.f;
				++trustedPreviousVertexCount_;
			}
		};
		if (currentDraw.topologySig == previousDraw.topologySig
			&& currentDraw.indexCount == previousDraw.indexCount)
		{
			for (std::uint32_t i = 0; i < currentDraw.indexCount; ++i)
			{
				const std::size_t currentOffset = static_cast<std::size_t>(currentDraw.firstIndex) + i;
				const std::size_t previousOffset = static_cast<std::size_t>(previousDraw.firstIndex) + i;
				if (currentOffset >= currentIndices.size() || previousOffset >= previousIndices.size())
					break;
				const auto currentVertex = currentIndices[currentOffset];
				const auto previousVertex = previousIndices[previousOffset];
				if (previousVertex >= previousVertices.size()) continue;
				writeCandidate(currentVertex, previousVertices[previousVertex]);
			}
			continue;
		}
		// A reindexed Naomi 2 draw would require proving correspondence in object
		// space independently of its per-draw matrix history. Reject it until that
		// stronger evidence exists rather than fitting projected motion.
		if (naomi2)
		{
			match.confidence = 0.f;
			continue;
		}
		// Reindexed geometry is accepted only when stable local vertex ordinals
		// prove a low-residual similarity transform to the accepted pose.
		if (currentDraw.vertexCount != previousDraw.vertexCount
			|| currentDraw.vertexCount < 2) continue;
		double currentX = 0., currentY = 0., previousX = 0., previousY = 0.;
		std::uint32_t sampleCount = 0;
		auto pairedVertex = [&](std::uint32_t currentVertex,
			const PreviousPosition *&previous) {
			if (currentVertex < currentDraw.firstVertex) return false;
			const auto local = currentVertex - currentDraw.firstVertex;
			const auto previousVertex = static_cast<std::size_t>(previousDraw.firstVertex) + local;
			if (local >= currentDraw.vertexCount || currentVertex >= context.verts.size()
				|| previousVertex >= previousVertices.size()) return false;
			previous = &previousVertices[previousVertex];
			return true;
		};
		for (std::uint32_t i = 0; i < currentDraw.indexCount; ++i)
		{
			const auto offset = static_cast<std::size_t>(currentDraw.firstIndex) + i;
			if (offset >= currentIndices.size()) break;
			const auto currentVertex = currentIndices[offset];
			const PreviousPosition *previous = nullptr;
			if (!pairedVertex(currentVertex, previous)) continue;
			currentX += context.verts[currentVertex].x;
			currentY += context.verts[currentVertex].y;
			previousX += previous->x;
			previousY += previous->y;
			++sampleCount;
		}
		if (sampleCount < 2) continue;
		const double inverseCount = 1. / sampleCount;
		currentX *= inverseCount; currentY *= inverseCount;
		previousX *= inverseCount; previousY *= inverseCount;
		double dot = 0., cross = 0., norm = 0.;
		for (std::uint32_t i = 0; i < currentDraw.indexCount; ++i)
		{
			const auto offset = static_cast<std::size_t>(currentDraw.firstIndex) + i;
			if (offset >= currentIndices.size()) break;
			const auto currentVertex = currentIndices[offset];
			const PreviousPosition *previous = nullptr;
			if (!pairedVertex(currentVertex, previous)) continue;
			const double cx = context.verts[currentVertex].x - currentX;
			const double cy = context.verts[currentVertex].y - currentY;
			const double px = previous->x - previousX;
			const double py = previous->y - previousY;
			dot += cx * px + cy * py;
			cross += cx * py - cy * px;
			norm += cx * cx + cy * cy;
		}
		if (norm <= std::numeric_limits<double>::epsilon()) continue;
		const double scaleCos = dot / norm;
		const double scaleSin = cross / norm;
		const double translateX = previousX - scaleCos * currentX + scaleSin * currentY;
		const double translateY = previousY - scaleSin * currentX - scaleCos * currentY;
		const double scale = std::sqrt(scaleCos * scaleCos + scaleSin * scaleSin);
		double squaredResidual = 0.;
		for (std::uint32_t i = 0; i < currentDraw.indexCount; ++i)
		{
			const auto offset = static_cast<std::size_t>(currentDraw.firstIndex) + i;
			if (offset >= currentIndices.size()) break;
			const auto currentVertex = currentIndices[offset];
			const PreviousPosition *previous = nullptr;
			if (!pairedVertex(currentVertex, previous)) continue;
			const double fitX = scaleCos * context.verts[currentVertex].x
				- scaleSin * context.verts[currentVertex].y + translateX;
			const double fitY = scaleSin * context.verts[currentVertex].x
				+ scaleCos * context.verts[currentVertex].y + translateY;
			const double dx = fitX - previous->x;
			const double dy = fitY - previous->y;
			squaredResidual += dx * dx + dy * dy;
		}
		match.fitResidual = static_cast<float>(std::sqrt(squaredResidual * inverseCount));
		if (match.fitResidual > .25f || scale < .5 || scale > 2.)
		{
			match.confidence = 0.f;
			continue;
		}
		match.rigid[0] = static_cast<float>(scaleCos);
		match.rigid[1] = static_cast<float>(scaleSin);
		match.rigid[2] = static_cast<float>(translateX);
		match.rigid[3] = static_cast<float>(translateY);
		match.confidence *= std::max(.65f, 1.f - match.fitResidual / .25f);
		for (std::uint32_t i = 0; i < currentDraw.indexCount; ++i)
		{
			const auto offset = static_cast<std::size_t>(currentDraw.firstIndex) + i;
			if (offset >= currentIndices.size()) break;
			const auto currentVertex = currentIndices[offset];
			const PreviousPosition *previous = nullptr;
			if (!pairedVertex(currentVertex, previous)) continue;
			PreviousPosition candidate{};
			candidate.x = match.rigid[0] * context.verts[currentVertex].x
				- match.rigid[1] * context.verts[currentVertex].y + match.rigid[2];
			candidate.y = match.rigid[1] * context.verts[currentVertex].x
				+ match.rigid[0] * context.verts[currentVertex].y + match.rigid[3];
			candidate.z = previous->z;
			candidate.valid = 1.f;
			writeCandidate(currentVertex, candidate);
		}
	}
	for (auto& position : previousPositions_)
		if (position.valid < 0.f) position.valid = 0.f;
	// A pixel cannot safely interpolate motion from a partly invalid primitive.
	// Downgrade the whole draw after shared-vertex conflicts have been resolved.
	for (std::size_t ci = 0; ci < drawCounts_[currentBuffer_]; ++ci)
	{
		auto& match = matchBuffer_[ci];
		if (match.confidence < .5f) continue;
		const auto& draw = drawBuffers_[currentBuffer_][ci];
		bool allPositionsTrusted = true;
		for (std::uint32_t i = 0; i < draw.indexCount; ++i)
		{
			const std::size_t offset = static_cast<std::size_t>(draw.firstIndex) + i;
			if (offset < currentIndices.size() && currentIndices[offset] == ~std::uint32_t{0})
				continue;
			if (offset >= currentIndices.size() || currentIndices[offset] >= previousPositions_.size()
				|| previousPositions_[currentIndices[offset]].valid != 1.f)
			{
				allPositionsTrusted = false;
				break;
			}
		}
		if (!allPositionsTrusted) match.confidence = 0.f;
	}
}

void NeuralInstrumentation::Discontinuity() noexcept
{
	++historyGeneration_;
	acceptedEvaluationCount_ = 0;
	resetPending_ = true;
}

void NeuralInstrumentation::BeginSource(FrameSource source) noexcept
{
	if (hasSource_ && source != lastSource_)
		Discontinuity();
	lastSource_ = source;
	hasSource_ = true;
}

const NeuralFrame& NeuralInstrumentation::CaptureGeometry(const rend_context& context,
	TextureRef color, TextureRef depth, std::uint32_t renderWidth, std::uint32_t renderHeight,
	std::uint32_t outputWidth, std::uint32_t outputHeight, Rect contentRect, Point2 jitter) noexcept
{
	BeginSource(FrameSource::Geometry);
	const auto screenWidth = static_cast<std::uint32_t>(context.globClip.x > 0
		? context.globClip.x : renderWidth);
	const auto screenHeight = static_cast<std::uint32_t>(context.globClip.y > 0
		? context.globClip.y : renderHeight);
	auto& current = drawBuffers_[currentBuffer_];
	drawCounts_[currentBuffer_] = 0;
	truncated_ = false;
	AppendList(context, context.global_param_op, ListType_Opaque, current,
		drawCounts_[currentBuffer_], truncated_);
	AppendList(context, context.global_param_pt, ListType_Punch_Through, current,
		drawCounts_[currentBuffer_], truncated_);
	AppendList(context, context.global_param_tr, ListType_Translucent, current,
		drawCounts_[currentBuffer_], truncated_);
	AppendSortedTranslucent(context, current, drawCounts_[currentBuffer_], truncated_);
	CaptureNaomi2Transforms(context);
	ClassifyOverlays(screenWidth, screenHeight);
	if (!CapturePositionSnapshot(context))
		truncated_ = true;
	if (truncated_) Discontinuity();
	MatchDrawsInto({drawBuffers_[referenceBuffer_].data(), drawCounts_[referenceBuffer_]},
		{current.data(), drawCounts_[currentBuffer_]}, matchBuffer_.data(), matchBuffer_.size());
	candidateDrawsBeforePositionValidation_ = 0;
	candidateTierDraws_[0] = candidateTierDraws_[1] = candidateTierDraws_[2] = 0;
	candidateAreaBeforePositionValidation_ = 0;
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		const auto& match = matchBuffer_[i];
		if (match.confidence < .5f) continue;
		++candidateDrawsBeforePositionValidation_;
		if (match.tier >= 1 && match.tier <= 3)
			++candidateTierDraws_[match.tier - 1];
		const auto& draw = current[i];
		const auto width = std::max(0, static_cast<int>(draw.bboxMax[0]) - draw.bboxMin[0]);
		const auto height = std::max(0, static_cast<int>(draw.bboxMax[1]) - draw.bboxMin[1]);
		candidateAreaBeforePositionValidation_ += static_cast<std::uint64_t>(width) * height;
	}
	BuildPreviousPositions(context);
	FinalizeConfidence();
	drawSnapshotHash_ = 1469598103934665603ull;
	for (std::size_t i = 0; i < drawCounts_[currentBuffer_]; ++i)
	{
		drawSnapshotHash_ ^= DrawSignature(current[i]);
		drawSnapshotHash_ *= 1099511628211ull;
	}
	frame_ = {};
	frame_.color = color;
	frame_.depth = depth;
	frame_.renderWidth = renderWidth;
	frame_.renderHeight = renderHeight;
	frame_.outputWidth = outputWidth;
	frame_.outputHeight = outputHeight;
	frame_.screenWidth = screenWidth;
	frame_.screenHeight = screenHeight;
	frame_.contentRect = contentRect;
	frame_.jitterX = jitter.x;
	frame_.jitterY = jitter.y;
	frame_.frameId = frameId_++;
	capturedFrameId_ = frame_.frameId;
	hasCapturedFrame_ = true;
	frame_.historyGeneration = historyGeneration_;
	frame_.historyAge = historyAge_;
	frame_.skippedFrameCount = skippedFrameCount_;
	frame_.historyValid = drawCounts_[referenceBuffer_] != 0 && !resetPending_;
	frame_.resetHistory = resetPending_;
	frame_.sceneCut = sceneCut_;
	frame_.truncated = truncated_;
	frame_.predominantly2D = IsPredominantly2DFrame(
		{current.data(), drawCounts_[currentBuffer_]}, screenWidth, screenHeight);
	frame_.overlayProfile = overlayProfile_;
	frame_.source = FrameSource::Geometry;
	frame_.draws = {current.data(), drawCounts_[currentBuffer_]};
	frame_.matches = {matchBuffer_.data(), drawCounts_[currentBuffer_]};
	frame_.correspondence = correspondenceStats_;
	return frame_;
}

const NeuralFrame& NeuralInstrumentation::CaptureSource(FrameSource source, TextureRef color,
	std::uint32_t renderWidth, std::uint32_t renderHeight,
	std::uint32_t outputWidth, std::uint32_t outputHeight, Rect contentRect) noexcept
{
	BeginSource(source);
	previousPositions_.clear();
	trustedPreviousVertexCount_ = 0;
	frame_ = {};
	frame_.color = color;
	frame_.renderWidth = renderWidth;
	frame_.renderHeight = renderHeight;
	frame_.outputWidth = outputWidth;
	frame_.outputHeight = outputHeight;
	frame_.contentRect = contentRect;
	frame_.frameId = frameId_++;
	capturedFrameId_ = frame_.frameId;
	hasCapturedFrame_ = true;
	frame_.historyGeneration = historyGeneration_;
	frame_.historyAge = 0;
	frame_.skippedFrameCount = 0;
	frame_.resetHistory = true;
	frame_.source = source;
	return frame_;
}

const NeuralFrame& NeuralInstrumentation::AttachTextures(TextureRef color, TextureRef depth,
	TextureRef motion, TextureRef mask, TextureRef confidence, TextureRef drawId) noexcept
{
	frame_.color = color;
	frame_.depth = depth;
	frame_.motion = motion;
	frame_.mask = mask;
	frame_.confidence = confidence;
	frame_.drawId = drawId;
	return frame_;
}

void NeuralInstrumentation::MarkEvaluated(std::uint64_t frameId) noexcept
{
	if (!hasCapturedFrame_ || capturedFrameId_ != frameId || frame_.source != FrameSource::Geometry)
		return;
	std::swap(referenceBuffer_, currentBuffer_);
	resetPending_ = false;
	lastAcceptedFrameId_ = frameId;
	hasAcceptedFrame_ = true;
	++acceptedEvaluationCount_;
}

} // namespace flycast::rend::neural
