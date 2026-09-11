// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
namespace flycast::rend::neural {
// Single-owner synchronous chunk executor. Retains at most five helper threads;
// the caller executes chunk zero. No queued frames and no borrowed work survives
// Run. Exceptions are selected in chunk order after every active chunk finishes.
class RemakeChunkWorkers {
 std::mutex mutex;std::condition_variable wake,done;
 std::vector<std::thread> threads;
 std::function<void(unsigned)> work;
 std::array<std::exception_ptr,6> errors{};
 unsigned count=0,remaining=0;std::uint64_t generation=0;bool stop=false;
 void worker(unsigned index,std::uint64_t seen) {
  for(;;) {
   {
    std::unique_lock<std::mutex> lock(mutex);
    wake.wait(lock,[&]{return stop||generation!=seen;});
    if(stop)return;
    seen=generation;if(index>=count)continue;
   }
   try{work(index);}catch(...){errors[index]=std::current_exception();}
   {std::lock_guard<std::mutex> lock(mutex);--remaining;}
   done.notify_one();
  }
 }
public:
 RemakeChunkWorkers(){threads.reserve(5);}
 RemakeChunkWorkers(const RemakeChunkWorkers&)=delete;
 RemakeChunkWorkers& operator=(const RemakeChunkWorkers&)=delete;
 ~RemakeChunkWorkers(){
  {std::lock_guard<std::mutex> lock(mutex);stop=true;}
  wake.notify_all();for(auto& t:threads)t.join();
 }
 void Run(unsigned chunks,const std::function<void(unsigned)>& callback) {
  if(chunks<1||chunks>6)throw std::invalid_argument("remake-chunk-count");
  // Start new helpers before publishing work. If creation throws, existing
  // helpers remain idle and are still owned by this object's destructor.
  while(threads.size()+1<chunks){
   const unsigned index=unsigned(threads.size())+1;const auto seen=generation;
   threads.emplace_back([this,index,seen]{worker(index,seen);});
  }
  {
   std::lock_guard<std::mutex> lock(mutex);
   work=callback;errors={};count=chunks;remaining=chunks-1;++generation;
  }
  wake.notify_all();
  try{work(0);}catch(...){errors[0]=std::current_exception();}
  {
   std::unique_lock<std::mutex> lock(mutex);
   done.wait(lock,[&]{return remaining==0;});work={};
  }
  for(unsigned i=0;i<chunks;++i)if(errors[i])std::rethrow_exception(errors[i]);
 }
};
}
