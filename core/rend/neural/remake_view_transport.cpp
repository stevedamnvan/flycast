// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_view_transport.h"
#include <fstream>
#include <stdexcept>
namespace flycast::rend::neural {
bool BuildRemakeViewPacket(const RemakeViewScene& scene,const RemakeTextureReader& reader,
 remake::Packet& output,std::string& error) {
 const auto fail=[&](const char* why){error=why;return false;};
 if(!scene.producer.Available() || scene.game!="T1401N" || scene.meshes.empty()
  || scene.meshes.size()>128 || !reader)return fail("view-packet-source");
 remake::Packet p;p.frame=scene.frame;p.producer=scene.producer;p.game=scene.game;p.sourceGitSha=scene.gitSha;
 p.space=remake::Space::SampledAnchor;p.diagnosticOrigin=remake::Vec3{};
 p.diagnosticEmbeddingProvenance=scene.scope;
 p.camera.provenance=remake::Provenance::Supplied;
 p.camera.fovY=float(2*std::atan(.5*scene.size[1]/scene.focalY)*180/3.14159265358979323846);
 p.camera.aspect=float(double(scene.size[0])/scene.size[1]*scene.focalY/scene.focalX);
 // Diagnostic enclosure from the retained experiment, NOT recovered game clips.
 p.camera.nearPlane=.1f;p.camera.farPlane=2501;
 p.omissions={"camera-relative experiment; world and game camera not recovered",
  "unsupported PVR draws and offscreen geometry omitted",
  "source color is not physical albedo; fog offset and modifier shading omitted",
  "HUD and translucent layers not part of this opaque diagnostic scene",
  "camera-relative temporal history and complete scene coverage unproven"};
	if(scene.estimatedVertices) p.omissions.push_back("projected-depth estimated vertices="+std::to_string(scene.estimatedVertices)+"; no source-transform or physical-depth claim");
 std::size_t vertices=0,textureBytes=0;
 for(const auto& source:scene.meshes) {
  if(source.vertices.size()>65536-vertices || source.vertices.size()%3)return fail("view-packet-vertex-bound");
  vertices+=source.vertices.size();
  const auto& draw=source.sourceDraw;
  remake::Mesh mesh;mesh.id=std::uint64_t(draw.ordinal)+1;mesh.frame=scene.frame;mesh.sourceTsp=draw.state.tsp.full;
  mesh.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  mesh.material.emplace();mesh.material->albedo={1,1,1};mesh.material->sourceColorExperiment=true;
  if(draw.texture) {
   mesh.texture={draw.state.tcw.full,draw.texture->upload,draw.texture->palette.value_or(0),draw.texture->rtt,true};
   mesh.material->sourceTexture=mesh.texture;
  }
  if(!reader(draw,mesh.material->sourceDdsBytes,error))return false;
  if(mesh.material->sourceDdsBytes.size()>remake::Limits{}.textureBytes-textureBytes)return fail("view-packet-texture-bound");
  textureBytes+=mesh.material->sourceDdsBytes.size();
  for(const auto& v:source.vertices) {
   remake::Vertex out;out.position={v.position[0],v.position[1],v.position[2]};
   out.normal=remake::Vec3{v.normal[0],v.normal[1],v.normal[2]};out.u=v.source.u;out.v=v.source.v;
   out.publicColor=remake::PublicColorFromBgra({v.source.col[0],v.source.col[1],v.source.col[2],v.source.col[3]});
   mesh.indices.push_back(unsigned(mesh.vertices.size()));mesh.vertices.push_back(out);
  }
  p.meshes.push_back(std::move(mesh));
 }
 auto checked=remake::ReadyForDiagnosticAdapter(p,p.frame,p.game,true);if(!checked.ok){error=checked.reason;return false;}
 output=std::move(p);error.clear();return true;
}
namespace {
constexpr const char* scope="observed-camera-relative-experiment-not-world-reconstruction";
void require(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
struct Wire {
 std::istream* input=nullptr;std::ostream* output=nullptr;
 std::size_t budget=72*1024*1024;
 void bytes(void* data,std::size_t count) {
  require(count<=budget,"view-wire-byte-bound");budget-=count;
  if(input)require(bool(input->read(static_cast<char*>(data),count)),"view-wire-truncated");
  else require(bool(output->write(static_cast<const char*>(data),count)),"view-wire-write");
 }
 void word(std::uint32_t& value) {
  unsigned char b[4]{};if(output)for(unsigned i=0;i<4;++i)b[i]=static_cast<unsigned char>(value>>(8*i));
  bytes(b,4);if(input){value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(b[i])<<(8*i);}
 }
 void wide(std::uint64_t& value){std::uint32_t lo=std::uint32_t(value),hi=std::uint32_t(value>>32);word(lo);word(hi);if(input)value=lo|(std::uint64_t(hi)<<32);}
 void real(float& value){std::uint32_t bits;std::memcpy(&bits,&value,4);word(bits);if(input)std::memcpy(&value,&bits,4);require(std::isfinite(value),"view-wire-nonfinite");}
 void vector(remake::Vec3& v){real(v.x);real(v.y);real(v.z);}
 std::uint32_t count(std::size_t size,std::uint32_t bound){auto n=std::uint32_t(size);require(size<=bound,"view-wire-count");word(n);require(n<=bound,"view-wire-count");return n;}
 void string(std::string& s,unsigned bound){const auto n=count(s.size(),bound);if(input)s.resize(n);if(n)bytes(s.data(),n);}
};
void packet(Wire& wire,remake::Packet& p) {
 std::uint32_t magic=0x56524346,version=1;wire.word(magic);wire.word(version);
 require(magic==0x56524346&&version==1,"view-wire-schema");
 wire.wide(p.frame);wire.wide(p.producer.epoch);wire.wide(p.producer.ordinal);wire.wide(p.producer.cycle);
 wire.string(p.game,64);wire.string(p.sourceGitSha,64);
 wire.string(p.diagnosticEmbeddingProvenance,128);require(p.diagnosticEmbeddingProvenance==scope
  ||p.diagnosticEmbeddingProvenance=="mixed-observed-and-projected-depth-estimate-not-world-reconstruction","view-wire-scope");
 wire.real(p.camera.fovY);wire.real(p.camera.aspect);wire.real(p.camera.nearPlane);wire.real(p.camera.farPlane);
 p.space=remake::Space::SampledAnchor;p.diagnosticOrigin=remake::Vec3{};p.camera.provenance=remake::Provenance::Supplied;
 const auto omissions=wire.count(p.omissions.size(),64);if(wire.input)p.omissions.resize(omissions);
 for(auto& text:p.omissions)wire.string(text,256);
 const auto meshes=wire.count(p.meshes.size(),128);if(wire.input)p.meshes.resize(meshes);
 std::size_t vertexTotal=0,indexTotal=0,textureTotal=0;
 for(auto& mesh:p.meshes) {
  wire.wide(mesh.id);mesh.frame=p.frame;mesh.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  std::uint32_t tsp=mesh.sourceTsp.value_or(0);wire.word(tsp);mesh.sourceTsp=tsp;
  std::uint32_t known=mesh.texture.known;wire.word(known);require(known<=1,"view-wire-texture-known");mesh.texture.known=known!=0;
  wire.wide(mesh.texture.id);wire.wide(mesh.texture.generation);wire.wide(mesh.texture.paletteGeneration);wire.wide(mesh.texture.rttGeneration);
  if(wire.input){mesh.material.emplace();mesh.material->albedo={1,1,1};mesh.material->sourceColorExperiment=true;if(known)mesh.material->sourceTexture=mesh.texture;}
  require(mesh.material.has_value()&&mesh.material->sourceDds.empty(),"view-wire-owned-texture-required");
  auto& data=mesh.material->sourceDdsBytes;const auto length=wire.count(data.size(),unsigned(remake::Limits{}.textureBytes-textureTotal));textureTotal+=length;
  if(wire.input)data.resize(length);if(length)wire.bytes(data.data(),length);
  const auto vertices=wire.count(mesh.vertices.size(),unsigned(65536-vertexTotal));vertexTotal+=vertices;
  if(wire.input)mesh.vertices.resize(vertices);
  for(auto& vertex:mesh.vertices) {
   wire.vector(vertex.position);if(wire.input)vertex.normal.emplace();require(vertex.normal.has_value(),"view-wire-normal-required");
   wire.vector(*vertex.normal);wire.real(vertex.u);wire.real(vertex.v);wire.word(vertex.publicColor);
  }
  const auto indices=wire.count(mesh.indices.size(),unsigned(262144-indexTotal));indexTotal+=indices;
  if(wire.input)mesh.indices.resize(indices);for(auto& index:mesh.indices)wire.word(index);
 }
 require(p.producer.Available()&&p.game=="T1401N"&&p.frame!=0,"view-wire-identity");
 auto checked=remake::ReadyForDiagnosticAdapter(p,p.frame,p.game,true);require(checked.ok,checked.reason.c_str());
}
}
bool SerializeRemakeViewPacket(std::ostream& out,const remake::Packet& source,std::string& error) {
 try {
  auto checked=remake::ReadyForDiagnosticAdapter(source,source.frame,source.game,true);require(checked.ok,checked.reason.c_str());
  // This format only represents the identity-view experiment, not arbitrary cameras/materials.
  require(source.camera.position.x==0&&source.camera.position.y==0&&source.camera.position.z==0
   && source.camera.right.x==1&&source.camera.right.y==0&&source.camera.right.z==0
   && source.camera.up.x==0&&source.camera.up.y==1&&source.camera.up.z==0
   && source.camera.forward.x==0&&source.camera.forward.y==0&&source.camera.forward.z==1,"view-wire-camera");
  require(source.diagnosticOrigin && source.diagnosticOrigin->x==0 && source.diagnosticOrigin->y==0
   && source.diagnosticOrigin->z==0,"view-wire-origin");
  for(const auto& mesh:source.meshes) {
   require(mesh.topology==remake::Topology::Triangles && mesh.sourceTsp && mesh.frame==source.frame,"view-wire-topology-or-frame");
   const auto& m=*mesh.material;
   require(m.albedo.x==1&&m.albedo.y==1&&m.albedo.z==1&&m.roughness==.8f,"view-wire-material");
  }
  remake::Packet copy=source;Wire wire{nullptr,&out};packet(wire,copy);require(bool(out),"view-wire-write");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool DeserializeRemakeViewPacket(std::istream& input,remake::Packet& output,std::string& error) {
 try {
  Wire wire{&input,nullptr};remake::Packet p;packet(wire,p);
  require(input.peek()==std::char_traits<char>::eof(),"view-wire-trailing-bytes");output=std::move(p);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool WriteRemakeViewPacket(const std::filesystem::path& path,const remake::Packet& source,std::string& error) {
 try {
  require(!std::filesystem::exists(path),"view-wire-output-exists");std::ofstream output(path,std::ios::binary);
  if(!SerializeRemakeViewPacket(output,source,error))return false;
  output.close();require(bool(output),"view-wire-close");return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool ReadRemakeViewPacket(const std::filesystem::path& path,remake::Packet& output,std::string& error) {
 try {
  require(std::filesystem::file_size(path)<=72*1024*1024,"view-wire-file-bound");
  std::ifstream input(path,std::ios::binary);return DeserializeRemakeViewPacket(input,output,error);
 }catch(const std::exception& e){error=e.what();return false;}
}
}
