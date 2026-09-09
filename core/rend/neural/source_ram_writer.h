// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <new>
namespace flycast::rend::neural {
struct SourceRamWrite {std::uint32_t address=0,pc=0,value=0;};
// Bounded direct-mapped observation cache. Aliases normalize to physical RAM;
// collisions replace records, never establish a match at a different address.
inline thread_local std::unique_ptr<std::array<SourceRamWrite,16384>> sourceRamWrites;
inline void InvalidateSourceRamWrites() noexcept {
 if(sourceRamWrites) sourceRamWrites->fill({});
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
  record={};
  if(offset>=base&&offset+4<=base+size)
   record={offset,pc,static_cast<std::uint32_t>(value>>((offset-base)*8))};
 }
}
inline std::uint32_t SourceRamWriter(std::uint32_t address,std::uint32_t value) noexcept {
 if(!sourceRamWrites||(address&3)||(address&0x1c000000)!=0x0c000000)return 0;
 const auto physical=address&0xffffff;
 const auto& record=(*sourceRamWrites)[(physical/4)%sourceRamWrites->size()];
 return record.address==physical&&record.value==value?record.pc:0;
}
}
