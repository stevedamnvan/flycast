// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/motion_reference.h"

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
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

// Large enough to satisfy the public DLAA feature's minimum input contract while
// retaining broad constant-color interiors for exposure/channel measurements.
constexpr UINT Width = 640;
constexpr UINT Height = 320;

struct QuadVertex {
	float position[2];
	float uv[2];
};

std::string HrText(const char *operation, HRESULT hr)
{
	std::ostringstream out;
	out << operation << " failed (HRESULT 0x" << std::hex << std::uppercase
		<< static_cast<unsigned long>(hr) << ')';
	return out.str();
}

bool ExtractRawString(const std::string& source, const char *symbol, std::string& value)
{
	const std::string marker = std::string(symbol) + " = R\"(";
	const auto begin = source.find(marker);
	if (begin == std::string::npos) return false;
	const auto contentBegin = begin + marker.size();
	const auto end = source.find(")\";", contentBegin);
	if (end == std::string::npos) return false;
	value = source.substr(contentBegin, end - contentBegin);
	return true;
}

bool Compile(const std::string& source, const char *entry, const char *target,
	const D3D_SHADER_MACRO *macros, ComPtr<ID3DBlob>& code, std::string& error)
{
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT hr = D3DCompile(source.data(), source.size(), "production-quad-shader",
		macros, nullptr, entry, target, D3DCOMPILE_ENABLE_STRICTNESS |
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr)) return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : HrText("compile production quad shader", hr);
	return false;
}

void SetPixel(Image& image, UINT x, UINT y, std::array<std::uint8_t, 4> rgba)
{
	const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
	std::copy(rgba.begin(), rgba.end(), image.rgba.begin() + offset);
}

Image MakeChart()
{
	Image image;
	image.width = Width;
	image.height = Height;
	image.rgba.resize(static_cast<std::size_t>(Width) * Height * 4);
	const std::array<std::array<std::uint8_t, 4>, 6> patches{{
		{{255,0,0,255}}, {{0,255,0,255}}, {{0,0,255,255}},
		{{0,255,255,255}}, {{255,0,255,255}}, {{255,255,0,255}}
	}};
	const std::uint8_t steps[] = {0,1,2,253,254,255};
	for (UINT y = 0; y < Height; ++y)
		for (UINT x = 0; x < Width; ++x)
		{
			if (y < Height / 4)
			{
				const auto value = static_cast<std::uint8_t>((x * 255u + (Width - 1) / 2) /
					(Width - 1));
				SetPixel(image, x, y, {value,value,value,255});
			}
			else if (y < Height / 2)
				SetPixel(image, x, y, patches[(x * patches.size()) / Width]);
			else if (y < Height * 3 / 4)
			{
				const auto value = steps[(x * std::size(steps)) / Width];
				SetPixel(image, x, y, {value,value,value,255});
			}
			else if (y < Height * 7 / 8)
			{
				const auto alpha = static_cast<std::uint8_t>((x * 255u + (Width - 1) / 2) /
					(Width - 1));
				SetPixel(image, x, y, {200,100,50,alpha});
			}
			else
			{
				const std::uint8_t value = ((x / 2 + y / 2) & 1u) ? 255 : 0;
				SetPixel(image, x, y, {value,value,value,255});
			}
		}
	return image;
}

std::string AdapterName(ID3D11Device *device)
{
	ComPtr<IDXGIDevice> dxgi;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC desc{};
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(dxgi.GetAddressOf()))) ||
		FAILED(dxgi->GetAdapter(adapter.GetAddressOf())) || FAILED(adapter->GetDesc(&desc)))
		return "unknown";
	char name[256]{};
	WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name,
		static_cast<int>(std::size(name)), nullptr, nullptr);
	return name;
}

bool Readback(ID3D11Device *device, ID3D11DeviceContext *context,
	ID3D11Texture2D *source, Image& image, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create color-contract readback", hr); return false; }
	context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) { error = HrText("map color-contract readback", hr); return false; }
	image.width = desc.Width;
	image.height = desc.Height;
	image.rgba.resize(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
	for (UINT y = 0; y < desc.Height; ++y)
		std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * desc.Width * 4,
			static_cast<const std::uint8_t *>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch,
			static_cast<std::size_t>(desc.Width) * 4);
	context->Unmap(staging.Get(), 0);
	return true;
}

bool ExactContentRects()
{
	using flycast::rend::neural::ComputeContentRect;
	const auto hd = ComputeContentRect(1920, 1080, 4.f / 3.f, false, 480);
	const auto qhd = ComputeContentRect(2560, 1440, 4.f / 3.f, false, 480);
	const auto uhd = ComputeContentRect(3840, 2160, 4.f / 3.f, false, 480);
	const auto wide = ComputeContentRect(3840, 2160, 16.f / 9.f, false, 480);
	if (hd.x != 240 || hd.width != 1440 || hd.height != 1080 ||
		qhd.x != 320 || qhd.width != 1920 || qhd.height != 1440 ||
		uhd.x != 480 || uhd.width != 2880 || uhd.height != 2160 ||
		wide.x != 0 || wide.width != 3840 || wide.height != 2160)
		return false;
	for (std::uint32_t width = 301; width <= 2049; width += 37)
		for (std::uint32_t height = 239; height <= 1201; height += 29)
		{
			const auto rect = ComputeContentRect(width, height, 4.f / 3.f, false, 480);
			if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 ||
				2 * rect.x + rect.width != static_cast<std::int32_t>(width) ||
				2 * rect.y + rect.height != static_cast<std::int32_t>(height))
				return false;
		}
	return true;
}

} // namespace

bool RunColorContractFixture(ColorContractResult& result, std::string& error)
{
	result.source = MakeChart();
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL selected{};
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
		D3D11_SDK_VERSION, device.GetAddressOf(), &selected, context.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create color-contract D3D11 device", hr); return false; }
	result.adapter = AdapterName(device.Get());
	std::ifstream input(std::string(NEURAL_SOURCE_DIR) + "/core/rend/dx11/dx11_shaders.cpp",
		std::ios::binary);
	if (!input) { error = "cannot open production dx11_shaders.cpp"; return false; }
	std::ostringstream stream;
	stream << input.rdbuf();
	std::string vertexSource;
	std::string pixelSource;
	if (!ExtractRawString(stream.str(), "QuadVertexShader", vertexSource) ||
		!ExtractRawString(stream.str(), "QuadPixelShader", pixelSource))
	{
		error = "cannot extract production quad shader raw strings";
		return false;
	}
	D3D_SHADER_MACRO macros[] = {{"ROTATE", "0"}, {nullptr, nullptr}};
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	if (!Compile(vertexSource, "main", "vs_4_0", macros, vsCode, error) ||
		!Compile(pixelSource, "main", "ps_4_0", nullptr, psCode, error))
		return false;
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	hr = device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		nullptr, vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, ps.GetAddressOf());
	const D3D11_INPUT_ELEMENT_DESC elements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ComPtr<ID3D11InputLayout> layout;
	if (SUCCEEDED(hr)) hr = device->CreateInputLayout(elements, static_cast<UINT>(std::size(elements)),
		vsCode->GetBufferPointer(), vsCode->GetBufferSize(), layout.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create production quad pipeline", hr); return false; }
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = Width;
	textureDesc.Height = Height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = result.source.rgba.data();
	initial.SysMemPitch = Width * 4;
	ComPtr<ID3D11Texture2D> sourceTexture;
	ComPtr<ID3D11ShaderResourceView> sourceView;
	hr = device->CreateTexture2D(&textureDesc, &initial, sourceTexture.GetAddressOf());
	if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(sourceTexture.Get(), nullptr,
		sourceView.GetAddressOf());
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	ComPtr<ID3D11Texture2D> outputTexture;
	ComPtr<ID3D11RenderTargetView> outputTarget;
	if (SUCCEEDED(hr)) hr = device->CreateTexture2D(&textureDesc, nullptr, outputTexture.GetAddressOf());
	if (SUCCEEDED(hr)) hr = device->CreateRenderTargetView(outputTexture.Get(), nullptr,
		outputTarget.GetAddressOf());
	const QuadVertex vertices[] = {
		{{-1,1},{0,0}}, {{1,1},{1,0}}, {{-1,-1},{0,1}}, {{1,-1},{1,1}}
	};
	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.ByteWidth = sizeof(vertices);
	vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	initial.pSysMem = vertices;
	ComPtr<ID3D11Buffer> vertexBuffer;
	if (SUCCEEDED(hr)) hr = device->CreateBuffer(&vertexDesc, &initial, vertexBuffer.GetAddressOf());
	const float white[] = {1,1,1,1};
	D3D11_BUFFER_DESC constantDesc{};
	constantDesc.ByteWidth = sizeof(white);
	constantDesc.Usage = D3D11_USAGE_IMMUTABLE;
	constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	initial.pSysMem = white;
	ComPtr<ID3D11Buffer> constants;
	if (SUCCEEDED(hr)) hr = device->CreateBuffer(&constantDesc, &initial, constants.GetAddressOf());
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	ComPtr<ID3D11SamplerState> sampler;
	if (SUCCEEDED(hr)) hr = device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create color-contract resources", hr); return false; }
	const float clear[] = {0,0,0,0};
	context->ClearRenderTargetView(outputTarget.Get(), clear);
	ID3D11RenderTargetView *target = outputTarget.Get();
	context->OMSetRenderTargets(1, &target, nullptr);
	D3D11_VIEWPORT viewport{0,0,static_cast<float>(Width),static_cast<float>(Height),0,1};
	context->RSSetViewports(1, &viewport);
	const UINT stride = sizeof(QuadVertex), offset = 0;
	ID3D11Buffer *vb = vertexBuffer.Get();
	context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	context->IASetInputLayout(layout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	context->VSSetShader(vs.Get(), nullptr, 0);
	context->PSSetShader(ps.Get(), nullptr, 0);
	ID3D11ShaderResourceView *srv = sourceView.Get();
	context->PSSetShaderResources(0, 1, &srv);
	ID3D11SamplerState *samplerPtr = sampler.Get();
	context->PSSetSamplers(0, 1, &samplerPtr);
	ID3D11Buffer *constant = constants.Get();
	context->PSSetConstantBuffers(0, 1, &constant);
	context->Draw(4, 0);
	context->Flush();
	if (!Readback(device.Get(), context.Get(), outputTexture.Get(), result.roundTrip, error))
		return false;
	ComputePsnr(result.source, result.roundTrip, result.differingPixels, result.maxDelta);
	result.byteExact = result.source.rgba == result.roundTrip.rgba;
	auto pixel = [&](UINT x, UINT y) {
		const auto offset = (static_cast<std::size_t>(y) * Width + x) * 4;
		return std::array<std::uint8_t,4>{result.roundTrip.rgba[offset],
			result.roundTrip.rgba[offset+1], result.roundTrip.rgba[offset+2],
			result.roundTrip.rgba[offset+3]};
	};
	result.channelsExact = pixel(53, 100) == std::array<std::uint8_t,4>{255,0,0,255}
		&& pixel(160, 100) == std::array<std::uint8_t,4>{0,255,0,255}
		&& pixel(267, 100) == std::array<std::uint8_t,4>{0,0,255,255}
		&& pixel(373, 100) == std::array<std::uint8_t,4>{0,255,255,255}
		&& pixel(480, 100) == std::array<std::uint8_t,4>{255,0,255,255}
		&& pixel(587, 100) == std::array<std::uint8_t,4>{255,255,0,255};
	result.grayscaleExact = pixel(0, 30)[0] == 0 && pixel(639, 30)[0] == 255
		&& pixel(53, 180)[0] == 0 && pixel(160, 180)[0] == 1
		&& pixel(267, 180)[0] == 2 && pixel(373, 180)[0] == 253
		&& pixel(480, 180)[0] == 254 && pixel(587, 180)[0] == 255;
	result.alphaIndependent = pixel(0, 260) == std::array<std::uint8_t,4>{200,100,50,0}
		&& pixel(639, 260) == std::array<std::uint8_t,4>{200,100,50,255}
		&& pixel(319, 260)[0] == 200 && pixel(319, 260)[1] == 100
		&& pixel(319, 260)[2] == 50;
	result.contentRectsExact = ExactContentRects();
	return result.byteExact && result.channelsExact && result.grayscaleExact
		&& result.alphaIndependent && result.contentRectsExact;
}

} // namespace neuraltest
