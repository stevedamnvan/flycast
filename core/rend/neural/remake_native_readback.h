// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_native_resource.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <limits>
#include <algorithm>
namespace flycast::rend::neural {
inline constexpr std::size_t NativeEvidenceByteLimit=64u*1024*1024;
struct NativeEvidenceLayout { std::size_t rowBytes=0,rows=0,bytes=0; };
inline bool NativeEvidenceFormatLayout(DXGI_FORMAT format,UINT width,UINT height,NativeEvidenceLayout& out) {
 out={};if(!width||!height||width>16384||height>16384)return false;
 unsigned pixel=0,block=0;
 switch(format) {
 case DXGI_FORMAT_BC1_TYPELESS:case DXGI_FORMAT_BC1_UNORM:case DXGI_FORMAT_BC1_UNORM_SRGB:
 case DXGI_FORMAT_BC4_TYPELESS:case DXGI_FORMAT_BC4_UNORM:case DXGI_FORMAT_BC4_SNORM:block=8;break;
 case DXGI_FORMAT_BC2_TYPELESS:case DXGI_FORMAT_BC2_UNORM:case DXGI_FORMAT_BC2_UNORM_SRGB:
 case DXGI_FORMAT_BC3_TYPELESS:case DXGI_FORMAT_BC3_UNORM:case DXGI_FORMAT_BC3_UNORM_SRGB:
 case DXGI_FORMAT_BC5_TYPELESS:case DXGI_FORMAT_BC5_UNORM:case DXGI_FORMAT_BC5_SNORM:
 case DXGI_FORMAT_BC6H_TYPELESS:case DXGI_FORMAT_BC6H_UF16:case DXGI_FORMAT_BC6H_SF16:
 case DXGI_FORMAT_BC7_TYPELESS:case DXGI_FORMAT_BC7_UNORM:case DXGI_FORMAT_BC7_UNORM_SRGB:block=16;break;
 case DXGI_FORMAT_R32G32B32A32_TYPELESS:case DXGI_FORMAT_R32G32B32A32_FLOAT:case DXGI_FORMAT_R32G32B32A32_UINT:case DXGI_FORMAT_R32G32B32A32_SINT:pixel=16;break;
 case DXGI_FORMAT_R32G32B32_TYPELESS:case DXGI_FORMAT_R32G32B32_FLOAT:case DXGI_FORMAT_R32G32B32_UINT:case DXGI_FORMAT_R32G32B32_SINT:pixel=12;break;
 case DXGI_FORMAT_R16G16B16A16_TYPELESS:case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_UNORM:case DXGI_FORMAT_R16G16B16A16_UINT:case DXGI_FORMAT_R16G16B16A16_SNORM:case DXGI_FORMAT_R16G16B16A16_SINT:
 case DXGI_FORMAT_R32G32_TYPELESS:case DXGI_FORMAT_R32G32_FLOAT:case DXGI_FORMAT_R32G32_UINT:case DXGI_FORMAT_R32G32_SINT:
 case DXGI_FORMAT_R32G8X24_TYPELESS:case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:pixel=8;break;
 case DXGI_FORMAT_R10G10B10A2_TYPELESS:case DXGI_FORMAT_R10G10B10A2_UNORM:case DXGI_FORMAT_R10G10B10A2_UINT:case DXGI_FORMAT_R11G11B10_FLOAT:
 case DXGI_FORMAT_R8G8B8A8_TYPELESS:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_R8G8B8A8_UINT:case DXGI_FORMAT_R8G8B8A8_SNORM:case DXGI_FORMAT_R8G8B8A8_SINT:
 case DXGI_FORMAT_R16G16_TYPELESS:case DXGI_FORMAT_R16G16_FLOAT:case DXGI_FORMAT_R16G16_UNORM:case DXGI_FORMAT_R16G16_UINT:case DXGI_FORMAT_R16G16_SNORM:case DXGI_FORMAT_R16G16_SINT:
 case DXGI_FORMAT_R32_TYPELESS:case DXGI_FORMAT_D32_FLOAT:case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R32_UINT:case DXGI_FORMAT_R32_SINT:
 case DXGI_FORMAT_R24G8_TYPELESS:case DXGI_FORMAT_D24_UNORM_S8_UINT:case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
 case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8X8_UNORM:case DXGI_FORMAT_B8G8R8A8_TYPELESS:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8X8_TYPELESS:case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
 case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:pixel=4;break;
 case DXGI_FORMAT_R8G8_TYPELESS:case DXGI_FORMAT_R8G8_UNORM:case DXGI_FORMAT_R8G8_UINT:case DXGI_FORMAT_R8G8_SNORM:case DXGI_FORMAT_R8G8_SINT:
 case DXGI_FORMAT_R16_TYPELESS:case DXGI_FORMAT_R16_FLOAT:case DXGI_FORMAT_D16_UNORM:case DXGI_FORMAT_R16_UNORM:case DXGI_FORMAT_R16_UINT:case DXGI_FORMAT_R16_SNORM:case DXGI_FORMAT_R16_SINT:
 case DXGI_FORMAT_B5G6R5_UNORM:case DXGI_FORMAT_B5G5R5A1_UNORM:case DXGI_FORMAT_B4G4R4A4_UNORM:pixel=2;break;
 case DXGI_FORMAT_R8_TYPELESS:case DXGI_FORMAT_R8_UNORM:case DXGI_FORMAT_R8_UINT:case DXGI_FORMAT_R8_SNORM:case DXGI_FORMAT_R8_SINT:case DXGI_FORMAT_A8_UNORM:pixel=1;break;
 default:return false; // Unknown, planar, bit-packed and video formats are unsupported.
 }
 NativeEvidenceLayout l;l.rowBytes=block?std::size_t((width+3)/4)*block:std::size_t(width)*pixel;l.rows=block?(height+3)/4:height;
 if(l.rowBytes>NativeEvidenceByteLimit/l.rows)return false;l.bytes=l.rowBytes*l.rows;out=l;return true;
}
inline bool PackNativeEvidenceRows(const void* data,std::size_t rowPitch,const NativeEvidenceLayout& l,
 std::vector<std::uint8_t>& out,std::size_t budget=NativeEvidenceByteLimit) noexcept {
 out.clear();if(!data||!l.rowBytes||!l.rows||l.rowBytes>rowPitch||l.rows>NativeEvidenceByteLimit/l.rowBytes
  ||l.bytes!=l.rowBytes*l.rows||l.bytes>(std::min)(budget,NativeEvidenceByteLimit)
  ||(l.rows>1&&rowPitch>((std::numeric_limits<std::size_t>::max)()-l.rowBytes)/(l.rows-1)))return false;
 try {std::vector<std::uint8_t> packed(l.bytes);for(std::size_t y=0;y<l.rows;++y)std::memcpy(packed.data()+y*l.rowBytes,static_cast<const std::uint8_t*>(data)+y*rowPitch,l.rowBytes);out=std::move(packed);return true;}catch(...){return false;}
}
struct NativeEvidenceMapped {
 ID3D11DeviceContext* context;ID3D11Resource* resource;UINT subresource;
 ~NativeEvidenceMapped(){context->Unmap(resource,subresource);}
};
inline bool ReadNativeEvidenceBuffer(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Buffer* source,
 std::size_t offset,std::size_t bytes,std::vector<std::uint8_t>& out,std::size_t budget=NativeEvidenceByteLimit) noexcept {
 out.clear();if(!NativeCaptureContextMatches(device,context,source)||!bytes||bytes>(std::min)(budget,NativeEvidenceByteLimit))return false;
 D3D11_BUFFER_DESC original{};source->GetDesc(&original);if(offset>original.ByteWidth||bytes>original.ByteWidth-offset)return false;
 // D3D11 constant buffers require a whole-buffer copy. Bound that staging
 // allocation too, but export only the explicitly requested logical span.
 const bool whole=(original.BindFlags&D3D11_BIND_CONSTANT_BUFFER)!=0;
 if(whole&&original.ByteWidth>(std::min)(budget,NativeEvidenceByteLimit))return false;
 try {D3D11_BUFFER_DESC desc{};desc.ByteWidth=whole?original.ByteWidth:static_cast<UINT>(bytes);desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Buffer> staging;if(FAILED(device->CreateBuffer(&desc,nullptr,&staging.get())))return false;
  D3D11_BOX box{static_cast<UINT>(offset),0,0,static_cast<UINT>(offset+bytes),1,1};
  if(whole)context->CopyResource(staging,source);else context->CopySubresourceRegion(staging,0,0,0,0,source,0,&box);
  D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context->Map(staging,0,D3D11_MAP_READ,0,&map)))return false;
  NativeEvidenceMapped unmap{context,staging,0};if(!map.pData)return false;std::vector<std::uint8_t> packed(bytes);std::memcpy(packed.data(),static_cast<const std::uint8_t*>(map.pData)+(whole?offset:0),bytes);out=std::move(packed);return true;
 }catch(...){return false;}
}
struct NativeEvidenceSubresource { UINT mip=0,slice=0,width=0,height=0;std::size_t rowBytes=0,rows=0;std::vector<std::uint8_t> bytes; };
struct NativeEvidenceTexture { D3D11_TEXTURE2D_DESC desc{};std::vector<NativeEvidenceSubresource> subresources; };
inline bool ReadNativeEvidenceTexture(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,
 NativeEvidenceTexture& out,std::size_t budget=NativeEvidenceByteLimit) noexcept {
 out={};if(!NativeCaptureContextMatches(device,context,source))return false;
 try {NativeEvidenceTexture result;source->GetDesc(&result.desc);const auto& d=result.desc;
  if(!d.Width||!d.Height||d.Width>16384||d.Height>16384||d.SampleDesc.Count!=1||d.SampleDesc.Quality||!d.MipLevels||d.MipLevels>15||!d.ArraySize||d.ArraySize>2048)return false;
  std::size_t total=0;budget=(std::min)(budget,NativeEvidenceByteLimit);
  for(UINT slice=0;slice<d.ArraySize;++slice)for(UINT mip=0;mip<d.MipLevels;++mip) {
   NativeEvidenceSubresource sub;sub.slice=slice;sub.mip=mip;sub.width=(std::max)(1u,d.Width>>mip);sub.height=(std::max)(1u,d.Height>>mip);
   NativeEvidenceLayout l;if(!NativeEvidenceFormatLayout(d.Format,sub.width,sub.height,l)||l.bytes>budget-total)return false;
   total+=l.bytes;sub.rowBytes=l.rowBytes;sub.rows=l.rows;result.subresources.push_back(std::move(sub));
  }
  auto desc=d;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
  ComPtr<ID3D11Texture2D> staging;if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging.get())))return false;context->CopyResource(staging,source);
  for(auto& sub:result.subresources) {const UINT index=D3D11CalcSubresource(sub.mip,sub.slice,d.MipLevels);D3D11_MAPPED_SUBRESOURCE map{};
   if(FAILED(context->Map(staging,index,D3D11_MAP_READ,0,&map)))return false;NativeEvidenceMapped unmap{context,staging,index};
   if(!PackNativeEvidenceRows(map.pData,map.RowPitch,{sub.rowBytes,sub.rows,sub.rowBytes*sub.rows},sub.bytes,budget))return false;
  }
  out=std::move(result);return true;
 }catch(...){return false;}
}
}

