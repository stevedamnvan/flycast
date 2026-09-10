// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <vector>
#include <exception>
#include <thread>
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
public:
 // Per-frame record of vertices far outside the viewport accepted under the
 // on-screen effect bound (D-210). Pixels is the raw public-projection error
 // of the worst such vertex; effect is its bounded on-screen consequence.
 struct ProjectionReport {
  std::size_t offscreenAccepted=0;
  double maxPixels=0,maxEffect=0,maxTangential=0,maxDiagonals=0;
 };
 // Decomposition of a projection error for a vertex at `distance` pixels from
 // the viewport center. Any edge that can affect the image passes within one
 // viewport diagonal of the center, so a radial error (the depth rounding of a
 // near-plane vertex scales its projection about the center) moves the visible
 // crossing point by at most radial*diagonal/distance; a tangential error can
 // move it by the whole amount and therefore stays under the unchanged 0.01
 // pixel guard. Only vertices at least four diagonals out qualify.
 struct OffscreenEffect { double distance=0,diagonals=0,radial=0,tangential=0,effect=0; bool farOutside=false; };
 static OffscreenEffect OffscreenEffectOf(double expectedX,double expectedY,double actualX,double actualY,double width,double height) {
  OffscreenEffect o{};
  const double ex=(expectedX-.5)*width,ey=(expectedY-.5)*height;
  const double dx=(actualX-expectedX)*width,dy=(actualY-expectedY)*height;
  const double diagonal=std::hypot(width,height);
  o.distance=std::hypot(ex,ey);o.diagonals=o.distance/diagonal;
  if(!(o.distance>0)||!std::isfinite(dx)||!std::isfinite(dy)) {
   o.radial=o.tangential=o.effect=std::numeric_limits<double>::infinity();return o;
  }
  o.radial=(dx*ex+dy*ey)/o.distance;
  o.tangential=std::abs(dx*ey-dy*ex)/o.distance;
  o.effect=std::abs(o.radial)*diagonal/o.distance;
  o.farOutside=o.distance>=4*diagonal;
  return o;
 }
private:
 ProjectionReport projectionReport{};
 // The public projection in double for the decomposition only: the float
 // public projection remains the contract for the unchanged pixel guard.
 static std::array<double,3> projectDouble(const remake::Camera& c,const remake::Vec3& p) {
  const double dx=double(p.x)-c.position.x,dy=double(p.y)-c.position.y,dz=double(p.z)-c.position.z;
  const auto axis=[&](const remake::Vec3& a){return dx*a.x+dy*a.y+dz*a.z;};
  const double x=axis(c.right),y=axis(c.up),z=axis(c.forward);
  const double t=std::tan(double(c.fovY)*3.14159265358979323846/360.0);
  if(!(z>0))return {std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::quiet_NaN(),z};
  return {0.5+x/(2*z*t*c.aspect),0.5-y/(2*z*t),z};
 }
public:
 // Diagnostic counts recorded for every evaluated support set: shared points
 // with the fixed reference and with the last accepted set, plus the rigid
 // basis motion from each. It does not decide continuity and asserts no world
 // semantics; `rejected` marks the shared-support rejection.
 struct SupportReport {
  std::size_t points=0,reference=0,sharedReference=0,lastAccepted=0,sharedLast=0;
  double rotationFromReferenceDegrees=0,translationFromReference=0;
  double rotationFromLastDegrees=0,translationFromLast=0;
  std::size_t bases=0,movingPoints=0; // Distinct rigid bases this frame; points outside the chosen one.
  bool lineageSelected=false; // Basis chosen by exact point lineage with the last accepted set.
  std::uint64_t framesSinceLast=0; // Source frames between this and the last accepted export: the unit of the from-last motion.
  bool available=false,rejected=false;
 };
private:
 std::set<Point> lastPoints; // Last accepted support only; bounded like reference.
 Matrix lastBasis{};
 SupportReport report{};
 std::uint32_t generation=0;
 remake::Vec3 origin{}; // Diagnostic origin label of the current fixed view; generation0 is zero.
 Matrix priorReference{};bool priorAvailable=false;
 static std::array<double,3> viewPosition(const Matrix& base,const Matrix& current) {
  const auto inverse=inverseBasis(base);
  Matrix delta{};
  for(unsigned r=0;r<3;++r) {
   for(unsigned c=0;c<3;++c)for(unsigned k=0;k<3;++k)delta[r*4+c]+=current[r*4+k]*inverse[k*4+c];
   delta[r*4+3]=current[r*4+3];for(unsigned k=0;k<3;++k)delta[r*4+3]-=delta[r*4+k]*base[k*4+3];
  }
  const auto deltaInverse=inverseBasis(delta);
  std::array<double,3> position{};
  for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k)position[r]-=deltaInverse[r*4+k]*delta[k*4+3];
  return position;
 }
 static void relative(const Matrix& base,const Matrix& current,double& degrees,double& distance) {
  const auto inverse=inverseBasis(base);
  double rotation[9]{},translation[3]{};
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
   for(unsigned k=0;k<3;++k)rotation[r*3+c]+=current[r*4+k]*inverse[k*4+c];
  for(unsigned r=0;r<3;++r) {
   translation[r]=current[r*4+3];
   for(unsigned k=0;k<3;++k)translation[r]-=rotation[r*3+k]*base[k*4+3];
  }
  const double trace=rotation[0]+rotation[4]+rotation[8];
  degrees=std::acos((std::max)(-1.,(std::min)(1.,(trace-1)/2)))*180/3.14159265358979323846;
  distance=std::sqrt(translation[0]*translation[0]+translation[1]*translation[1]+translation[2]*translation[2]);
 }
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
 const SupportReport& LastSupportReport()const{return report;}
 std::uint32_t Generation()const{return generation;}
 remake::Vec3 Origin()const{return origin;}
 // Retire the fixed view after a rejected shared-support check. The next accepted
 // source becomes a new reference whose diagnostic origin is the retired view's
 // camera-relative position of that source: a distinct label that breaks strict
 // continuity for every consumer, not a recovered world relation.
 bool Reanchor() {
  if(!first.Available()||!report.rejected)return false;
  priorReference=reference;priorAvailable=true;
  first={};reference={};referencePoints.clear();lastPoints.clear();lastBasis={};
  projectionError=0;report={};projectionReport={};++generation;return true;
 }
 double MaximumProjectionError()const{return projectionError;}
 const ProjectionReport& LastProjectionReport()const{return projectionReport;}
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
  // Group exact input points by rigid basis. The dominant basis supplies the
  // observed view; other bases are moving objects whose vertices are already
  // in view space and embed exactly like the rest, but never anchor support.
  // A split without a clear majority stays ambiguous (LOG767).
  std::map<Matrix,std::set<Point>> bases;
  for(const auto& entry:transforms) {
   const auto& t=*entry.second;
   if(t.pc!=0x8c03a9ea||t.input[3]!=1)return fail("anchor-source-domain");
   Matrix m{};
   const double scales[]={scene.focalX,-scene.focalY,1.};
   for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)m[r*4+c]=double(t.matrix[c*4+r])/scales[r];
   if(!rigid(m))return fail("anchor-nonrigid-source");
   if(bases.size()>=64&&!bases.count(m))return fail("anchor-ambiguous-source-basis");
   Point key{};std::memcpy(key.data(),t.input.data(),sizeof(key));bases[m].insert(key);
  }
  if(bases.empty())return fail("anchor-insufficient-source-support");
  std::size_t runnerUp=0,total=0;
  std::map<Matrix,std::set<Point>>::const_iterator dominant=bases.end();
  for(auto it=bases.begin();it!=bases.end();++it) {
   total+=it->second.size();
   if(dominant==bases.end()||it->second.size()>dominant->second.size()) {
    if(dominant!=bases.end())runnerUp=(std::max)(runnerUp,dominant->second.size());
    dominant=it;
   } else runnerUp=(std::max)(runnerUp,it->second.size());
  }
  // Lineage first: with several bases, keep the one whose exact points continue
  // the last accepted support (same shared-support thresholds). Two large rigid
  // groups then stay consistently anchored instead of flipping or rejecting.
  // Without lineage the size majority decides; an even split stays ambiguous.
  std::map<Matrix,std::set<Point>>::const_iterator chosen=bases.end();
  std::size_t chosenShared=0;
  if(bases.size()>1&&!lastPoints.empty())
   for(auto it=bases.begin();it!=bases.end();++it) {
    std::size_t shared=0;for(const auto& v:it->second)shared+=lastPoints.count(v);
    if(shared>=16&&shared*2>=(std::min)(it->second.size(),lastPoints.size())&&shared>chosenShared){chosen=it;chosenShared=shared;}
   }
  const bool lineageSelected=chosen!=bases.end();
  if(!lineageSelected) {
   if(bases.size()>1&&dominant->second.size()<2*runnerUp)return fail("anchor-ambiguous-source-basis");
   chosen=dominant;
  }
  const Matrix current=chosen->first;const bool have=true;
  std::set<Point> points=chosen->second;
  const std::size_t movingPoints=total-points.size();
  if(!have||points.size()<16)return fail("anchor-insufficient-source-support");
  SupportReport support{};
  support.bases=bases.size();support.movingPoints=movingPoints;support.lineageSelected=lineageSelected;
  if(first.Available()) {
   std::size_t shared=0;for(const auto& v:points)shared+=referencePoints.count(v);
   support.available=true;
   support.points=points.size();support.reference=referencePoints.size();support.sharedReference=shared;
   support.lastAccepted=lastPoints.size();
   support.framesSinceLast=source.frame>lastFrame?source.frame-lastFrame:0;
   for(const auto& v:points)support.sharedLast+=lastPoints.count(v);
   relative(reference,current,support.rotationFromReferenceDegrees,support.translationFromReference);
   relative(lastBasis,current,support.rotationFromLastDegrees,support.translationFromLast);
   // Source-qualified lineage: exact object-space input points shared with the
   // last accepted set, chained back to the fixed reference. The first-view
   // snapshot alone rejected ordinary visibility drift inside one arena
   // (LOG766). Thresholds are unchanged; the basis stays relative to the
   // reference, so the coordinate relation is still the same rigid chain.
   if(support.sharedLast<16||support.sharedLast*2<(std::min)(points.size(),lastPoints.size())) {
    support.rejected=true;report=support;
    return fail("anchor-source-support-changed");
   }
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
  // Fixed first accepted source view. A re-anchored generation labels its new
  // fixed view with the retired view's camera-relative position of this source.
  remake::Vec3 candidateOrigin=origin;
  if(!first.Available()&&priorAvailable) {
   const auto position=viewPosition(priorReference,current);
   candidateOrigin={float(position[0]),float(position[1]),float(position[2])};
   if(!std::isfinite(candidateOrigin.x)||!std::isfinite(candidateOrigin.y)||!std::isfinite(candidateOrigin.z))
    return fail("anchor-generation-origin");
   if(candidateOrigin.x==origin.x&&candidateOrigin.y==origin.y&&candidateOrigin.z==origin.z)
    candidateOrigin.x=std::nextafter(candidateOrigin.x,std::numeric_limits<float>::infinity());
  }
  embedded.diagnosticOrigin=candidateOrigin;
  embedded.diagnosticEmbeddingProvenance="diagnostic-camera-embedded-anchor-not-world-reconstruction";
  embedded.omissions.push_back("observed common source anchor; static world identity and physical scale unproven");
  if(generation)embedded.omissions.push_back("re-anchored generation "+std::to_string(generation)
   +" after rejected source support; origin is the retired view's camera-relative position, not world identity");
  if(bases.size()>1)embedded.omissions.push_back(std::to_string(movingPoints)+" source points under "+std::to_string(bases.size()-1)
   +" additional rigid bases (moving objects) are embedded by view position only and excluded from anchor support");
  double maximum=0;ProjectionReport projection{};
  // D-218: vertices embed independently (identical per-vertex arithmetic;
  // the reductions are order-free maxima and counts, and the first failing
  // vertex in packet order still decides the error), so chunks run on
  // worker threads.
  const auto embedVertex=[&](remake::Vertex& v,double& maximum,ProjectionReport& projection,std::string& error)->bool {
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
   const bool depthOk=std::isfinite(actual.z)&&std::abs(double(actual.z)-expected.z)<=(std::max)(1e-5,std::abs(double(expected.z))*1e-5);
   if(std::isfinite(pixels)&&pixels<=.01&&depthOk){maximum=(std::max)(maximum,pixels);return true;}
   // D-210: a vertex far outside the viewport whose remaining error is radial
   // (near-plane depth rounding scaling the projection about the center) is
   // accepted when its bounded on-screen effect and its tangential component
   // both satisfy the unchanged 0.01 pixel guard. Measured in double so the
   // float public projection's own quantization far off-screen is not counted.
   const auto expectedD=projectDouble(packet.camera,input);
   const auto actualD=projectDouble(embedded.camera,v.position);
   const auto off=OffscreenEffectOf(expectedD[0],expectedD[1],actualD[0],actualD[1],double(scene.size[0]),double(scene.size[1]));
   if(depthOk&&std::isfinite(pixels)&&off.farOutside&&off.tangential<=.01&&off.effect<=.01) {
    ++projection.offscreenAccepted;
    projection.maxPixels=(std::max)(projection.maxPixels,pixels);
    projection.maxEffect=(std::max)(projection.maxEffect,off.effect);
    projection.maxTangential=(std::max)(projection.maxTangential,off.tangential);
    projection.maxDiagonals=(std::max)(projection.maxDiagonals,off.diagonals);
    return true;
   }
   error="anchor-projection-mismatch pixels="+std::to_string(pixels)+" expected="+std::to_string(expected.x)+","+std::to_string(expected.y)
    +" actual="+std::to_string(actual.x)+","+std::to_string(actual.y)+" diagonals="+std::to_string(off.diagonals)
    +" radial="+std::to_string(off.radial)+" tangential="+std::to_string(off.tangential)+" effect="+std::to_string(off.effect)
    +" depth="+std::to_string(expected.z)+" actual_depth="+std::to_string(actual.z);
   return false;

  };
  {
   std::vector<remake::Vertex*> vertexList;
   for(auto& mesh:embedded.meshes)for(auto& v:mesh.vertices)vertexList.push_back(&v);
   struct Chunk {double maximum=0;ProjectionReport projection{};std::string error;std::size_t failed=SIZE_MAX;std::exception_ptr thrown;};
   const std::size_t total=vertexList.size();
   const unsigned hardware=std::thread::hardware_concurrency();
   const unsigned workers=total<1024?1u:(std::min)(6u,(std::max)(1u,hardware/2));
   std::vector<Chunk> chunks(workers);
   const auto runChunk=[&](unsigned c) {
    auto& r=chunks[c];const std::size_t begin=total*c/workers,end=total*(c+1)/workers;
    try {
     for(std::size_t i=begin;i<end;++i)if(!embedVertex(*vertexList[i],r.maximum,r.projection,r.error)){r.failed=i;return;}
    }catch(...){r.thrown=std::current_exception();r.failed=begin;}
   };
   std::vector<std::thread> threads;
   for(unsigned c=1;c<workers;++c)threads.emplace_back(runChunk,c);
   runChunk(0);
   for(auto& t:threads)t.join();
   for(const auto& r:chunks) {
    if(r.thrown)std::rethrow_exception(r.thrown);
    if(r.failed!=SIZE_MAX){error=r.error;return false;}
    maximum=(std::max)(maximum,r.maximum);
    projection.offscreenAccepted+=r.projection.offscreenAccepted;
    projection.maxPixels=(std::max)(projection.maxPixels,r.projection.maxPixels);
    projection.maxEffect=(std::max)(projection.maxEffect,r.projection.maxEffect);
    projection.maxTangential=(std::max)(projection.maxTangential,r.projection.maxTangential);
    projection.maxDiagonals=(std::max)(projection.maxDiagonals,r.projection.maxDiagonals);
   }
  }
  if(projection.offscreenAccepted)embedded.omissions.push_back(std::to_string(projection.offscreenAccepted)
   +" vertices at least four viewport diagonals outside the viewport accepted under the on-screen effect bound (raw public-projection error up to "
   +std::to_string(projection.maxPixels)+" pixels, bounded on-screen effect up to "+std::to_string(projection.maxEffect)+" pixels)");
  const auto checked=remake::ReadyForDiagnosticAdapter(embedded,embedded.frame,embedded.game,true);
  if(!checked.ok){error=checked.reason;return false;}
  if(!support.available){support.bases=bases.size();support.movingPoints=movingPoints;support.lineageSelected=lineageSelected;}
  lastPoints=points;lastBasis=current;report=support;
  if(!first.Available()){first=p;reference=current;referencePoints=std::move(points);origin=candidateOrigin;}
  last=p;lastFrame=source.frame;projectionError=maximum;projectionReport=projection;packet=std::move(embedded);error.clear();return true;
 }
};
}
