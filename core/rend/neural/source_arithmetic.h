// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <new>
#include <optional>
#include "source_transform.h"
namespace flycast::rend::neural {
struct SourceArithmetic {
 std::uint64_t serial=0;
 std::uint32_t pc=0,layout=0,lhs=0,rhs=0,result=0;
 bool exact=false;
};
inline bool VerifySourceArithmetic(const SourceArithmetic& record) noexcept {
 float lhs,rhs;std::memcpy(&lhs,&record.lhs,4);std::memcpy(&rhs,&record.rhs,4);
 if(!std::isfinite(lhs)||!std::isfinite(rhs))return false;
 float result;
 switch(record.layout&255) {
 case 1:result=lhs+rhs;break;case 2:result=lhs-rhs;break;
 case 3:result=lhs*rhs;break;case 4:if(rhs==0)return false;result=lhs/rhs;break;
 default:return false;
 }
 std::uint32_t bits;std::memcpy(&bits,&result,4);return bits==record.result;
}
inline thread_local SourceArithmetic pendingSourceArithmetic;
struct SourceArithmeticOrigin {std::uint32_t value=0;std::optional<SourceTransform> transform;std::uint64_t epoch=0;};
inline thread_local std::array<SourceArithmeticOrigin,256> sourceArithmeticOrigins{};
inline thread_local std::uint64_t sourceArithmeticEpoch=1;
inline thread_local unsigned sourceArithmeticLive=0;
inline thread_local std::optional<SourceTransform> pendingArithmeticOrigin,pendingDerivedStore;
inline void ClearSourceArithmeticOrigins() noexcept {
 if(sourceArithmeticEpoch==UINT64_MAX) {sourceArithmeticOrigins={};sourceArithmeticEpoch=1;}
 else ++sourceArithmeticEpoch;
 sourceArithmeticLive=0;
 pendingArithmeticOrigin.reset();pendingDerivedStore.reset();
}
inline void KillSourceArithmeticOrigin(std::uint32_t reg,std::uint32_t count) noexcept {
 for(unsigned i=0;i<count&&reg+i<256;++i) {
  auto& origin=sourceArithmeticOrigins[reg+i];
  if(origin.epoch==sourceArithmeticEpoch&&origin.transform)--sourceArithmeticLive;
  origin={};
 }
}
inline void SeedSourceArithmeticOrigin(std::uint32_t reg,const SourceTransform& transform) noexcept {
 for(unsigned i=0;i<4&&reg+i<255;++i) {
  auto& origin=sourceArithmeticOrigins[reg+i];
  if(origin.epoch!=sourceArithmeticEpoch||!origin.transform)++sourceArithmeticLive;
  std::memcpy(&origin.value,&transform.output[i],4);origin.transform=transform;origin.epoch=sourceArithmeticEpoch;
 }
}
inline std::optional<SourceTransform> GetSourceArithmeticOrigin(std::uint32_t reg,std::uint32_t value) noexcept {
 return reg<255&&sourceArithmeticOrigins[reg].epoch==sourceArithmeticEpoch&&sourceArithmeticOrigins[reg].value==value?sourceArithmeticOrigins[reg].transform:std::nullopt;
}
inline void CopySourceArithmeticOrigin(std::uint32_t src,std::uint32_t dst,std::uint32_t value) noexcept {
 const auto transform=GetSourceArithmeticOrigin(src,value);
 if(dst<255) {KillSourceArithmeticOrigin(dst,1);sourceArithmeticOrigins[dst]={value,transform,sourceArithmeticEpoch};if(transform)++sourceArithmeticLive;}
}
inline void PrepareDerivedStore(std::uint32_t reg,std::uint32_t value) noexcept {pendingDerivedStore=GetSourceArithmeticOrigin(reg,value);}
inline bool ValidateSourceArithmeticRegister(std::uint32_t reg,std::uint32_t value) noexcept {
 if(reg>=255)return false;
 const auto& origin=sourceArithmeticOrigins[reg];
 if(origin.epoch!=sourceArithmeticEpoch||!origin.transform)return false;
 if(origin.value!=value) {KillSourceArithmeticOrigin(reg,1);return false;}
 return true;
}
inline thread_local std::unique_ptr<std::array<SourceArithmetic,4096>> sourceArithmetic;
inline thread_local std::uint64_t sourceArithmeticSerial=0,sourceArithmeticRejected=0;
inline void BeginSourceArithmetic(std::uint32_t pc,std::uint32_t layout,std::uint32_t lhs,std::uint32_t rhs) noexcept {
 pendingSourceArithmetic={0,pc,layout,lhs,rhs};
 const auto left=GetSourceArithmeticOrigin((layout>>16)&255,lhs);
 const auto right=GetSourceArithmeticOrigin(layout>>24,rhs);
 pendingArithmeticOrigin=left?left:right;
 if(left&&right&&left->serial!=right->serial)pendingArithmeticOrigin.reset();
}
inline void EndSourceArithmetic(std::uint32_t result) noexcept {
 KillSourceArithmeticOrigin((pendingSourceArithmetic.layout>>8)&255,1);
 if(sourceArithmeticSerial==UINT64_MAX)return;
 if(!sourceArithmetic)sourceArithmetic.reset(new(std::nothrow) std::array<SourceArithmetic,4096>{});
 if(!sourceArithmetic)return;
 auto record=pendingSourceArithmetic;record.result=result;
 record.exact=VerifySourceArithmetic(record);if(!record.exact)++sourceArithmeticRejected;
 const auto dst=(record.layout>>8)&255;
 if(dst<255)sourceArithmeticOrigins[dst]={result,record.exact?pendingArithmeticOrigin:std::nullopt,sourceArithmeticEpoch};
 if(dst<255&&sourceArithmeticOrigins[dst].transform)++sourceArithmeticLive;
 record.serial=++sourceArithmeticSerial;
 (*sourceArithmetic)[record.serial%sourceArithmetic->size()]=record;
}
}
