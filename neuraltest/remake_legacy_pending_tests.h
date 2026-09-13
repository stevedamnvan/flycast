// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_legacy_pending.h"
#include <stdexcept>

template<class Suite>
void TestRemakeLegacyPending(Suite& suite) {
 using namespace neuraltest::remake;
 struct Counted {
  int refs=1,releases=0;bool underflow=false;
  void AddRef() noexcept {++refs;}
  void Release() noexcept {++releases;if(--refs<0)underflow=true;}
 };
 struct Indices {
  bool failCopy=false;
  Indices& operator=(const Indices& other) {
   if(other.failCopy)throw std::runtime_error("injected index allocation failure");
   failCopy=other.failCopy;return *this;
  }
 };
 struct Resource {Counted* vb=nullptr;Counted* texture=nullptr;Indices indices;};
 // Early HRESULT-style return after one retained mesh and one partial new mesh.
 {
  Counted oldVb,oldTexture,newVb;
  Resource old{&oldVb,&oldTexture,{}};
  const auto fail=[&]() {
   LegacyPendingResources<Resource> pending;pending.resources.resize(2);
   pending.Retain(0,old);pending.resources[1].vb=&newVb;
   return false; // Texture creation fails before assigning its output.
  };
  suite.Expect(!fail()&&oldVb.refs==1&&oldTexture.refs==1&&newVb.refs==0
   &&newVb.releases==1,"legacy pending partial allocation rollback retains old ownership");
 }
 // Texture creation can fail after assigning an object (e.g. upload failure).
 {
  Counted oldVb,oldTexture,newVb,newTexture;
  Resource old{&oldVb,&oldTexture,{}};
  try {
   LegacyPendingResources<Resource> pending;pending.resources.resize(2);
   pending.Retain(0,old);pending.resources[1].vb=&newVb;pending.resources[1].texture=&newTexture;
   throw std::runtime_error("injected later preparation failure");
  } catch(const std::runtime_error&) {}
  suite.Expect(oldVb.refs==1&&oldTexture.refs==1&&newVb.refs==0&&newTexture.refs==0
   &&newVb.releases==1&&newTexture.releases==1,"legacy pending exception releases partial objects and added references");
 }
 {
  Counted firstVb,firstTexture,secondVb,secondTexture;
  Resource first{&firstVb,&firstTexture,{}},second{&secondVb,&secondTexture,{true}};
  bool threw=false;
  try {
   LegacyPendingResources<Resource> pending;pending.resources.resize(2);
   pending.Retain(0,first);pending.Retain(1,second);
  } catch(const std::runtime_error&) {threw=true;}
  suite.Expect(threw&&firstVb.refs==1&&firstTexture.refs==1&&secondVb.refs==1
   &&secondTexture.refs==1&&secondVb.releases==0&&secondTexture.releases==0,
   "legacy pending failed CPU copy does not release unacquired references");
 }
 {
  Counted retainedVb,retainedTexture,retiredVb,retiredTexture,newVb,newTexture;
  {
   LegacyPendingResources<Resource> live;
   live.resources={{&retainedVb,&retainedTexture,{}},{&retiredVb,&retiredTexture,{}}};
   {
    LegacyPendingResources<Resource> pending;pending.resources.resize(2);
    pending.Retain(1,live.resources[0]);pending.resources[0].vb=&newVb;pending.resources[0].texture=&newTexture;
    live.resources.swap(pending.resources);
   }
   suite.Expect(live.resources[0].vb==&newVb&&live.resources[1].vb==&retainedVb
    &&retainedVb.refs==1&&retainedTexture.refs==1&&retiredVb.refs==0&&retiredTexture.refs==0,
    "legacy pending commit preserves reordered retained refs and retires removed slots");
  }
  suite.Expect(retainedVb.refs==0&&retainedTexture.refs==0&&newVb.refs==0&&newTexture.refs==0
   &&!retainedVb.underflow&&!retainedTexture.underflow&&!retiredVb.underflow&&!retiredTexture.underflow
   &&!newVb.underflow&&!newTexture.underflow,"legacy pending committed ownership releases once without underflow");
 }
}
