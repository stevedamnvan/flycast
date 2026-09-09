// SPDX-License-Identifier: GPL-2.0-or-later
#include "quality_capture.h"
#include <cstdlib>
#include <cstdio>
#include "pvr_scene_capture.h"
#include "motion_reference.h"
#include "version.h"
#include "remake_input_replay.h"

#include <stb/stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <thread>

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
	std::uint32_t skip, std::uint32_t limit, bool lateOverlayProof, std::uint64_t startFrame, std::uint64_t startProducer)
{
	if (root == root_ && skip == skip_ && limit == limit_
		&& lateOverlayProof == lateOverlayProof_ && startFrame == startFrame_ && startProducer == startProducer_)
		return;
	root_ = root;
	remakeChannel_.Close();
	remakePreparedBeforeComposite_=false;
	remakeInputReplayed_=false;remakeReplayOriginalFrame_=0;
	remakeReturnedImage_.reset();
	pvrSnapshot_.reset();
	remakeView_.reset();
	remakePacket_.reset();remakePacketStatus_="not-requested";
	skip_ = skip;
	limit_ = (std::min)(limit, 240u);
	lateOverlayProof_ = lateOverlayProof;
	seen_ = captured_ = lateOverlayCaptured_ = 0;
	startFrame_ = startFrame; sourceFrame_ = 0;
	startProducer_ = startProducer; sourceProducer_ = 0;
	captureStartConsumed_ = false;
	previousFrameId_ = 0;
	pendingLateOverlayFrameId_ = 0;
	pendingLateOverlayContentRect_ = {};
	previousFinal_ = {};
	previousSource_ = {};
	pendingPreFlycastOverlayFull_ = {};
}

bool QualityCaptureWriter::WantsFrame() const noexcept
{
	return !root_.empty() && limit_ != 0 && captured_ < limit_;
}

bool QualityCaptureWriter::CapturesCurrentFrame() const noexcept
{
	return WantsFrame() && (startProducer_ ? sourceProducer_ >= startProducer_
		: startFrame_ ? sourceFrame_ >= startFrame_ : seen_ >= skip_);
}

bool QualityCaptureWriter::ConsumeCaptureStart() noexcept
{
	if (captureStartConsumed_ || !CapturesCurrentFrame())
		return false;
	captureStartConsumed_ = true;
	return true;
}

void QualityCaptureGpuTimer::Reset()
{
	active_ = false;
	marked_.fill(false);
	for (auto& point : points_) point.reset();
	disjoint_.reset();
	device_.reset();
}

void QualityCaptureGpuTimer::Configure(ID3D11Device *device, bool enabled)
{
	if (!enabled || !device)
	{
		Reset();
		return;
	}
	if (device_.get() == device && disjoint_) return;
	Reset();
	D3D11_QUERY_DESC disjointDesc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
	D3D11_QUERY_DESC timestampDesc{D3D11_QUERY_TIMESTAMP, 0};
	if (FAILED(device->CreateQuery(&disjointDesc, &disjoint_.get()))) return;
	for (auto& point : points_)
		if (FAILED(device->CreateQuery(&timestampDesc, &point.get())))
		{
			Reset();
			return;
		}
	device->AddRef();
	device_.reset(device);
}

void QualityCaptureGpuTimer::BeginFrame(ID3D11DeviceContext *context,
	bool captureCurrentFrame)
{
	active_ = false;
	if (!captureCurrentFrame || !context || !disjoint_) return;
	marked_.fill(false);
	context->Begin(disjoint_);
	active_ = true;
	Mark(context, CaptureGpuTimingPoint::PvrBegin);
}

void QualityCaptureGpuTimer::Mark(ID3D11DeviceContext *context,
	CaptureGpuTimingPoint point)
{
	if (!active_ || !context) return;
	const auto index = static_cast<std::size_t>(point);
	if (index >= PointCount) return;
	context->End(points_[index]);
	marked_[index] = true;
}

bool QualityCaptureGpuTimer::EndAndResolve(ID3D11DeviceContext *context,
	QualityGpuTimings& timings)
{
	timings = {};
	if (!active_ || !context || !disjoint_) return false;
	context->End(disjoint_);
	active_ = false;
	// Quality capture is already an explicitly synchronous developer path. A
	// single flush here makes the exact retained frame's timestamps observable;
	// production performance telemetry never enters this method.
	context->Flush();
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
	while (context->GetData(disjoint_, &disjoint, sizeof(disjoint),
		D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
	{
		if (std::chrono::steady_clock::now() >= deadline) return false;
		std::this_thread::yield();
	}
	if (disjoint.Disjoint || disjoint.Frequency == 0) return false;
	std::array<UINT64, PointCount> timestamps{};
	for (std::size_t i = 0; i < PointCount; ++i)
	{
		if (!marked_[i]) continue;
		while (context->GetData(points_[i], &timestamps[i], sizeof(timestamps[i]),
			D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
		{
			if (std::chrono::steady_clock::now() >= deadline) return false;
			std::this_thread::yield();
		}
	}
	auto duration = [&](CaptureGpuTimingPoint begin, CaptureGpuTimingPoint end,
		double& milliseconds) {
		const auto b = static_cast<std::size_t>(begin);
		const auto e = static_cast<std::size_t>(end);
		if (!marked_[b] || !marked_[e] || timestamps[e] < timestamps[b]) return false;
		milliseconds = static_cast<double>(timestamps[e] - timestamps[b]) * 1000.
			/ static_cast<double>(disjoint.Frequency);
		return true;
	};
	timings.pvrAvailable = duration(CaptureGpuTimingPoint::PvrBegin,
		CaptureGpuTimingPoint::PvrEnd, timings.pvrMs);
	timings.guidanceAvailable = duration(CaptureGpuTimingPoint::GuidanceBegin,
		CaptureGpuTimingPoint::GuidanceEnd, timings.guidanceMs);
	timings.evaluateAvailable = duration(CaptureGpuTimingPoint::EvaluateBegin,
		CaptureGpuTimingPoint::EvaluateEnd, timings.evaluateMs);
	timings.compositeAvailable = duration(CaptureGpuTimingPoint::CompositeBegin,
		CaptureGpuTimingPoint::CompositeEnd, timings.compositeMs);
	timings.totalAvailable = duration(CaptureGpuTimingPoint::PvrBegin,
		CaptureGpuTimingPoint::CompositeEnd, timings.totalMs);
	timings.available = timings.pvrAvailable || timings.guidanceAvailable
		|| timings.evaluateAvailable || timings.compositeAvailable || timings.totalAvailable;
	return timings.available;
}

void QualityCaptureWriter::ExchangeRemakePacket()
{
	std::string conversionError;
	if(const auto* token=std::getenv("FLYCAST_REMAKE_CHANNEL");token&&*token) {
		if(!remakeChannel_.IsOpen()&&!remakeChannel_.OpenPublisher(token,conversionError))remakePacketStatus_=conversionError;
		else {
			RemakeReturnedImage returned;std::string returnError;
			if(remakeChannel_.ReceiveImage(returned,returnError)==RemakeChannelResult::Received) {
				std::fprintf(stderr,"Remake returned image source=%llu sequence=%llu bytes=%u presentation=false\n",
					(unsigned long long)returned.frame,(unsigned long long)returned.source.sequence,(unsigned)returned.bgra.size());
				remakeReturnedImage_=std::move(returned);
			}
			RemakeChannelReceipt receipt;
			const auto result=remakeChannel_.PublishForReturn(*remakePacket_,receipt,conversionError);
			if(result==RemakeChannelResult::Published)remakePacketStatus_="live-published sequence="+std::to_string(receipt.sequence)
				+" bytes="+std::to_string(receipt.bytes)+" digest="+std::to_string(receipt.digest)+"; presentation-unproven";
			else remakePacketStatus_=conversionError;
			// Explicit synchronous developer proof only, never ordinary pacing.
			const auto* waitTest=std::getenv("FLYCAST_REMAKE_RETURN_TEST_WAIT");
			if(result==RemakeChannelResult::Published&&waitTest&&std::strcmp(waitTest,"1")==0) {
				const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
				do {
					const auto received=remakeChannel_.ReceiveImage(returned,returnError);
					if(received==RemakeChannelResult::Received) {
						std::fprintf(stderr,"Remake returned image source=%llu sequence=%llu bytes=%u presentation=false\n",
							(unsigned long long)returned.frame,(unsigned long long)returned.source.sequence,(unsigned)returned.bgra.size());
						remakeReturnedImage_=std::move(returned);break;
					}
					if(received!=RemakeChannelResult::Empty)break;
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}while(std::chrono::steady_clock::now()<deadline);
			}
		}
	}
}

bool QualityCaptureWriter::PrepareRemakeBeforeComposite(const PvrDecodedPacket& snapshot,
	const QualityCaptureMetadata& metadata,const RemakeTextureReader& reader,const std::function<void()>& beforeExchange)
{
	if(remakePreparedBeforeComposite_&&remakePacket_&&remakePacket_->frame==metadata.frameId
		&&remakePacket_->producer.epoch==metadata.producerIdentity.epoch
		&&remakePacket_->producer.ordinal==metadata.producerIdentity.ordinal
		&&remakePacket_->producer.cycle==metadata.producerIdentity.cycle)
		return ReturnedRemakeFrame(metadata.frameId)!=nullptr;
	remakePreparedBeforeComposite_=false;remakePacket_.reset();remakeView_.reset();remakeReturnedImage_.reset();
	remakeInputReplayed_=false;remakeReplayOriginalFrame_=0;
	if(!CapturesCurrentFrame()||!reader)return false;
	RemakeViewScene scene;remake::Packet packet;std::string error;
	const auto* estimated=std::getenv("FLYCAST_REMAKE_ESTIMATE_UNTRACED");
	const bool estimateUntraced=estimated&&std::strcmp(estimated,"1")==0;
	if(!BuildRemakeViewScene(snapshot,metadata.producerIdentity,metadata.frameId,scene,error,estimateUntraced)
		||!BuildRemakeViewPacket(scene,reader,packet,error)){remakePacketStatus_=error;return false;}
	remakeView_=std::move(scene);remakePacket_=std::move(packet);
	if(const auto* locked=std::getenv("FLYCAST_REMAKE_LOCKED_INPUT_ROOT");locked&&*locked) {
		remakePreparedBeforeComposite_=true; // Preserve rejected replay diagnostics during archival too.
		RemakeReturnedImage image;
		if(!ReadLockedRemakeInput(std::filesystem::u8path(locked),*remakePacket_,image,remakeReplayOriginalFrame_,remakePacketStatus_))return false;
		remakeReturnedImage_=std::move(image);remakeInputReplayed_=true;remakePreparedBeforeComposite_=true;
		remakePacketStatus_="locked-returned-input-replay-not-live";return true;
	}
	if(beforeExchange)beforeExchange();
	ExchangeRemakePacket();remakePreparedBeforeComposite_=true;
	return ReturnedRemakeFrame(metadata.frameId)!=nullptr;
}

bool QualityCaptureWriter::Capture(ID3D11Device *device, ID3D11DeviceContext *context,
	const QualityCaptureMetadata& metadata, const QualityCaptureTextures& textures,
	std::string& error)
{
	pvrSnapshot_.reset();
	if(!remakePacket_||remakePacket_->frame!=metadata.frameId
		||remakePacket_->producer.epoch!=metadata.producerIdentity.epoch
		||remakePacket_->producer.ordinal!=metadata.producerIdentity.ordinal
		||remakePacket_->producer.cycle!=metadata.producerIdentity.cycle) {
		remakeView_.reset();remakePacket_.reset();remakePacketStatus_="not-requested";
		remakePreparedBeforeComposite_=false;
	}
	if (!WantsFrame()) return true;
	if (startProducer_ ? metadata.producerIdentity.ordinal < startProducer_
		: startFrame_ ? metadata.frameId < startFrame_ : seen_++ < skip_) return true;
	if (textures.pvrPacketRequested && !textures.pvrContext)
	{
		error = "requested PVR packet context unavailable";
		return false;
	}
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
	std::optional<PvrDecodedPacket> pendingSnapshot;
	if (textures.pvrContext) {
		pendingSnapshot.emplace();
		if (!SnapshotPvrScenePacket(*textures.pvrContext, textures.pvrViewport,
			metadata.frameId, metadata.gameId, *pendingSnapshot, error)) return false;
	}
	if (textures.pvrContext && !WritePvrScenePacket(frameRoot / "pvr-scene.json",
		*textures.pvrContext, textures.pvrViewport, metadata.frameId, metadata.gameId, error))
		return false;
	if (textures.pvrMaterials && !textures.pvrMaterials(frameRoot / "pvr-scene.json", error))
		return false;
	if(pendingSnapshot&&!pendingSnapshot->sourceVertices.empty()
		&&!WritePvrSourceWitness(frameRoot / "pvr-source-witness.json",*pendingSnapshot,error))return false;

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
	if (textures.pvrReplay)
	{
		PvrReplayTextures replay;
		if (!textures.pvrReplay(frameRoot / "pvr-scene.json", replay, error)) return false;
		RawTexture raw; RgbaImage base;
		if (!read(replay.priorFramebuffer, raw, base)
			|| !WritePng(frameRoot / "pvr-pre-frame.png", base, error)) return false;
		std::array<std::uint64_t,4> differences{};
		std::array<unsigned,4> maxChannelDelta{};
		RgbaImage decodedImage;
		std::uint64_t decodedVersusRetained = 0;
		const char *names[] = {"pvr-decoded-replay.png", "pvr-wrong-viewport.png", "pvr-wrong-depth.png", "pvr-retained-buffer-replay.png"};
		for (size_t lane = 0; lane < 4; ++lane)
		{
			RgbaImage rgba;
			if (!read(replay.color[lane], raw, rgba) || rgba.width != native.width || rgba.height != native.height)
			{ error = "pvr-replay-readback-size"; return false; }
			for (size_t i = 0; i < native.pixels.size(); i += 4)
				if (std::memcmp(native.pixels.data() + i, rgba.pixels.data() + i, 4) != 0) ++differences[lane];
			for (size_t i = 0; i < native.pixels.size(); ++i)
				maxChannelDelta[lane] = (std::max)(maxChannelDelta[lane], static_cast<unsigned>(std::abs(int(native.pixels[i]) - int(rgba.pixels[i]))));
			if (!WritePng(frameRoot / names[lane], rgba, error)) return false;
			if (lane == 0) decodedImage = rgba;
			if (lane == 3) for (size_t i = 0; i < rgba.pixels.size(); i += 4)
				if (std::memcmp(rgba.pixels.data() + i, decodedImage.pixels.data() + i, 4) != 0) ++decodedVersusRetained;
		}
		std::ofstream proof(frameRoot / "pvr-replay-proof.json");
		proof.imbue(std::locale::classic());
		proof << "{\"schema\":1,\"frame_id\":" << metadata.frameId
			<< ",\"decoded_geometry_pixels_different\":" << differences[0]
			<< ",\"wrong_viewport_pixels_different\":" << differences[1]
			<< ",\"wrong_depth_pixels_different\":" << differences[2]
			<< ",\"retained_buffer_replay_pixels_different\":" << differences[3]
			<< ",\"decoded_versus_retained_replay_pixels_different\":" << decodedVersusRetained
			<< ",\"decoded_max_channel_delta\":" << maxChannelDelta[0]
			<< ",\"retained_max_channel_delta\":" << maxChannelDelta[3]
			<< ",\"geometry_alignment_passed\":" << (decodedVersusRetained == 0 ? "true" : "false")
			<< ",\"source_frame_exact\":" << (differences[0] == 0 && differences[3] == 0 ? "true" : "false")
			<< ",\"retained_same_frame_supplemental_state\":true,\"world_camera_recovered\":false,\"standalone_replay\":false,\"passed\":"
			<< (differences[0] == 0 && differences[3] == 0 && decodedVersusRetained == 0
				&& differences[1] > 100 && differences[2] > 100 ? "true" : "false") << "}\n";
		if (!proof) { error = "pvr-replay-proof-write"; return false; }
	}

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
		hudMismatch = 0, hudProtected = 0;
	const bool hudComparable = final.width == native.width && final.height == native.height
		&& native.width == source.width && native.height == source.height;
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
		if (overlayRaw.bytes[pixel] >= 128)
		{
			++hudProtected;
			const auto offset = pixel * 4;
			if (hudComparable)
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

	std::ofstream overlayDraws(frameRoot / "overlay-draws.json");
	overlayDraws.imbue(std::locale::classic());
	overlayDraws << "{\n  \"schema\": 2,\n  \"frame_id\": " << metadata.frameId
		<< ",\n  \"coordinate_space\": \"PVR-native-screen\""
		<< ",\n  \"screen_size\": [" << metadata.screenWidth << ',' << metadata.screenHeight << ']'
		<< ",\n  \"overlay_profile\": \"" << OverlayProfileName(metadata.overlayProfile) << "\""
		<< ",\n  \"draws\": [";
	bool firstOverlayDraw = true;
	for (const auto& evidence : metadata.overlayDraws)
	{
		const auto& draw = evidence.draw;
		if (!firstOverlayDraw) overlayDraws << ',';
		firstOverlayDraw = false;
		overlayDraws << "\n    {\"ordinal\":" << draw.ordinal << ",\"list\":" << draw.list
			<< ",\"pass\":" << draw.pass << ",\"flags\":" << unsigned(draw.flags)
			<< ",\"blend\":" << unsigned(draw.blend) << ",\"texture_id\":" << draw.texId
			<< ",\"texture_generation\":" << draw.textureGeneration
			<< ",\"palette_generation\":" << draw.paletteGeneration
			<< ",\"rtt_generation\":" << draw.rttGeneration
			<< ",\"state_signature\":" << draw.stateSig << ",\"uv_signature\":" << draw.uvSig
			<< ",\"topology_signature\":" << draw.topologySig
			<< ",\"vertex_count\":" << draw.vertexCount << ",\"index_count\":" << draw.indexCount
			<< ",\"screen_aligned_primitive_count\":" << draw.screenAlignedPrimitiveCount
			<< ",\"bbox\":[" << draw.bboxMin[0] << ',' << draw.bboxMin[1] << ','
			<< draw.bboxMax[0] << ',' << draw.bboxMax[1] << ']'
			<< ",\"depth_range\":[";
		if (std::isfinite(draw.zMin)) overlayDraws << draw.zMin; else overlayDraws << "null";
		overlayDraws << ',';
		if (std::isfinite(draw.zMax)) overlayDraws << draw.zMax; else overlayDraws << "null";
		overlayDraws << "],\"stable_accepted_frames\":" << unsigned(evidence.stableAcceptedFrames)
			<< ",\"texture_use_count\":" << evidence.textureUseCount
			<< ",\"classified\":" << (evidence.classified ? "true" : "false") << '}';
	}
	overlayDraws << "\n  ]\n}\n";
	if (!overlayDraws) { error = "failed writing overlay draw diagnostics"; return false; }

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
		<< ",\n  \"hud_pixel_mismatch_count\": "
		<< (hudComparable ? std::to_string(hudMismatch) : "null")
		<< ",\n  \"hud_protected_pixel_count\": " << hudProtected
		<< ",\n  \"hud_protected_pixel_percentage\": " << hudProtected * 100. / pixels
		<< ",\n  \"hud_comparison_available\": " << (hudComparable ? "true" : "false")
		<< ",\n  \"hud_protected_pixels_verified\": "
		<< (hudComparable && hudProtected > 0 && hudMismatch == 0 ? "true" : "false")
		<< ",\n  \"output_frame_repeated\": " << (repeated ? "true" : "false")
		<< ",\n  \"output_frame_drop_count\": " << dropped
		<< ",\n  \"depth_invalid_pixel_coverage\": " << invalidDepth / pixels
		<< ",\n  \"motion_invalid_pixel_coverage\": " << invalidMotion / pixels
		<< ",\n  \"trusted_pixel_percentage\": " << trusted * 100. / pixels
		<< ",\n  \"reactive_pixel_percentage\": " << reactive * 100. / pixels
		<< ",\n  \"gpu_timings_ms\": {"
		<< "\n    \"scope\": \"synchronous-developer-capture-only\""
		<< ",\n    \"evaluation_scope\": \""
		<< (metadata.d3d11On12
			? metadata.gpuTimings.evaluateAvailable ? "d3d12-backend-exact-frame"
				: "unavailable-no-exact-frame-retirement"
			: "native-d3d11-context") << "\"";
	auto timing = [&](const char *name, bool available, double value) {
		metrics << ",\n    \"" << name << "\": ";
		if (available) metrics << value;
		else metrics << "null";
	};
	timing("base_pvr", metadata.gpuTimings.pvrAvailable, metadata.gpuTimings.pvrMs);
	timing("guidance", metadata.gpuTimings.guidanceAvailable,
		metadata.gpuTimings.guidanceMs);
	timing("stage_evaluate", metadata.gpuTimings.evaluateAvailable,
		metadata.gpuTimings.evaluateMs);
	timing("overlay_and_present_blit", metadata.gpuTimings.compositeAvailable,
		metadata.gpuTimings.compositeMs);
	timing("frame_gpu_timestamp_span", metadata.gpuTimings.totalAvailable,
		metadata.gpuTimings.totalMs);
	metrics << "\n  },"
		<< "\n  \"capture_mode\": \"synchronous-developer-only-excluded-from-performance\"\n}\n";
	if (!metrics) { error = "failed writing metrics.json"; return false; }

	std::ofstream manifest(frameRoot / "manifest.json");
	manifest.imbue(std::locale::classic());
	manifest << "{\n  \"schema\": 3,\n  \"git_sha\": \"" << GIT_HASH
		<< "\",\n  \"game_id\": \"" << Json(metadata.gameId)
		<< "\",\n  \"frame_id\": " << metadata.frameId
		<< ",\n  \"producer_identity\": {\"available\":" << (metadata.producerIdentity.Available() ? "true" : "false")
		<< ",\"clock\":\"sh4-scheduler-cycles\",\"epoch\":" << metadata.producerIdentity.epoch
		<< ",\"ordinal\":" << metadata.producerIdentity.ordinal
		<< ",\"cycle\":" << metadata.producerIdentity.cycle << "}"
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
		<< ",\n  \"raster_jitter\": [" << metadata.jitterX << ", " << metadata.jitterY << "]"
		<< ",\n  \"raster_jitter_applied\": "
		<< (metadata.rasterJitterApplied ? "true" : "false")
		<< ",\n  \"raster_jitter_reason\": \"" << Json(metadata.rasterJitterReason) << "\""
		<< ",\n  \"pvr_screen_size\": [" << metadata.screenWidth << ", " << metadata.screenHeight << "]"
		<< ",\n  \"content_rect\": [" << metadata.contentRect.x << ", " << metadata.contentRect.y
		<< ", " << metadata.contentRect.width << ", " << metadata.contentRect.height << "]"
		<< ",\n  \"api\": \"" << (metadata.d3d11On12 ? "d3d11on12" : "d3d11") << "\""
		<< ",\n  \"renderer\": \"" << (metadata.oitRenderer ? "dx11-oit" : "dx11") << "\""
		<< ",\n  \"neural_mode\": " << metadata.neuralMode
		<< ",\n  \"public_dlss_preset\": " << metadata.dlssPreset
		<< ",\n  \"overlay_policy\": " << metadata.overlayPolicy
		<< ",\n  \"overlay_profile\": \"" << OverlayProfileName(metadata.overlayProfile) << "\""
		<< ",\n  \"profile\": \"" << Json(metadata.profile) << "\""
		<< ",\n  \"external_settings\": \"" << Json(metadata.externalRecommendation) << "\""
		<< ",\n  \"evaluation_accepted\": " << (metadata.evaluationAccepted ? "true" : "false")
		<< ",\n  \"submit_status\": \"" << Json(metadata.submitStatus) << "\""
		<< ",\n  \"remake_input\": \"" << Json(metadata.remakeInput) << "\""
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
		<< ",\n  \"capture_gpu_timings_present\": "
		<< (metadata.gpuTimings.available ? "true" : "false")
		<< ",\n  \"capture_stalls_gpu\": true,\n  \"eligible_for_performance_metrics\": false\n}\n";
	if (!manifest) { error = "failed writing manifest.json"; return false; }

	previousFrameId_ = metadata.frameId;
	previousFinal_ = final;
	previousSource_ = source;
	if (lateOverlayProof_)
	{
		pendingLateOverlayFrameId_ = metadata.frameId;
		pendingLateOverlayContentRect_ = metadata.contentRect;
		pendingPreFlycastOverlayFull_ = finalFull;
	}
	++captured_;
	if (captured_ == limit_ && !lateOverlayProof_)
	{
		std::ofstream complete(root_ / "capture-complete.json");
		complete.imbue(std::locale::classic());
		complete << "{\n  \"schema\": 1,\n  \"git_sha\": \"" << GIT_HASH
			<< "\",\n  \"game_id\": \"" << Json(metadata.gameId)
			<< "\",\n  \"captured_frames\": " << captured_
			<< ",\n  \"status\": \"complete\"\n}\n";
		if (!complete) { error = "failed writing capture completion marker"; return false; }
	}
	pvrSnapshot_ = std::move(pendingSnapshot);
	if(pvrSnapshot_ && !pvrSnapshot_->sourceVertices.empty()) {
		RemakeViewScene scene;std::string conversionError;
		if(!remakePreparedBeforeComposite_) {
			if(BuildRemakeViewScene(*pvrSnapshot_,metadata.producerIdentity,metadata.frameId,scene,conversionError))
				remakeView_=std::move(scene);
			else remakePacketStatus_=conversionError;
		}
		if(remakeView_ && textures.remakeTextureReader) {
			remake::Packet packet;
			if((remakePreparedBeforeComposite_&&remakePacket_)||BuildRemakeViewPacket(*remakeView_,textures.remakeTextureReader,packet,conversionError)) {
				if(!remakePreparedBeforeComposite_) {remakePacket_=std::move(packet);remakePacketStatus_="owned-live-source-packet; consumer-not-connected";}
				if(!remakePreparedBeforeComposite_) ExchangeRemakePacket();
				// Archive the actual owned return for byte comparison with consumer output.
				if(remakeReturnedImage_&&remakeReturnedImage_->frame==metadata.frameId) {
					const auto& returned=*remakeReturnedImage_;
					std::ofstream receiptFile(frameRoot/"remake-return.json");
					receiptFile.imbue(std::locale::classic());
					receiptFile<<"{\"frame\":"<<returned.frame<<",\"sequence\":"<<returned.source.sequence
						<<",\"source_digest\":"<<returned.source.digest<<",\"pixel_bytes\":"<<returned.bgra.size()
						<<",\"prepared_before_composite\":"<<(remakePreparedBeforeComposite_?"true":"false")
						<<",\"input_origin\":\""<<(remakeInputReplayed_?"locked-replay":"live-channel")<<"\""
						<<",\"replay_original_frame\":"<<remakeReplayOriginalFrame_
						<<",\"depth_values\":"<<returned.projectionDepth.size()
						<<",\"presentation_proven\":false}\n";
					if(!receiptFile)remakePacketStatus_+="; return-receipt-write-failed";
					std::ofstream pixelsFile(frameRoot/"remake-return.bgra",std::ios::binary);
					pixelsFile.write(reinterpret_cast<const char*>(returned.bgra.data()),returned.bgra.size());
					if(!pixelsFile)remakePacketStatus_+="; return-pixels-write-failed";
					if(!returned.projectionDepth.empty()) {
						std::ofstream depthFile(frameRoot/"remake-return-depth.f32",std::ios::binary);
						depthFile.write(reinterpret_cast<const char*>(returned.projectionDepth.data()),returned.projectionDepth.size()*sizeof(float));
						if(!depthFile)remakePacketStatus_+="; return-depth-write-failed";
					}
					if(textures.remakeComposite) {
						ComPtr<ID3D11Texture2D> composite;RawTexture compositeRaw;
						if(!textures.remakeComposite(returned,composite,conversionError)
							||!ReadTexture(device,context,composite,compositeRaw,conversionError)
							||!WritePng(frameRoot/"remake-protected-composite.png",ToRgba(compositeRaw),conversionError))
							remakePacketStatus_+="; return-composite-failed="+conversionError;
						else {
							RawTexture nativeAfter,finalAfter;
							const bool unchanged=ReadTexture(device,context,textures.nativeColor,nativeAfter,conversionError)
								&&ReadTexture(device,context,textures.finalComposite,finalAfter,conversionError)
								&&nativeAfter.format==nativeRaw.format&&nativeAfter.width==nativeRaw.width
								&&nativeAfter.height==nativeRaw.height&&nativeAfter.bytes==nativeRaw.bytes
								&&finalAfter.format==finalRaw.format&&finalAfter.width==finalRaw.width
								&&finalAfter.height==finalRaw.height&&finalAfter.bytes==finalRaw.bytes;
							std::ofstream proof(frameRoot/"remake-composite-native-proof.json");proof.imbue(std::locale::classic());
							proof<<"{\"frame\":"<<returned.frame<<",\"native_targets_unchanged\":"
								<<(unchanged?"true":"false")<<",\"presentation_proven\":false}\n";
							if(!unchanged||!proof)remakePacketStatus_+="; return-composite-native-proof-failed";
						}
					}
				}
				// The consumer never reads this archive; failure cannot stall the channel.
				if(!WriteRemakeViewPacket(frameRoot/"remake-view.bin",*remakePacket_,conversionError))
					remakePacketStatus_+="; archive-failed="+conversionError;
			} else remakePacketStatus_=conversionError;
		}
	}
	return true;
}

bool QualityCaptureWriter::CapturePresentedWithFlycastOverlays(ID3D11Device *device,
	ID3D11DeviceContext *context, ID3D11Texture2D *presented,
	std::uint64_t frameId, const Rect& contentRect, std::string& error)
{
	if (!lateOverlayProof_ || pendingLateOverlayFrameId_ == 0)
		return true;
	if (frameId != pendingLateOverlayFrameId_ || !device || !context || !presented)
	{
		error = "late-overlay capture does not match the pending neural frame";
		return false;
	}
	RawTexture presentedRaw;
	if (!ReadTexture(device, context, presented, presentedRaw, error))
		return false;
	const auto presentedFull = ToRgba(presentedRaw);
	if (presentedFull.width != pendingPreFlycastOverlayFull_.width
		|| presentedFull.height != pendingPreFlycastOverlayFull_.height
		|| presentedFull.pixels.size() != pendingPreFlycastOverlayFull_.pixels.size())
	{
		error = "late-overlay presented and pre-overlay dimensions disagree";
		return false;
	}
	if (contentRect.x != pendingLateOverlayContentRect_.x
		|| contentRect.y != pendingLateOverlayContentRect_.y
		|| contentRect.width != pendingLateOverlayContentRect_.width
		|| contentRect.height != pendingLateOverlayContentRect_.height)
	{
		error = "late-overlay content rectangle changed within one frame";
		return false;
	}

	std::uint64_t changedPixels = 0;
	std::uint64_t contentChangedPixels = 0;
	std::uint8_t maxDelta = 0;
	for (std::uint32_t y = 0; y < presentedFull.height; ++y)
		for (std::uint32_t x = 0; x < presentedFull.width; ++x)
		{
			const auto pixel = static_cast<std::size_t>(y) * presentedFull.width + x;
			bool changed = false;
			for (std::size_t channel = 0; channel < 4; ++channel)
			{
				const auto before = pendingPreFlycastOverlayFull_.pixels[pixel * 4 + channel];
				const auto after = presentedFull.pixels[pixel * 4 + channel];
				const auto delta = static_cast<std::uint8_t>(std::abs(int(before) - int(after)));
				maxDelta = (std::max)(maxDelta, delta);
				changed = changed || delta != 0;
			}
			if (!changed) continue;
			++changedPixels;
			if (x >= static_cast<std::uint32_t>((std::max)(0, contentRect.x))
				&& y >= static_cast<std::uint32_t>((std::max)(0, contentRect.y))
				&& x < static_cast<std::uint32_t>((std::max)(0, contentRect.x + contentRect.width))
				&& y < static_cast<std::uint32_t>((std::max)(0, contentRect.y + contentRect.height)))
				++contentChangedPixels;
		}

	std::ostringstream frameName;
	frameName.imbue(std::locale::classic());
	frameName << "frame-" << std::setw(6) << std::setfill('0') << frameId;
	const auto frameRoot = root_ / frameName.str();
	const auto presentedContent = Crop(presentedFull, contentRect);
	const auto difference = Difference(pendingPreFlycastOverlayFull_, presentedFull);
	if (!WritePng(frameRoot / "presented-with-flycast-overlays.png",
		presentedContent, error)
		|| !WritePng(frameRoot / "flycast-overlay-difference.png", difference, error))
		return false;

	const bool passed = changedPixels != 0 && contentChangedPixels != 0;
	std::ofstream proof(frameRoot / "late-overlay-proof.json");
	proof.imbue(std::locale::classic());
	proof << "{\n  \"schema\": 1,\n  \"frame_id\": " << frameId
		<< ",\n  \"pre_overlay_artifact\": \"final-composited.png\""
		<< ",\n  \"presented_artifact\": \"presented-with-flycast-overlays.png\""
		<< ",\n  \"difference_artifact\": \"flycast-overlay-difference.png\""
		<< ",\n  \"pre_overlay_boundary\": \"after neural scene and protected game overlay composite\""
		<< ",\n  \"presented_boundary\": \"after gui_display_osd ImGui draw data\""
		<< ",\n  \"changed_pixels_full_backbuffer\": " << changedPixels
		<< ",\n  \"changed_pixels_content_rect\": " << contentChangedPixels
		<< ",\n  \"max_channel_delta\": " << unsigned(maxDelta)
		<< ",\n  \"neural_buffer_contains_flycast_overlay\": false"
		<< ",\n  \"passed\": " << (passed ? "true" : "false")
		<< "\n}\n";
	if (!proof)
	{
		error = "failed writing late-overlay proof";
		return false;
	}

	pendingLateOverlayFrameId_ = 0;
	pendingPreFlycastOverlayFull_ = {};
	++lateOverlayCaptured_;
	if (lateOverlayCaptured_ == limit_)
	{
		std::ofstream complete(root_ / "capture-complete.json");
		complete.imbue(std::locale::classic());
		complete << "{\n  \"schema\": 1,\n  \"git_sha\": \"" << GIT_HASH
			<< "\",\n  \"captured_frames\": " << captured_
			<< ",\n  \"late_overlay_proof_frames\": " << lateOverlayCaptured_
			<< ",\n  \"late_overlay_proof_requested\": true"
			<< ",\n  \"status\": \"complete\"\n}\n";
		if (!complete)
		{
			error = "failed writing late-overlay capture completion marker";
			return false;
		}
	}
	return true;
}

} // namespace flycast::rend::neural
