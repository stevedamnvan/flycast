// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
namespace neuraltest::remake {
// Returned projection depth at or beyond the declared far plane.
//
// The helper's D3D9 projection maps view depth z to f(z-n)/((f-n)z): exactly 1
// at z=f and, for points beyond the far plane (which the ray-traced runtime does
// not clip), values up to the limit f/(f-n). Every value in (1, f/(f-n)] is the
// projection of a point at or beyond the declared far plane, which the [0,1]
// return contract expresses as 1. Values above the limit are not the projection
// of any point in front of the camera; they are left as measured so the host
// still rejects the return. Depth in front of the far plane is never altered
// and raw depth artifacts stay raw. This is the projection's own semantic made
// explicit (D-209), not a relaxed acceptance. The report carries the measured
// maximum so the actual excess stays visible in the helper log.
struct RemakeFarPlaneReport { std::size_t beyondFar=0,aboveLimit=0; float maxDepth=0,limit=0; };
inline RemakeFarPlaneReport RemakeClampBeyondFarPlane(std::vector<float>& depth,float nearPlane,float farPlane) noexcept {
 RemakeFarPlaneReport r{};
 if(!(nearPlane>0)||!(farPlane>nearPlane)||!std::isfinite(farPlane))return r;
 r.limit=farPlane/(farPlane-nearPlane);
 for(float& v:depth) {
  if(!std::isfinite(v))continue;
  r.maxDepth=(std::max)(r.maxDepth,v);
  if(v>1){if(v<=r.limit){v=1;++r.beyondFar;}else ++r.aboveLimit;}
 }
 return r;
}
}
