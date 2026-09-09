// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <limits>
#include <array>
#include <atomic>
#include "source_ram_writer.h"
#include "source_arithmetic.h"
namespace flycast::rend::neural {
struct SourceSqByteWriter {std::uint32_t pc=0;std::uint8_t value=0;std::uint32_t ram=0,readPc=0,producerPc=0;};
struct SourceRegisterRead {std::uint32_t address=0,pc=0,value=0;bool valid=false;std::uint32_t producerPc=0;std::optional<SourceTransform> transform;};
inline thread_local std::array<std::optional<SourceTransform>,16> sourceSqTransforms{};
inline thread_local std::array<SourceRegisterRead,256> sourceRegisterReads{};
inline thread_local std::array<SourceSqByteWriter,64> sourceSqWriters{};
inline std::atomic<std::uint64_t> sourceSqResetEpoch{1};
inline thread_local std::uint64_t sourceSqSeenEpoch=1;
inline void InvalidateSourceSqWriters() noexcept {sourceSqWriters={};sourceSqTransforms={};sourceRegisterReads={};InvalidateSourceRamWrites();ClearSourceArithmeticOrigins();}
inline void ResetSourceSqWriters() noexcept {sourceSqResetEpoch.fetch_add(1,std::memory_order_relaxed);}
inline void RefreshSourceSqWriters() noexcept {
 const auto epoch=sourceSqResetEpoch.load(std::memory_order_relaxed);
 if(epoch!=sourceSqSeenEpoch) {InvalidateSourceSqWriters();sourceSqSeenEpoch=epoch;}
}
inline void ObserveSourceSqStore(std::uint32_t address,std::uint32_t pc,
 std::uint32_t size,std::uint64_t value) noexcept {
 if ((address>>26)!=0x38 || !pc) return;
 RefreshSourceSqWriters();
 const auto readSlot=(size>>8)&511;size&=255;
 const auto offset=address&63;
 if ((size!=1&&size!=2&&size!=4&&size!=8)||offset+size>64) {
  sourceSqWriters={};return;
 }
 const SourceRegisterRead* read=readSlot && readSlot<=sourceRegisterReads.size() ? &sourceRegisterReads[readSlot-1] : nullptr;
 const bool linked=read&&read->valid&&size==4&&read->value==static_cast<std::uint32_t>(value);
 for(unsigned word=offset/4;word<=(offset+size-1)/4;++word)sourceSqTransforms[word].reset();
 if(linked&&!(offset&3))sourceSqTransforms[offset/4]=read->transform;
 for(unsigned i=0;i<size;++i) sourceSqWriters[offset+i]={pc,static_cast<std::uint8_t>(value>>(8*i)),linked?read->address+i:0,linked?read->pc:0,linked?read->producerPc:0};
}
// Invocation identity only, NOT a RAM/texture content generation or transform.
struct SourceSqInvocation {std::uint64_t serial=0;std::uint32_t pc=0,address=0;};
inline thread_local SourceSqInvocation currentSourceSq{};
inline thread_local std::uint64_t sourceSqSerial=0;
inline thread_local bool sourceSqScopeActive=false;
class SourceSqScope {
public:
 SourceSqScope(std::uint32_t pc,std::uint32_t address):previous_(currentSourceSq),wasActive_(sourceSqScopeActive) {
  RefreshSourceSqWriters();
  sourceSqScopeActive=true;
  currentSourceSq={};
  if(!wasActive_&&pc&&sourceSqSerial!=(std::numeric_limits<std::uint64_t>::max)())
   currentSourceSq={++sourceSqSerial,pc,address};
 }
 ~SourceSqScope(){
  // A subsequent submission requires newly witnessed writes, even if old SQ
  // bytes remain physically present. Never carry writer authority across PREF.
  if(!wasActive_) {
   const auto base=currentSourceSq.address&32;
   for(unsigned i=0;i<32;++i) sourceSqWriters[base+i]={};
   for(unsigned i=0;i<8;++i)sourceSqTransforms[base/4+i].reset();
  }
  currentSourceSq=previous_;sourceSqScopeActive=wasActive_;
 }
 SourceSqScope(const SourceSqScope&)=delete;
 SourceSqScope& operator=(const SourceSqScope&)=delete;
private: SourceSqInvocation previous_;bool wasActive_;
};
}
