// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <new>
namespace flycast::rend::neural {
struct SourceTransform {
 std::uint64_t serial=0;
 std::uint32_t pc=0;
 std::array<float,4> input{},output{};
 std::array<float,16> matrix{};
};
// Executed observations only. Ring entries do not assert camera semantics or
// correspondence with a RAM write; consumers must prove that separately.
inline thread_local std::unique_ptr<std::array<SourceTransform,4096>> sourceTransforms;
inline thread_local std::uint64_t sourceTransformSerial=0;
inline void RetainSourceTransform(SourceTransform observation) noexcept {
 if(sourceTransformSerial==UINT64_MAX)return;
 if(!sourceTransforms)sourceTransforms.reset(new(std::nothrow) std::array<SourceTransform,4096>{});
 if(!sourceTransforms)return;
 observation.serial=++sourceTransformSerial;
 (*sourceTransforms)[observation.serial%sourceTransforms->size()]=observation;
}
}
