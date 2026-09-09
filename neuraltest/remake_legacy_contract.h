// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
namespace neuraltest::remake {
// Resource compatibility only. Caller must validate packets and exact DDS bytes.
inline bool LegacyResourceCompatible(const Mesh& a,const Mesh& b) {
 return a.id==b.id && a.topology==b.topology && a.indices==b.indices &&
  a.vertices.size()==b.vertices.size() && a.transform==b.transform &&
  a.texture.known==b.texture.known && a.texture.id==b.texture.id &&
  a.texture.generation==b.texture.generation &&
  a.texture.paletteGeneration==b.texture.paletteGeneration &&
  a.texture.rttGeneration==b.texture.rttGeneration;
}
inline bool LegacySamplingSupported(const Mesh& m) {
 // Until the GPU cutout contract is proven, never render an alpha-tested mesh
 // as opaque just because its texture/sampling state is otherwise supported.
 return !m.sourceAlphaReference && m.sourceTsp && ((*m.sourceTsp>>13)&3)<=1 && ((*m.sourceTsp>>6)&3)==3;
}
}
