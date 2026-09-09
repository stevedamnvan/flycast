// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "pvr_scene_capture.h"
#include <d3d11.h>
#include <array>
#include <memory>
namespace flycast::rend::neural {
struct MaterialShaderGlobals {
 bool valid=false,fogEnabled=false;
 bool sourceBytesUnchanged=false;
 std::array<float,3> fogVertex{},fogRam{};
 std::array<float,4> clampMin{},clampMax{};
 float fogDensity=0,alphaReference=0,shadowScale=0;
};
struct MaterialMip { unsigned width=0,height=0; std::vector<std::uint8_t> bytes; };
struct MaterialPixels { DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN; std::vector<MaterialMip> mips; };
enum class MaterialReadbackResult { Pending, Ready, Invalid };
// One owned, bounded GPU copy. No waits/flushes in Begin or Poll. The caller
// supplies authoritative resource/generation identity; this is not a cache.
class MaterialReadback {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 MaterialReadback();~MaterialReadback();
 MaterialReadback(const MaterialReadback&)=delete;
 MaterialReadback& operator=(const MaterialReadback&)=delete;
 bool Begin(ID3D11Device*,ID3D11DeviceContext*,ID3D11Texture2D*,
  const PvrCapturedTexture&,std::size_t& remainingBytes,std::string&);
 MaterialReadbackResult Poll(ID3D11DeviceContext*,ID3D11Texture2D*,
  const PvrCapturedTexture&,MaterialPixels&,std::string&);
 void Reset();
 bool Pending() const noexcept;
};
// Bounded generation-qualified texture staging/cache for the live scene feed.
// BeginFrame expires old entries and resets on epoch/frame discontinuity.
class RemakeTextureCache {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 RemakeTextureCache();~RemakeTextureCache();
 void BeginFrame(std::uint64_t frame,std::uint64_t epoch);
 void Reset();
 bool Request(ID3D11Device*,ID3D11DeviceContext*,ID3D11Texture2D*,
  const PvrCapturedTexture&,std::vector<unsigned char>&,std::string&);
 std::size_t Entries()const noexcept;
};
std::string MaterialContentHash(const std::vector<std::uint8_t>&);
bool ReadMaterialPixels(ID3D11Device*,ID3D11DeviceContext*,ID3D11Texture2D*,
 std::size_t& remainingBytes,MaterialPixels&,std::string& error);
bool DecodeMaterialTexel(DXGI_FORMAT,const std::uint8_t*,std::size_t,
 std::array<std::uint8_t,4>&,const MaterialMip* palette=nullptr,unsigned paletteBase=0);
bool MaterialGenerationMatches(const PvrCapturedTexture&,unsigned upload,unsigned rtt,unsigned palette);
bool EncodeRemakeMaterialDds(const MaterialPixels&,std::vector<unsigned char>&,std::string&);
// Synchronous developer boundary only; verifies the live primary binding before
// and after readback. Unsupported formats remain failure/native fallback.
bool ReadRemakeViewTexture(ID3D11Device*,ID3D11DeviceContext*,const rend_context&,
 const PvrCapturedDraw&,std::size_t&,std::vector<unsigned char>&,std::string&,RemakeTextureCache* asyncCache=nullptr);
bool WritePvrMaterials(const std::filesystem::path& scene,ID3D11Device*,ID3D11DeviceContext*,
 const rend_context&,ID3D11Texture2D* palette,unsigned paletteFormat,unsigned filtering,unsigned anisotropy,
 std::uint64_t frame,const std::string& game,std::string& error,const MaterialShaderGlobals& globals);
}
