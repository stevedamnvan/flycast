// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace flycast::rend::neural {
inline bool RemakeRendererAllowed(bool oit,const char* optIn)noexcept {
 return !oit||(optIn&&optIn[0]=='1'&&optIn[1]=='\0');
}
// Optional developer-only capture window. Invalid input disables capture;
// never turn malformed text into an unbounded run.
inline bool RemakeMovingCaptureEnabled(const char* text)noexcept {
 return text&&text[0]=='1'&&text[1]=='\0';
}
inline unsigned RemakePreviewCaptureLimit(const char* text,const char* moving=nullptr)noexcept {
 if(!text)return 3;
 if(!*text)return 0;
 const unsigned maximum=RemakeMovingCaptureEnabled(moving)?360:30;
 unsigned value=0;
 for(;*text;++text){if(*text<'0'||*text>'9')return 0;value=value*10+unsigned(*text-'0');if(value>maximum)return 0;}
 return value;
}
enum class RemakeDisplayKind { Fallback, HoldNative, Remake };
struct RemakeDisplayDecision {RemakeDisplayKind kind;std::uint64_t frame;};
// Align entry with a short original-native hold; never move displayed scene
// time backward. Timeout latches fallback until explicit reset/disable.
class RemakePresentationPolicy {
 enum class Phase { Idle, Warming, Active, Failed };
 Phase phase_=Phase::Idle;
 std::uint64_t floor_=0,last_=0,tick_=0;
public:
 void Reset()noexcept{*this={};}
 void Fail()noexcept{phase_=Phase::Failed;}
 RemakeDisplayDecision Choose(std::uint64_t current,std::uint64_t candidate,bool enabled)noexcept {
  if(!enabled){Reset();return {RemakeDisplayKind::Fallback,current};}
  if(!current||current<tick_||candidate>current)Fail();
  tick_=current;
  if(phase_==Phase::Failed)return {RemakeDisplayKind::Fallback,current};
  const bool valid=candidate&&current-candidate<=8;
  if(phase_==Phase::Idle&&valid){phase_=Phase::Warming;floor_=current;}
  if(phase_==Phase::Warming) {
   if(valid&&candidate>=floor_){phase_=Phase::Active;last_=candidate;}
   else if(current-floor_<=8)return {RemakeDisplayKind::HoldNative,floor_};
   else Fail();
  }
  if(phase_==Phase::Active) {
   if(valid&&candidate>=last_)last_=candidate;
   if(current-last_<=8)return {RemakeDisplayKind::Remake,last_};
   Fail();
  }
  return {RemakeDisplayKind::Fallback,current};
 }
};
}
