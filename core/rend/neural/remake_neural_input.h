// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
#include <algorithm>
#include <cmath>

namespace flycast::rend::neural {
// Bounded submission experiment only. No temporal correspondence is asserted.
// Public returned normal projection depth maps to inverted normalized depth;
// this does not reinterpret it as native logarithmic PVR depth.
struct RemakeNeuralInput {
 std::vector<unsigned char> rgba;
 std::vector<float> invertedDepth;
};
inline bool BuildRemakeNeuralInput(const RemakeReturnedImage& image,
 std::uint64_t frame, const ProducerIdentity& producer, RemakeNeuralInput& output)
{
 if(image.frame!=frame || image.producer.epoch!=producer.epoch
  || image.producer.ordinal!=producer.ordinal || image.producer.cycle!=producer.cycle
  || image.width!=640 || image.height!=480 || image.bgra.size()!=640*480*4
  || image.projectionDepth.size()!=640*480 || !std::isfinite(image.nearPlane)
  || !std::isfinite(image.farPlane) || !(image.nearPlane>0 && image.farPlane>image.nearPlane)) return false;
 RemakeNeuralInput candidate;
 candidate.rgba=image.bgra;
 candidate.invertedDepth.reserve(image.projectionDepth.size());
 for(float depth:image.projectionDepth) {
  if(!std::isfinite(depth)||depth<0||depth>1)return false;
  candidate.invertedDepth.push_back(1.f-depth);
 }
 for(std::size_t i=0;i<candidate.rgba.size();i+=4)
  std::swap(candidate.rgba[i],candidate.rgba[i+2]);
 output=std::move(candidate);return true;
}
}
