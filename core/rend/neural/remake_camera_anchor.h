// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_view_transport.h"
#include <map>
#include <set>

namespace flycast::rend::neural {
// A common observed source basis, not a recovered physical world or game camera.
// State is committed by the caller only with a successfully published packet.
class RemakeCameraAnchor {
 using Matrix=std::array<double,12>;
 using Point=std::array<std::uint32_t,4>;
 Matrix reference{};
 std::set<Point> referencePoints;
 ProducerIdentity first{},last{};
 std::uint64_t lastFrame=0;
 double projectionError=0;
 static bool rigid(const Matrix& m) {
  for(double x:m)if(!std::isfinite(x))return false;
  for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b) {
   double dot=0;for(unsigned k=0;k<3;++k)dot+=m[a*4+k]*m[b*4+k];
   if(std::abs(dot-(a==b?1.:0.))>1e-6)return false;
  }
  return true;
 }
 static Matrix inverseBasis(const Matrix& m) {
  const double a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],i=m[10];
  const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
  Matrix out{e*i-f*h,c*h-b*i,b*f-c*e,0,f*g-d*i,a*i-c*g,c*d-a*f,0,d*h-e*g,b*g-a*h,a*e-b*d,0};
  for(auto& x:out)x/=determinant;
  return out;
 }
public:
 void Reset(){*this={};}
 std::uint64_t ReferenceOrdinal()const{return first.ordinal;}
 double MaximumProjectionError()const{return projectionError;}
 bool Apply(const PvrDecodedPacket& source,const RemakeViewScene& scene,
  remake::Packet& packet,std::string& error) {
  const auto fail=[&](const char* why){error=why;return false;};
  const auto p=source.sourceProducer;
  if(!p.Available()||source.frame!=scene.frame||packet.frame!=source.frame
   ||scene.producer.epoch!=p.epoch||scene.producer.ordinal!=p.ordinal||scene.producer.cycle!=p.cycle
   ||packet.producer.epoch!=p.epoch||packet.producer.ordinal!=p.ordinal||packet.producer.cycle!=p.cycle
   ||source.game!="T1401N"||scene.game!=source.game||packet.game!=source.game)
   return fail("anchor-source-identity");
  if(source.sourceVertices.size()>SourceObservationBatch::capacity||scene.meshes.size()>128
   ||packet.diagnosticEmbeddingProvenance=="diagnostic-camera-embedded-anchor-not-world-reconstruction")
   return fail("anchor-input-bound-or-already-embedded");
  if(last.Available()&&(p.epoch!=last.epoch||p.ordinal<=last.ordinal||p.cycle<last.cycle||source.frame<=lastFrame))
   return fail("anchor-source-discontinuity");
  std::set<std::uint64_t> used;
  for(const auto& mesh:scene.meshes)for(const auto& v:mesh.vertices)
   if(!v.estimatedPosition&&v.transformSerial)used.insert(v.transformSerial);
  if(used.size()>4096)return fail("anchor-observation-bound");
  std::map<std::uint64_t,const SourceTransform*> transforms;
  for(const auto& v:source.sourceVertices)if(v.copy.xyzTransforms[0]) {
   const auto& t=*v.copy.xyzTransforms[0];
   if(used.count(t.serial))transforms.emplace(t.serial,&t);
  }
  if(transforms.size()!=used.size())return fail("anchor-observation-missing");
  Matrix current{};bool have=false;std::set<Point> points;
  for(const auto& entry:transforms) {
   const auto& t=*entry.second;
   if(t.pc!=0x8c03a9ea||t.input[3]!=1)return fail("anchor-source-domain");
   Matrix m{};
   const double scales[]={scene.focalX,-scene.focalY,1.};
   for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)m[r*4+c]=double(t.matrix[c*4+r])/scales[r];
   if(!rigid(m))return fail("anchor-nonrigid-source");
   if(have&&m!=current)return fail("anchor-ambiguous-source-basis");
   current=m;have=true;
   Point key{};std::memcpy(key.data(),t.input.data(),sizeof(key));points.insert(key);
  }
  if(!have||points.size()<16)return fail("anchor-insufficient-source-support");
  if(first.Available()) {
   std::size_t shared=0;for(const auto& v:points)shared+=referencePoints.count(v);
   if(shared<16||shared*2<(std::min)(points.size(),referencePoints.size()))
    return fail("anchor-source-support-changed");
  }
  Matrix delta{1,0,0,0,0,1,0,0,0,0,1,0};
  const auto referenceInverse=first.Available()?inverseBasis(reference):delta;
  if(first.Available())for(unsigned r=0;r<3;++r) {
   for(unsigned c=0;c<3;++c) {
    delta[r*4+c]=0;
    for(unsigned k=0;k<3;++k)delta[r*4+c]+=current[r*4+k]*referenceInverse[k*4+c];
   }
   delta[r*4+3]=current[r*4+3];
   for(unsigned k=0;k<3;++k)delta[r*4+3]-=delta[r*4+k]*reference[k*4+3];
  }
  if(!rigid(delta))return fail("anchor-relative-nonrigid");
  remake::Packet embedded=packet;
  // Invert the actual public float basis, not its approximately equal transpose.
  Matrix published=delta;for(auto& value:published)value=float(value);
  const auto inverse=inverseBasis(published);
  double cameraPosition[3]{};
  for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k)cameraPosition[r]-=inverse[r*4+k]*delta[k*4+3];
  embedded.camera.position={float(cameraPosition[0]),float(cameraPosition[1]),float(cameraPosition[2])};
  const auto embedPosition=[&](remake::Vec3 v) {
   const double xyz[]={v.x,v.y,v.z};
   double out[]={double(embedded.camera.position.x),double(embedded.camera.position.y),double(embedded.camera.position.z)};
   for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k)out[r]+=inverse[r*4+k]*xyz[k];
   return remake::Vec3{float(out[0]),float(out[1]),float(out[2])};
  };
  embedded.camera.right={float(delta[0]),float(delta[1]),float(delta[2])};
  embedded.camera.up={float(delta[4]),float(delta[5]),float(delta[6])};
  embedded.camera.forward={float(delta[8]),float(delta[9]),float(delta[10])};
  embedded.diagnosticOrigin=remake::Vec3{}; // Fixed first accepted source view.
  embedded.diagnosticEmbeddingProvenance="diagnostic-camera-embedded-anchor-not-world-reconstruction";
  embedded.omissions.push_back("observed common source anchor; static world identity and physical scale unproven");
  double maximum=0;
  for(auto& mesh:embedded.meshes)for(auto& v:mesh.vertices) {
   const auto input=v.position;
   const auto expected=remake::Project(packet.camera,v.position);
   v.position=embedPosition(v.position);
   if(v.normal) {
    const double n[]={v.normal->x,v.normal->y,v.normal->z};double out[3]{},length=0;
    for(unsigned r=0;r<3;++r){for(unsigned k=0;k<3;++k)out[r]+=published[k*4+r]*n[k];length+=out[r]*out[r];}
    length=std::sqrt(length);v.normal=remake::Vec3{float(out[0]/length),float(out[1]/length),float(out[2]/length)};
   }
   auto actual=remake::Project(embedded.camera,v.position);
   // A rounded world vertex can land just outside an unchanged clip plane.
   // Move its view-space intersection inward by representable depth steps,
   // along the same projection ray. Keep the original pixel/depth guard below.
   float targetDepth=input.z;
   bool adjusted=false;
   for(unsigned attempt=0;attempt<128;++attempt) {
    const bool outside=actual.z<embedded.camera.nearPlane||actual.z>embedded.camera.farPlane;
    const double rayError=(std::max)(std::abs(double(actual.x)-expected.x)*scene.size[0],
     std::abs(double(actual.y)-expected.y)*scene.size[1]);
    if(!outside&&(!adjusted||rayError<=.01))break;
    adjusted=true;
    // Recheck enclosure after every alignment: changing X/Y can change rounded Z.
    targetDepth=outside?std::nextafter(targetDepth,actual.z<embedded.camera.nearPlane?embedded.camera.farPlane:embedded.camera.nearPlane):actual.z;
    const double ratio=double(targetDepth)/input.z;
    v.position=embedPosition({float(input.x*ratio),float(input.y*ratio),targetDepth});
    actual=remake::Project(embedded.camera,v.position);
   }
   // Inverting in double still rounds each published world coordinate to a
   // float independently. For near/off-screen vertices that rounded point can
   // miss the ray even though a neighboring representable point satisfies the
   // original projection and depth contract. Search a bounded local lattice;
   // never alter the lens, clip planes, source identity or acceptance limits.
   const auto projectionError=[&](remake::Vec3 projected) {
    if(!std::isfinite(projected.x)||!std::isfinite(projected.y)||!std::isfinite(projected.z)
     ||projected.z<embedded.camera.nearPlane||projected.z>embedded.camera.farPlane
     ||std::abs(double(projected.z)-expected.z)>(std::max)(1e-5,std::abs(double(expected.z))*1e-5))
     return std::numeric_limits<double>::infinity();
    return (std::max)(std::abs(double(projected.x)-expected.x)*scene.size[0],
     std::abs(double(projected.y)-expected.y)*scene.size[1]);
   };
   double best=projectionError(actual);
   if(best>.01) {
    const auto center=v.position;
    std::array<std::array<float,9>,3> grid{};
    const float axes[]={center.x,center.y,center.z};
    for(unsigned axis=0;axis<3;++axis) {
     grid[axis][4]=axes[axis];
     for(unsigned distance=1;distance<=4;++distance) {
      grid[axis][4-distance]=std::nextafter(grid[axis][5-distance],-std::numeric_limits<float>::infinity());
      grid[axis][4+distance]=std::nextafter(grid[axis][3+distance],std::numeric_limits<float>::infinity());
     }
    }
    // Follow the same view ray when selecting a representable coordinate plane.
    // Independent ULP steps alone cannot follow a steep off-screen ray: one Z
    // step can require many X steps. Solve its depth in double, then round once.
    const double ray[]={double(input.x)/input.z,double(input.y)/input.z,1.};
    double direction[3]{};
    const double origin[]={embedded.camera.position.x,embedded.camera.position.y,embedded.camera.position.z};
    for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k)direction[r]+=inverse[r*4+k]*ray[k];
    for(unsigned axis=0;axis<3&&best>.01;++axis)for(unsigned plane=0;plane<9&&best>.01;++plane) {
     if(std::abs(direction[axis])<1e-12)continue;
     const double depth=(double(grid[axis][plane])-origin[axis])/direction[axis];
     if(depth<=0||std::abs(depth-input.z)>(std::max)(1e-5,std::abs(double(input.z))*1e-5))continue;
     const remake::Vec3 candidate{float(origin[0]+direction[0]*depth),float(origin[1]+direction[1]*depth),float(origin[2]+direction[2]*depth)};
     try {
      const auto projected=remake::Project(embedded.camera,candidate);
      const double error=projectionError(projected);
      if(error<best){best=error;v.position=candidate;actual=projected;}
     }catch(const std::invalid_argument&) {}
    }
    // Cover the existing allowed depth interval along that same ray. This is
    // bounded quantization search, not an enlarged error budget; the unchanged
    // post-projection check decides whether a representable point is acceptable.
    const double depthBudget=(std::max)(1e-5,std::abs(double(input.z))*1e-5);
    for(int step=-64;step<=64&&best>.01;++step) {
     const double depth=double(input.z)+depthBudget*step/64.;
     if(depth<embedded.camera.nearPlane||depth>embedded.camera.farPlane)continue;
     const remake::Vec3 candidate{float(origin[0]+direction[0]*depth),float(origin[1]+direction[1]*depth),float(origin[2]+direction[2]*depth)};
     try {
      const auto projected=remake::Project(embedded.camera,candidate);
      const double error=projectionError(projected);
      if(error<best){best=error;v.position=candidate;actual=projected;}
     }catch(const std::invalid_argument&) {}
    }
    for(int radius=1;radius<=4&&best>.01;++radius)
     for(int x=-radius;x<=radius&&best>.01;++x)
      for(int y=-radius;y<=radius&&best>.01;++y)
       for(int z=-radius;z<=radius&&best>.01;++z) {
        if((std::max)({std::abs(x),std::abs(y),std::abs(z)})!=radius)continue;
        const remake::Vec3 candidate{grid[0][4+x],grid[1][4+y],grid[2][4+z]};
        try {
         const auto projected=remake::Project(embedded.camera,candidate);
         const double error=projectionError(projected);
         if(error<best){best=error;v.position=candidate;actual=projected;}
        }catch(const std::invalid_argument&) { /* Invalid neighbor cannot be accepted. */ }
       }
   }
   if(actual.z<embedded.camera.nearPlane||actual.z>embedded.camera.farPlane)
    {error="anchor-enclosure-unrepresentable expected="+std::to_string(expected.z)+" actual="+std::to_string(actual.z);return false;}
   const double pixels=(std::max)(std::abs(double(actual.x)-expected.x)*scene.size[0],
    std::abs(double(actual.y)-expected.y)*scene.size[1]);
   if(!std::isfinite(pixels)||pixels>.01||!std::isfinite(actual.z)
    ||std::abs(double(actual.z)-expected.z)>(std::max)(1e-5,std::abs(double(expected.z))*1e-5))
    {error="anchor-projection-mismatch pixels="+std::to_string(pixels)+" expected="+std::to_string(expected.x)+","+std::to_string(expected.y)
     +" depth="+std::to_string(expected.z)+" actual_depth="+std::to_string(actual.z);return false;}
   maximum=(std::max)(maximum,pixels);
  }
  const auto checked=remake::ReadyForDiagnosticAdapter(embedded,embedded.frame,embedded.game,true);
  if(!checked.ok){error=checked.reason;return false;}
  if(!first.Available()){first=p;reference=current;referencePoints=std::move(points);}
  last=p;lastFrame=source.frame;projectionError=maximum;packet=std::move(embedded);error.clear();return true;
 }
};
}
