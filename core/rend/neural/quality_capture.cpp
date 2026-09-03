// SPDX-License-Identifier: GPL-2.0-or-later
#include "quality_capture.h"
#include "version.h"

#include <stb/stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace flycast::rend::neural {
namespace {

struct RawTexture {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	std::uint32_t bytesPerPixel = 0;
	std::vector<std::uint8_t> bytes;
};

std::uint32_t BytesPerPixel(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8_UNORM: return 1;
	case DXGI_FORMAT_R16_UINT: return 2;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM: return 4;
	default: return 0;
	}
}

bool ReadTexture(ID3D11Device *device, ID3D11DeviceContext *context,
	ID3D11Texture2D *source, RawTexture& out, std::string& error)
{
	if (!source)
		return false;
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	out.bytesPerPixel = BytesPerPixel(desc.Format);
	if (out.bytesPerPixel == 0 || desc.SampleDesc.Count != 1 || desc.ArraySize != 1)
	{
		error = "unsupported capture texture format " + std::to_string(desc.Format);
		return false;
	}
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;
	ID3D11Texture2D *staging = nullptr;
	HRESULT result = device->CreateTexture2D(&desc, nullptr, &staging);
	if (FAILED(result))
	{
		error = "capture staging texture creation failed";
		return false;
	}
	context->CopyResource(staging, source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	result = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(result))
	{
		staging->Release();
		error = "capture staging texture map failed";
		return false;
	}
	out.width = desc.Width;
	out.height = desc.Height;
	out.format = desc.Format;
	const std::size_t rowBytes = static_cast<std::size_t>(desc.Width) * out.bytesPerPixel;
	out.bytes.resize(rowBytes * desc.Height);
	for (std::uint32_t y = 0; y < desc.Height; ++y)
		std::memcpy(out.bytes.data() + y * rowBytes,
			static_cast<const std::uint8_t *>(mapped.pData) + y * mapped.RowPitch, rowBytes);
	context->Unmap(staging, 0);
	staging->Release();
	return true;
}

float HalfToFloat(std::uint16_t value)
{
	const std::uint32_t sign = static_cast<std::uint32_t>(value & 0x8000u) << 16;
	std::uint32_t exponent = (value >> 10) & 0x1fu;
	std::uint32_t mantissa = value & 0x3ffu;
	std::uint32_t bits;
	if (exponent == 0)
	{
		if (mantissa == 0)
			bits = sign;
		else
		{
			exponent = 127 - 15 + 1;
			while ((mantissa & 0x400u) == 0) { mantissa <<= 1; --exponent; }
			bits = sign | (exponent << 23) | ((mantissa & 0x3ffu) << 13);
		}
	}
	else if (exponent == 31)
		bits = sign | 0x7f800000u | (mantissa << 13);
	else
		bits = sign | ((exponent + (127 - 15)) << 23) | (mantissa << 13);
	float result;
	std::memcpy(&result, &bits, sizeof(result));
	return result;
}

QualityCaptureWriter::RgbaImage ToRgba(const RawTexture& raw)
{
	QualityCaptureWriter::RgbaImage image;
	image.width = raw.width;
	image.height = raw.height;
	image.pixels.resize(static_cast<std::size_t>(raw.width) * raw.height * 4, 255);
	for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(raw.width) * raw.height; ++pixel)
	{
		auto *dst = image.pixels.data() + pixel * 4;
		const auto *src = raw.bytes.data() + pixel * raw.bytesPerPixel;
		switch (raw.format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			std::memcpy(dst, src, 4);
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
			break;
		case DXGI_FORMAT_R8_UNORM:
			dst[0] = dst[1] = dst[2] = src[0];
			break;
		case DXGI_FORMAT_R16_UINT:
		{
			std::uint16_t id;
			std::memcpy(&id, src, sizeof(id));
			dst[0] = static_cast<std::uint8_t>((id * 73u) & 255u);
			dst[1] = static_cast<std::uint8_t>((id * 151u) & 255u);
			dst[2] = static_cast<std::uint8_t>((id * 199u) & 255u);
			if (id == 0) dst[0] = dst[1] = dst[2] = 0;
			break;
		}
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_TYPELESS:
		{
			float depth;
			std::memcpy(&depth, src, sizeof(depth));
			const auto value = static_cast<std::uint8_t>(std::lround(
				std::clamp(std::isfinite(depth) ? depth : 0.f, 0.f, 1.f) * 255.f));
			dst[0] = dst[1] = dst[2] = value;
			break;
		}
		case DXGI_FORMAT_R16G16_FLOAT:
		{
			std::uint16_t halves[2];
			std::memcpy(halves, src, sizeof(halves));
			const float x = HalfToFloat(halves[0]);
			const float y = HalfToFloat(halves[1]);
			dst[0] = static_cast<std::uint8_t>(std::lround(
				std::clamp(x / 32.f + .5f, 0.f, 1.f) * 255.f));
			dst[1] = static_cast<std::uint8_t>(std::lround(
				std::clamp(y / 32.f + .5f, 0.f, 1.f) * 255.f));
			dst[2] = 128;
			break;
		}
		default: break;
		}
	}
	return image;
}

QualityCaptureWriter::RgbaImage Crop(const QualityCaptureWriter::RgbaImage& source,
	const Rect& rect)
{
	const int x0 = std::clamp(rect.x, 0, static_cast<int>(source.width));
	const int y0 = std::clamp(rect.y, 0, static_cast<int>(source.height));
	const int x1 = std::clamp(rect.x + rect.width, x0, static_cast<int>(source.width));
	const int y1 = std::clamp(rect.y + rect.height, y0, static_cast<int>(source.height));
	QualityCaptureWriter::RgbaImage result;
	result.width = static_cast<std::uint32_t>(x1 - x0);
	result.height = static_cast<std::uint32_t>(y1 - y0);
	result.pixels.resize(static_cast<std::size_t>(result.width) * result.height * 4);
	for (std::uint32_t y = 0; y < result.height; ++y)
		std::memcpy(result.pixels.data() + static_cast<std::size_t>(y) * result.width * 4,
			source.pixels.data() + (static_cast<std::size_t>(y + y0) * source.width + x0) * 4,
			static_cast<std::size_t>(result.width) * 4);
	return result;
}

bool WritePng(const std::filesystem::path& path,
	const QualityCaptureWriter::RgbaImage& image, std::string& error)
{
	if (image.width == 0 || image.height == 0 || image.pixels.empty())
		return false;
	if (!stbi_write_png(path.string().c_str(), static_cast<int>(image.width),
		static_cast<int>(image.height), 4, image.pixels.data(),
		static_cast<int>(image.width * 4)))
	{
		error = "failed writing " + path.string();
		return false;
	}
	return true;
}

bool WriteRaw(const std::filesystem::path& path, const RawTexture& raw,
	std::string& error)
{
	std::ofstream stream(path, std::ios::binary);
	stream.write(reinterpret_cast<const char *>(raw.bytes.data()),
		static_cast<std::streamsize>(raw.bytes.size()));
	if (!stream)
	{
		error = "failed writing " + path.string();
		return false;
	}
	return true;
}

std::uint64_t Hash(const QualityCaptureWriter::RgbaImage& image)
{
	std::uint64_t hash = 1469598103934665603ull;
	for (const auto value : image.pixels) { hash ^= value; hash *= 1099511628211ull; }
	return hash;
}

std::uint64_t Hash(const RawTexture& texture)
{
	constexpr std::uint64_t offset = 14695981039346656037ull;
	constexpr std::uint64_t prime = 1099511628211ull;
	std::uint64_t hash = offset;
	for (const auto byte : texture.bytes)
	{
		hash ^= byte;
		hash *= prime;
	}
	return hash;
}

std::string Hex(std::uint64_t value)
{
	std::ostringstream stream;
	stream.imbue(std::locale::classic());
	stream << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << value;
	return stream.str();
}

QualityCaptureWriter::RgbaImage Difference(const QualityCaptureWriter::RgbaImage& a,
	const QualityCaptureWriter::RgbaImage& b)
{
	QualityCaptureWriter::RgbaImage result;
	if (a.width != b.width || a.height != b.height)
		return result;
	result.width = a.width;
	result.height = a.height;
	result.pixels.resize(a.pixels.size());
	for (std::size_t i = 0; i < a.pixels.size(); ++i)
		result.pixels[i] = static_cast<std::uint8_t>(std::abs(
			static_cast<int>(a.pixels[i]) - static_cast<int>(b.pixels[i])));
	return result;
}

std::string Json(const std::string& value)
{
	std::string result;
	for (const char c : value)
	{
		if (c == '\\' || c == '"') result.push_back('\\');
		if (c == '\n') result += "\\n";
		else if (static_cast<unsigned char>(c) >= 0x20) result.push_back(c);
	}
	return result;
}

double MeanSquared(const QualityCaptureWriter::RgbaImage& difference)
{
	if (difference.pixels.empty()) return 0.;
	double sum = 0.;
	for (std::size_t i = 0; i < difference.pixels.size(); i += 4)
		for (std::size_t channel = 0; channel < 3; ++channel)
			sum += static_cast<double>(difference.pixels[i + channel])
				* difference.pixels[i + channel];
	return sum / (static_cast<double>(difference.width) * difference.height * 3.);
}

double ColorMeanDrift(const QualityCaptureWriter::RgbaImage& a,
	const QualityCaptureWriter::RgbaImage& b)
{
	if (a.width != b.width || a.height != b.height || a.pixels.empty()) return 0.;
	double sumsA[3]{}, sumsB[3]{};
	for (std::size_t i = 0; i < a.pixels.size(); i += 4)
		for (std::size_t channel = 0; channel < 3; ++channel)
		{
			sumsA[channel] += a.pixels[i + channel];
			sumsB[channel] += b.pixels[i + channel];
		}
	return (std::abs(sumsA[0] - sumsB[0]) + std::abs(sumsA[1] - sumsB[1])
		+ std::abs(sumsA[2] - sumsB[2]))
		/ (static_cast<double>(a.width) * a.height * 3.);
}

double SaturationDrift(const QualityCaptureWriter::RgbaImage& a,
	const QualityCaptureWriter::RgbaImage& b)
{
	if (a.width != b.width || a.height != b.height || a.pixels.empty()) return 0.;
	double sum = 0.;
	for (std::size_t i = 0; i < a.pixels.size(); i += 4)
	{
		const auto satA = (std::max)({a.pixels[i], a.pixels[i + 1], a.pixels[i + 2]})
			- (std::min)({a.pixels[i], a.pixels[i + 1], a.pixels[i + 2]});
		const auto satB = (std::max)({b.pixels[i], b.pixels[i + 1], b.pixels[i + 2]})
			- (std::min)({b.pixels[i], b.pixels[i + 1], b.pixels[i + 2]});
		sum += std::abs(static_cast<int>(satA) - static_cast<int>(satB));
	}
	return sum / (static_cast<double>(a.width) * a.height);
}

double BlackLevelDrift(const QualityCaptureWriter::RgbaImage& a,
	const QualityCaptureWriter::RgbaImage& b)
{
	if (a.width != b.width || a.height != b.height || a.pixels.empty()) return 0.;
	double sum = 0.;
	std::uint64_t count = 0;
	for (std::size_t i = 0; i < a.pixels.size(); i += 4)
	{
		const int lumaA = (a.pixels[i] + a.pixels[i + 1] + a.pixels[i + 2]) / 3;
		if (lumaA > 16) continue;
		const int lumaB = (b.pixels[i] + b.pixels[i + 1] + b.pixels[i + 2]) / 3;
		sum += std::abs(lumaA - lumaB);
		++count;
	}
	return count == 0 ? 0. : sum / count;
}

void EdgeMetrics(const QualityCaptureWriter::RgbaImage& reference,
	const QualityCaptureWriter::RgbaImage& output, double& displacement,
	double& continuity)
{
	displacement = continuity = 0.;
	if (reference.width != output.width || reference.height != output.height
		|| reference.width < 2 || reference.height < 2) return;
	const auto edgeMap = [](const QualityCaptureWriter::RgbaImage& image) {
		std::vector<std::uint8_t> edges(static_cast<std::size_t>(image.width) * image.height);
		auto luma = [&](std::uint32_t x, std::uint32_t y) {
			const auto i = (static_cast<std::size_t>(y) * image.width + x) * 4;
			return (static_cast<int>(image.pixels[i]) * 54
				+ static_cast<int>(image.pixels[i + 1]) * 183
				+ static_cast<int>(image.pixels[i + 2]) * 19) / 256;
		};
		for (std::uint32_t y = 0; y + 1 < image.height; ++y)
			for (std::uint32_t x = 0; x + 1 < image.width; ++x)
			{
				const int center = luma(x, y);
				edges[static_cast<std::size_t>(y) * image.width + x] =
					std::abs(center - luma(x + 1, y)) > 32
					|| std::abs(center - luma(x, y + 1)) > 32;
			}
		return edges;
	};
	const auto referenceEdges = edgeMap(reference);
	const auto outputEdges = edgeMap(output);
	std::uint64_t count = 0, matched = 0;
	double distanceSum = 0.;
	for (int y = 0; y < static_cast<int>(reference.height); ++y)
		for (int x = 0; x < static_cast<int>(reference.width); ++x)
		{
			if (!referenceEdges[static_cast<std::size_t>(y) * reference.width + x]) continue;
			++count;
			int best = 3;
			for (int dy = -2; dy <= 2; ++dy)
				for (int dx = -2; dx <= 2; ++dx)
				{
					const int ox = x + dx, oy = y + dy;
					if (ox < 0 || oy < 0 || ox >= static_cast<int>(output.width)
						|| oy >= static_cast<int>(output.height)) continue;
					if (outputEdges[static_cast<std::size_t>(oy) * output.width + ox])
						best = (std::min)(best, (std::max)(std::abs(dx), std::abs(dy)));
				}
			matched += best <= 1;
			distanceSum += best;
		}
	displacement = count == 0 ? 0. : distanceSum / count;
	continuity = count == 0 ? 100. : matched * 100. / count;
}

} // namespace

void QualityCaptureWriter::Configure(const std::filesystem::path& root,
	std::uint32_t skip, std::uint32_t limit)
{
	if (root == root_ && skip == skip_ && limit == limit_)
		return;
	root_ = root;
	skip_ = skip;
	limit_ = (std::min)(limit, 240u);
	seen_ = captured_ = 0;
	previousFrameId_ = 0;
	previousFinal_ = {};
	previousSource_ = {};
}

bool QualityCaptureWriter::WantsFrame() const noexcept
{
	return !root_.empty() && limit_ != 0 && captured_ < limit_;
}

bool QualityCaptureWriter::Capture(ID3D11Device *device, ID3D11DeviceContext *context,
	const QualityCaptureMetadata& metadata, const QualityCaptureTextures& textures,
	std::string& error)
{
	if (!WantsFrame()) return true;
	if (seen_++ < skip_) return true;
	if (!device || !context || !textures.nativeColor || !textures.sourceColor
		|| !textures.finalComposite)
	{
		error = "required production capture textures are unavailable";
		return false;
	}

	std::ostringstream frameName;
	frameName.imbue(std::locale::classic());
	frameName << "frame-" << std::setw(6) << std::setfill('0') << metadata.frameId;
	const auto frameRoot = root_ / frameName.str();
	std::error_code ec;
	std::filesystem::create_directories(frameRoot, ec);
	if (ec) { error = "cannot create capture directory: " + ec.message(); return false; }

	auto read = [&](ID3D11Texture2D *texture, RawTexture& raw,
		QualityCaptureWriter::RgbaImage& rgba) {
		if (!texture) return false;
		if (!ReadTexture(device, context, texture, raw, error)) return false;
		rgba = ToRgba(raw);
		return true;
	};
	RawTexture nativeRaw, sourceRaw, depthRaw, motionRaw, maskRaw, confidenceRaw, drawRaw,
		overlayRaw, publicRaw, finalRaw;
	RgbaImage native, source, depth, motion, mask, confidence, draw, overlay, publicOutput, finalFull;
	if (!read(textures.nativeColor, nativeRaw, native)
		|| !read(textures.sourceColor, sourceRaw, source)
		|| !read(textures.depth, depthRaw, depth)
		|| !read(textures.motion, motionRaw, motion)
		|| !read(textures.biasMask, maskRaw, mask)
		|| !read(textures.confidence, confidenceRaw, confidence)
		|| !read(textures.drawId, drawRaw, draw)
		|| !read(textures.overlay, overlayRaw, overlay)
		|| !read(textures.finalComposite, finalRaw, finalFull))
		return false;
	const bool hasPublicOutput = read(textures.publicOutput, publicRaw, publicOutput);
	if (textures.publicOutput && !hasPublicOutput) return false;
	const auto final = Crop(finalFull, metadata.contentRect);

	if (!WritePng(frameRoot / "native-pvr-color.png", native, error)
		|| !WritePng(frameRoot / "source-color.png", source, error)
		|| !WriteRaw(frameRoot / "depth.f32", depthRaw, error)
		|| !WritePng(frameRoot / "depth.png", depth, error)
		|| !WriteRaw(frameRoot / "motion.rg16f", motionRaw, error)
		|| !WritePng(frameRoot / "motion.png", motion, error)
		|| !WritePng(frameRoot / "bias-mask.png", mask, error)
		|| !WritePng(frameRoot / "confidence.png", confidence, error)
		|| !WriteRaw(frameRoot / "draw-id.r16u", drawRaw, error)
		|| !WritePng(frameRoot / "draw-id.png", draw, error)
		|| !WritePng(frameRoot / "overlay-classification.png", overlay, error)
		|| !WritePng(frameRoot / "final-composited.png", final, error))
		return false;
	if (hasPublicOutput && !WritePng(frameRoot / "public-dlaa-output.png", publicOutput, error))
		return false;
	if (hasPublicOutput && metadata.externalOutputConfirmed
		&& !WritePng(frameRoot / "neural-rendering-output.png", publicOutput, error))
		return false;
	const auto nativeDifference = hasPublicOutput ? Difference(native, publicOutput) : RgbaImage{};
	const auto flicker = Difference(previousFinal_, final);
	if (!nativeDifference.pixels.empty()
		&& !WritePng(frameRoot / "native-versus-output-difference.png", nativeDifference, error))
		return false;
	if (!flicker.pixels.empty()
		&& !WritePng(frameRoot / "temporal-flicker.png", flicker, error))
		return false;

	std::uint64_t invalidDepth = 0, invalidMotion = 0, trusted = 0, reactive = 0,
		hudMismatch = 0;
	for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(source.width) * source.height; ++pixel)
	{
		float d;
		std::memcpy(&d, depthRaw.bytes.data() + pixel * 4, sizeof(d));
		invalidDepth += !std::isfinite(d) || d < 0.f || d > 1.f;
		std::uint16_t halves[2];
		std::memcpy(halves, motionRaw.bytes.data() + pixel * 4, sizeof(halves));
		invalidMotion += !std::isfinite(HalfToFloat(halves[0]))
			|| !std::isfinite(HalfToFloat(halves[1]));
		const bool pixelReactive = maskRaw.bytes[pixel] >= 128;
		reactive += pixelReactive;
		trusted += !pixelReactive && confidenceRaw.bytes[pixel] >= 128;
		if (overlayRaw.bytes[pixel] >= 128 && final.width == native.width
			&& final.height == native.height)
		{
			const auto offset = pixel * 4;
			hudMismatch += std::memcmp(native.pixels.data() + offset,
				final.pixels.data() + offset, 4) != 0;
		}
	}
	const double pixels = static_cast<double>(source.width) * source.height;
	const double temporalVariance = flicker.pixels.empty() ? 0. : MeanSquared(flicker);
	double trailEnergy = 0., reprojectionError = 0.;
	std::uint64_t trailSamples = 0, reprojectionSamples = 0;
	if (hasPublicOutput && publicOutput.width == native.width
		&& publicOutput.height == native.height)
	{
		for (std::uint32_t y = 0; y < native.height; ++y)
			for (std::uint32_t x = 0; x < native.width; ++x)
			{
				const auto pixel = static_cast<std::size_t>(y) * native.width + x;
				if (maskRaw.bytes[pixel] >= 128)
				{
					for (std::size_t channel = 0; channel < 3; ++channel)
						trailEnergy += std::abs(static_cast<int>(native.pixels[pixel * 4 + channel])
							- static_cast<int>(publicOutput.pixels[pixel * 4 + channel]));
					trailSamples += 3;
				}
				if (previousSource_.width != source.width || previousSource_.height != source.height
					|| maskRaw.bytes[pixel] >= 128 || confidenceRaw.bytes[pixel] < 128)
					continue;
				std::uint16_t halves[2];
				std::memcpy(halves, motionRaw.bytes.data() + pixel * 4, sizeof(halves));
				const int previousX = static_cast<int>(std::lround(x + HalfToFloat(halves[0])));
				const int previousY = static_cast<int>(std::lround(y + HalfToFloat(halves[1])));
				if (previousX < 0 || previousY < 0 || previousX >= static_cast<int>(source.width)
					|| previousY >= static_cast<int>(source.height)) continue;
				const auto previousPixel = static_cast<std::size_t>(previousY) * source.width + previousX;
				for (std::size_t channel = 0; channel < 3; ++channel)
					reprojectionError += std::abs(static_cast<int>(source.pixels[pixel * 4 + channel])
						- static_cast<int>(previousSource_.pixels[previousPixel * 4 + channel]));
				reprojectionSamples += 3;
			}
	}
	trailEnergy = trailSamples == 0 ? 0. : trailEnergy / trailSamples;
	reprojectionError = reprojectionSamples == 0 ? 0. : reprojectionError / reprojectionSamples;
	double edgeDisplacement = 0., thinLineContinuity = 0.;
	if (hasPublicOutput) EdgeMetrics(native, publicOutput, edgeDisplacement, thinLineContinuity);
	const double colorDrift = hasPublicOutput ? ColorMeanDrift(native, publicOutput) : 0.;
	const double saturationDrift = hasPublicOutput ? SaturationDrift(native, publicOutput) : 0.;
	const double blackLevelDrift = hasPublicOutput ? BlackLevelDrift(native, publicOutput) : 0.;
	const bool repeated = !previousFinal_.pixels.empty() && Hash(previousFinal_) == Hash(final);
	const std::uint64_t dropped = previousFrameId_ != 0 && metadata.frameId > previousFrameId_ + 1
		? metadata.frameId - previousFrameId_ - 1 : 0;

	std::ofstream metrics(frameRoot / "metrics.json");
	metrics.imbue(std::locale::classic());
	metrics << std::fixed << std::setprecision(6)
		<< "{\n  \"static_frame_temporal_variance\": " << temporalVariance
		<< ",\n  \"motion_reprojection_error\": " << reprojectionError
		<< ",\n  \"disocclusion_trail_energy\": " << trailEnergy
		<< ",\n  \"edge_silhouette_displacement\": " << edgeDisplacement
		<< ",\n  \"thin_line_continuity\": " << thinLineContinuity
		<< ",\n  \"low_frequency_color_drift\": " << colorDrift
		<< ",\n  \"saturation_drift\": " << saturationDrift
		<< ",\n  \"black_level_drift\": " << blackLevelDrift
		<< ",\n  \"hud_pixel_mismatch_count\": " << hudMismatch
		<< ",\n  \"output_frame_repeated\": " << (repeated ? "true" : "false")
		<< ",\n  \"output_frame_drop_count\": " << dropped
		<< ",\n  \"depth_invalid_pixel_coverage\": " << invalidDepth / pixels
		<< ",\n  \"motion_invalid_pixel_coverage\": " << invalidMotion / pixels
		<< ",\n  \"trusted_pixel_percentage\": " << trusted * 100. / pixels
		<< ",\n  \"reactive_pixel_percentage\": " << reactive * 100. / pixels
		<< ",\n  \"gpu_timings_ms\": null,"
		<< "\n  \"capture_mode\": \"synchronous-developer-only-excluded-from-performance\"\n}\n";
	if (!metrics) { error = "failed writing metrics.json"; return false; }

	std::ofstream manifest(frameRoot / "manifest.json");
	manifest.imbue(std::locale::classic());
	manifest << "{\n  \"schema\": 3,\n  \"git_sha\": \"" << GIT_HASH
		<< "\",\n  \"game_id\": \"" << Json(metadata.gameId)
		<< "\",\n  \"frame_id\": " << metadata.frameId
		<< ",\n  \"history_generation\": " << metadata.historyGeneration
		<< ",\n  \"history_age\": " << metadata.historyAge
		<< ",\n  \"skipped_frame_count\": " << metadata.skippedFrameCount
		<< ",\n  \"history_valid\": " << (metadata.historyValid ? "true" : "false")
		<< ",\n  \"reset_history\": " << (metadata.resetHistory ? "true" : "false")
		<< ",\n  \"scene_cut\": " << (metadata.sceneCut ? "true" : "false")
		<< ",\n  \"truncated\": " << (metadata.truncated ? "true" : "false")
		<< ",\n  \"predominantly_2d\": " << (metadata.predominantly2D ? "true" : "false")
		<< ",\n  \"draw_count\": " << metadata.drawCount
		<< ",\n  \"correspondence\": {"
		<< "\n    \"opaque_draws\": " << metadata.correspondence.opaqueDraws
		<< ",\n    \"punch_through_draws\": " << metadata.correspondence.punchThroughDraws
		<< ",\n    \"translucent_draws\": " << metadata.correspondence.translucentDraws
		<< ",\n    \"trusted_draws_before_scene_cut\": " << metadata.correspondence.trustedDrawsBeforeSceneCut
		<< ",\n    \"candidate_draws_before_position_validation\": " << metadata.correspondence.candidateDrawsBeforePositionValidation
		<< ",\n    \"candidate_tier1_draws\": " << metadata.correspondence.candidateTier1Draws
		<< ",\n    \"candidate_tier2_draws\": " << metadata.correspondence.candidateTier2Draws
		<< ",\n    \"candidate_tier3_draws\": " << metadata.correspondence.candidateTier3Draws
		<< ",\n    \"trusted_previous_vertices\": " << metadata.correspondence.trustedPreviousVertices
		<< ",\n    \"reactive_draws\": " << metadata.correspondence.reactiveDraws
		<< ",\n    \"ambiguous_draws\": " << metadata.correspondence.ambiguousDraws
		<< ",\n    \"unmatched_draws\": " << metadata.correspondence.unmatchedDraws
		<< ",\n    \"matched_area_before_scene_cut\": " << metadata.correspondence.matchedAreaBeforeSceneCut
		<< ",\n    \"candidate_area_before_position_validation\": " << metadata.correspondence.candidateAreaBeforePositionValidation
		<< ",\n    \"total_area_for_scene_cut\": " << metadata.correspondence.totalAreaForSceneCut
		<< ",\n    \"matched_opaque_area_before_scene_cut\": " << metadata.correspondence.matchedOpaqueAreaBeforeSceneCut
		<< ",\n    \"total_opaque_area_for_scene_cut\": " << metadata.correspondence.totalOpaqueAreaForSceneCut
		<< "\n  }"
		<< ",\n  \"render_size\": [" << metadata.renderWidth << ", " << metadata.renderHeight << "]"
		<< ",\n  \"output_size\": [" << metadata.outputWidth << ", " << metadata.outputHeight << "]"
		<< ",\n  \"content_rect\": [" << metadata.contentRect.x << ", " << metadata.contentRect.y
		<< ", " << metadata.contentRect.width << ", " << metadata.contentRect.height << "]"
		<< ",\n  \"api\": \"" << (metadata.d3d11On12 ? "d3d11on12" : "d3d11") << "\""
		<< ",\n  \"renderer\": \"" << (metadata.oitRenderer ? "dx11-oit" : "dx11") << "\""
		<< ",\n  \"neural_mode\": " << metadata.neuralMode
		<< ",\n  \"public_dlss_preset\": " << metadata.dlssPreset
		<< ",\n  \"overlay_policy\": " << metadata.overlayPolicy
		<< ",\n  \"profile\": \"" << Json(metadata.profile) << "\""
		<< ",\n  \"external_settings\": \"" << Json(metadata.externalRecommendation) << "\""
		<< ",\n  \"evaluation_accepted\": " << (metadata.evaluationAccepted ? "true" : "false")
		<< ",\n  \"submit_status\": \"" << Json(metadata.submitStatus) << "\""
		<< ",\n  \"external_contract_evaluated\": "
		<< (metadata.externalContractEvaluated ? "true" : "false")
		<< ",\n  \"external_output_confirmed\": "
		<< (metadata.externalOutputConfirmed ? "true" : "false")
		<< ",\n  \"contract_hashes\": {"
		<< "\n    \"color_fnv64\": \"" << Hex(Hash(sourceRaw)) << "\","
		<< "\n    \"depth_fnv64\": \"" << Hex(Hash(depthRaw)) << "\","
		<< "\n    \"motion_fnv64\": \"" << Hex(Hash(motionRaw)) << "\","
		<< "\n    \"mask_fnv64\": \"" << Hex(Hash(maskRaw)) << "\","
		<< "\n    \"returned_fnv64\": \""
		<< (hasPublicOutput ? Hex(Hash(publicRaw)) : std::string{}) << "\"\n  }"
		<< ",\n  \"public_output_present\": " << (hasPublicOutput ? "true" : "false")
		<< ",\n  \"neural_rendering_output_present\": "
		<< (hasPublicOutput && metadata.externalOutputConfirmed ? "true" : "false")
		<< ",\n  \"capture_stalls_gpu\": true,\n  \"eligible_for_performance_metrics\": false\n}\n";
	if (!manifest) { error = "failed writing manifest.json"; return false; }

	previousFrameId_ = metadata.frameId;
	previousFinal_ = final;
	previousSource_ = source;
	++captured_;
	if (captured_ == limit_)
	{
		std::ofstream complete(root_ / "capture-complete.json");
		complete.imbue(std::locale::classic());
		complete << "{\n  \"schema\": 1,\n  \"git_sha\": \"" << GIT_HASH
			<< "\",\n  \"game_id\": \"" << Json(metadata.gameId)
			<< "\",\n  \"captured_frames\": " << captured_
			<< ",\n  \"status\": \"complete\"\n}\n";
		if (!complete) { error = "failed writing capture completion marker"; return false; }
	}
	return true;
}

} // namespace flycast::rend::neural
