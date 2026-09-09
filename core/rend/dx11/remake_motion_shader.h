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
};
float viewDepth(float d) {
 return nearPlane*farPlane/(farPlane-d*(farPlane-nearPlane));
}
bool agrees(float a,float b) {
 if(!isfinite(a)||!isfinite(b)||a<0||b<0||a>=1||b>=1)return false;
 float za=viewDepth(a),zb=viewDepth(b);
 return abs(za-zb)<=absoluteDepthTolerance+relativeDepthTolerance*min(za,zb);
}
Guidance main(Raster v) {
 Guidance o;
 o.motion=0; o.confidence=0; o.drawId=0; o.bias=1;
 // Do not give returned reflections, missing geometry or a wrong visible
 // surface the identity of the replayed triangle.
 if(!agrees(v.position.z,currentDepth.Load(int3(int2(v.position.xy),0))))return o;
 o.drawId=v.currentDraw;
 if(v.confidence<0.5||v.previousDraw==0||v.previousClip.w<=0)return o;
 float3 previousNdc=v.previousClip.xyz/v.previousClip.w;
 float2 previousPixel=(previousNdc.xy*float2(0.5,-0.5)+0.5)*extent;
 float2 motion=previousPixel-v.position.xy;
 // Homogeneous divide can leave sub-ULP-at-640px noise on static geometry.
 // Suppress less than 1/4096 pixel, independently per component.
 motion=float2(abs(motion.x)<1.0/4096?0:motion.x,abs(motion.y)<1.0/4096?0:motion.y);
 if(!all(isfinite(previousPixel))||any(previousPixel<0)||any(previousPixel>=extent)
  ||length(motion)>128)return o;
 int3 sample=int3(int2(floor(previousPixel)),0);
 if(previousDrawId.Load(sample)!=v.previousDraw
  ||!agrees(previousNdc.z,previousDepth.Load(sample)))return o;
 o.motion=motion; o.confidence=v.confidence; o.bias=0;
 return o;
}
)";
