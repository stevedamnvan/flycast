// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstdlib>
#include <string_view>
namespace flycast::rend::neural {
struct RemakeExtent {
 std::uint32_t width=0,height=0;
 constexpr std::uint32_t Pixels()const{return width*height;}
 constexpr bool Valid()const{return (width==640&&height==480)||(width==1280&&height==960);}
};
inline RemakeExtent ParseRemakeExtent(std::string_view text) {
 if(text.empty()||text=="640x480")return {640,480};
 if(text=="1280x960")return {1280,960};
 return {};
}
// Immutable process launch contract. A changed extent/look requires a fresh
// launch/channel/history; never resize a live mapping or reuse old receipts.
inline RemakeExtent SelectedRemakeExtent() {
 static const RemakeExtent extent=[] {
  const char* value=std::getenv("FLYCAST_REMAKE_OUTPUT_SIZE");
  return ParseRemakeExtent(value?value:"");
 }();
 return extent;
}
inline std::uint32_t RemakeWidth(){return SelectedRemakeExtent().width;}
inline std::uint32_t RemakeHeight(){return SelectedRemakeExtent().height;}
inline std::uint32_t RemakePixels(){return SelectedRemakeExtent().Pixels();}
}
