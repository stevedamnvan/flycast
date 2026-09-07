// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/pvr_material_capture.h"
#include "windows/comptr.h"
#include "version.h"
#include <d3d11.h>
#include <array>
#include <fstream>
#include <locale>
#include <stdexcept>
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
  unsigned comparisons=0,controls=0;
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
  std::ofstream report(out/"contract.txt");report<<"git_sha="<<GIT_HASH<<"\napi=native-D3D11-WARP\nraw_and_rgba_comparisons="<<comparisons<<"\nnegative_controls="<<controls<<"\nsource_material_only=true\n";check(bool(report),"material-fixture-report");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
}
