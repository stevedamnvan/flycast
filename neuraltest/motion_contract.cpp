// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

constexpr UINT Width = 128;
constexpr UINT Height = 96;

struct MotionVertex {
	float current[2];
	float previous[2];
	float uv[2];
};

struct RenderedMotion {
	Image color;
	std::vector<std::uint16_t> motion;
	std::vector<float> motionFloat;
};

struct GpuContext {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11InputLayout> layout;
	ComPtr<ID3D11RasterizerState> raster;
	std::string adapter;
};

std::string HrText(const char *operation, HRESULT hr)
{
	std::ostringstream out;
	out << operation << " failed (HRESULT 0x" << std::hex << std::uppercase
		<< static_cast<unsigned long>(hr) << ')';
	return out.str();
}

float HalfToFloat(std::uint16_t half)
{
	const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
	std::uint32_t exponent = (half >> 10) & 0x1fu;
	std::uint32_t mantissa = half & 0x3ffu;
	std::uint32_t bits = 0;
	if (exponent == 0)
	{
		if (mantissa == 0)
			bits = sign;
		else
		{
			exponent = 127 - 15 + 1;
			while ((mantissa & 0x400u) == 0) { mantissa <<= 1; --exponent; }
			mantissa &= 0x3ffu;
			bits = sign | (exponent << 23) | (mantissa << 13);
		}
	}
	else if (exponent == 31)
		bits = sign | 0x7f800000u | (mantissa << 13);
	else
		bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
	float value = 0.f;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

bool Compile(const char *source, const char *entry, const char *target,
	ComPtr<ID3DBlob>& code, std::string& error)
{
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT hr = D3DCompile(source, std::strlen(source), "motion-contract",
		nullptr, nullptr, entry, target, D3DCOMPILE_ENABLE_STRICTNESS |
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr))
		return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : HrText("compile motion-contract shader", hr);
	return false;
}

bool Initialize(GpuContext& gpu, std::string& error)
{
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL selected{};
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
		D3D11_SDK_VERSION, gpu.device.GetAddressOf(), &selected, gpu.context.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create motion-contract D3D11 device", hr); return false; }
	ComPtr<IDXGIDevice> dxgi;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC adapterDesc{};
	if (SUCCEEDED(gpu.device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(adapter.GetAddressOf())) &&
		SUCCEEDED(adapter->GetDesc(&adapterDesc)))
	{
		char name[256]{};
		WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, name,
			static_cast<int>(std::size(name)), nullptr, nullptr);
		gpu.adapter = name;
	}
	static const char shader[] = R"(
struct VSIn { float2 current : POSITION0; float2 previous : POSITION1; float2 uv : TEXCOORD0; };
struct VSOut { float4 position : SV_POSITION; float2 current : TEXCOORD0; float2 previous : TEXCOORD1; float2 uv : TEXCOORD2; };
cbuffer Params : register(b0) { float2 rasterJitter; float motionMultiplier; float pad; };
VSOut VSMain(VSIn i) {
  VSOut o;
  float2 raster = i.current + rasterJitter;
  o.position=float4(raster.x / 64.0 - 1.0, 1.0 - raster.y / 48.0, 0, 1);
  o.current=i.current; o.previous=i.previous; o.uv=i.uv; return o;
}
struct PSOut { float4 color : SV_TARGET0; float2 motion : SV_TARGET1; float depth : SV_TARGET2; };
PSOut PSMain(VSOut i) {
  PSOut o;
  float2 cell=floor(i.uv * float2(31,23));
  float pattern=fmod(cell.x * 13 + cell.y * 7, 29) / 28.0;
  o.color=float4(i.uv.x, pattern, i.uv.y, 1);
  o.motion=(i.previous-i.current) * motionMultiplier;
  o.depth=log2(1.0 + 100000.0 * 0.0025) / 34.0;
  return o;
}
)";
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	if (!Compile(shader, "VSMain", "vs_5_0", vsCode, error) ||
		!Compile(shader, "PSMain", "ps_5_0", psCode, error))
		return false;
	hr = gpu.device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		nullptr, gpu.vertexShader.GetAddressOf());
	if (SUCCEEDED(hr)) hr = gpu.device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, gpu.pixelShader.GetAddressOf());
	const D3D11_INPUT_ELEMENT_DESC elements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"POSITION", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	if (SUCCEEDED(hr)) hr = gpu.device->CreateInputLayout(elements,
		static_cast<UINT>(std::size(elements)), vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		gpu.layout.GetAddressOf());
	D3D11_RASTERIZER_DESC raster{};
	raster.FillMode = D3D11_FILL_SOLID;
	raster.CullMode = D3D11_CULL_NONE;
	raster.DepthClipEnable = TRUE;
	if (SUCCEEDED(hr)) hr = gpu.device->CreateRasterizerState(&raster, gpu.raster.GetAddressOf());
	if (SUCCEEDED(hr))
		return true;
	error = HrText("create motion-contract pipeline", hr);
	return false;
}

std::array<MotionVertex, 4> Quad(float currentLeft, float currentTop, float width, float height,
	float previousLeft, float previousTop)
{
	return {{{{currentLeft,currentTop},{previousLeft,previousTop},{0,0}},
		{{currentLeft+width,currentTop},{previousLeft+width,previousTop},{1,0}},
		{{currentLeft,currentTop+height},{previousLeft,previousTop+height},{0,1}},
		{{currentLeft+width,currentTop+height},{previousLeft+width,previousTop+height},{1,1}}}};
}

bool CreateTexture(ID3D11Device *device, DXGI_FORMAT format, UINT bind,
	ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& target,
	std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = Width;
	desc.Height = Height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.BindFlags = bind;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
	if (SUCCEEDED(hr) && (bind & D3D11_BIND_RENDER_TARGET) != 0)
		hr = device->CreateRenderTargetView(texture.Get(), nullptr, target.GetAddressOf());
	if (SUCCEEDED(hr)) return true;
	error = HrText("create motion-contract target", hr);
	return false;
}

bool ReadColor(GpuContext& gpu, ID3D11Texture2D *source, Image& image, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = gpu.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create motion color readback", hr); return false; }
	gpu.context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = gpu.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) { error = HrText("map motion color readback", hr); return false; }
	image.width = Width;
	image.height = Height;
	image.rgba.resize(static_cast<std::size_t>(Width) * Height * 4);
	for (UINT y = 0; y < Height; ++y)
		std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * Width * 4,
			static_cast<const std::uint8_t *>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch,
			static_cast<std::size_t>(Width) * 4);
	gpu.context->Unmap(staging.Get(), 0);
	return true;
}

bool ReadMotion(GpuContext& gpu, ID3D11Texture2D *source, RenderedMotion& output,
	std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = gpu.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create motion readback", hr); return false; }
	gpu.context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = gpu.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) { error = HrText("map motion readback", hr); return false; }
	output.motion.resize(static_cast<std::size_t>(Width) * Height * 2);
	output.motionFloat.resize(output.motion.size());
	for (UINT y = 0; y < Height; ++y)
	{
		const auto *sourceRow = reinterpret_cast<const std::uint16_t *>(
			static_cast<const std::uint8_t *>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch);
		auto *destination = output.motion.data() + static_cast<std::size_t>(y) * Width * 2;
		std::memcpy(destination, sourceRow, static_cast<std::size_t>(Width) * 4);
		for (UINT x = 0; x < Width * 2; ++x)
			output.motionFloat[(static_cast<std::size_t>(y) * Width * 2) + x] = HalfToFloat(destination[x]);
	}
	gpu.context->Unmap(staging.Get(), 0);
	return true;
}

bool Render(GpuContext& gpu, const std::vector<MotionVertex>& vertexData, float jitterX,
	float jitterY, float multiplier, RenderedMotion& output, std::string& error)
{
	if (vertexData.size() < 3)
	{
		error = "motion-contract geometry is empty";
		return false;
	}
	ComPtr<ID3D11Texture2D> color;
	ComPtr<ID3D11RenderTargetView> colorTarget;
	ComPtr<ID3D11Texture2D> motion;
	ComPtr<ID3D11RenderTargetView> motionTarget;
	ComPtr<ID3D11Texture2D> depth;
	ComPtr<ID3D11RenderTargetView> depthTarget;
	if (!CreateTexture(gpu.device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, color, colorTarget, error) ||
		!CreateTexture(gpu.device.Get(), DXGI_FORMAT_R16G16_FLOAT,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, motion, motionTarget, error) ||
		!CreateTexture(gpu.device.Get(), DXGI_FORMAT_R32_FLOAT,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, depth, depthTarget, error))
		return false;
	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.ByteWidth = static_cast<UINT>(vertexData.size() * sizeof(MotionVertex));
	vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = vertexData.data();
	ComPtr<ID3D11Buffer> vertices;
	HRESULT hr = gpu.device->CreateBuffer(&vertexDesc, &initial, vertices.GetAddressOf());
	struct Params { float jitter[2]; float multiplier; float pad; } params{{jitterX,jitterY},multiplier,0};
	D3D11_BUFFER_DESC constantDesc{};
	constantDesc.ByteWidth = sizeof(Params);
	constantDesc.Usage = D3D11_USAGE_IMMUTABLE;
	constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	initial.pSysMem = &params;
	ComPtr<ID3D11Buffer> constants;
	if (SUCCEEDED(hr)) hr = gpu.device->CreateBuffer(&constantDesc, &initial, constants.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create motion-contract buffers", hr); return false; }
	const float clear[] = {0,0,0,0};
	gpu.context->ClearRenderTargetView(colorTarget.Get(), clear);
	gpu.context->ClearRenderTargetView(motionTarget.Get(), clear);
	gpu.context->ClearRenderTargetView(depthTarget.Get(), clear);
	ID3D11RenderTargetView *targets[] = {colorTarget.Get(), motionTarget.Get(), depthTarget.Get()};
	gpu.context->OMSetRenderTargets(static_cast<UINT>(std::size(targets)), targets, nullptr);
	D3D11_VIEWPORT viewport{0,0,static_cast<float>(Width),static_cast<float>(Height),0,1};
	gpu.context->RSSetViewports(1, &viewport);
	gpu.context->RSSetState(gpu.raster.Get());
	const UINT stride = sizeof(MotionVertex), offset = 0;
	ID3D11Buffer *vertexBuffer = vertices.Get();
	gpu.context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	gpu.context->IASetInputLayout(gpu.layout.Get());
	gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	gpu.context->VSSetShader(gpu.vertexShader.Get(), nullptr, 0);
	ID3D11Buffer *constantBuffer = constants.Get();
	gpu.context->VSSetConstantBuffers(0, 1, &constantBuffer);
	gpu.context->PSSetConstantBuffers(0, 1, &constantBuffer);
	gpu.context->PSSetShader(gpu.pixelShader.Get(), nullptr, 0);
	gpu.context->Draw(static_cast<UINT>(vertexData.size()), 0);
	gpu.context->Flush();
	return ReadColor(gpu, color.Get(), output.color, error) &&
		ReadMotion(gpu, motion.Get(), output, error);
}

std::array<float, 2> MotionAt(const RenderedMotion& rendered, UINT x, UINT y)
{
	const auto offset = (static_cast<std::size_t>(y) * Width + x) * 2;
	return {rendered.motionFloat[offset], rendered.motionFloat[offset + 1]};
}

double ReprojectionError(const Image& previous, const RenderedMotion& current)
{
	double error = 0.;
	std::uint64_t samples = 0;
	for (UINT y = 0; y < Height; ++y)
		for (UINT x = 0; x < Width; ++x)
		{
			const auto currentOffset = (static_cast<std::size_t>(y) * Width + x) * 4;
			if (current.color.rgba[currentOffset + 3] == 0)
				continue;
			const auto motion = MotionAt(current, x, y);
			const int previousX = static_cast<int>(std::lround(static_cast<float>(x) + motion[0]));
			const int previousY = static_cast<int>(std::lround(static_cast<float>(y) + motion[1]));
			if (previousX < 0 || previousY < 0 || previousX >= static_cast<int>(Width)
				|| previousY >= static_cast<int>(Height))
				continue;
			const auto previousOffset = (static_cast<std::size_t>(previousY) * Width + previousX) * 4;
			if (previous.rgba[previousOffset + 3] == 0)
				continue;
			for (int channel = 0; channel < 3; ++channel)
				error += std::abs(static_cast<int>(current.color.rgba[currentOffset + channel]) -
					static_cast<int>(previous.rgba[previousOffset + channel]));
			++samples;
		}
	return samples == 0 ? std::numeric_limits<double>::infinity() :
		error / static_cast<double>(samples * 3);
}

std::array<float, 3> Barycentric(float x, float y, const std::array<MotionVertex, 3>& triangle)
{
	const float x0 = triangle[0].current[0], y0 = triangle[0].current[1];
	const float x1 = triangle[1].current[0], y1 = triangle[1].current[1];
	const float x2 = triangle[2].current[0], y2 = triangle[2].current[1];
	const float denominator = (y1-y2)*(x0-x2)+(x2-x1)*(y0-y2);
	const float a = ((y1-y2)*(x-x2)+(x2-x1)*(y-y2))/denominator;
	const float b = ((y2-y0)*(x-x2)+(x0-x2)*(y-y2))/denominator;
	return {a,b,1.f-a-b};
}

} // namespace

bool RunMotionContractFixture(MotionContractResult& result, std::string& error)
{
	GpuContext gpu;
	if (!Initialize(gpu, error))
		return false;
	result.adapter = gpu.adapter;
	auto renderQuad = [&](float currentLeft, float currentTop, float previousLeft,
		float previousTop, float jitterX, float jitterY, float multiplier,
		RenderedMotion& rendered) {
		const auto quad = Quad(currentLeft, currentTop, 48, 48, previousLeft, previousTop);
		return Render(gpu, std::vector<MotionVertex>(quad.begin(), quad.end()), jitterX,
			jitterY, multiplier, rendered, error);
	};
	RenderedMotion staticMotion;
	RenderedMotion translated;
	RenderedMotion vertical;
	RenderedMotion camera;
	RenderedMotion jittered;
	RenderedMotion previous;
	RenderedMotion reversed;
	RenderedMotion doubled;
	if (!renderQuad(40, 20, 40, 20, 0, 0, 1, staticMotion) ||
		!renderQuad(40, 20, 36, 20, 0, 0, 1, translated) ||
		!renderQuad(40, 17, 40, 20, 0, 0, 1, vertical) ||
		!renderQuad(46, 18, 40, 20, 0, 0, 1, camera) ||
		!renderQuad(40, 20, 40, 20, .5f, -.25f, 1, jittered) ||
		!renderQuad(36, 20, 36, 20, 0, 0, 1, previous) ||
		!renderQuad(40, 20, 36, 20, 0, 0, -1, reversed) ||
		!renderQuad(40, 20, 36, 20, 0, 0, 2, doubled))
		return false;

	std::array<MotionVertex, 3> deformation{{
		{{30,20},{30,20},{0,0}}, {{100,25},{96,28},{1,0}}, {{50,80},{50,80},{0,1}}
	}};
	RenderedMotion deformed;
	if (!Render(gpu, std::vector<MotionVertex>(deformation.begin(), deformation.end()),
		0, 0, 1, deformed, error))
		return false;
	const UINT sampleX = 60, sampleY = 42;
	const auto staticValue = MotionAt(staticMotion, 64, 44);
	const auto translatedValue = MotionAt(translated, 64, 44);
	const auto verticalValue = MotionAt(vertical, 64, 40);
	const auto cameraValue = MotionAt(camera, 64, 42);
	const auto jitterValue = MotionAt(jittered, 64, 44);
	const auto deformationValue = MotionAt(deformed, sampleX, sampleY);
	const auto weights = Barycentric(sampleX + .5f, sampleY + .5f, deformation);
	std::array<float, 2> expectedDeformation{};
	for (std::size_t i = 0; i < deformation.size(); ++i)
	{
		expectedDeformation[0] += weights[i] *
			(deformation[i].previous[0] - deformation[i].current[0]);
		expectedDeformation[1] += weights[i] *
			(deformation[i].previous[1] - deformation[i].current[1]);
	}
	result.staticX = staticValue[0]; result.staticY = staticValue[1];
	result.translateX = translatedValue[0]; result.translateY = translatedValue[1];
	result.verticalX = verticalValue[0]; result.verticalY = verticalValue[1];
	result.cameraX = cameraValue[0]; result.cameraY = cameraValue[1];
	result.jitterX = jitterValue[0]; result.jitterY = jitterValue[1];
	result.deformationX = deformationValue[0]; result.deformationY = deformationValue[1];
	result.expectedDeformationX = expectedDeformation[0];
	result.expectedDeformationY = expectedDeformation[1];
	result.previousColor = previous.color;
	result.currentColor = translated.color;
	result.correctMotion = translated.motion;
	result.reversedMotion = reversed.motion;
	result.doubledMotion = doubled.motion;
	result.correctReprojectionError = ReprojectionError(previous.color, translated);
	result.reversedReprojectionError = ReprojectionError(previous.color, reversed);
	result.doubledReprojectionError = ReprojectionError(previous.color, doubled);
	const auto close = [](float a, float b, float tolerance = .02f) {
		return std::abs(a - b) <= tolerance;
	};
	result.analyticTruth = staticValue[0] == 0.f && staticValue[1] == 0.f
		&& close(translatedValue[0], -4.f) && translatedValue[1] == 0.f
		&& verticalValue[0] == 0.f && close(verticalValue[1], 3.f)
		&& close(cameraValue[0], -6.f) && close(cameraValue[1], 2.f)
		&& jitterValue[0] == 0.f && jitterValue[1] == 0.f
		&& close(deformationValue[0], expectedDeformation[0])
		&& close(deformationValue[1], expectedDeformation[1]);
	result.negativeControlsFail = std::isfinite(result.correctReprojectionError)
		&& result.reversedReprojectionError > result.correctReprojectionError + 1.
		&& result.doubledReprojectionError > result.correctReprojectionError + 1.;
	return result.analyticTruth && result.negativeControlsFail;
}

} // namespace neuraltest
