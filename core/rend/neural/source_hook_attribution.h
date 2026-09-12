// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// D-240 groundwork (diagnostic only, FLYCAST_REMAKE_HOOK_ATTRIBUTION=1 with
// the CPU timing diagnostic): which guest code the source-observation hooks
// run from, and which of that code actually feeds a complete copy
// observation (SQ writer, XYZ store, read and RAM-producer PCs and the FTRV
// transform PC). Counting only; no hook is skipped, no observation semantics
// change, nothing here is performance evidence. The summary is the evidence
// a narrowed observation scope needs before any hook is confined to a region.
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "source_observation_scope.h"
namespace flycast::rend::neural {
enum class SourceHookKind : unsigned { Store=0, SqWrite, Arithmetic, Ftrv, BlockEntry, Boundary, Read, Count };
constexpr unsigned SourceHookKindCount=unsigned(SourceHookKind::Count);
inline const char* SourceHookKindName(unsigned kind) noexcept {
 static const char* const names[SourceHookKindCount]={"stores","sq_writes","arithmetic","ftrv","block_entries","boundaries","reads"};
 return kind<SourceHookKindCount?names[kind]:"unknown";
}
struct SourceHookPcTally { std::array<std::uint64_t,SourceHookKindCount> calls{}; };
using SourceHookPcTallies=std::unordered_map<std::uint32_t,SourceHookPcTally>;
using SourceHookContributors=std::unordered_set<std::uint32_t>;
inline bool SourceHookAttributionEnabled=false;
inline thread_local SourceHookPcTallies sourceHookPcTallies;
inline thread_local SourceHookContributors sourceHookContributingPcs;
// Compiled block ranges are the scope header's registry (the recompiler
// registers every block when either the diagnostic or the narrowed scope is
// requested); an interior PC resolves to the block with the greatest start at
// or below it, or 0 when no registered block holds it.
inline std::uint32_t SourceHookBlockOf(std::uint32_t pc) noexcept {
 std::uint32_t start=0,end=0;return SourceObservationBlockRange(pc,start,end)?start:0;
}
inline void AttributeSourceHook(SourceHookKind kind,std::uint32_t pc) noexcept {
 if(!SourceHookAttributionEnabled||!pc)return;
 try { ++sourceHookPcTallies[pc].calls[unsigned(kind)]; } catch(...) {}
}
inline void NoteSourceHookContributor(std::uint32_t pc) noexcept {
 if(!pc)return;
 if(SourceHookAttributionEnabled){try { sourceHookContributingPcs.insert(pc); } catch(...) {}}
 if(SourceObservationScopeRequested())NoteSourceObservationContributor(pc);
}
struct SourceHookAttributionSummary {
 std::size_t pcs=0,blocks=0,contributingPcs=0,contributingBlocks=0;
 std::array<std::uint64_t,SourceHookKindCount> total{},fromContributingPcs{},fromContributingBlocks{};
 struct Block { std::uint32_t start=0; std::uint64_t calls=0; bool contributing=false; std::array<std::uint64_t,SourceHookKindCount> byKind{}; };
 std::vector<Block> topBlocks; // Descending by calls, bounded.
};
// blockOf(pc) returns the compiled block's start address, or 0 when the PC
// is not in a known block; such a PC counts as its own block. A block is
// contributing when it holds any contributing PC.
template<class BlockOf>
SourceHookAttributionSummary SummarizeSourceHookAttribution(const SourceHookPcTallies& tallies,
 const SourceHookContributors& contributors,BlockOf blockOf,std::size_t topBound=16) {
 SourceHookAttributionSummary s;s.pcs=tallies.size();s.contributingPcs=contributors.size();
 std::unordered_map<std::uint32_t,SourceHookAttributionSummary::Block> blocks;
 std::unordered_set<std::uint32_t> contributingBlocks;
 const auto key=[&](std::uint32_t pc){const std::uint32_t b=blockOf(pc);return b?b:pc;};
 for(auto pc:contributors)contributingBlocks.insert(key(pc));
 for(const auto& entry:tallies) {
  const auto block=key(entry.first);auto& b=blocks[block];b.start=block;b.contributing=contributingBlocks.count(block)>0;
  const bool contributingPc=contributors.count(entry.first)>0;
  for(unsigned k=0;k<SourceHookKindCount;++k) {
   const auto n=entry.second.calls[k];b.calls+=n;b.byKind[k]+=n;s.total[k]+=n;
   if(contributingPc)s.fromContributingPcs[k]+=n;
   if(b.contributing)s.fromContributingBlocks[k]+=n;
  }
 }
 s.blocks=blocks.size();
 for(const auto& b:blocks)s.contributingBlocks+=b.second.contributing;
 s.topBlocks.reserve(blocks.size());
 for(const auto& b:blocks)s.topBlocks.push_back(b.second);
 std::sort(s.topBlocks.begin(),s.topBlocks.end(),[](const auto& a,const auto& b){return a.calls!=b.calls?a.calls>b.calls:a.start<b.start;});
 if(s.topBlocks.size()>topBound)s.topBlocks.resize(topBound);
 return s;
}
}
