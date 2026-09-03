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

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

constexpr UINT Width = 16;
constexpr UINT Height = 8;

struct QuadVertex { float position[2]; float uv[2]; };
struct Surface {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D12Device> device12;
	ComPtr<ID3D12CommandQueue> queue12;
	std::string name;
	std::string adapter;
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
	if (FAILED(hr)) { error = HrText("create overlay surface", hr); return false; }
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

bool Readback(ID3D11Device *device, ID3D11DeviceContext *context,
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
	if (FAILED(hr)) { error = HrText("read overlay output", hr); return false; }
	image.width = desc.Width; image.height = desc.Height;
	image.rgba.resize(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
	for (UINT y = 0; y < desc.Height; ++y)
		std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * desc.Width * 4,
			static_cast<const std::uint8_t *>(mapped.pData)
				+ static_cast<std::size_t>(y) * mapped.RowPitch,
			static_cast<std::size_t>(desc.Width) * 4);
	context->Unmap(staging.Get(), 0);
	return true;
}

} // namespace

bool RunOverlayContractFixture(bool d3d11On12,
	OverlayContractResult& result, std::string& error)
{
	Surface surface;
	if (!CreateSurface(d3d11On12, surface, error)) return false;
	result.surface = surface.name; result.adapter = surface.adapter;
	std::ifstream input(std::string(NEURAL_SOURCE_DIR)
		+ "/core/rend/dx11/dx11_shaders.cpp", std::ios::binary);
	std::ostringstream sourceStream;
	sourceStream << input.rdbuf();
	std::string vertexSource;
	std::string pixelSource;
	if (!input || !ExtractRawString(sourceStream.str(), "QuadVertexShader", vertexSource)
		|| !ExtractRawString(sourceStream.str(), "NeuralOverlayCompositePixelShader", pixelSource))
	{
		error = "cannot extract production overlay shaders";
		return false;
	}
	D3D_SHADER_MACRO macros[] = {{"ROTATE", "0"}, {nullptr, nullptr}};
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	ComPtr<ID3DBlob> diagnostics;
	HRESULT hr = D3DCompile(vertexSource.data(), vertexSource.size(), "overlay-vs", macros,
		nullptr, "main", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		vsCode.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr)) hr = D3DCompile(pixelSource.data(), pixelSource.size(), "overlay-ps",
		nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		psCode.GetAddressOf(), diagnostics.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
			diagnostics->GetBufferSize()) : HrText("compile overlay shaders", hr);
		return false;
	}
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	hr = surface.device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		nullptr, vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, ps.GetAddressOf());
	const D3D11_INPUT_ELEMENT_DESC elements[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
	};
	ComPtr<ID3D11InputLayout> layout;
	if (SUCCEEDED(hr)) hr = surface.device->CreateInputLayout(elements,
		static_cast<UINT>(std::size(elements)), vsCode->GetBufferPointer(),
		vsCode->GetBufferSize(), layout.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create overlay pipeline", hr); return false; }

	result.original = {Width, Height, {}};
	result.neural = {Width, Height, {}};
	result.mask = {Width, Height, {}};
	const auto pixelCount = static_cast<std::size_t>(Width) * Height;
	result.original.rgba.resize(pixelCount * 4);
	result.neural.rgba.resize(pixelCount * 4);
	result.mask.rgba.resize(pixelCount * 4);
	std::vector<std::uint8_t> maskValues(pixelCount);
	for (std::size_t i = 0; i < pixelCount; ++i)
	{
		result.original.rgba[i * 4] = static_cast<std::uint8_t>(31 + i);
		result.original.rgba[i * 4 + 1] = static_cast<std::uint8_t>(211 - i);
		result.original.rgba[i * 4 + 2] = static_cast<std::uint8_t>(71 + i);
		result.original.rgba[i * 4 + 3] = 255;
		result.neural.rgba[i * 4] = static_cast<std::uint8_t>(180 - i);
		result.neural.rgba[i * 4 + 1] = static_cast<std::uint8_t>(20 + i);
		result.neural.rgba[i * 4 + 2] = static_cast<std::uint8_t>(240 - i);
		result.neural.rgba[i * 4 + 3] = 255;
		maskValues[i] = (i % 5 == 0 || (i / Width == 1 && i % Width < 8)) ? 255 : 0;
		result.mask.rgba[i * 4] = result.mask.rgba[i * 4 + 1]
			= result.mask.rgba[i * 4 + 2] = maskValues[i];
		result.mask.rgba[i * 4 + 3] = 255;
	}

	auto createTexture = [&](DXGI_FORMAT format, UINT bind, const void *data, UINT pitch,
		ComPtr<ID3D11Texture2D>& texture) {
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width=Width; desc.Height=Height; desc.MipLevels=1; desc.ArraySize=1;
		desc.Format=format; desc.SampleDesc.Count=1; desc.BindFlags=bind;
		D3D11_SUBRESOURCE_DATA initial{data,pitch,0};
		return surface.device->CreateTexture2D(&desc, data ? &initial : nullptr,
			texture.GetAddressOf());
	};
	ComPtr<ID3D11Texture2D> originalTexture;
	ComPtr<ID3D11Texture2D> maskTexture;
	ComPtr<ID3D11Texture2D> outputTexture;
	hr = createTexture(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE,
		result.original.rgba.data(), Width * 4, originalTexture);
	if (SUCCEEDED(hr)) hr = createTexture(DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE,
		maskValues.data(), Width, maskTexture);
	if (SUCCEEDED(hr)) hr = createTexture(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET,
		result.neural.rgba.data(), Width * 4, outputTexture);
	ComPtr<ID3D11ShaderResourceView> originalView;
	ComPtr<ID3D11ShaderResourceView> maskView;
	ComPtr<ID3D11RenderTargetView> outputTarget;
	if (SUCCEEDED(hr)) hr = surface.device->CreateShaderResourceView(originalTexture.Get(), nullptr,
		originalView.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateShaderResourceView(maskTexture.Get(), nullptr,
		maskView.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateRenderTargetView(outputTexture.Get(), nullptr,
		outputTarget.GetAddressOf());
	const QuadVertex vertices[] = {
		{{-1,1},{0,0}}, {{1,1},{1,0}}, {{-1,-1},{0,1}}, {{1,-1},{1,1}}
	};
	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.ByteWidth=sizeof(vertices); vertexDesc.Usage=D3D11_USAGE_IMMUTABLE;
	vertexDesc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData{vertices,0,0};
	ComPtr<ID3D11Buffer> vertexBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&vertexDesc, &vertexData,
		vertexBuffer.GetAddressOf());
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU=samplerDesc.AddressV=samplerDesc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD=D3D11_FLOAT32_MAX;
	ComPtr<ID3D11SamplerState> sampler;
	if (SUCCEEDED(hr)) hr = surface.device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create overlay resources", hr); return false; }

	ID3D11RenderTargetView *target = outputTarget.Get();
	surface.context->OMSetRenderTargets(1, &target, nullptr);
	D3D11_VIEWPORT viewport{0,0,static_cast<float>(Width),static_cast<float>(Height),0,1};
	surface.context->RSSetViewports(1, &viewport);
	const UINT stride=sizeof(QuadVertex), offset=0;
	ID3D11Buffer *vb=vertexBuffer.Get();
	surface.context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
	surface.context->IASetInputLayout(layout.Get());
	surface.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	surface.context->VSSetShader(vs.Get(),nullptr,0);
	surface.context->PSSetShader(ps.Get(),nullptr,0);
	ID3D11ShaderResourceView *views[]={originalView.Get(),maskView.Get()};
	surface.context->PSSetShaderResources(0,2,views);
	ID3D11SamplerState *samplerPtr=sampler.Get();
	surface.context->PSSetSamplers(0,1,&samplerPtr);
	surface.context->Draw(4,0);
	ID3D11ShaderResourceView *nullViews[2]{};
	surface.context->PSSetShaderResources(0,2,nullViews);
	if (!Readback(surface.device.Get(), surface.context.Get(), outputTexture.Get(),
		result.composited, error)) return false;
	for (std::size_t i = 0; i < pixelCount; ++i)
	{
		const bool protectedPixel = maskValues[i] != 0;
		const auto expectedOffset = i * 4;
		const auto& expected = protectedPixel ? result.original.rgba : result.neural.rgba;
		bool matches = true;
		for (int c = 0; c < 4; ++c)
			matches = matches && result.composited.rgba[expectedOffset + c]
				== expected[expectedOffset + c];
		if (protectedPixel)
		{
			++result.protectedPixels;
			if (!matches) ++result.protectedMismatch;
			bool wrongMatches = true;
			for (int c = 0; c < 4; ++c)
				wrongMatches = wrongMatches && result.neural.rgba[expectedOffset + c]
					== result.original.rgba[expectedOffset + c];
			if (!wrongMatches) ++result.wrongProtectedMismatch;
		}
		else if (!matches)
			++result.worldChanged;
	}
	return result.protectedPixels != 0 && result.protectedMismatch == 0
		&& result.worldChanged == 0 && result.wrongProtectedMismatch == result.protectedPixels;
}

} // namespace neuraltest
