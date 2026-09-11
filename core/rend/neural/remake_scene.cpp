// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_scene.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <fstream>

namespace flycast::rend::neural::remake {
namespace {
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool validSourceDdsHeader(const unsigned char* header, std::uint64_t size) {
 if(size<152 || size>64*1024*1024)return false;
 auto word=[&](unsigned offset) {return std::uint32_t(header[offset])|(std::uint32_t(header[offset+1])<<8)
  |(std::uint32_t(header[offset+2])<<16)|(std::uint32_t(header[offset+3])<<24);};
 if(word(0)!=0x20534444 || word(4)!=124 || word(76)!=32 || word(84)!=0x30315844
  || word(128)!=28 || word(132)!=3 || word(136)!=0 || word(140)!=1)return false;
 unsigned w=word(16),h=word(12),levels=word(28);
 if(!w||!h||w>4096||h>4096||!levels||levels>13)return false;
 // Deliberately narrow contract: exactly the source_dds serializer's layout.
 const unsigned flags=0x100f|(levels>1?0x20000:0);
 const unsigned caps=0x1000|(levels>1?0x400008:0);
 if(word(8)!=flags || word(20)!=w*4 || word(24)!=0 || word(80)!=4
  || word(108)!=caps || word(112)!=0 || word(116)!=0 || word(120)!=0
  || word(124)!=0 || word(144)!=0)return false;
 for(unsigned offset=32;offset<76;offset+=4)if(word(offset)!=0)return false;
 for(unsigned offset=88;offset<108;offset+=4)if(word(offset)!=0)return false;
 std::uint64_t bytes=148;
 for(unsigned i=0;i<levels;++i) {
  bytes+=std::uint64_t(w)*h*4;
  if(i+1<levels && w==1 && h==1)return false;
  w=std::max(1u,w/2);h=std::max(1u,h/2);
 }
 return bytes==size;
}
bool validSourceDds(const std::filesystem::path& path) {
 std::error_code ec;
 if(!path.is_absolute() || path.native().size()>4096 || path.extension()!=L".dds"
  || !std::filesystem::is_regular_file(path,ec))return false;
 const auto size=std::filesystem::file_size(path,ec);
 if(ec || size<152 || size>64*1024*1024)return false;
 std::array<unsigned char,148> header{};std::ifstream in(path,std::ios::binary);
 return in.read(reinterpret_cast<char*>(header.data()),header.size())
  && validSourceDdsHeader(header.data(),size);
}
float dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
bool validAxes(const Camera& c) {
 const auto r=c.right,u=c.up,f=c.forward;
 const Vec3 cross{r.y*u.z-r.z*u.y,r.z*u.x-r.x*u.z,r.x*u.y-r.y*u.x};
 return finite(r)&&finite(u)&&finite(f) && std::abs(dot(r,r)-1)<1e-6f
  && std::abs(dot(u,u)-1)<1e-6f && std::abs(dot(f,f)-1)<1e-6f
  && std::abs(dot(r,u))<1e-6f && std::abs(dot(r,f))<1e-6f
  && std::abs(dot(u,f))<1e-6f && dot(cross,f)>0;
}
bool validCamera(const Camera& c) {
 return finite(c.position) && validAxes(c) && std::isfinite(c.fovY) && c.fovY > 1 && c.fovY < 179
  && std::isfinite(c.aspect) && c.aspect > 0 && std::isfinite(c.nearPlane)
  && std::isfinite(c.farPlane) && c.nearPlane > 0 && c.farPlane > c.nearPlane;
}
float tangent(const Camera& c) { return std::tan(c.fovY * 0.008726646259971648f); }
}
bool ValidSourceDdsBytes(const std::vector<unsigned char>& bytes) {
 return bytes.size()>=152 && validSourceDdsHeader(bytes.data(),bytes.size());
}
Result OwnDiagnosticTextures(Packet& packet) {
 auto checked=Validate(packet,packet.frame,packet.game);if(!checked.ok)return checked;
 Packet owned=packet;
 std::size_t total=0;
 for(auto& mesh:owned.meshes) {
  if(!mesh.material)return {false,"material-unknown"};
  auto& material=*mesh.material;
  // A referenced texture (D-212) is owned by the consumer's session cache,
  // not by this packet; there is nothing to read or copy here.
  if(mesh.textureWire==TextureWire::Referenced)continue;
  if(!material.sourceDdsBytes.empty()) {
   if(!material.sourceDds.empty()||!ValidSourceDdsBytes(material.sourceDdsBytes))return {false,"source-texture-contract"};
  } else {
   if(!validSourceDds(material.sourceDds))return {false,"source-texture-contract"};
   std::error_code ec;const auto size=std::filesystem::file_size(material.sourceDds,ec);
   if(ec||size>Limits{}.textureBytes-total)return {false,"texture-byte-limit"};
   material.sourceDdsBytes.resize(static_cast<std::size_t>(size));
   std::ifstream input(material.sourceDds,std::ios::binary);
   if(!input.read(reinterpret_cast<char*>(material.sourceDdsBytes.data()),material.sourceDdsBytes.size())
     || input.peek()!=std::char_traits<char>::eof() || !ValidSourceDdsBytes(material.sourceDdsBytes))
    return {false,"source-texture-contract"};
   material.sourceDds.clear();
  }
  if(material.sourceDdsBytes.size()>Limits{}.textureBytes-total)return {false,"texture-byte-limit"};
  total+=material.sourceDdsBytes.size();
 }
 checked=Validate(owned,owned.frame,owned.game);if(!checked.ok)return checked;
 packet=std::move(owned);return {true,"owned-textures-not-live-provider-proof"};
}
Result Validate(const Packet& p, std::uint64_t frame, const std::string& game, const Limits& limits) {
 if (p.version != 1) return {false, "schema"};
 if (p.frame != frame || p.game != game || p.game.empty() || p.game.size() > 64) return {false, "identity"};
 if(p.sourceGitSha.size()>64)return {false,"source-sha-bound"};
 if (p.space != Space::World && p.space != Space::View && p.space != Space::PvrProjected
  && p.space != Space::SampledAnchor) return {false, "space"};
 if (p.camera.provenance != Provenance::Unknown && p.camera.provenance != Provenance::Supplied
  && p.camera.provenance != Provenance::Analytic) return {false, "provenance"};
 if (p.truncated) return {false, "truncated"};
 if (p.meshes.empty() || p.meshes.size() > limits.meshes || p.omissions.size() > 64) return {false, "count-limit"};
 // Account actual packet-owned element storage, not allocator capacity. Ingestion
 // must check these same limits before allocation; this API accepts no files.
 std::size_t bytes = sizeof(Packet), vertices = 0, indices = 0, textureBytes = 0;
 auto addBytes = [&](std::size_t n) {
  if (bytes > limits.bytes || n > limits.bytes - bytes) return false;
  bytes += n; return true;
 };
 if (!addBytes(p.game.size())) return {false, "byte-limit"};
 if (!addBytes(p.sourceGitSha.size())) return {false,"byte-limit"};
 for (const auto& omission : p.omissions)
  if (omission.empty() || omission.size() > 256 || !addBytes(sizeof(std::string) + omission.size())) return {false, "omission-limit"};
 if (p.camera.provenance != Provenance::Unknown && !validCamera(p.camera)) return {false, "camera"};
 std::set<std::uint64_t> ids;
 for (const auto& m : p.meshes) {
  if (m.frame != frame) return {false, "mesh-frame"};
  if (!m.id || !ids.insert(m.id).second) return {false, "mesh-id"};
  if (m.topology != Topology::Triangles && m.topology != Topology::Strip) return {false, "topology"};
  if (m.vertices.size() > limits.vertices - vertices || m.indices.size() > limits.indices - indices) return {false, "count-limit"};
  vertices += m.vertices.size(); indices += m.indices.size();
  if (!addBytes(sizeof(Mesh))) return {false, "byte-limit"};
  if(m.material) {
   if(m.material->sourceDdsBytes.size()>limits.textureBytes-textureBytes)return {false,"texture-byte-limit"};
   textureBytes+=m.material->sourceDdsBytes.size();
   const auto length=m.material->sourceDds.native().size();
   if(length>4096 || length>(limits.bytes-bytes)/sizeof(std::filesystem::path::value_type)
    || !addBytes(length*sizeof(std::filesystem::path::value_type)))return {false,"byte-limit"};
  }
  // Division before multiplication prevents overflow with adversarial limits.
  if (m.vertices.size() > (limits.bytes - bytes) / sizeof(Vertex)
   || !addBytes(m.vertices.size() * sizeof(Vertex))) return {false, "byte-limit"};
  if (m.indices.size() > (limits.bytes - bytes) / sizeof(std::uint32_t)
   || !addBytes(m.indices.size() * sizeof(std::uint32_t))) return {false, "byte-limit"};
  if (m.vertices.size() < 3 || m.indices.size() < 3
   || (m.topology == Topology::Triangles && m.indices.size() % 3)) return {false, "topology"};
  for (auto index : m.indices) if (index >= m.vertices.size()) return {false, "index"};
  for (const auto& v : m.vertices) {
   if (!finite(v.position) || !std::isfinite(v.u) || !std::isfinite(v.v)) return {false, "nonfinite"};
   if (v.normal && (!finite(*v.normal) || std::abs(v.normal->x*v.normal->x + v.normal->y*v.normal->y + v.normal->z*v.normal->z - 1) > 0.001f)) return {false, "normal"};
  }
  if (m.transform) for (float n : *m.transform) if (!std::isfinite(n)) return {false, "transform"};
 }
 return {true, "valid-packet-not-scene-proof"};
}
static Result ReadyForScene(const Packet& p, std::uint64_t frame, const std::string& game, bool diagnostic) {
 auto result = Validate(p, frame, game); if (!result.ok) return result;
 if (p.camera.provenance == Provenance::Unknown) return {false, "projection-unknown"};
 if (diagnostic) {
  if(p.space!=Space::SampledAnchor || p.camera.provenance!=Provenance::Supplied || p.omissions.empty())
   return {false,"diagnostic-provenance-required"};
 } else {
  if (p.space != Space::World) return {false, "world-space-required"};
  if (!p.omissions.empty()) return {false, "incomplete-scene"};
 }
 for (const auto& m : p.meshes) {
  if (!m.transform) return {false, "transform-unknown"};
  // M1 accepts baked world-space meshes with an explicit identity transform.
  // General transforms need inverse-transpose normal handling, not guessed math.
  constexpr std::array<float,12> identity{1,0,0,0, 0,1,0,0, 0,0,1,0};
  if (*m.transform != identity) return {false, "transform-unsupported"};
  if (m.texture.known && (!m.material || !m.material->sourceTexture))
   return {false, "textured-material-unsupported"};
  if (!m.material) return {false,"material-unknown"};
  const auto& material=*m.material;
  const bool memoryTexture=!material.sourceDdsBytes.empty();
  const bool referenced=m.textureWire==TextureWire::Referenced;
  if(referenced && (!m.texture.known || memoryTexture || !material.sourceDds.empty()))return {false,"source-texture-reference-contract"};
  if(m.textureWire==TextureWire::Registered && (!m.texture.known || !memoryTexture))return {false,"source-texture-reference-contract"};
  if(memoryTexture && (!material.sourceDds.empty() || !material.sourceColorExperiment
    || !ValidSourceDdsBytes(material.sourceDdsBytes)))return {false,"source-texture-contract"};
  if(material.sourceTexture) {
   const auto& bound=*material.sourceTexture;
   if(!m.texture.known || !bound.known || !material.sourceColorExperiment || (material.sourceDds.empty()&&!memoryTexture&&!referenced)
    || bound.id!=m.texture.id || bound.generation!=m.texture.generation
    || bound.paletteGeneration!=m.texture.paletteGeneration || bound.rttGeneration!=m.texture.rttGeneration)
    return {false,"source-texture-identity"};
  }
  if(!material.sourceDds.empty() && (!material.sourceColorExperiment || !validSourceDds(material.sourceDds)))
   return {false,"source-texture-contract"};
  if (!finite(material.albedo) || material.albedo.x<0 || material.albedo.x>1
   || material.albedo.y<0 || material.albedo.y>1 || material.albedo.z<0 || material.albedo.z>1
   || !std::isfinite(material.roughness) || material.roughness<0 || material.roughness>1)
   return {false,"material-parameters"};
  // Same checks as projecting every vertex through Project(): the camera is
  // validated once and the lens tangent computed once instead of per vertex.
  if (p.camera.provenance == Provenance::Unknown || !validCamera(p.camera)) throw std::invalid_argument("projection");
  const double t=std::tan(double(p.camera.fovY)*3.14159265358979323846/360.0);
  const auto& c=p.camera;
  for (const auto& v : m.vertices) {
   if (!v.normal) return {false, "normal-unknown"};
   auto world = WorldPosition(m, v);
   if (!finite(world)) return {false, "clip-unsupported"};
   const double dx=double(world.x)-c.position.x,dy=double(world.y)-c.position.y,dz=double(world.z)-c.position.z;
   const double z=dx*c.forward.x+dy*c.forward.y+dz*c.forward.z;
   const Vec3 delta{world.x-c.position.x,world.y-c.position.y,world.z-c.position.z};
   if (dot(delta,c.forward) <= 0) return {false, "clip-unsupported"};
   if (z <= 0) throw std::invalid_argument("behind camera");
   const double x=dx*c.right.x+dy*c.right.y+dz*c.right.z,y=dx*c.up.x+dy*c.up.y+dz*c.up.z;
   const Vec3 s{float(0.5+x/(2*z*t*c.aspect)),float(0.5-y/(2*z*t)),float(z)};
   if (!finite(s) || s.z < c.nearPlane || s.z > c.farPlane) return {false, "clip-unsupported"};
  }
 }
 return {true, diagnostic?"diagnostic-only-ready":"synthetic-untextured-adapter-ready"};
}
Result ReadyForAdapter(const Packet& p,std::uint64_t frame,const std::string& game) {
 return ReadyForScene(p,frame,game,false);
}
Result ReadyForDiagnosticAdapter(const Packet& p,std::uint64_t frame,const std::string& game,bool clips) {
 if(!clips)return {false,"diagnostic-clips-not-declared"};
 return ReadyForScene(p,frame,game,true);
}
std::vector<std::uint32_t> Triangles(const Mesh& m) {
 std::vector<std::uint32_t> out;
 if (m.indices.size() > Limits{}.indices || m.vertices.size() > Limits{}.vertices) throw std::invalid_argument("topology bounds");
 if (m.topology != Topology::Triangles && m.topology != Topology::Strip) throw std::invalid_argument("topology");
 if (m.indices.size() < 3 || (m.topology == Topology::Triangles && m.indices.size()%3)) throw std::invalid_argument("topology");
 for (auto i : m.indices) if (i >= m.vertices.size()) throw std::invalid_argument("index");
 for (std::size_t i = 0; i + 2 < m.indices.size(); i += m.topology == Topology::Strip ? 1 : 3) {
  auto a=m.indices[i], b=m.indices[i+1], c=m.indices[i+2];
  if (m.topology == Topology::Strip && (i&1)) std::swap(a,b);
  if (a!=b && b!=c && a!=c) out.insert(out.end(), {a,b,c});
 }
 return out;
}
FlatNormalMesh DeriveFlatNormals(const Mesh& source, Space space, const Limits& limits) {
 if(space!=Space::World && space!=Space::View) throw std::invalid_argument("normal coordinate domain");
 if(source.vertices.size()>limits.vertices || source.indices.size()>limits.indices)
  throw std::invalid_argument("normal input budget");
 if(source.topology!=Topology::Triangles && source.topology!=Topology::Strip)
  throw std::invalid_argument("normal topology");
 if(source.indices.size()<3 || (source.topology==Topology::Triangles && source.indices.size()%3))
  throw std::invalid_argument("normal topology");
 const auto faces=source.topology==Topology::Strip?source.indices.size()-2:source.indices.size()/3;
 // Worst-case split is checked before copying any mesh or allocating output.
 if(faces>limits.vertices/3 || faces>limits.indices/3
   || faces>limits.bytes/(3*(sizeof(Vertex)+sizeof(std::uint32_t))))
  throw std::invalid_argument("normal expansion budget");
 for(auto i:source.indices) if(i>=source.vertices.size() || !finite(source.vertices[i].position))
  throw std::invalid_argument("normal invalid vertex");
 FlatNormalMesh result;
 // Copy metadata without copying the potentially large original vertex arrays.
 result.mesh.id=source.id; result.mesh.frame=source.frame;
 result.mesh.material=source.material; result.mesh.texture=source.texture;
 result.mesh.transform=source.transform; result.mesh.topology=Topology::Triangles;
 result.mesh.vertices.reserve(faces*3);result.mesh.indices.reserve(faces*3);
 const auto step=source.topology==Topology::Strip?1u:3u;
 for(std::size_t i=0;i+2<source.indices.size();i+=step) {
  auto a=source.indices[i],b=source.indices[i+1],c=source.indices[i+2];
  if(source.topology==Topology::Strip && (i&1))std::swap(a,b);
  const auto p=source.vertices[a].position,q=source.vertices[b].position,r=source.vertices[c].position;
  const double ux=double(q.x)-p.x,uy=double(q.y)-p.y,uz=double(q.z)-p.z;
  const double vx=double(r.x)-p.x,vy=double(r.y)-p.y,vz=double(r.z)-p.z;
  const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
  const double length=std::sqrt(nx*nx+ny*ny+nz*nz);
  if(length==0) {++result.degenerateTriangles;continue;}
  const Vec3 normal{float(nx/length),float(ny/length),float(nz/length)};
  for(auto index:{a,b,c}) {
   auto vertex=source.vertices[index];vertex.normal=normal;
   result.mesh.indices.push_back(std::uint32_t(result.mesh.vertices.size()));
   result.mesh.vertices.push_back(vertex);
  }
 }
 return result;
}
Vec3 WorldPosition(const Mesh& m, const Vertex& v) {
 if (!m.transform) throw std::invalid_argument("unknown transform");
 const auto& a=*m.transform; const auto p=v.position;
 return {a[0]*p.x+a[1]*p.y+a[2]*p.z+a[3], a[4]*p.x+a[5]*p.y+a[6]*p.z+a[7], a[8]*p.x+a[9]*p.y+a[10]*p.z+a[11]};
}
Vec3 Project(const Camera& c, Vec3 p) {
 if (c.provenance == Provenance::Unknown || !validCamera(c) || !finite(p)) throw std::invalid_argument("projection");
 // Keep the public float contract, but avoid accumulating cancellation and
 // lens rounding in the CPU reference projection of distant/offscreen points.
 const double dx=double(p.x)-c.position.x,dy=double(p.y)-c.position.y,dz=double(p.z)-c.position.z;
 auto axis=[&](Vec3 a) {return dx*a.x+dy*a.y+dz*a.z;};
 const double x=axis(c.right),y=axis(c.up),z=axis(c.forward);
 if (z <= 0) throw std::invalid_argument("behind camera");
 const double t=std::tan(double(c.fovY)*3.14159265358979323846/360.0);
 return {float(0.5+x/(2*z*t*c.aspect)),float(0.5-y/(2*z*t)),float(z)};
}
Vec3 Unproject(const Camera& c, Vec3 p) {
 if (c.provenance == Provenance::Unknown || !validCamera(c) || !finite(p) || p.z <= 0) throw std::invalid_argument("unprojection");
 const double t=std::tan(double(c.fovY)*3.14159265358979323846/360.0);
 const double x=(double(p.x)-.5)*2*p.z*t*c.aspect,y=(.5-double(p.y))*2*p.z*t,z=p.z;
 return {float(c.position.x+x*c.right.x+y*c.up.x+z*c.forward.x),
  float(c.position.y+x*c.right.y+y*c.up.y+z*c.forward.y),
  float(c.position.z+x*c.right.z+y*c.up.z+z*c.forward.z)};
}
std::optional<float> DepthAt(const Packet& p, float x, float y) {
 if (!ReadyForAdapter(p,p.frame,p.game).ok) return {};
 std::optional<float> nearest;
 for (const auto& m:p.meshes) {
  auto indices=Triangles(m);
  for (std::size_t i=0;i<indices.size();i+=3) {
   const auto a=Project(p.camera,WorldPosition(m,m.vertices[indices[i]]));
   const auto b=Project(p.camera,WorldPosition(m,m.vertices[indices[i+1]]));
   const auto c=Project(p.camera,WorldPosition(m,m.vertices[indices[i+2]]));
   const float d=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
   if (std::abs(d)<1e-8f) continue;
   const float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/d;
   const float v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/d, w=1-u-v;
   if (u<0 || v<0 || w<0) continue;
   const float z=1/(u/a.z+v/b.z+w/c.z);
   if (!nearest || z<*nearest) nearest=z;
  }
 }
 return nearest;
}
Packet Synthetic(std::uint64_t frame,float cameraX) {
 Packet p; p.frame=frame; p.game="synthetic-overlap"; p.space=Space::World;
 p.camera.provenance=Provenance::Analytic; p.camera.position.x=cameraX;
 for (int n=1;n<=2;++n) {
  Mesh m; m.id=n; m.frame=frame; m.material=Mesh::Material{};
  m.transform=std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0};
  const float s=float(n);
  m.vertices={{{-s,-s,2*s},Vec3{0,0,-1},0,1},{{s,-s,2*s},Vec3{0,0,-1},1,1},{{0,s,2*s},Vec3{0,0,-1},.5f,0}};
  m.indices={0,1,2}; p.meshes.push_back(m);
 }
 return p;
}
}
