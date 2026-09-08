// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace neuraltest {
// Harness-only range model. No pointer dereference, renderer hook or camera claim.
struct TaOrigin { std::uint64_t generation, transfer, sourceOffset; };
class TaProvenance {
 struct Range { std::uint64_t destination, size, transfer, source; };
 std::vector<Range> ranges;
 std::uint64_t generation = 0;
 bool valid = false;
 std::size_t limit;
public:
 explicit TaProvenance(std::size_t limit = 262144) : limit(limit) {}
 // Generation is an observer-owned monotonic token, never a recycled context
 // address or a frame ordinal. A failed Begin must not lower its high-water mark.
 void Begin(std::uint64_t epoch) {
  ranges.clear(); valid = epoch > generation;
  if (valid) generation = epoch;
 }
 bool Add(std::uint64_t epoch, std::uint64_t destination, std::uint64_t size,
          std::uint64_t transfer, std::uint64_t source) {
  constexpr auto max = UINT64_MAX;
  if (!valid || epoch != generation || !size || !transfer || ranges.size() >= limit
      || size > max - destination || size > max - source) { valid = false; return false; }
  for (const auto& r : ranges)
   if (destination < r.destination + r.size && r.destination < destination + size) {
    valid = false; return false;
   }
  ranges.push_back({destination,size,transfer,source}); return true;
 }
 std::optional<TaOrigin> Resolve(std::uint64_t epoch, std::uint64_t offset, std::uint64_t size) const {
  if (!valid || epoch != generation || !size || size > UINT64_MAX - offset) return {};
  for (const auto& r : ranges)
   if (offset >= r.destination && offset - r.destination < r.size
       && size <= r.size - (offset - r.destination))
    return TaOrigin{generation,r.transfer,r.source + offset - r.destination};
  // Split intervals need an explicit multi-part mapping; never guess continuity.
  return {};
 }
};
inline std::optional<TaOrigin> RemappedOrigin(const std::vector<std::optional<TaOrigin>>& decoded,
 const std::vector<std::uint32_t>& finalToDecoded, std::size_t finalVertex, std::uint64_t generation) {
 if (finalVertex >= finalToDecoded.size() || finalToDecoded[finalVertex] >= decoded.size()) return {};
 const auto& origin = decoded[finalToDecoded[finalVertex]];
 if (!generation || !origin || origin->generation != generation || !origin->transfer) return {};
 return origin;
}
}
