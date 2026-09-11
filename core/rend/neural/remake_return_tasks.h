// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <condition_variable>
#include <mutex>
#include <thread>

namespace flycast::rend::neural {
// One reusable CPU worker. Run returns only after both independent tasks have
// completed; task contexts may therefore live on the caller's stack. No GPU
// calls belong here. One caller at a time, and tasks must not throw.
class RemakeReturnTasks {
 using Task=void(*)(void*) noexcept;
 std::mutex mutex;
 std::condition_variable wake,done;
 Task task=nullptr;
 void* context=nullptr;
 bool pending=false,stopping=false;
 std::thread worker;
public:
 RemakeReturnTasks():worker([this]{
  std::unique_lock<std::mutex> lock(mutex);
  for(;;) {
   wake.wait(lock,[this]{return pending||stopping;});
   if(stopping)return;
   const auto fn=task;auto* data=context;
   lock.unlock();fn(data);lock.lock();
   pending=false;done.notify_one();
  }
 }){}
 ~RemakeReturnTasks(){
  {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
  wake.notify_one();worker.join();
 }
 RemakeReturnTasks(const RemakeReturnTasks&)=delete;
 RemakeReturnTasks& operator=(const RemakeReturnTasks&)=delete;
 void Run(Task background,void* backgroundContext,Task foreground,void* foregroundContext) {
  {std::lock_guard<std::mutex> lock(mutex);task=background;context=backgroundContext;pending=true;}
  wake.notify_one();foreground(foregroundContext);
  std::unique_lock<std::mutex> lock(mutex);done.wait(lock,[this]{return !pending;});
 }
};
}
