// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include <memory>
#include <mutex>
namespace flycast::rend::neural {
// Opt-in launcher-owned control mapping; allocates a new token, never reclaims one.
bool RequestRemakeSession(const std::string& root,std::string& token,std::string& error);
enum class RemakeChannelResult { Published, Received, Empty, Busy, Closed, Invalid };
struct RemakeChannelReceipt {std::uint64_t sequence=0,digest=0;std::uint32_t bytes=0;};
struct RemakeReturnedImage {
 RemakeChannelReceipt source;std::uint64_t frame=0;ProducerIdentity producer;
 std::uint32_t width=0,height=0;std::vector<unsigned char> bgra;
	// Optional same-frame public depth; projection interpretation still experimental.
	std::vector<float> projectionDepth;float nearPlane=0,farPlane=0;
};
// Windows, one producer/consumer, two bounded slots. No waits or file transport
// on Publish; busy means skip/native fallback. Not a neural acceptance history.
// Host-side bookkeeping is mutex-protected so a feed worker may publish while
// the render thread receives, expires and closes; the long serialization and
// digest run outside the lock on a mapping kept alive by the publisher.
class RemakeLiveChannel {
 struct Impl;std::shared_ptr<Impl> impl_;mutable std::mutex mutex_;
public:
 RemakeLiveChannel();~RemakeLiveChannel();
 RemakeLiveChannel(const RemakeLiveChannel&)=delete;
 RemakeLiveChannel& operator=(const RemakeLiveChannel&)=delete;
 bool CreateConsumer(const std::string& token,std::string& error);
 bool OpenPublisher(const std::string& token,std::string& error);
 void Close();
 bool IsOpen() const noexcept;
 RemakeChannelResult Publish(const remake::Packet&,RemakeChannelReceipt&,std::string&);
 // Return-aware producer: consumed source slots do not release pending image
 // ownership. Busy skips without waiting; legacy one-way Publish is unchanged.
 RemakeChannelResult PublishForReturn(const remake::Packet&,RemakeChannelReceipt&,std::string&);
 bool HasReturnCredit() const noexcept;
 // Diagnostic: publisher sequence, highest returned sequence, each source's
 // frame and sequence, and the transport slot states (free/writing/ready/reading).
 std::string DescribeReturnCredit() const;
 // Retire expired source ownership, not neural history. Late replies reject.
 unsigned ExpireReturns(std::uint64_t currentFrame,const ProducerIdentity&,std::uint64_t maxAge);
 RemakeChannelResult Receive(remake::Packet&,RemakeChannelReceipt&,std::string&);
 // D-219: named auto-reset events signal a published packet (consumer waits)
 // and a returned image (publisher waits), so neither side polls with a
 // timer-resolution sleep. True when signaled; false on timeout or no event.
 bool WaitForPublished(unsigned milliseconds) const noexcept;
 bool WaitForReturned(unsigned milliseconds) const noexcept;
 // Diagnostic final-color return only. Does not authorize presentation/history.
 RemakeChannelResult ReturnImage(const RemakeReturnedImage&,std::string&);
 RemakeChannelResult ReceiveImage(RemakeReturnedImage&,std::string&);
};
}
