// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstring>
#include <iomanip>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

std::string HrText(const char *operation, HRESULT hr)
{
	std::ostringstream out;
	out << operation << " failed (HRESULT 0x" << std::hex << std::uppercase
		<< static_cast<unsigned long>(hr) << ')';
	return out.str();
}

bool Compile(const char *source, const char *entry, const char *target,
	ComPtr<ID3DBlob>& blob, std::string& error)
{
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT hr = D3DCompile(source, std::strlen(source), "neuraltest-fixture", nullptr, nullptr,
		entry, target, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		blob.GetAddressOf(), diagnostics.GetAddressOf());
	if (FAILED(hr))
	{
		error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
			diagnostics->GetBufferSize()) : HrText("D3DCompile", hr);
		return false;
	}
	return true;
}

} // namespace

std::uint64_t HashImage(const Image& image)
{
	std::uint64_t hash = 1469598103934665603ull;
	for (std::uint8_t byte : image.rgba)
	{
		hash ^= byte;
		hash *= 1099511628211ull;
	}
	return hash;
}

bool RenderFixture(const Fixture& fixture, const RenderOptions& options, RenderResult& result,
	std::string& error)
{
	if (options.renderer != "dx11" && options.renderer != "dx11-oit")
	{
		error = "renderer must be dx11 or dx11-oit";
		return false;
	}
	if (options.scale != 1 && options.scale != 4 && options.scale != 8)
	{
		error = "scale must be 1, 4, or 8";
		return false;
	}

	const UINT width = 320u * options.scale;
	const UINT height = 240u * options.scale;
	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL obtained{};
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	HRESULT hr = D3D11CreateDevice(nullptr,
		options.warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
		requested, static_cast<UINT>(std::size(requested)), D3D11_SDK_VERSION,
		device.GetAddressOf(), &obtained, context.GetAddressOf());
	if (FAILED(hr) && !options.warp)
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, requested,
			static_cast<UINT>(std::size(requested)), D3D11_SDK_VERSION,
			device.GetAddressOf(), &obtained, context.GetAddressOf());
	if (FAILED(hr))
	{
		error = HrText("D3D11CreateDevice", hr);
		return false;
	}

	ComPtr<IDXGIDevice> dxgiDevice;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC desc{};
	if (SUCCEEDED(device.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) &&
		SUCCEEDED(adapter->GetDesc(&desc)))
	{
		char adapterName[256]{};
		WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName,
			static_cast<int>(std::size(adapterName)), nullptr, nullptr);
		result.adapter = adapterName;
	}

	D3D11_TEXTURE2D_DESC colorDesc{};
	colorDesc.Width = width;
	colorDesc.Height = height;
	colorDesc.MipLevels = 1;
	colorDesc.ArraySize = 1;
	colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	colorDesc.SampleDesc.Count = 1;
	colorDesc.Usage = D3D11_USAGE_DEFAULT;
	colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	ComPtr<ID3D11Texture2D> color;
	ComPtr<ID3D11RenderTargetView> rtv;
	if (FAILED(hr = device->CreateTexture2D(&colorDesc, nullptr, color.GetAddressOf())) ||
		FAILED(hr = device->CreateRenderTargetView(color.Get(), nullptr, rtv.GetAddressOf())))
	{
		error = HrText("create color target", hr);
		return false;
	}

	D3D11_TEXTURE2D_DESC depthDesc = colorDesc;
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	ComPtr<ID3D11Texture2D> depth;
	ComPtr<ID3D11DepthStencilView> dsv;
	if (FAILED(hr = device->CreateTexture2D(&depthDesc, nullptr, depth.GetAddressOf())) ||
		FAILED(hr = device->CreateDepthStencilView(depth.Get(), nullptr, dsv.GetAddressOf())))
	{
		error = HrText("create depth target", hr);
		return false;
	}

	static const char shader[] = R"(
struct VSIn { float3 pos : POSITION; float4 color : COLOR0; };
struct VSOut { float4 pos : SV_Position; float4 color : COLOR0; };
cbuffer Params : register(b0) { float2 jitter; float2 pad; };
VSOut VSMain(VSIn i) { VSOut o; o.pos=float4(i.pos.xy+jitter, i.pos.z, 1); o.color=i.color; return o; }
float4 PSMain(VSOut i) : SV_Target { return i.color; }
)";
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	if (!Compile(shader, "VSMain", "vs_5_0", vsCode, error) ||
		!Compile(shader, "PSMain", "ps_5_0", psCode, error))
		return false;
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	if (FAILED(hr = device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr,
		vs.GetAddressOf())) || FAILED(hr = device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, ps.GetAddressOf())))
	{
		error = HrText("create fixture shader", hr);
		return false;
	}
	const D3D11_INPUT_ELEMENT_DESC elements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ComPtr<ID3D11InputLayout> layout;
	if (FAILED(hr = device->CreateInputLayout(elements, static_cast<UINT>(std::size(elements)),
		vsCode->GetBufferPointer(), vsCode->GetBufferSize(), layout.GetAddressOf())))
	{
		error = HrText("CreateInputLayout", hr);
		return false;
	}

	auto makeBuffer = [&](const void *data, UINT size, UINT bind, ID3D11Buffer **out) {
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = size;
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.BindFlags = bind;
		D3D11_SUBRESOURCE_DATA init{};
		init.pSysMem = data;
		return device->CreateBuffer(&bd, &init, out);
	};
	ComPtr<ID3D11Buffer> vb;
	ComPtr<ID3D11Buffer> ib;
	if (FAILED(hr = makeBuffer(fixture.vertices.data(),
		static_cast<UINT>(fixture.vertices.size() * sizeof(Vertex)), D3D11_BIND_VERTEX_BUFFER,
		vb.GetAddressOf())) || FAILED(hr = makeBuffer(fixture.indices.data(),
		static_cast<UINT>(fixture.indices.size() * sizeof(std::uint16_t)), D3D11_BIND_INDEX_BUFFER,
		ib.GetAddressOf())))
	{
		error = HrText("create fixture geometry", hr);
		return false;
	}
	struct Params { float x, y, pad0, pad1; } params{};
	if (options.jitter)
	{
		params.x = (options.frame & 1u ? .5f : -.5f) * 2.f / static_cast<float>(width);
		params.y = (options.frame & 2u ? .5f : -.5f) * 2.f / static_cast<float>(height);
	}
	ComPtr<ID3D11Buffer> constants;
	if (FAILED(hr = makeBuffer(&params, sizeof(params), D3D11_BIND_CONSTANT_BUFFER,
		constants.GetAddressOf())))
	{
		error = HrText("create fixture constants", hr);
		return false;
	}

	const float clear[] = {.025f, .03f, .045f, 1.f};
	context->ClearRenderTargetView(rtv.Get(), clear);
	context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
	ID3D11RenderTargetView *views[] = {rtv.Get()};
	context->OMSetRenderTargets(1, views, dsv.Get());
	D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
	context->RSSetViewports(1, &viewport);
	const UINT stride = sizeof(Vertex);
	const UINT offset = 0;
	ID3D11Buffer *vertexBuffers[] = {vb.Get()};
	context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetInputLayout(layout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(vs.Get(), nullptr, 0);
	ID3D11Buffer *constantBuffers[] = {constants.Get()};
	context->VSSetConstantBuffers(0, 1, constantBuffers);
	context->PSSetShader(ps.Get(), nullptr, 0);
	context->DrawIndexed(static_cast<UINT>(fixture.indices.size()), 0, 0);

	D3D11_TEXTURE2D_DESC stagingDesc = colorDesc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	if (FAILED(hr = device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf())))
	{
		error = HrText("create staging target", hr);
		return false;
	}
	context->CopyResource(staging.Get(), color.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
	{
		error = HrText("map staging target", hr);
		return false;
	}
	result.color.width = width;
	result.color.height = height;
	result.color.rgba.resize(static_cast<std::size_t>(width) * height * 4);
	for (UINT y = 0; y < height; ++y)
		std::memcpy(result.color.rgba.data() + static_cast<std::size_t>(y) * width * 4,
			static_cast<const std::uint8_t *>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch,
			static_cast<std::size_t>(width) * 4);
	context->Unmap(staging.Get(), 0);
	result.hash = HashImage(result.color);
	return true;
}

} // namespace neuraltest
