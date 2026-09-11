// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <atomic>
#include <vector>
#include <utility>

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
// A writable pointer/reference/iterator may outlive a validation call. Once
// any such alias escapes, never reuse this allocation's validation result.
// Fresh assignment allocates new storage; const-only pipeline ownership can
// then validate once. Copies own their bytes and carry the measured result.
class RemakeDepthBuffer {
 static std::uint64_t NewIdentity() {
  static std::atomic<std::uint64_t> next{1};
  auto value=next.load(std::memory_order_relaxed);
  while(value!=UINT64_MAX) {
   if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
  }
  return 0; // Saturation disables reuse rather than reusing an identity.
 }
 std::vector<float> values;
 std::uint64_t identity=NewIdentity();
 mutable std::atomic<int> validity{-1};
 bool writableAlias=false;
 void expose(){writableAlias=true;identity=0;validity.store(-1,std::memory_order_relaxed);}
 void replace(std::vector<float> next){values.swap(next);identity=NewIdentity();writableAlias=false;validity.store(-1,std::memory_order_relaxed);}
public:
 RemakeDepthBuffer()=default;
 RemakeDepthBuffer(const std::vector<float>& source):values(source){}
 RemakeDepthBuffer(const RemakeDepthBuffer& other):values(other.values),identity(other.identity?other.identity:NewIdentity()),validity(int(other.Validity())){}
 RemakeDepthBuffer(RemakeDepthBuffer&& other)noexcept:values(std::move(other.values)),
  identity(other.identity),validity(other.validity.load(std::memory_order_relaxed)),writableAlias(other.writableAlias){other.identity=0;other.validity.store(-1,std::memory_order_relaxed);}
 RemakeDepthBuffer& operator=(const RemakeDepthBuffer& other){if(this!=&other){RemakeDepthBuffer copy(other);*this=std::move(copy);}return *this;}
 RemakeDepthBuffer& operator=(RemakeDepthBuffer&& other)noexcept {
  if(this!=&other){values=std::move(other.values);identity=other.identity;other.identity=0;writableAlias=other.writableAlias;
   validity.store(other.validity.load(std::memory_order_relaxed),std::memory_order_relaxed);
   other.validity.store(-1,std::memory_order_relaxed);}return *this;
 }
 std::size_t size()const{return values.size();}
 bool empty()const{return values.empty();}
 const float* data()const{return values.data();}
 float* data(){expose();return values.data();}
 auto begin()const{return values.begin();} auto end()const{return values.end();}
 auto begin(){expose();return values.begin();} auto end(){expose();return values.end();}
 const float& operator[](std::size_t i)const{return values[i];}
 float& operator[](std::size_t i){expose();return values[i];}
 void clear(){replace({});}
 void pop_back(){values.pop_back();identity=writableAlias?0:NewIdentity();validity.store(-1,std::memory_order_relaxed);}
 void resize(std::size_t count){
  if(count!=values.size()){values.resize(count);identity=writableAlias?0:NewIdentity();validity.store(-1,std::memory_order_relaxed);}
 }
 void assign(std::size_t count,float value){replace(std::vector<float>(count,value));}
 template<class Iterator> void assign(Iterator first,Iterator last){replace(std::vector<float>(first,last));}
 bool HasReusableValidation()const{return !writableAlias&&validity.load(std::memory_order_relaxed)>=0;}
 // Equal nonzero identities imply equal unchanged bytes, including owned copies.
 std::uint64_t ContentIdentity()const{return identity;}
 RemakeDepthValidity Validity()const {
  const int cached=validity.load(std::memory_order_relaxed);
  if(!writableAlias&&cached>=0)return RemakeDepthValidity(cached);
  const auto result=ValidateRemakeDepth(values.data(),values.size());
  if(!writableAlias)validity.store(int(result),std::memory_order_relaxed);
  return result;
 }
 bool operator==(const RemakeDepthBuffer& other)const{return values==other.values;}
 bool operator!=(const RemakeDepthBuffer& other)const{return !(*this==other);}
};
}
