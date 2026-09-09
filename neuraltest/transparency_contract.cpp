// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/dx11/oit/native_effect_blend.h"
#include "rend/neural/remake_oit_effects.h"
#include "rend/neural/remake_effect_identity.h"

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

template<class T> using TestComPtr = Microsoft::WRL::ComPtr<T>;

namespace neuraltest {
namespace {

constexpr UINT Width = 4;
constexpr UINT Height = 1;
constexpr std::uint32_t Eol = 0xffffffffu;

bool EffectIdentityControls(std::string& error)
{
 using namespace flycast::rend::neural;
 const EffectIdentitySource source{1,20,100};
 std::vector<EffectIdentityPixel> pixels{{0xff112233,0x3e800000,0,1},
  {0xff445566,0x3f000000,1,Eol}};
 std::vector<EffectIdentityPoly> poly{{0x12345678,0x87654321}};
 std::vector<std::uint32_t> base,other;
 auto run=[&](const auto& p,const auto& pp,const auto& heads,const auto& state,
  const EffectIdentitySource& id,std::vector<std::uint32_t>& out,unsigned layers=8){
  return CanonicalEffectIdentity(id,heads,p,pp,state,layers,out,error);
 };
 const std::vector<std::uint32_t> heads{0,Eol},state{1,2,3};
 if(!run(pixels,poly,heads,state,source,base))return false;
 std::ostringstream archive(std::ios::binary);
 if(!WriteEffectIdentity(archive,base)) {error="effect identity archive write";return false;}
 for(unsigned mutation=0;mutation<5;++mutation) {
  auto bytes=archive.str();
  if(mutation==1)bytes[0]^=1;
  if(mutation==2)bytes[12]^=1;
  if(mutation==3)bytes.pop_back();
  if(mutation==4)bytes.push_back('x');
  std::istringstream in(bytes,std::ios::binary);
  if(MatchEffectIdentity(in,base,error)!=(mutation==0)) {
   error="effect identity archive negative control";return false;
  }
 }
 auto moved=pixels;std::swap(moved[0],moved[1]);moved[1].next=0;moved[0].next=Eol;
 moved.push_back({99,0,0,Eol}); // Unreachable allocation is irrelevant.
 if(!run(moved,poly,std::vector<std::uint32_t>{1,Eol},state,source,other)||base!=other)
  {error="effect identity rejected relocated equivalent stack";return false;}
 for(unsigned mutation=0;mutation<6;++mutation) {
  auto p=pixels;auto pp=poly;auto s=state;auto id=source;
  if(mutation==0)p[0].color^=1;
  if(mutation==1)pp[0].primary^=1u<<29;
  if(mutation==2)p[0].sequence^=0x80000000u;
  if(mutation==3)p[0].depthBits=0x3f400000;
  if(mutation==4)++id.ordinal;
  if(mutation==5)++s[0];
  if(!run(p,pp,heads,s,id,other)||base==other)
   {error="effect identity missed semantic mutation "+std::to_string(mutation);return false;}
 }
 const char* failures[]={"effect-identity-cycle","effect-identity-pointer-range",
  "effect-identity-polygon-range","effect-identity-invalid-depth","effect-identity-truncated-stack"};
 for(unsigned mutation=0;mutation<5;++mutation) {
  auto p=pixels;
  if(mutation==0)p[1].next=0;
  if(mutation==1)p[1].next=999;
  if(mutation==2)p[0].sequence=1u<<17;
  if(mutation==3)p[0].depthBits=0x7fc00000;
  if(run(p,poly,heads,state,source,other,mutation==4?1:8)||!other.empty()||error!=failures[mutation])
   {error="effect identity accepted malformed stack";return false;}
 }
 pixels[1].depthBits=pixels[0].depthBits;pixels[1].sequence=pixels[0].sequence;
 if(!run(pixels,poly,heads,state,source,base))return false;
 pixels[0].next=Eol;pixels[1].next=0;
 if(!run(pixels,poly,std::vector<std::uint32_t>{1,Eol},state,source,other)||base==other)
  {error="effect identity erased equal-key blend order";return false;}
 error.clear();return true;
}

struct Surface {
	TestComPtr<ID3D11Device> device;
	TestComPtr<ID3D11DeviceContext> context;
	TestComPtr<ID3D12Device> device12;
	TestComPtr<ID3D12CommandQueue> queue12;
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
	TestComPtr<IDXGIDevice> dxgi;
	TestComPtr<IDXGIAdapter> adapter;
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
		if (std::strcmp(fileName, "native_effect_blend.hlsl") == 0) {
			*data = NativeEffectBlendHlsl; *bytes = sizeof(NativeEffectBlendHlsl) - 1; return S_OK;
		}
		if (std::strcmp(fileName, "oit_header.hlsl") != 0) return E_FAIL;
		*data = header_.data(); *bytes = static_cast<UINT>(header_.size()); return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }
private:
	const std::string& header_;
};

bool CreateTexture(ID3D11Device *device, DXGI_FORMAT format, UINT bindFlags,
	const void *data, UINT pitch, TestComPtr<ID3D11Texture2D>& texture, std::string& error)
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
	TestComPtr<ID3D11Texture2D> staging;
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
	if(!EffectIdentityControls(error))return false;
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
	TestComPtr<ID3DBlob> vsCode;
	TestComPtr<ID3DBlob> psCode;
	TestComPtr<ID3DBlob> diagnostics;
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
	TestComPtr<ID3D11VertexShader> vs;
	TestComPtr<ID3D11PixelShader> ps;
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
	TestComPtr<ID3D11Texture2D> opaqueTexture;
	TestComPtr<ID3D11Texture2D> pointerTexture;
	TestComPtr<ID3D11Texture2D> colorTexture;
	TestComPtr<ID3D11Texture2D> maskTexture;
	if (!CreateTexture(surface.device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_SHADER_RESOURCE, opaque.data(), Width * 4, opaqueTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R32_UINT,
		D3D11_BIND_UNORDERED_ACCESS, pointers.data(), Width * 4, pointerTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_RENDER_TARGET, nullptr, 0, colorTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM,
		D3D11_BIND_RENDER_TARGET, nullptr, 0, maskTexture, error)) return false;
	TestComPtr<ID3D11ShaderResourceView> opaqueView;
	TestComPtr<ID3D11UnorderedAccessView> pointerUav;
	TestComPtr<ID3D11RenderTargetView> colorTarget;
	TestComPtr<ID3D11RenderTargetView> maskTarget;
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
	TestComPtr<ID3D11Buffer> pixelBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&bufferDesc, &pixelData,
		pixelBuffer.GetAddressOf());
	D3D11_UNORDERED_ACCESS_VIEW_DESC pixelUavDesc{};
	pixelUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	pixelUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	pixelUavDesc.Buffer.NumElements = static_cast<UINT>(pixels.size());
	TestComPtr<ID3D11UnorderedAccessView> pixelUav;
	if (SUCCEEDED(hr)) hr = surface.device->CreateUnorderedAccessView(pixelBuffer.Get(),
		&pixelUavDesc, pixelUav.GetAddressOf());

	bufferDesc.ByteWidth = static_cast<UINT>(sizeof(poly));
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = sizeof(std::int32_t) * 2;
	D3D11_SUBRESOURCE_DATA polyData{poly.data(), 0, 0};
	TestComPtr<ID3D11Buffer> polyBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&bufferDesc, &polyData,
		polyBuffer.GetAddressOf());
	D3D11_SHADER_RESOURCE_VIEW_DESC polyViewDesc{};
	polyViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	polyViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	polyViewDesc.Buffer.NumElements = 1;
	TestComPtr<ID3D11ShaderResourceView> polyView;
	if (SUCCEEDED(hr)) hr = surface.device->CreateShaderResourceView(polyBuffer.Get(),
		&polyViewDesc, polyView.GetAddressOf());

	D3D11_BUFFER_DESC constantsDesc{};
	constantsDesc.ByteWidth = sizeof(float) * 24;
	constantsDesc.Usage = D3D11_USAGE_DEFAULT;
	constantsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	std::array<float, 24> constants{};
	constants[21] = 1.f;
	D3D11_SUBRESOURCE_DATA constantsData{constants.data(), 0, 0};
	TestComPtr<ID3D11Buffer> constantBuffer;
	if (SUCCEEDED(hr)) hr = surface.device->CreateBuffer(&constantsDesc, &constantsData,
		constantBuffer.GetAddressOf());
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	TestComPtr<ID3D11SamplerState> sampler;
	if (SUCCEEDED(hr)) hr = surface.device->CreateSamplerState(&samplerDesc,
		sampler.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create transparency resources", hr); return false; }
	// Exercise the production source-owned snapshot and deferred resolver at its
	// supported extent. Mutate original pointers after capture; replay must still
	// preserve the captured effects, repeat without consuming them, and reject a
	// different producer. This fixture starts with depth-filtered fragments; it
	// does not claim to test geometry occlusion.
	std::vector<std::uint32_t> fullPointers(640*512), fullBackground(640*480,0xff202020u);
	for(std::size_t i=0;i<fullPointers.size();++i)fullPointers[i]=pointers[i%4];
	D3D11_TEXTURE2D_DESC fullDesc{};
	fullDesc.Width=640;fullDesc.Height=512;fullDesc.MipLevels=fullDesc.ArraySize=fullDesc.SampleDesc.Count=1;
	fullDesc.Format=DXGI_FORMAT_R32_UINT;fullDesc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
	D3D11_SUBRESOURCE_DATA fullData{fullPointers.data(),640*4,0};
	TestComPtr<ID3D11Texture2D> fullPointerTexture,fullColorTexture;
	hr=surface.device->CreateTexture2D(&fullDesc,&fullData,fullPointerTexture.GetAddressOf());
	fullDesc.Height=480;fullDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;fullDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
	fullData.pSysMem=fullBackground.data();
	if(SUCCEEDED(hr))hr=surface.device->CreateTexture2D(&fullDesc,&fullData,fullColorTexture.GetAddressOf());
	if(FAILED(hr)){error=HrText("create source effects fixture",hr);return false;}
	const flycast::rend::neural::ProducerIdentity effectIdentity{1,20,100};
	const auto effects=flycast::rend::neural::RemakeOitEffects::Capture(surface.device.Get(),surface.context.Get(),
		effectIdentity,pixelBuffer.Get(),fullPointerTexture.Get(),polyBuffer.Get(),constantBuffer.Get(),ps.Get(),vs.Get(),nullptr,8,0);
	if(!effects){error="source effects capture failed";return false;}
	std::vector<std::uint32_t> gpuIdentity, repeatedIdentity;
	if(!effects->ReadIdentityForEvidence(surface.device.Get(),surface.context.Get(),effectIdentity,gpuIdentity,error))return false;
	std::vector<flycast::rend::neural::EffectIdentityPixel> expectedPixels(pixels.size());
	std::memcpy(expectedPixels.data(),pixels.data(),sizeof(pixels));
	std::vector<flycast::rend::neural::EffectIdentityPoly> expectedPoly(1);
	std::memcpy(expectedPoly.data(),poly.data(),sizeof(poly));
	std::vector<std::uint32_t> expectedState(2+constants.size());expectedState[0]=1;expectedState[1]=0;
	std::memcpy(expectedState.data()+2,constants.data(),sizeof(constants));
	const std::vector<std::uint32_t> expectedHeads(fullPointers.begin(),fullPointers.begin()+640*480);
	if(!flycast::rend::neural::CanonicalEffectIdentity({1,20,100},expectedHeads,expectedPixels,expectedPoly,
		expectedState,8,repeatedIdentity,error)||gpuIdentity!=repeatedIdentity) {
		error="effect GPU identity differs from uploaded fixture truth";return false;
	}
	if(effects->ReadIdentityForEvidence(surface.device.Get(),surface.context.Get(),{1,21,101},repeatedIdentity,error)
		||!repeatedIdentity.empty()||error!="effect-evidence-contract") {
		error="effect GPU identity accepted wrong source";return false;
	}
	const auto usage=flycast::rend::neural::CountRemakeEffects(std::array<const flycast::rend::neural::RemakeOitEffects*,3>{effects.get(),nullptr,effects.get()});
	const auto emptyUsage=flycast::rend::neural::CountRemakeEffects(std::array<const flycast::rend::neural::RemakeOitEffects*,3>{});
	const auto otherEffects=flycast::rend::neural::RemakeOitEffects::Capture(surface.device.Get(),surface.context.Get(),
		effectIdentity,pixelBuffer.Get(),fullPointerTexture.Get(),polyBuffer.Get(),constantBuffer.Get(),ps.Get(),vs.Get());
	const auto distinctUsage=flycast::rend::neural::CountRemakeEffects(std::array<const flycast::rend::neural::RemakeOitEffects*,3>{effects.get(),otherEffects.get(),effects.get()});
	if(usage.snapshots!=1||usage.objects!=6||usage.logicalBytes!=sizeof(pixels)+sizeof(poly)+sizeof(constants)+640u*480*4
		||emptyUsage.snapshots||emptyUsage.objects||emptyUsage.logicalBytes
		||!otherEffects||distinctUsage.snapshots!=2||distinctUsage.objects!=12||distinctUsage.logicalBytes!=usage.logicalBytes*2) {
		error="source effects resource accounting or duplicate-owner control failed";return false;
	}
	Surface otherDevice;
	if(!CreateSurface(false,otherDevice,error))return false;
	if(flycast::rend::neural::RemakeOitEffects::Capture(otherDevice.device.Get(),surface.context.Get(),
		effectIdentity,pixelBuffer.Get(),fullPointerTexture.Get(),polyBuffer.Get(),constantBuffer.Get(),ps.Get(),vs.Get())) {
		error="source effects accepted a different device";return false;
	}
	std::fill(fullPointers.begin(),fullPointers.end(),Eol);
	{
		using namespace flycast::rend::neural;
		const EffectIdentityPoly alphaPoly{(4u<<29)|(5u<<26),0xffffffffu};
		D3D11_BUFFER_DESC alphaDesc{};polyBuffer->GetDesc(&alphaDesc);
		D3D11_SUBRESOURCE_DATA initial{&alphaPoly,0,0};TestComPtr<ID3D11Buffer> alphaBuffer;
		if(FAILED(surface.device->CreateBuffer(&alphaDesc,&initial,alphaBuffer.GetAddressOf()))) {error="alpha ownership fixture buffer";return false;}
		auto owned=RemakeOitEffects::Capture(surface.device.Get(),surface.context.Get(),effectIdentity,
			pixelBuffer.Get(),fullPointerTexture.Get(),alphaBuffer.Get(),constantBuffer.Get(),ps.Get(),vs.Get(),nullptr,8,0,{alphaPoly});
		if(!owned){error="alpha ownership fixture capture";return false;}
		const std::vector<AlphaEffectSelection> selection{{0,alphaPoly}};
		for(bool exclude:{true,false,true}) {
			::ComPtr<ID3D11Texture2D> result;::ComPtr<ID3D11ShaderResourceView> view;
			if(!owned->Compose(surface.device.Get(),surface.context.Get(),effectIdentity,fullColorTexture.Get(),result,view,
				exclude?selection:std::vector<AlphaEffectSelection>{})) {error="alpha ownership GPU compose";return false;}
			D3D11_TEXTURE2D_DESC desc{};result->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
			TestComPtr<ID3D11Texture2D> read;
			if(FAILED(surface.device->CreateTexture2D(&desc,nullptr,read.GetAddressOf())))return false;
			surface.context->CopyResource(read.Get(),result);D3D11_MAPPED_SUBRESOURCE mapped{};
			if(FAILED(surface.context->Map(read.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
			unsigned changes=0;
			for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x) {
				const auto* pixel=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
				changes+=pixel[0]!=32||pixel[1]!=32||pixel[2]!=32;
			}
			surface.context->Unmap(read.Get(),0);
			if((exclude&&changes)||(!exclude&&!changes)){error="alpha exclusion changes wrong pixels or mutates original stack";return false;}
		}
	}
	if(effects->RetainNativeBackgroundForEvidence(otherDevice.device.Get(),surface.context.Get(),fullColorTexture.Get())
		||effects->NativeBackgroundForEvidence(effectIdentity)
		||!effects->RetainNativeBackgroundForEvidence(surface.device.Get(),surface.context.Get(),fullColorTexture.Get())
		||effects->RetainNativeBackgroundForEvidence(surface.device.Get(),surface.context.Get(),fullColorTexture.Get())
		||effects->NativeBackgroundForEvidence({1,21,101})
		||effects->ObjectCount()!=7||effects->LogicalBytes()!=usage.logicalBytes+640u*480*4) {
		error="native background ownership/source/repeat/accounting control failed";return false;
	}
	// Mutate the source after capture; subsequent replay must use the owned copy.
	auto changedBackground=fullBackground;std::fill(changedBackground.begin(),changedBackground.end(),0);
	surface.context->UpdateSubresource(fullColorTexture.Get(),0,nullptr,changedBackground.data(),640*4,0);
	surface.context->UpdateSubresource(fullPointerTexture.Get(),0,nullptr,fullPointers.data(),640*4,0);
	if(!effects->ReadIdentityForEvidence(surface.device.Get(),surface.context.Get(),effectIdentity,repeatedIdentity,error)
		||gpuIdentity!=repeatedIdentity) {error="effect GPU identity changed with original pointer mutation";return false;}
	::ComPtr<ID3D11Texture2D> effectOutput;
	::ComPtr<ID3D11ShaderResourceView> effectView;
	if(effects->Compose(surface.device.Get(),surface.context.Get(),{1,21,101},fullColorTexture.Get(),effectOutput,effectView)) {
		error="source effects accepted wrong producer";return false;
	}
	for(unsigned repeat=0;repeat<2;++repeat) {
		if(!effects->Compose(surface.device.Get(),surface.context.Get(),effectIdentity,effects->NativeBackgroundForEvidence(effectIdentity),effectOutput,effectView)) {
			error="source effects replay failed";return false;
		}
		D3D11_TEXTURE2D_DESC readDesc{};effectOutput->GetDesc(&readDesc);
		readDesc.Usage=D3D11_USAGE_STAGING;readDesc.BindFlags=0;readDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
		TestComPtr<ID3D11Texture2D> readTexture;
		hr=surface.device->CreateTexture2D(&readDesc,nullptr,readTexture.GetAddressOf());
		if(FAILED(hr)){error=HrText("create effect readback",hr);return false;}
		surface.context->CopyResource(readTexture.Get(),effectOutput);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		hr=surface.context->Map(readTexture.Get(),0,D3D11_MAP_READ,0,&mapped);
		if(FAILED(hr)){error=HrText("map effect readback",hr);return false;}
		bool correct=true;
		for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x) {
			const auto* p=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
			const unsigned expectedRed[]={32,96,160,160}; // OIT pack is RGBA, high byte first.
			const unsigned expectedOther=x%4?255:32;
			correct=correct&&p[0]==expectedRed[x%4]&&p[1]==expectedOther&&p[2]==expectedOther&&p[3]==255;
		}
		surface.context->Unmap(readTexture.Get(),0);
		if(!correct){error="source effects snapshot/absent/additive/repeat pixels differ";return false;}
	}

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

	std::ifstream mergeInput(std::string(NEURAL_SOURCE_DIR)
		+ "/core/rend/dx11/dx11_shaders.cpp", std::ios::binary);
	std::ostringstream mergeSourceStream;
	mergeSourceStream << mergeInput.rdbuf();
	std::string mergePixel;
	if (!mergeInput || !ExtractRawString(mergeSourceStream.str(),
		"NeuralReactiveCoveragePixelShader", mergePixel))
	{
		error = "cannot extract production reactive merge shader";
		return false;
	}
	TestComPtr<ID3DBlob> mergeCode;
	hr = D3DCompile(mergePixel.data(), mergePixel.size(), "production-reactive-merge",
		nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		mergeCode.GetAddressOf(), diagnostics.ReleaseAndGetAddressOf());
	TestComPtr<ID3D11PixelShader> mergeShader;
	if (SUCCEEDED(hr)) hr = surface.device->CreatePixelShader(mergeCode->GetBufferPointer(),
		mergeCode->GetBufferSize(), nullptr, mergeShader.GetAddressOf());
	const std::array<std::uint8_t, Width> baseValues = {0, 0, 255, 0};
	const std::array<std::uint8_t, Width> coverageValues = {0, 255, 0, 255};
	TestComPtr<ID3D11Texture2D> baseTexture;
	TestComPtr<ID3D11Texture2D> coverageTexture;
	if (SUCCEEDED(hr) && (!CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM,
		D3D11_BIND_RENDER_TARGET, baseValues.data(), Width, baseTexture, error)
		|| !CreateTexture(surface.device.Get(), DXGI_FORMAT_R8_UNORM,
		D3D11_BIND_SHADER_RESOURCE, coverageValues.data(), Width, coverageTexture, error)))
		return false;
	TestComPtr<ID3D11RenderTargetView> baseTarget;
	TestComPtr<ID3D11ShaderResourceView> coverageView;
	if (SUCCEEDED(hr)) hr = surface.device->CreateRenderTargetView(baseTexture.Get(), nullptr,
		baseTarget.GetAddressOf());
	if (SUCCEEDED(hr)) hr = surface.device->CreateShaderResourceView(coverageTexture.Get(), nullptr,
		coverageView.GetAddressOf());
	if (FAILED(hr)) { error = HrText("create reactive merge resources", hr); return false; }
	ID3D11RenderTargetView *mergeTarget = baseTarget.Get();
	surface.context->OMSetRenderTargets(1, &mergeTarget, nullptr);
	surface.context->PSSetShader(mergeShader.Get(), nullptr, 0);
	ID3D11ShaderResourceView *coverageResource = coverageView.Get();
	surface.context->PSSetShaderResources(0, 1, &coverageResource);
	surface.context->Draw(4, 0);
	ID3D11ShaderResourceView *nullResource = nullptr;
	surface.context->PSSetShaderResources(0, 1, &nullResource);
	Image merged;
	if (!ReadMask(surface.device.Get(), surface.context.Get(), baseTexture.Get(), merged, error))
		return false;
	result.mergePreservesBase = merged.rgba[0] == 0 && merged.rgba[4] == 255
		&& merged.rgba[8] == 255 && merged.rgba[12] == 255;
	return result.emptyAndModifierClear && result.singleLayerReactive
		&& result.multiLayerReactive && result.wrongControlFailed && result.mergePreservesBase;
}

} // namespace neuraltest
