// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_legacy_reuse.h"
#include "remake_legacy_contract.h"

template<class Suite>
void TestRemakeLegacyReuse(Suite& suite) {
 using namespace neuraltest::remake;
 using Mapping=std::vector<std::size_t>;
 const auto map=[](std::vector<int> incoming,std::vector<int> previous) {
  return LegacyReuseMapping(incoming.size(),previous.size(),
   [&](std::size_t i,std::size_t j){return incoming[i]==previous[j];});
 };
 suite.Expect(map({1,9,2,3},{1,2,3})==Mapping{0,3,1,2},"legacy reuse middle insertion retains following draws");
 suite.Expect(map({1,3},{1,2,3})==Mapping{0,2},"legacy reuse middle removal retains following draws");
 suite.Expect(map({3,1,2},{1,2,3})==Mapping{2,0,1},"legacy reuse preserves incoming order across reorder");
 suite.Expect(map({7,7,7},{7,7})==Mapping{0,1,2},"legacy reuse never aliases duplicate allocations");
 suite.Expect(map({8,9},{1,2,3})==Mapping{3,3},"legacy reuse unmatched uses previous count sentinel");
 suite.Expect(map({1,2},{})==Mapping{0,0}&&map({},{1,2}).empty(),"legacy reuse handles empty source and destination");
 Mesh base;base.id=4;base.vertices.resize(3);base.indices={0,1,2};
 base.texture.known=true;base.texture.id=8;
 const auto matched=[&](const Mesh& changed) {
  return LegacyReuseMapping(1,1,[&](std::size_t,std::size_t){return LegacyResourceCompatible(changed,base);})==Mapping{0};
 };
 auto changed=base;changed.vertices[0].position.x=3;changed.vertices[0].normal=Vec3{0,1,0};
 changed.vertices[0].u=.5f;changed.vertices[0].publicColor=0;
 suite.Expect(matched(changed),"legacy reuse accepts dynamic vertex attributes for fresh upload");
 changed=base;changed.id++;suite.Expect(!matched(changed),"legacy reuse rejects different mesh identity");
 changed=base;changed.indices={2,1,0};suite.Expect(!matched(changed),"legacy reuse rejects changed index order");
 changed=base;changed.vertices.resize(4);suite.Expect(!matched(changed),"legacy reuse rejects changed vertex extent");
 changed=base;changed.topology=Topology::Strip;suite.Expect(!matched(changed),"legacy reuse rejects topology change");
 changed=base;changed.transform=std::array<float,12>{};suite.Expect(!matched(changed),"legacy reuse rejects transform change");
 changed=base;changed.sourceAlphaBlend=true;suite.Expect(!matched(changed),"legacy reuse rejects blend route change");
 changed=base;changed.texture.generation++;suite.Expect(!matched(changed),"legacy reuse rejects texture generation change");
 changed=base;changed.texture.paletteGeneration++;suite.Expect(!matched(changed),"legacy reuse rejects palette generation change");
 changed=base;changed.texture.rttGeneration++;suite.Expect(!matched(changed),"legacy reuse rejects RTT generation change");
 // The allocation planner must honor the caller's exact-byte result, even
 // when metadata agrees, and continue searching a different old allocation.
 const std::vector<unsigned char> incomingBytes{1,2,3,4};
 const std::vector<std::vector<unsigned char>> oldBytes{{1,2,3,5},{1,2,3,4}};
 suite.Expect(LegacyReuseMapping(1,2,[&](std::size_t,std::size_t j){
  return LegacyResourceCompatible(base,base)&&incomingBytes==oldBytes[j];
 })==Mapping{1},"legacy reuse respects additional exact byte predicate");
}
