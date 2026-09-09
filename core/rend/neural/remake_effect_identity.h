// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace flycast::rend::neural {
// Developer replay evidence only. Input must be obtained from an owned snapshot,
// never from the current frame. No hashing of unused allocation or GPU addresses.
struct EffectIdentityPixel { std::uint32_t color, depthBits, sequence, next; };
struct EffectIdentityPoly { std::uint32_t primary, secondary; };
struct EffectIdentitySource { std::uint64_t epoch, ordinal, cycle; };
inline bool CanonicalEffectIdentity(const EffectIdentitySource& source,
 const std::vector<std::uint32_t>& heads,
 const std::vector<EffectIdentityPixel>& pixels,
 const std::vector<EffectIdentityPoly>& polygons,
 const std::vector<std::uint32_t>& resolverState, std::uint32_t maxLayers,
 std::vector<std::uint32_t>& output, std::string& error)
{
 output.clear();
 const auto fail=[&](const char* why){error=why;return false;};
 if(!source.epoch||!source.ordinal||!source.cycle||heads.empty()||heads.size()>640u*480
  ||pixels.size()>512u*1024*1024/16||polygons.empty()||polygons.size()>8192
  ||resolverState.empty()||resolverState.size()>1024||!maxLayers||maxLayers>256)
  return fail("effect-identity-input-bound");
 std::vector<std::uint32_t> result{1}; // version, full source and resolver state
 for(auto v:{source.epoch,source.ordinal,source.cycle}) {
  result.push_back(static_cast<std::uint32_t>(v));result.push_back(static_cast<std::uint32_t>(v>>32));
 }
 result.push_back(maxLayers);result.push_back(static_cast<std::uint32_t>(heads.size()));
 result.push_back(static_cast<std::uint32_t>(resolverState.size()));
 result.insert(result.end(),resolverState.begin(),resolverState.end());
 std::vector<std::uint32_t> chain;chain.reserve(maxLayers);
 const auto depth=[&](std::uint32_t i){float value;std::memcpy(&value,&pixels[i].depthBits,4);return value;};
 for(auto head:heads) {
  chain.clear();
  for(auto index=head;index!=0xffffffffu;index=pixels[index].next) {
   if(index>=pixels.size())return fail("effect-identity-pointer-range");
   if(std::find(chain.begin(),chain.end(),index)!=chain.end())return fail("effect-identity-cycle");
   if(chain.size()==maxLayers)return fail("effect-identity-truncated-stack");
   if(!std::isfinite(depth(index)))return fail("effect-identity-invalid-depth");
   if(((pixels[index].sequence&0x3fffffffu)>>17)>=polygons.size())return fail("effect-identity-polygon-range");
   chain.push_back(index);
  }
  // Same stable ordering as fillAndSortFragmentArray. Equal-key traversal order
  // is meaningful; changing it must not disappear during canonicalization.
  std::stable_sort(chain.begin(),chain.end(),[&](auto a,auto b){
   return depth(a)<depth(b)||(depth(a)==depth(b)
    &&(pixels[a].sequence&0x3fffffffu)<(pixels[b].sequence&0x3fffffffu));
  });
  if(result.size()+1+chain.size()*5>16u*1024*1024)return fail("effect-identity-output-bound");
  result.push_back(static_cast<std::uint32_t>(chain.size()));
  for(auto index:chain) {
   const auto& p=pixels[index];const auto& poly=polygons[(p.sequence&0x3fffffffu)>>17];
   result.insert(result.end(),{p.color,p.depthBits,p.sequence,poly.primary,poly.secondary});
  }
 }
 output=std::move(result);error.clear();return true;
}
}
