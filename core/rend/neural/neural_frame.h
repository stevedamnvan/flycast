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
	std::uint32_t firstVertex = 0;
	std::uint32_t vertexCount = 0;
	std::uint32_t firstIndex = 0;
	std::uint32_t indexCount = 0;
	std::uint16_t stripCount = 0;
	std::uint32_t uvSig = 0;
	std::uint32_t geomSig = 0;
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
	std::uint8_t tier = 0;
	std::uint8_t reason = 0;
	float rigid[4]{};
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
	bool historyValid = false;
	bool resetHistory = false;
	bool truncated = false;
	FrameSource source = FrameSource::Geometry;
	ArrayView<DrawRecord> draws;
	ArrayView<DrawMatch> matches;
};

} // namespace flycast::rend::neural
