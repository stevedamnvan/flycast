// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/TexCache.h"
#include "rend/texconv.h"
namespace flycast::rend::neural {
// GPU palette indices are shared across draw banks; CPU-expanded textures are not.
inline unsigned PvrDrawPaletteGeneration(const BaseTextureCacheData& texture,TCW draw) {
 if(texture.gpuPalette) {
  if(draw.PixelFmt==PixelPal4)return pal_hash_16[draw.PalSelect];
  if(draw.PixelFmt==PixelPal8)return pal_hash_256[draw.PalSelect>>4];
 }
 return texture.palette_hash;
}
inline bool PvrDrawTextureWordMatches(TCW resource,TCW draw,bool gpuPalette) {
 if(gpuPalette) {
  if((draw.PixelFmt!=PixelPal4&&draw.PixelFmt!=PixelPal8)||resource.PixelFmt!=draw.PixelFmt)return false;
  resource.PalSelect=0;draw.PalSelect=0;
 }
 return resource.full==draw.full;
}
}
