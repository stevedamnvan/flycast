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
#include "remake_cpu_scope.h"
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
// Non-thread-local mirror of sourceArithmeticLive for the recompiler's inline
// gate (D-217): only the emulation thread executes recompiled code, and the
// hooks below are no-ops while no origin is live.
inline std::uint32_t sourceArithmeticLiveFlag=0;
inline void MirrorSourceArithmeticLive() noexcept {sourceArithmeticLiveFlag=sourceArithmeticLive;}
// D-218: one byte per register, nonzero while that register holds a live
// origin (epoch current and transform present); maintained wherever the
// live count changes so the recompiler can skip boundary calls per register.
inline std::uint8_t sourceArithmeticLiveBytes[256]{};
inline void MirrorSourceArithmeticLiveByte(std::uint32_t reg) noexcept {
 if(reg<256)sourceArithmeticLiveBytes[reg]=sourceArithmeticOrigins[reg].epoch==sourceArithmeticEpoch&&sourceArithmeticOrigins[reg].transform.has_value();
}
inline thread_local std::optional<SourceTransform> pendingArithmeticOrigin,pendingDerivedStore;
inline void ClearSourceArithmeticOrigins() noexcept {
 if(sourceArithmeticEpoch==UINT64_MAX) {sourceArithmeticOrigins={};sourceArithmeticEpoch=1;}
 else ++sourceArithmeticEpoch;
 sourceArithmeticLive=0;MirrorSourceArithmeticLive();std::memset(sourceArithmeticLiveBytes,0,sizeof(sourceArithmeticLiveBytes));
 pendingArithmeticOrigin.reset();pendingDerivedStore.reset();
}
inline void KillSourceArithmeticOrigin(std::uint32_t reg,std::uint32_t count) noexcept {
 for(unsigned i=0;i<count&&reg+i<256;++i) {
  auto& origin=sourceArithmeticOrigins[reg+i];
  if(origin.epoch==sourceArithmeticEpoch&&origin.transform)--sourceArithmeticLive;
  // Invalidation needs no write to the inactive transform's matrix payload.
  // Epoch/value checks and disengagement retain the same lookup semantics.
  origin.transform.reset();origin.epoch=0;origin.value=0;sourceArithmeticLiveBytes[reg+i]=0;
 }
 MirrorSourceArithmeticLive();
}
inline void SeedSourceArithmeticOrigin(std::uint32_t reg,const SourceTransform& transform) noexcept {
 for(unsigned i=0;i<4&&reg+i<255;++i) {
  auto& origin=sourceArithmeticOrigins[reg+i];
  if(origin.epoch!=sourceArithmeticEpoch||!origin.transform)++sourceArithmeticLive;
  std::memcpy(&origin.value,&transform.output[i],4);origin.transform=transform;origin.epoch=sourceArithmeticEpoch;
  sourceArithmeticLiveBytes[reg+i]=1;
 }
 MirrorSourceArithmeticLive();
}
inline std::optional<SourceTransform> GetSourceArithmeticOrigin(std::uint32_t reg,std::uint32_t value) noexcept {
 return reg<255&&sourceArithmeticOrigins[reg].epoch==sourceArithmeticEpoch&&sourceArithmeticOrigins[reg].value==value?sourceArithmeticOrigins[reg].transform:std::nullopt;
}
inline void CopySourceArithmeticOrigin(std::uint32_t src,std::uint32_t dst,std::uint32_t value) noexcept {
 SourceHookCycles cycles(SourceHookCopyCycles);
 if(!sourceArithmeticLive)return; // D-217: nothing to copy or kill.
 const auto transform=GetSourceArithmeticOrigin(src,value);
 if(dst<255) {KillSourceArithmeticOrigin(dst,1);sourceArithmeticOrigins[dst]={value,transform,sourceArithmeticEpoch};if(transform)++sourceArithmeticLive;MirrorSourceArithmeticLiveByte(dst);}
 MirrorSourceArithmeticLive();
}
inline void PrepareDerivedStore(std::uint32_t reg,std::uint32_t value) noexcept {pendingDerivedStore=GetSourceArithmeticOrigin(reg,value);}
// Same lookup without copying the transform; the pointer is valid until the
// next origin change on this thread (D-217: one call per observed store).
inline const SourceTransform* SourceArithmeticOriginPtr(std::uint32_t reg,std::uint32_t value) noexcept {
 if(reg>=255)return nullptr;
 const auto& origin=sourceArithmeticOrigins[reg];
 return origin.epoch==sourceArithmeticEpoch&&origin.value==value&&origin.transform?&*origin.transform:nullptr;
}
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
 ++SourceHookArithmeticCalls;
 SourceHookCycles cycles(SourceHookArithmeticCycles);
 pendingSourceArithmetic={0,pc,layout,lhs,rhs};
 if(!sourceArithmeticLive){pendingArithmeticOrigin.reset();return;} // D-217: no origin can propagate.
 const auto left=GetSourceArithmeticOrigin((layout>>16)&255,lhs);
 const auto right=GetSourceArithmeticOrigin(layout>>24,rhs);
 pendingArithmeticOrigin=left?left:right;
 if(left&&right&&left->serial!=right->serial)pendingArithmeticOrigin.reset();
}
inline void EndSourceArithmetic(std::uint32_t result) noexcept {
 SourceHookCycles cycles(SourceHookArithmeticCycles);
 KillSourceArithmeticOrigin((pendingSourceArithmetic.layout>>8)&255,1);
 if(sourceArithmeticSerial==UINT64_MAX)return;
 if(!sourceArithmetic)sourceArithmetic.reset(new(std::nothrow) std::array<SourceArithmetic,4096>{});
 if(!sourceArithmetic)return;
 auto record=pendingSourceArithmetic;record.result=result;
 record.exact=VerifySourceArithmetic(record);if(!record.exact)++sourceArithmeticRejected;
 const auto dst=(record.layout>>8)&255;
 if(dst<255)sourceArithmeticOrigins[dst]={result,record.exact?pendingArithmeticOrigin:std::nullopt,sourceArithmeticEpoch};
 if(dst<255&&sourceArithmeticOrigins[dst].transform)++sourceArithmeticLive;
 if(dst<255)MirrorSourceArithmeticLiveByte(dst);
 MirrorSourceArithmeticLive();
 record.serial=++sourceArithmeticSerial;
 (*sourceArithmetic)[record.serial%sourceArithmetic->size()]=record;
}
}
