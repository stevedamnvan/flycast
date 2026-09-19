// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_overlay_snapshot.h"

template<class Suite>
void RunRemakeDiagnosticOverlayTests(Suite& suite) {
 using namespace flycast::rend::neural;
 const auto producer=[](std::uint64_t frame){return ProducerIdentity{7,frame-1,frame*3};};
 const auto snapshot=[&](std::uint64_t frame,std::uint64_t sequence) {
  RemakeOverlaySnapshot result;result.identity.frame=frame;result.identity.producer=producer(frame);
  result.identity.receipt={sequence,sequence*11,64};
  result.captureTransport=std::make_shared<remake::Packet>();
  result.lease=std::make_shared<int>(int(sequence));return result;
 };
 const auto returned=[](const RemakeOverlaySnapshot& source) {
  RemakeReturnedImage image;image.frame=source.identity.frame;image.producer=source.identity.producer;
  image.source=source.identity.receipt;return image;
 };
 const char* reason=nullptr;
 {
  RemakeDiagnosticOverlayStore store;
  auto first=snapshot(6301,1156);const auto image=returned(first);
  std::weak_ptr<void> originalLease=first.lease;
  bool retained=store.Retain(std::move(first),6301,producer(6301),reason);
  for(std::uint64_t i=1;i<=4;++i)
   retained=store.Retain(snapshot(6301+i,1156+i),6301+i,producer(6301+i),reason)&&retained;
  auto* original=store.Find(1156);auto* collision=store.Find(1160);
  suite.Expect(retained&&original&&collision&&original!=collision&&!originalLease.expired()
   &&original->identity.Matches(image,6306,producer(6306)),
   "diagnostic overlay retains delayed sequence1156 after former modulo collision1160 at age5");
  store.Expire(6309,producer(6309));original=store.Find(1156);
  suite.Expect(original&&original->identity.Matches(image,6309,producer(6309))&&!originalLease.expired(),
   "diagnostic overlay preserves source and original lease at inclusive age8");
  store.Expire(6310,producer(6310));
  suite.Expect(!store.Find(1156)&&originalLease.expired()&&store.Find(1160),
   "diagnostic overlay age9 expires only ineligible entries");
 }
 {
  RemakeDiagnosticOverlayStore store;bool retained=true;
  std::array<std::weak_ptr<void>,RemakeDiagnosticOverlayStore::Capacity> leases;
  for(std::size_t i=0;i<leases.size();++i) {
   auto item=snapshot(100+i,200+i);leases[i]=item.lease;
   retained=store.Retain(std::move(item),108,producer(108),reason)&&retained;
  }
  suite.Expect(retained&&store.Find(200)&&store.Find(208),
   "diagnostic overlay retains all nine sources within inclusive eight-frame return age");
  auto overflow=snapshot(108,209);const auto overflowLease=overflow.lease;
  const bool full=store.Retain(std::move(overflow),108,producer(108),reason);
  bool originalsIntact=true;for(std::size_t i=0;i<leases.size();++i)
   originalsIntact=originalsIntact&&store.Find(200+i)&&!leases[i].expired();
  suite.Expect(!full&&std::string(reason)=="capacity-exhausted"&&originalsIntact
   &&overflow.lease==overflowLease&&!store.Find(209),
   "diagnostic overlay overflow fails without overwriting sources or consuming caller lease");
  auto duplicate=snapshot(108,204);duplicate.identity.receipt.digest^=1;
  const auto originalDigest=store.Find(204)->identity.receipt.digest;
  suite.Expect(!store.Retain(std::move(duplicate),108,producer(108),reason)
   &&std::string(reason)=="duplicate-receipt"&&duplicate.lease
   &&store.Find(204)->identity.receipt.digest==originalDigest,
   "diagnostic overlay duplicate sequence rejects changed receipt without replacing original");
  suite.Expect(store.Retain(snapshot(109,209),109,producer(109),reason)
   &&!store.Find(200)&&leases[0].expired()&&store.Find(201)&&!leases[1].expired(),
   "diagnostic overlay expiration makes space without evicting still-eligible neighbor");
 }
 {
  RemakeDiagnosticOverlayStore store;auto held=snapshot(200,300);
  std::weak_ptr<void> lease=held.lease;
  suite.Expect(store.Retain(std::move(held),205,producer(205),reason),
   "diagnostic overlay source ready for malformed admission checks");
  bool rejected=true;
  for(unsigned mutation=0;mutation<11;++mutation) {
   auto candidate=snapshot(205,301);auto current=producer(205);
   if(mutation==0)candidate.captureTransport.reset();
   if(mutation==1)candidate.identity.frame=0;
   if(mutation==2)candidate.identity.receipt.sequence=0;
   if(mutation==3)candidate.identity.receipt.bytes=0;
   if(mutation==4)candidate.identity.producer={};
   if(mutation==5)current={};
   if(mutation==6)++candidate.identity.producer.epoch;
   if(mutation==7)++candidate.identity.producer.ordinal;
   if(mutation==8)++candidate.identity.producer.cycle;
   if(mutation==9)candidate.identity.frame=206;
   if(mutation==10)candidate.identity.frame=196;
   const auto callerLease=candidate.lease;
   rejected=!store.Retain(std::move(candidate),205,current,reason)&&rejected;
   rejected=rejected&&std::string(reason)=="invalid-or-expired-source"
    &&candidate.lease==callerLease&&store.Find(300)&&!lease.expired();
  }
  suite.Expect(rejected,
   "diagnostic overlay invalid scope identity age and current producer preserve earlier retained source");
  auto nextEpoch=producer(205);++nextEpoch.epoch;store.Expire(205,nextEpoch);
  suite.Expect(!store.Find(300)&&lease.expired(),
   "diagnostic overlay epoch retirement releases prior session ownership");
 }
 {
  RemakeDiagnosticOverlayStore store;auto source=snapshot(400,500);
  const auto image=returned(source);std::weak_ptr<void> lease=source.lease;
  const bool retained=store.Retain(std::move(source),402,producer(402),reason);
  auto* entry=store.Find(500);bool exact=retained&&entry&&entry->identity.Matches(image,402,producer(402));
  if(entry)for(unsigned mutation=0;mutation<7;++mutation) {
   auto wrong=image;
   if(mutation==0)++wrong.frame;
   if(mutation==1)++wrong.producer.epoch;
   if(mutation==2)++wrong.producer.ordinal;
   if(mutation==3)++wrong.producer.cycle;
   if(mutation==4)++wrong.source.sequence;
   if(mutation==5)++wrong.source.digest;
   if(mutation==6)++wrong.source.bytes;
   exact=exact&&!entry->identity.Matches(wrong,402,producer(402));
  }
  suite.Expect(exact&&!store.Find(0),
   "diagnostic overlay sequence lookup never relaxes exact frame producer digest or byte-count receipt gates");
  RemakeOverlaySnapshot accepted;
  if(entry){accepted=std::move(*entry);*entry={};}
  store.Reset();
  suite.Expect(!store.Find(500)&&!lease.expired()&&accepted.identity.Matches(image,402,producer(402)),
   "diagnostic overlay transfer retains original ownership after store reset");
  accepted={};suite.Expect(lease.expired(),
   "diagnostic overlay accepted owner release retires the final snapshot lease");
  auto pending=snapshot(410,510);std::weak_ptr<void> pendingLease=pending.lease;
  store.Retain(std::move(pending),410,producer(410),reason);store.Reset();
  suite.Expect(!store.Find(510)&&pendingLease.expired(),
   "diagnostic overlay full history reset releases pending retained snapshots");
 }
}
