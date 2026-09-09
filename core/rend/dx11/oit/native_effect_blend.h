// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Shared by the native OIT resolver and source-owned effect replay fixtures.
// Keep native encoded-color arithmetic and per-fragment saturation. A flattened
// alpha layer cannot represent destination-alpha or secondary-buffer operations.
inline constexpr char NativeEffectBlendHlsl[] = R"(
float4 nativeEffectCoefficient(int mode, float4 other, float4 src, float4 dst)
{
 switch(mode) {
  case 0: return 0.f;
  case 1: return 1.f;
  case 2: return other;
  case 3: return 1.f - other;
  case 4: return src.a;
  case 5: return 1.f - src.a;
  case 6: return dst.a;
  case 7: return 1.f - dst.a;
 }
 return 0.f; // Packed PVR state only supplies 0..7.
}
float4 nativeEffectBlend(float4 src, float4 dst, int srcMode, int dstMode)
{
 return clamp(dst * nativeEffectCoefficient(dstMode, src, src, dst)
   + src * nativeEffectCoefficient(srcMode, dst, src, dst), 0.f, 1.f);
}
)";
