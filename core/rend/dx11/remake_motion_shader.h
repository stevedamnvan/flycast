// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Returned-scene guidance, not native PVR guidance. The caller supplies depth
// and draw identity belonging to the last accepted evaluation. Keep disabled
// until raster/depth negative controls and the live integration are validated.
inline constexpr char RemakeMotionVertexShader[] = R"(
cbuffer Contract : register(b0) {
 float2 extent; float nearPlane; float farPlane;
 float absoluteDepthTolerance; float relativeDepthTolerance; float2 padding;
};
struct Input {
 float3 current : POSITION0;
 float3 previous : POSITION1;
 float confidence : TEXCOORD0;
 uint currentDraw : TEXCOORD1;
 uint previousDraw : TEXCOORD2;
};
struct Raster {
 float4 position : SV_Position;
 float4 previousClip : TEXCOORD0;
 nointerpolation float confidence : TEXCOORD1;
 nointerpolation uint currentDraw : TEXCOORD2;
 nointerpolation uint previousDraw : TEXCOORD3;
};
float4 clipPosition(float3 p) {
 float2 ndc = p.xy / extent * float2(2,-2) + float2(-1,1);
 return float4(ndc*p.z, farPlane/(farPlane-nearPlane)*p.z
  - nearPlane*farPlane/(farPlane-nearPlane), p.z);
}
Raster main(Input v) {
 Raster o;
 o.position=clipPosition(v.current);
 // Perspective interpolation of homogeneous previous positions, followed by
 // division in the pixel shader, preserves deforming triangle correspondence.
 // Interpolating already-divided screen vectors would not do so.
 o.previousClip=clipPosition(v.previous);
 o.confidence=v.confidence;
 o.currentDraw=v.currentDraw;
 o.previousDraw=v.previousDraw;
 return o;
}
)";

inline constexpr char RemakeMotionPixelShader[] = R"(
cbuffer Contract : register(b0) {
 float2 extent; float nearPlane; float farPlane;
 float absoluteDepthTolerance; float relativeDepthTolerance; float2 padding;
};
Texture2D<float> currentDepth : register(t0);
Texture2D<float> previousDepth : register(t1);
Texture2D<uint> previousDrawId : register(t2);
struct Raster {
 float4 position : SV_Position;
 float4 previousClip : TEXCOORD0;
 nointerpolation float confidence : TEXCOORD1;
 nointerpolation uint currentDraw : TEXCOORD2;
 nointerpolation uint previousDraw : TEXCOORD3;
};
struct Guidance {
 float2 motion : SV_Target0;
 float confidence : SV_Target1;
 uint drawId : SV_Target2;
 float bias : SV_Target3;
 uint reason : SV_Target4; // 0 trusted, 1 current depth, 2 correspondence,
 // 3 prior clip, 4 outside/magnitude, 5 prior identity, 6 prior depth, 7 uncovered.
 float rasterDepth : SV_Target5;
};
float viewDepth(float d) {
 return nearPlane*farPlane/(farPlane-d*(farPlane-nearPlane));
}
bool agrees(float a,float b) {
 if(!isfinite(a)||!isfinite(b)||a<0||b<0||a>=1||b>=1)return false;
 float za=viewDepth(a),zb=viewDepth(b);
 return abs(za-zb)<=absoluteDepthTolerance+relativeDepthTolerance*min(za,zb);
}
bool inPlaneFootprint(float expected,float observed,float2 slope) {
 if(!isfinite(expected)||!isfinite(observed)||!all(isfinite(slope))
  ||expected<0||expected>=1||observed<0||observed>=1)return false;
 // One render-pixel sampling footprint on this triangle's projection-depth
 // plane, not a global view-depth tolerance or an assumed runtime jitter.
 float radius=abs(slope.x)+abs(slope.y);
 float nearest=clamp(observed,max(0,expected-radius),min(0.99999994,expected+radius));
 return agrees(nearest,observed);
}
bool currentSurface(float depth,float2 slope,int2 pixel) {
 if(any(pixel<1)||any(pixel>=int2(extent)-1))return false;
 // Every neighboring sample must also fit the same plane. A center-only
 // expanded interval would admit thin surfaces and occlusion boundaries.
 [unroll] for(int y=-1;y<=1;++y) [unroll] for(int x=-1;x<=1;++x) {
  float expected=depth+dot(slope,float2(x,y));
  if(!inPlaneFootprint(expected,currentDepth.Load(int3(pixel+int2(x,y),0)),slope))return false;
 }
 return true;
}
Guidance main(Raster v) {
 // Derivatives must be evaluated before divergent rejection branches. Convert
 // the previous depth plane from current raster coordinates to previous pixels.
 float3 previousNdc=v.previousClip.xyz/v.previousClip.w;
 float2 previousPixel=(previousNdc.xy*float2(0.5,-0.5)+0.5)*extent;
 float2 px=ddx(previousPixel),py=ddy(previousPixel);
 float zx=ddx(previousNdc.z),zy=ddy(previousNdc.z);
 float determinant=px.x*py.y-px.y*py.x;
 float2 previousSlope=float2(zx*py.y-zy*px.y,px.x*zy-py.x*zx)/determinant;
 Guidance o;
 o.motion=0; o.confidence=0; o.drawId=0; o.bias=1;
 o.reason=1; o.rasterDepth=v.position.z;
 // Do not give returned reflections, missing geometry or a wrong visible
 // surface the identity of the replayed triangle.
 float2 slope=float2(ddx(v.position.z),ddy(v.position.z));
 if(!currentSurface(v.position.z,slope,int2(v.position.xy)))return o;
 o.drawId=v.currentDraw;
 o.reason=2;
 if(v.confidence<0.5||v.previousDraw==0)return o;
 o.reason=3;
 if(v.previousClip.w<=0)return o;
 float2 motion=previousPixel-v.position.xy;
 // Homogeneous divide can leave sub-ULP-at-640px noise on static geometry.
 // Suppress less than 1/4096 pixel, independently per component.
 motion=float2(abs(motion.x)<1.0/4096?0:motion.x,abs(motion.y)<1.0/4096?0:motion.y);
 o.reason=4;
 if(!all(isfinite(previousPixel))||any(previousPixel<0)||any(previousPixel>=extent)
  ||length(motion)>128)return o;
 int3 sample=int3(int2(floor(previousPixel)),0);
 o.reason=6;
 if(!all(isfinite(previousSlope))||!isfinite(determinant)||abs(determinant)<1e-6
  ||any(sample.xy<1)||any(sample.xy>=int2(extent)-1))return o;
 bool identityValid=true,depthValid=true;
 [unroll] for(int y=-1;y<=1;++y) [unroll] for(int x=-1;x<=1;++x) {
  int3 neighbor=sample+int3(x,y,0);
  identityValid=identityValid&&(previousDrawId.Load(neighbor)==v.previousDraw);
  float expected=previousNdc.z+dot(previousSlope,float2(neighbor.xy)+0.5-previousPixel);
  depthValid=depthValid&&inPlaneFootprint(expected,previousDepth.Load(neighbor),previousSlope);
 }
 if(!identityValid){o.reason=5;return o;}
 if(!depthValid)return o;
 o.reason=0;
 o.motion=motion; o.confidence=v.confidence; o.bias=0;
 return o;
}
)";
