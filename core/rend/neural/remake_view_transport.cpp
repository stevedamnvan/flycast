// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_view_transport.h"
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace flycast::rend::neural {
namespace {
// Camera-relative identity transform only. Clip geometry, never widen the
// supplied enclosure or reinterpret original PVR depth. A triangle can become
// at most a pentagon after the two parallel plane clips.
bool ClipViewMesh(remake::Mesh& mesh,float nearPlane,float farPlane) {
 std::vector<remake::Vertex> clipped;
 const auto intersect=[](const remake::Vertex& a,const remake::Vertex& b,float plane) {
  const double t=(double(plane)-a.position.z)/(double(b.position.z)-a.position.z);
  const auto lerp=[&](float x,float y){return float(double(x)+(double(y)-x)*t);};
  remake::Vertex v=a;v.position={lerp(a.position.x,b.position.x),lerp(a.position.y,b.position.y),plane};
  v.u=lerp(a.u,b.u);v.v=lerp(a.v,b.v);
  // Production source triangles carry a single flat normal.
  v.publicColor=0;
  for(unsigned c=0;c<4;++c) {
   const auto x=(a.publicColor>>(c*8))&255u,y=(b.publicColor>>(c*8))&255u;
   const auto value=unsigned(std::lround(double(x)+(double(y)-x)*t));
   v.publicColor|=(value&255u)<<(c*8);
  }
  return v;
 };
 clipped.reserve(mesh.vertices.size());
 // Fixed-capacity polygons (a triangle clipped by two planes has at most six
 // candidates before the pentagon bound applies): no heap use per triangle.
 struct Polygon {std::array<remake::Vertex,8> v;std::size_t n=0;};
 for(std::size_t i=0;i<mesh.vertices.size();i+=3) {
  Polygon polygon;for(unsigned k=0;k<3;++k)polygon.v[polygon.n++]=mesh.vertices[i+k];
  for(std::size_t k=0;k<polygon.n;++k){const auto& v=polygon.v[k];if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)
   ||!std::isfinite(v.position.z)||!std::isfinite(v.u)||!std::isfinite(v.v))return false;}
  for(unsigned pass=0;pass<2&&polygon.n;++pass) {
   const float plane=pass?farPlane:nearPlane;
   const auto inside=[&](const remake::Vertex& v){return pass?v.position.z<=plane:v.position.z>=plane;};
   Polygon next;auto a=polygon.v[polygon.n-1];bool aInside=inside(a);
   for(std::size_t k=0;k<polygon.n;++k) {
    const auto& b=polygon.v[k];const bool bInside=inside(b);
    if(aInside!=bInside)next.v[next.n++]=intersect(a,b,plane);
    if(bInside)next.v[next.n++]=b;
    a=b;aInside=bInside;
   }
   if(next.n>5)return false;
   polygon=next;
  }
  for(std::size_t j=1;j+1<polygon.n;++j) {
   if(clipped.size()>65536-3)return false;
   clipped.push_back(polygon.v[0]);clipped.push_back(polygon.v[j]);clipped.push_back(polygon.v[j+1]);
  }
 }
 mesh.vertices=std::move(clipped);mesh.indices.clear();
 for(std::size_t i=0;i<mesh.vertices.size();++i)mesh.indices.push_back(unsigned(i));
 return true;
}
}
bool BuildRemakeViewPacket(const RemakeViewScene& scene,const RemakeTextureReader& reader,
 remake::Packet& output,std::string& error,const RemakeTextureSent& sent) {
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
 std::size_t vertices=0,textureBytes=0,clippedVertices=0;
 for(const auto& mesh:scene.meshes)if(mesh.sourceAlphaReference) {
  p.omissions.push_back("source alpha-tested cutouts included experimentally; native/Remix coverage validation required");break;
 }
 for(const auto& source:scene.meshes) {
  if(source.vertices.size()>65536-vertices || source.vertices.size()%3)return fail("view-packet-vertex-bound");
  vertices+=source.vertices.size();
  const auto& draw=source.sourceDraw;
  remake::Mesh mesh;mesh.id=(std::uint64_t(draw.list)<<32)|(std::uint64_t(draw.ordinal)+1);
  mesh.frame=scene.frame;mesh.sourceTsp=draw.state.tsp.full;mesh.sourceAlphaReference=source.sourceAlphaReference;
  mesh.sourceAlphaBlend=source.sourceAlphaBlend;
  mesh.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  mesh.material.emplace();mesh.material->albedo={1,1,1};mesh.material->sourceColorExperiment=true;
  if(draw.texture) {
   mesh.texture={draw.state.tcw.full,draw.texture->upload,draw.texture->palette.value_or(0),draw.texture->rtt,true};
   mesh.material->sourceTexture=mesh.texture;
  }
  if(sent&&draw.texture&&sent(mesh.texture))mesh.textureWire=remake::TextureWire::Referenced;
  else {
   if(!reader(draw,mesh.material->sourceDdsBytes,error))return false;
   if(sent&&draw.texture&&!mesh.material->sourceDdsBytes.empty())mesh.textureWire=remake::TextureWire::Registered;
  }
  if(mesh.material->sourceDdsBytes.size()>remake::Limits{}.textureBytes-textureBytes)return fail("view-packet-texture-bound");
  textureBytes+=mesh.material->sourceDdsBytes.size();
  for(const auto& v:source.vertices) {
   remake::Vertex out;out.position={v.position[0],v.position[1],v.position[2]};
   out.normal=remake::Vec3{v.normal[0],v.normal[1],v.normal[2]};out.u=v.source.u;out.v=v.source.v;
   out.publicColor=remake::PublicColorFromBgra({v.source.col[0],v.source.col[1],v.source.col[2],v.source.col[3]});
   mesh.indices.push_back(unsigned(mesh.vertices.size()));mesh.vertices.push_back(out);
  }
  if(!ClipViewMesh(mesh,p.camera.nearPlane,p.camera.farPlane))return fail("view-packet-clip-invalid-or-bound");
  if(mesh.vertices.empty())continue;
  if(mesh.vertices.size()>65536-clippedVertices)return fail("view-packet-clipped-vertex-bound");
  clippedVertices+=mesh.vertices.size();
  p.meshes.push_back(std::move(mesh));
 }
 auto checked=remake::ReadyForDiagnosticAdapter(p,p.frame,p.game,true);if(!checked.ok){error=checked.reason;return false;}
 output=std::move(p);error.clear();return true;
}
namespace {
constexpr const char* scope="observed-camera-relative-experiment-not-world-reconstruction";
constexpr const char* anchoredScope="diagnostic-camera-embedded-anchor-not-world-reconstruction";
bool identityPose(const remake::Camera& c) {
 return c.position.x==0&&c.position.y==0&&c.position.z==0
  &&c.right.x==1&&c.right.y==0&&c.right.z==0
  &&c.up.x==0&&c.up.y==1&&c.up.z==0
  &&c.forward.x==0&&c.forward.y==0&&c.forward.z==1;
}
void require(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
struct Wire {
 std::istream* input=nullptr;std::ostream* output=nullptr;
 // Raw in-memory input (live channel): no stream sentry per word.
 const char* rawInput=nullptr;std::size_t rawLeft=0;
 std::size_t budget=72*1024*1024;
 void bytes(void* data,std::size_t count) {
  require(count<=budget,"view-wire-byte-bound");budget-=count;
  if(rawInput){require(count<=rawLeft,"view-wire-truncated");std::memcpy(data,rawInput,count);rawInput+=count;rawLeft-=count;}
  else if(input)require(bool(input->read(static_cast<char*>(data),count)),"view-wire-truncated");
  else require(bool(output->write(static_cast<const char*>(data),count)),"view-wire-write");
 }
 bool reading()const{return rawInput||input;}
 void word(std::uint32_t& value) {
  unsigned char b[4]{};if(!reading())for(unsigned i=0;i<4;++i)b[i]=static_cast<unsigned char>(value>>(8*i));
  bytes(b,4);if(reading()){value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(b[i])<<(8*i);}
 }
 void wide(std::uint64_t& value){std::uint32_t lo=std::uint32_t(value),hi=std::uint32_t(value>>32);word(lo);word(hi);if(reading())value=lo|(std::uint64_t(hi)<<32);}
 void real(float& value){std::uint32_t bits;std::memcpy(&bits,&value,4);word(bits);if(reading())std::memcpy(&value,&bits,4);require(std::isfinite(value),"view-wire-nonfinite");}
 void vector(remake::Vec3& v){real(v.x);real(v.y);real(v.z);}
 std::uint32_t count(std::size_t size,std::uint32_t bound){auto n=std::uint32_t(size);require(size<=bound,"view-wire-count");word(n);require(n<=bound,"view-wire-count");return n;}
 void string(std::string& s,unsigned bound){const auto n=count(s.size(),bound);if(reading())s.resize(n);if(n)bytes(s.data(),n);}
};
void packet(Wire& wire,remake::Packet& p) {
 // Preserve byte-identical version1 output for existing opaque-only captures.
 std::uint32_t magic=0x56524346,version=1;
 if(!wire.reading())for(const auto& mesh:p.meshes)if(mesh.sourceAlphaReference)version=2;
 if(!wire.reading())for(const auto& mesh:p.meshes)if(mesh.sourceAlphaBlend)version=3;
 if(!wire.reading()&&p.diagnosticEmbeddingProvenance==anchoredScope)version=4;
 if(!wire.reading())for(const auto& mesh:p.meshes)if(mesh.textureWire!=remake::TextureWire::Carried)version=5;
 wire.word(magic);wire.word(version);
 require(magic==0x56524346&&(version>=1&&version<=5),"view-wire-schema");
 wire.wide(p.frame);wire.wide(p.producer.epoch);wire.wide(p.producer.ordinal);wire.wide(p.producer.cycle);
 wire.string(p.game,64);wire.string(p.sourceGitSha,64);
 wire.string(p.diagnosticEmbeddingProvenance,128);require(p.diagnosticEmbeddingProvenance==scope
  ||p.diagnosticEmbeddingProvenance=="mixed-observed-and-projected-depth-estimate-not-world-reconstruction"
  ||(version>=4&&p.diagnosticEmbeddingProvenance==anchoredScope),"view-wire-scope");
 wire.real(p.camera.fovY);wire.real(p.camera.aspect);wire.real(p.camera.nearPlane);wire.real(p.camera.farPlane);
 p.space=remake::Space::SampledAnchor;p.camera.provenance=remake::Provenance::Supplied;
 // Version4 is always anchored; version5 carries the pose only for anchored scope.
 if(version==4||(version>=5&&p.diagnosticEmbeddingProvenance==anchoredScope)) {
  require(p.diagnosticEmbeddingProvenance==anchoredScope,"view-wire-anchor-scope");
  wire.vector(p.camera.position);wire.vector(p.camera.right);wire.vector(p.camera.up);wire.vector(p.camera.forward);
  if(wire.reading())p.diagnosticOrigin.emplace();
  require(p.diagnosticOrigin.has_value(),"view-wire-origin");wire.vector(*p.diagnosticOrigin);
 }else p.diagnosticOrigin=remake::Vec3{};
 const auto omissions=wire.count(p.omissions.size(),64);if(wire.reading())p.omissions.resize(omissions);
 for(auto& text:p.omissions)wire.string(text,256);
 const auto meshes=wire.count(p.meshes.size(),128);if(wire.reading())p.meshes.resize(meshes);
 std::size_t vertexTotal=0,indexTotal=0,textureTotal=0;
 for(auto& mesh:p.meshes) {
  wire.wide(mesh.id);mesh.frame=p.frame;mesh.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  std::uint32_t tsp=mesh.sourceTsp.value_or(0);wire.word(tsp);mesh.sourceTsp=tsp;
  if(version>=2) {
   std::uint32_t alpha=mesh.sourceAlphaReference?*mesh.sourceAlphaReference:256u;
   wire.word(alpha);require(alpha<=256,"view-wire-alpha-reference");
   if(alpha==256)mesh.sourceAlphaReference.reset();else mesh.sourceAlphaReference=static_cast<std::uint8_t>(alpha);
  }else mesh.sourceAlphaReference.reset();
  if(version>=3) {std::uint32_t alpha=mesh.sourceAlphaBlend;wire.word(alpha);require(alpha<=1,"view-wire-alpha-blend");mesh.sourceAlphaBlend=alpha!=0;}
  else mesh.sourceAlphaBlend=false;
  std::uint32_t known=mesh.texture.known;wire.word(known);require(known<=1,"view-wire-texture-known");mesh.texture.known=known!=0;
  wire.wide(mesh.texture.id);wire.wide(mesh.texture.generation);wire.wide(mesh.texture.paletteGeneration);wire.wide(mesh.texture.rttGeneration);
  std::uint32_t mode=std::uint32_t(mesh.textureWire);
  if(version>=5){wire.word(mode);require(mode<=2&&(mode==0||known),"view-wire-texture-mode");mesh.textureWire=remake::TextureWire(mode);}
  else{require(mode==0,"view-wire-texture-mode");mesh.textureWire=remake::TextureWire::Carried;}
  if(wire.reading()){mesh.material.emplace();mesh.material->albedo={1,1,1};mesh.material->sourceColorExperiment=true;if(known)mesh.material->sourceTexture=mesh.texture;}
  require(mesh.material.has_value()&&mesh.material->sourceDds.empty(),"view-wire-owned-texture-required");
  auto& data=mesh.material->sourceDdsBytes;const auto length=wire.count(data.size(),unsigned(remake::Limits{}.textureBytes-textureTotal));textureTotal+=length;
  require(mode==2?length==0:mode==1?length>0:true,"view-wire-texture-mode");
  if(wire.reading())data.resize(length);if(length)wire.bytes(data.data(),length);
  const auto vertices=wire.count(mesh.vertices.size(),unsigned(remake::Limits{}.vertices-vertexTotal));vertexTotal+=vertices;
  if(wire.reading())mesh.vertices.resize(vertices);
  for(auto& vertex:mesh.vertices) {
   wire.vector(vertex.position);if(wire.reading())vertex.normal.emplace();require(vertex.normal.has_value(),"view-wire-normal-required");
   wire.vector(*vertex.normal);wire.real(vertex.u);wire.real(vertex.v);wire.word(vertex.publicColor);
  }
  const auto indices=wire.count(mesh.indices.size(),unsigned(262144-indexTotal));indexTotal+=indices;
  if(wire.reading())mesh.indices.resize(indices);for(auto& index:mesh.indices)wire.word(index);
 }
 require(p.producer.Available()&&p.game=="T1401N"&&p.frame!=0,"view-wire-identity");
 auto checked=remake::ReadyForDiagnosticAdapter(p,p.frame,p.game,true);require(checked.ok,checked.reason.c_str());
}
}
// Read-only writer: never duplicate the owned texture payload to serialize it.
namespace {
struct ConstWire {
 // Words are staged in memory and written to the stream once at the end:
 // the same bytes as the per-word stream writes, without a sentry per word.
 std::ostream* out;std::vector<char> staged;std::size_t budget=72*1024*1024;
 explicit ConstWire(std::ostream& output):out(&output){staged.reserve(1u<<20);}
 void bytes(const void* p,std::size_t n){require(n<=budget,"view-wire-byte-bound");budget-=n;
  staged.insert(staged.end(),static_cast<const char*>(p),static_cast<const char*>(p)+n);}
 void word(std::uint32_t v){unsigned char b[4];for(unsigned i=0;i<4;++i)b[i]=static_cast<unsigned char>(v>>(8*i));bytes(b,4);}
 void wide(std::uint64_t v){word(std::uint32_t(v));word(std::uint32_t(v>>32));}
 void real(float v){require(std::isfinite(v),"view-wire-nonfinite");std::uint32_t bits;std::memcpy(&bits,&v,4);word(bits);}
 void vector(const remake::Vec3& v){real(v.x);real(v.y);real(v.z);}
 // One 36-byte record per vertex: the same little-endian words as nine
 // real/word calls (verified byte-identical by the round-trip tests), staged
 // with one append instead of nine. Finite checks are unchanged.
 void vertex(const remake::Vertex& v) {
  require(v.normal.has_value(),"view-wire-normal-required");
  const float reals[8]={v.position.x,v.position.y,v.position.z,v.normal->x,v.normal->y,v.normal->z,v.u,v.v};
  for(float r:reals)require(std::isfinite(r),"view-wire-nonfinite");
  static_assert(std::numeric_limits<float>::is_iec559&&sizeof(float)==4,"vertex record");
  unsigned char record[36];std::memcpy(record,reals,32);
  for(unsigned i=0;i<4;++i)record[32+i]=static_cast<unsigned char>(v.publicColor>>(8*i));
  bytes(record,36);
 }
 void reserve(std::size_t n){staged.reserve(staged.size()+n);}
 void count(std::size_t n,unsigned bound){require(n<=bound,"view-wire-count");word(std::uint32_t(n));}
 void string(const std::string& s,unsigned bound){count(s.size(),bound);if(!s.empty())bytes(s.data(),s.size());}
 void flush(){require(bool(out->write(staged.data(),std::streamsize(staged.size()))),"view-wire-write");staged.clear();}
};
void writePacket(ConstWire& w,const remake::Packet& p) {
 unsigned version=1;
 for(const auto& m:p.meshes)if(m.sourceAlphaReference)version=2;
 for(const auto& m:p.meshes)if(m.sourceAlphaBlend)version=3;
 if(p.diagnosticEmbeddingProvenance==anchoredScope)version=4;
 for(const auto& m:p.meshes)if(m.textureWire!=remake::TextureWire::Carried)version=5;
 w.word(0x56524346);w.word(version);
 w.wide(p.frame);w.wide(p.producer.epoch);w.wide(p.producer.ordinal);w.wide(p.producer.cycle);
 w.string(p.game,64);w.string(p.sourceGitSha,64);w.string(p.diagnosticEmbeddingProvenance,128);
 require(p.diagnosticEmbeddingProvenance==scope||p.diagnosticEmbeddingProvenance=="mixed-observed-and-projected-depth-estimate-not-world-reconstruction"
  ||(version>=4&&p.diagnosticEmbeddingProvenance==anchoredScope),"view-wire-scope");
 w.real(p.camera.fovY);w.real(p.camera.aspect);w.real(p.camera.nearPlane);w.real(p.camera.farPlane);
 if(version==4||(version>=5&&p.diagnosticEmbeddingProvenance==anchoredScope)){w.vector(p.camera.position);w.vector(p.camera.right);w.vector(p.camera.up);w.vector(p.camera.forward);
  require(p.diagnosticOrigin.has_value(),"view-wire-origin");w.vector(*p.diagnosticOrigin);}
 w.count(p.omissions.size(),64);for(const auto& s:p.omissions)w.string(s,256);
 w.count(p.meshes.size(),128);std::size_t vertexTotal=0,indexTotal=0,textureTotal=0;
 for(const auto& m:p.meshes){
  w.wide(m.id);w.word(m.sourceTsp.value_or(0));
  if(version>=2)w.word(m.sourceAlphaReference?*m.sourceAlphaReference:256u);
  if(version>=3)w.word(m.sourceAlphaBlend);
  w.word(m.texture.known);w.wide(m.texture.id);w.wide(m.texture.generation);w.wide(m.texture.paletteGeneration);w.wide(m.texture.rttGeneration);
  const auto mode=std::uint32_t(m.textureWire);require(mode<=2&&(mode==0||m.texture.known),"view-wire-texture-mode");
  if(version>=5)w.word(mode);
  require(m.material&&m.material->sourceDds.empty(),"view-wire-owned-texture-required");
  const auto& data=m.material->sourceDdsBytes;
  require(mode==2?data.empty():mode==1?!data.empty():true,"view-wire-texture-mode");
  w.count(data.size(),unsigned(remake::Limits{}.textureBytes-textureTotal));textureTotal+=data.size();
  if(!data.empty())w.bytes(data.data(),data.size());
  w.count(m.vertices.size(),unsigned(remake::Limits{}.vertices-vertexTotal));vertexTotal+=m.vertices.size();
  w.reserve(m.vertices.size()*36+m.indices.size()*4);
  for(const auto& v:m.vertices)w.vertex(v);
  w.count(m.indices.size(),unsigned(262144-indexTotal));indexTotal+=m.indices.size();
  static_assert(sizeof(std::uint32_t)==4,"index word");
  {std::vector<unsigned char> words(m.indices.size()*4);
   for(std::size_t i=0;i<m.indices.size();++i)for(unsigned b=0;b<4;++b)words[i*4+b]=static_cast<unsigned char>(m.indices[i]>>(8*b));
   if(!words.empty())w.bytes(words.data(),words.size());}
 }
 require(p.producer.Available()&&p.game=="T1401N"&&p.frame,"view-wire-identity");
}
}
bool SerializeRemakeViewPacket(std::ostream& out,const remake::Packet& source,std::string& error) {
 try {
  auto checked=remake::ReadyForDiagnosticAdapter(source,source.frame,source.game,true);require(checked.ok,checked.reason.c_str());
  const bool anchored=source.diagnosticEmbeddingProvenance==anchoredScope;
  require(anchored||identityPose(source.camera),"view-wire-camera");
  require(source.diagnosticOrigin && (anchored||(source.diagnosticOrigin->x==0 && source.diagnosticOrigin->y==0
   && source.diagnosticOrigin->z==0)),"view-wire-origin");
  for(const auto& mesh:source.meshes) {
   require(mesh.topology==remake::Topology::Triangles && mesh.sourceTsp && mesh.frame==source.frame,"view-wire-topology-or-frame");
   const auto& m=*mesh.material;
   require(m.albedo.x==1&&m.albedo.y==1&&m.albedo.z==1&&m.roughness==.8f,"view-wire-material");
  }
  ConstWire wire(out);writePacket(wire,source);wire.flush();require(bool(out),"view-wire-write");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool VerifyRemakeViewWireParity(const remake::Packet& source,std::string& error) {
 try {
  std::ostringstream actual(std::ios::binary),referenceWire(std::ios::binary);
  if(!SerializeRemakeViewPacket(actual,source,error))return false;
  auto copy=source;Wire wire{nullptr,&referenceWire};packet(wire,copy);
  require(actual.str()==referenceWire.str(),"view-wire-reference-mismatch");return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool DeserializeRemakeViewPacket(std::istream& input,remake::Packet& output,std::string& error) {
 try {
  Wire wire{&input,nullptr};remake::Packet p;packet(wire,p);
  require(input.peek()==std::char_traits<char>::eof(),"view-wire-trailing-bytes");output=std::move(p);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool DeserializeRemakeViewPacket(const char* data,std::size_t size,remake::Packet& output,std::string& error) {
 try {
  Wire wire;wire.rawInput=data;wire.rawLeft=size;remake::Packet p;packet(wire,p);
  require(wire.rawLeft==0,"view-wire-trailing-bytes");output=std::move(p);error.clear();return true;
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
