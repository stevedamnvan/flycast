// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "version.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace neuraltest {

bool WritePng(const std::filesystem::path& path, const Image& image, std::string& error)
{
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec)
	{
		error = "cannot create output directory: " + ec.message();
		return false;
	}
	if (image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4)
	{
		error = "invalid RGBA image size";
		return false;
	}
	if (!stbi_write_png(path.string().c_str(), static_cast<int>(image.width),
		static_cast<int>(image.height), 4, image.rgba.data(), static_cast<int>(image.width * 4)))
	{
		error = "stbi_write_png failed for " + path.string();
		return false;
	}
	return true;
}

bool WriteRaw(const std::filesystem::path& path, const Image& image, std::string& error)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
	{
		error = "cannot open raw output: " + path.string();
		return false;
	}
	out.write(reinterpret_cast<const char *>(image.rgba.data()),
		static_cast<std::streamsize>(image.rgba.size()));
	if (!out)
	{
		error = "failed writing raw output: " + path.string();
		return false;
	}
	return true;
}

bool ReadPng(const std::filesystem::path& path, Image& image, std::string& error)
{
	int width = 0;
	int height = 0;
	int components = 0;
	stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height, &components, 4);
	if (!pixels)
	{
		error = "cannot read PNG " + path.string() + ": " + stbi_failure_reason();
		return false;
	}
	image.width = static_cast<std::uint32_t>(width);
	image.height = static_cast<std::uint32_t>(height);
	image.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
	stbi_image_free(pixels);
	return true;
}

double ComputePsnr(const Image& a, const Image& b, std::uint32_t& differingPixels,
	std::uint8_t& maxDelta)
{
	differingPixels = 0;
	maxDelta = 0;
	if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size())
		return 0.;
	double squaredError = 0.;
	for (std::size_t pixel = 0; pixel < a.rgba.size() / 4; ++pixel)
	{
		bool differs = false;
		for (std::size_t channel = 0; channel < 4; ++channel)
		{
			const auto i = pixel * 4 + channel;
			const auto delta = static_cast<std::uint8_t>(std::abs(
				static_cast<int>(a.rgba[i]) - static_cast<int>(b.rgba[i])));
			maxDelta = std::max(maxDelta, delta);
			differs = differs || delta != 0;
			squaredError += static_cast<double>(delta) * delta;
		}
		differingPixels += differs ? 1u : 0u;
	}
	if (squaredError == 0.)
		return std::numeric_limits<double>::infinity();
	const double mse = squaredError / static_cast<double>(a.rgba.size());
	return 10. * std::log10((255. * 255.) / mse);
}

bool WriteRenderPackage(const std::filesystem::path& root, const Fixture& fixture,
	const RenderOptions& options, const RenderResult& result, std::string& error)
{
	if (!WritePng(root / "color.png", result.color, error) ||
		!WriteRaw(root / "color.raw", result.color, error))
		return false;
	std::ofstream manifest(root / "manifest.json");
	if (!manifest)
	{
		error = "cannot create manifest.json";
		return false;
	}
	manifest << "{\n"
		<< "  \"schema\": 1,\n"
		<< "  \"git_sha\": \"" << GIT_HASH << "\",\n"
		<< "  \"fixture\": \"" << fixture.name << "\",\n"
		<< "  \"frame\": " << options.frame << ",\n"
		<< "  \"renderer\": \"" << options.renderer << "\",\n"
		<< "  \"driver\": \"" << result.driver << "\",\n"
		<< "  \"scale\": " << options.scale << ",\n"
		<< "  \"width\": " << result.color.width << ",\n"
		<< "  \"height\": " << result.color.height << ",\n"
		<< "  \"output\": \"" << result.color.width << 'x' << result.color.height << "\",\n"
		<< "  \"mode\": \"native\",\n"
		<< "  \"jitter\": " << (options.jitter ? "true" : "false") << ",\n"
		<< "  \"history_valid\": false,\n"
		<< "  \"history_generation\": 0,\n"
		<< "  \"config_hash\": \"0x" << std::hex << result.hash << "\",\n"
		<< "  \"adapter\": \"" << result.adapter << "\",\n"
		<< "  \"color_fnv1a64\": \"0x" << std::hex << std::setw(16)
		<< std::setfill('0') << result.hash << "\",\n"
		<< "  \"production_renderer\": false\n"
		<< "}\n";
	std::ofstream report(root / "report.md");
	report << "# neuraltest render report\n\n"
		<< "- Fixture: `" << fixture.name << "`\n"
		<< "- Requested renderer: `" << options.renderer << "`\n"
		<< "- Driver: `" << result.driver << "`\n"
		<< "- Adapter: `" << result.adapter << "`\n"
		<< "- Resolution: " << result.color.width << "x" << result.color.height << "\n"
		<< "- Color hash: `0x" << std::hex << result.hash << "`\n"
		<< "- Depth: no data (production instrumentation not implemented)\n"
		<< "- Motion: no data (production instrumentation not implemented)\n\n"
		<< "This artifact was rasterized by the ROM-free test-only D3D11 driver. It does not establish "
			"parity with Flycast's production DX11 or DX11/OIT renderer.\n";
	return static_cast<bool>(report);
}

} // namespace neuraltest
