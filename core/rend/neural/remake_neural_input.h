// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
#include "remake_cpu_scope.h"
#include "remake_depth_validation.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace flycast::rend::neural {
// Bounded submission experiment only. No temporal correspondence is asserted.
// Public returned normal projection depth maps to inverted normalized depth;
// this does not reinterpret it as native logarithmic PVR depth.
struct RemakeNeuralInput {
 std::vector<unsigned char> rgba;
 std::vector<float> invertedDepth;
};
// Same shape and bound checks as the conversion below, without the conversion
// (the render thread accepts on this; the worker converts, D-213).
inline bool RemakeReturnedImageWellFormed(const RemakeReturnedImage& image)
{
 if(!SelectedRemakeExtent().Valid() || image.width!=RemakeWidth() || image.height!=RemakeHeight() || image.bgra.size()!=RemakePixels()*4
  || image.projectionDepth.size()!=RemakePixels() || !std::isfinite(image.nearPlane)
  || !std::isfinite(image.farPlane) || !(image.nearPlane>0 && image.farPlane>image.nearPlane)) return false;
 // A wholly zero color/depth readback is an absent-output diagnostic, not a
 // usable near-plane scene. Do not reject legitimately black opaque images.
 if(std::none_of(image.bgra.begin(),image.bgra.end(),[](unsigned char v){return v!=0;})
  &&std::all_of(image.projectionDepth.begin(),image.projectionDepth.end(),[](float v){return v==0;}))return false;
 if(image.projectionDepth.Validity()!=RemakeDepthValidity::Valid)return false;
 return true;
}
// Diagnostic only: why a returned image is not accepted as neural input.
inline std::string DescribeRemakeReturnedImage(const RemakeReturnedImage& image,
 std::uint64_t frame, const ProducerIdentity& producer)
{
 std::string out="frame="+std::to_string(image.frame)+"/"+std::to_string(frame)
  +" producer="+std::to_string(image.producer.epoch)+":"+std::to_string(image.producer.ordinal)+":"+std::to_string(image.producer.cycle)
  +"/"+std::to_string(producer.epoch)+":"+std::to_string(producer.ordinal)+":"+std::to_string(producer.cycle)
  +" extent="+std::to_string(image.width)+"x"+std::to_string(image.height)
  +"/"+std::to_string(RemakeWidth())+"x"+std::to_string(RemakeHeight())
  +" bgra="+std::to_string(image.bgra.size())+" depth="+std::to_string(image.projectionDepth.size())
  +" near="+std::to_string(image.nearPlane)+" far="+std::to_string(image.farPlane);
 const bool zeroColor=std::none_of(image.bgra.begin(),image.bgra.end(),[](unsigned char v){return v!=0;});
 const bool zeroDepth=std::all_of(image.projectionDepth.begin(),image.projectionDepth.end(),[](float v){return v==0;});
 std::size_t badDepth=0;for(float d:image.projectionDepth)badDepth+=!std::isfinite(d)||d<0||d>1;
 out+=" zero_color="+std::to_string(zeroColor)+" zero_depth="+std::to_string(zeroDepth)+" bad_depth="+std::to_string(badDepth);
 return out;
}
inline bool BuildRemakeNeuralInput(const RemakeReturnedImage& image,
 std::uint64_t frame, const ProducerIdentity& producer, RemakeNeuralInput& output)
{
 static thread_local unsigned validateCount=0,copyCount=0,convertCount=0;
 RemakeCpuScope validateTiming("input-validate",frame,validateCount);
 if(image.frame!=frame || image.producer.epoch!=producer.epoch
  || image.producer.ordinal!=producer.ordinal || image.producer.cycle!=producer.cycle
  || !RemakeReturnedImageWellFormed(image)) return false;
 validateTiming.End();
 RemakeCpuScope copyTiming("input-color-copy",frame,copyCount);
 RemakeNeuralInput candidate;
 candidate.rgba=image.bgra;
 copyTiming.End();
 RemakeCpuScope convertTiming("input-convert",frame,convertCount);
 candidate.invertedDepth.reserve(image.projectionDepth.size());
 for(float depth:image.projectionDepth)candidate.invertedDepth.push_back(1.f-depth);
 for(std::size_t i=0;i<candidate.rgba.size();i+=4)
  std::swap(candidate.rgba[i],candidate.rgba[i+2]);
 output=std::move(candidate);return true;
}
}

