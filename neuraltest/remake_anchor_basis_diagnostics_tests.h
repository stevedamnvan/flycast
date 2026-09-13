// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_camera_anchor.h"
// Invoke with the existing valid anchor p/view/packet fixture, before mutation.
template<class Suite, class Source, class View, class Packet>
void TestRemakeAnchorBasisDiagnostics(Suite& suite,const Source& source,const View& view,const Packet& packet) {
 using flycast::rend::neural::RemakeCameraAnchor;
 auto check=[&](unsigned count,bool split) {
  auto observed=source;auto scene=view;auto output=packet;
  observed.sourceVertices.clear();scene.meshes[0].vertices.clear();
  for(unsigned i=0;i<count;++i) {
   auto witness=source.sourceVertices[0];
   for(auto& t:witness.copy.xyzTransforms) {
    t->serial=i+1;t->input={float(i%8),float(i/8),10,1};
    t->matrix[12]+=float(view.focalX)*(split?(i>=count/2?1.f:0.f):float(i));
   }
   observed.sourceVertices.push_back(witness);
   auto vertex=view.meshes[0].vertices[0];vertex.transformSerial=i+1;scene.meshes[0].vertices.push_back(vertex);
  }
  RemakeCameraAnchor anchor;std::string error;
  const bool accepted=anchor.Apply(observed,scene,output,error);
  const auto& detail=anchor.LastBasisReport();
  suite.Expect(!accepted&&error=="anchor-ambiguous-source-basis","basis diagnostics preserve ambiguity rejection string");
  suite.Expect(!anchor.LastSupportReport().rejected&&!anchor.Reanchor(),"basis diagnostics do not authorize reanchor");
  if(split) suite.Expect(detail.rejection==RemakeCameraAnchor::BasisReport::Rejection::UnsupportedSplit
    &&detail.bases==2&&detail.dominant==23&&detail.runnerUp==23&&detail.maximumShared==0,
    "equal split reports bounded support without lineage");
  else suite.Expect(detail.rejection==RemakeCameraAnchor::BasisReport::Rejection::BasisLimit
    &&detail.bases==65&&detail.dominant==1&&detail.runnerUp==1,
    "65th basis reports cap independently of unsupported split");
  // Early identity failure must not expose a stale rejection from the last call.
  observed.game="invalid";anchor.Apply(observed,scene,output,error);
  suite.Expect(anchor.LastBasisReport().rejection==RemakeCameraAnchor::BasisReport::Rejection::None,
    "basis diagnostics clear between attempts");
 };
 check(46,true);check(65,false);
}
