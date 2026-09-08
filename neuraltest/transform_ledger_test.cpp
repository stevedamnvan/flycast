#include "transform_ledger.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc,char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--concurrent")==0){
        fc067::TransformLedger ledger;std::atomic<bool> ok{true},go{false};std::vector<std::thread> writers;
        for(unsigned id=0;id<8;id++)writers.emplace_back([&,id]{while(!go.load())std::this_thread::yield();
            for(unsigned event=0;event<2000;event++){char line[64];int n=std::snprintf(line,sizeof(line),"writer=%u event=%u\n",id,event);if(!ledger.append(line,n)){ok=false;break;}}});
        go=true;for(auto& thread:writers)thread.join();std::vector<std::uint8_t> bytes;
        if(!ok || !ledger.finish(bytes)){std::fprintf(stderr,"concurrent append failed\n");return 1;}
#ifdef _WIN32
        if(_setmode(_fileno(stdout),_O_BINARY)==-1)return 2;
#endif
        return std::fwrite(bytes.data(),1,bytes.size(),stdout)==bytes.size()?0:2;
    }
    std::vector<std::uint8_t> output;
    for(int level : {-1,0,10}) {
        fc067::TransformLedger invalid(level);
        if(invalid.append("x",1) || invalid.finish(output) || !output.empty())return 1;
    }
    const char* exact="FC067_CT_BLOCK generation=10 slot=920 kind=0\n";
    std::vector<char> expected;
    for(unsigned i=0;i<1000;i++)expected.insert(expected.end(),exact,exact+std::strlen(exact));
    for(int level : {1,3,6,9}) {
        fc067::TransformLedger candidate(level);
        if(!candidate.append(expected.data(),expected.size()) || !candidate.finish(output))return 1;
        std::vector<char> restored(expected.size());uLongf size=restored.size();
        if(uncompress(reinterpret_cast<Bytef*>(restored.data()),&size,output.data(),output.size())!=Z_OK
            || size!=expected.size() || restored!=expected)return 1;
    }
    fc067::TransformLedger empty;
    if(empty.finish(output) || !output.empty())return 1;
    fc067::TransformLedger overflow;
    std::vector<char> zeros(1024*1024,'x');
    for(unsigned i=0;i<128;++i)if(!overflow.append(zeros.data(),zeros.size()))return 1;
    if(overflow.append("x",1) || overflow.finish(output) || !output.empty())return 1;
    fc067::TransformLedger noisy;
    std::array<char,65536> noise{};std::uint32_t state=0x87654321;
    bool rejected=false;
    for(unsigned block=0;block<160 && !rejected;++block) {
        for(auto& c:noise){state^=state<<13;state^=state>>17;state^=state<<5;c=static_cast<char>(state);}
        rejected=!noisy.append(noise.data(),noise.size());
    }
    if(!rejected || noisy.finish(output) || !output.empty())return 1;
    fc067::TransformLedger golden;
    const char* line="FC067_CT_BLOCK generation=10 slot=920 kind=0\n";
    for(unsigned i=0;i<1000;++i)if(!golden.append(line,std::strlen(line)))return 1;
    if(!golden.finish(output) || output.empty())return 1;
    if(golden.append(line,std::strlen(line)))return 1;
    std::fprintf(stderr,"ledger empty/raw-cap/compressed-cap/golden/terminal checks passed\n");
    if(argc==2 && std::strcmp(argv[1],"--emit")==0) {
#ifdef _WIN32
        if(_setmode(_fileno(stdout),_O_BINARY)==-1)return 2;
#endif
        return std::fwrite(output.data(),1,output.size(),stdout)==output.size()?0:2;
    }
    return argc==1?0:2;
}
