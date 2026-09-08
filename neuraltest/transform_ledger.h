#pragma once
// Developer-only lossless envelope. Event interpretation remains independent.
#include <array>
#include <cstdint>
#include <vector>
#include <utility>
#include <mutex>
#include <zlib.h>

namespace fc067 {
class TransformLedger {
public:
    static constexpr std::size_t compressedLimit=8*1024*1024,expandedLimit=128*1024*1024;
    TransformLedger() {
        ready_=deflateInit(&stream_,3)==Z_OK;failed_=!ready_;
        if(ready_)bytes_.reserve(compressedLimit);
    }
    ~TransformLedger() {if(ready_)deflateEnd(&stream_);}
    TransformLedger(const TransformLedger&)=delete;
    TransformLedger& operator=(const TransformLedger&)=delete;
    bool append(const char* data,std::size_t size) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if(failed_ || finished_ || size>expandedLimit-total_ || size>UINT32_MAX)return fail();
        total_+=size;stream_.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(data));
        stream_.avail_in=static_cast<uInt>(size);
        do {if(!pump(Z_NO_FLUSH))return false;}while(stream_.avail_in);
        return true;
    }
    bool finish(std::vector<std::uint8_t>& output) {
        const std::lock_guard<std::mutex> lock(mutex_);
        output.clear();if(failed_ || finished_ || !total_)return fail();
        while(!finished_)if(!pump(Z_FINISH))return false;
        output=std::move(bytes_);return true;
    }
private:
    bool fail(){failed_=true;bytes_.clear();return false;}
    bool pump(int flush) {
        std::array<std::uint8_t,16384> buffer{};
        stream_.next_out=buffer.data();stream_.avail_out=static_cast<uInt>(buffer.size());
        const int result=deflate(&stream_,flush);
        if(result!=Z_OK && result!=Z_STREAM_END)return fail();
        const std::size_t size=buffer.size()-stream_.avail_out;
        if(size>compressedLimit-bytes_.size())return fail();
        bytes_.insert(bytes_.end(),buffer.begin(),buffer.begin()+size);
        finished_=result==Z_STREAM_END;return true;
    }
    z_stream stream_{};
    std::mutex mutex_;
    bool ready_=false,failed_=false,finished_=false;
    std::size_t total_=0;
    std::vector<std::uint8_t> bytes_;
};
}
