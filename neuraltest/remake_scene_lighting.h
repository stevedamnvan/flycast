// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include <cmath>
#include <optional>
#include <string_view>

namespace neuraltest::remake {
// Sequence-owned authored direction, independent of material resource lifetime.
class AnchoredSceneLight {
 std::optional<Vec3> direction_,origin_;
 std::uint64_t epoch_=0;
 std::string game_;
 unsigned reanchors_=0;
public:
 unsigned Reanchors()const{return reanchors_;}
 std::optional<Vec3> Select(const Packet& p,std::optional<Vec3> authoredDirection={}) {
  if(p.diagnosticEmbeddingProvenance!="diagnostic-camera-embedded-anchor-not-world-reconstruction"
   ||!p.producer.Available()||!p.diagnosticOrigin)return {};
  const auto o=*p.diagnosticOrigin,d=authoredDirection.value_or(p.camera.forward);
  if(!std::isfinite(o.x)||!std::isfinite(o.y)||!std::isfinite(o.z)
   ||!std::isfinite(d.x)||!std::isfinite(d.y)||!std::isfinite(d.z)
   ||std::abs(d.x*d.x+d.y*d.y+d.z*d.z-1)>1e-5f)return {};
  if(direction_) {
   if(p.producer.epoch!=epoch_||p.game!=game_)return {};
   if(o.x!=origin_->x||o.y!=origin_->y||o.z!=origin_->z) {
    // Explicit anchor generation change: fix the new first direction exactly as
    // a fresh session would. This is not a world-consistent light across a cut.
    direction_=d;origin_=o;++reanchors_;
   }
  } else {direction_=d;origin_=o;epoch_=p.producer.epoch;game_=p.game;}
  return direction_;
 }
};
// Authored pilot direction in the first accepted view basis, not recovered sun.
inline Vec3 TempleLightDirection(const Camera& camera,bool fill) {
 const float x=fill?-.65f:.45f,y=fill?-.15f:-.75f,z=1.f;
 Vec3 d{camera.right.x*x+camera.up.x*y+camera.forward.x*z,
        camera.right.y*x+camera.up.y*y+camera.forward.y*z,
        camera.right.z*x+camera.up.z*y+camera.forward.z*z};
 const float length=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
 if(!std::isfinite(length)||length<1e-6f)return {};
 return {d.x/length,d.y/length,d.z/length};
}
// Harness-authored bounds, not a recovered game light or NVIDIA intensity scale.
inline std::optional<float> ParseSceneLightRadiance(std::wstring_view text) {
 if(text.empty() || text.size()>12)return {};
 double value=0,place=.1;
 bool decimal=false,digit=false;
 for(auto c:text) {
  if(c==L'.'&&!decimal){decimal=true;continue;}
  if(c<L'0'||c>L'9')return {};
  digit=true;
  if(decimal){value+=(c-L'0')*place;place*=.1;}
  else value=value*10+(c-L'0');
  if(value>30)return {};
 }
 if(!digit||!std::isfinite(value))return {};
 return static_cast<float>(value);
}
}
