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
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

constexpr UINT Width = 128;
constexpr UINT Height = 96;

struct ContractVertex {
	float x;
	float y;
	float sourceDepth;
	float color[4];
};

struct ProductionVertex {
	float x;
	float y;
	float z;
	std::uint32_t color;
	std::uint32_t specular;
	float u;
	float v;
};

struct Naomi2ProductionVertex {
	float x;
	float y;
	float z;
	std::uint32_t color;
	std::uint32_t specular;
	float u;
	float v;
	std::uint32_t color1;
	std::uint32_t specular1;
	float u1;
	float v1;
	float nx;
	float ny;
	float nz;
};

struct PreviousPosition {
	float x;
	float y;
	float z;
	float valid;
};

struct Surface {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D12Device> device12;
	ComPtr<ID3D12CommandQueue> queue12;
	std::string name;
	std::string adapter;
};

struct Targets {
	ComPtr<ID3D11Texture2D> color;
	ComPtr<ID3D11RenderTargetView> colorTarget;
	ComPtr<ID3D11Texture2D> depth;
	ComPtr<ID3D11DepthStencilView> depthTarget;
	std::array<ComPtr<ID3D11Texture2D>, 5> guidance;
	std::array<ComPtr<ID3D11RenderTargetView>, 5> guidanceTargets;
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
	if (begin == std::string::npos)
		return false;
	const auto contentBegin = begin + marker.size();
	const auto end = source.find(")\";", contentBegin);
	if (end == std::string::npos)
		return false;
	value = source.substr(contentBegin, end - contentBegin);
	return true;
}

class PixelInclude final : public ID3DInclude {
public:
	explicit PixelInclude(const std::string& common) : common_(common) {}

	HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID,
		LPCVOID *data, UINT *bytes) override
	{
		if (std::strcmp(fileName, "pixel_common.hlsl") != 0)
			return E_FAIL;
		*data = common_.data();
		*bytes = static_cast<UINT>(common_.size());
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }

private:
	const std::string& common_;
};

bool CompilePixel(const std::string& source, PixelInclude& includes, bool neuralExport,
	bool alphaTest, ComPtr<ID3DBlob>& code, std::string& error)
{
	D3D_SHADER_MACRO macros[] = {
		{"pp_Gouraud", "0"}, {"DIV_POS_Z", "0"}, {"pp_Texture", "0"},
		{"pp_UseAlpha", "1"}, {"pp_IgnoreTexA", "0"}, {"pp_ShadInstr", "0"},
		{"pp_Offset", "0"}, {"pp_FogCtrl", "2"}, {"pp_BumpMap", "0"},
		{"FogClamping", "0"}, {"pp_TriLinear", "0"}, {"pp_Palette", "0"},
		{"cp_AlphaTest", alphaTest ? "1" : "0"}, {"pp_ClipInside", "0"},
		{"DITHERING", "0"}, {"NEURAL_EXPORT", neuralExport ? "1" : "0"},
		{nullptr, nullptr}
	};
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT hr = D3DCompile(source.data(), source.size(), "production-dx11-pixel",
		macros, &includes, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS |
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr))
		return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : HrText("compile production pixel shader", hr);
	return false;
}

bool CompileVertex(ComPtr<ID3DBlob>& code, std::string& error)
{
	static const char source[] = R"(
struct VSIn { float2 pos : POSITION; float sourceDepth : TEXCOORD0; float4 color : COLOR0; };
struct VSOut { float4 pos : SV_POSITION; float4 uv : TEXCOORD0; nointerpolation float4 col : COLOR0; nointerpolation float4 spec : COLOR1; };
VSOut main(VSIn i) { VSOut o; o.pos=float4(i.pos, 0, 1); o.uv=float4(0,0,0,i.sourceDepth); o.col=i.color; o.spec=0; return o; }
)";
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT hr = D3DCompile(source, std::strlen(source), "depth-contract-vertex",
		nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS |
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr))
		return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : HrText("compile depth contract vertex shader", hr);
	return false;
}

bool CompileProductionVertex(const std::string& source, bool naomi2,
	ComPtr<ID3DBlob>& code, std::string& error)
{
	D3D_SHADER_MACRO macros[] = {
		{"pp_Gouraud", "1"}, {"DIV_POS_Z", "0"}, {"POSITION_ONLY", "0"},
		{"pp_Texture", "0"}, {"pp_TwoVolumes", "0"},
		{"LIGHT_ON", naomi2 ? "0" : "1"}, {"MODIFIER_VOLUME", "0"},
		{"NEURAL_EXPORT", "1"}, {nullptr, nullptr}
	};
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT hr = D3DCompile(source.data(), source.size(), "production-motion-vertex",
		macros, nullptr, "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS |
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(hr)) return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : HrText("compile production motion vertex shader", hr);
	return false;
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
		if (SUCCEEDED(hr))
			hr = surface.device12->CreateCommandQueue(&queueDesc,
				IID_PPV_ARGS(surface.queue12.GetAddressOf()));
		IUnknown *queues[] = {surface.queue12.Get()};
		if (SUCCEEDED(hr))
			hr = D3D11On12CreateDevice(surface.device12.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT,
				levels, static_cast<UINT>(std::size(levels)), queues, 1, 0,
				surface.device.GetAddressOf(), surface.context.GetAddressOf(), &selected);
		surface.name = "d3d11on12";
	}
	if (FAILED(hr))
	{
		error = HrText(on12 ? "create D3D11On12 depth-contract surface" :
			"create native D3D11 depth-contract surface", hr);
		return false;
	}
	surface.adapter = AdapterName(surface.device.Get());
	return true;
}

bool CreateTargets(ID3D11Device *device, Targets& targets, std::string& error)
{
	D3D11_TEXTURE2D_DESC color{};
	color.Width = Width;
	color.Height = Height;
	color.MipLevels = 1;
	color.ArraySize = 1;
	color.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	color.SampleDesc.Count = 1;
	color.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	HRESULT hr = device->CreateTexture2D(&color, nullptr, targets.color.GetAddressOf());
	if (SUCCEEDED(hr))
		hr = device->CreateRenderTargetView(targets.color.Get(), nullptr,
			targets.colorTarget.GetAddressOf());
	D3D11_TEXTURE2D_DESC depth = color;
	depth.Format = DXGI_FORMAT_R32_TYPELESS;
	depth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (SUCCEEDED(hr))
		hr = device->CreateTexture2D(&depth, nullptr, targets.depth.GetAddressOf());
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv{};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (SUCCEEDED(hr))
		hr = device->CreateDepthStencilView(targets.depth.Get(), &dsv,
			targets.depthTarget.GetAddressOf());
	const DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R8_UNORM,
		DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT};
	for (std::size_t i = 0; SUCCEEDED(hr) && i < targets.guidance.size(); ++i)
	{
		D3D11_TEXTURE2D_DESC desc = color;
		desc.Format = formats[i];
		hr = device->CreateTexture2D(&desc, nullptr, targets.guidance[i].GetAddressOf());
		if (SUCCEEDED(hr))
			hr = device->CreateRenderTargetView(targets.guidance[i].Get(), nullptr,
				targets.guidanceTargets[i].GetAddressOf());
	}
	if (SUCCEEDED(hr))
		return true;
	error = HrText("create depth-contract targets", hr);
	return false;
}

float ClipX(float pixel) { return pixel / static_cast<float>(Width) * 2.f - 1.f; }
float ClipY(float pixel) { return 1.f - pixel / static_cast<float>(Height) * 2.f; }

void AddQuad(std::vector<ContractVertex>& vertices, float left, float top, float right,
	float bottom, float depth, std::array<float, 4> color)
{
	const ContractVertex quad[] = {
		{ClipX(left), ClipY(top), depth, {color[0], color[1], color[2], color[3]}},
		{ClipX(right), ClipY(top), depth, {color[0], color[1], color[2], color[3]}},
		{ClipX(left), ClipY(bottom), depth, {color[0], color[1], color[2], color[3]}},
		{ClipX(right), ClipY(bottom), depth, {color[0], color[1], color[2], color[3]}},
	};
	vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
}

bool ReadColor(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *source,
	Image& image, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create color readback", hr); return false; }
	context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) { error = HrText("map color readback", hr); return false; }
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

bool ReadDepth(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *source,
	std::vector<float>& values, std::string& error)
{
	D3D11_TEXTURE2D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create depth readback", hr); return false; }
	context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) { error = HrText("map depth readback", hr); return false; }
	values.resize(static_cast<std::size_t>(desc.Width) * desc.Height);
	for (UINT y = 0; y < desc.Height; ++y)
		std::memcpy(values.data() + static_cast<std::size_t>(y) * desc.Width,
			static_cast<const std::uint8_t *>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch,
			static_cast<std::size_t>(desc.Width) * sizeof(float));
	context->Unmap(staging.Get(), 0);
	return true;
}

float HalfToFloat(std::uint16_t half)
{
	const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
	std::uint32_t exponent = (half >> 10) & 0x1fu;
	std::uint32_t mantissa = half & 0x3ffu;
	std::uint32_t bits = 0;
	if (exponent == 0)
	{
		if (mantissa == 0) bits = sign;
		else
		{
			exponent = 127 - 15 + 1;
			while ((mantissa & 0x400u) == 0) { mantissa <<= 1; --exponent; }
			mantissa &= 0x3ffu;
			bits = sign | (exponent << 23) | (mantissa << 13);
		}
	}
	else if (exponent == 31) bits = sign | 0x7f800000u | (mantissa << 13);
	else bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
	float value = 0.f;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

bool ReadProductionGuidance(ID3D11Device *device, ID3D11DeviceContext *context,
	const Targets& targets, ProductionMotionResult& result, bool trusted,
	bool oversized, std::string& error)
{
	std::array<std::array<std::uint8_t, 4>, 5> samples{};
	for (std::size_t i = 0; i < targets.guidance.size(); ++i)
	{
		D3D11_TEXTURE2D_DESC desc{};
		targets.guidance[i]->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture2D> staging;
		HRESULT hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
		if (SUCCEEDED(hr)) context->CopyResource(staging.Get(), targets.guidance[i].Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (SUCCEEDED(hr)) hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr)) { error = HrText("read production motion guidance", hr); return false; }
		const auto *pixel = static_cast<const std::uint8_t *>(mapped.pData)
			+ static_cast<std::size_t>(Height / 2) * mapped.RowPitch
			+ static_cast<std::size_t>(Width / 2) * (i == 0 ? 4 : i >= 3 ? 2 : 1);
		std::memcpy(samples[i].data(), pixel, i == 0 ? 4 : i >= 3 ? 2 : 1);
		context->Unmap(staging.Get(), 0);
	}
	std::uint16_t motionHalf[2]{};
	std::memcpy(motionHalf, samples[0].data(), sizeof(motionHalf));
	const float motionX = HalfToFloat(motionHalf[0]);
	const float motionY = HalfToFloat(motionHalf[1]);
	if (trusted)
	{
		result.trustedX = motionX;
		result.trustedY = motionY;
		result.trustedMask = samples[1][0];
		result.trustedConfidence = samples[2][0];
		std::memcpy(&result.trustedDrawId, samples[3].data(), sizeof(result.trustedDrawId));
		std::memcpy(&result.trustedPreviousDrawId, samples[4].data(),
			sizeof(result.trustedPreviousDrawId));
	}
	else if (oversized)
	{
		result.oversizedX = motionX;
		result.oversizedY = motionY;
		result.oversizedMask = samples[1][0];
		result.oversizedConfidence = samples[2][0];
	}
	else
	{
		result.invalidX = motionX;
		result.invalidY = motionY;
		result.invalidMask = samples[1][0];
		result.invalidConfidence = samples[2][0];
	}
	return true;
}

bool PixelIs(const Image& image, UINT x, UINT y, std::array<std::uint8_t, 3> rgb)
{
	const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
	return image.rgba[offset] == rgb[0] && image.rgba[offset + 1] == rgb[1]
		&& image.rgba[offset + 2] == rgb[2];
}

bool RenderPass(Surface& surface, Targets& targets, ID3D11VertexShader *vertexShader,
	ID3D11PixelShader *opaque, ID3D11PixelShader *punch, ID3D11InputLayout *layout,
	ID3D11Buffer *vertices, ID3D11Buffer *indices, ID3D11DepthStencilState *depthState,
	bool reverse, bool neuralExport, Image& color, std::vector<float>& depth,
	std::string& error)
{
	auto *context = surface.context.Get();
	const float black[] = {0.f, 0.f, 0.f, 1.f};
	context->ClearRenderTargetView(targets.colorTarget.Get(), black);
	context->ClearDepthStencilView(targets.depthTarget.Get(), D3D11_CLEAR_DEPTH, 0.f, 0);
	if (neuralExport)
	{
		ID3D11RenderTargetView *views[] = {targets.guidanceTargets[0].Get(),
			targets.guidanceTargets[1].Get(), targets.guidanceTargets[2].Get(),
			targets.guidanceTargets[3].Get(), targets.guidanceTargets[4].Get()};
		context->OMSetRenderTargets(static_cast<UINT>(std::size(views)), views,
			targets.depthTarget.Get());
		for (auto& view : targets.guidanceTargets)
			context->ClearRenderTargetView(view.Get(), black);
	}
	else
	{
		ID3D11RenderTargetView *view = targets.colorTarget.Get();
		context->OMSetRenderTargets(1, &view, targets.depthTarget.Get());
	}
	D3D11_VIEWPORT viewport{0, 0, static_cast<float>(Width), static_cast<float>(Height), 0, 1};
	context->RSSetViewports(1, &viewport);
	context->OMSetDepthStencilState(depthState, 0);
	const UINT stride = sizeof(ContractVertex);
	const UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
	context->IASetIndexBuffer(indices, DXGI_FORMAT_R16_UINT, 0);
	context->IASetInputLayout(layout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(vertexShader, nullptr, 0);
	auto draw = [&](UINT quad, ID3D11PixelShader *shader) {
		context->PSSetShader(shader, nullptr, 0);
		context->DrawIndexed(6, quad * 6, 0);
	};
	if (reverse) { draw(1, opaque); draw(0, opaque); }
	else { draw(0, opaque); draw(1, opaque); }
	draw(2, punch);
	context->Flush();
	if (!ReadDepth(surface.device.Get(), context, targets.depth.Get(), depth, error))
		return false;
	return neuralExport || ReadColor(surface.device.Get(), context, targets.color.Get(), color, error);
}

} // namespace

bool RunDepthContractFixture(bool d3d11On12, DepthContractResult& result, std::string& error)
{
	Surface surface;
	if (!CreateSurface(d3d11On12, surface, error))
		return false;
	result.surface = surface.name;
	result.adapter = surface.adapter;
	std::ifstream input(std::string(NEURAL_SOURCE_DIR) + "/core/rend/dx11/dx11_shaders.cpp",
		std::ios::binary);
	if (!input) { error = "cannot open production dx11_shaders.cpp"; return false; }
	std::ostringstream stream;
	stream << input.rdbuf();
	std::string common;
	std::string pixel;
	if (!ExtractRawString(stream.str(), "PixelShaderCommon", common) ||
		!ExtractRawString(stream.str(), "PixelShader", pixel))
	{
		error = "cannot extract production pixel shader raw strings";
		return false;
	}
	PixelInclude includes(common);
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> nativeOpaqueCode;
	ComPtr<ID3DBlob> nativePunchCode;
	ComPtr<ID3DBlob> exportOpaqueCode;
	ComPtr<ID3DBlob> exportPunchCode;
	if (!CompileVertex(vsCode, error) ||
		!CompilePixel(pixel, includes, false, false, nativeOpaqueCode, error) ||
		!CompilePixel(pixel, includes, false, true, nativePunchCode, error) ||
		!CompilePixel(pixel, includes, true, false, exportOpaqueCode, error) ||
		!CompilePixel(pixel, includes, true, true, exportPunchCode, error))
		return false;
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> nativeOpaque;
	ComPtr<ID3D11PixelShader> nativePunch;
	ComPtr<ID3D11PixelShader> exportOpaque;
	ComPtr<ID3D11PixelShader> exportPunch;
	HRESULT hr = surface.device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		nullptr, vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(nativeOpaqueCode->GetBufferPointer(),
		nativeOpaqueCode->GetBufferSize(), nullptr, nativeOpaque.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(nativePunchCode->GetBufferPointer(),
		nativePunchCode->GetBufferSize(), nullptr, nativePunch.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(exportOpaqueCode->GetBufferPointer(),
		exportOpaqueCode->GetBufferSize(), nullptr, exportOpaque.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(exportPunchCode->GetBufferPointer(),
		exportPunchCode->GetBufferSize(), nullptr, exportPunch.GetAddressOf());
	const D3D11_INPUT_ELEMENT_DESC elements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ComPtr<ID3D11InputLayout> layout;
	if (SUCCEEDED(hr)) hr = surface.device->CreateInputLayout(elements,
		static_cast<UINT>(std::size(elements)), vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		layout.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create depth-contract shaders/layout", hr); return false; }

	std::vector<ContractVertex> vertexData;
	constexpr float farSource = .0005f;
	constexpr float nearSource = .005f;
	AddQuad(vertexData, 10, 10, 48, 44, farSource, {1, 0, 0, 1});
	AddQuad(vertexData, 30, 22, 76, 66, nearSource, {0, 1, 0, 1});
	AddQuad(vertexData, 84, 20, 116, 56, nearSource, {0, 0, 1, 1});
	const std::uint16_t indexData[] = {
		0,1,2,2,1,3, 4,5,6,6,5,7, 8,9,10,10,9,11
	};
	auto createBuffer = [&](const void *data, UINT bytes, UINT flags, ID3D11Buffer **buffer) {
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = bytes;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = flags;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = data;
		return surface.device->CreateBuffer(&desc, &initial, buffer);
	};
	ComPtr<ID3D11Buffer> vertices;
	ComPtr<ID3D11Buffer> indices;
	hr = createBuffer(vertexData.data(), static_cast<UINT>(vertexData.size() * sizeof(ContractVertex)),
		D3D11_BIND_VERTEX_BUFFER, vertices.GetAddressOf());
	if (SUCCEEDED(hr)) hr = createBuffer(indexData, sizeof(indexData), D3D11_BIND_INDEX_BUFFER,
		indices.GetAddressOf());
	D3D11_RASTERIZER_DESC rasterDesc{};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthClipEnable = TRUE;
	ComPtr<ID3D11RasterizerState> raster;
	if (SUCCEEDED(hr)) hr = surface.device->CreateRasterizerState(&rasterDesc, raster.GetAddressOf());
	D3D11_DEPTH_STENCIL_DESC correctDesc{};
	correctDesc.DepthEnable = TRUE;
	correctDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	correctDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	D3D11_DEPTH_STENCIL_DESC wrongDesc = correctDesc;
	wrongDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	ComPtr<ID3D11DepthStencilState> correctState;
	ComPtr<ID3D11DepthStencilState> wrongState;
	if (SUCCEEDED(hr)) hr = surface.device->CreateDepthStencilState(&correctDesc,
		correctState.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateDepthStencilState(&wrongDesc,
		wrongState.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create depth-contract geometry/state", hr); return false; }
	surface.context->RSSetState(raster.Get());

	Targets targets;
	if (!CreateTargets(surface.device.Get(), targets, error))
		return false;
	std::vector<float> nativeDepth;
	if (!RenderPass(surface, targets, vs.Get(), nativeOpaque.Get(), nativePunch.Get(), layout.Get(),
		vertices.Get(), indices.Get(), correctState.Get(), false, false, result.correctColor,
		nativeDepth, error))
		return false;
	if (!RenderPass(surface, targets, vs.Get(), exportOpaque.Get(), exportPunch.Get(), layout.Get(),
		vertices.Get(), indices.Get(), correctState.Get(), false, true, result.correctColor,
		result.correctDepth, error))
		return false;
	std::vector<float> reversedNativeDepth;
	if (!RenderPass(surface, targets, vs.Get(), nativeOpaque.Get(), nativePunch.Get(), layout.Get(),
		vertices.Get(), indices.Get(), correctState.Get(), true, false, result.reversedColor,
		reversedNativeDepth, error))
		return false;
	if (!RenderPass(surface, targets, vs.Get(), exportOpaque.Get(), exportPunch.Get(), layout.Get(),
		vertices.Get(), indices.Get(), correctState.Get(), true, true, result.reversedColor,
		result.reversedDepth, error))
		return false;

	// Deliberately apply conventional near-is-smaller comparison to the proven
	// inverted PVR encoding. The farther red surface then rejects the nearer green.
	surface.context->ClearDepthStencilView(targets.depthTarget.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
	const float black[] = {0, 0, 0, 1};
	surface.context->ClearRenderTargetView(targets.colorTarget.Get(), black);
	ID3D11RenderTargetView *colorTarget = targets.colorTarget.Get();
	surface.context->OMSetRenderTargets(1, &colorTarget, targets.depthTarget.Get());
	D3D11_VIEWPORT viewport{0, 0, static_cast<float>(Width), static_cast<float>(Height), 0, 1};
	surface.context->RSSetViewports(1, &viewport);
	surface.context->OMSetDepthStencilState(wrongState.Get(), 0);
	const UINT stride = sizeof(ContractVertex);
	const UINT offset = 0;
	ID3D11Buffer *vertexBuffer = vertices.Get();
	surface.context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	surface.context->IASetIndexBuffer(indices.Get(), DXGI_FORMAT_R16_UINT, 0);
	surface.context->IASetInputLayout(layout.Get());
	surface.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	surface.context->VSSetShader(vs.Get(), nullptr, 0);
	surface.context->PSSetShader(nativeOpaque.Get(), nullptr, 0);
	surface.context->DrawIndexed(6, 0, 0);
	surface.context->DrawIndexed(6, 6, 0);
	surface.context->PSSetShader(nativePunch.Get(), nullptr, 0);
	surface.context->DrawIndexed(6, 12, 0);
	surface.context->Flush();
	if (!ReadColor(surface.device.Get(), surface.context.Get(), targets.color.Get(),
		result.wrongColor, error) || !ReadDepth(surface.device.Get(), surface.context.Get(),
		targets.depth.Get(), result.wrongDepth, error))
		return false;

	auto depthAt = [](const std::vector<float>& values, UINT x, UINT y) {
		return values[static_cast<std::size_t>(y) * Width + x];
	};
	result.clearDepth = depthAt(result.correctDepth, 122, 90);
	result.farDepth = depthAt(result.correctDepth, 20, 20);
	result.nearDepth = depthAt(result.correctDepth, 64, 40);
	result.punchDepth = depthAt(result.correctDepth, 100, 36);
	result.expectedFarDepth = std::log2(1.f + 100000.f * farSource) / 34.f;
	result.expectedNearDepth = std::log2(1.f + 100000.f * nearSource) / 34.f;
	const auto close = [](float a, float b) { return std::abs(a - b) <= 1e-6f; };
	result.nearIsGreater = result.nearDepth > result.farDepth;
	result.clearIsNoGeometry = result.clearDepth == 0.f;
	result.visibleOrderingAgrees = PixelIs(result.correctColor, 36, 30, {0, 255, 0})
		&& PixelIs(result.correctColor, 20, 20, {255, 0, 0});
	result.punchThroughAgrees = PixelIs(result.correctColor, 100, 36, {0, 0, 255})
		&& close(result.punchDepth, result.nearDepth);
	result.reversedSubmissionStable = result.correctColor.rgba == result.reversedColor.rgba
		&& result.correctDepth == result.reversedDepth;
	result.wrongOrderFailed = PixelIs(result.wrongColor, 36, 30, {255, 0, 0})
		&& depthAt(result.wrongDepth, 36, 30) < result.nearDepth;
	result.nativeExportExact = nativeDepth == result.correctDepth
		&& reversedNativeDepth == result.reversedDepth
		&& close(result.farDepth, result.expectedFarDepth)
		&& close(result.nearDepth, result.expectedNearDepth);
	return result.nearIsGreater && result.clearIsNoGeometry && result.visibleOrderingAgrees
		&& result.punchThroughAgrees && result.reversedSubmissionStable
		&& result.wrongOrderFailed && result.nativeExportExact;
}

bool RunProductionMotionFixture(bool d3d11On12, ProductionMotionResult& result,
	std::string& error)
{
	Surface surface;
	if (!CreateSurface(d3d11On12, surface, error)) return false;
	result.surface = surface.name;
	result.adapter = surface.adapter;
	std::ifstream input(std::string(NEURAL_SOURCE_DIR) + "/core/rend/dx11/dx11_shaders.cpp",
		std::ios::binary);
	if (!input) { error = "cannot open production dx11_shaders.cpp"; return false; }
	std::ostringstream stream;
	stream << input.rdbuf();
	std::string common;
	std::string pixel;
	std::string vertex;
	if (!ExtractRawString(stream.str(), "PixelShaderCommon", common)
		|| !ExtractRawString(stream.str(), "PixelShader", pixel)
		|| !ExtractRawString(stream.str(), "VertexShader", vertex))
	{
		error = "cannot extract production motion shaders";
		return false;
	}
	PixelInclude includes(common);
	ComPtr<ID3DBlob> vsCode;
	ComPtr<ID3DBlob> psCode;
	if (!CompileProductionVertex(vertex, false, vsCode, error)
		|| !CompilePixel(pixel, includes, true, false, psCode, error)) return false;
	std::ifstream naomi2Input(std::string(NEURAL_SOURCE_DIR)
		+ "/core/rend/dx11/dx11_naomi2.cpp", std::ios::binary);
	if (!naomi2Input) { error = "cannot open production dx11_naomi2.cpp"; return false; }
	std::ostringstream naomi2Stream;
	naomi2Stream << naomi2Input.rdbuf();
	std::string naomi2Vertex;
	std::string naomi2Color;
	if (!ExtractRawString(naomi2Stream.str(), "DX11N2VertexShader", naomi2Vertex)
		|| !ExtractRawString(naomi2Stream.str(), "DX11N2ColorShader", naomi2Color))
	{
		error = "cannot extract production Naomi 2 motion shader";
		return false;
	}
	ComPtr<ID3DBlob> naomi2VsCode;
	if (!CompileProductionVertex(naomi2Vertex + "\n" + naomi2Color, true,
		naomi2VsCode, error)) return false;
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11VertexShader> naomi2Vs;
	ComPtr<ID3D11PixelShader> ps;
	HRESULT hr = surface.device->CreateVertexShader(vsCode->GetBufferPointer(),
		vsCode->GetBufferSize(), nullptr, vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateVertexShader(
		naomi2VsCode->GetBufferPointer(), naomi2VsCode->GetBufferSize(), nullptr,
		naomi2Vs.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(psCode->GetBufferPointer(),
		psCode->GetBufferSize(), nullptr, ps.GetAddressOf());
	const D3D11_INPUT_ELEMENT_DESC elements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
			D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ComPtr<ID3D11InputLayout> layout;
	ComPtr<ID3D11InputLayout> naomi2Layout;
	if (SUCCEEDED(hr)) hr = surface.device->CreateInputLayout(elements,
		static_cast<UINT>(std::size(elements)), vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
		layout.GetAddressOf());
	const D3D11_INPUT_ELEMENT_DESC naomi2Elements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
			D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	if (SUCCEEDED(hr)) hr = surface.device->CreateInputLayout(naomi2Elements,
		static_cast<UINT>(std::size(naomi2Elements)), naomi2VsCode->GetBufferPointer(),
		naomi2VsCode->GetBufferSize(), naomi2Layout.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create production motion shaders/layout", hr); return false; }

	const ProductionVertex vertices[] = {
		{-.5f, .5f, .0025f, 0xffffffffu, 0, 0, 0},
		{ .5f, .5f, .0025f, 0xffffffffu, 0, 1, 0},
		{-.5f,-.5f, .0025f, 0xffffffffu, 0, 0, 1},
		{ .5f,-.5f, .0025f, 0xffffffffu, 0, 1, 1},
	};
	auto makePrevious = [](float deltaX, float deltaY, float valid) {
		return std::array<PreviousPosition, 4>{{
			{-.5f + deltaX, .5f + deltaY, .0025f, valid},
			{ .5f + deltaX, .5f + deltaY, .0025f, valid},
			{-.5f + deltaX,-.5f + deltaY, .0025f, valid},
			{ .5f + deltaX,-.5f + deltaY, .0025f, valid},
		}};
	};
	const auto trustedPrevious = makePrevious(-8.f / Width, -6.f / Height, 1.f);
	const auto invalidPrevious = makePrevious(-8.f / Width, -6.f / Height, 0.f);
	const auto oversizedPrevious = makePrevious(3.f, 0.f, 1.f);
	const Naomi2ProductionVertex naomi2Vertices[] = {
		{-.5f, .5f, 1.f, 0xffffffffu, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{ .5f, .5f, 1.f, 0xffffffffu, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
		{-.5f,-.5f, 1.f, 0xffffffffu, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
		{ .5f,-.5f, 1.f, 0xffffffffu, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1},
	};
	const std::array<PreviousPosition, 4> naomi2Previous{{
		{-.5f, .5f, 1.f, 1.f}, { .5f, .5f, 1.f, 1.f},
		{-.5f,-.5f, 1.f, 1.f}, { .5f,-.5f, 1.f, 1.f},
	}};
	auto createBuffer = [&](const void *data, UINT bytes, UINT bind, D3D11_USAGE usage,
		ID3D11Buffer **buffer) {
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = bytes;
		desc.Usage = usage;
		desc.BindFlags = bind;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = data;
		return surface.device->CreateBuffer(&desc, &initial, buffer);
	};
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> naomi2VertexBuffer;
	ComPtr<ID3D11Buffer> previousBuffer;
	hr = createBuffer(vertices, sizeof(vertices), D3D11_BIND_VERTEX_BUFFER,
		D3D11_USAGE_IMMUTABLE, vertexBuffer.GetAddressOf());
	if (SUCCEEDED(hr)) hr = createBuffer(trustedPrevious.data(), sizeof(trustedPrevious),
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT, previousBuffer.GetAddressOf());
	if (SUCCEEDED(hr)) hr = createBuffer(naomi2Vertices, sizeof(naomi2Vertices),
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE,
		naomi2VertexBuffer.GetAddressOf());

	struct alignas(16) VertexConstants {
		float matrix[16]; float planes[16]; float renderSize[2]; float padding[2];
	} vertexConstants{};
	vertexConstants.matrix[0] = vertexConstants.matrix[5] =
		vertexConstants.matrix[10] = vertexConstants.matrix[15] = 1.f;
	vertexConstants.planes[0] = 1.f; vertexConstants.planes[3] = 1.f;
	vertexConstants.planes[5] = 1.f; vertexConstants.planes[7] = 1.f;
	vertexConstants.planes[8] = -1.f; vertexConstants.planes[11] = 1.f;
	vertexConstants.planes[13] = -1.f; vertexConstants.planes[15] = 1.f;
	vertexConstants.renderSize[0] = static_cast<float>(Width);
	vertexConstants.renderSize[1] = static_cast<float>(Height);
	struct alignas(16) PixelConstants { float values[24]; } pixelConstants{};
	struct alignas(16) PolyConstants {
		float clip[4]; float palette; float trilinear; float confidence;
		std::uint32_t drawId; float bias; std::uint32_t previousDrawId; float padding[2];
	} polyConstants{};
	struct alignas(16) Naomi2PolyConstants {
		float modelView[16];
		float normal[16];
		float projection[16];
		std::int32_t values[4];
		float gloss[4];
		std::int32_t constantColor[4];
		float previousModelView[16];
		float previousProjection[16];
		float previousValid[4];
	} naomi2Constants{};
	static_assert(sizeof(Naomi2PolyConstants) == 384);
	auto setIdentity = [](float *matrix) {
		matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
	};
	setIdentity(naomi2Constants.modelView);
	setIdentity(naomi2Constants.normal);
	setIdentity(naomi2Constants.projection);
	setIdentity(naomi2Constants.previousModelView);
	setIdentity(naomi2Constants.previousProjection);
	naomi2Constants.modelView[12] = 8.f / Width;
	naomi2Constants.modelView[13] = 6.f / Height;
	naomi2Constants.previousValid[0] = 1.f;
	polyConstants.trilinear = 1.f;
	polyConstants.confidence = 1.f;
	polyConstants.drawId = 7;
	polyConstants.previousDrawId = 5;
	auto constantBuffer = [&](const void *data, UINT bytes, ID3D11Buffer **buffer) {
		return createBuffer(data, bytes, D3D11_BIND_CONSTANT_BUFFER,
			D3D11_USAGE_IMMUTABLE, buffer);
	};
	ComPtr<ID3D11Buffer> vertexConstantBuffer;
	ComPtr<ID3D11Buffer> pixelConstantBuffer;
	ComPtr<ID3D11Buffer> polyConstantBuffer;
	ComPtr<ID3D11Buffer> naomi2ConstantBuffer;
	if (SUCCEEDED(hr)) hr = constantBuffer(&vertexConstants, sizeof(vertexConstants),
		vertexConstantBuffer.GetAddressOf());
	if (SUCCEEDED(hr)) hr = constantBuffer(&pixelConstants, sizeof(pixelConstants),
		pixelConstantBuffer.GetAddressOf());
	if (SUCCEEDED(hr)) hr = constantBuffer(&polyConstants, sizeof(polyConstants),
		polyConstantBuffer.GetAddressOf());
	if (SUCCEEDED(hr)) hr = createBuffer(&naomi2Constants, sizeof(naomi2Constants),
		D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT,
		naomi2ConstantBuffer.GetAddressOf());
	D3D11_RASTERIZER_DESC rasterDesc{};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthClipEnable = TRUE;
	ComPtr<ID3D11RasterizerState> raster;
	if (SUCCEEDED(hr)) hr = surface.device->CreateRasterizerState(&rasterDesc,
		raster.GetAddressOf());
	D3D11_DEPTH_STENCIL_DESC depthDesc{};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	ComPtr<ID3D11DepthStencilState> depthState;
	if (SUCCEEDED(hr)) hr = surface.device->CreateDepthStencilState(&depthDesc,
		depthState.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create production motion buffers/state", hr); return false; }
	Targets targets;
	if (!CreateTargets(surface.device.Get(), targets, error)) return false;

	auto render = [&](const auto& positions, bool trusted, bool oversized) {
		surface.context->UpdateSubresource(previousBuffer.Get(), 0, nullptr,
			positions.data(), 0, 0);
		const float zero[4]{};
		const float one[4] = {1,1,1,1};
		surface.context->ClearRenderTargetView(targets.guidanceTargets[0].Get(), zero);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[1].Get(), one);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[2].Get(), zero);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[3].Get(), zero);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[4].Get(), zero);
		surface.context->ClearDepthStencilView(targets.depthTarget.Get(), D3D11_CLEAR_DEPTH, 0.f, 0);
		ID3D11RenderTargetView *views[] = {targets.guidanceTargets[0].Get(),
			targets.guidanceTargets[1].Get(), targets.guidanceTargets[2].Get(),
			targets.guidanceTargets[3].Get(), targets.guidanceTargets[4].Get()};
		surface.context->OMSetRenderTargets(static_cast<UINT>(std::size(views)), views,
			targets.depthTarget.Get());
		D3D11_VIEWPORT viewport{0,0,static_cast<float>(Width),static_cast<float>(Height),0,1};
		surface.context->RSSetViewports(1, &viewport);
		surface.context->RSSetState(raster.Get());
		surface.context->OMSetDepthStencilState(depthState.Get(), 0);
		ID3D11Buffer *buffers[] = {vertexBuffer.Get(), previousBuffer.Get()};
		const UINT strides[] = {sizeof(ProductionVertex), sizeof(PreviousPosition)};
		const UINT offsets[] = {0,0};
		surface.context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
		surface.context->IASetInputLayout(layout.Get());
		surface.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		surface.context->VSSetShader(vs.Get(), nullptr, 0);
		ID3D11Buffer *vsConstants[] = {vertexConstantBuffer.Get()};
		surface.context->VSSetConstantBuffers(0, 1, vsConstants);
		surface.context->PSSetShader(ps.Get(), nullptr, 0);
		ID3D11Buffer *psConstants[] = {pixelConstantBuffer.Get(), polyConstantBuffer.Get()};
		surface.context->PSSetConstantBuffers(0, 2, psConstants);
		surface.context->Draw(4, 0);
		surface.context->Flush();
		return ReadProductionGuidance(surface.device.Get(), surface.context.Get(), targets,
			result, trusted, oversized, error);
	};
	if (!render(trustedPrevious, true, false)
		|| !render(invalidPrevious, false, false)
		|| !render(oversizedPrevious, false, true)) return false;
	auto renderNaomi2 = [&](float valid, ProductionMotionResult& readback,
		bool trusted) {
		naomi2Constants.previousValid[0] = valid;
		surface.context->UpdateSubresource(previousBuffer.Get(), 0, nullptr,
			naomi2Previous.data(), 0, 0);
		surface.context->UpdateSubresource(naomi2ConstantBuffer.Get(), 0, nullptr,
			&naomi2Constants, 0, 0);
		const float zero[4]{};
		const float one[4] = {1,1,1,1};
		surface.context->ClearRenderTargetView(targets.guidanceTargets[0].Get(), zero);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[1].Get(), one);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[2].Get(), zero);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[3].Get(), zero);
		surface.context->ClearRenderTargetView(targets.guidanceTargets[4].Get(), zero);
		surface.context->ClearDepthStencilView(targets.depthTarget.Get(), D3D11_CLEAR_DEPTH, 0.f, 0);
		ID3D11RenderTargetView *views[] = {targets.guidanceTargets[0].Get(),
			targets.guidanceTargets[1].Get(), targets.guidanceTargets[2].Get(),
			targets.guidanceTargets[3].Get(), targets.guidanceTargets[4].Get()};
		surface.context->OMSetRenderTargets(static_cast<UINT>(std::size(views)), views,
			targets.depthTarget.Get());
		D3D11_VIEWPORT viewport{0,0,static_cast<float>(Width),static_cast<float>(Height),0,1};
		surface.context->RSSetViewports(1, &viewport);
		surface.context->RSSetState(raster.Get());
		surface.context->OMSetDepthStencilState(depthState.Get(), 0);
		ID3D11Buffer *buffers[] = {naomi2VertexBuffer.Get(), previousBuffer.Get()};
		const UINT strides[] = {sizeof(Naomi2ProductionVertex), sizeof(PreviousPosition)};
		const UINT offsets[] = {0,0};
		surface.context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
		surface.context->IASetInputLayout(naomi2Layout.Get());
		surface.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		surface.context->VSSetShader(naomi2Vs.Get(), nullptr, 0);
		ID3D11Buffer *vsConstants[] = {vertexConstantBuffer.Get(),
			naomi2ConstantBuffer.Get()};
		surface.context->VSSetConstantBuffers(0, 2, vsConstants);
		surface.context->PSSetShader(ps.Get(), nullptr, 0);
		ID3D11Buffer *psConstants[] = {pixelConstantBuffer.Get(), polyConstantBuffer.Get()};
		surface.context->PSSetConstantBuffers(0, 2, psConstants);
		surface.context->Draw(4, 0);
		surface.context->Flush();
		return ReadProductionGuidance(surface.device.Get(), surface.context.Get(), targets,
			readback, trusted, false, error);
	};
	ProductionMotionResult naomi2Trusted;
	ProductionMotionResult naomi2Invalid;
	if (!renderNaomi2(1.f, naomi2Trusted, true)
		|| !renderNaomi2(0.f, naomi2Invalid, false)) return false;
	result.naomi2X = naomi2Trusted.trustedX;
	result.naomi2Y = naomi2Trusted.trustedY;
	result.naomi2Mask = naomi2Trusted.trustedMask;
	result.naomi2Confidence = naomi2Trusted.trustedConfidence;
	result.naomi2InvalidX = naomi2Invalid.invalidX;
	result.naomi2InvalidY = naomi2Invalid.invalidY;
	result.naomi2InvalidMask = naomi2Invalid.invalidMask;
	result.naomi2InvalidConfidence = naomi2Invalid.invalidConfidence;
	const auto close = [](float a, float b) { return std::abs(a - b) <= .01f; };
	result.analyticTruth = close(result.trustedX, -4.f) && close(result.trustedY, 3.f)
		&& result.trustedMask == 0 && result.trustedConfidence == 255
		&& result.trustedDrawId == 7 && result.trustedPreviousDrawId == 5;
	result.invalidProtected = result.invalidX == 0.f && result.invalidY == 0.f
		&& result.invalidMask == 255 && result.invalidConfidence == 0;
	result.magnitudeProtected = result.oversizedX == 0.f && result.oversizedY == 0.f
		&& result.oversizedMask == 255 && result.oversizedConfidence == 0;
	result.naomi2AnalyticTruth = close(result.naomi2X, -4.f)
		&& close(result.naomi2Y, 3.f) && result.naomi2Mask == 0
		&& result.naomi2Confidence == 255;
	result.naomi2InvalidProtected = result.naomi2InvalidX == 0.f
		&& result.naomi2InvalidY == 0.f && result.naomi2InvalidMask == 255
		&& result.naomi2InvalidConfidence == 0;
	const bool passed = result.analyticTruth && result.invalidProtected
		&& result.magnitudeProtected && result.naomi2AnalyticTruth
		&& result.naomi2InvalidProtected;
	if (!passed)
	{
		std::ostringstream detail;
		detail << result.surface << " production motion samples: trusted=["
			<< result.trustedX << ',' << result.trustedY << "] mask="
			<< static_cast<unsigned>(result.trustedMask) << " confidence="
			<< static_cast<unsigned>(result.trustedConfidence) << " draw="
			<< result.trustedDrawId << " previous_draw=" << result.trustedPreviousDrawId
			<< " invalid=[" << result.invalidX << ','
			<< result.invalidY << "] mask=" << static_cast<unsigned>(result.invalidMask)
			<< " confidence=" << static_cast<unsigned>(result.invalidConfidence)
			<< " oversized=[" << result.oversizedX << ',' << result.oversizedY
			<< "] mask=" << static_cast<unsigned>(result.oversizedMask)
			<< " confidence=" << static_cast<unsigned>(result.oversizedConfidence)
			<< " naomi2=[" << result.naomi2X << ',' << result.naomi2Y
			<< "] mask=" << static_cast<unsigned>(result.naomi2Mask)
			<< " confidence=" << static_cast<unsigned>(result.naomi2Confidence)
			<< " naomi2_invalid=[" << result.naomi2InvalidX << ','
			<< result.naomi2InvalidY << "] mask="
			<< static_cast<unsigned>(result.naomi2InvalidMask)
			<< " confidence=" << static_cast<unsigned>(result.naomi2InvalidConfidence);
		error = detail.str();
	}
	return passed;
}

} // namespace neuraltest
