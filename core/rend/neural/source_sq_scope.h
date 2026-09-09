// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <limits>
namespace flycast::rend::neural {
// Invocation identity only, NOT a RAM/texture content generation or transform.
struct SourceSqInvocation {std::uint64_t serial=0;std::uint32_t pc=0,address=0;};
inline thread_local SourceSqInvocation currentSourceSq{};
inline thread_local std::uint64_t sourceSqSerial=0;
inline thread_local bool sourceSqScopeActive=false;
class SourceSqScope {
public:
 SourceSqScope(std::uint32_t pc,std::uint32_t address):previous_(currentSourceSq),wasActive_(sourceSqScopeActive) {
  sourceSqScopeActive=true;
  currentSourceSq={};
  if(!wasActive_&&pc&&sourceSqSerial!=(std::numeric_limits<std::uint64_t>::max)())
   currentSourceSq={++sourceSqSerial,pc,address};
 }
 ~SourceSqScope(){currentSourceSq=previous_;sourceSqScopeActive=wasActive_;}
 SourceSqScope(const SourceSqScope&)=delete;
 SourceSqScope& operator=(const SourceSqScope&)=delete;
private: SourceSqInvocation previous_;bool wasActive_;
};
}
