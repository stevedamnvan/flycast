// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <vector>

namespace neuraltest::remake {
// Temporary COM ownership for refresh preparation. Swapping resources with the
// live collection commits ownership; destruction then retires the old slots.
template<class Resource>
struct LegacyPendingResources {
 std::vector<Resource> resources;
 LegacyPendingResources()=default;
 LegacyPendingResources(const LegacyPendingResources&)=delete;
 LegacyPendingResources& operator=(const LegacyPendingResources&)=delete;
 ~LegacyPendingResources() {
  for(auto& r:resources) {
   if(r.vb)r.vb->Release();
   if(r.texture)r.texture->Release();
  }
 }
 // Destination must be a fresh empty slot. Copy potentially throwing CPU data
 // first; COM AddRef is nonthrowing and each reference immediately has an owner.
 void Retain(std::size_t index,const Resource& source) {
  auto& next=resources[index];
  next.indices=source.indices;
  next.vb=source.vb;next.vb->AddRef();
  next.texture=source.texture;next.texture->AddRef();
 }
};
}
