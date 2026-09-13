// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_native_draw.h"
#include "remake_native_readback.h"
#include "remake_native_provenance.h"
#include <set>
#include <string>
namespace flycast::rend::neural {
struct NativeIdentitySink {
 std::vector<std::uint32_t> words;bool ok=true;
 std::size_t remainingBytes()const{return words.size()<=NativeEvidenceByteLimit/4?NativeEvidenceByteLimit-words.size()*4:0;}
 bool Add(std::uint32_t v){if(!ok||remainingBytes()<4){ok=false;return false;}words.push_back(v);return true;}
 bool Float(float v){std::uint32_t word;std::memcpy(&word,&v,4);return Add(word);}
 bool Bytes(const std::vector<std::uint8_t>& bytes){
  if(!ok||bytes.size()>NativeEvidenceByteLimit||remainingBytes()<4+(bytes.size()+3)/4*4){ok=false;return false;}
  Add(static_cast<std::uint32_t>(bytes.size()));for(std::size_t i=0;i<bytes.size();i+=4){std::uint32_t v=0;for(unsigned j=0;j<4&&i+j<bytes.size();++j)v|=std::uint32_t(bytes[i+j])<<(8*j);Add(v);}return ok;
 }
};
inline bool EncodeNativeIdentityTexture(NativeIdentitySink& s,const NativeEvidenceTexture& texture) {
 const auto& d=texture.desc;
 for(auto v:{d.Width,d.Height,d.MipLevels,d.ArraySize,static_cast<UINT>(d.Format),d.SampleDesc.Count,d.SampleDesc.Quality,
  static_cast<UINT>(d.Usage),d.BindFlags,d.CPUAccessFlags,d.MiscFlags})s.Add(v);
 s.Add(static_cast<UINT>(texture.subresources.size()));
 for(const auto& sub:texture.subresources){for(auto v:{sub.mip,sub.slice,sub.width,sub.height})s.Add(v);s.Add(static_cast<UINT>(sub.rowBytes));s.Add(static_cast<UINT>(sub.rows));if(!s.Bytes(sub.bytes))return false;}
 return s.ok;
}
struct NativeIdentityElement {UINT slot,offset,size;};
inline bool NativeIdentityLayoutElements(const std::vector<std::uint8_t>& proof,std::vector<NativeIdentityElement>& elements) {
 elements.clear();if(!ValidateNativeProvenance(proof,NativeProvenanceKind::InputLayout))return false;
 std::size_t at=12;const auto word=[&](){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(proof[at++])<<(i*8);return v;};
 const auto codeSize=word();at+=codeSize;const auto count=word();std::array<UINT,32> next{};
 for(UINT i=0;i<count;++i){const auto name=word();at+=name;word();const auto format=static_cast<DXGI_FORMAT>(word());const auto slot=word();auto offset=word();const auto classification=word();const auto step=word();
  if(slot>=32||classification!=D3D11_INPUT_PER_VERTEX_DATA||step)return false;
  NativeEvidenceLayout layout;if(!NativeEvidenceFormatLayout(format,1,1,layout)||layout.rowBytes>16)return false;
  // BC, depth, typeless and packed video are not vertex element formats.
  switch(format){
  case DXGI_FORMAT_R32G32B32A32_FLOAT:case DXGI_FORMAT_R32G32B32A32_UINT:case DXGI_FORMAT_R32G32B32A32_SINT:
  case DXGI_FORMAT_R32G32B32_FLOAT:case DXGI_FORMAT_R32G32B32_UINT:case DXGI_FORMAT_R32G32B32_SINT:
  case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_UNORM:case DXGI_FORMAT_R16G16B16A16_UINT:case DXGI_FORMAT_R16G16B16A16_SNORM:case DXGI_FORMAT_R16G16B16A16_SINT:
  case DXGI_FORMAT_R32G32_FLOAT:case DXGI_FORMAT_R32G32_UINT:case DXGI_FORMAT_R32G32_SINT:
  case DXGI_FORMAT_R10G10B10A2_UNORM:case DXGI_FORMAT_R10G10B10A2_UINT:
  case DXGI_FORMAT_B8G8R8A8_UNORM:
  case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UINT:case DXGI_FORMAT_R8G8B8A8_SNORM:case DXGI_FORMAT_R8G8B8A8_SINT:
  case DXGI_FORMAT_R16G16_FLOAT:case DXGI_FORMAT_R16G16_UNORM:case DXGI_FORMAT_R16G16_UINT:case DXGI_FORMAT_R16G16_SNORM:case DXGI_FORMAT_R16G16_SINT:
  case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R32_UINT:case DXGI_FORMAT_R32_SINT:
  case DXGI_FORMAT_R8G8_UNORM:case DXGI_FORMAT_R8G8_UINT:case DXGI_FORMAT_R8G8_SNORM:case DXGI_FORMAT_R8G8_SINT:
  case DXGI_FORMAT_R16_FLOAT:case DXGI_FORMAT_R16_UNORM:case DXGI_FORMAT_R16_UINT:case DXGI_FORMAT_R16_SNORM:case DXGI_FORMAT_R16_SINT:
  case DXGI_FORMAT_R8_UNORM:case DXGI_FORMAT_R8_UINT:case DXGI_FORMAT_R8_SNORM:case DXGI_FORMAT_R8_SINT:break;
  default:return false;
  }
  // Current renderer layouts use explicit offsetof values. Do not guess
  // implicit format alignment for a different layout.
  if(offset==D3D11_APPEND_ALIGNED_ELEMENT)return false;
  if(offset>2048||layout.rowBytes>2048-offset)return false;next[slot]=offset+static_cast<UINT>(layout.rowBytes);
  elements.push_back({slot,offset,static_cast<UINT>(layout.rowBytes)});
 }
 return true;
}
inline bool EncodeNativeDrawIdentity(const NativeEffectDraw& d,ID3D11DeviceContext* context,
 std::vector<std::uint32_t>& output,std::string& error) {
 output.clear();const auto fail=[&](const char* why){error=why;return false;};
 try {
 if(!NativeCaptureContextMatches(d.owner,context,d.indices)||!d.count||d.count>262144||d.viewportCount>16||d.scissorCount>16)return fail("normal-identity-contract");
 if(d.topology!=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST&&d.topology!=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)return fail("normal-identity-topology");
 NativeIdentitySink s;s.Add(0x3144494e);s.Add(1);
 std::vector<std::uint8_t> proof;
 if(!ReadNativeProvenance(d.vs,NativeProvenanceKind::VertexShader,proof)||!s.Bytes(proof))return fail("normal-identity-vs-provenance");
 if(!ReadNativeProvenance(d.ps,NativeProvenanceKind::PixelShader,proof)||!s.Bytes(proof))return fail("normal-identity-ps-provenance");
 if(!ReadNativeProvenance(d.layout,NativeProvenanceKind::InputLayout,proof)||!s.Bytes(proof))return fail("normal-identity-layout-provenance");
 std::vector<NativeIdentityElement> elements;if(!NativeIdentityLayoutElements(proof,elements))return fail("normal-identity-layout-unsupported");
 for(auto v:{d.count,d.start,static_cast<UINT>(d.base),static_cast<UINT>(d.indexFormat),d.indexOffset,static_cast<UINT>(d.topology)})s.Add(v);
 const UINT indexSize=d.indexFormat==DXGI_FORMAT_R16_UINT?2:d.indexFormat==DXGI_FORMAT_R32_UINT?4:0;
 if(!indexSize||d.indexOffset%indexSize)return fail("normal-identity-index-format");
 const std::uint64_t indexBegin=std::uint64_t(d.indexOffset)+std::uint64_t(d.start)*indexSize;
 if(indexBegin>(std::numeric_limits<std::size_t>::max)())return fail("normal-identity-index-bound");
 std::vector<std::uint8_t> indices;if(!ReadNativeEvidenceBuffer(d.owner,context,d.indices,static_cast<std::size_t>(indexBegin),std::size_t(d.count)*indexSize,indices,s.remainingBytes())||!s.Bytes(indices))return fail("normal-identity-index-read");
 std::set<std::uint32_t> used;
 for(UINT i=0;i<d.count;++i){UINT index=0;for(UINT j=0;j<indexSize;++j)index|=UINT(indices[i*indexSize+j])<<(j*8);
  if(d.topology==D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP&&index==(indexSize==2?0xffffu:0xffffffffu))continue;
  const std::int64_t vertex=std::int64_t(index)+d.base;if(vertex<0||vertex>UINT32_MAX)return fail("normal-identity-vertex-index");used.insert(static_cast<UINT>(vertex));
 }
 // Fetch only actual input-layout bytes of addressed vertices, not unused
 // allocation tails or structure padding. Stable sorting is safe: raw index
 // order is already encoded above and controls primitive assembly.
 for(UINT slot=0;slot<32;++slot){s.Add(d.vertices[slot]?1:0);s.Add(d.strides[slot]);s.Add(d.offsets[slot]);}
 std::array<std::vector<std::uint8_t>,32> vertexData;std::array<std::uint64_t,32> vertexBegin{},vertexEnd{};
 vertexBegin.fill(UINT64_MAX);
 for(auto vertex:used)for(const auto& e:elements){
  if(!d.vertices[e.slot]||!d.strides[e.slot]||d.strides[e.slot]>2048)return fail("normal-identity-vertex-binding");
  const std::uint64_t offset=std::uint64_t(d.offsets[e.slot])+std::uint64_t(vertex)*d.strides[e.slot]+e.offset;
  if(offset>(std::numeric_limits<std::size_t>::max)())return fail("normal-identity-vertex-offset");
  vertexBegin[e.slot]=(std::min)(vertexBegin[e.slot],offset);vertexEnd[e.slot]=(std::max)(vertexEnd[e.slot],offset+e.size);
 }
 std::size_t stagedBytes=0;
 for(UINT slot=0;slot<32;++slot)if(vertexBegin[slot]!=UINT64_MAX){const auto bytes=vertexEnd[slot]-vertexBegin[slot];
  if(bytes>NativeEvidenceByteLimit-stagedBytes||!ReadNativeEvidenceBuffer(d.owner,context,d.vertices[slot],static_cast<std::size_t>(vertexBegin[slot]),static_cast<std::size_t>(bytes),vertexData[slot],NativeEvidenceByteLimit-stagedBytes))return fail("normal-identity-vertex-read");stagedBytes+=static_cast<std::size_t>(bytes);
 }
 s.Add(static_cast<UINT>(used.size()));
 for(auto vertex:used){s.Add(vertex);for(const auto& e:elements){const auto offset=std::uint64_t(d.offsets[e.slot])+std::uint64_t(vertex)*d.strides[e.slot]+e.offset-vertexBegin[e.slot];
  const auto first=vertexData[e.slot].begin()+static_cast<std::size_t>(offset);std::vector<std::uint8_t> bytes(first,first+e.size);if(!s.Bytes(bytes))return fail("normal-identity-vertex-bound");
 }}
 const auto constants=[&](const auto& buffers){for(const auto& buffer:buffers){s.Add(buffer?1:0);if(buffer){D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
   if(desc.ByteWidth!=144&&desc.ByteWidth!=96&&desc.ByteWidth!=48)return false;
   std::vector<std::uint8_t> bytes;if(!ReadNativeEvidenceBuffer(d.owner,context,buffer,0,desc.ByteWidth,bytes,s.remainingBytes())||!s.Bytes(bytes))return false;
  }}return s.ok;};
 if(!constants(d.vsConstants)||!constants(d.psConstants))return fail("normal-identity-constants");
 const auto views=[&](const auto& bindings){for(const auto& view:bindings){s.Add(view?1:0);if(!view)continue;
  D3D11_SHADER_RESOURCE_VIEW_DESC desc{};view->GetDesc(&desc);s.Add(static_cast<UINT>(desc.Format));s.Add(static_cast<UINT>(desc.ViewDimension));
  switch(desc.ViewDimension){
  case D3D11_SRV_DIMENSION_TEXTURE2D:s.Add(desc.Texture2D.MostDetailedMip);s.Add(desc.Texture2D.MipLevels);break;
  case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:s.Add(desc.Texture2DArray.MostDetailedMip);s.Add(desc.Texture2DArray.MipLevels);s.Add(desc.Texture2DArray.FirstArraySlice);s.Add(desc.Texture2DArray.ArraySize);break;
  case D3D11_SRV_DIMENSION_TEXTURECUBE:s.Add(desc.TextureCube.MostDetailedMip);s.Add(desc.TextureCube.MipLevels);break;
  case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:s.Add(desc.TextureCubeArray.MostDetailedMip);s.Add(desc.TextureCubeArray.MipLevels);s.Add(desc.TextureCubeArray.First2DArrayFace);s.Add(desc.TextureCubeArray.NumCubes);break;
  default:return false;
  }
  ComPtr<ID3D11Resource> resource;view->GetResource(&resource.get());ComPtr<ID3D11Texture2D> texture;
  if(!resource||FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&texture.get()))))return false;
  NativeEvidenceTexture data;if(!ReadNativeEvidenceTexture(d.owner,context,texture,data,s.remainingBytes())||!EncodeNativeIdentityTexture(s,data))return false;
 }return s.ok;};
 if(!views(d.vsViews)||!views(d.psViews))return fail("normal-identity-views");
 const auto samplers=[&](const auto& bindings){for(const auto& sampler:bindings){s.Add(sampler?1:0);if(!sampler)continue;D3D11_SAMPLER_DESC x{};sampler->GetDesc(&x);
  for(auto v:{static_cast<UINT>(x.Filter),static_cast<UINT>(x.AddressU),static_cast<UINT>(x.AddressV),static_cast<UINT>(x.AddressW)})s.Add(v);
  s.Float(x.MipLODBias);s.Add(x.MaxAnisotropy);s.Add(x.ComparisonFunc);for(float v:x.BorderColor)s.Float(v);s.Float(x.MinLOD);s.Float(x.MaxLOD);
 }return s.ok;};
 if(!samplers(d.vsSamplers)||!samplers(d.psSamplers))return fail("normal-identity-samplers");
 s.Add(d.blend?1:0);if(d.blend){D3D11_BLEND_DESC x{};d.blend->GetDesc(&x);s.Add(x.AlphaToCoverageEnable);s.Add(x.IndependentBlendEnable);
  for(const auto& t:x.RenderTarget)for(auto v:{static_cast<UINT>(t.BlendEnable),static_cast<UINT>(t.SrcBlend),static_cast<UINT>(t.DestBlend),static_cast<UINT>(t.BlendOp),static_cast<UINT>(t.SrcBlendAlpha),static_cast<UINT>(t.DestBlendAlpha),static_cast<UINT>(t.BlendOpAlpha),static_cast<UINT>(t.RenderTargetWriteMask)})s.Add(v);
 }
 for(float v:d.blendFactor)s.Float(v);s.Add(d.sampleMask);s.Add(d.stencilRef);
 s.Add(d.depth?1:0);if(d.depth){D3D11_DEPTH_STENCIL_DESC x{};d.depth->GetDesc(&x);s.Add(x.DepthEnable);s.Add(x.DepthWriteMask);s.Add(x.DepthFunc);s.Add(x.StencilEnable);s.Add(x.StencilReadMask);s.Add(x.StencilWriteMask);
  for(const auto& f:{x.FrontFace,x.BackFace}){s.Add(f.StencilFailOp);s.Add(f.StencilDepthFailOp);s.Add(f.StencilPassOp);s.Add(f.StencilFunc);}
 }
 s.Add(d.raster?1:0);if(d.raster){D3D11_RASTERIZER_DESC x{};d.raster->GetDesc(&x);s.Add(x.FillMode);s.Add(x.CullMode);s.Add(x.FrontCounterClockwise);s.Add(static_cast<UINT>(x.DepthBias));s.Float(x.DepthBiasClamp);s.Float(x.SlopeScaledDepthBias);s.Add(x.DepthClipEnable);s.Add(x.ScissorEnable);s.Add(x.MultisampleEnable);s.Add(x.AntialiasedLineEnable);}
 s.Add(d.viewportCount);for(UINT i=0;i<d.viewportCount;++i){const auto& v=d.viewports[i];for(float f:{v.TopLeftX,v.TopLeftY,v.Width,v.Height,v.MinDepth,v.MaxDepth})s.Float(f);}
 s.Add(d.scissorCount);for(UINT i=0;i<d.scissorCount;++i){const auto& r=d.scissors[i];for(auto v:{r.left,r.top,r.right,r.bottom})s.Add(static_cast<UINT>(v));}
 if(!s.ok)return fail("normal-identity-bound");output=std::move(s.words);error.clear();return true;
 }catch(...){return fail("normal-identity-allocation");}
}
}

