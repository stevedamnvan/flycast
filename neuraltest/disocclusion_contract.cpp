// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/dx11/remake_motion_raster.h"

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

bool RunRemakeMotionRasterFixture(bool on12,std::string& error)
{
 using namespace flycast::rend::neural;
 Surface surface;if(!CreateSurface(on12,surface,error))return false;
 RemakeMotionRaster raster;if(!raster.Initialize(surface.device.Get(),&D3DCompile,error))return false;
 const auto projection=[](float z){return 100.f/99.f-100.f/(99.f*z);};
 std::vector<float> current(640*480,projection(10)),previous=current;
 std::vector<std::uint16_t> ids(640*480,1);
 auto idView=[&](std::uint16_t id,ComPtr<ID3D11ShaderResourceView>& view,bool edge=false) {
  std::fill(ids.begin(),ids.end(),id);
  if(edge)ids[160*640+201]=2;
  D3D11_TEXTURE2D_DESC d{};d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
  d.Format=DXGI_FORMAT_R16_UINT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data{};data.pSysMem=ids.data();data.SysMemPitch=640*2;
  ComPtr<ID3D11Texture2D> tex;
  return SUCCEEDED(surface.device->CreateTexture2D(&d,&data,tex.GetAddressOf()))
   &&SUCCEEDED(surface.device->CreateShaderResourceView(tex.Get(),nullptr,view.GetAddressOf()));
 };
 auto read=[&](ID3D11Texture2D* texture,unsigned bytes,std::uint32_t& value) {
  D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture2D> staging;
  if(FAILED(surface.device->CreateTexture2D(&d,nullptr,staging.GetAddressOf())))return false;
  surface.context->CopyResource(staging.Get(),texture);
  D3D11_MAPPED_SUBRESOURCE mapped{};
  if(FAILED(surface.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
  value=0;std::memcpy(&value,static_cast<const char*>(mapped.pData)+160*mapped.RowPitch+200*bytes,bytes);
  surface.context->Unmap(staging.Get(),0);return true;
 };
 // Exercise actual retained depth textures on both graphics APIs, and compare
 // sampled outputs with the same bytes forced through fresh uploads.
 {
  RemakeDepthBuffer prior;prior.assign(640*480,projection(10));
  ComPtr<ID3D11ShaderResourceView> view;if(!idView(1,view))return false;
  float priorZ=10;
  for(unsigned step=0;step<4;++step) {
   const float nextZ=step%2?10.f:20.f;
   RemakeDepthBuffer next;next.assign(640*480,projection(nextZ));
   RemakeMotionStream stream;stream.indices={0,1,2};
   stream.vertices={{{100,100,nextZ},{100,100,priorZ},1,1,1},{{500,100,nextZ},{500,100,priorZ},1,1,1},{{100,400,nextZ},{100,400,priorZ},1,1,1}};
   RemakeRasterOutput retained;
   if(!raster.Render(surface.context.Get(),stream,next,prior,view.Get(),1,100,.1f,0,retained,error))return false;
   std::array<std::uint32_t,6> cached{};const unsigned bytes[]={4,1,2,1,2,4};
   for(unsigned i=0;i<6;++i)if(!read(retained.textures[i].Get(),bytes[i],cached[i]))return false;
   // A separate renderer forces all uploads without disturbing this renderer's
   // retained current depth, which must become previous on the next step.
   RemakeMotionRaster fresh;if(!fresh.Initialize(surface.device.Get(),&D3DCompile,error))return false;
   RemakeRasterOutput uncached;
   if(!fresh.Render(surface.context.Get(),stream,next,prior,view.Get(),1,100,.1f,0,uncached,error))return false;
   for(unsigned i=0;i<6;++i){std::uint32_t value=0;
    if(!read(uncached.textures[i].Get(),bytes[i],value)||value!=cached[i]) {
     error="remake depth ping-pong differs from forced upload step="+std::to_string(step);return false;
    }
   }
   if(cached[1]!=255||cached[3]!=0||cached[4]!=0){error="remake depth ping-pong lost expected history";return false;}
   prior=std::move(next);priorZ=nextZ;
  }
 }
 for(unsigned mode=0;mode<11;++mode) {
  RemakeMotionStream stream;stream.indices={0,1,2};
  stream.vertices={{{100,100,10},{100,100,10},1,1,1},{{500,100,10},{500,100,10},1,1,1},{{100,400,10},{100,400,10},1,1,1}};
  std::fill(current.begin(),current.end(),projection(10));previous=current;
  if(mode==1)for(auto& v:stream.vertices)v.previousScreen.x-=4;
  if(mode==2)std::fill(previous.begin(),previous.end(),1.f);
  if(mode==4)std::fill(current.begin(),current.end(),1.f);
  if(mode>=6) {
   stream.vertices[1].currentScreen.z=20;
   stream.vertices[2].currentScreen.z=30;
   stream.vertices[1].previousScreen.x-=4;
   for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x) {
    const float b=(x+.5f-100)/400,c=(y+.5f-100)/300,a=1-b-c;
    const float shift=mode>=7?(mode==10?6.f:.75f):0.f;
    const float sb=(x+.5f+shift-100)/400,sc=(y+.5f-100)/300,sa=1-sb-sc;
    current[y*640+x]=std::clamp(projection(1/(sa/10+sb/20+sc/30)),0.f,1.f);
   }
  }
  if(mode==8)current[160*640+201]=1;
  if(mode==9){current[160*640+199]=1;current[160*640+201]=1;}
  ComPtr<ID3D11ShaderResourceView> view;if(!idView(mode==3?2:1,view))return false;
  RemakeRasterOutput output;
  if(!raster.Render(surface.context.Get(),stream,current,previous,mode==5?nullptr:view.Get(),1,100,mode>=7?.0001f:.1f,0,output,error))return false;
  std::uint32_t motion=0,bias=0,confidence=0,id=0,reason=0,rasterBits=0;
  if(!read(output.textures[0].Get(),4,motion)||!read(output.textures[1].Get(),1,confidence)
   ||!read(output.textures[2].Get(),2,id)||!read(output.textures[3].Get(),1,bias)
   ||!read(output.textures[4].Get(),2,reason)||!read(output.textures[5].Get(),4,rasterBits))return false;
  const unsigned expectedReasons[]={0,0,6,5,1,5,0,0,1,1,1};
  float actualDepth;std::memcpy(&actualDepth,&rasterBits,4);
  if(reason!=expectedReasons[mode]||!std::isfinite(actualDepth)
   ||(mode<6&&std::abs(actualDepth-projection(10))>1e-6f)) {
   error="remake raster reason/depth diagnostic mismatch mode="+std::to_string(mode);return false;
  }
  bool valid=false;
  if(mode==0)valid=motion==0&&bias==0&&confidence==255&&id==1;
  if(mode==1)valid=std::abs(int(motion&65535)-int(FloatToHalf(-4)))<=1
   &&(motion>>16)==0&&bias==0&&confidence==255;
  if(mode>=2&&mode<=5)valid=motion==0&&bias==255&&confidence==0&&(mode!=4||id==0);
  if(mode==6||mode==7) {
   const float b=100.5f/400,c=60.5f/300,a=1-b-c;
   const float denominator=a/10+b/20+c/30;
   const auto hx=FloatToHalf((100*a/10+496*b/20+100*c/30)/denominator-200.5f);
   const auto hy=FloatToHalf((100*a/10+100*b/20+400*c/30)/denominator-160.5f);
   valid=std::abs(int(motion&65535)-int(hx))<=1&&std::abs(int(motion>>16)-int(hy))<=1&&bias==0&&confidence==255;
  }
  if(mode>=8)valid=motion==0&&bias==255&&confidence==0&&id==0;
  if(!valid){error="remake raster mode="+std::to_string(mode)+" motion="+std::to_string(motion)+" bias="+std::to_string(bias)+" confidence="+std::to_string(confidence);return false;}
 }
 for(unsigned mode=0;mode<4;++mode) {
  RemakeMotionStream stream;stream.indices={0,1,2};
  stream.vertices={{{100,100,10},{100,100,10},1,1,1},{{500,100,20},{500,100,20},1,1,1},{{100,400,30},{100,400,30},1,1,1}};
  for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x) {
   const float b=(x+.5f+.75f-100)/400,c=(y+.5f-100)/300,a=1-b-c;
   current[y*640+x]=std::clamp(projection(1/(a/10+b/20+c/30)),0.f,1.f);
   const float pb=(x+.5f+(mode==3?6.f:.75f)-100)/400,pa=1-pb-c;
   previous[y*640+x]=std::clamp(projection(1/(pa/10+pb/20+c/30)),0.f,1.f);
  }
  if(mode==1)previous[160*640+201]=1;
  ComPtr<ID3D11ShaderResourceView> view;if(!idView(1,view,mode==2))return false;
  RemakeRasterOutput output;
  if(!raster.Render(surface.context.Get(),stream,current,previous,view.Get(),1,100,.0001f,0,output,error))return false;
  std::uint32_t motion=0,bias=0,reason=0;
  if(!read(output.textures[0].Get(),4,motion)||!read(output.textures[3].Get(),1,bias)
   ||!read(output.textures[4].Get(),2,reason))return false;
  const unsigned expected[]={0,6,5,6};
  if(motion!=0||bias!=(mode?255u:0u)||reason!=expected[mode]) {
   error="remake previous-footprint control failed mode="+std::to_string(mode)
    +" reason="+std::to_string(reason)+" motion="+std::to_string(motion)+" bias="+std::to_string(bias);return false;
  }
 }
 // Returned shading changes must reject history without rejecting stable RGB
 // merely because alpha differs. Exercise the production shader on both APIs.
 for(unsigned mode=0;mode<3;++mode) {
  RemakeMotionStream stream;stream.indices={0,1,2};
  stream.requiresColorValidation=true;
  stream.vertices={{{100,100,10},{100,100,10},1,1,1},{{500,100,10},{500,100,10},1,1,1},{{100,400,10},{100,400,10},1,1,1}};
  std::fill(current.begin(),current.end(),projection(10));previous=current;
  std::vector<unsigned char> now(640*480*4,100),before=now;
  if(mode==1)for(auto& vertex:stream.vertices)vertex.previousScreen.x-=4;
  if(mode==1)for(unsigned i=0;i<before.size();i+=4)before[i]=180;
  if(mode==2)for(unsigned i=3;i<before.size();i+=4)before[i]=0;
  ComPtr<ID3D11ShaderResourceView> view;if(!idView(1,view))return false;
  RemakeRasterOutput output;
  if(!raster.Render(surface.context.Get(),stream,current,previous,view.Get(),1,100,.1f,0,output,error,&now,&before))return false;
  RemakeRasterOutput missing;
  if(raster.Render(surface.context.Get(),stream,current,previous,view.Get(),1,100,.1f,0,missing,error)
   ||error!="remake-raster-color-required"||missing.textures[0])return false;
  std::uint32_t motion=0,bias=0,reason=0;
  if(!read(output.textures[0].Get(),4,motion)||!read(output.textures[3].Get(),1,bias)
   ||!read(output.textures[4].Get(),2,reason))return false;
  const bool motionCorrect=mode==1?(std::abs(int(motion&65535)-int(FloatToHalf(-4)))<=1&&(motion>>16)==0):motion==0;
  if(!motionCorrect||bias!=(mode==1?255u:0u)||reason!=(mode==1?8u:0u)) {
   error="remake shading consistency control mode="+std::to_string(mode);return false;
  }
  before.pop_back();RemakeRasterOutput rejected;
  if(raster.Render(surface.context.Get(),stream,current,previous,view.Get(),1,100,.1f,0,rejected,error,&now,&before)
   ||error!="remake-raster-color-bound"||rejected.textures[0])return false;
 }
 // Retained resource-owner validation must still reject a genuinely different
 // device after accommodating a host's wrapped creation interface.
 Surface foreign;if(!CreateSurface(false,foreign,error))return false;
 D3D11_TEXTURE2D_DESC d{};d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
 d.Format=DXGI_FORMAT_R16_UINT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> foreignTexture;ComPtr<ID3D11ShaderResourceView> foreignView;
 if(FAILED(foreign.device->CreateTexture2D(&d,nullptr,foreignTexture.GetAddressOf()))
  ||FAILED(foreign.device->CreateShaderResourceView(foreignTexture.Get(),nullptr,foreignView.GetAddressOf())))return false;
 RemakeMotionStream stream;stream.indices={0,1,2};
 stream.vertices={{{100,100,10},{100,100,10},1,1,1},{{500,100,10},{500,100,10},1,1,1},{{100,400,10},{100,400,10},1,1,1}};
 RemakeRasterOutput rejected;std::string reason;
 if(raster.Render(surface.context.Get(),stream,current,previous,foreignView.Get(),1,100,.1f,0,rejected,reason)
  ||reason!="remake-raster-previous-wrong-device"||rejected.textures[0]) {
  error="remake raster foreign previous owner was not atomically rejected";return false;
 }
 return true;
}
} // namespace neuraltest
