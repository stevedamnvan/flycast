// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
#include <optional>
#include <string_view>

namespace neuraltest::remake {
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
