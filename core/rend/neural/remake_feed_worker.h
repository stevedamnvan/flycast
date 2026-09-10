// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_camera_anchor.h"
#include "remake_temporal_scene.h"
#include "remake_overlay_snapshot.h"
#include "remake_live_channel.h"
#include "pvr_scene_capture.h"
#include "remake_view_scene.h"
#include <chrono>
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
// that touches the device (snapshot, view scene, texture reads, packet build,
// overlay copy); the camera anchor, temporal capture, serialization and digest
// run here on one worker with a single pending job. A busy worker is an explicit
// native fallback for that source: the render thread never waits, so a slow
// lane cannot slow emulation. The worker owns the camera anchor so support
// lineage stays sequential; results are drained on the render thread, which
// applies history retirement and logging in source order.
struct RemakeFeedJob {
 std::uint64_t frame=0;ProducerIdentity producer;
 PvrDecodedPacket snapshot;RemakeViewScene scene;remake::Packet packet;RemakeOverlaySnapshot overlay;
 bool anchored=false,temporal=false,managed=false,captureScene=false;
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
 double workerMs=0;
};
class RemakeFeedWorker {
 mutable std::mutex mutex;std::condition_variable wake;std::thread thread;
 std::optional<RemakeFeedJob> pending;bool busy=false,stop=false,resetAnchor=false;
 std::deque<RemakeFeedResult> results;
 RemakeCameraAnchor anchor; // Worker thread only once started.
 std::uint64_t busySkips=0,dispatched=0,completed=0;
 RemakeFeedResult process(RemakeFeedJob& job) {
  const auto start=std::chrono::steady_clock::now();
  RemakeFeedResult r;r.frame=job.frame;r.producer=job.producer;r.overlay=std::move(job.overlay);r.anchored=job.anchored;
  std::string error;RemakeCameraAnchor proposed;
  if(job.anchored) {
   proposed=anchor;
   if(!proposed.Apply(job.snapshot,job.scene,job.packet,error)) {
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
   r.temporalScene=CaptureRemakeTemporalScene(job.packet,error);
   if(!r.temporalScene){r.stage="temporal-source";r.error=error;r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;}
  }
  RemakeChannelReceipt receipt;
  const auto result=job.publish?job.publish(job.packet,receipt,error):RemakeChannelResult::Invalid;
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
 // False means the worker is busy: an explicit native fallback for this source.
 bool Dispatch(RemakeFeedJob&& job) {
  {
   std::lock_guard<std::mutex> lock(mutex);
   if(!thread.joinable()||busy||pending){++busySkips;return false;}
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
