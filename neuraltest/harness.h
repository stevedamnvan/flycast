// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace neuraltest {

struct Vertex {
	float x;
	float y;
	float z;
	float r;
	float g;
	float b;
	float a;
};

struct Fixture {
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
	bool dynamic = false;
	bool hasDepthGroundTruth = false;
	bool hasMotionGroundTruth = false;
};

struct Image {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> rgba;
};

struct RenderOptions {
	std::string renderer = "dx11";
	std::uint32_t scale = 1;
	std::uint32_t frame = 0;
	bool jitter = false;
	bool warp = false;
};

struct RenderResult {
	Image color;
	std::string adapter;
	std::string driver = "test-only-d3d11";
	std::uint64_t hash = 0;
};

const std::vector<std::string>& FixtureNames();
bool MakeFixture(const std::string& name, std::uint32_t frame, Fixture& fixture, std::string& error);
bool RenderFixture(const Fixture& fixture, const RenderOptions& options, RenderResult& result, std::string& error);
bool WritePng(const std::filesystem::path& path, const Image& image, std::string& error);
bool WriteRaw(const std::filesystem::path& path, const Image& image, std::string& error);
bool ReadPng(const std::filesystem::path& path, Image& image, std::string& error);
std::uint64_t HashImage(const Image& image);
double ComputePsnr(const Image& a, const Image& b, std::uint32_t& differingPixels, std::uint8_t& maxDelta);
bool WriteRenderPackage(const std::filesystem::path& root, const Fixture& fixture,
	const RenderOptions& options, const RenderResult& result, std::string& error);

} // namespace neuraltest
