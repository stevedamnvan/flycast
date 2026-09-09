// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_input_replay.h"
#include "remake_view_transport.h"
#include "remake_neural_input.h"
#include "json/json.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <locale>

namespace flycast::rend::neural {
bool SameRemakeReplayScene(const remake::Packet& retained,const remake::Packet& current,std::string& error)
{
 // Renderer counters and build labels may differ. Never normalize game clock,
 // geometry, textures, projection, omissions or other serialized scene content.
 auto normalized=retained;normalized.frame=current.frame;normalized.sourceGitSha=current.sourceGitSha;
 for(auto& mesh:normalized.meshes)mesh.frame=current.frame;
 std::ostringstream a(std::ios::binary),b(std::ios::binary);
 if(!SerializeRemakeViewPacket(a,normalized,error)||!SerializeRemakeViewPacket(b,current,error))return false;
 const auto left=a.str(),right=b.str();
 if(left!=right){std::size_t offset=0;while(offset<left.size()&&offset<right.size()&&left[offset]==right[offset])++offset;
  error="locked-replay-scene-mismatch offset="+std::to_string(offset)+" retained_bytes="+std::to_string(left.size())+" current_bytes="+std::to_string(right.size());return false;}
 error.clear();return true;
}
bool ReadLockedRemakeInput(const std::filesystem::path& root,const remake::Packet& current,
 RemakeReturnedImage& output,std::uint64_t& originalFrame,std::string& error)
{
 try {
  std::filesystem::path match;remake::Packet retained;unsigned entries=0;
  for(const auto& entry:std::filesystem::directory_iterator(root)) {
   if(++entries>512)throw std::runtime_error("locked-replay-directory-bound");
   if(!entry.is_directory()||entry.path().filename().string().rfind("frame-",0)!=0)continue;
   remake::Packet candidate;
   if(!ReadRemakeViewPacket(entry.path()/"remake-view.bin",candidate,error))return false;
   if(candidate.producer.epoch!=current.producer.epoch||candidate.producer.ordinal!=current.producer.ordinal
    ||candidate.producer.cycle!=current.producer.cycle)continue;
   if(!SameRemakeReplayScene(candidate,current,error))return false;
   if(!match.empty())throw std::runtime_error("locked-replay-ambiguous-source");
   match=entry.path();retained=std::move(candidate);
  }
  if(match.empty())throw std::runtime_error("locked-replay-producer-not-found");
  auto read=[&](const std::filesystem::path& path,void* data,std::size_t size) {
   if(std::filesystem::file_size(path)!=size)throw std::runtime_error("locked-replay-file-extent");
   std::ifstream stream(path,std::ios::binary);
   if(!stream.read(static_cast<char*>(data),size))throw std::runtime_error("locked-replay-read");
  };
  const auto receiptPath=match/"remake-return.json";
  if(std::filesystem::file_size(receiptPath)>16384)throw std::runtime_error("locked-replay-receipt-bound");
  std::ifstream receiptStream(receiptPath);nlohmann::json receipt;receiptStream>>receipt;
  std::ostringstream wire(std::ios::binary);if(!SerializeRemakeViewPacket(wire,retained,error))return false;
  std::uint64_t hash=14695981039346656037ull;
  for(unsigned char c:wire.str()){hash^=c;hash*=1099511628211ull;}
  if(receipt.at("frame").get<std::uint64_t>()!=retained.frame
   ||receipt.at("source_digest").get<std::uint64_t>()!=hash
   ||receipt.at("depth_values").get<unsigned>()!=640*480
   ||receipt.at("pixel_bytes").get<unsigned>()!=640*480*4)
    throw std::runtime_error("locked-replay-receipt-mismatch");
  RemakeReturnedImage image;image.frame=current.frame;image.producer=current.producer;
  image.width=640;image.height=480;image.nearPlane=retained.camera.nearPlane;image.farPlane=retained.camera.farPlane;
  image.source={receipt.at("sequence").get<std::uint64_t>(),hash,static_cast<std::uint32_t>(wire.str().size())};
  image.bgra.resize(640*480*4);image.projectionDepth.resize(640*480);
  read(match/"remake-return.bgra",image.bgra.data(),image.bgra.size());
  read(match/"remake-return-depth.f32",image.projectionDepth.data(),image.projectionDepth.size()*sizeof(float));
  RemakeNeuralInput validated;
  if(!BuildRemakeNeuralInput(image,current.frame,current.producer,validated))throw std::runtime_error("locked-replay-invalid-input");
  const auto manifestPath=match/"manifest.json";
  if(std::filesystem::file_size(manifestPath)>131072)throw std::runtime_error("locked-replay-manifest-bound");
  std::ifstream manifestStream(manifestPath);nlohmann::json manifest;manifestStream>>manifest;
  auto hexDigest=[](const void* bytes,std::size_t count) {
   auto p=static_cast<const unsigned char*>(bytes);std::uint64_t h=14695981039346656037ull;
   for(std::size_t i=0;i<count;++i){h^=p[i];h*=1099511628211ull;}
   std::ostringstream out;out.imbue(std::locale::classic());out<<std::uppercase<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return out.str();
  };
  const auto& hashes=manifest.at("contract_hashes");const auto& producer=manifest.at("producer_identity");
  if(manifest.at("frame_id").get<std::uint64_t>()!=retained.frame
   ||manifest.at("git_sha").get<std::string>()!=retained.sourceGitSha
   ||manifest.at("remake_input")!="returned-scene-reset-only-inverted-projection-experiment"
   ||producer.at("epoch").get<std::uint64_t>()!=retained.producer.epoch
   ||producer.at("ordinal").get<std::uint64_t>()!=retained.producer.ordinal
   ||producer.at("cycle").get<std::uint64_t>()!=retained.producer.cycle
   ||hashes.at("color_fnv64")!=hexDigest(validated.rgba.data(),validated.rgba.size())
   ||hashes.at("depth_fnv64")!=hexDigest(validated.invertedDepth.data(),validated.invertedDepth.size()*sizeof(float)))
    throw std::runtime_error("locked-replay-source-input-hash-mismatch");
  output=std::move(image);originalFrame=retained.frame;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
}
