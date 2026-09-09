// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "hw/sh4/dyna/shil.h"
#include <vector>
namespace flycast::rend::neural {
// Direct same-block reaching definition only; no arithmetic or cross-block inference.
inline int DirectSourceRead(const std::vector<shil_opcode>& ops,std::size_t index) {
 if(index>=ops.size())return -1;
 const auto& store=ops[index];
 if(store.op!=shop_writem||store.size!=4||!store.rs2.is_r32())return -1;
 for(std::size_t j=index;j-->0;) {
  const auto& prior=ops[j];
  if(prior.op==shop_ifb)return -1;
  const auto overlaps=[&](const shil_param& p){return p.is_reg()&&store.rs2._reg>=p._reg&&store.rs2._reg<p._reg+p.count();};
  if(overlaps(prior.rd)||overlaps(prior.rd2))
   return j<256&&prior.op==shop_readm&&prior.size==4&&prior.rd.is_r32()&&prior.rd._reg==store.rs2._reg ? static_cast<int>(j) : -1;
 }
 return -1;
}
}
