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

constexpr UINT Width = 64;
constexpr UINT Height = 48;

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
		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (SUCCEEDED(hr)) hr = surface.device12->CreateCommandQueue(&queueDesc,
			IID_PPV_ARGS(surface.queue12.GetAddressOf()));
		IUnknown *queues[] = {surface.queue12.Get()};
		if (SUCCEEDED(hr)) hr = D3D11On12CreateDevice(surface.device12.Get(),
			D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
			queues, 1, 0, surface.device.GetAddressOf(), surface.context.GetAddressOf(),
			&selected);
		surface.name = "d3d11on12";
	}
	if (FAILED(hr)) { error = HrText("create disocclusion surface", hr); return false; }
	surface.adapter = AdapterName(surface.device.Get());
	return true;
}

bool ExtractShader(std::string& shader, std::string& error)
{
	std::ifstream input(std::string(NEURAL_SOURCE_DIR)
		+ "/core/rend/dx11/dx11_shaders.cpp", std::ios::binary);
	if (!input) { error = "cannot open production dx11_shaders.cpp"; return false; }
	std::ostringstream stream;
	stream << input.rdbuf();
	const std::string marker = "NeuralDisocclusionPixelShader = R\"(";
	const auto begin = stream.str().find(marker);
	if (begin == std::string::npos) { error = "cannot find production disocclusion shader"; return false; }
	const auto content = begin + marker.size();
	const auto end = stream.str().find(")\";", content);
	if (end == std::string::npos) { error = "cannot parse production disocclusion shader"; return false; }
	shader = stream.str().substr(content, end - content);
	return true;
}

std::uint16_t FloatToHalf(float value)
{
	std::uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	const std::uint32_t sign = (bits >> 16) & 0x8000u;
	int exponent = static_cast<int>((bits >> 23) & 0xffu) - 127 + 15;
	std::uint32_t mantissa = bits & 0x7fffffu;
	if (exponent <= 0) return static_cast<std::uint16_t>(sign);
	if (exponent >= 31) return static_cast<std::uint16_t>(sign | 0x7c00u);
	return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10)
		| ((mantissa + 0x1000u) >> 13));
}

bool CreateTexture(ID3D11Device *device, DXGI_FORMAT format, UINT bindFlags,
	const void *data, UINT pitch, ComPtr<ID3D11Texture2D>& texture,
	ComPtr<ID3D11ShaderResourceView>& view, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = Width; desc.Height = Height; desc.MipLevels = 1; desc.ArraySize = 1;
	desc.Format = format; desc.SampleDesc.Count = 1; desc.BindFlags = bindFlags;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = data; initial.SysMemPitch = pitch;
	HRESULT hr = device->CreateTexture2D(&desc, data ? &initial : nullptr,
		texture.GetAddressOf());
	if (SUCCEEDED(hr) && (bindFlags & D3D11_BIND_SHADER_RESOURCE))
		hr = device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create disocclusion texture", hr); return false; }
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
	if (FAILED(hr)) { error = HrText("read disocclusion mask", hr); return false; }
	image.width = Width; image.height = Height;
	image.rgba.resize(static_cast<std::size_t>(Width) * Height * 4);
	for (UINT y = 0; y < Height; ++y)
		for (UINT x = 0; x < Width; ++x)
		{
			const auto value = *(static_cast<const std::uint8_t *>(mapped.pData)
				+ static_cast<std::size_t>(y) * mapped.RowPitch + x);
			const auto offset = (static_cast<std::size_t>(y) * Width + x) * 4;
			image.rgba[offset] = image.rgba[offset + 1] = image.rgba[offset + 2] = value;
			image.rgba[offset + 3] = 255;
		}
	context->Unmap(staging.Get(), 0);
	return true;
}

void FillRect(std::vector<std::uint8_t>& values, UINT left, UINT top, UINT right,
	UINT bottom, std::uint8_t value)
{
	for (UINT y = top; y < bottom; ++y)
		for (UINT x = left; x < right; ++x) values[static_cast<std::size_t>(y) * Width + x] = value;
}

template<typename T>
void FillRect(std::vector<T>& values, UINT left, UINT top, UINT right,
	UINT bottom, T value)
{
	for (UINT y = top; y < bottom; ++y)
		for (UINT x = left; x < right; ++x) values[static_cast<std::size_t>(y) * Width + x] = value;
}

bool RegionIs(const Image& image, UINT left, UINT top, UINT right, UINT bottom,
	std::uint8_t value)
{
	for (UINT y = top; y < bottom; ++y)
		for (UINT x = left; x < right; ++x)
			if (image.rgba[(static_cast<std::size_t>(y) * Width + x) * 4] != value) return false;
	return true;
}

} // namespace

bool RunDisocclusionContractFixture(bool d3d11On12,
	DisocclusionContractResult& result, std::string& error)
{
	Surface surface;
	if (!CreateSurface(d3d11On12, surface, error)) return false;
	result.surface = surface.name;
	result.adapter = surface.adapter;
	std::string pixelSource;
	if (!ExtractShader(pixelSource, error)) return false;
	static const char vertexSource[] = R"(
struct V { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
V main(uint id : SV_VertexID) {
	float2 p[4] = {float2(-1,-1),float2(-1,1),float2(1,-1),float2(1,1)};
	V o; o.pos=float4(p[id],0,1); o.uv=float2((p[id].x+1)*.5,(1-p[id].y)*.5); return o;
})";
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	ComPtr<ID3DBlob> diagnostics;
	HRESULT hr = D3DCompile(vertexSource, std::strlen(vertexSource), "disocclusion-vs",
		nullptr, nullptr, "main", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		vsCode.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr)) hr = D3DCompile(pixelSource.data(), pixelSource.size(),
		"production-disocclusion-ps", nullptr, nullptr, "main", "ps_4_0",
		D3DCOMPILE_ENABLE_STRICTNESS, 0, psCode.GetAddressOf(), diagnostics.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
			diagnostics->GetBufferSize()) : HrText("compile disocclusion shaders", hr);
		return false;
	}
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	hr = surface.device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		nullptr, vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, ps.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create disocclusion shaders", hr); return false; }

	const std::size_t pixels = static_cast<std::size_t>(Width) * Height;
	std::vector<std::uint8_t> mask(pixels, 0);
	std::vector<std::uint8_t> confidence(pixels, 255);
	std::vector<float> currentDepth(pixels, .2f);
	std::vector<float> previousDepth(pixels, .2f);
	std::vector<std::uint16_t> expectedId(pixels, 1);
	std::vector<std::uint16_t> previousId(pixels, 1);
	std::vector<std::uint16_t> motion(pixels * 2, 0);
	auto setMotion = [&](UINT left, UINT top, UINT right, UINT bottom, float x, float y) {
		for (UINT py = top; py < bottom; ++py)
			for (UINT px = left; px < right; ++px)
			{
				const auto offset = (static_cast<std::size_t>(py) * Width + px) * 2;
				motion[offset] = FloatToHalf(x); motion[offset + 1] = FloatToHalf(y);
			}
	};
	// Four-pixel-wide bands: out-of-rect, static, camera pan, depth mismatch,
	// crossing identities, revealed background, newly visible, and scene cut.
	setMotion(0, 8, 4, 20, -4.f, 0.f);
	setMotion(12, 8, 16, 20, -4.f, 0.f);
	FillRect(previousDepth, 20, 8, 24, 20, .1f);
	FillRect(previousId, 28, 8, 32, 20, static_cast<std::uint16_t>(2));
	FillRect(mask, 36, 8, 40, 20, static_cast<std::uint8_t>(255));
	FillRect(confidence, 36, 8, 40, 20, static_cast<std::uint8_t>(0));
	FillRect(currentDepth, 36, 8, 40, 20, 0.f);
	FillRect(expectedId, 36, 8, 40, 20, static_cast<std::uint16_t>(0));
	FillRect(previousDepth, 44, 8, 48, 20, 0.f);
	FillRect(mask, 52, 8, 56, 20, static_cast<std::uint8_t>(255));
	FillRect(confidence, 52, 8, 56, 20, static_cast<std::uint8_t>(0));
	FillRect(currentDepth, 58, 8, 62, 20, .8f);
	FillRect(previousDepth, 58, 8, 62, 20, .807f);

	std::array<ComPtr<ID3D11Texture2D>, 7> inputs;
	std::array<ComPtr<ID3D11ShaderResourceView>, 7> views;
	if (!CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE,
		mask.data(), Width, inputs[0], views[0], error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE,
			currentDepth.data(), Width * 4, inputs[1], views[1], error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R16G16_FLOAT, D3D11_BIND_SHADER_RESOURCE,
			motion.data(), Width * 4, inputs[2], views[2], error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE,
			confidence.data(), Width, inputs[3], views[3], error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R16_UINT, D3D11_BIND_SHADER_RESOURCE,
			expectedId.data(), Width * 2, inputs[4], views[4], error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE,
			previousDepth.data(), Width * 4, inputs[5], views[5], error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R16_UINT, D3D11_BIND_SHADER_RESOURCE,
			previousId.data(), Width * 2, inputs[6], views[6], error)) return false;
	ComPtr<ID3D11Texture2D> output;
	ComPtr<ID3D11ShaderResourceView> unused;
	if (!CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM, D3D11_BIND_RENDER_TARGET,
		nullptr, 0, output, unused, error)) return false;
	ComPtr<ID3D11RenderTargetView> target;
	hr = surface.device->CreateRenderTargetView(output.Get(), nullptr, target.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create disocclusion target", hr); return false; }
	ID3D11RenderTargetView *targetPtr = target.Get();
	surface.context->OMSetRenderTargets(1, &targetPtr, nullptr);
	const float clear[4] = {1,1,1,1};
	surface.context->ClearRenderTargetView(target.Get(), clear);
	D3D11_VIEWPORT viewport{0,0,static_cast<float>(Width),static_cast<float>(Height),0,1};
	surface.context->RSSetViewports(1, &viewport);
	surface.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	surface.context->VSSetShader(vs.Get(), nullptr, 0);
	surface.context->PSSetShader(ps.Get(), nullptr, 0);
	ID3D11ShaderResourceView *inputViews[7]{};
	for (std::size_t i = 0; i < views.size(); ++i) inputViews[i] = views[i].Get();
	surface.context->PSSetShaderResources(0, static_cast<UINT>(std::size(inputViews)), inputViews);
	surface.context->Draw(4, 0);
	ID3D11ShaderResourceView *nullViews[7]{};
	surface.context->PSSetShaderResources(0, static_cast<UINT>(std::size(nullViews)), nullViews);
	surface.context->Flush();
	if (!ReadMask(surface.device.Get(), surface.context.Get(), output.Get(), result.resolvedMask,
		error)) return false;
	result.wrongMask.width = Width; result.wrongMask.height = Height;
	result.wrongMask.rgba.resize(pixels * 4);
	for (std::size_t i = 0; i < pixels; ++i)
	{
		for (int channel = 0; channel < 3; ++channel) result.wrongMask.rgba[i * 4 + channel] = mask[i];
		result.wrongMask.rgba[i * 4 + 3] = 255;
	}
	result.outsideProtected = RegionIs(result.resolvedMask, 0, 8, 4, 20, 255);
	result.staticTrusted = RegionIs(result.resolvedMask, 4, 8, 8, 20, 0);
	result.cameraPanTrusted = RegionIs(result.resolvedMask, 12, 8, 16, 20, 0);
	result.depthToleranceTrusted = RegionIs(result.resolvedMask, 58, 8, 62, 20, 0);
	result.depthProtected = RegionIs(result.resolvedMask, 20, 8, 24, 20, 255);
	result.crossingProtected = RegionIs(result.resolvedMask, 28, 8, 32, 20, 255);
	result.revealProtected = RegionIs(result.resolvedMask, 36, 8, 40, 20, 255);
	result.newlyVisibleProtected = RegionIs(result.resolvedMask, 44, 8, 48, 20, 255);
	result.sceneCutProtected = RegionIs(result.resolvedMask, 52, 8, 56, 20, 255);
	const std::array<std::array<UINT, 4>, 4> disocclusionOnly = {{{0,8,4,20},
		{20,8,24,20}, {28,8,32,20}, {44,8,48,20}}};
	for (const auto& region : disocclusionOnly)
		for (UINT y = region[1]; y < region[3]; ++y)
			for (UINT x = region[0]; x < region[2]; ++x)
			{
				const auto index = static_cast<std::size_t>(y) * Width + x;
				if (result.resolvedMask.rgba[index * 4] == 255) ++result.protectedPixels;
				if (mask[index] == 0) ++result.wrongMissedPixels;
			}
	result.correctTrailEnergy = 0;
	result.wrongTrailEnergy = static_cast<std::uint64_t>(result.wrongMissedPixels) * 64;
	return result.staticTrusted && result.cameraPanTrusted && result.depthToleranceTrusted
		&& result.revealProtected
		&& result.crossingProtected && result.depthProtected && result.outsideProtected
		&& result.sceneCutProtected && result.newlyVisibleProtected
		&& result.protectedPixels == result.wrongMissedPixels
		&& result.correctTrailEnergy == 0 && result.wrongTrailEnergy > 0;
}

} // namespace neuraltest
