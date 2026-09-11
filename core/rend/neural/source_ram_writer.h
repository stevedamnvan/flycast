// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include "source_transform.h"
namespace flycast::rend::neural {
struct SourceRamWrite {std::uint32_t address=0,pc=0,value=0;std::uint64_t transform=0;};
// Bounded direct-mapped observation cache. Aliases normalize to physical RAM;
// collisions replace records, never establish a match at a different address.
// D-218: the hot record is 24 bytes (the whole ring fits the second-level
// cache); the owned transform payload lives in a parallel array and is
// consulted only when its serial equals record.transform.
inline thread_local std::unique_ptr<std::array<SourceRamWrite,16384>> sourceRamWrites;
inline thread_local std::unique_ptr<std::array<std::optional<SourceTransform>,16384>> sourceRamOwnedTransforms;
inline std::optional<SourceTransform>& SourceRamOwnedTransform(std::size_t index) noexcept {
 return (*sourceRamOwnedTransforms)[index%sourceRamOwnedTransforms->size()];
}
inline void InvalidateSourceRamWrites() noexcept {
 if(sourceRamWrites) for(auto& record:*sourceRamWrites) {record.pc=0;record.transform=0;}
}
inline void ObserveSourceRamWrite(std::uint32_t address,std::uint32_t pc,
 std::uint32_t size,std::uint64_t value) noexcept {
 if((address&0x1c000000)!=0x0c000000)return;
 if(!sourceRamWrites) {
  sourceRamWrites.reset(new(std::nothrow) std::array<SourceRamWrite,16384>{});
  sourceRamOwnedTransforms.reset(new(std::nothrow) std::array<std::optional<SourceTransform>,16384>{});
  if(!sourceRamOwnedTransforms)sourceRamWrites.reset();
 }
 if(!sourceRamWrites)return;
 const auto base=address&0xffffff;
 if((size!=1&&size!=2&&size!=4&&size!=8)||base+size>0x1000000) {InvalidateSourceRamWrites();return;}
 for(unsigned offset=base&~3u;offset<base+size;offset+=4) {
  auto& record=(*sourceRamWrites)[(offset/4)%sourceRamWrites->size()];
  // Scalar reset only (D-217): the owned transform payload is consulted only
  // when its serial equals record.transform, which this clears.
  record.transform=0;
  if(offset>=base&&offset+4<=base+size) {record.address=offset;record.pc=pc;record.value=static_cast<std::uint32_t>(value>>((offset-base)*8));}
  else {record.address=0;record.pc=0;record.value=0;}
 }
}
inline std::uint32_t SourceRamWriter(std::uint32_t address,std::uint32_t value) noexcept {
 if(!sourceRamWrites||(address&3)||(address&0x1c000000)!=0x0c000000)return 0;
 const auto physical=address&0xffffff;
 const auto& record=(*sourceRamWrites)[(physical/4)%sourceRamWrites->size()];
 return record.address==physical&&record.value==value?record.pc:0;
}
inline std::optional<SourceTransform> SourceRamTransform(std::uint32_t address,std::uint32_t value) noexcept {
 if(!SourceRamWriter(address,value))return std::nullopt;
 const auto index=((address&0xffffff)/4)%sourceRamWrites->size();
 const auto& record=(*sourceRamWrites)[index];const auto& owned=SourceRamOwnedTransform(index);
 return owned&&owned->serial==record.transform?owned:std::nullopt;
}
// Read writer and optional transform from one matching observation. The output
// owns its transform; no pointer into the mutable direct-mapped cache escapes.
inline std::uint32_t ReadSourceRamObservation(std::uint32_t address,std::uint32_t value,
 std::optional<SourceTransform>& transform) noexcept {
 transform.reset();
 if(!sourceRamWrites||(address&3)||(address&0x1c000000)!=0x0c000000)return 0;
 const auto physical=address&0xffffff;
 const auto index=(physical/4)%sourceRamWrites->size();
 const auto& record=(*sourceRamWrites)[index];
 if(record.address!=physical||record.value!=value||!record.pc)return 0;
 const auto& owned=SourceRamOwnedTransform(index);
 if(owned&&owned->serial==record.transform)transform=*owned;
 return record.pc;
}
inline bool CarrySourceRamTransform(std::uint32_t address,std::uint32_t pc,std::uint32_t value,
 const SourceTransform* transform) noexcept {
 if(!transform||!transform->serial||SourceRamWriter(address,value)!=pc||!pc)return false;
 const auto index=((address&0xffffff)/4)%sourceRamWrites->size();
 (*sourceRamWrites)[index].transform=transform->serial;SourceRamOwnedTransform(index)=*transform;return true;
}
inline bool CarrySourceRamTransform(std::uint32_t address,std::uint32_t pc,std::uint32_t value,
 const std::optional<SourceTransform>& transform) noexcept {
 return CarrySourceRamTransform(address,pc,value,transform?&*transform:nullptr);
}
}
