// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include <memory>
namespace flycast::rend::neural {
enum class RemakeChannelResult { Published, Received, Empty, Busy, Closed, Invalid };
struct RemakeChannelReceipt {std::uint64_t sequence=0,digest=0;std::uint32_t bytes=0;};
struct RemakeReturnedImage {
 RemakeChannelReceipt source;std::uint64_t frame=0;ProducerIdentity producer;
 std::uint32_t width=0,height=0;std::vector<unsigned char> bgra;
};
// Windows, one producer/consumer, two bounded slots. No waits or file transport
// on Publish; busy means skip/native fallback. Not a neural acceptance history.
class RemakeLiveChannel {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 RemakeLiveChannel();~RemakeLiveChannel();
 RemakeLiveChannel(const RemakeLiveChannel&)=delete;
 RemakeLiveChannel& operator=(const RemakeLiveChannel&)=delete;
 bool CreateConsumer(const std::string& token,std::string& error);
 bool OpenPublisher(const std::string& token,std::string& error);
 void Close();
 bool IsOpen() const noexcept;
 RemakeChannelResult Publish(const remake::Packet&,RemakeChannelReceipt&,std::string&);
 RemakeChannelResult Receive(remake::Packet&,RemakeChannelReceipt&,std::string&);
 // Diagnostic final-color return only. Does not authorize presentation/history.
 RemakeChannelResult ReturnImage(const RemakeReturnedImage&,std::string&);
 RemakeChannelResult ReceiveImage(RemakeReturnedImage&,std::string&);
};
}
