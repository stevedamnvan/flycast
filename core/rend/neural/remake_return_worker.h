// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
#include "remake_temporal_scene.h"
#include "remake_motion_stream.h"
#include "remake_neural_input.h"
#include "remake_overlay_snapshot.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
namespace flycast::rend::neural {
// Off-render-thread returned-image receive and preparation (D-213, D-214).
// The worker receives returned images from the channel itself (free-running,
// one poll per millisecond when empty), checks the image is well formed,
// prepares the geometry motion stream against the newest image ahead of it
// and converts the neural input. The render thread takes one prepared image at
// a time and applies the identity gate (age, epoch, original overlay match)
// exactly as before; the device-bound stages (input upload, motion raster,
// consumer submit, output ownership) stay there. The prepared stream is used
// only when the history it was built against is the accepted history at
// evaluation time; otherwise the render thread rebuilds it. Temporal scenes are
// registered by sequence when the render thread publishes an overlay; a return
// whose scene is not registered yet is prepared without a stream and the render
// thread builds it. A closed channel is reported, never handled, here.
struct RemakeReturnJob {
 RemakeReturnedImage returned;RemakeOverlaySnapshot overlay;
 std::shared_ptr<const RemakeTemporalScene> previous; // Last accepted at dispatch; may be empty.
};
struct RemakeReturnResult {
 RemakeReturnedImage returned;RemakeOverlaySnapshot overlay;
 std::uint64_t previousFrame=0; // 0: no accepted history at dispatch.
 bool wellFormed=false,temporal=false,streamReady=false;std::string streamError;RemakeMotionStream stream;
 bool inputReady=false;RemakeNeuralInput input;
 double workerMs=0;
};
class RemakeReturnWorker {
 mutable std::mutex mutex;std::condition_variable wake;std::thread thread;
 std::optional<RemakeReturnJob> pending;bool busy=false,stop=false;
 std::uint64_t generation=0; // Results from a discarded generation are dropped.
 std::deque<std::pair<std::uint64_t,RemakeReturnResult>> results;
 std::uint64_t busySkips=0,dispatched=0,completed=0,received=0,invalidReturns=0;
 RemakeLiveChannel* channel=nullptr;bool closed=false;
 std::map<std::uint64_t,std::shared_ptr<const RemakeTemporalScene>> scenes; // By channel sequence, bounded.
 std::shared_ptr<const RemakeTemporalScene> chain; // Newest received scene: the history the next image is prepared against.
 static RemakeReturnResult prepare(RemakeReturnedImage&& image,RemakeOverlaySnapshot&& overlay,
  const std::shared_ptr<const RemakeTemporalScene>& scene,const std::shared_ptr<const RemakeTemporalScene>& previous) {
  const auto start=std::chrono::steady_clock::now();
  RemakeReturnResult r;r.returned=std::move(image);r.overlay=std::move(overlay);
  r.wellFormed=RemakeReturnedImageWellFormed(r.returned);
  if(scene) {
   r.temporal=true;r.previousFrame=previous?previous->frame:0;
   r.streamReady=BuildRemakeMotionStream(previous.get(),*scene,r.stream,r.streamError);
  }
  if(r.wellFormed)r.inputReady=BuildRemakeNeuralInput(r.returned,r.returned.frame,r.returned.producer,r.input);
  r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  return r;
 }
 void run() {
  for(;;) {
   RemakeReturnJob job;std::uint64_t mine=0;bool haveJob=false;RemakeLiveChannel* poll=nullptr;
   {
    std::unique_lock<std::mutex> lock(mutex);
    wake.wait_for(lock,std::chrono::milliseconds(1),[&]{return stop||pending.has_value();});
    if(stop)return;
    mine=generation;
    if(pending){job=std::move(*pending);pending.reset();busy=true;haveJob=true;}
    else if(channel&&!closed)poll=channel;
   }
   if(haveJob) {
    const auto scene=job.overlay.temporalScene;
    RemakeReturnResult result=prepare(std::move(job.returned),std::move(job.overlay),scene,job.previous);
    std::lock_guard<std::mutex> lock(mutex);
    busy=false;++completed;
    if(!stop&&mine==generation)results.emplace_back(mine,std::move(result));
    continue;
   }
   if(!poll)continue;
   RemakeReturnedImage image;std::string error;
   const auto outcome=poll->ReceiveImage(image,error);
   if(outcome==RemakeChannelResult::Closed){std::lock_guard<std::mutex> lock(mutex);closed=true;continue;}
   if(outcome!=RemakeChannelResult::Received) {
    // A channel not open on this side yet is idle, not an invalid return.
    if(outcome==RemakeChannelResult::Invalid&&error!="return-publisher-role"){std::lock_guard<std::mutex> lock(mutex);++invalidReturns;}
    // D-219: block on the consumer's returned-image event (bounded) instead
    // of returning to the 1 ms timed wait, which is timer-resolution bound.
    if(outcome==RemakeChannelResult::Empty)poll->WaitForReturned(2);
    continue;
   }
   std::shared_ptr<const RemakeTemporalScene> scene,previous;
   {
    std::lock_guard<std::mutex> lock(mutex);
    if(mine!=generation)continue;
    if(const auto found=scenes.find(image.source.sequence);found!=scenes.end())scene=found->second;
    previous=chain;if(scene)chain=scene;busy=true;
   }
   RemakeReturnResult result=prepare(std::move(image),RemakeOverlaySnapshot{},scene,previous);
   std::lock_guard<std::mutex> lock(mutex);
   busy=false;++completed;++received;
   if(!stop&&mine==generation)results.emplace_back(mine,std::move(result));
  }
 }
public:
 RemakeReturnWorker()=default;
 ~RemakeReturnWorker(){Stop();}
 RemakeReturnWorker(const RemakeReturnWorker&)=delete;
 RemakeReturnWorker& operator=(const RemakeReturnWorker&)=delete;
 void Start() {
  std::lock_guard<std::mutex> lock(mutex);
  if(thread.joinable())return;
  stop=false;thread=std::thread([this]{run();});
 }
 void Stop() {
  {std::lock_guard<std::mutex> lock(mutex);stop=true;pending.reset();channel=nullptr;}
  wake.notify_all();
  if(thread.joinable())thread.join();
  std::lock_guard<std::mutex> lock(mutex);results.clear();busy=false;thread=std::thread();
 }
 // Free-running receive from this channel (host side, publisher role). The
 // channel object must outlive the worker or be detached with nullptr first.
 void Attach(RemakeLiveChannel* live){std::lock_guard<std::mutex> lock(mutex);if(channel!=live){channel=live;closed=false;}}
 bool Attached()const{std::lock_guard<std::mutex> lock(mutex);return channel!=nullptr;}
 bool ChannelClosed()const{std::lock_guard<std::mutex> lock(mutex);return closed;}
 // The temporal scene published for a channel sequence (render thread, at overlay publish).
 void RegisterScene(std::uint64_t sequence,std::shared_ptr<const RemakeTemporalScene> scene) {
  std::lock_guard<std::mutex> lock(mutex);
  if(!scene)return;
  scenes[sequence]=std::move(scene);
  while(scenes.size()>8)scenes.erase(scenes.begin());
 }
 // False means busy: the caller keeps its synchronous path and the job is untouched.
 bool Dispatch(RemakeReturnJob&& job) {
  {
   std::lock_guard<std::mutex> lock(mutex);
   if(!thread.joinable()||busy||pending){++busySkips;return false;}
   pending=std::move(job);++dispatched;
  }
  wake.notify_one();return true;
 }
 // History retirement: drop the pending job, every result not yet drained
 // (including one still being prepared), the registered scenes and the chain;
 // a closed flag is cleared so a re-opened channel is polled again.
 void Discard() {
  std::lock_guard<std::mutex> lock(mutex);++generation;pending.reset();results.clear();scenes.clear();chain.reset();closed=false;
 }
 // One prepared result in order, or nothing: the render thread evaluates one
 // image per frame and must not overwrite an image it has not evaluated yet.
 std::optional<RemakeReturnResult> Next() {
  std::lock_guard<std::mutex> lock(mutex);
  while(!results.empty()) {
   auto r=std::move(results.front());results.pop_front();
   if(r.first==generation)return std::move(r.second);
  }
  return std::nullopt;
 }
 std::size_t Ready()const{std::lock_guard<std::mutex> lock(mutex);std::size_t n=0;for(const auto& r:results)n+=r.first==generation;return n;}
 std::vector<RemakeReturnResult> Drain() {
  std::lock_guard<std::mutex> lock(mutex);
  std::vector<RemakeReturnResult> out;out.reserve(results.size());
  for(auto& r:results)if(r.first==generation)out.push_back(std::move(r.second));
  results.clear();return out;
 }
 bool Idle()const{std::lock_guard<std::mutex> lock(mutex);return !busy&&!pending;}
 std::uint64_t BusySkips()const{std::lock_guard<std::mutex> lock(mutex);return busySkips;}
 std::uint64_t Dispatched()const{std::lock_guard<std::mutex> lock(mutex);return dispatched;}
 std::uint64_t Completed()const{std::lock_guard<std::mutex> lock(mutex);return completed;}
 std::uint64_t Received()const{std::lock_guard<std::mutex> lock(mutex);return received;}
 std::uint64_t InvalidReturns()const{std::lock_guard<std::mutex> lock(mutex);return invalidReturns;}
};
}
