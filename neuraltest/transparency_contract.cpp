// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

constexpr UINT Width = 4;
constexpr UINT Height = 1;
constexpr std::uint32_t Eol = 0xffffffffu;

struct Surface {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D12Device> device12;
	ComPtr<ID3D12CommandQueue> queue12;
	std::string name;
	std::string adapter;
};

struct OitPixel {
	std::uint32_t color;
	float depth;
	std::uint32_t sequence;
	std::uint32_t next;
};

std::string HrText(const char *operation, HRESULT hr)
{
	std::ostringstream out;
	out << operation << " failed (HRESULT 0x" << std::hex << std::uppercase
		<< static_cast<unsigned long>(hr) << ')';
	return out.str();
}

std::string AdapterName(ID3D11Device *device)
{
	ComPtr<IDXGIDevice> dxgi;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC desc{};
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(dxgi.GetAddressOf())))
		|| FAILED(dxgi->GetAdapter(adapter.GetAddressOf()))
		|| FAILED(adapter->GetDesc(&desc))) return "unknown";
	char name[256]{};
	WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name,
		static_cast<int>(std::size(name)), nullptr, nullptr);
	return name;
}

bool CreateSurface(bool on12, Surface& surface, std::string& error)
{
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL selected{};
	HRESULT hr = S_OK;
	if (!on12)
	{
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
			D3D11_SDK_VERSION, surface.device.GetAddressOf(), &selected,
			surface.context.GetAddressOf());
		surface.name = "native-d3d11";
	}
	else
	{
		hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(surface.device12.GetAddressOf()));
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (SUCCEEDED(hr)) hr = surface.device12->CreateCommandQueue(&desc,
			IID_PPV_ARGS(surface.queue12.GetAddressOf()));
		IUnknown *queues[] = {surface.queue12.Get()};
		if (SUCCEEDED(hr)) hr = D3D11On12CreateDevice(surface.device12.Get(),
			D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
			queues, 1, 0, surface.device.GetAddressOf(), surface.context.GetAddressOf(),
			&selected);
		surface.name = "d3d11on12";
	}
	if (FAILED(hr)) { error = HrText("create transparency surface", hr); return false; }
	surface.adapter = AdapterName(surface.device.Get());
	return true;
}

bool ExtractRawString(const std::string& source, const char *symbol, std::string& value)
{
	const std::string marker = std::string(symbol) + " = R\"(";
	const auto begin = source.find(marker);
	if (begin == std::string::npos) return false;
	const auto content = begin + marker.size();
	const auto end = source.find(")\";", content);
	if (end == std::string::npos) return false;
	value = source.substr(content, end - content);
	return true;
}

class OitInclude final : public ID3DInclude
{
public:
	explicit OitInclude(const std::string& header) : header_(header) {}
	HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID,
		LPCVOID *data, UINT *bytes) override
	{
		if (std::strcmp(fileName, "oit_header.hlsl") != 0) return E_FAIL;
		*data = header_.data(); *bytes = static_cast<UINT>(header_.size()); return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }
private:
	const std::string& header_;
};

bool CreateTexture(ID3D11Device *device, DXGI_FORMAT format, UINT bindFlags,
	const void *data, UINT pitch, ComPtr<ID3D11Texture2D>& texture, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = Width; desc.Height = Height; desc.MipLevels = 1; desc.ArraySize = 1;
	desc.Format = format; desc.SampleDesc.Count = 1; desc.BindFlags = bindFlags;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = data; initial.SysMemPitch = pitch;
	const HRESULT hr = device->CreateTexture2D(&desc, data ? &initial : nullptr,
		texture.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create transparency texture", hr); return false; }
	return true;
}

bool ReadMask(ID3D11Device *device, ID3D11DeviceContext *context,
	ID3D11Texture2D *source, Image& image, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
	if (SUCCEEDED(hr)) context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(hr)) hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) { error = HrText("read transparency mask", hr); return false; }
	image.width = Width; image.height = Height; image.rgba.resize(Width * Height * 4);
	for (UINT x = 0; x < Width; ++x)
	{
		const auto value = static_cast<const std::uint8_t *>(mapped.pData)[x];
		for (int c = 0; c < 3; ++c) image.rgba[x * 4 + c] = value;
		image.rgba[x * 4 + 3] = 255;
	}
	context->Unmap(staging.Get(), 0);
	return true;
}

} // namespace

bool RunTransparencyContractFixture(bool d3d11On12,
	TransparencyContractResult& result, std::string& error)
{
	Surface surface;
	if (!CreateSurface(d3d11On12, surface, error)) return false;
	result.surface = surface.name; result.adapter = surface.adapter;

	std::ifstream input(std::string(NEURAL_SOURCE_DIR)
		+ "/core/rend/dx11/oit/dx11_oitshaders.cpp", std::ios::binary);
	std::ostringstream sourceStream;
	sourceStream << input.rdbuf();
	std::string header;
	std::string pixel;
	if (!input || !ExtractRawString(sourceStream.str(), "static const char OITShaderHeader[]", header)
		|| !ExtractRawString(sourceStream.str(), "static const char OITFinalShaderSource[]", pixel))
	{
		error = "cannot extract production OIT final shader";
		return false;
	}
	static const char vertex[] = R"(
float4 main(uint id : SV_VertexID) : SV_Position {
	float2 p[4] = {float2(-1,-1),float2(-1,1),float2(1,-1),float2(1,1)};
	return float4(p[id],0,1);
})";
	D3D_SHADER_MACRO macros[] = {
		{"MAX_PIXELS_PER_FRAGMENT", "8"}, {"DITHERING", "0"}, {nullptr, nullptr}
	};
	OitInclude includes(header);
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	ComPtr<ID3DBlob> diagnostics;
	HRESULT hr = D3DCompile(vertex, std::strlen(vertex), "transparency-vs", nullptr,
		nullptr, "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		vsCode.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr)) hr = D3DCompile(pixel.data(), pixel.size(), "production-oit-final",
		macros, &includes, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		psCode.GetAddressOf(), diagnostics.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
			diagnostics->GetBufferSize()) : HrText("compile transparency shaders", hr);
		return false;
	}
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	hr = surface.device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		nullptr, vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, ps.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create transparency shaders", hr); return false; }

	const std::array<std::uint32_t, Width> pointers = {Eol, 0, 1, 2};
	const std::array<OitPixel, 4> pixels = {{
		{0x40ffffffu, .5f, 0, Eol},
		{0x80ffffffu, .5f, 0, Eol},
		{0x20ffffffu, .6f, 0, 3},
		{0x60ffffffu, .4f, 0, Eol},
	}};
	const std::array<std::int32_t, 2> poly = {
		static_cast<std::int32_t>((1u << 29) | (1u << 26)), -1
	};
	const std::array<std::uint32_t, Width> opaque = {
		0xff202020u, 0xff202020u, 0xff202020u, 0xff202020u
	};
	ComPtr<ID3D11Texture2D> opaqueTexture;
	ComPtr<ID3D11Texture2D> pointerTexture;
	ComPtr<ID3D11Texture2D> colorTexture;
	ComPtr<ID3D11Texture2D> maskTexture;
	if (!CreateTexture(surface.device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_SHADER_RESOURCE, opaque.data(), Width * 4, opaqueTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R32_UINT,
		D3D11_BIND_UNORDERED_ACCESS, pointers.data(), Width * 4, pointerTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_RENDER_TARGET, nullptr, 0, colorTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM,
		D3D11_BIND_RENDER_TARGET, nullptr, 0, maskTexture, error)) return false;
	ComPtr<ID3D11ShaderResourceView> opaqueView;
	ComPtr<ID3D11UnorderedAccessView> pointerUav;
	ComPtr<ID3D11RenderTargetView> colorTarget;
	ComPtr<ID3D11RenderTargetView> maskTarget;
	hr = surface.device->CreateShaderResourceView(opaqueTexture.Get(), nullptr,
		opaqueView.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateUnorderedAccessView(pointerTexture.Get(),
		nullptr, pointerUav.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateRenderTargetView(colorTexture.Get(), nullptr,
		colorTarget.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateRenderTargetView(maskTexture.Get(), nullptr,
		maskTarget.GetAddressOf());

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = static_cast<UINT>(sizeof(pixels));
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(OitPixel);
	D3D11_SUBRESOURCE_DATA pixelData{pixels.data(), 0, 0};
	ComPtr<ID3D11Buffer> pixelBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&bufferDesc, &pixelData,
		pixelBuffer.GetAddressOf());
	D3D11_UNORDERED_ACCESS_VIEW_DESC pixelUavDesc{};
	pixelUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	pixelUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	pixelUavDesc.Buffer.NumElements = static_cast<UINT>(pixels.size());
	ComPtr<ID3D11UnorderedAccessView> pixelUav;
	if (SUCCEEDED(hr)) hr = surface.device->CreateUnorderedAccessView(pixelBuffer.Get(),
		&pixelUavDesc, pixelUav.GetAddressOf());

	bufferDesc.ByteWidth = static_cast<UINT>(sizeof(poly));
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = sizeof(std::int32_t) * 2;
	D3D11_SUBRESOURCE_DATA polyData{poly.data(), 0, 0};
	ComPtr<ID3D11Buffer> polyBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&bufferDesc, &polyData,
		polyBuffer.GetAddressOf());
	D3D11_SHADER_RESOURCE_VIEW_DESC polyViewDesc{};
	polyViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	polyViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	polyViewDesc.Buffer.NumElements = 1;
	ComPtr<ID3D11ShaderResourceView> polyView;
	if (SUCCEEDED(hr)) hr = surface.device->CreateShaderResourceView(polyBuffer.Get(),
		&polyViewDesc, polyView.GetAddressOf());

	D3D11_BUFFER_DESC constantsDesc{};
	constantsDesc.ByteWidth = sizeof(float) * 24;
	constantsDesc.Usage = D3D11_USAGE_DEFAULT;
	constantsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	std::array<float, 24> constants{};
	constants[21] = 1.f;
	D3D11_SUBRESOURCE_DATA constantsData{constants.data(), 0, 0};
	ComPtr<ID3D11Buffer> constantBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&constantsDesc, &constantsData,
		constantBuffer.GetAddressOf());
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	ComPtr<ID3D11SamplerState> sampler;
	if (SUCCEEDED(hr)) hr = surface.device->CreateSamplerState(&samplerDesc,
		sampler.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create transparency resources", hr); return false; }

	ID3D11RenderTargetView *targets[] = {colorTarget.Get(), maskTarget.Get()};
	ID3D11UnorderedAccessView *uavs[] = {pixelUav.Get(), pointerUav.Get()};
	surface.context->OMSetRenderTargetsAndUnorderedAccessViews(2, targets, nullptr, 2, 2,
		uavs, nullptr);
	const float zero[4]{};
	surface.context->ClearRenderTargetView(maskTarget.Get(), zero);
	D3D11_VIEWPORT viewport{0, 0, static_cast<float>(Width), static_cast<float>(Height), 0, 1};
	surface.context->RSSetViewports(1, &viewport);
	surface.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	surface.context->VSSetShader(vs.Get(), nullptr, 0);
	surface.context->PSSetShader(ps.Get(), nullptr, 0);
	ID3D11ShaderResourceView *opaqueResource = opaqueView.Get();
	ID3D11ShaderResourceView *polyParams = polyView.Get();
	ID3D11Buffer *constantsBuffer = constantBuffer.Get();
	ID3D11SamplerState *samplerState = sampler.Get();
	surface.context->PSSetShaderResources(0, 1, &opaqueResource);
	surface.context->PSSetShaderResources(5, 1, &polyParams);
	surface.context->PSSetConstantBuffers(0, 1, &constantsBuffer);
	surface.context->PSSetSamplers(0, 1, &samplerState);
	surface.context->Draw(4, 0);
	surface.context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0,
		nullptr, nullptr);
	surface.context->Flush();
	if (!ReadMask(surface.device.Get(), surface.context.Get(), maskTexture.Get(),
		result.reactiveMask, error)) return false;
	const auto value = [&](UINT x) { return result.reactiveMask.rgba[x * 4]; };
	result.emptyAndModifierClear = value(0) == 0;
	result.singleLayerReactive = value(1) == 255 && value(2) == 255;
	result.multiLayerReactive = value(3) == 255;
	result.wrongControlFailed = result.singleLayerReactive && result.multiLayerReactive;
	return result.emptyAndModifierClear && result.singleLayerReactive
		&& result.multiLayerReactive && result.wrongControlFailed;
}

} // namespace neuraltest
