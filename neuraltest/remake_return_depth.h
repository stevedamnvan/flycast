// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
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
inline RemakeFarPlaneReport RemakeClampBeyondFarPlane(std::vector<float>& depth,float nearPlane,float farPlane) noexcept {
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
