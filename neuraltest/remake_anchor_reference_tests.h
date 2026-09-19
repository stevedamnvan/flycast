// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_feed_worker.h"

template<class Suite,class Snapshot,class Scene,class Packet>
void TestRemakeAnchorReference(Suite& suite,const Snapshot& snapshot,const Scene& scene,const Packet& packet) {
 using namespace flycast::rend::neural;
 const auto reference=snapshot.sourceProducer.ordinal;
 unsigned publications=0;
 const auto job=[&](std::uint64_t ordinal,bool publish=true) {
  RemakeFeedJob j;j.snapshot=snapshot;j.scene=scene;j.packet=packet;
  const auto delta=ordinal-reference;
  j.producer=snapshot.sourceProducer;j.producer.ordinal=ordinal;j.producer.cycle+=delta;
  j.frame=packet.frame+delta;j.snapshot.frame=j.frame;j.snapshot.sourceProducer=j.producer;
  j.scene.frame=j.frame;j.scene.producer=j.producer;j.packet.frame=j.frame;j.packet.producer=j.producer;
  for(auto& m:j.packet.meshes)m.frame=j.frame;
  j.anchored=j.managed=j.captureScene=true;j.anchorReferenceProducer=reference;
  j.publish=[&,publish](const auto&,RemakeChannelReceipt& receipt,std::string&) {
   ++publications;receipt={1,7,9};return publish?RemakeChannelResult::Published:RemakeChannelResult::Busy;
  };return j;
 };
 const auto run=[](RemakeFeedWorker& worker,RemakeFeedJob j) {
  const auto target=worker.Completed()+1;
  if(!worker.Dispatch(std::move(j)))return std::vector<RemakeFeedResult>{};
  for(unsigned i=0;i<2000&&worker.Completed()<target;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  return worker.Drain();
 };
 RemakeFeedWorker worker;worker.Start();
 auto mislabeled=job(reference+1);mislabeled.producer.ordinal=reference;
 auto identityResult=run(worker,std::move(mislabeled));
 suite.Expect(identityResult.size()==1&&identityResult[0].error=="anchor-reference-identity"&&publications==0,
  "anchor reference cannot substitute actual source with a matching job label");
 auto early=job(reference);early.anchorReferenceProducer=reference+1;
 auto r=run(worker,std::move(early));
 suite.Expect(r.size()==1&&r[0].error=="anchor-reference-awaiting"&&publications==0,"anchor reference earlier source cannot publish");
 r=run(worker,job(reference+1));
 suite.Expect(r.size()==1&&r[0].error=="anchor-reference-missed"&&publications==0,"anchor reference later source cannot substitute");
 auto invalid=job(reference);invalid.managed=false;r=run(worker,std::move(invalid));
 suite.Expect(r.size()==1&&r[0].error=="anchor-reference-scope"&&publications==0,"anchor reference rejects unmanaged scope");
 r=run(worker,job(reference,false));
 suite.Expect(r.size()==1&&r[0].stage=="publish"&&publications==1,"anchor reference exact valid scene reaches publisher failure");
 r=run(worker,job(reference+1));
 suite.Expect(r.size()==1&&r[0].error=="anchor-reference-missed"&&publications==1,"failed anchor publication cannot seed later source");
 r=run(worker,job(reference));
 suite.Expect(r.size()==1&&r[0].stage.empty()&&r[0].referenceOrdinal==reference&&publications==2,"anchor reference exact validated source establishes anchor");
 r=run(worker,job(reference+1));
 suite.Expect(r.size()==1&&r[0].stage.empty()&&r[0].referenceOrdinal==reference&&publications==3,"established anchor continues without reselection");
 RemakeFeedWorker ordinary;ordinary.Start();auto normal=job(reference+1);normal.anchorReferenceProducer=0;
 r=run(ordinary,std::move(normal));
 suite.Expect(r.size()==1&&r[0].stage.empty()&&r[0].referenceOrdinal==reference+1,"default worker retains first available source behavior");
}
