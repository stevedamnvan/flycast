// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_extent.h"
#include "pvr_scene_capture.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace flycast::rend::neural {
// Deliberately camera-relative diagnostic geometry, not recovered world space.
// Original PVR positions/attributes and transform W remain available unchanged.
struct RemakeViewVertex {
 Vertex source{};
 std::array<float,3> position{},normal{};
 std::uint32_t sourceVertex=0;
 std::uint64_t transformSerial=0;
 float transformW=0;
	bool estimatedPosition=false;
};
struct RemakeViewMesh {
 PvrCapturedDraw sourceDraw;
 std::optional<std::uint8_t> sourceAlphaReference;
 bool sourceAlphaBlend=false;
 std::vector<RemakeViewVertex> vertices; // Expanded triangle list; flat normals.
};
// D-223 (experimental, default off; FLYCAST_REMAKE_SMOOTH_NORMALS=1): the
// source stream carries pre-lit colors and no normals, so the expansion above
// assigns flat face normals and the consumer lights every facet as a plane.
// Smoothing averages, per source vertex, the face normals of the triangles that
// share it and lie within a 60 degree crease of the facet being shaded; sharp
// edges keep their own normal. A shading choice on the exported geometry, not
// recovered source shading or new geometry.
// Level 1 groups by source vertex index (one strip's shared vertices). Level 2
// (FLYCAST_REMAKE_SMOOTH_NORMALS=2, D-225) additionally welds, within the same
// source draw only, vertices whose source position, texture coordinate and
// base colour are bit-identical: the same logical vertex the game resubmitted
// for a neighbouring strip. Coincident positions with different attributes
// (a seam between two surfaces) are never welded; the crease still applies.
inline int RemakeSmoothNormalsLevel() {
 static const int level=[]{const char* v=std::getenv("FLYCAST_REMAKE_SMOOTH_NORMALS");return v&&std::strcmp(v,"2")==0?2:v&&std::strcmp(v,"1")==0?1:0;}();
 return level;
}
inline bool RemakeSmoothNormalsEnabled() { return RemakeSmoothNormalsLevel()!=0; }
inline void SmoothRemakeViewNormals(RemakeViewMesh& mesh,int level=RemakeSmoothNormalsLevel()) {
 constexpr float creaseCosine=0.5f; // 60 degrees.
 // Thread-owned scratch retains its high-water capacity across meshes/frames.
 // Contents never escape and every used element is rewritten before reading.
 // Separate threads cannot share scratch; thread exit releases the storage.
 static thread_local std::vector<std::pair<std::uint64_t,std::uint32_t>> order;
 static thread_local std::vector<std::pair<std::array<std::uint32_t,6>,std::uint32_t>> keyed;
 static thread_local std::vector<std::array<float,3>> smoothed;
 order.clear();order.reserve(mesh.vertices.size());
 if(level>=2) {
  // Sort exact attribute identity followed by source index. Replacing the
  // sorted key with a monotonic group preserves the final pair ordering.
  keyed.clear();keyed.reserve(mesh.vertices.size());
  for(std::uint32_t i=0;i<mesh.vertices.size();++i) {
   const auto& v=mesh.vertices[i].source;std::array<std::uint32_t,6> key{};
   std::memcpy(&key[0],&v.x,4);std::memcpy(&key[1],&v.y,4);std::memcpy(&key[2],&v.z,4);
   std::memcpy(&key[3],&v.u,4);std::memcpy(&key[4],&v.v,4);std::memcpy(&key[5],v.col,4);
   keyed.emplace_back(key,i);
  }
  std::sort(keyed.begin(),keyed.end());
  std::uint64_t group=0;
  for(std::size_t i=0;i<keyed.size();++i){if(i&&keyed[i].first!=keyed[i-1].first)++group;order.emplace_back(group,keyed[i].second);}
 } else {
  for(std::uint32_t i=0;i<mesh.vertices.size();++i)order.emplace_back(mesh.vertices[i].sourceVertex,i);
  std::sort(order.begin(),order.end());
 }
 smoothed.resize(mesh.vertices.size());
 for(std::size_t begin=0;begin<order.size();) {
  std::size_t end=begin;while(end<order.size()&&order[end].first==order[begin].first)++end;
  for(std::size_t i=begin;i<end;++i) {
   const auto& n=mesh.vertices[order[i].second].normal;float sx=0,sy=0,sz=0;
   for(std::size_t j=begin;j<end;++j) {
    const auto& m=mesh.vertices[order[j].second].normal;
    if(n[0]*m[0]+n[1]*m[1]+n[2]*m[2]<creaseCosine)continue;
    sx+=m[0];sy+=m[1];sz+=m[2];
   }
   const float length=std::sqrt(sx*sx+sy*sy+sz*sz);
   smoothed[order[i].second]=length>1e-12f?std::array<float,3>{sx/length,sy/length,sz/length}:n;
  }
  begin=end;
 }
 for(std::uint32_t i=0;i<mesh.vertices.size();++i)mesh.vertices[i].normal=smoothed[i];
}
struct RemakeViewScene {
 std::uint64_t frame=0;
 ProducerIdentity producer;
 std::string game,gitSha;
 // LOG511 measured lens under an orthogonal, unit-scale source-basis assumption.
 double focalX=614.714447081469,focalY=565.5372405666603;
 std::array<unsigned,2> size{640,480};
 std::vector<RemakeViewMesh> meshes;
 std::size_t omittedDraws=0,rejectedVertices=0,degenerateTriangles=0;
 double maximumProjectionError=0;
	std::size_t estimatedVertices=0;
 const char* scope="observed-camera-relative-experiment-not-world-reconstruction";
};

// CPU-only conversion of an owned live snapshot. No capture-file/texture reads,
// no renderer mutation and no temporal history. Output changes only on success.
// The empirical tolerance admits an experiment; it does NOT close strict parity.
inline bool BuildRemakeViewScene(const PvrDecodedPacket& packet,
 const ProducerIdentity& expectedProducer,std::uint64_t expectedFrame,
 RemakeViewScene& output,std::string& error,bool estimateUntraced=false,bool includeCutouts=false,bool includeAlpha=false,bool deferSmoothing=false) {
 const auto fail=[&](const char* why){error=why;return false;};
 if(!expectedProducer.Available() || packet.frame!=expectedFrame || !expectedFrame
  || packet.sourceProducer.epoch!=expectedProducer.epoch
  || packet.sourceProducer.ordinal!=expectedProducer.ordinal
  || packet.sourceProducer.cycle!=expectedProducer.cycle)return fail("view-source-identity");
 // The backing render target can be the explicit doubled output extent;
 // source PVR vertices/lens remain in native pixels. The exact native viewport
 // certificate below must still pass: output scaling is not a new camera.
 if(packet.game!="T1401N" || (packet.framebufferSize!=std::array<std::uint32_t,2>{640,480}
  &&(!SelectedRemakeExtent().Valid()||packet.framebufferSize!=std::array<std::uint32_t,2>{RemakeWidth(),RemakeHeight()})))
  return fail("view-title-or-viewport-unsupported");
 const std::array<float,16> expectedViewport{2.f/640,0,0,0,0,-2.f/480,0,0,0,0,1,0,-1,1,0,1};
 for(unsigned i=0;i<16;++i)if(!std::isfinite(packet.viewport[i])
  || std::abs(packet.viewport[i]-expectedViewport[i])>1e-7f)return fail("view-viewport-unsupported");
 if(packet.vertices.size()>65536 || packet.indices.size()>262144 || packet.draws.size()>8192
  || packet.sourceVertices.size()>SourceObservationBatch::capacity)return fail("view-input-bound");
 RemakeViewScene result;result.frame=packet.frame;result.producer=packet.sourceProducer;
 result.game=packet.game;result.gitSha=packet.gitSha;
 std::size_t copyMatches=0,transformMatches=0,lensMatches=0;
 std::size_t uniqueCopies=0,unchangedCopies=0,xyzCopies=0;
 double firstFx=0,firstFy=0,firstResidual=0,firstDepth=0,firstPredictedDepth=0;
 std::vector<std::optional<RemakeViewVertex>> converted(packet.vertices.size());
 std::vector<unsigned char> seen(packet.vertices.size(),0);
 for(const auto& observation:packet.sourceVertices) {
  const auto index=observation.copy.decodedVertex;
  if(index>=seen.size())return fail("view-source-vertex-bound");
  if(seen[index]<2)++seen[index];
 }
 for(const auto& observation:packet.sourceVertices) {
  const auto& copy=observation.copy;const auto index=copy.decodedVertex;
  const auto& v=packet.vertices[index];const auto& xyz=copy.xyzTransforms;
  uniqueCopies+=seen[index]==1;
  unchangedCopies+=copy.before==copy.after;
  xyzCopies+=bool(xyz[0]&&xyz[1]&&xyz[2]);
  bool valid=seen[index]==1 && copy.before==copy.after
   && std::memcmp(copy.after.data()+1,&v.x,3*sizeof(float))==0;
  valid=valid&&xyz[0]&&xyz[1]&&xyz[2];
  if(!valid){++result.rejectedVertices;continue;}
  ++copyMatches;
  const auto& t=*xyz[0];
  const auto same=[&](const SourceTransform& other){return t.serial==other.serial && t.pc==other.pc
   && t.input==other.input && t.matrix==other.matrix && t.output==other.output;};
  valid=t.serial && t.pc==0x8c03a9ea && same(*xyz[1]) && same(*xyz[2]);
  for(float value:t.input)valid=valid&&std::isfinite(value);
  for(float value:t.output)valid=valid&&std::isfinite(value);
  for(float value:t.matrix)valid=valid&&std::isfinite(value);
  valid=valid && std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)
   && std::isfinite(v.u)&&std::isfinite(v.v) && t.output[2]>0;
  double rows[3][3]{},norms[3]{};
  for(unsigned r=0;r<3;++r) {
   for(unsigned c=0;c<3;++c){rows[r][c]=t.matrix[c*4+r];norms[r]+=rows[r][c]*rows[r][c];}
   norms[r]=std::sqrt(norms[r]);valid=valid&&norms[r]>0&&std::isfinite(norms[r]);
  }
  if(valid) {
   ++transformMatches;
   if(!firstFx){firstFx=norms[0]/norms[2];firstFy=norms[1]/norms[2];}
   valid=std::abs(norms[2]-1)<1e-4 && std::abs(norms[0]/norms[2]-result.focalX)<.001
    && std::abs(norms[1]/norms[2]-result.focalY)<.001;
   for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b) {
    double dot=0;for(unsigned c=0;c<3;++c)dot+=rows[a][c]*rows[b][c];
    valid=valid&&std::abs(dot/(norms[a]*norms[b]))<1e-5;
   }
  }
  double residual=0;
  if(valid) {
   ++lensMatches;
   residual=(std::max)(std::abs(double(t.output[0])/t.output[2]+320-v.x),
    std::abs(double(t.output[1])/t.output[2]+240-v.y));
   const double predictedDepth=.95/double(t.output[2]);
   if(lensMatches==1){firstResidual=residual;firstDepth=v.z;firstPredictedDepth=predictedDepth;}
   valid=residual<=.01 && std::abs(predictedDepth-v.z)<=(std::max)(1e-7,std::abs(predictedDepth)*1e-5);
  }
  if(!valid){++result.rejectedVertices;continue;}
  RemakeViewVertex out;out.source=v;out.sourceVertex=index;
  out.position={float(t.output[0]/result.focalX),float(-t.output[1]/result.focalY),t.output[2]};
  out.transformSerial=t.serial;out.transformW=t.output[3];
  converted[index]=out;result.maximumProjectionError=(std::max)(result.maximumProjectionError,residual);
 }
	if(estimateUntraced) {
		// Explicit alternative camera-relative embedding, not recovered transforms.
		// Retain a current observed anchor before applying its measured lens/scale.
		std::size_t anchors=0;for(const auto& v:converted)if(v)++anchors;
		if(anchors<3) {
			error="estimated-view-missing-observed-anchor copies="+std::to_string(copyMatches)
			 +" observations="+std::to_string(packet.sourceVertices.size())
			 +" unique="+std::to_string(uniqueCopies)+" unchanged="+std::to_string(unchangedCopies)
			 +" xyz="+std::to_string(xyzCopies)
			 +" transforms="+std::to_string(transformMatches)+" lens="+std::to_string(lensMatches)
			 +" first_fx="+std::to_string(firstFx)+" first_fy="+std::to_string(firstFy)
			 +" first_residual="+std::to_string(firstResidual)
			 +" first_depth="+std::to_string(firstDepth)
			 +" predicted_depth="+std::to_string(firstPredictedDepth);return false;
		}
		result.scope="mixed-observed-and-projected-depth-estimate-not-world-reconstruction";
		for(std::size_t i=0;i<packet.vertices.size();++i)if(!converted[i]) {
			const auto& v=packet.vertices[i];
			if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.z<=0
				||!std::isfinite(v.u)||!std::isfinite(v.v))continue;
			const double depth=.95/double(v.z);
			// Preserve crossing primitives for the downstream triangle clipper.
			// Rejecting one outside vertex here discards the entire source draw.
			// The supplied .1..2501 enclosure is still enforced after expansion.
			RemakeViewVertex out;out.source=v;out.sourceVertex=unsigned(i);out.estimatedPosition=true;
			out.position={float((v.x-320)*depth/result.focalX),float(-(v.y-240)*depth/result.focalY),float(depth)};
			if(!std::isfinite(out.position[0])||!std::isfinite(out.position[1])||!std::isfinite(out.position[2]))continue;
			converted[i]=out;++result.estimatedVertices;
		}
	}
 std::size_t totalVertices=0,totalReferences=0;
 for(const auto& draw:packet.draws) {
  if(!draw.state.count)continue;
  if(draw.protectedOverlay){++result.omittedDraws;continue;}
  const auto& state=draw.state;
  const bool cutout=includeCutouts&&draw.list==1;
  const bool alpha=includeAlpha&&draw.list==2&&state.tsp.SrcInstr==4&&state.tsp.DstInstr==5
   &&!state.tsp.SrcSelect&&!state.tsp.DstSelect&&state.tsp.ShadInstr==3&&state.tsp.FilterMode<=1
   &&state.tsp.FogCtrl!=3&&state.tcw.PixelFmt!=PixelBumpMap;
  if(cutout&&!packet.sourceAlphaReference)return fail("view-cutout-source-alpha-missing");
  if(cutout&&(state.tsp.ShadInstr!=3||state.tsp.FilterMode>1||state.tsp.FogCtrl==3||state.tcw.PixelFmt==PixelBumpMap)) {++result.omittedDraws;continue;}
  if((draw.list!=0&&!cutout&&!alpha) || draw.vertexRange || state.isNaomi2() || state.pcw.Volume
   || state.texture || state.texture1 || draw.texture1 || (state.isp.ZWriteDis&&!alpha)
   || (state.pcw.Texture && !draw.texture)) {++result.omittedDraws;continue;}
  if(state.first>packet.indices.size() || state.count>packet.indices.size()-state.first)
   return fail("view-draw-range");
  if(state.count>262144-totalReferences)return fail("view-reference-work-bound");
  totalReferences+=state.count;
  bool complete=true;
  for(std::size_t i=state.first;i<std::size_t(state.first)+state.count;++i) {
   const auto vertex=packet.indices[i];if(vertex==UINT32_MAX)continue;
   if(vertex>=converted.size())return fail("view-index-bound");
   complete=complete&&converted[vertex].has_value();
  }
  if(!complete){++result.omittedDraws;continue;}
  RemakeViewMesh mesh;mesh.sourceDraw=draw;if(cutout)mesh.sourceAlphaReference=packet.sourceAlphaReference;
  mesh.sourceAlphaBlend=alpha;
  std::uint32_t previous[2]{};std::size_t stripLength=0;
  for(std::size_t i=state.first;i<std::size_t(state.first)+state.count;++i) {
   const auto vertex=packet.indices[i];
   if(vertex==UINT32_MAX){stripLength=0;continue;}
   if(stripLength>=2) {
    const auto a=previous[(stripLength&1)?1:0],b=previous[(stripLength&1)?0:1];
    const auto& p=converted[a]->position;const auto& q=converted[b]->position;const auto& r=converted[vertex]->position;
    const double x=double(q[0])-p[0],y=double(q[1])-p[1],z=double(q[2])-p[2],u=double(r[0])-p[0],v=double(r[1])-p[1],w=double(r[2])-p[2];
    const double nx=y*w-z*v,ny=z*u-x*w,nz=x*v-y*u,length=std::sqrt(nx*nx+ny*ny+nz*nz);
    if(a==b||a==vertex||b==vertex||length<1e-12)++result.degenerateTriangles;
    else {
     if(totalVertices>65536-3)return fail("view-expanded-vertex-bound");
     const std::array<float,3> normal{float(nx/length),float(ny/length),float(nz/length)};
     for(auto id:{a,b,vertex}){auto out=*converted[id];out.normal=normal;mesh.vertices.push_back(out);}
     totalVertices+=3;
    }
   }
   previous[0]=previous[1];previous[1]=vertex;++stripLength;
  }
  if(mesh.vertices.empty()){++result.omittedDraws;continue;}
  if(!deferSmoothing&&RemakeSmoothNormalsEnabled())SmoothRemakeViewNormals(mesh);
  if(result.meshes.size()>=128)return fail("view-mesh-bound");
  result.meshes.push_back(std::move(mesh));
 }
 if(result.meshes.empty())return fail("view-no-supported-geometry");
 output=std::move(result);error.clear();return true;
}
}
