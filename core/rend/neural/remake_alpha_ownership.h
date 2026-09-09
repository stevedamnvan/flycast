// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_effect_identity.h"
#include "producer_identity.h"
#include <set>
namespace flycast::rend::neural {
struct AlphaEffectSelection { std::uint32_t ordinal; EffectIdentityPoly expected; };
inline std::vector<std::uint32_t> AlphaEffectSelectionIdentity(const std::vector<AlphaEffectSelection>& selected) {
 if(selected.size()>128)return {};
 std::vector<std::uint32_t> words{1,static_cast<std::uint32_t>(selected.size())};
 for(const auto& s:selected)words.insert(words.end(),{s.ordinal,s.expected.primary,s.expected.secondary});
 return words;
}
// Plan against source-owned native parameters, never a current-frame ordinal.
// Keep the original stack intact; these words apply only to a receipt's replay.
inline bool PlanAlphaEffectExclusion(const ProducerIdentity& owned,const ProducerIdentity& requested,
 const std::vector<EffectIdentityPoly>& parameters,const std::vector<AlphaEffectSelection>& selected,
 std::vector<AlphaEffectSelection>& output) {
 if(!owned.Available()||!requested.Available()||owned.epoch!=requested.epoch
  ||owned.ordinal!=requested.ordinal||owned.cycle!=requested.cycle||parameters.empty()
  ||parameters.size()>8192||selected.empty()||selected.size()>128)return false;
 std::vector<AlphaEffectSelection> result;std::set<std::uint32_t> used;
 for(const auto& s:selected) {
  if(s.ordinal>=parameters.size()||!used.insert(s.ordinal).second)return false;
  const auto& p=parameters[s.ordinal];
  if(p.primary!=s.expected.primary||p.secondary!=s.expected.secondary
   ||p.secondary!=0xffffffffu||((p.primary>>29)&7)!=4||((p.primary>>26)&7)!=5
   ||((p.primary>>24)&3)!=0)return false;
  // Source ZERO + destination ONE leaves native primary accumulation unchanged.
  auto replacement=s;replacement.expected.primary=(p.primary&0x03ffffffu)|(1u<<26);
  result.push_back(replacement);
 }
 output=std::move(result);return true;
}
}
