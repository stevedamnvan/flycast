// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <vector>

namespace neuraltest::remake {
// Allocation correspondence only, in incoming draw order. The caller supplies
// the full compatibility and exact texture-byte predicate; no identity is
// inferred here. previousCount denotes an incoming allocation with no match.
template<class Compatible>
std::vector<std::size_t> LegacyReuseMapping(std::size_t incomingCount,
 std::size_t previousCount,Compatible compatible) {
 std::vector<std::size_t> result(incomingCount,previousCount);
 std::vector<bool> used(previousCount,false);
 for(std::size_t i=0;i<incomingCount;++i)
  for(std::size_t j=0;j<previousCount;++j)
   if(!used[j]&&compatible(i,j)) {
    result[i]=j;used[j]=true;break;
   }
 return result;
}
}
