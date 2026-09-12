// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// D-240 narrowed observation scope (experimental, opt-in
// FLYCAST_REMAKE_OBSERVATION_SCOPE=narrow; default full). The source
// observation hooks stay compiled into every block, but each hooked block
// reads one flag at entry and its hooks run only while that flag is set.
// Discovery runs with every flag set; the guest code regions whose hooks fed a
// complete copy observation (SQ writer, XYZ stores, reads, RAM producers,
// FTRV, and the arithmetic and boundaries that carried a live origin) are
// collected from the process's own execution, never from a configuration.
// Once observations have been complete and the region set stable for long
// enough, only blocks overlapping a region keep their hooks. If complete
// observations then stop while submissions continue, every flag is set again
// (widening), discovery restarts and the union is kept; after a bounded
// number of widenings the scope stays full for the rest of the session.
// The gate code is emitted in full mode too (every flag set), so both modes
// compile byte-identical blocks and share one code-cache reset schedule,
// which keeps the emulated timeline the same across the two modes.
// What narrows is which code is watched, not what an observation asserts:
// records, value checks and the anchor's rejections are unchanged, and a
// vertex whose chain ran unwatched simply has no observation. The flag is
// read once per block execution, so a flip never splits a hook pair.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>
namespace flycast::rend::neural {
inline bool SourceObservationScopeRequested() noexcept {
 static const bool requested=[](){const char* v=std::getenv("FLYCAST_REMAKE_OBSERVATION_SCOPE");return v&&std::strcmp(v,"narrow")==0;}();
 return requested;
}
// Diagnostic only: FLYCAST_REMAKE_OBSERVATION_SCOPE_PARTS limits which parts of
// the scope bookkeeping run (comma list of note, ta, ctrl, reg; default all).
enum SourceObservationScopePart : unsigned { ScopePartNote=1, ScopePartTa=2, ScopePartCtrl=4, ScopePartReg=8 };
inline bool SourceObservationScopePartEnabled(unsigned part) noexcept {
 static const unsigned mask=[](){const char* v=std::getenv("FLYCAST_REMAKE_OBSERVATION_SCOPE_PARTS");if(!v)return 15u;
  unsigned m=0;if(std::strstr(v,"note"))m|=ScopePartNote;if(std::strstr(v,"ta"))m|=ScopePartTa;if(std::strstr(v,"ctrl"))m|=ScopePartCtrl;if(std::strstr(v,"reg"))m|=ScopePartReg;return m;}();
 return (mask&part)!=0;
}
enum class SourceObservationScopeMode : int { Full=0, Narrow=1 };
enum class SourceObservationScopeEvent : int { None=0, Narrow=1, Widen=2 };
inline const char* SourceObservationScopeName(SourceObservationScopeMode m) noexcept { return m==SourceObservationScopeMode::Narrow?"narrow":"full"; }
inline const char* SourceObservationScopeEventName(SourceObservationScopeEvent e) noexcept {
 return e==SourceObservationScopeEvent::Narrow?"narrow":e==SourceObservationScopeEvent::Widen?"widen":"none";
}
struct SourceObservationScope {
 static constexpr unsigned DiscoveryFrames=300,StableFrames=60,WidenAfterEmptyFrames=30,WidenBound=8;
 SourceObservationScopeMode mode=SourceObservationScopeMode::Full;
 std::map<std::uint32_t,std::uint32_t> regions; // start -> end (exclusive), disjoint and non-adjacent.
 unsigned framesObserved=0,stableFrames=0,emptyFrames=0,widened=0,narrowings=0;
 bool exhausted=false,changed=false;
 std::size_t RegionBytes()const{std::size_t n=0;for(const auto& r:regions)n+=r.second-r.first;return n;}
 // Adds [start,end) and merges overlapping or adjacent regions; true when the set grew.
 bool RecordRegion(std::uint32_t start,std::uint32_t end) {
  if(end<=start)return false;
  auto it=regions.upper_bound(start);
  if(it!=regions.begin()){auto prev=std::prev(it);if(prev->second>=start){if(prev->second>=end)return false;start=prev->first;end=(std::max)(end,prev->second);regions.erase(prev);}}
  for(it=regions.lower_bound(start);it!=regions.end()&&it->first<=end;){end=(std::max)(end,it->second);it=regions.erase(it);}
  regions[start]=end;changed=true;return true;
 }
 bool Contains(std::uint32_t pc)const{auto it=regions.upper_bound(pc);return it!=regions.begin()&&std::prev(it)->second>pc;}
 // Whether a block [start,start+size) keeps its hooks under the current mode.
 bool Observed(std::uint32_t start,std::uint32_t size)const {
  if(mode==SourceObservationScopeMode::Full)return true;
  if(!size)return false;
  auto it=regions.upper_bound(start);
  if(it!=regions.begin()&&std::prev(it)->second>start)return true;
  return it!=regions.end()&&it->first<start+size;
 }
 // Called once per emulated frame with the complete records and SQ
 // submissions that frame saw. Returns the transition to apply, if any.
 SourceObservationScopeEvent EndFrame(std::size_t complete,std::size_t submissions) {
  const bool grew=changed;changed=false;
  if(mode==SourceObservationScopeMode::Full) {
   if(exhausted)return SourceObservationScopeEvent::None;
   ++framesObserved;
   if(grew)stableFrames=0;else if(complete)++stableFrames;
   if(framesObserved>=DiscoveryFrames&&stableFrames>=StableFrames&&!regions.empty()) {
    mode=SourceObservationScopeMode::Narrow;emptyFrames=0;++narrowings;return SourceObservationScopeEvent::Narrow;
   }
   return SourceObservationScopeEvent::None;
  }
  if(submissions&&!complete)++emptyFrames;else emptyFrames=0;
  if(emptyFrames<WidenAfterEmptyFrames)return SourceObservationScopeEvent::None;
  mode=SourceObservationScopeMode::Full;framesObserved=0;stableFrames=0;emptyFrames=0;
  if(widened>=WidenBound)exhausted=true;else ++widened;
  return SourceObservationScopeEvent::Widen;
 }
};
inline SourceObservationScope sourceObservationScope; // Emulation thread only.
// Per-block flag table read by emitted code at block entry: slot by block
// start address; a collision can only keep a block watched, never unwatch it.
constexpr std::size_t SourceObservationFlagSlots=65536;
inline std::uint32_t SourceObservationFlagSlot(std::uint32_t start) noexcept { return (start>>1)&(SourceObservationFlagSlots-1); }
struct SourceObservationFlagTable { std::array<std::uint8_t,SourceObservationFlagSlots> flags; SourceObservationFlagTable(){flags.fill(1);} };
inline SourceObservationFlagTable sourceObservationFlags;
inline std::uint8_t sourceObservationBlockActive=1; // Loaded from the table at each hooked block's entry.
// Registered compiled blocks (start -> byte size); shared with the hook attribution diagnostic.
inline std::map<std::uint32_t,std::uint32_t> sourceObservationBlocks;
inline std::size_t sourceObservationWatchedBlocks=0;
inline void ResetSourceObservationBlocks() noexcept {
 sourceObservationBlocks.clear();sourceObservationWatchedBlocks=0;
 sourceObservationFlags.flags.fill(sourceObservationScope.mode==SourceObservationScopeMode::Full?1:0);
}
// Block range holding pc: the registered block with the greatest start at or below it.
inline bool SourceObservationBlockRange(std::uint32_t pc,std::uint32_t& start,std::uint32_t& end) noexcept {
 auto it=sourceObservationBlocks.upper_bound(pc);
 if(it==sourceObservationBlocks.begin())return false;
 --it;if(pc>=it->first+it->second)return false;
 start=it->first;end=it->first+it->second;return true;
}
inline void RegisterSourceObservationBlock(std::uint32_t start,std::uint32_t size) noexcept {
 if(!start||!size||!SourceObservationScopePartEnabled(ScopePartReg))return;
 try { sourceObservationBlocks[start]=size; } catch(...) { return; }
 auto& flag=sourceObservationFlags.flags[SourceObservationFlagSlot(start)];
 const bool watched=sourceObservationScope.Observed(start,size);
 if(watched){if(!flag)flag=1;++sourceObservationWatchedBlocks;}
}
// Recomputes every flag from the registered blocks under the current mode.
inline void ApplySourceObservationScopeFlags() noexcept {
 auto& flags=sourceObservationFlags.flags;
 const bool full=sourceObservationScope.mode==SourceObservationScopeMode::Full;
 flags.fill(full?1:0);sourceObservationWatchedBlocks=0;
 for(const auto& block:sourceObservationBlocks)
  if(sourceObservationScope.Observed(block.first,block.second)){flags[SourceObservationFlagSlot(block.first)]=1;++sourceObservationWatchedBlocks;}
}
// Discovery: the block holding a contributing PC becomes a region. The last
// resolved range is cached because contributing hooks run in bursts.
inline thread_local std::uint32_t sourceObservationLastStart=0,sourceObservationLastEnd=0;
inline void NoteSourceObservationContributor(std::uint32_t pc) noexcept {
 if(!pc||sourceObservationScope.mode!=SourceObservationScopeMode::Full||!SourceObservationScopePartEnabled(ScopePartNote))return;
 if(pc>=sourceObservationLastStart&&pc<sourceObservationLastEnd)return;
 std::uint32_t start=0,end=0;
 if(!SourceObservationBlockRange(pc,start,end))return;
 sourceObservationLastStart=start;sourceObservationLastEnd=end;
 try { sourceObservationScope.RecordRegion(start,end); } catch(...) {}
}
inline void ForgetSourceObservationContributorCache() noexcept { sourceObservationLastStart=sourceObservationLastEnd=0; }
// Per-frame counters filled by the TA submission path.
inline thread_local std::uint64_t sourceObservationFrameComplete=0,sourceObservationFrameSubmissions=0;
}
