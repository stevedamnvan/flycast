// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/pvr_material_capture.h"
#include "rend/neural/remake_overlay_snapshot.h"
#include "windows/comptr.h"
#include "version.h"
#include <d3d11.h>
#include <array>
#include <fstream>
#include <locale>
#include <stdexcept>
#include <limits>
#include <chrono>
#include <thread>
namespace neuraltest {
bool RunMaterialContract(const std::filesystem::path& out,std::string& error) {
 using namespace flycast::rend::neural;
 try {
  auto check=[](bool b,const char* s){if(!b)throw std::runtime_error(s);};
  struct Grouping : std::numpunct<char> {char do_thousands_sep()const override{return ',';}std::string do_grouping()const override{return "\3";}};
  const auto oldLocale=std::locale();std::locale::global(std::locale(std::locale::classic(),new Grouping));
  const auto groupedHash=MaterialContentHash({});std::locale::global(oldLocale);
  check(groupedHash=="CBF29CE484222325","material-fixture-locale-independent-hash");
  check(!std::filesystem::exists(out),"material-fixture-output-exists");
  ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
  check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device.get(),&level,&context.get())),"material-fixture-device");
  const std::array<std::array<std::uint8_t,4>,4> rgba{{{{255,0,0,255}},{{0,255,0,255}},{{0,0,255,255}},{{255,255,255,0}}}};
  MaterialMip palette{32,32,std::vector<std::uint8_t>(4096)};
  for(unsigned i=0;i<4;++i)for(unsigned c=0;c<4;++c)palette.bytes[i*4+c]=rgba[i][c==0?2:c==2?0:c];
  const DXGI_FORMAT formats[]={DXGI_FORMAT_B5G5R5A1_UNORM,DXGI_FORMAT_B4G4R4A4_UNORM,DXGI_FORMAT_B5G6R5_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_A8_UNORM};
  unsigned comparisons=0,controls=0,asyncComparisons=0,asyncControls=0;
  unsigned overlayComparisons=0;
  {
   D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=4;desc.MipLevels=desc.ArraySize=1;
   desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   std::vector<std::uint8_t> original(64),maskBytes(16),changed(64,0);
   for(unsigned i=0;i<64;++i)original[i]=std::uint8_t(i*3);
   for(unsigned i=0;i<16;++i)maskBytes[i]=i%2?255:0;
   D3D11_SUBRESOURCE_DATA initial{original.data(),16,0},maskInitial{maskBytes.data(),4,0};
   auto maskDesc=desc;maskDesc.Format=DXGI_FORMAT_R8_UNORM;
   ComPtr<ID3D11Texture2D> color,mask;
   check(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&color.get()))
    &&SUCCEEDED(device->CreateTexture2D(&maskDesc,&maskInitial,&mask.get())),"overlay-owned-fixture-resources");
   ProducerIdentity producer{1,2,3};RemakeOverlaySnapshot owned;
   check(CaptureRemakeOverlay(device,context,color,mask,10,producer,owned),"overlay-original-frame-copy");
   context->UpdateSubresource(color,0,nullptr,changed.data(),16,0);
   context->UpdateSubresource(mask,0,nullptr,changed.data(),4,0);
   std::size_t budget=1024;MaterialPixels retainedColor,newColor;
   check(ReadMaterialPixels(device,context,owned.color,budget,retainedColor,error)
    &&ReadMaterialPixels(device,context,color,budget,newColor,error),"overlay-owned-readback");
   maskDesc.Usage=D3D11_USAGE_STAGING;maskDesc.BindFlags=0;maskDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   ComPtr<ID3D11Texture2D> staging;check(SUCCEEDED(device->CreateTexture2D(&maskDesc,nullptr,&staging.get())),"overlay-mask-staging");
   context->CopyResource(staging,owned.mask);D3D11_MAPPED_SUBRESOURCE mapped{};
   check(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped)),"overlay-mask-readback");
   std::vector<unsigned char> retainedMask(16);
   for(unsigned y=0;y<4;++y)std::memcpy(retainedMask.data()+y*4,static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch,4);
   context->Unmap(staging,0);
   check(retainedColor.mips[0].bytes==original&&retainedMask==maskBytes
    &&newColor.mips[0].bytes==changed&&original!=changed,"overlay-source-mutation-cannot-change-owned-frame");overlayComparisons+=3;
   const auto kept=owned.color.get();
   check(!CaptureRemakeOverlay(device,context,color,nullptr,11,producer,owned)&&owned.color.get()==kept
    &&owned.identity.frame==10,"overlay-copy-failure-preserves-owner");++controls;
  }
  {
   rend_context emptyContext;
   MaterialShaderGlobals g;
   auto rejects=[&](const char* expected) {
    std::string why;
    // Null devices and nonexistent scene ensure validation precedes IO/GPU work.
    check(!WritePvrMaterials(out/"absent-scene.json",nullptr,nullptr,emptyContext,
     nullptr,0,0,0,1,"fixture",why,g)&&why==expected,"material-globals-rejection");
    ++controls;
   };
   rejects("material-shader-globals-missing");
   g.valid=true;rejects("material-shader-source-modified");
   g.sourceBytesUnchanged=true;
   g.fogVertex[0]=std::numeric_limits<float>::quiet_NaN();rejects("material-fog-nonfinite");
   g.fogVertex[0]=0;g.clampMax[3]=std::numeric_limits<float>::infinity();rejects("material-clamp-nonfinite");
   g.clampMax[3]=1;g.fogDensity=std::numeric_limits<float>::infinity();rejects("material-scalar-nonfinite");
   check(!std::filesystem::exists(out),"material-globals-rejection-no-output");
  }
  for(auto format:formats) {
   unsigned bpp=format==DXGI_FORMAT_A8_UNORM?1:(format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_R8G8B8A8_UNORM?4:2);
   D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=4;desc.MipLevels=3;desc.ArraySize=1;desc.Format=format;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   std::array<std::vector<std::uint8_t>,3> bytes;std::array<D3D11_SUBRESOURCE_DATA,3> initial{};
   for(unsigned mip=0;mip<3;++mip) {
    unsigned w=4>>mip;bytes[mip].resize(w*w*bpp);
    for(unsigned pixel=0;pixel<w*w;++pixel) {
     unsigned c=pixel%4;
     if(bpp==1)bytes[mip][pixel]=std::uint8_t(c);
     else if(bpp==4)for(unsigned j=0;j<4;++j)bytes[mip][pixel*4+j]=rgba[c][format==DXGI_FORMAT_B8G8R8A8_UNORM?(j==0?2:j==2?0:j):j];
     else {
      const unsigned f5551[]={0xFC00,0x83E0,0x801F,0x7FFF},f4444[]={0xFF00,0xF0F0,0xF00F,0x0FFF},f565[]={0xF800,0x07E0,0x001F,0xFFFF};
      unsigned v=format==DXGI_FORMAT_B5G5R5A1_UNORM?f5551[c]:format==DXGI_FORMAT_B4G4R4A4_UNORM?f4444[c]:f565[c];
      bytes[mip][pixel*2]=std::uint8_t(v);bytes[mip][pixel*2+1]=std::uint8_t(v>>8);
     }
    }
    initial[mip].pSysMem=bytes[mip].data();initial[mip].SysMemPitch=w*bpp;
   }
   ComPtr<ID3D11Texture2D> texture;check(SUCCEEDED(device->CreateTexture2D(&desc,initial.data(),&texture.get())),"material-fixture-texture");
   size_t budget=1024;MaterialPixels decoded;
   if(!ReadMaterialPixels(device,context,texture,budget,decoded,error))throw std::runtime_error(error);
   check(budget==1024-21*bpp&&decoded.mips.size()==3,"material-fixture-budget-accounting");
   {
    MaterialReadback readback;PvrCapturedTexture generation;generation.upload=7;generation.rtt=2;generation.palette=9;
    MaterialPixels pixels=decoded;std::size_t remaining=1;
    check(!readback.Begin(device,context,texture,generation,remaining,error)&&remaining==1&&!readback.Pending(),"material-async-budget-atomic");++asyncControls;
    remaining=1024;
    check(readback.Begin(device,context,texture,generation,remaining,error)&&remaining==1024-21*bpp&&readback.Pending(),"material-async-begin-budget");++asyncControls;
    const auto keptBudget=remaining;
    check(!readback.Begin(device,context,texture,generation,remaining,error)&&error=="material-async-busy"&&remaining==keptBudget&&readback.Pending(),"material-async-busy-preserves-ticket");++asyncControls;
    // Only the fixture submits work explicitly and waits for its deadline.
    // Production Begin/Poll never flush, sleep or wait for GPU completion.
    context->Flush();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    auto state=MaterialReadbackResult::Pending;
    while(state==MaterialReadbackResult::Pending&&std::chrono::steady_clock::now()<deadline) {
     state=readback.Poll(context,texture,generation,pixels,error);
     if(state==MaterialReadbackResult::Pending)std::this_thread::yield();
    }
    check(state==MaterialReadbackResult::Ready&&!readback.Pending()&&pixels.format==decoded.format&&pixels.mips.size()==3,"material-async-completion");
    for(unsigned mip=0;mip<3;++mip){check(pixels.mips[mip].bytes==decoded.mips[mip].bytes,"material-async-exact-mips");++asyncComparisons;}
    check(readback.Poll(context,texture,generation,pixels,error)==MaterialReadbackResult::Invalid&&pixels.mips[0].bytes==decoded.mips[0].bytes,"material-async-double-retire-rejected");++asyncControls;
    for(unsigned mutation=0;mutation<5;++mutation) {
     remaining=1024;check(readback.Begin(device,context,texture,generation,remaining,error),"material-async-negative-begin");
     auto changed=generation;
     if(mutation==0)++changed.upload;
     if(mutation==1)++changed.rtt;
     if(mutation==2)++*changed.palette;
     if(mutation==3)changed.palette.reset();
     check(readback.Poll(context,mutation==4?nullptr:texture.get(),changed,pixels,error)==MaterialReadbackResult::Invalid
      &&!readback.Pending()&&pixels.mips[0].bytes==decoded.mips[0].bytes,"material-async-generation-resource-rejection");++asyncControls;
    }
    remaining=1024;check(readback.Begin(device,context,texture,generation,remaining,error),"material-async-reset-begin");
    readback.Reset();check(!readback.Pending()&&readback.Poll(context,texture,generation,pixels,error)==MaterialReadbackResult::Invalid,"material-async-reset-retires-ticket");++asyncControls;
    if(format!=DXGI_FORMAT_A8_UNORM) {
     RemakeTextureCache cache;cache.BeginFrame(1,1);std::vector<unsigned char> dds{42},expected;
     check(EncodeRemakeMaterialDds(decoded,expected,error),"material-cache-reference");
     check(!cache.Request(device,context,texture,generation,dds,error)&&error=="material-cache-pending"
      &&dds==std::vector<unsigned char>{42}&&cache.Entries()==1,"material-cache-first-request-queues-only");++asyncControls;
     context->Flush();const auto cacheDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);bool ready=false;
     do {ready=cache.Request(device,context,texture,generation,dds,error);if(!ready)std::this_thread::yield();}
     while(!ready&&error=="material-cache-pending"&&std::chrono::steady_clock::now()<cacheDeadline);
     check(ready&&dds==expected,"material-cache-exact-dds");++asyncComparisons;
     cache.BeginFrame(2,1);
     check(cache.Request(device,context,texture,generation,dds,error)&&dds==expected&&cache.Entries()==1,"material-cache-unchanged-generation-reuses");++asyncControls;
     auto changed=generation;++changed.upload;
     auto updated=decoded;updated.mips[0].bytes[0]^=0xff;
     context->UpdateSubresource(texture,0,nullptr,updated.mips[0].bytes.data(),4*bpp,0);
     check(!cache.Request(device,context,texture,changed,dds,error)&&error=="material-cache-pending"
      &&dds==expected&&cache.Entries()==1,"material-cache-generation-change-never-serves-old-bytes");++asyncControls;
     std::vector<unsigned char> updatedDds;check(EncodeRemakeMaterialDds(updated,updatedDds,error)&&updatedDds!=expected,"material-cache-mutation-not-inert");
     context->Flush();const auto updateDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);ready=false;
     do {ready=cache.Request(device,context,texture,changed,dds,error);if(!ready)std::this_thread::yield();}
     while(!ready&&error=="material-cache-pending"&&std::chrono::steady_clock::now()<updateDeadline);
     check(ready&&dds==updatedDds,"material-cache-updated-content-exact");++asyncComparisons;
     context->UpdateSubresource(texture,0,nullptr,bytes[0].data(),4*bpp,0);
     cache.BeginFrame(122,1);check(cache.Entries()==1,"material-cache-age-boundary");++asyncControls;
     cache.BeginFrame(123,1);check(cache.Entries()==0,"material-cache-expiry-releases-ticket");++asyncControls;
     check(!cache.Request(device,context,texture,changed,dds,error)&&cache.Entries()==1,"material-cache-requeue");
     cache.BeginFrame(1,2);check(cache.Entries()==0,"material-cache-epoch-clears");++asyncControls;
     if(format==DXGI_FORMAT_B5G5R5A1_UNORM) {
      for(unsigned i=0;i<129;++i) {
       ComPtr<ID3D11Texture2D> another;check(SUCCEEDED(device->CreateTexture2D(&desc,initial.data(),&another.get())),"material-cache-bound-texture");
       check(!cache.Request(device,context,another,generation,dds,error)
        &&error==(i<128?"material-cache-pending":"material-cache-budget"),"material-cache-entry-bound");
      }
      check(cache.Entries()==128,"material-cache-maximum-owned-entries");++asyncControls;
      cache.Reset();check(cache.Entries()==0,"material-cache-reset-releases-all");++asyncControls;
     }
    }
   }
   for(unsigned mip=0;mip<3;++mip) {
    const auto& actual=decoded.mips[mip];check(actual.bytes==bytes[mip],"material-fixture-raw-mip");++comparisons;
    Image image{actual.width,actual.height,std::vector<std::uint8_t>(actual.width*actual.height*4)};
    for(unsigned i=0;i<actual.width*actual.height;++i) {
     std::array<std::uint8_t,4> color{};
     auto expected=rgba[i%4];if(format==DXGI_FORMAT_B5G6R5_UNORM)expected[3]=255;
     check(DecodeMaterialTexel(format,actual.bytes.data()+i*bpp,bpp,color,&palette)&&color==expected,"material-fixture-channel-palette-golden");
     std::copy(color.begin(),color.end(),image.rgba.begin()+i*4);++comparisons;
    }
    if(!WritePng(out/("format-"+std::to_string(format)+"-mip-"+std::to_string(mip)+".png"),image,error))throw std::runtime_error(error);
   }
   size_t byteLimit=1;MaterialPixels unchanged=decoded;
   check(!ReadMaterialPixels(device,context,texture,byteLimit,unchanged,error)&&error=="material-byte-budget"&&byteLimit==1&&unchanged.mips[0].bytes==decoded.mips[0].bytes,"material-fixture-overflow-rejection");++controls;
  }
  std::array<std::uint8_t,4> color{};std::uint8_t index=0;
  const std::array<std::uint8_t,2> half4444{0x88,0x88};
  check(DecodeMaterialTexel(DXGI_FORMAT_B4G4R4A4_UNORM,half4444.data(),2,color)&&color==std::array<std::uint8_t,4>{136,136,136,136},"material-fixture-unorm-alpha-golden");++comparisons;
  check(DecodeMaterialTexel(DXGI_FORMAT_A8_UNORM,&index,1,color,&palette,1)&&color!=rgba[0],"material-fixture-wrong-palette-non-inert");++controls;
  check(!DecodeMaterialTexel(DXGI_FORMAT_A8_UNORM,&index,1,color,nullptr),"material-fixture-missing-palette");++controls;
  std::array<std::uint8_t,4> redBGRA{0,0,255,255};
  check(DecodeMaterialTexel(DXGI_FORMAT_R8G8B8A8_UNORM,redBGRA.data(),4,color)&&color!=rgba[0],"material-fixture-wrong-channel-non-inert");++controls;
  PvrCapturedTexture g;g.upload=3;g.rtt=2;g.palette=8;
  check(MaterialGenerationMatches(g,3,2,8)&&!MaterialGenerationMatches(g,4,2,8)&&!MaterialGenerationMatches(g,3,3,8)&&!MaterialGenerationMatches(g,3,2,9),"material-fixture-generation-negatives");controls+=3;
  for(unsigned test=0;test<2;++test) {
   D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=4;desc.MipLevels=1;desc.ArraySize=test?1:2;
   desc.Format=test?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   ComPtr<ID3D11Texture2D> texture;check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture.get())),"material-fixture-unsupported-create");
   size_t budget=1024;MaterialPixels pixels;
   check(!ReadMaterialPixels(device,context,texture,budget,pixels,error)&&error=="material-resource-format-or-bounds"&&budget==1024&&pixels.mips.empty(),"material-fixture-array-format-rejection");++controls;
  }
  std::ofstream report(out/"contract.txt");report<<"git_sha="<<GIT_HASH<<"\napi=native-D3D11-WARP\nraw_and_rgba_comparisons="<<comparisons<<"\nnegative_controls="<<controls<<"\nasync_exact_mips="<<asyncComparisons<<"\nasync_controls="<<asyncControls<<"\noverlay_owned_comparisons="<<overlayComparisons<<"\nfixture_waits_excluded_from_performance=true\nsource_material_only=true\n";check(bool(report),"material-fixture-report");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
}
