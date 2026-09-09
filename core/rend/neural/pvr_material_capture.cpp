// SPDX-License-Identifier: GPL-2.0-or-later
#include "pvr_material_capture.h"
#include "pvr_palette_binding.h"
#include "rend/dx11/dx11_texture.h"
#include "json/json.hpp"
#include "version.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <atomic>

namespace flycast::rend::neural {
namespace { std::atomic<std::size_t> uploadSnapshotBytes{0}; }
MaterialUploadSnapshot::~MaterialUploadSnapshot(){uploadSnapshotBytes.fetch_sub(chargedBytes);}
std::shared_ptr<const MaterialUploadSnapshot> CaptureMaterialUpload(ID3D11Texture2D* resource,
 DXGI_FORMAT format,unsigned width,unsigned height,unsigned levels,const std::uint8_t* source,
 std::size_t bytes,unsigned upload,unsigned rtt) noexcept {
 try {
  const unsigned bpp=format==DXGI_FORMAT_A8_UNORM?1:format==DXGI_FORMAT_B8G8R8A8_UNORM?4:
   (format==DXGI_FORMAT_B5G5R5A1_UNORM||format==DXGI_FORMAT_B4G4R4A4_UNORM||format==DXGI_FORMAT_B5G6R5_UNORM)?2:0;
  if(!resource||!source||!bpp||!width||!height||width>4096||height>4096||!levels||levels>13)return {};
  if(levels>1&&(width!=height||width!=(1u<<(levels-1))))return {};
  std::size_t required=0;
  for(unsigned i=0;i<levels;++i)required+=std::size_t(std::max(1u,width>>i))*std::max(1u,height>>i)*bpp;
  constexpr std::size_t limit=64*1024*1024;
  if(bytes!=required||bytes>limit)return {};
  const auto charge=bytes+(bpp==1?0:(bytes/bpp)*4+148); // Indices need a draw-qualified palette later.
  if(charge>limit)return {};
  auto snapshot=std::make_shared<MaterialUploadSnapshot>();
  auto used=uploadSnapshotBytes.load();
  do {if(used>limit-charge)return {};} while(!uploadSnapshotBytes.compare_exchange_weak(used,used+charge));
  snapshot->chargedBytes=charge;snapshot->resource.get()=resource;resource->AddRef();snapshot->upload=upload;snapshot->rtt=rtt;
  snapshot->pixels.format=format;snapshot->pixels.mips.resize(levels);
  std::size_t offset=0;
  for(unsigned i=0;i<levels;++i) {
   auto& mip=snapshot->pixels.mips[levels-i-1];
   mip.width=levels==1?width:1u<<i;mip.height=levels==1?height:1u<<i;
   const auto size=std::size_t(mip.width)*mip.height*bpp;
   mip.bytes.assign(source+offset,source+offset+size);offset+=size;
  }
  std::string error;
  if(bpp!=1&&!EncodeRemakeMaterialDds(snapshot->pixels,snapshot->dds,error))return {};
  return snapshot;
 }catch(...){return {};}
}
bool MaterialUploadMatches(const MaterialUploadSnapshot& snapshot,ID3D11Texture2D* resource,
 const PvrCapturedTexture& generation) noexcept {
 if(!resource||static_cast<ID3D11Texture2D*>(snapshot.resource)!=resource||snapshot.upload!=generation.upload||snapshot.rtt!=generation.rtt
  ||snapshot.pixels.mips.empty())return false;
 D3D11_TEXTURE2D_DESC desc{};resource->GetDesc(&desc);
 return desc.ArraySize==1&&desc.SampleDesc.Count==1&&desc.Format==snapshot.pixels.format&&desc.Width==snapshot.pixels.mips[0].width
  &&desc.Height==snapshot.pixels.mips[0].height&&desc.MipLevels==snapshot.pixels.mips.size();
}
std::shared_ptr<const MaterialPaletteSnapshot> CaptureMaterialPalette(ID3D11Texture2D* resource,
 const std::uint32_t* rgba,const std::uint32_t* hash16,const std::uint32_t* hash256) noexcept {
 try {
  if(!rgba||!hash16||!hash256)return {};
  auto snapshot=std::make_shared<MaterialPaletteSnapshot>();
  snapshot->upload=CaptureMaterialUpload(resource,DXGI_FORMAT_B8G8R8A8_UNORM,32,32,1,
   reinterpret_cast<const std::uint8_t*>(rgba),4096,0,0);
  if(!snapshot->upload)return {};
  std::copy_n(hash16,64,snapshot->hash16.begin());std::copy_n(hash256,4,snapshot->hash256.begin());
  return snapshot;
 }catch(...){return {};}
}
bool MaterialPaletteMatches(const MaterialPaletteSnapshot& snapshot,ID3D11Texture2D* resource,
 unsigned bank,bool smallBank,unsigned generation) noexcept {
 PvrCapturedTexture identity;identity.upload=0;identity.rtt=0;
 return snapshot.upload&&MaterialUploadMatches(*snapshot.upload,resource,identity)
  &&bank<(smallBank?64u:4u)&&(smallBank?snapshot.hash16[bank]:snapshot.hash256[bank])==generation;
}
namespace {
using Json=nlohmann::json;
void Require(bool b,const char* e){if(!b)throw std::runtime_error(e);}
unsigned Bpp(DXGI_FORMAT f) {
 switch(f) {
 case DXGI_FORMAT_B5G5R5A1_UNORM:case DXGI_FORMAT_B4G4R4A4_UNORM:case DXGI_FORMAT_B5G6R5_UNORM:return 2;
 case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM:return 4;
 case DXGI_FORMAT_A8_UNORM:return 1;
 default:return 0;
 }
}
std::string Hash(const std::vector<std::uint8_t>& bytes) {
 std::uint64_t h=14695981039346656037ull;for(auto b:bytes){h^=b;h*=1099511628211ull;}
 std::ostringstream s;s.imbue(std::locale::classic());s<<std::hex<<std::uppercase<<std::setw(16)<<std::setfill('0')<<h;return s.str();
}
bool Same(const MaterialPixels& a,const MaterialPixels& b) {
 if(a.format!=b.format||a.mips.size()!=b.mips.size())return false;
 for(size_t i=0;i<a.mips.size();++i)if(a.mips[i].width!=b.mips[i].width||a.mips[i].height!=b.mips[i].height||a.mips[i].bytes!=b.mips[i].bytes)return false;
 return true;
}
}
bool MaterialGenerationMatches(const PvrCapturedTexture& saved,unsigned upload,unsigned rtt,unsigned palette) {
 return saved.upload==upload&&saved.rtt==rtt&&(!saved.palette||*saved.palette==palette);
}
std::string MaterialContentHash(const std::vector<std::uint8_t>& bytes){return Hash(bytes);}
bool DecodeMaterialTexel(DXGI_FORMAT f,const std::uint8_t* p,size_t size,
 std::array<std::uint8_t,4>& out,const MaterialMip* palette,unsigned base) {
 const unsigned bpp=Bpp(f);if(!p||!bpp||size<bpp)return false;
 std::array<std::uint8_t,4> value{};
 if(f==DXGI_FORMAT_A8_UNORM) {
  if(!palette||palette->width!=32||palette->height!=32||palette->bytes.size()!=4096||base>1023||p[0]>1023-base)return false;
  const auto* q=palette->bytes.data()+4*(base+p[0]);value={q[2],q[1],q[0],q[3]};
 } else if(bpp==4) {
  if(f==DXGI_FORMAT_B8G8R8A8_UNORM)value={p[2],p[1],p[0],p[3]};else value={p[0],p[1],p[2],p[3]};
 } else {
  unsigned v=p[0]|(unsigned(p[1])<<8);
  auto scale=[](unsigned x,unsigned max){return std::uint8_t((x*255+max/2)/max);};
  if(f==DXGI_FORMAT_B5G5R5A1_UNORM)value={scale((v>>10)&31,31),scale((v>>5)&31,31),scale(v&31,31),std::uint8_t((v>>15)*255)};
  else if(f==DXGI_FORMAT_B4G4R4A4_UNORM)value={scale((v>>8)&15,15),scale((v>>4)&15,15),scale(v&15,15),scale(v>>12,15)};
  else value={scale(v>>11,31),scale((v>>5)&63,63),scale(v&31,31),255};
 }
 out=value;return true;
}
bool ReadMaterialPixels(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture,
 size_t& remaining,MaterialPixels& output,std::string& error) {
 try {
  Require(device&&context&&texture,"material-null-resource");
  D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);const unsigned bpp=Bpp(desc.Format);
  Require(bpp&&desc.Width>0&&desc.Width<=4096&&desc.Height>0&&desc.Height<=4096
   &&desc.MipLevels>0&&desc.MipLevels<=13&&desc.ArraySize==1&&desc.SampleDesc.Count==1,"material-resource-format-or-bounds");
  size_t bytes=0;for(unsigned i=0;i<desc.MipLevels;++i)bytes+=size_t(std::max(1u,desc.Width>>i))*std::max(1u,desc.Height>>i)*bpp;
  Require(bytes<=remaining,"material-byte-budget");
  MaterialPixels result;result.format=desc.Format;
  desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
  ComPtr<ID3D11Texture2D> staging;
  Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging.get())),"material-staging-create");
  context->CopyResource(staging,texture);
  for(unsigned i=0;i<desc.MipLevels;++i) {
   MaterialMip mip;mip.width=std::max(1u,desc.Width>>i);mip.height=std::max(1u,desc.Height>>i);
   size_t row=size_t(mip.width)*bpp;mip.bytes.resize(row*mip.height);
   D3D11_MAPPED_SUBRESOURCE map{};
   Require(SUCCEEDED(context->Map(staging,i,D3D11_MAP_READ,0,&map)),"material-staging-map");
   if(map.RowPitch<row){context->Unmap(staging,i);throw std::runtime_error("material-row-pitch");}
   for(unsigned y=0;y<mip.height;++y)std::memcpy(mip.bytes.data()+y*row,static_cast<const std::uint8_t*>(map.pData)+size_t(y)*map.RowPitch,row);
   context->Unmap(staging,i);result.mips.push_back(std::move(mip));
  }
  remaining-=bytes;output=std::move(result);error.clear();return true;
 } catch(const std::exception& e){error=e.what();return false;}
}
struct MaterialReadback::Impl {
 ComPtr<ID3D11DeviceContext> context;
 ComPtr<ID3D11Texture2D> source,staging;
 ComPtr<ID3D11Query> completion;
 PvrCapturedTexture generation;
 D3D11_TEXTURE2D_DESC desc{};
 unsigned bpp=0;
};
MaterialReadback::MaterialReadback()=default;
MaterialReadback::~MaterialReadback()=default;
void MaterialReadback::Reset(){impl_.reset();}
bool MaterialReadback::Pending()const noexcept{return bool(impl_);}
bool MaterialReadback::Begin(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture,
 const PvrCapturedTexture& generation,std::size_t& remaining,std::string& error) {
 try {
  Require(!impl_,"material-async-busy");
  Require(device&&context&&texture,"material-null-resource");
  Require(context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE,"material-async-context-type");
  ComPtr<ID3D11Device> contextDevice,textureDevice;context->GetDevice(&contextDevice.get());texture->GetDevice(&textureDevice.get());
  Require(contextDevice.get()==device&&textureDevice.get()==device,"material-async-device");
  auto pending=std::make_unique<Impl>();texture->GetDesc(&pending->desc);auto& desc=pending->desc;
  pending->bpp=Bpp(desc.Format);
  Require(pending->bpp&&desc.Width>0&&desc.Width<=4096&&desc.Height>0&&desc.Height<=4096
   &&desc.MipLevels>0&&desc.MipLevels<=13&&desc.ArraySize==1&&desc.SampleDesc.Count==1,"material-resource-format-or-bounds");
  std::size_t bytes=0;for(unsigned i=0;i<desc.MipLevels;++i)
   bytes+=std::size_t(std::max(1u,desc.Width>>i))*std::max(1u,desc.Height>>i)*pending->bpp;
  Require(bytes<=remaining&&bytes<=64*1024*1024,"material-byte-budget");
  auto stagingDesc=desc;stagingDesc.Usage=D3D11_USAGE_STAGING;stagingDesc.BindFlags=0;
  stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;stagingDesc.MiscFlags=0;
  Require(SUCCEEDED(device->CreateTexture2D(&stagingDesc,nullptr,&pending->staging.get())),"material-staging-create");
  D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
  Require(SUCCEEDED(device->CreateQuery(&query,&pending->completion.get())),"material-async-query-create");
  pending->context.get()=context;context->AddRef();pending->source.get()=texture;texture->AddRef();pending->generation=generation;
  context->CopyResource(pending->staging,texture);context->End(pending->completion);
  impl_=std::move(pending);remaining-=bytes;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
MaterialReadbackResult MaterialReadback::Poll(ID3D11DeviceContext* context,ID3D11Texture2D* texture,
 const PvrCapturedTexture& generation,MaterialPixels& output,std::string& error) {
 try {
  Require(impl_!=nullptr,"material-async-empty");auto& job=*impl_;
  Require(context==job.context.get()&&texture==job.source.get(),"material-async-resource-context-changed");
  Require(generation.upload==job.generation.upload&&generation.rtt==job.generation.rtt
   &&generation.palette==job.generation.palette,"material-async-generation-changed");
  BOOL done=FALSE;
  const auto hr=context->GetData(job.completion,&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
  Require(SUCCEEDED(hr),"material-async-query-failed");
  if(hr==S_FALSE||!done){error.clear();return MaterialReadbackResult::Pending;}
  MaterialPixels pixels;pixels.format=job.desc.Format;
  for(unsigned i=0;i<job.desc.MipLevels;++i) {
   MaterialMip mip;mip.width=std::max(1u,job.desc.Width>>i);mip.height=std::max(1u,job.desc.Height>>i);
   const std::size_t row=std::size_t(mip.width)*job.bpp;mip.bytes.resize(row*mip.height);
   D3D11_MAPPED_SUBRESOURCE map{};
   const auto mapped=context->Map(job.staging,i,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
   if(mapped==DXGI_ERROR_WAS_STILL_DRAWING){error.clear();return MaterialReadbackResult::Pending;}
   Require(SUCCEEDED(mapped),"material-async-map-failed");
   struct Unmap {ID3D11DeviceContext* context;ID3D11Texture2D* texture;unsigned mip;
    ~Unmap(){context->Unmap(texture,mip);}} unmap{context,job.staging,i};
   Require(map.pData&&map.RowPitch>=row,"material-row-pitch");
   for(unsigned y=0;y<mip.height;++y)
    std::memcpy(mip.bytes.data()+y*row,static_cast<const std::uint8_t*>(map.pData)+std::size_t(y)*map.RowPitch,row);
   pixels.mips.push_back(std::move(mip));
  }
  output=std::move(pixels);Reset();error.clear();return MaterialReadbackResult::Ready;
 }catch(const std::exception& e){error=e.what();Reset();return MaterialReadbackResult::Invalid;}
}
bool EncodeRemakeMaterialDds(const MaterialPixels& pixels,std::vector<unsigned char>& output,std::string& error,const MaterialMip* palette,unsigned paletteBase) {
 try {
  Require(!pixels.mips.empty()&&pixels.mips.size()<=13,"view-dds-mips");
  Require(pixels.format!=DXGI_FORMAT_A8_UNORM||(palette&&palette->width==32&&palette->height==32&&palette->bytes.size()==4096&&paletteBase<=1023),"view-dds-palette-required");
  const auto bpp=Bpp(pixels.format);Require(bpp!=0,"view-dds-format");
  const auto width=pixels.mips[0].width,height=pixels.mips[0].height;
  Require(width&&height&&width<=4096&&height<=4096,"view-dds-size");
  std::size_t bytes=148;
  for(unsigned level=0;level<pixels.mips.size();++level) {
   const auto& mip=pixels.mips[level];
   Require(mip.width==std::max(1u,width>>level)&&mip.height==std::max(1u,height>>level)
    &&mip.bytes.size()==std::size_t(mip.width)*mip.height*bpp,"view-dds-mip-layout");
   if(level>0)Require(pixels.mips[level-1].width>1||pixels.mips[level-1].height>1,"view-dds-excess-mips");
   const auto size=std::size_t(mip.width)*mip.height*4;Require(size<=64*1024*1024-bytes,"view-dds-byte-bound");bytes+=size;
  }
  std::vector<unsigned char> data(bytes,0);
  const auto word=[&](unsigned offset,unsigned value){for(unsigned i=0;i<4;++i)data[offset+i]=static_cast<unsigned char>(value>>(8*i));};
  word(0,0x20534444);word(4,124);word(8,0x100f|(pixels.mips.size()>1?0x20000:0));word(12,height);word(16,width);word(20,width*4);
  word(28,unsigned(pixels.mips.size()));word(76,32);word(80,4);word(84,0x30315844);
  word(108,0x1000|(pixels.mips.size()>1?0x400008:0));word(128,28);word(132,3);word(140,1);
  std::size_t cursor=148;
  for(const auto& mip:pixels.mips)for(std::size_t offset=0;offset<mip.bytes.size();offset+=bpp) {
   std::array<std::uint8_t,4> rgba{};Require(DecodeMaterialTexel(pixels.format,mip.bytes.data()+offset,bpp,rgba,palette,paletteBase),"view-dds-decode");
   for(auto channel:rgba)data[cursor++]=channel;
  }
  output=std::move(data);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
struct RemakeTextureCache::Impl {
 struct Entry {
  ComPtr<ID3D11Texture2D> source;PvrCapturedTexture generation;
  MaterialReadback readback,paletteReadback;std::vector<unsigned char> dds;
  ComPtr<ID3D11Texture2D> palette;unsigned paletteBase=0;
  MaterialPixels pixels,palettePixels;
  std::uint64_t lastFrame=0;std::size_t bytes=0;
 };
 std::vector<std::unique_ptr<Entry>> entries;
 ComPtr<ID3D11DeviceContext> context;
 std::uint64_t frame=0,epoch=0;
};
RemakeTextureCache::RemakeTextureCache()=default;
RemakeTextureCache::~RemakeTextureCache()=default;
void RemakeTextureCache::Reset(){impl_.reset();}
std::size_t RemakeTextureCache::Entries()const noexcept{return impl_?impl_->entries.size():0;}
void RemakeTextureCache::BeginFrame(std::uint64_t frame,std::uint64_t epoch) {
 if(!frame||!epoch){Reset();return;}
 if(impl_&&(impl_->epoch!=epoch||frame<impl_->frame))Reset();
 if(!impl_)impl_=std::make_unique<Impl>();
 impl_->frame=frame;impl_->epoch=epoch;
 auto& entries=impl_->entries;
 entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& e){return frame-e->lastFrame>120;}),entries.end());
}
bool RemakeTextureCache::Request(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture,
 const PvrCapturedTexture& generation,std::vector<unsigned char>& output,std::string& error,ID3D11Texture2D* palette,unsigned paletteBase,
 const MaterialPixels* cpuPixels,const MaterialMip* cpuPalette) {
 try {
  Require(impl_&&context&&texture,"material-cache-frame-or-resource");auto& cache=*impl_;
  D3D11_TEXTURE2D_DESC sourceDesc{};texture->GetDesc(&sourceDesc);
  if(sourceDesc.Format==DXGI_FORMAT_A8_UNORM) {
   Require(palette&&generation.palette&&paletteBase<=1023,"material-cache-palette-required");
   D3D11_TEXTURE2D_DESC desc{};palette->GetDesc(&desc);
   Require(desc.Width==32&&desc.Height==32&&desc.MipLevels==1&&desc.ArraySize==1&&desc.SampleDesc.Count==1&&desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM,"material-cache-palette-layout");
  } else {palette=nullptr;paletteBase=0;}
  if(cache.context&&cache.context.get()!=context){Reset();throw std::runtime_error("material-cache-context-changed");}
  if(!cache.context){cache.context.get()=context;context->AddRef();}
  auto& entries=cache.entries;
  auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e->source.get()==texture&&e->palette.get()==palette&&e->paletteBase==paletteBase;});
  if(it!=entries.end()&&((*it)->generation.upload!=generation.upload||(*it)->generation.rtt!=generation.rtt
   ||(*it)->generation.palette!=generation.palette)){entries.erase(it);it=entries.end();}
  std::size_t used=0;for(const auto& e:entries)used+=e->bytes;
  constexpr std::size_t limit=64*1024*1024;
  if(it==entries.end()) {
   Require(entries.size()<128&&used<limit,"material-cache-budget");
   auto entry=std::make_unique<Impl::Entry>();std::size_t remaining=limit-used;
   if(cpuPixels) {
    std::size_t size=0;for(const auto& mip:cpuPixels->mips)size+=mip.bytes.size();
    Require(size<=remaining,"material-cache-budget");remaining-=size;entry->pixels=*cpuPixels;
   }else if(!entry->readback.Begin(device,context,texture,generation,remaining,error))return false;
   if(palette) {
    if(cpuPalette) {
     Require(cpuPalette->bytes.size()<=remaining,"material-cache-budget");remaining-=cpuPalette->bytes.size();
     entry->palettePixels.format=DXGI_FORMAT_B8G8R8A8_UNORM;entry->palettePixels.mips.push_back(*cpuPalette);
    }else if(!entry->paletteReadback.Begin(device,context,palette,generation,remaining,error))return false;
    entry->palette.get()=palette;palette->AddRef();entry->paletteBase=paletteBase;
   }
   entry->source.get()=texture;texture->AddRef();entry->generation=generation;entry->lastFrame=cache.frame;
   entry->bytes=limit-used-remaining;used+=entry->bytes;entries.push_back(std::move(entry));it=entries.end()-1;
   if(!cpuPixels||(palette&&!cpuPalette)){error="material-cache-pending";return false;}
  }
  auto& entry=**it;entry.lastFrame=cache.frame;
  if(entry.dds.empty()) {
   if(entry.pixels.mips.empty()) {
    const auto result=entry.readback.Poll(context,texture,generation,entry.pixels,error);
    if(result==MaterialReadbackResult::Invalid){entries.erase(it);return false;}
   }
   if(palette&&entry.palettePixels.mips.empty()) {
    const auto result=entry.paletteReadback.Poll(context,palette,generation,entry.palettePixels,error);
    if(result==MaterialReadbackResult::Invalid){entries.erase(it);return false;}
   }
   if(entry.pixels.mips.empty()||(palette&&entry.palettePixels.mips.empty())){error="material-cache-pending";return false;}
   std::vector<unsigned char> dds;
   if(!EncodeRemakeMaterialDds(entry.pixels,dds,error,palette?&entry.palettePixels.mips[0]:nullptr,paletteBase)){entries.erase(it);return false;}
   if(dds.size()>limit-(used-entry.bytes)){entries.erase(it);error="material-cache-budget";return false;}
   entry.bytes=dds.size();entry.dds=std::move(dds);
   entry.pixels={};entry.palettePixels={};
  }
  output=entry.dds;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool ReadRemakeViewTexture(ID3D11Device* device,ID3D11DeviceContext* context,const rend_context& live,
 const PvrCapturedDraw& draw,std::size_t& remaining,std::vector<unsigned char>& output,std::string& error,RemakeTextureCache* asyncCache,ID3D11Texture2D* palette,
 const MaterialPaletteSnapshot* paletteSnapshot) {
 try {
  Require(!live.isRTT&&draw.list<=2,"view-texture-list");
  const auto& list=draw.list==0?live.global_param_op:draw.list==1?live.global_param_pt:live.global_param_tr;
  Require(draw.ordinal<list.size(),"view-texture-draw");
  const auto& source=list[draw.ordinal];
  Require(source.first==draw.state.first&&source.count==draw.state.count&&source.tcw.full==draw.state.tcw.full
   &&source.tsp.full==draw.state.tsp.full&&source.pcw.full==draw.state.pcw.full,"view-texture-state");
  if(!draw.state.pcw.Texture) {
   MaterialPixels white;white.format=DXGI_FORMAT_R8G8B8A8_UNORM;white.mips.push_back({1,1,{255,255,255,255}});
   return EncodeRemakeMaterialDds(white,output,error);
  }
  Require(source.texture&&draw.texture&&PvrDrawTextureWordMatches(source.texture->tcw,draw.state.tcw,source.texture->gpuPalette),"view-texture-binding");
  auto* texture=static_cast<DX11Texture*>(source.texture);const auto& generation=*draw.texture;
  const auto matches=[&]{return MaterialGenerationMatches(generation,texture->Updates,texture->rttGeneration,generation.palette?PvrDrawPaletteGeneration(*texture,draw.state.tcw):0);};
  Require(matches()&&texture->texture&&texture->textureView,"view-texture-generation");
  const unsigned base=draw.state.tcw.PixelFmt==PixelPal4?(draw.state.tcw.PalSelect<<4):((draw.state.tcw.PalSelect>>4)<<8);
  if(!texture->gpuPalette)palette=nullptr;
  Require(!texture->gpuPalette||palette,"view-texture-palette-missing");
  if(asyncCache) {
   // Exact owned CPU upload bytes avoid a GPU round trip. Generated mips,
   // GPU palettes, RTT/resource replacement and unavailable copies use staging.
   if(!texture->gpuPalette&&texture->remakeUpload
    &&MaterialUploadMatches(*texture->remakeUpload,texture->texture,generation)) {
	   Require(matches(),"view-texture-generation");
    const auto& bytes=texture->remakeUpload->dds;
    Require(!bytes.empty()&&bytes.size()<=remaining,"view-texture-budget");
    output=bytes;remaining-=bytes.size();error.clear();
    return true;
   }
   std::vector<unsigned char> bytes;
   const MaterialPixels* ownedIndices=nullptr;const MaterialMip* ownedPalette=nullptr;
   if(texture->gpuPalette&&texture->remakeUpload&&generation.palette&&paletteSnapshot&&paletteSnapshot->upload
    &&MaterialUploadMatches(*texture->remakeUpload,texture->texture,generation)
    &&MaterialPaletteMatches(*paletteSnapshot,palette,
     draw.state.tcw.PixelFmt==PixelPal4?draw.state.tcw.PalSelect:draw.state.tcw.PalSelect>>4,
     draw.state.tcw.PixelFmt==PixelPal4,*generation.palette)) {
    ownedIndices=&texture->remakeUpload->pixels;ownedPalette=&paletteSnapshot->upload->pixels.mips[0];
   }
   if(!asyncCache->Request(device,context,texture->texture,generation,bytes,error,palette,base,ownedIndices,ownedPalette)) {
    if(error=="material-cache-pending") {
     D3D11_TEXTURE2D_DESC desc{};texture->texture->GetDesc(&desc);
     error+=" format="+std::to_string(desc.Format)+" gpu_palette="+std::to_string(texture->gpuPalette)
      +" generated_mips="+std::to_string((desc.MiscFlags&D3D11_RESOURCE_MISC_GENERATE_MIPS)!=0)
      +" owned_upload="+std::to_string(bool(texture->remakeUpload));
    }
    return false;
   }
   Require(matches()&&bytes.size()<=remaining,"view-texture-generation-or-budget");
   remaining-=bytes.size();output=std::move(bytes);return true;
  }
  MaterialPixels pixels;if(!ReadMaterialPixels(device,context,texture->texture,remaining,pixels,error))return false;
  MaterialPixels palettePixels;if(palette&&!ReadMaterialPixels(device,context,palette,remaining,palettePixels,error))return false;
  Require(!palette||(palettePixels.format==DXGI_FORMAT_B8G8R8A8_UNORM&&palettePixels.mips.size()==1),"view-texture-palette-layout");
  Require(matches(),"view-texture-generation-changed");
  return EncodeRemakeMaterialDds(pixels,output,error,palette?&palettePixels.mips[0]:nullptr,base);
 }catch(const std::exception& e){error=e.what();return false;}
}
bool WritePvrMaterials(const std::filesystem::path& scene,ID3D11Device* device,ID3D11DeviceContext* context,
 const rend_context& live,ID3D11Texture2D* palette,unsigned paletteFormat,unsigned filtering,unsigned anisotropy,std::uint64_t frame,const std::string& game,std::string& error,const MaterialShaderGlobals& globals) {
 try {
  Require(globals.valid,"material-shader-globals-missing");
  Require(globals.sourceBytesUnchanged,"material-shader-source-modified");
  for(const auto& values:{globals.fogVertex,globals.fogRam})
   for(float v:values)Require(std::isfinite(v),"material-fog-nonfinite");
  for(const auto& values:{globals.clampMin,globals.clampMax})
   for(float v:values)Require(std::isfinite(v),"material-clamp-nonfinite");
  Require(std::isfinite(globals.fogDensity)&&std::isfinite(globals.alphaReference)
   &&std::isfinite(globals.shadowScale),"material-scalar-nonfinite");
  PvrDecodedPacket packet;if(!ReadPvrScenePacket(scene,frame,game,packet,error))throw std::runtime_error(error);
  Require(!live.isRTT,"material-rtt-unsupported");
  const auto output=scene.parent_path()/"materials";Require(!std::filesystem::exists(output),"material-output-exists");
  Require(packet.draws.size()==live.global_param_op.size()+live.global_param_pt.size()+live.global_param_tr.size(),"material-draw-count");
  struct Pending {const DX11Texture* source;PvrCapturedTexture generation;TCW draw;};
  std::vector<Pending> pending;
  std::map<const DX11Texture*,unsigned> logical;
  Json bindings=Json::array();bool paletteNeeded=false;
  for(const auto& draw:packet.draws) {
   const auto& list=draw.list==0?live.global_param_op:draw.list==1?live.global_param_pt:live.global_param_tr;
   Require(draw.ordinal<list.size(),"material-ordinal");const auto& p=list[draw.ordinal];const auto& saved=draw.state;
   Require(!p.isNaomi2()&&p.first==saved.first&&p.count==saved.count&&p.tcw.full==saved.tcw.full&&p.tsp.full==saved.tsp.full
    &&p.tcw1.full==saved.tcw1.full&&p.tsp1.full==saved.tsp1.full&&p.pcw.full==saved.pcw.full&&p.isp.full==saved.isp.full&&p.tileclip==saved.tileclip,"material-draw-state");
   for(unsigned slot=0;slot<2;++slot) {
    const auto& generation=slot?draw.texture1:draw.texture;
    const auto* texture=static_cast<const DX11Texture*>(slot?p.texture1:p.texture);
    Require(slot!=0||!p.pcw.Texture||texture,"material-textured-draw-missing-resource");
    Require(bool(generation)==bool(texture),"material-resource-presence");
    Json binding={{"list",draw.list},{"ordinal",draw.ordinal},{"slot",slot},{"asset",nullptr}};
    if(texture) {
     Require(MaterialGenerationMatches(*generation,texture->Updates,texture->rttGeneration,generation->palette?PvrDrawPaletteGeneration(*texture,slot?p.tcw1:p.tcw):0),"material-generation-mismatch");
     Require(texture->texture&&texture->textureView,"material-resource-unavailable");
     Require(texture->rttGeneration==0,"material-rtt-content-unsupported");
     ComPtr<ID3D11Resource> viewed;texture->textureView->GetResource(&viewed.get());
     Require(viewed.get()==static_cast<ID3D11Resource*>(static_cast<ID3D11Texture2D*>(texture->texture)),"material-srv-resource-mismatch");
     D3D11_SHADER_RESOURCE_VIEW_DESC srv{};texture->textureView->GetDesc(&srv);
     D3D11_TEXTURE2D_DESC desc{};texture->texture->GetDesc(&desc);
     Require(texture->gpuPalette==(desc.Format==DXGI_FORMAT_A8_UNORM),"material-index-format");
     Require(srv.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D&&srv.Format==desc.Format&&srv.Texture2D.MostDetailedMip==0
      &&(srv.Texture2D.MipLevels==desc.MipLevels||srv.Texture2D.MipLevels==UINT(-1)),"material-srv-layout-unsupported");
     auto it=logical.find(texture);
     if(it==logical.end()) {Require(pending.size()<255,"material-resource-budget");unsigned index=unsigned(pending.size());pending.push_back({texture,*generation,slot?p.tcw1:p.tcw});it=logical.emplace(texture,index).first;}
     const TCW tcw=slot?p.tcw1:p.tcw;const TSP tsp=slot?p.tsp1:p.tsp;
     const unsigned base=tcw.PixelFmt==PixelPal4?(tcw.PalSelect<<4):((tcw.PalSelect>>4)<<8);
     binding["logical_resource"]=it->second;binding["upload_generation"]=texture->Updates;
     binding["rtt_generation"]=texture->rttGeneration;binding["palette_hash"]=generation->palette?Json(*generation->palette):Json(nullptr);
     binding["tcw"]=tcw.full;binding["tsp"]=tsp.full;binding["area"]=texture->area;
     binding["gpu_palette"]=texture->gpuPalette;binding["palette_base"]=texture->gpuPalette?Json(base):Json(nullptr);
     binding["custom_replaced"]=texture->is_custom_replaced;binding["shading_instruction"]=tsp.ShadInstr;
     paletteNeeded|=texture->gpuPalette;
    }
    bindings.push_back(std::move(binding));
   }
  }
  size_t remaining=64u*1024*1024;std::vector<MaterialPixels> assets;std::vector<unsigned> logicalAssets;
  auto add=[&](ID3D11Texture2D* texture) {
   MaterialPixels pixels;if(!ReadMaterialPixels(device,context,texture,remaining,pixels,error))throw std::runtime_error(error);
   for(unsigned i=0;i<assets.size();++i)if(Same(assets[i],pixels))return i;
   Require(assets.size()<256,"material-resource-budget");assets.push_back(std::move(pixels));return unsigned(assets.size()-1);
  };
  for(const auto& p:pending)logicalAssets.push_back(add(p.source->texture));
  Json paletteAsset=nullptr;
  if(paletteNeeded) {
   Require(palette,"material-palette-unavailable");unsigned index=add(palette);
   Require(assets[index].format==DXGI_FORMAT_B8G8R8A8_UNORM&&assets[index].mips.size()==1&&assets[index].mips[0].width==32&&assets[index].mips[0].height==32,"material-palette-layout");paletteAsset=index;
  }
  for(const auto& p:pending)Require(MaterialGenerationMatches(p.generation,p.source->Updates,p.source->rttGeneration,p.generation.palette?PvrDrawPaletteGeneration(*p.source,p.draw):0),"material-generation-changed-during-capture");
  for(auto& b:bindings)if(b.contains("logical_resource"))b["asset"]=logicalAssets.at(b["logical_resource"].get<unsigned>());
  std::filesystem::create_directories(output);Json descriptions=Json::array();
  for(unsigned i=0;i<assets.size();++i) {
   Json mips=Json::array();const auto& asset=assets[i];
   for(unsigned level=0;level<asset.mips.size();++level) {
    const auto& mip=asset.mips[level];std::string name="asset-"+std::to_string(i)+"-mip-"+std::to_string(level)+".raw";
    std::ofstream file(output/name,std::ios::binary);file.write(reinterpret_cast<const char*>(mip.bytes.data()),std::streamsize(mip.bytes.size()));Require(bool(file),"material-raw-write");
    mips.push_back({{"level",level},{"width",mip.width},{"height",mip.height},{"row_bytes",mip.width*Bpp(asset.format)},{"bytes",mip.bytes.size()},{"fnv64",Hash(mip.bytes)},{"file",name}});
   }
   descriptions.push_back({{"id",i},{"dxgi_format",unsigned(asset.format)},{"mips",mips}});
  }
  Json result={{"schema","flycast-source-materials-v1"},{"frame_id",frame},{"game_id",game},{"git_sha",GIT_HASH},{"scene_sha",packet.gitSha},
   {"scene_file","../pvr-scene.json"},{"camera_provenance","unknown"},{"source_texture_not_physical_albedo",true},
   {"sampling_state","per-draw TSP plus recorded global overrides"},{"texture_filtering_override",filtering},
   {"vertex_color_storage","raw packet color byte fields bind as BGRA8 in native DX11"},
   {"anisotropic_filtering",anisotropy},{"mip_lod_bias",-1.5},{"palette_format",paletteFormat},{"palette_asset",paletteAsset},
   {"readback_bytes",64u*1024*1024-remaining},{"logical_resources",pending.size()},{"assets",descriptions},{"bindings",bindings},
   {"claims",{"capture-seam resources with generation checks; not pre-draw history","no normal/roughness/metalness inference","synchronous developer capture; not performance"}}};
  result["shader_globals"]={{"provenance","native-pixel-constant-upload"},{"fog_enabled",globals.fogEnabled},
   {"cpu_snapshot_source_bytes_unchanged",globals.sourceBytesUnchanged},
   {"fog_color_vertex",globals.fogVertex},{"fog_color_ram",globals.fogRam},
   {"clamp_min",globals.clampMin},{"clamp_max",globals.clampMax},{"fog_density",globals.fogDensity},
   {"alpha_reference",globals.alphaReference},{"shadow_scale",globals.shadowScale}};
  std::ofstream manifest(output/"manifest.json");manifest<<result.dump(2);Require(bool(manifest),"material-manifest-write");error.clear();return true;
 } catch(const std::exception& e){error=e.what();return false;}
}
}
