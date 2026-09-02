// SPDX-License-Identifier: GPL-2.0-or-later
#include "instrumentation.h"

#include "hw/pvr/ta_ctx.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
	std::uint16_t list, std::uint32_t listIndex, std::uint16_t ordinal) noexcept
{
	DrawRecord record{};
	record.list = list;
	record.pass = static_cast<std::uint16_t>(PassFor(context, listIndex, list));
	std::uint64_t state = 1469598103934665603ull;
	Mix(state, poly.pcw.full & 0x300ceu);
	Mix(state, poly.isp.full & 0xf4000000u);
	Mix(state, poly.tsp.full);
	Mix(state, poly.tcw.full);
	Mix(state, poly.tileclip);
	Mix(state, poly.tsp1.full);
	Mix(state, poly.tcw1.full);
	Mix(state, static_cast<std::uint32_t>(poly.mvMatrix));
	Mix(state, static_cast<std::uint32_t>(poly.projMatrix));
	record.stateSig = Fold(state);
	record.texId = poly.tcw.full;
	record.firstIndex = poly.first;
	record.indexCount = poly.count;
	record.stripCount = poly.count == 0 ? 0 : 1;
	record.ordinal = ordinal;
	record.blend = static_cast<std::uint8_t>((poly.tsp.SrcInstr << 3) | poly.tsp.DstInstr);
	record.n2Mv = static_cast<std::int16_t>(poly.mvMatrix);
	record.n2Proj = static_cast<std::int16_t>(poly.projMatrix);
	if (poly.isNaomi2()) record.flags |= DrawNaomi2;
	if (list == ListType_Translucent && poly.count <= 6) record.flags |= DrawReactive;

	std::uint64_t uvHash = 1469598103934665603ull;
	std::uint64_t geomHash = 1469598103934665603ull;
	float zMin = std::numeric_limits<float>::infinity();
	float zMax = -std::numeric_limits<float>::infinity();
	float xMin = std::numeric_limits<float>::infinity();
	float yMin = std::numeric_limits<float>::infinity();
	float xMax = -std::numeric_limits<float>::infinity();
	float yMax = -std::numeric_limits<float>::infinity();
	std::uint32_t firstVertex = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t lastVertex = 0;
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
		Mix(geomHash, QuantizeFloat(vertex.x, 16.f));
		Mix(geomHash, QuantizeFloat(vertex.y, 16.f));
		Mix(geomHash, QuantizeFloat(vertex.z, 4096.f));
	}
	if (firstVertex == std::numeric_limits<std::uint32_t>::max())
	{
		record.flags |= DrawDegenerate;
		firstVertex = 0;
		zMin = zMax = xMin = yMin = xMax = yMax = 0.f;
	}
	record.firstVertex = firstVertex;
	record.vertexCount = lastVertex >= firstVertex ? lastVertex - firstVertex + 1 : 0;
	record.uvSig = Fold(uvHash);
	Mix(geomHash, QuantizeFloat((xMin + xMax) * .5f, 16.f));
	Mix(geomHash, QuantizeFloat((yMin + yMax) * .5f, 16.f));
	record.geomSig = Fold(geomHash);
	record.zMin = zMin;
	record.zMax = zMax;
	record.bboxMin[0] = QuantizeCoord(xMin);
	record.bboxMin[1] = QuantizeCoord(yMin);
	record.bboxMax[0] = QuantizeCoord(xMax);
	record.bboxMax[1] = QuantizeCoord(yMax);
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
		output[count++] = MakeRecord(context, polys[i], list, i, ordinal);
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
		frame_ = {};
	}
}

void NeuralInstrumentation::Discontinuity() noexcept
{
	++historyGeneration_;
	resetPending_ = true;
}

const NeuralFrame& NeuralInstrumentation::CaptureGeometry(const rend_context& context,
	TextureRef color, TextureRef depth, std::uint32_t renderWidth, std::uint32_t renderHeight,
	std::uint32_t outputWidth, std::uint32_t outputHeight, Rect contentRect, Point2 jitter) noexcept
{
	auto& current = drawBuffers_[currentBuffer_];
	const std::uint32_t previousBuffer = currentBuffer_ ^ 1u;
	drawCounts_[currentBuffer_] = 0;
	truncated_ = false;
	AppendList(context, context.global_param_op, ListType_Opaque, current,
		drawCounts_[currentBuffer_], truncated_);
	AppendList(context, context.global_param_pt, ListType_Punch_Through, current,
		drawCounts_[currentBuffer_], truncated_);
	AppendList(context, context.global_param_tr, ListType_Translucent, current,
		drawCounts_[currentBuffer_], truncated_);
	if (truncated_) Discontinuity();
	MatchDrawsInto({drawBuffers_[previousBuffer].data(), drawCounts_[previousBuffer]},
		{current.data(), drawCounts_[currentBuffer_]}, matchBuffer_.data(), matchBuffer_.size());
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
	frame_.contentRect = contentRect;
	frame_.jitterX = jitter.x;
	frame_.jitterY = jitter.y;
	frame_.frameId = frameId_++;
	frame_.historyGeneration = historyGeneration_;
	frame_.historyValid = drawCounts_[previousBuffer] != 0 && !resetPending_;
	frame_.resetHistory = resetPending_;
	frame_.truncated = truncated_;
	frame_.source = FrameSource::Geometry;
	frame_.draws = {current.data(), drawCounts_[currentBuffer_]};
	frame_.matches = {matchBuffer_.data(), drawCounts_[currentBuffer_]};
	resetPending_ = false;
	currentBuffer_ = previousBuffer;
	return frame_;
}

} // namespace flycast::rend::neural
