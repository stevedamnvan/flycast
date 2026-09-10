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
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
namespace flycast::rend::neural {
// Off-render-thread returned-image preparation (D-213). After the render
// thread has received and identity-checked a returned image, the CPU-only
// stages (geometry motion stream against the last accepted temporal scene and
// the neural input conversion) run here on one worker with a single pending
// job. The device-bound stages (input upload, motion raster, consumer submit,
// output ownership) stay on the render thread. A busy worker means the render
// thread keeps the previous synchronous path for that image: never a wait,
// never a dropped return. The prepared stream is used only when the temporal
// history the worker saw is still the history at evaluation time; otherwise
// the render thread rebuilds it, so no history is ever manufactured.
struct RemakeReturnJob {
 RemakeReturnedImage returned;RemakeOverlaySnapshot overlay;
 std::shared_ptr<const RemakeTemporalScene> previous; // Last accepted at dispatch; may be empty.
};
struct RemakeReturnResult {
 RemakeReturnedImage returned;RemakeOverlaySnapshot overlay;
 std::uint64_t previousFrame=0; // 0: no accepted history at dispatch.
 bool temporal=false,streamReady=false;std::string streamError;RemakeMotionStream stream;
 bool inputReady=false;RemakeNeuralInput input;
 double workerMs=0;
};
class RemakeReturnWorker {
 mutable std::mutex mutex;std::condition_variable wake;std::thread thread;
 std::optional<RemakeReturnJob> pending;bool busy=false,stop=false;
 std::uint64_t generation=0; // Results from a discarded generation are dropped.
 std::deque<std::pair<std::uint64_t,RemakeReturnResult>> results;
 std::uint64_t busySkips=0,dispatched=0,completed=0;
 static RemakeReturnResult process(RemakeReturnJob& job) {
  const auto start=std::chrono::steady_clock::now();
  RemakeReturnResult r;r.returned=std::move(job.returned);r.overlay=std::move(job.overlay);
  if(const auto temporal=r.overlay.temporalScene) {
   r.temporal=true;r.previousFrame=job.previous?job.previous->frame:0;
   r.streamReady=BuildRemakeMotionStream(job.previous.get(),*temporal,r.stream,r.streamError);
  }
  r.inputReady=BuildRemakeNeuralInput(r.returned,r.returned.frame,r.returned.producer,r.input);
  r.workerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  return r;
 }
 void run() {
  for(;;) {
   RemakeReturnJob job;std::uint64_t mine=0;
   {
    std::unique_lock<std::mutex> lock(mutex);
    wake.wait(lock,[&]{return stop||pending.has_value();});
    if(stop)return;
    job=std::move(*pending);pending.reset();busy=true;mine=generation;
   }
   RemakeReturnResult result=process(job);
   {
    std::lock_guard<std::mutex> lock(mutex);
    busy=false;++completed;
    if(!stop&&mine==generation)results.emplace_back(mine,std::move(result));
   }
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
  {std::lock_guard<std::mutex> lock(mutex);stop=true;pending.reset();}
  wake.notify_all();
  if(thread.joinable())thread.join();
  std::lock_guard<std::mutex> lock(mutex);results.clear();busy=false;thread=std::thread();
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
 // History retirement: drop the pending job and every result not yet drained,
 // including one still being prepared.
 void Discard() {
  std::lock_guard<std::mutex> lock(mutex);++generation;pending.reset();results.clear();
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
};
}
