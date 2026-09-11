// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_live_channel.h"
#include "remake_cpu_scope.h"
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
// D-218: sources that may be outstanding (published, not yet returned) at once.
constexpr unsigned kInFlight=3;
constexpr LONG freeSlot=0,writingSlot=1,readySlot=2,readingSlot=3;
struct alignas(64) Slot {
 volatile LONG state;std::uint32_t bytes;std::uint64_t sequence,digest;
 alignas(64) char payload[capacity];
};
struct ImageSlot {
 volatile LONG imageState;
 RemakeChannelReceipt imageSource;
 std::uint64_t imageFrame,imageEpoch,imageOrdinal,imageCycle,imageDigest;
 unsigned char imagePixels[1280*960*4];
	std::uint32_t depthCount;float nearPlane,farPlane;
	std::uint64_t depthDigest;float depthPixels[1280*960];
};
struct Shared {
 volatile LONG ready,publisherPid;std::uint32_t magic,version,ownerPid;
 std::uint32_t width,height;
 Slot slots[kInFlight];
 ImageSlot images[kInFlight];
};
bool name(const std::string& token,std::wstring& output) {
 if(token.empty()||token.size()>64)return false;
 for(unsigned char c:token)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return false;
 output=L"Local\\FlycastRemake-";output.append(token.begin(),token.end());return true;
}
// D-219: transport-only digest for the returned image and depth slots (not
// the packet receipt digest, which the archives persist): FNV-1a over 8-byte
// words with a byte tail. Both ends of the channel share this function.
std::uint64_t imageDigest64(const char* data,std::size_t size) {
 std::uint64_t hash=14695981039346656037ull;std::size_t i=0;
 for(;i+8<=size;i+=8){std::uint64_t word;std::memcpy(&word,data+i,8);hash^=word;hash*=1099511628211ull;}
 for(;i<size;++i){hash^=static_cast<unsigned char>(data[i]);hash*=1099511628211ull;}
 return hash;
}
std::uint64_t digest(const char* data,std::size_t size) {
 // Byte-serial FNV-1a: the locked archives persist this receipt digest and
 // the replay recomputes it (LOG780), so the function is not changed.
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
 Source sources[kInFlight];std::uint64_t returnedSequence=0;
 HANDLE publishedEvent=nullptr,returnedEvent=nullptr; // D-219 wake events (named, auto-reset).
 void openEvents(const std::wstring& path) {
  publishedEvent=CreateEventW(nullptr,FALSE,FALSE,(path+L"-published").c_str());
  returnedEvent=CreateEventW(nullptr,FALSE,FALSE,(path+L"-returned").c_str());
 }
 ~Impl(){
  if(shared){if(owner||publisherClaimed)InterlockedExchange(&shared->ready,0);UnmapViewOfFile(shared);}
  // Wake the peer so a wait on a closing channel observes the closed header.
  if(publishedEvent){SetEvent(publishedEvent);CloseHandle(publishedEvent);}
  if(returnedEvent){SetEvent(returnedEvent);CloseHandle(returnedEvent);}
  if(peer)CloseHandle(peer);if(mapping)CloseHandle(mapping);
 }
 bool live()const{return shared&&InterlockedCompareExchange(&shared->ready,1,1)==1
  &&(owner||!peer||WaitForSingleObject(peer,0)==WAIT_TIMEOUT);}
};
RemakeLiveChannel::RemakeLiveChannel()=default;
RemakeLiveChannel::~RemakeLiveChannel()=default;
void RemakeLiveChannel::Close(){std::lock_guard<std::mutex> lock(mutex_);impl_.reset();}
bool RemakeLiveChannel::IsOpen()const noexcept{std::lock_guard<std::mutex> lock(mutex_);return bool(impl_);}
bool RemakeLiveChannel::CreateConsumer(const std::string& token,std::string& error) {
 std::lock_guard<std::mutex> lock(mutex_);
 if(!SelectedRemakeExtent().Valid()){error="channel-extent";return false;}
 if(impl_){error="channel-already-open";return false;}
 std::wstring path;if(!name(token,path)){error="channel-token";return false;}
 auto p=std::make_shared<Impl>();SetLastError(ERROR_SUCCESS);
 p->mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(sizeof(Shared)),path.c_str());
 if(!p->mapping||GetLastError()==ERROR_ALREADY_EXISTS){error="channel-create-or-existing";return false;}
 p->shared=static_cast<Shared*>(MapViewOfFile(p->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
 if(!p->shared){error="channel-map";return false;}
 p->owner=true;p->shared->magic=0x434d5246;p->shared->version=5;p->shared->ownerPid=GetCurrentProcessId();
 p->shared->width=RemakeWidth();p->shared->height=RemakeHeight();
 p->openEvents(path);
 // A newly created pagefile-backed mapping is zero-initialized; publish header last.
 InterlockedExchange(&p->shared->ready,1);impl_=std::move(p);error.clear();return true;
}
bool RemakeLiveChannel::OpenPublisher(const std::string& token,std::string& error) {
 std::lock_guard<std::mutex> lock(mutex_);
 if(impl_){error="channel-already-open";return false;}
 std::wstring path;if(!name(token,path)){error="channel-token";return false;}
 auto p=std::make_shared<Impl>();p->mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,path.c_str());
 if(!p->mapping){error="channel-consumer-unavailable";return false;}
 p->shared=static_cast<Shared*>(MapViewOfFile(p->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
 if(!p->shared||!p->live()||p->shared->magic!=0x434d5246||p->shared->version!=5){error="channel-header";return false;}
 if(!SelectedRemakeExtent().Valid()||p->shared->width!=RemakeWidth()||p->shared->height!=RemakeHeight()){error="channel-extent";return false;}
 p->peer=OpenProcess(SYNCHRONIZE,FALSE,p->shared->ownerPid);
 if(!p->peer||!p->live()){error="channel-consumer-ended";return false;}
 if(InterlockedCompareExchange(&p->shared->publisherPid,LONG(GetCurrentProcessId()),0)!=0){error="channel-publisher-already-claimed";return false;}
 p->publisherClaimed=true;p->openEvents(path);
 impl_=std::move(p);error.clear();return true;
}
bool RemakeLiveChannel::WaitForPublished(unsigned milliseconds)const noexcept {
 HANDLE event=nullptr;
 {std::lock_guard<std::mutex> lock(mutex_);if(impl_)event=impl_->publishedEvent;}
 if(!event){Sleep(milliseconds?1:0);return false;}
 return WaitForSingleObject(event,milliseconds)==WAIT_OBJECT_0;
}
bool RemakeLiveChannel::WaitForReturned(unsigned milliseconds)const noexcept {
 HANDLE event=nullptr;
 {std::lock_guard<std::mutex> lock(mutex_);if(impl_)event=impl_->returnedEvent;}
 if(!event){Sleep(milliseconds?1:0);return false;}
 return WaitForSingleObject(event,milliseconds)==WAIT_OBJECT_0;
}
bool RemakeLiveChannel::HasReturnCredit()const noexcept {
 std::lock_guard<std::mutex> lock(mutex_);
 if(!impl_||impl_->owner||!impl_->live()||impl_->sequence==UINT64_MAX)return false;
 const auto& pending=impl_->sources[(impl_->sequence+1)%kInFlight];
 if(pending.frame&&pending.receipt.sequence>impl_->returnedSequence)return false;
 for(auto& slot:impl_->shared->slots)
  if(InterlockedCompareExchange(&slot.state,freeSlot,freeSlot)==freeSlot)return true;
 return false;
}
std::string RemakeLiveChannel::DescribeReturnCredit() const {
 std::lock_guard<std::mutex> lock(mutex_);
 if(!impl_||!impl_->shared)return "channel=closed";
 std::string out="sequence="+std::to_string(impl_->sequence)+" returned="+std::to_string(impl_->returnedSequence)+" sources=";
 for(unsigned i=0;i<kInFlight;++i){const auto& s=impl_->sources[i];out+=std::to_string(s.receipt.sequence)+":"+std::to_string(s.frame)+(i+1<kInFlight?",":"");}
 out+=" slots=";
 for(unsigned i=0;i<kInFlight;++i){const auto state=InterlockedCompareExchange(&impl_->shared->slots[i].state,0,0);out+=std::to_string(state)+(i+1<kInFlight?",":"");}
 out+=" images=";
 for(unsigned i=0;i<kInFlight;++i){const auto state=InterlockedCompareExchange(&impl_->shared->images[i].imageState,0,0);out+=std::to_string(state)+(i+1<kInFlight?",":"");}
 return out;
}
RemakeChannelResult RemakeLiveChannel::PublishForReturn(const remake::Packet& packet,RemakeChannelReceipt& receipt,std::string& error) {
 {
  std::lock_guard<std::mutex> lock(mutex_);
  if(!impl_||impl_->owner){error="channel-publisher-role";return RemakeChannelResult::Invalid;}
  if(!impl_->live()){error="channel-consumer-closed";return RemakeChannelResult::Closed;}
  if(impl_->sequence!=UINT64_MAX) {
   const auto& pending=impl_->sources[(impl_->sequence+1)%kInFlight];
   if(pending.frame&&pending.receipt.sequence>impl_->returnedSequence) {
    error="channel-return-credit-busy";return RemakeChannelResult::Busy;
   }
  }
 }
 return Publish(packet,receipt,error);
}
unsigned RemakeLiveChannel::ExpireReturns(std::uint64_t currentFrame,const ProducerIdentity& current,std::uint64_t maxAge) {
 std::lock_guard<std::mutex> lock(mutex_);
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
 const auto ordered=[&](const Impl& p) {
  return p.sequence!=UINT64_MAX && !(p.sequence && (packet.frame<=p.frame || packet.producer.epoch!=p.producer.epoch
   ||packet.producer.ordinal<=p.producer.ordinal||packet.producer.cycle<p.producer.cycle));
 };
 std::shared_ptr<Impl> held;
 {
  std::lock_guard<std::mutex> lock(mutex_);
  if(!impl_||impl_->owner){error="channel-publisher-role";return RemakeChannelResult::Invalid;}
  if(!impl_->live()){error="channel-consumer-closed";return RemakeChannelResult::Closed;}
  if(!ordered(*impl_)){error="channel-source-order";return RemakeChannelResult::Invalid;}
  held=impl_;
 }
 Slot* slot=nullptr;for(auto& candidate:held->shared->slots)if(InterlockedCompareExchange(&candidate.state,writingSlot,freeSlot)==freeSlot){slot=&candidate;break;}
 if(!slot){error="channel-busy-native-fallback";return RemakeChannelResult::Busy;}
 SlotGuard guard{slot};
 try {
  // Serialization and digest run without the lock; the mapping stays alive
  // through `held` even if the render thread closes the channel meanwhile.
  OutputBuffer buffer(slot->payload);std::ostream output(&buffer);
  if(!SerializeRemakeViewPacket(output,packet,error))return RemakeChannelResult::Invalid;
  const auto bytes=std::uint32_t(buffer.size());const auto hash=digest(slot->payload,bytes);
  std::lock_guard<std::mutex> lock(mutex_);
  if(impl_!=held||!held->live()){error="channel-consumer-closed";return RemakeChannelResult::Closed;}
  auto& p=*held;
  if(!ordered(p)){error="channel-source-order";return RemakeChannelResult::Invalid;}
  slot->bytes=bytes;slot->sequence=p.sequence+1;slot->digest=hash;
  receipt={slot->sequence,slot->digest,slot->bytes};p.sequence=slot->sequence;p.frame=packet.frame;p.producer=packet.producer;
  p.sources[p.sequence%kInFlight]={receipt,packet.frame,packet.producer,packet.camera.nearPlane,packet.camera.farPlane};
  guard.slot=nullptr;InterlockedExchange(&slot->state,readySlot);
  if(held->publishedEvent)SetEvent(held->publishedEvent);
  error.clear();return RemakeChannelResult::Published;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
RemakeChannelResult RemakeLiveChannel::Receive(remake::Packet& output,RemakeChannelReceipt& receipt,std::string& error) {
 std::lock_guard<std::mutex> lock(mutex_);
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
  remake::Packet packet;
  if(!DeserializeRemakeViewPacket(slot->payload,slot->bytes,packet,error))return RemakeChannelResult::Invalid;
  receipt={slot->sequence,slot->digest,slot->bytes};p.sequence=slot->sequence;output=std::move(packet);
  p.sources[p.sequence%kInFlight]={receipt,output.frame,output.producer,output.camera.nearPlane,output.camera.farPlane};
  error.clear();return RemakeChannelResult::Received;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
namespace {
bool sameReceipt(const RemakeChannelReceipt& a,const RemakeChannelReceipt& b) {
 return a.sequence==b.sequence&&a.digest==b.digest&&a.bytes==b.bytes;
}
}
RemakeChannelResult RemakeLiveChannel::ReturnImage(const RemakeReturnedImage& image,std::string& error) {
 std::lock_guard<std::mutex> lock(mutex_);
 if(!impl_||!impl_->owner){error="return-consumer-role";return RemakeChannelResult::Invalid;}
 auto& p=*impl_;auto& s=p.shared->images[image.source.sequence%kInFlight];
 if(!p.live()){error="return-closed";return RemakeChannelResult::Closed;}
 const auto& source=p.sources[image.source.sequence%kInFlight];
 if(!image.source.sequence||image.source.sequence<=p.returnedSequence||!sameReceipt(image.source,source.receipt)
  ||image.frame!=source.frame||image.producer.epoch!=source.producer.epoch
  ||image.producer.ordinal!=source.producer.ordinal||image.producer.cycle!=source.producer.cycle
  ||image.width!=RemakeWidth()||image.height!=RemakeHeight()||image.bgra.size()!=RemakePixels()*4) {
  error="return-source-or-format";return RemakeChannelResult::Invalid;
 }
	const bool hasDepth=!image.projectionDepth.empty();
	const char* depthError=nullptr;
	if(hasDepth) {
		if(image.projectionDepth.size()!=RemakePixels())depthError="return-depth-extent";
		else if(image.nearPlane!=source.nearPlane||image.farPlane!=source.farPlane)depthError="return-depth-projection";
		else if(!std::all_of(image.projectionDepth.begin(),image.projectionDepth.end(),[](float v){return std::isfinite(v);}))depthError="return-depth-nonfinite";
		else if(!std::all_of(image.projectionDepth.begin(),image.projectionDepth.end(),[](float v){return v>=0&&v<=1;}))depthError="return-depth-range";
	} else if(image.nearPlane!=0||image.farPlane!=0)depthError="return-depth-missing";
	if(depthError){error=depthError;return RemakeChannelResult::Invalid;}
 if(InterlockedCompareExchange(&s.imageState,writingSlot,freeSlot)!=freeSlot){error="return-busy";return RemakeChannelResult::Busy;}
 s.imageSource=image.source;s.imageFrame=image.frame;s.imageEpoch=image.producer.epoch;
 s.imageOrdinal=image.producer.ordinal;s.imageCycle=image.producer.cycle;
 std::memcpy(s.imagePixels,image.bgra.data(),RemakePixels()*4);
 s.imageDigest=imageDigest64(reinterpret_cast<const char*>(s.imagePixels),RemakePixels()*4);
	s.depthCount=hasDepth?RemakePixels():0;s.nearPlane=image.nearPlane;s.farPlane=image.farPlane;s.depthDigest=0;
	if(hasDepth){std::memcpy(s.depthPixels,image.projectionDepth.data(),RemakePixels()*sizeof(float));s.depthDigest=imageDigest64(reinterpret_cast<const char*>(s.depthPixels),RemakePixels()*sizeof(float));}
 p.returnedSequence=image.source.sequence;InterlockedExchange(&s.imageState,readySlot);
 if(p.returnedEvent)SetEvent(p.returnedEvent);
 error.clear();return RemakeChannelResult::Published;
}
RemakeChannelResult RemakeLiveChannel::ReceiveImage(RemakeReturnedImage& output,std::string& error) {
 std::lock_guard<std::mutex> lock(mutex_);
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
 const auto& source=p.sources[s.imageSource.sequence%kInFlight];
 static thread_local unsigned colorValidateCount=0,colorCopyCount=0,depthValidateCount=0,depthCopyCount=0;
 RemakeCpuScope colorValidateTiming("return-channel-color-validate",s.imageFrame,colorValidateCount);
 if(!s.imageSource.sequence||s.imageSource.sequence<=p.returnedSequence||!sameReceipt(s.imageSource,source.receipt)
  ||s.imageFrame!=source.frame||s.imageEpoch!=source.producer.epoch||s.imageOrdinal!=source.producer.ordinal
  ||s.imageCycle!=source.producer.cycle||s.imageDigest!=imageDigest64(reinterpret_cast<const char*>(s.imagePixels),RemakePixels()*4)) {
  error="return-stale-source-or-integrity";return RemakeChannelResult::Invalid;
 }
 try {
  colorValidateTiming.End();
  RemakeReturnedImage image;image.source=s.imageSource;image.frame=s.imageFrame;image.producer=source.producer;
  RemakeCpuScope colorCopyTiming("return-channel-color-copy",s.imageFrame,colorCopyCount);
  image.width=RemakeWidth();image.height=RemakeHeight();image.bgra.assign(s.imagePixels,s.imagePixels+RemakePixels()*4);
  colorCopyTiming.End();
	if(s.depthCount) {
		RemakeCpuScope depthValidateTiming("return-channel-depth-validate",s.imageFrame,depthValidateCount);
		if(s.depthCount!=RemakePixels()||s.nearPlane!=source.nearPlane||s.farPlane!=source.farPlane
			||s.depthDigest!=imageDigest64(reinterpret_cast<const char*>(s.depthPixels),RemakePixels()*sizeof(float))
			||!std::all_of(s.depthPixels,s.depthPixels+RemakePixels(),[](float v){return std::isfinite(v)&&v>=0&&v<=1;})) {
			error="return-depth-integrity";return RemakeChannelResult::Invalid;
		}
		depthValidateTiming.End();
		RemakeCpuScope depthCopyTiming("return-channel-depth-copy",s.imageFrame,depthCopyCount);
		image.projectionDepth.assign(s.depthPixels,s.depthPixels+RemakePixels());image.nearPlane=s.nearPlane;image.farPlane=s.farPlane;
	}else if(s.nearPlane!=0||s.farPlane!=0||s.depthDigest!=0){error="return-depth-empty-header";return RemakeChannelResult::Invalid;}
  output=std::move(image);p.returnedSequence=s.imageSource.sequence;error.clear();return RemakeChannelResult::Received;
 }catch(const std::exception& e){error=e.what();return RemakeChannelResult::Invalid;}
}
}
