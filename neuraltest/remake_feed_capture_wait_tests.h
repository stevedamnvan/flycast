// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_feed_worker.h"
#include <future>
template<class Suite>
void TestRemakeFeedCaptureWait(Suite& suite) {
 using namespace flycast::rend::neural;
 RemakeFeedWorker worker;worker.Start();
 std::promise<void> entered,release;auto released=release.get_future().share();
 auto job=[](unsigned frame){RemakeFeedJob j;j.frame=frame;j.publish=[](const remake::Packet&,RemakeChannelReceipt&,std::string&){return RemakeChannelResult::Invalid;};return j;};
 auto first=job(1);first.publish=[&](const remake::Packet&,RemakeChannelReceipt&,std::string&){entered.set_value();released.wait();return RemakeChannelResult::Invalid;};
 suite.Expect(worker.Dispatch(std::move(first)),"capture wait fixture active enqueue");
 const bool active=entered.get_future().wait_for(std::chrono::seconds(2))==std::future_status::ready;
 suite.Expect(active,"capture wait fixture entered publish");
 if(!active){release.set_value();worker.Stop();return;}
 suite.Expect(worker.Dispatch(job(2)),"capture wait preserves pending slot");
 std::promise<void> thirdEntered;
 auto third=job(3);third.publish=[&](const remake::Packet&,RemakeChannelReceipt&,std::string&){thirdEntered.set_value();return RemakeChannelResult::Invalid;};
 suite.Expect(!worker.DispatchForCapture(std::move(third),std::chrono::milliseconds(0))&&third.publish&&worker.Dispatched()==2,
  "capture timeout does not consume or replace caller job");
 auto waiter=std::async(std::launch::async,[&]{return worker.DispatchForCapture(std::move(third),std::chrono::milliseconds(1000));});
 release.set_value();suite.Expect(waiter.get(),"capture waiter acquires consumed pending slot");
 const bool thirdRan=thirdEntered.get_future().wait_for(std::chrono::seconds(2))==std::future_status::ready;
 // Callback entry proves both preceding jobs completed in the single worker.
 auto results=worker.Drain();
 suite.Expect(thirdRan&&results.size()>=2&&results[0].frame==1&&results[1].frame==2,
  "capture slot wait retains FIFO ownership of active and pending jobs");
 worker.Stop();
 // Stop must wake a slot waiter while the active callback remains held.
 RemakeFeedWorker stopped;stopped.Start();std::promise<void> in2,free2;auto freed=free2.get_future().share();
 auto blocked=job(4);blocked.publish=[&](const remake::Packet&,RemakeChannelReceipt&,std::string&){in2.set_value();freed.wait();return RemakeChannelResult::Invalid;};
 stopped.Dispatch(std::move(blocked));const bool ready=in2.get_future().wait_for(std::chrono::seconds(2))==std::future_status::ready;
 if(!ready){free2.set_value();stopped.Stop();suite.Expect(false,"stop waiter fixture entered");return;}
 stopped.Dispatch(job(5));auto last=job(6);
 auto waiting=std::async(std::launch::async,[&]{return stopped.DispatchForCapture(std::move(last),std::chrono::milliseconds(1000));});
 auto stopping=std::async(std::launch::async,[&]{stopped.Stop();});
 const bool woke=waiting.wait_for(std::chrono::milliseconds(500))==std::future_status::ready;
 suite.Expect(woke,"stop wakes capture slot waiter before active callback completes");
 free2.set_value();suite.Expect(!waiting.get()&&last.publish,"stopped wait preserves caller ownership");stopping.get();
}
