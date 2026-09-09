// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "pvr_scene_capture.h"
#include <cmath>
#include <cstring>

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
 RemakeViewScene& output,std::string& error,bool estimateUntraced=false,bool includeCutouts=false,bool includeAlpha=false) {
 const auto fail=[&](const char* why){error=why;return false;};
 if(!expectedProducer.Available() || packet.frame!=expectedFrame || !expectedFrame
  || packet.sourceProducer.epoch!=expectedProducer.epoch
  || packet.sourceProducer.ordinal!=expectedProducer.ordinal
  || packet.sourceProducer.cycle!=expectedProducer.cycle)return fail("view-source-identity");
 if(packet.game!="T1401N" || packet.framebufferSize!=std::array<std::uint32_t,2>{640,480})
  return fail("view-title-or-viewport-unsupported");
 const std::array<float,16> expectedViewport{2.f/640,0,0,0,0,-2.f/480,0,0,0,0,1,0,-1,1,0,1};
 for(unsigned i=0;i<16;++i)if(!std::isfinite(packet.viewport[i])
  || std::abs(packet.viewport[i]-expectedViewport[i])>1e-7f)return fail("view-viewport-unsupported");
 if(packet.vertices.size()>65536 || packet.indices.size()>262144 || packet.draws.size()>8192
  || packet.sourceVertices.size()>SourceObservationBatch::capacity)return fail("view-input-bound");
 RemakeViewScene result;result.frame=packet.frame;result.producer=packet.sourceProducer;
 result.game=packet.game;result.gitSha=packet.gitSha;
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
  bool valid=seen[index]==1 && copy.before==copy.after
   && std::memcmp(copy.after.data()+1,&v.x,3*sizeof(float))==0;
  valid=valid&&xyz[0]&&xyz[1]&&xyz[2];
  if(!valid){++result.rejectedVertices;continue;}
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
   valid=std::abs(norms[2]-1)<1e-4 && std::abs(norms[0]/norms[2]-result.focalX)<.001
    && std::abs(norms[1]/norms[2]-result.focalY)<.001;
   for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b) {
    double dot=0;for(unsigned c=0;c<3;++c)dot+=rows[a][c]*rows[b][c];
    valid=valid&&std::abs(dot/(norms[a]*norms[b]))<1e-5;
   }
  }
  double residual=0;
  if(valid) {
   residual=(std::max)(std::abs(double(t.output[0])/t.output[2]+320-v.x),
    std::abs(double(t.output[1])/t.output[2]+240-v.y));
   const double predictedDepth=.95/double(t.output[2]);
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
		if(anchors<3)return fail("estimated-view-missing-observed-anchor");
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
  if(result.meshes.size()>=128)return fail("view-mesh-bound");
  result.meshes.push_back(std::move(mesh));
 }
 if(result.meshes.empty())return fail("view-no-supported-geometry");
 output=std::move(result);error.clear();return true;
}
}
