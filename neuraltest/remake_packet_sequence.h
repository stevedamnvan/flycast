// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <istream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace neuraltest::remake {
// Explicit offline diagnostic input. No discovery, sorting or missing-frame fill.
inline std::vector<std::filesystem::path> ParsePacketSequencePaths(std::istream& input) {
 std::vector<std::filesystem::path> paths;std::set<std::filesystem::path> seen;
 std::string line;std::size_t bytes=0;
 while(std::getline(input,line)) {
  bytes+=line.size()+1;
  if(bytes>1024*1024||line.size()>2048)throw std::invalid_argument("packet sequence list size");
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  const auto path=std::filesystem::u8path(line).lexically_normal();
  if(line.empty()||line.find('\0')!=std::string::npos||!path.is_absolute()||!seen.insert(path).second)
   throw std::invalid_argument("packet sequence absolute unique paths required");
  paths.push_back(path);
  if(paths.size()>300)throw std::invalid_argument("packet sequence maximum300");
 }
 if(input.bad()||paths.size()<3)throw std::invalid_argument("packet sequence requires3..300 paths");
 return paths;
}
inline bool PacketSequenceFrames(long frames,std::size_t count) {
 return count>=3&&count<=300&&frames==long(60+count);
}
}
