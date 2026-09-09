// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "pvr_scene_capture.h"
#include <d3d11.h>
#include <array>
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
 const PvrCapturedDraw&,std::size_t&,std::vector<unsigned char>&,std::string&);
bool WritePvrMaterials(const std::filesystem::path& scene,ID3D11Device*,ID3D11DeviceContext*,
 const rend_context&,ID3D11Texture2D* palette,unsigned paletteFormat,unsigned filtering,unsigned anisotropy,
 std::uint64_t frame,const std::string& game,std::string& error,const MaterialShaderGlobals& globals);
}
