// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_live_channel.h"
#include "remake_view_transport.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <streambuf>
#include <istream>
#include <ostream>
#include <cstring>
#include <algorithm>
namespace flycast::rend::neural {
namespace {
constexpr std::size_t capacity=72*1024*1024;
constexpr LONG freeSlot=0,writingSlot=1,readySlot=2,readingSlot=3;
struct alignas(64) Slot {
 volatile LONG state;std::uint32_t bytes;std::uint64_t sequence,digest;
 alignas(64) char payload[capacity];
};
struct Shared {
 volatile LONG ready,publisherPid;std::uint32_t magic,version,ownerPid;
 Slot slots[2];
};
bool name(const std::string& token,std::wstring& output) {
 if(token.empty()||token.size()>64)return false;
 for(unsigned char c:token)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return false;
 output=L"Local\\FlycastRemake-";output.append(token.begin(),token.end());return true;
}
std::uint64_t digest(const char* data,std::size_t size) {
 std::uint64_t hash=14695981039346656037ull;
 for(std::size_t i=0;i<size;++i){hash^=static_cast<unsigned char>(data[i]);hash*=1099511628211ull;}return hash;
}
class OutputBuffer:public std::streambuf {
 char* base_;std::size_t used_=0;
 std::streamsize xsputn(const char* data,std::streamsize n) override {
  if(n<0||std::uint64_t(n)>capacity-used_)return 0;
  std::memcpy(base_+used_,data,std::size_t(n));used_+=std::size_t(n);return n;
 }
 int_type overflow(int_type c) override {
  if(traits_type::eq_int_type(c,traits_type::eof()))return traits_type::not_eof(c);
  const char value=traits_type::to_char_type(c);return xsputn(&value,1)==1?c:traits_type::eof();
 }
public:
 explicit OutputBuffer(char* data):base_(data){}
 std::size_t size()const{return used_;}
};
class InputBuffer:public std::streambuf {
public:InputBuffer(char* data,std::size_t size){setg(data,data,data+size);}
};
struct SlotGuard {Slot* slot;~SlotGuard(){if(slot)InterlockedExchange(&slot->state,freeSlot);}};
}
struct RemakeLiveChannel::Impl {
 HANDLE mapping=nullptr,peer=nullptr;Shared* shared=nullptr;bool owner=false;
 std::uint64_t sequence=0,frame=0;ProducerIdentity producer;
 ~Impl(){
  if(shared){if(owner)InterlockedExchange(&shared->ready,0);UnmapViewOfFile(shared);}
  if(peer)CloseHandle(peer);if(mapping)CloseHandle(mapping);
 }
 bool live()const{return shared&&InterlockedCompareExchange(&shared->ready,1,1)==1
  &&(owner||!peer||WaitForSingleObject(peer,0)==WAIT_TIMEOUT);}
};
RemakeLiveChannel::RemakeLiveChannel()=default;
RemakeLiveChannel::~RemakeLiveChannel()=default;
void RemakeLiveChannel::Close(){impl_.reset();}
bool RemakeLiveChannel::IsOpen()const noexcept{return bool(impl_);}
bool RemakeLiveChannel::CreateConsumer(const std::string& token,std::string& error) {
 if(impl_){error="channel-already-open";return false;}
 std::wstring path;if(!name(token,path)){error="channel-token";return false;}
 auto p=std::make_unique<Impl>();SetLastError(ERROR_SUCCESS);
 p->mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(sizeof(Shared)),path.c_str());
 if(!p->mapping||GetLastError()==ERROR_ALREADY_EXISTS){error="channel-create-or-existing";return false;}
 p->shared=static_cast<Shared*>(MapViewOfFile(p->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
 if(!p->shared){error="channel-map";return false;}
 p->owner=true;p->shared->magic=0x434d5246;p->shared->version=1;p->shared->ownerPid=GetCurrentProcessId();
 // A newly created pagefile-backed mapping is zero-initialized; publish header last.
 InterlockedExchange(&p->shared->ready,1);impl_=std::move(p);error.clear();return true;
}
bool RemakeLiveChannel::OpenPublisher(const std::string& token,std::string& error) {
 if(impl_){error="channel-already-open";return false;}
 std::wstring path;if(!name(token,path)){error="channel-token";return false;}
 auto p=std::make_unique<Impl>();p->mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,path.c_str());
 if(!p->mapping){error="channel-consumer-unavailable";return false;}
 p->shared=static_cast<Shared*>(MapViewOfFile(p->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
 if(!p->shared||!p->live()||p->shared->magic!=0x434d5246||p->shared->version!=1){error="channel-header";return false;}
 p->peer=OpenProcess(SYNCHRONIZE,FALSE,p->shared->ownerPid);
 if(!p->peer||!p->live()){error="channel-consumer-ended";return false;}
 if(InterlockedCompareExchange(&p->shared->publisherPid,LONG(GetCurrentProcessId()),0)!=0){error="channel-publisher-already-claimed";return false;}
 impl_=std::move(p);error.clear();return true;
}
RemakeChannelResult RemakeLiveChannel::Publish(const remake::Packet& packet,RemakeChannelReceipt& receipt,std::string& error) {
 if(!impl_||impl_->owner){error="channel-publisher-role";return RemakeChannelResult::Invalid;}
 auto& p=*impl_;if(!p.live()){error="channel-consumer-closed";return RemakeChannelResult::Closed;}
 if(p.sequence==UINT64_MAX || (p.sequence && (packet.frame<=p.frame || packet.producer.epoch!=p.producer.epoch
  ||packet.producer.ordinal<=p.producer.ordinal||packet.producer.cycle<p.producer.cycle))) {
  error="channel-source-order";return RemakeChannelResult::Invalid;
 }
 Slot* slot=nullptr;for(auto& candidate:p.shared->slots)if(InterlockedCompareExchange(&candidate.state,writingSlot,freeSlot)==freeSlot){slot=&candidate;break;}
 if(!slot){error="channel-busy-native-fallback";return RemakeChannelResult::Busy;}
 SlotGuard guard{slot};
 try {
  OutputBuffer buffer(slot->payload);std::ostream output(&buffer);
  if(!SerializeRemakeViewPacket(output,packet,error))return RemakeChannelResult::Invalid;
  slot->bytes=std::uint32_t(buffer.size());slot->sequence=p.sequence+1;slot->digest=digest(slot->payload,slot->bytes);
  receipt={slot->sequence,slot->digest,slot->bytes};p.sequence=slot->sequence;p.frame=packet.frame;p.producer=packet.producer;
  guard.slot=nullptr;InterlockedExchange(&slot->state,readySlot);error.clear();return RemakeChannelResult::Published;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
RemakeChannelResult RemakeLiveChannel::Receive(remake::Packet& output,RemakeChannelReceipt& receipt,std::string& error) {
 if(!impl_||!impl_->owner){error="channel-consumer-role";return RemakeChannelResult::Invalid;}
 auto& p=*impl_;if(!p.live()){error="channel-closed";return RemakeChannelResult::Closed;}
 Slot* slot=nullptr;
 for(auto& candidate:p.shared->slots)if(InterlockedCompareExchange(&candidate.state,readySlot,readySlot)==readySlot
  &&(!slot||candidate.sequence<slot->sequence))slot=&candidate;
 if(!slot){error.clear();return RemakeChannelResult::Empty;}
 if(InterlockedCompareExchange(&slot->state,readingSlot,readySlot)!=readySlot){error="channel-read-owner";return RemakeChannelResult::Invalid;}
 SlotGuard guard{slot};
 if(!slot->bytes||slot->bytes>capacity||slot->sequence!=p.sequence+1||slot->digest!=digest(slot->payload,slot->bytes)) {
  error="channel-payload-integrity-or-sequence";return RemakeChannelResult::Invalid;
 }
 try {
  InputBuffer buffer(slot->payload,slot->bytes);std::istream input(&buffer);remake::Packet packet;
  if(!DeserializeRemakeViewPacket(input,packet,error))return RemakeChannelResult::Invalid;
  receipt={slot->sequence,slot->digest,slot->bytes};p.sequence=slot->sequence;output=std::move(packet);
  error.clear();return RemakeChannelResult::Received;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
}
