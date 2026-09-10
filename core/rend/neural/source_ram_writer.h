// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include "source_transform.h"
namespace flycast::rend::neural {
struct SourceRamWrite {std::uint32_t address=0,pc=0,value=0;std::uint64_t transform=0;std::optional<SourceTransform> ownedTransform;};
// Bounded direct-mapped observation cache. Aliases normalize to physical RAM;
// collisions replace records, never establish a match at a different address.
inline thread_local std::unique_ptr<std::array<SourceRamWrite,16384>> sourceRamWrites;
inline void InvalidateSourceRamWrites() noexcept {
 if(sourceRamWrites) for(auto& record:*sourceRamWrites) {record.pc=0;record.transform=0;}
}
inline void ObserveSourceRamWrite(std::uint32_t address,std::uint32_t pc,
 std::uint32_t size,std::uint64_t value) noexcept {
 if((address&0x1c000000)!=0x0c000000)return;
 if(!sourceRamWrites)sourceRamWrites.reset(new(std::nothrow) std::array<SourceRamWrite,16384>{});
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
 const auto& record=(*sourceRamWrites)[((address&0xffffff)/4)%sourceRamWrites->size()];
 return record.ownedTransform&&record.ownedTransform->serial==record.transform?record.ownedTransform:std::nullopt;
}
inline bool CarrySourceRamTransform(std::uint32_t address,std::uint32_t pc,std::uint32_t value,
 const SourceTransform* transform) noexcept {
 if(!transform||!transform->serial||SourceRamWriter(address,value)!=pc||!pc)return false;
 auto& record=(*sourceRamWrites)[((address&0xffffff)/4)%sourceRamWrites->size()];
 record.transform=transform->serial;record.ownedTransform=*transform;return true;
}
inline bool CarrySourceRamTransform(std::uint32_t address,std::uint32_t pc,std::uint32_t value,
 const std::optional<SourceTransform>& transform) noexcept {
 return CarrySourceRamTransform(address,pc,value,transform?&*transform:nullptr);
}
}
