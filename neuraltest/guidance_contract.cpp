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
	std::array<ComPtr<ID3D11Texture2D>, 4> guidance;
	std::array<ComPtr<ID3D11RenderTargetView>, 4> guidanceTargets;
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
		DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R16_UINT};
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
			targets.guidanceTargets[3].Get()};
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

} // namespace neuraltest
