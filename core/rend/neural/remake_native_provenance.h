// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d11.h>
#include <cstdint>
#include <vector>
#include <cstddef>
#include <utility>
namespace flycast::rend::neural {
enum class NativeProvenanceKind : std::uint32_t { VertexShader=1,PixelShader=2,InputLayout=3 };
inline constexpr std::size_t NativeProvenanceBytecodeLimit=1024*1024;
inline constexpr std::size_t NativeProvenanceLimit=NativeProvenanceBytecodeLimit+8192;
// Private content is copied and owned by the D3D object, never an address token.
inline const GUID NativeProvenanceGuid={0x9279da51,0x712c,0x4b4b,{0x91,0xb6,0x52,0x0d,0x3b,0x67,0xb8,0x31}};
inline void NativeProvenanceWord(std::vector<std::uint8_t>& out,std::uint32_t v) {
 for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(v>>(i*8)));
}
inline bool EncodeNativeShaderProvenance(NativeProvenanceKind kind,const void* code,std::size_t size,
 std::vector<std::uint8_t>& out) {
 out.clear();
 if((kind!=NativeProvenanceKind::VertexShader&&kind!=NativeProvenanceKind::PixelShader)
  ||!code||!size||size>NativeProvenanceBytecodeLimit)return false;
 for(auto word:{0x3152504eu,1u,static_cast<std::uint32_t>(kind),static_cast<std::uint32_t>(size)})NativeProvenanceWord(out,word);
 const auto* bytes=static_cast<const std::uint8_t*>(code);out.insert(out.end(),bytes,bytes+size);return true;
}
inline bool EncodeNativeLayoutProvenance(const D3D11_INPUT_ELEMENT_DESC* elements,UINT count,
 const void* code,std::size_t size,std::vector<std::uint8_t>& out) {
 out.clear();if(count>32||(count&&!elements)||!code||!size||size>NativeProvenanceBytecodeLimit)return false;
 std::vector<std::uint8_t> result;
 for(auto word:{0x3152504eu,1u,static_cast<std::uint32_t>(NativeProvenanceKind::InputLayout),static_cast<std::uint32_t>(size)})NativeProvenanceWord(result,word);
 const auto* bytes=static_cast<const std::uint8_t*>(code);result.insert(result.end(),bytes,bytes+size);NativeProvenanceWord(result,count);
 for(UINT i=0;i<count;++i) {
  const auto& e=elements[i];if(!e.SemanticName)return false;
  std::size_t n=0;while(n<128&&e.SemanticName[n])++n;if(!n||n==128)return false;
  NativeProvenanceWord(result,static_cast<std::uint32_t>(n));result.insert(result.end(),e.SemanticName,e.SemanticName+n);
  for(auto word:{e.SemanticIndex,static_cast<UINT>(e.Format),e.InputSlot,e.AlignedByteOffset,static_cast<UINT>(e.InputSlotClass),e.InstanceDataStepRate})NativeProvenanceWord(result,word);
 }
 out=std::move(result);return true;
}
inline bool ValidateNativeProvenance(const std::vector<std::uint8_t>& bytes,NativeProvenanceKind expected) {
 if(bytes.size()<16||bytes.size()>NativeProvenanceLimit)return false;
 std::size_t at=0;
 const auto word=[&](std::uint32_t& v){if(bytes.size()-at<4)return false;v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(bytes[at++])<<(8*i);return true;};
 std::uint32_t magic,version,kind,size;
 if(!word(magic)||!word(version)||!word(kind)||!word(size)||magic!=0x3152504eu||version!=1
  ||kind!=static_cast<std::uint32_t>(expected)||kind<1||kind>3||!size||size>NativeProvenanceBytecodeLimit||size>bytes.size()-at)return false;
 at+=size;
 if(expected==NativeProvenanceKind::InputLayout) {
  std::uint32_t count;if(!word(count)||count>32)return false;
  for(std::uint32_t i=0;i<count;++i) {
   std::uint32_t n;if(!word(n)||!n||n>127||n>bytes.size()-at)return false;
   for(std::uint32_t j=0;j<n;++j)if(!bytes[at+j])return false;
   at+=n;std::uint32_t field;for(unsigned j=0;j<6;++j)if(!word(field))return false;
  }
 }
 return at==bytes.size();
}
inline bool AttachNativeShaderProvenance(ID3D11DeviceChild* object,NativeProvenanceKind kind,const void* code,std::size_t size) noexcept {
 if(!object)return false;
 try {std::vector<std::uint8_t> bytes;if(!EncodeNativeShaderProvenance(kind,code,size,bytes))return false;
  return SUCCEEDED(object->SetPrivateData(NativeProvenanceGuid,static_cast<UINT>(bytes.size()),bytes.data()));
 }catch(...){return false;}
}
inline bool AttachNativeLayoutProvenance(ID3D11DeviceChild* object,const D3D11_INPUT_ELEMENT_DESC* elements,UINT count,const void* code,std::size_t size) noexcept {
 if(!object)return false;
 try {std::vector<std::uint8_t> bytes;if(!EncodeNativeLayoutProvenance(elements,count,code,size,bytes))return false;
  return SUCCEEDED(object->SetPrivateData(NativeProvenanceGuid,static_cast<UINT>(bytes.size()),bytes.data()));
 }catch(...){return false;}
}
inline bool ReadNativeProvenance(ID3D11DeviceChild* object,NativeProvenanceKind kind,std::vector<std::uint8_t>& out) noexcept {
 out.clear();if(!object)return false;
 try {UINT size=0;if(FAILED(object->GetPrivateData(NativeProvenanceGuid,&size,nullptr))||!size||size>NativeProvenanceLimit)return false;
  std::vector<std::uint8_t> bytes(size);UINT actual=size;
  if(FAILED(object->GetPrivateData(NativeProvenanceGuid,&actual,bytes.data()))||actual!=size||!ValidateNativeProvenance(bytes,kind))return false;
  out=std::move(bytes);return true;
 }catch(...){return false;}
}
}
