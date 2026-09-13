// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_view_scene.h"
// Call with the original four-vertex witnessed p fixture immediately after its first successful Build.
template<class Suite>
void TestRemakeSortedAlpha(Suite& suite,const flycast::rend::neural::PvrDecodedPacket& source) {
 using namespace flycast::rend::neural;
 auto p=source;p.draws.resize(1);auto& d=p.draws[0];d.list=2;d.ordinal=18;d.vertexRange=true;
 d.state.first=0;d.state.count=4;d.state.tsp.SrcInstr=4;d.state.tsp.DstInstr=5;
 d.state.tsp.SrcSelect=0;d.state.tsp.DstSelect=0;d.state.tsp.ShadInstr=3;d.state.tsp.FilterMode=0;d.state.tsp.FogCtrl=0;
 p.sortedOrderCaptured=true;p.indices={3,3,3,0,1,2,3,3,3,2,1,3};p.sortedTriangles={{18,3,3},{18,9,3}};
 RemakeViewScene out;std::string error;
 auto run=[&](const PvrDecodedPacket& q,bool alpha=true){return BuildRemakeViewScene(q,q.sourceProducer,q.frame,out,error,false,false,alpha,true);};
 suite.Expect(run(p)&&out.meshes.size()==1&&out.meshes[0].vertices.size()==6
  &&out.meshes[0].vertices[0].source.u==p.vertices[0].u&&out.meshes[0].vertices[3].source.u==p.vertices[2].u
  &&out.meshes[0].vertices[5].source.u==p.vertices[3].u,"sorted alpha consumes submitted triples in exact order");
 auto merged=p;auto other=d;other.ordinal=19;merged.draws.push_back(other);
 suite.Expect(run(merged)&&out.meshes.size()==1,"merged-away source owner does not duplicate sorted geometry");
 auto bad=p;bad.sortedOrderCaptured=false;
 suite.Expect(!run(bad)&&error=="view-sorted-provenance","sorted alpha requires captured provenance");
 bad=p;bad.sortedTriangles[0].count=4;
 suite.Expect(!run(bad)&&error=="view-sorted-range","sorted alpha rejects partial triples");
 bad=p;bad.indices[3]=UINT32_MAX;
 suite.Expect(!run(bad)&&error=="view-sorted-index","sorted list cannot use strip restart");
 bad=p;bad.sortedTriangles[0].polyIndex=99;
 suite.Expect(!run(bad)&&error=="view-sorted-owner","sorted alpha rejects nonexistent owner");
 bad=p;bad.draws[0].state.tsp.SrcInstr=1;
 suite.Expect(!run(bad)&&error=="view-no-supported-geometry","sorted alpha keeps source blend gate");
 suite.Expect(!run(p,false)&&error=="view-no-supported-geometry","sorted alpha stays opt-in");
}
