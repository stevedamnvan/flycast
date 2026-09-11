// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>
#if defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#endif
namespace neuraltest::remake {
// Returned projection depth outside the declared clip range.
//
// The helper's D3D9 projection maps view depth z to f(z-n)/((f-n)z): exactly 1
// at z=f, exactly 0 at z=n, values in (1, f/(f-n)] for points beyond the far
// plane, negative values for points between the camera and the near plane
// (0<z<n) and values above f/(f-n) only for z<0. The ray-traced runtime clips at
// neither plane. The [0,1] return contract expresses "at or beyond the far
// plane" as 1 and "at or before the near plane" as 0, so such values are
// returned as the plane they lie beyond and counted. Values above the limit
// are not the projection of any point in front of the camera; they are left as
// measured so the host still rejects the return. Depth inside the clip range is
// never altered and raw depth artifacts stay raw. This is the projection's own
// semantic made explicit (D-209), not a relaxed acceptance. The report carries
// the measured extremes so the actual excess stays visible in the helper log.
struct RemakeFarPlaneReport { std::size_t beyondFar=0,beforeNear=0,aboveLimit=0; float maxDepth=0,minDepth=0,limit=0; };
// Extract the R component and apply the same clip policy in one traversal.
// Pitch includes row padding; texelBytes is 4 for R32F or 16 for RGBA32F.
inline RemakeFarPlaneReport RemakeExtractClampedDepth(const void* source,std::size_t pitch,
 std::size_t texelBytes,std::size_t width,std::size_t height,float* output,
 float nearPlane,float farPlane) noexcept {
 RemakeFarPlaneReport r{};
 const bool clip=nearPlane>0&&farPlane>nearPlane&&std::isfinite(farPlane);
 if(clip)r.limit=farPlane/(farPlane-nearPlane);
 for(std::size_t y=0;y<height;++y) {
  const auto* row=static_cast<const unsigned char*>(source)+y*pitch;
  const auto scalar=[&](std::size_t x) {
   float v;std::memcpy(&v,row+x*texelBytes,sizeof(v));
   if(clip&&std::isfinite(v)) {
    r.maxDepth=(std::max)(r.maxDepth,v);r.minDepth=(std::min)(r.minDepth,v);
    if(v>1){if(v<=r.limit){v=1;++r.beyondFar;}else ++r.aboveLimit;}
    else if(v<0){v=0;++r.beforeNear;}
   }
   output[y*width+x]=v;
  };
  std::size_t x=0;
#if defined(_M_X64) || defined(__SSE2__)
  if(clip&&texelBytes==sizeof(float)) {
   const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f);
   auto maximum=zero;
   for(;width-x>=4;x+=4) {
    const auto v=_mm_loadu_ps(reinterpret_cast<const float*>(row+x*sizeof(float)));
    // Ordered comparisons exclude every NaN/infinity. The common [0,1]
    // path needs no clipping or counters; copy original bits, including -0.
    const auto valid=_mm_and_ps(_mm_cmpge_ps(v,zero),_mm_cmple_ps(v,one));
    if(_mm_movemask_ps(valid)==15) {
     _mm_storeu_ps(output+y*width+x,v);maximum=_mm_max_ps(maximum,v);
    } else for(std::size_t lane=0;lane<4;++lane)scalar(x+lane);
   }
   float lanes[4];_mm_storeu_ps(lanes,maximum);
   for(float v:lanes)r.maxDepth=(std::max)(r.maxDepth,v);
  }
#endif
  for(;x<width;++x)scalar(x);
 }
 return r;
}
template<class DepthBuffer>
inline RemakeFarPlaneReport RemakeClampBeyondFarPlane(DepthBuffer& depth,float nearPlane,float farPlane) noexcept {
 RemakeFarPlaneReport r{};
 if(!(nearPlane>0)||!(farPlane>nearPlane)||!std::isfinite(farPlane))return r;
 r.limit=farPlane/(farPlane-nearPlane);
 for(float& v:depth) {
  if(!std::isfinite(v))continue;
  r.maxDepth=(std::max)(r.maxDepth,v);r.minDepth=(std::min)(r.minDepth,v);
  if(v>1){if(v<=r.limit){v=1;++r.beyondFar;}else ++r.aboveLimit;}
  else if(v<0){v=0;++r.beforeNear;}
 }
 return r;
}
}
