// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d11.h>

inline D3D11_BLEND_DESC NeuralCoverageBlendDescription()
{
 D3D11_BLEND_DESC desc{};desc.IndependentBlendEnable=TRUE;
 for(unsigned slot=0;slot<8;++slot) {
  auto& target=desc.RenderTarget[slot];const bool coverage=slot==1||slot==5;
  target.RenderTargetWriteMask=coverage?D3D11_COLOR_WRITE_ENABLE_RED:0;
  target.BlendEnable=coverage;
  target.SrcBlend=target.DestBlend=D3D11_BLEND_ONE;
  target.SrcBlendAlpha=target.DestBlendAlpha=D3D11_BLEND_ONE;
  target.BlendOp=target.BlendOpAlpha=D3D11_BLEND_OP_MAX;
 }
 return desc;
}
