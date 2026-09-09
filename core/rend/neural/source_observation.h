// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "producer_identity.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>

namespace flycast::rend::neural {
// Observed SQ-to-TA copy only. Does not assert a camera or upstream transform.
struct SourceCopyObservation {
 // SQ invocation serial, not an upstream RAM content generation.
 std::uint64_t generation=0, cycle=0;
 std::uint32_t taOffset=0, sourceAddress=0, writerPc=0;
 std::array<std::uint32_t,8> before{}, after{};
 std::uint32_t decodedVertex = UINT32_MAX;
};
struct SourceVertexObservation {
 std::uint32_t child = 0;
 SourceCopyObservation copy;
};
// One producer-owned frame. Overflow/incomplete publication discards authority,
// not emulated data. Consumers receive an owned copy, never live RAM pointers.
class SourceObservationBatch {
public:
 static constexpr std::size_t capacity=16384;
 std::size_t Size() const noexcept { return records_.size(); }
 bool JoinVertex(ProducerIdentity identity, std::uint32_t offset,
                 const void* words, std::uint32_t vertex) noexcept {
  if (!Get(identity) || !words || vertex == UINT32_MAX) return false;
  auto it = std::lower_bound(records_.begin(), records_.end(), offset,
   [](const SourceCopyObservation& record, std::uint32_t value) { return record.taOffset < value; });
  if (it == records_.end() || it->taOffset != offset ||
      it->decodedVertex != UINT32_MAX || std::memcmp(it->after.data(), words, 32)) return false;
  it->decodedVertex = vertex;
  return true;
 }
 // Collection precedes queue submission, when the producer stamp is assigned.
 void BeginContext(std::uint64_t contextGeneration) {
  records_.clear();identity_={};contextGeneration_=contextGeneration;
  failed_=contextGeneration==0;sealed_=false;
 }
 void Begin(ProducerIdentity identity) {
  BeginContext(1);identity_=identity;failed_=!identity.Available();
 }
 bool Append(const SourceCopyObservation& record) {
  if(failed_||sealed_)return false;
  if(!contextGeneration_||!record.generation||!record.writerPc||
     (record.taOffset&31)||records_.size()==capacity||
     (!records_.empty()&&(record.cycle<records_.back().cycle||record.taOffset<=records_.back().taOffset))||
     record.before!=record.after) {failed_=true;return false;}
  records_.push_back(record);return true;
 }
 bool Seal(std::size_t expectedCopies) {
  return Seal(identity_,expectedCopies);
 }
 bool Seal(ProducerIdentity identity,std::size_t expectedCopies) {
  if(failed_||sealed_||!identity.Available()||expectedCopies==0||expectedCopies!=records_.size()
     ||records_.back().cycle>identity.cycle) {
   failed_=true;return false;
  }
  identity_=identity;sealed_=true;return true;
 }
 const std::vector<SourceCopyObservation>* Get(ProducerIdentity identity) const noexcept {
  return !failed_&&sealed_&&identity.Available()&&identity.epoch==identity_.epoch&&
   identity.ordinal==identity_.ordinal&&identity.cycle==identity_.cycle ? &records_ : nullptr;
 }
private:
 ProducerIdentity identity_{};
 std::uint64_t contextGeneration_=0;
 std::vector<SourceCopyObservation> records_;
 bool failed_=true,sealed_=false;
};
}
