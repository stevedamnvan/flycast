// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d11.h>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>
#include "windows/comptr.h"

namespace flycast::rend::neural {
// Copy on the source immediate context before subsequent native writes. No
// pointer-key cache here: constants may change between consecutive draws.
inline bool NativeCaptureContextMatches(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11Resource* source) noexcept {
 if(!device||!context||!source||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
 ComPtr<ID3D11Device> contextDevice,sourceDevice;
 context->GetDevice(&contextDevice.get());source->GetDevice(&sourceDevice.get());
 return contextDevice.get()==device&&sourceDevice.get()==device;
}
// LOG895: every owned copy used to be a fresh D3D11 resource per frame (about
// 126 objects per source frame). Retired copies of an identical shape are
// kept here and reused for the next capture on the same device, so a frame
// reuses allocations instead of creating them. Ownership is unchanged: a
// snapshot exclusively owns its copies while it lives and only its destructor
// retires them; the pool never hands out a resource that is still owned.
// D3D11 orders a CopyResource into a reused resource after every queued read
// of it on the immediate context. Bounded per shape; a device mismatch means
// no pooling, never a wrong-device resource.
class NativeResourcePool {
 struct TextureKey {
  UINT width,height,mips,array,bind,misc;DXGI_FORMAT format;UINT samples,quality;
  bool operator<(const TextureKey& o)const noexcept {
   return std::tie(width,height,mips,array,bind,misc,format,samples,quality)
    <std::tie(o.width,o.height,o.mips,o.array,o.bind,o.misc,o.format,o.samples,o.quality);
  }
 };
 ComPtr<ID3D11Device> device;ID3D11Device* address=nullptr; // Identity for const checks; the ComPtr keeps it alive.
 std::mutex mutex;
 std::map<std::pair<UINT,UINT>,std::vector<ComPtr<ID3D11Buffer>>> buffers;
 std::map<TextureKey,std::vector<ComPtr<ID3D11Texture2D>>> textures;
 std::size_t limitPerShape;
 std::uint64_t created=0,reused=0,retired=0,dropped=0;
 static TextureKey Key(const D3D11_TEXTURE2D_DESC& d)noexcept {
  return {d.Width,d.Height,d.MipLevels,d.ArraySize,d.BindFlags,d.MiscFlags,d.Format,d.SampleDesc.Count,d.SampleDesc.Quality};
 }
 bool Owns(ID3D11DeviceChild* resource)const noexcept {
  if(!resource)return false;
  ComPtr<ID3D11Device> owner;resource->GetDevice(&owner.get());return owner.get()==address;
 }
public:
 explicit NativeResourcePool(ID3D11Device* owner,std::size_t limit=256):address(owner),limitPerShape(limit){
  if(owner){owner->AddRef();device.reset(owner);}
 }
 NativeResourcePool(const NativeResourcePool&)=delete;
 NativeResourcePool& operator=(const NativeResourcePool&)=delete;
 bool Serves(ID3D11Device* candidate)const noexcept{return candidate&&candidate==address;}
 // Description already normalized by the caller (default usage, no CPU access).
 ComPtr<ID3D11Buffer> AcquireBuffer(ID3D11Device* owner,const D3D11_BUFFER_DESC& desc) {
  ComPtr<ID3D11Buffer> result;
  if(Serves(owner)) {
   std::lock_guard<std::mutex> lock(mutex);
   auto found=buffers.find({desc.ByteWidth,desc.BindFlags});
   if(found!=buffers.end()&&!found->second.empty()){result=std::move(found->second.back());found->second.pop_back();++reused;return result;}
  }
  if(!owner||FAILED(owner->CreateBuffer(&desc,nullptr,&result.get())))return {};
  if(Serves(owner)){std::lock_guard<std::mutex> lock(mutex);++created;}
  return result;
 }
 ComPtr<ID3D11Texture2D> AcquireTexture(ID3D11Device* owner,const D3D11_TEXTURE2D_DESC& desc) {
  ComPtr<ID3D11Texture2D> result;
  if(Serves(owner)) {
   std::lock_guard<std::mutex> lock(mutex);
   auto found=textures.find(Key(desc));
   if(found!=textures.end()&&!found->second.empty()){result=std::move(found->second.back());found->second.pop_back();++reused;return result;}
  }
  if(!owner||FAILED(owner->CreateTexture2D(&desc,nullptr,&result.get())))return {};
  if(Serves(owner)){std::lock_guard<std::mutex> lock(mutex);++created;}
  return result;
 }
 // Retire only what this pool's device created with the pool's own shapes.
 void Retire(ID3D11Buffer* buffer) {
  if(!Owns(buffer))return;
  D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
  if(desc.Usage!=D3D11_USAGE_DEFAULT||desc.CPUAccessFlags||desc.MiscFlags)return;
  std::lock_guard<std::mutex> lock(mutex);
  auto& shape=buffers[{desc.ByteWidth,desc.BindFlags}];
  if(shape.size()>=limitPerShape){++dropped;return;}
  buffer->AddRef();shape.emplace_back();shape.back().reset(buffer);++retired;
 }
 void Retire(ID3D11Texture2D* texture) {
  if(!Owns(texture))return;
  D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
  if(desc.Usage!=D3D11_USAGE_DEFAULT||desc.CPUAccessFlags)return;
  std::lock_guard<std::mutex> lock(mutex);
  auto& shape=textures[Key(desc)];
  if(shape.size()>=limitPerShape){++dropped;return;}
  texture->AddRef();shape.emplace_back();shape.back().reset(texture);++retired;
 }
 // The texture behind a view copy; the view itself is released by its owner.
 void Retire(ID3D11ShaderResourceView* view) {
  if(!view)return;
  ComPtr<ID3D11Resource> resource;view->GetResource(&resource.get());
  ComPtr<ID3D11Texture2D> texture;
  if(resource&&SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&texture.get()))))Retire(texture.get());
 }
 struct Counters { std::uint64_t created,reused,retired,dropped;std::size_t held; };
 Counters Statistics() {
  std::lock_guard<std::mutex> lock(mutex);
  std::size_t held=0;for(const auto& s:buffers)held+=s.second.size();for(const auto& s:textures)held+=s.second.size();
  return {created,reused,retired,dropped,held};
 }
 void Clear(){std::lock_guard<std::mutex> lock(mutex);buffers.clear();textures.clear();}
};
inline ComPtr<ID3D11Buffer> CopyNativeEffectBuffer(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11Buffer* source,NativeResourcePool* pool=nullptr) {
 ComPtr<ID3D11Buffer> copy;
 if(!NativeCaptureContextMatches(device,context,source))return copy;
 D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
 // Replay binds vertex/index/constants only. Reject special buffer semantics.
 const UINT allowed=D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER|D3D11_BIND_CONSTANT_BUFFER;
 if(!desc.ByteWidth||!desc.BindFlags||(desc.BindFlags&~allowed)||desc.MiscFlags)return copy;
 desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
 if(pool)copy=pool->AcquireBuffer(device,desc);
 else if(FAILED(device->CreateBuffer(&desc,nullptr,&copy.get())))return {};
 if(!copy)return {};
 context->CopyResource(copy,source);return copy;
}
inline ComPtr<ID3D11Texture2D> CopyNativeEffectTexture(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11Texture2D* source,NativeResourcePool* pool=nullptr) {
 ComPtr<ID3D11Texture2D> copy;
 if(!NativeCaptureContextMatches(device,context,source))return copy;
 D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
 // Preserve every mip, array slice, format and sample. Shared handles and
 // mip-generation ownership belong to the live source, not this snapshot.
 const UINT allowedMisc=D3D11_RESOURCE_MISC_GENERATE_MIPS|D3D11_RESOURCE_MISC_TEXTURECUBE;
 if(desc.MiscFlags&~allowedMisc)return copy;
 desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
 desc.MiscFlags&=D3D11_RESOURCE_MISC_TEXTURECUBE;
 if(pool)copy=pool->AcquireTexture(device,desc);
 else if(FAILED(device->CreateTexture2D(&desc,nullptr,&copy.get())))return {};
 if(!copy)return {};
 context->CopyResource(copy,source);return copy;
}
// Return an owning view with the exact original interpretation. Buffer/3D
// views are unsupported here and must not silently become empty bindings.
inline ComPtr<ID3D11ShaderResourceView> CopyNativeEffectView(ID3D11Device* device,
 ID3D11DeviceContext* context,ID3D11ShaderResourceView* source,NativeResourcePool* pool=nullptr) {
 if(!source)return {};
 ComPtr<ID3D11Resource> resource;source->GetResource(&resource.get());
 if(!NativeCaptureContextMatches(device,context,resource))return {};
 ComPtr<ID3D11Texture2D> texture;
 if(FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D),
  reinterpret_cast<void**>(&texture.get()))))return {};
 auto owned=CopyNativeEffectTexture(device,context,texture,pool);
 if(!owned)return {};
 D3D11_SHADER_RESOURCE_VIEW_DESC desc{};source->GetDesc(&desc);
 ComPtr<ID3D11ShaderResourceView> view;
 if(FAILED(device->CreateShaderResourceView(owned,&desc,&view.get()))){if(pool)pool->Retire(owned.get());return {};}
 return view;
}

// Caller guarantees sampled textures are immutable for this capture interval.
// Keep source views alive to prevent address reuse; never share across frames.
struct NativeViewCopies {
 struct Entry { ComPtr<ID3D11ShaderResourceView> source,copy; };
 std::map<ID3D11ShaderResourceView*,Entry> copies;
 std::shared_ptr<NativeResourcePool> pool; // Optional: retired copies return here at destruction.
 NativeViewCopies()=default;
 NativeViewCopies(const NativeViewCopies&)=delete;
 NativeViewCopies& operator=(const NativeViewCopies&)=delete;
 ~NativeViewCopies(){if(pool)for(auto& item:copies)pool->Retire(item.second.copy.get());}
 ComPtr<ID3D11ShaderResourceView> Get(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11ShaderResourceView* source){
  if(!source)return {};
  ComPtr<ID3D11Resource> resource;source->GetResource(&resource.get());
  if(!NativeCaptureContextMatches(device,context,resource))return {};
  auto found=copies.find(source);if(found!=copies.end())return found->second.copy;
  auto copy=CopyNativeEffectView(device,context,source,pool.get());
  if(copy){Entry entry;source->AddRef();entry.source.reset(source);entry.copy=copy;copies.emplace(source,std::move(entry));}
  return copy;
 }
};

// Caller must guarantee geometry is immutable for this capture interval.
// Never use for constants; each draw may update their contents.
struct NativeGeometryCopies {
 std::map<ID3D11Buffer*,ComPtr<ID3D11Buffer>> copies;
 std::shared_ptr<NativeResourcePool> pool; // Optional: retired copies return here at destruction.
 NativeGeometryCopies()=default;
 NativeGeometryCopies(const NativeGeometryCopies&)=delete;
 NativeGeometryCopies& operator=(const NativeGeometryCopies&)=delete;
 ~NativeGeometryCopies(){if(pool)for(auto& item:copies)pool->Retire(item.second.get());}
 ComPtr<ID3D11Buffer> Get(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Buffer* source){
  if(!NativeCaptureContextMatches(device,context,source))return {};
  D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
  if(desc.BindFlags&~(D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER))return {};
  auto found=copies.find(source);if(found!=copies.end())return found->second;
  auto copy=CopyNativeEffectBuffer(device,context,source,pool.get());
  if(copy)copies.emplace(source,copy);return copy;
 }
};

}
