// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <utility>
namespace flycast::rend::neural {
struct RemakeDepthUploadPlan {bool swap=false,current=true,previous=true;};
inline RemakeDepthUploadPlan PlanRemakeDepthUploads(std::uint64_t cachedCurrent,
 std::uint64_t cachedPrevious,std::uint64_t current,std::uint64_t previous) {
 RemakeDepthUploadPlan plan;
 if(previous&&previous==cachedCurrent){plan.swap=true;std::swap(cachedCurrent,cachedPrevious);}
 plan.current=!current||current!=cachedCurrent;
 plan.previous=!previous||previous!=cachedPrevious;
 return plan;
}
}
