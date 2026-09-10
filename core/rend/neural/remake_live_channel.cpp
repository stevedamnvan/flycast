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
#include <cmath>
namespace flycast::rend::neural {
namespace {
constexpr std::size_t capacity=72*1024*1024;
constexpr LONG freeSlot=0,writingSlot=1,readySlot=2,readingSlot=3;
struct alignas(64) Slot {
 volatile LONG state;std::uint32_t bytes;std::uint64_t sequence,digest;
 alignas(64) char payload[capacity];
};
struct ImageSlot {
 volatile LONG imageState;
 RemakeChannelReceipt imageSource;
 std::uint64_t imageFrame,imageEpoch,imageOrdinal,imageCycle,imageDigest;
 unsigned char imagePixels[640*480*4];
	std::uint32_t depthCount;float nearPlane,farPlane;
	std::uint64_t depthDigest;float depthPixels[640*480];
};
struct Shared {
 volatile LONG ready,publisherPid;std::uint32_t magic,version,ownerPid;
 Slot slots[2];
 ImageSlot images[2];
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
bool RequestRemakeSession(const std::string& root,std::string& token,std::string& error) {
 std::wstring path;
 if(root.size()>48||!name(root,path)){error="session-root-invalid";return false;}
 path+=L"-control";
 HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,path.c_str());
 if(!mapping){error="session-controller-unavailable";return false;}
 auto* words=static_cast<volatile LONG*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,16));
 bool ok=false;
 if(words) {
  if(words[0]==0x534d5246&&words[1]==1) {
   HANDLE owner=OpenProcess(SYNCHRONIZE,FALSE,DWORD(words[3]));
   if(owner&&WaitForSingleObject(owner,0)==WAIT_TIMEOUT) {
    const LONG generation=InterlockedCompareExchange(words+2,0,0);
    if(generation>=0&&generation<8&&InterlockedCompareExchange(words+2,generation+1,generation)==generation) {
     token=root+"-g"+std::to_string(generation+1);ok=true;
    }
   }
   if(owner)CloseHandle(owner);
  }
  UnmapViewOfFile(const_cast<LONG*>(words));
 }
 CloseHandle(mapping);
 error=ok?"":"session-controller-invalid-or-exhausted";return ok;
}

struct RemakeLiveChannel::Impl {
 HANDLE mapping=nullptr,peer=nullptr;Shared* shared=nullptr;bool owner=false,publisherClaimed=false;
 std::uint64_t sequence=0,frame=0;ProducerIdentity producer;
 struct Source {RemakeChannelReceipt receipt;std::uint64_t frame=0;ProducerIdentity producer;float nearPlane=0,farPlane=0;};
 Source sources[2];std::uint64_t returnedSequence=0;
 ~Impl(){
  if(shared){if(owner||publisherClaimed)InterlockedExchange(&shared->ready,0);UnmapViewOfFile(shared);}
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
 p->owner=true;p->shared->magic=0x434d5246;p->shared->version=4;p->shared->ownerPid=GetCurrentProcessId();
 // A newly created pagefile-backed mapping is zero-initialized; publish header last.
 InterlockedExchange(&p->shared->ready,1);impl_=std::move(p);error.clear();return true;
}
bool RemakeLiveChannel::OpenPublisher(const std::string& token,std::string& error) {
 if(impl_){error="channel-already-open";return false;}
 std::wstring path;if(!name(token,path)){error="channel-token";return false;}
 auto p=std::make_unique<Impl>();p->mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,path.c_str());
 if(!p->mapping){error="channel-consumer-unavailable";return false;}
 p->shared=static_cast<Shared*>(MapViewOfFile(p->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
 if(!p->shared||!p->live()||p->shared->magic!=0x434d5246||p->shared->version!=4){error="channel-header";return false;}
 p->peer=OpenProcess(SYNCHRONIZE,FALSE,p->shared->ownerPid);
 if(!p->peer||!p->live()){error="channel-consumer-ended";return false;}
 if(InterlockedCompareExchange(&p->shared->publisherPid,LONG(GetCurrentProcessId()),0)!=0){error="channel-publisher-already-claimed";return false;}
 p->publisherClaimed=true;
 impl_=std::move(p);error.clear();return true;
}
bool RemakeLiveChannel::HasReturnCredit()const noexcept {
 if(!impl_||impl_->owner||!impl_->live()||impl_->sequence==UINT64_MAX)return false;
 const auto& pending=impl_->sources[(impl_->sequence+1)%2];
 if(pending.frame&&pending.receipt.sequence>impl_->returnedSequence)return false;
 for(auto& slot:impl_->shared->slots)
  if(InterlockedCompareExchange(&slot.state,freeSlot,freeSlot)==freeSlot)return true;
 return false;
}
RemakeChannelResult RemakeLiveChannel::PublishForReturn(const remake::Packet& packet,RemakeChannelReceipt& receipt,std::string& error) {
 if(!impl_||impl_->owner){error="channel-publisher-role";return RemakeChannelResult::Invalid;}
 if(!impl_->live()){error="channel-consumer-closed";return RemakeChannelResult::Closed;}
 if(impl_->sequence!=UINT64_MAX) {
  const auto& pending=impl_->sources[(impl_->sequence+1)%2];
  if(pending.frame&&pending.receipt.sequence>impl_->returnedSequence) {
   error="channel-return-credit-busy";return RemakeChannelResult::Busy;
  }
 }
 return Publish(packet,receipt,error);
}
unsigned RemakeLiveChannel::ExpireReturns(std::uint64_t currentFrame,const ProducerIdentity& current,std::uint64_t maxAge) {
 if(!impl_||impl_->owner||!currentFrame||!current.Available())return 0;
 unsigned expired=0;
 for(auto& source:impl_->sources) {
  if(source.frame&&source.receipt.sequence>impl_->returnedSequence
   &&(source.producer.epoch!=current.epoch||source.frame>currentFrame||currentFrame-source.frame>maxAge)) {
   source={};++expired;
  }
 }
 return expired;
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
  p.sources[p.sequence%2]={receipt,packet.frame,packet.producer,packet.camera.nearPlane,packet.camera.farPlane};
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
  p.sources[p.sequence%2]={receipt,output.frame,output.producer,output.camera.nearPlane,output.camera.farPlane};
  error.clear();return RemakeChannelResult::Received;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
namespace {
bool sameReceipt(const RemakeChannelReceipt& a,const RemakeChannelReceipt& b) {
 return a.sequence==b.sequence&&a.digest==b.digest&&a.bytes==b.bytes;
}
}
RemakeChannelResult RemakeLiveChannel::ReturnImage(const RemakeReturnedImage& image,std::string& error) {
 if(!impl_||!impl_->owner){error="return-consumer-role";return RemakeChannelResult::Invalid;}
 auto& p=*impl_;auto& s=p.shared->images[image.source.sequence%2];
 if(!p.live()){error="return-closed";return RemakeChannelResult::Closed;}
 const auto& source=p.sources[image.source.sequence%2];
 if(!image.source.sequence||image.source.sequence<=p.returnedSequence||!sameReceipt(image.source,source.receipt)
  ||image.frame!=source.frame||image.producer.epoch!=source.producer.epoch
  ||image.producer.ordinal!=source.producer.ordinal||image.producer.cycle!=source.producer.cycle
  ||image.width!=640||image.height!=480||image.bgra.size()!=sizeof(s.imagePixels)) {
  error="return-source-or-format";return RemakeChannelResult::Invalid;
 }
	const bool hasDepth=!image.projectionDepth.empty();
	const char* depthError=nullptr;
	if(hasDepth) {
		if(image.projectionDepth.size()!=640*480)depthError="return-depth-extent";
		else if(image.nearPlane!=source.nearPlane||image.farPlane!=source.farPlane)depthError="return-depth-projection";
		else if(!std::all_of(image.projectionDepth.begin(),image.projectionDepth.end(),[](float v){return std::isfinite(v);}))depthError="return-depth-nonfinite";
		else if(!std::all_of(image.projectionDepth.begin(),image.projectionDepth.end(),[](float v){return v>=0&&v<=1;}))depthError="return-depth-range";
	} else if(image.nearPlane!=0||image.farPlane!=0)depthError="return-depth-missing";
	if(depthError){error=depthError;return RemakeChannelResult::Invalid;}
 if(InterlockedCompareExchange(&s.imageState,writingSlot,freeSlot)!=freeSlot){error="return-busy";return RemakeChannelResult::Busy;}
 s.imageSource=image.source;s.imageFrame=image.frame;s.imageEpoch=image.producer.epoch;
 s.imageOrdinal=image.producer.ordinal;s.imageCycle=image.producer.cycle;
 std::memcpy(s.imagePixels,image.bgra.data(),sizeof(s.imagePixels));
 s.imageDigest=digest(reinterpret_cast<const char*>(s.imagePixels),sizeof(s.imagePixels));
	s.depthCount=hasDepth?640*480:0;s.nearPlane=image.nearPlane;s.farPlane=image.farPlane;s.depthDigest=0;
	if(hasDepth){std::memcpy(s.depthPixels,image.projectionDepth.data(),sizeof(s.depthPixels));s.depthDigest=digest(reinterpret_cast<const char*>(s.depthPixels),sizeof(s.depthPixels));}
 p.returnedSequence=image.source.sequence;InterlockedExchange(&s.imageState,readySlot);
 error.clear();return RemakeChannelResult::Published;
}
RemakeChannelResult RemakeLiveChannel::ReceiveImage(RemakeReturnedImage& output,std::string& error) {
 if(!impl_||impl_->owner){error="return-publisher-role";return RemakeChannelResult::Invalid;}
 auto& p=*impl_;
 // One producer and one receiver. Ready slots are immutable until this receiver
 // claims them; choose oldest to preserve accepted source order.
 ImageSlot* oldest=nullptr;
 for(auto& slot:p.shared->images)
  if(InterlockedCompareExchange(&slot.imageState,readySlot,readySlot)==readySlot
   &&(!oldest||slot.imageSource.sequence<oldest->imageSource.sequence))oldest=&slot;
 if(!oldest){error.clear();return p.live()?RemakeChannelResult::Empty:RemakeChannelResult::Closed;}
 auto& s=*oldest;
 // A completed slot remains readable after orderly consumer close.
 if(InterlockedCompareExchange(&s.imageState,readingSlot,readySlot)!=readySlot) {
  error.clear();return p.live()?RemakeChannelResult::Empty:RemakeChannelResult::Closed;
 }
 struct Release {volatile LONG* state;~Release(){InterlockedExchange(state,freeSlot);}} release{&s.imageState};
 const auto& source=p.sources[s.imageSource.sequence%2];
 if(!s.imageSource.sequence||s.imageSource.sequence<=p.returnedSequence||!sameReceipt(s.imageSource,source.receipt)
  ||s.imageFrame!=source.frame||s.imageEpoch!=source.producer.epoch||s.imageOrdinal!=source.producer.ordinal
  ||s.imageCycle!=source.producer.cycle||s.imageDigest!=digest(reinterpret_cast<const char*>(s.imagePixels),sizeof(s.imagePixels))) {
  error="return-stale-source-or-integrity";return RemakeChannelResult::Invalid;
 }
 try {
  RemakeReturnedImage image;image.source=s.imageSource;image.frame=s.imageFrame;image.producer=source.producer;
  image.width=640;image.height=480;image.bgra.assign(s.imagePixels,s.imagePixels+sizeof(s.imagePixels));
	if(s.depthCount) {
		if(s.depthCount!=640*480||s.nearPlane!=source.nearPlane||s.farPlane!=source.farPlane
			||s.depthDigest!=digest(reinterpret_cast<const char*>(s.depthPixels),sizeof(s.depthPixels))
			||!std::all_of(s.depthPixels,s.depthPixels+640*480,[](float v){return std::isfinite(v)&&v>=0&&v<=1;})) {
			error="return-depth-integrity";return RemakeChannelResult::Invalid;
		}
		image.projectionDepth.assign(s.depthPixels,s.depthPixels+640*480);image.nearPlane=s.nearPlane;image.farPlane=s.farPlane;
	}else if(s.nearPlane!=0||s.farPlane!=0||s.depthDigest!=0){error="return-depth-empty-header";return RemakeChannelResult::Invalid;}
  output=std::move(image);p.returnedSequence=s.imageSource.sequence;error.clear();return RemakeChannelResult::Received;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
}
