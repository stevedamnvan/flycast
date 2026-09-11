// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_camera_anchor.h"
#include "remake_temporal_scene.h"
#include "remake_overlay_snapshot.h"
#include "remake_live_channel.h"
#include "pvr_scene_capture.h"
#include "remake_view_scene.h"
#include "remake_view_transport.h"
#include "remake_alpha_ownership.h"
#include <array>
#include <chrono>
#include <map>
#include <set>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
namespace flycast::rend::neural {
// Off-render-thread scene feed (D-211). The render thread keeps every stage
// that touches the device (snapshot, view scene, texture reads,
// overlay copy); normal smoothing, packet build, camera anchor, temporal capture,
// serialization and digest run on one worker with one pending slot. A full slot is an explicit
// native fallback for that source: the render thread never waits, so a slow
// lane cannot slow emulation. The worker owns the camera anchor so support
// lineage stays sequential; results are drained on the render thread, which
// applies history retirement and logging in source order.
using RemakeSentTextureSet=std::set<std::array<std::uint64_t,4>>;
struct RemakeFeedJob {
 std::uint64_t frame=0;ProducerIdentity producer;
 PvrDecodedPacket snapshot;RemakeViewScene scene;remake::Packet packet;RemakeOverlaySnapshot overlay;
 bool anchored=false,temporal=false,managed=false,captureScene=false;
 // Geometry and texture selection are already owned; smoothing changes only
 // normals and can precede packet creation without device/context access.
 bool smoothNormals=false;
 // Packet build on the worker (D-213): the render thread stages every needed
 // texture's bytes by draw (list, ordinal) and an immutable snapshot of the
 // identities the consumer already holds; no device access happens here.
 bool buildPacket=false,registerMore=true,byReference=false; // byReference: register/reference even while nothing is held yet.
 std::map<std::pair<std::uint32_t,std::uint32_t>,std::vector<unsigned char>> textures;
 std::shared_ptr<const RemakeSentTextureSet> sent;
 // Alpha ownership: the render thread verified the source bindings and staged
 // the effect identity words of every alpha draw; the selections are taken from
 // the meshes that survive clipping in the built packet.
 bool alphaOwnership=false;std::map<std::uint32_t,EffectIdentityPoly> alphaParams;
 std::function<RemakeChannelResult(const remake::Packet&,RemakeChannelReceipt&,std::string&)> publish;
};
struct RemakeFeedResult {
 std::uint64_t frame=0;ProducerIdentity producer;
 std::string stage,error; // Empty stage: published.
 RemakeChannelReceipt receipt;RemakeOverlaySnapshot overlay;std::shared_ptr<RemakeTemporalScene> temporalScene;
 std::shared_ptr<remake::Packet> capturedPacket;
 remake::Vec3 cameraPosition{};
 // Anchor observations for render-thread history retirement and logging.
 bool anchored=false,supportChanged=false,regenerated=false,viewCut=false;
 std::uint32_t generation=0;std::uint64_t referenceOrdinal=0;remake::Vec3 origin{};double projectionMaxPixels=0;
 RemakeCameraAnchor::SupportReport support{};RemakeCameraAnchor::ProjectionReport projection{};
 // Texture identities the consumer registered from this published packet
 // (D-212); the render thread records them as sent only after Published.
 std::vector<remake::TextureIdentity> registeredTextures;std::size_t registeredBytes=0;
 bool alphaOwnership=false;
 double workerMs=0,packetMs=0,anchorMs=0,temporalMs=0,publishMs=0; // Diagnostic stage times inside the worker.
};
class RemakeFeedWorker {
 mutable std::mutex mutex;std::condition_variable wake;std::thread thread;
 std::optional<RemakeFeedJob> pending;bool busy=false,stop=false,resetAnchor=false;
 std::deque<RemakeFeedResult> results;
 RemakeChunkWorkers chunkWorkers; // Explicit owner, never a TLS destructor join.
 RemakeCameraAnchor anchor; // Worker thread only once started.
 std::uint64_t busySkips=0,dispatched=0,completed=0;
 RemakeFeedResult process(RemakeFeedJob& job) {
  const auto start=std::chrono::steady_clock::now();
  RemakeFeedResult r;r.frame=job.frame;r.producer=job.producer;r.overlay=std::move(job.overlay);r.anchored=job.anchored;
  std::string error;RemakeCameraAnchor proposed;
  if(job.smoothNormals)for(auto& mesh:job.scene.meshes)SmoothRemakeViewNormals(mesh);
  if(job.buildPacket) {
   const RemakeTextureReader reader=[&](const PvrCapturedDraw& draw,std::vector<unsigned char>& bytes,std::string& why) {
    const auto found=job.textures.find({draw.list,draw.ordinal});
    if(found==job.textures.end()){why="texture-not-staged";return false;}
    bytes=found->second;return true;
   };
   RemakeTextureSent sent;
   if(job.byReference){auto held=job.sent;sent=[held](const remake::TextureIdentity& t){return held&&held->count({t.id,t.generation,t.paletteGeneration,t.rttGeneration})!=0;};}
   if(!BuildRemakeViewPacket(job.scene,reader,job.packet,error,sent)) {
    r.stage="packet";r.error=error;r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;
   }
   if(!job.registerMore)for(auto& mesh:job.packet.meshes)if(mesh.textureWire==remake::TextureWire::Registered)mesh.textureWire=remake::TextureWire::Carried;
   if(job.alphaOwnership) {
    r.alphaOwnership=true;
    for(const auto& mesh:job.packet.meshes)if(mesh.sourceAlphaBlend) {
     const auto ordinal=std::uint32_t(mesh.id)-1;const auto found=job.alphaParams.find(ordinal);
     if((mesh.id>>32)!=2||found==job.alphaParams.end()) {
      r.stage="alpha-ownership";r.error="source-list-range";r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;
     }
     r.overlay.alphaEffectSelections.push_back({ordinal,found->second});
    }
   }
   r.packetMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  }
  const auto elapsed=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
  if(job.anchored) {
   const auto anchorStart=elapsed();
   proposed=anchor;
   const bool applied=proposed.Apply(job.snapshot,job.scene,job.packet,error,&chunkWorkers);
   r.anchorMs=elapsed()-anchorStart;
   if(!applied) {
    r.stage="camera-anchor";r.error=error;r.support=proposed.LastSupportReport();r.referenceOrdinal=proposed.ReferenceOrdinal();
    if(job.managed&&error=="anchor-source-support-changed") {
     // Genuine source-view cut: retire the fixed view here so the next accepted
     // source starts a labeled generation; the render thread retires histories.
     anchor=std::move(proposed);r.regenerated=anchor.Reanchor();r.supportChanged=true;
    }
    r.generation=anchor.Generation();
    r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;
   }
   r.generation=proposed.Generation();r.support=proposed.LastSupportReport();
   if(r.support.available&&!r.support.rejected&&(r.support.rotationFromLastDegrees>20||r.support.translationFromLast>1))r.viewCut=true;
  }
  if(job.temporal) {
   const auto temporalStart=elapsed();
   r.temporalScene=CaptureRemakeTemporalScene(job.packet,error);
   r.temporalMs=elapsed()-temporalStart;
   if(!r.temporalScene){r.stage="temporal-source";r.error=error;r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;}
  }
  RemakeChannelReceipt receipt;
  const auto publishStart=elapsed();
  const auto result=job.publish?job.publish(job.packet,receipt,error):RemakeChannelResult::Invalid;
  r.publishMs=elapsed()-publishStart;
  if(result!=RemakeChannelResult::Published) {
   r.stage="publish";r.error=error.empty()?"feed-publisher-missing":error;
   r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;
  }
  r.receipt=receipt;r.overlay.identity.receipt=receipt;
  if(r.temporalScene)r.temporalScene->receipt=receipt;
  if(job.anchored) {
   anchor=std::move(proposed);
   r.referenceOrdinal=anchor.ReferenceOrdinal();r.origin=anchor.Origin();r.projectionMaxPixels=anchor.MaximumProjectionError();
   r.support=anchor.LastSupportReport();r.projection=anchor.LastProjectionReport();r.generation=anchor.Generation();
  }
  r.cameraPosition=job.packet.camera.position;
  for(const auto& mesh:job.packet.meshes)if(mesh.textureWire==remake::TextureWire::Registered&&mesh.material) {
   r.registeredTextures.push_back(mesh.texture);r.registeredBytes+=mesh.material->sourceDdsBytes.size();
  }
  if(job.captureScene)r.capturedPacket=std::make_shared<remake::Packet>(std::move(job.packet));
  r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  return r;
 }
 void run() {
  for(;;) {
   RemakeFeedJob job;bool reset=false;
   {
    std::unique_lock<std::mutex> lock(mutex);
    wake.wait(lock,[&]{return stop||pending.has_value()||resetAnchor;});
    if(stop)return;
    reset=resetAnchor;resetAnchor=false;
    if(!pending){if(reset)anchor.Reset();continue;}
    job=std::move(*pending);pending.reset();busy=true;
   }
   if(reset)anchor.Reset();
   RemakeFeedResult result=process(job);
   {
    std::lock_guard<std::mutex> lock(mutex);
    busy=false;++completed;
    if(!stop)results.push_back(std::move(result));
   }
  }
 }
public:
 RemakeFeedWorker()=default;
 ~RemakeFeedWorker(){Stop();}
 RemakeFeedWorker(const RemakeFeedWorker&)=delete;
 RemakeFeedWorker& operator=(const RemakeFeedWorker&)=delete;
 void Start() {
  std::lock_guard<std::mutex> lock(mutex);
  if(thread.joinable())return;
  stop=false;thread=std::thread([this]{run();});
 }
 void Stop() {
  {std::lock_guard<std::mutex> lock(mutex);stop=true;pending.reset();}
  wake.notify_all();
  if(thread.joinable())thread.join();
  std::lock_guard<std::mutex> lock(mutex);results.clear();busy=false;thread=std::thread();
 }
 // One active job plus one FIFO pending slot absorbs scheduling jitter.
 // A full slot rejects without waiting or replacing an owned source.
 bool Dispatch(RemakeFeedJob&& job) {
  {
   std::lock_guard<std::mutex> lock(mutex);
   if(!thread.joinable()||pending){++busySkips;return false;}
   pending=std::move(job);++dispatched;
  }
  wake.notify_one();return true;
 }
 // Retire the worker-owned anchor and any pending job (session or epoch change).
 void ResetAnchor() {
  {std::lock_guard<std::mutex> lock(mutex);resetAnchor=true;pending.reset();}
  wake.notify_one();
 }
 std::vector<RemakeFeedResult> Drain() {
  std::lock_guard<std::mutex> lock(mutex);
  std::vector<RemakeFeedResult> out;out.reserve(results.size());
  for(auto& r:results)out.push_back(std::move(r));
  results.clear();return out;
 }
 bool Idle()const{std::lock_guard<std::mutex> lock(mutex);return !busy&&!pending;}
 std::uint64_t BusySkips()const{std::lock_guard<std::mutex> lock(mutex);return busySkips;}
 std::uint64_t Dispatched()const{std::lock_guard<std::mutex> lock(mutex);return dispatched;}
 std::uint64_t Completed()const{std::lock_guard<std::mutex> lock(mutex);return completed;}
};
}
