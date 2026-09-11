// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <atomic>
#include <vector>
#include <utility>
#include <memory>
#include <type_traits>

namespace flycast::rend::neural {
enum class RemakeDepthValidity { Valid, Nonfinite, OutOfRange };
// Integer reductions permit vectorization without fast-math assumptions.
// Both signed zeros are valid. Every NaN payload and infinity is nonfinite;
// finite negative values and values above one are out of range.
inline RemakeDepthValidity ValidateRemakeDepth(const float* values,std::size_t count) {
 static_assert(sizeof(float)==sizeof(std::uint32_t)&&std::numeric_limits<float>::is_iec559,
  "Projection depth requires IEEE binary32");
 std::uint32_t nonfinite=0,outOfRange=0;
 for(std::size_t i=0;i<count;++i) {
  std::uint32_t bits;std::memcpy(&bits,values+i,sizeof(bits));
  const auto magnitude=bits&0x7fffffffu;
  nonfinite|=std::uint32_t(magnitude>=0x7f800000u);
  outOfRange|=std::uint32_t(magnitude>0x3f800000u)|std::uint32_t(bits>0x80000000u);
 }
 return nonfinite?RemakeDepthValidity::Nonfinite:
  outOfRange?RemakeDepthValidity::OutOfRange:RemakeDepthValidity::Valid;
}
// Const-only copies share storage. Before writable access, detach shared bytes.
// If an alias already escaped, copying takes a separate snapshot instead.
// The vector object itself is never exposed mutably.
template<class T> class RemakeImageBuffer {
 using Values=std::vector<T>;
 static std::uint64_t NewIdentity() {
  static std::atomic<std::uint64_t> next{1};
  auto value=next.load(std::memory_order_relaxed);
  while(value!=UINT64_MAX)if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
  return 0;
 }
 std::shared_ptr<Values> values;
 std::uint64_t identity=NewIdentity();
 mutable std::atomic<int> validity{-1};
 bool writableAlias=false;
 const Values& read()const {static const Values empty;return values?*values:empty;}
 Values& write() {
  if(!values)values=std::make_shared<Values>();
  else if(!values.unique())values=std::make_shared<Values>(*values);
  return *values;
 }
 void expose(){write();writableAlias=true;identity=0;validity.store(-1,std::memory_order_relaxed);}
 void replace(Values next){values=std::make_shared<Values>(std::move(next));identity=NewIdentity();writableAlias=false;validity.store(-1,std::memory_order_relaxed);}
 void changed(){identity=writableAlias?0:NewIdentity();validity.store(-1,std::memory_order_relaxed);}
public:
 RemakeImageBuffer()=default;
 RemakeImageBuffer(const Values& source):values(std::make_shared<Values>(source)){}
 RemakeImageBuffer(const RemakeImageBuffer& other)
  :values(other.writableAlias?std::make_shared<Values>(other.read()):other.values),
   identity(other.identity?other.identity:NewIdentity()),validity(int(other.Validity())){}
 RemakeImageBuffer(RemakeImageBuffer&& other)noexcept:values(std::move(other.values)),
  identity(other.identity),validity(other.validity.load(std::memory_order_relaxed)),writableAlias(other.writableAlias) {
  other.identity=0;other.validity.store(-1,std::memory_order_relaxed);
 }
 RemakeImageBuffer& operator=(const RemakeImageBuffer& other){if(this!=&other){RemakeImageBuffer copy(other);*this=std::move(copy);}return *this;}
 RemakeImageBuffer& operator=(RemakeImageBuffer&& other)noexcept {
  if(this!=&other){values=std::move(other.values);identity=other.identity;other.identity=0;writableAlias=other.writableAlias;
   validity.store(other.validity.load(std::memory_order_relaxed),std::memory_order_relaxed);
   other.validity.store(-1,std::memory_order_relaxed);}return *this;
 }
 RemakeImageBuffer& operator=(const Values& source){
  if(values&&values.unique()){*values=source;changed();}
  else replace(source);
  return *this;
 }
 std::size_t size()const{return read().size();}
 bool empty()const{return read().empty();}
 const T* data()const{return read().data();}
 T* data(){expose();return values->data();}
 auto begin()const{return read().begin();} auto end()const{return read().end();}
 auto begin(){expose();return values->begin();} auto end(){expose();return values->end();}
 const T& operator[](std::size_t i)const{return read()[i];}
 T& operator[](std::size_t i){expose();return (*values)[i];}
 const Values& Read()const{return read();}
 void clear(){replace({});}
 void pop_back(){write().pop_back();changed();}
 void resize(std::size_t count){if(count!=size()){write().resize(count);changed();}}
 void assign(std::size_t count,T value){replace(Values(count,value));}
 template<class Iterator> void assign(Iterator first,Iterator last){replace(Values(first,last));}
 bool HasReusableValidation()const{return !writableAlias&&validity.load(std::memory_order_relaxed)>=0;}
 std::uint64_t ContentIdentity()const{return identity;}
 RemakeDepthValidity Validity()const {
  if constexpr(!std::is_same_v<T,float>)return RemakeDepthValidity::Valid;
  else {
   const int cached=validity.load(std::memory_order_relaxed);
   if(!writableAlias&&cached>=0)return RemakeDepthValidity(cached);
   const auto result=ValidateRemakeDepth(read().data(),read().size());
   if(!writableAlias)validity.store(int(result),std::memory_order_relaxed);
   return result;
  }
 }
 bool operator==(const RemakeImageBuffer& other)const{return read()==other.read();}
 bool operator!=(const RemakeImageBuffer& other)const{return !(*this==other);}
};
using RemakeDepthBuffer=RemakeImageBuffer<float>;
using RemakeColorBuffer=RemakeImageBuffer<unsigned char>;
}
