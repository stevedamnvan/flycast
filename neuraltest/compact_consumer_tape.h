#pragma once
// Developer transport only. Callers must establish actual observation provenance.
#include <array>
#include <cstdint>
#include <vector>

namespace fc067 {
struct ConsumerRecord {
    std::array<std::uint64_t,8> identity{};
    std::array<std::uint32_t,8> location{};
    std::array<std::uint32_t,8> before{},after{};
};

class ConsumerTape {
public:
    static constexpr std::uint32_t capacity=12288;
    explicit ConsumerTape(std::uint32_t expected): expected_(expected) {
        failed_=expected==0 || expected>capacity;
        if(!failed_) payload_.reserve(static_cast<std::size_t>(expected)*160);
    }
    bool append(const ConsumerRecord& r) {
        const auto& i=r.identity;const auto& p=r.location;
        if(failed_ || finished_ || count_>=expected_ || i[0]!=count_+1 || !i[1] || !i[4]
            || !i[5] || !i[6] || i[6]!=i[7] || p[7] || r.before!=r.after
            || r.after[0]>>29!=7 || p[2]%32 || p[2]>=8*1024*1024
            || p[3]>>26!=0x38 || p[3]%32 || p[4]%4 || p[4]>0xfffffff7u
            || p[5]!=p[4]+4 || p[6]!=p[4]+8) return fail();
        for(auto v:i) put(payload_,v,8);
        for(auto v:p) put(payload_,v,4);
        for(auto v:r.before) put(payload_,v,4);
        for(auto v:r.after) put(payload_,v,4);
        ++count_;return true;
    }
    bool finish(std::vector<std::uint8_t>& out) {
        out.clear();
        if(failed_ || finished_ || count_!=expected_) return fail();
        std::uint32_t crc=0xffffffffu;
        for(auto byte:payload_) {
            crc^=byte;
            for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
        }
        const char magic[]="FC067C01";
        out.assign(magic,magic+8);put(out,count_,4);put(out,crc^0xffffffffu,4);
        out.insert(out.end(),payload_.begin(),payload_.end());finished_=true;return true;
    }
    bool failed() const {return failed_;}
    std::size_t bytes() const {return payload_.size();}
private:
    static void put(std::vector<std::uint8_t>& out,std::uint64_t value,unsigned bytes) {
        for(unsigned b=0;b<bytes;++b) out.push_back(static_cast<std::uint8_t>(value>>(8*b)));
    }
    bool fail() {failed_=true;payload_.clear();return false;}
    std::uint32_t expected_=0,count_=0;
    bool failed_=false,finished_=false;
    std::vector<std::uint8_t> payload_;
};
}
