#include "compact_consumer_tape.h"
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static fc067::ConsumerRecord record(std::uint64_t sample=1) {
    fc067::ConsumerRecord r;
    r.identity={sample,3,1781,0x100000002ull,10,0x100002020ull,0x200003040ull,0x200003040ull};
    r.location={1420,277,32,0xe0000020,0x8c001000,0x8c001004,0x8c001008,0};
    r.before={0xe0000000,0x3f800000,0x40000000,0x3f000000,0,0,0,0};r.after=r.before;
    return r;
}
int main(int argc,char** argv) {
    unsigned checks=0;
    auto check=[&](bool ok){++checks;if(!ok)std::fprintf(stderr,"FAILED check %u\n",checks);return ok;};
    std::vector<std::uint8_t> output;
    fc067::ConsumerTape invalid(0),large(fc067::ConsumerTape::capacity+1),partial(2);
    if(!check(invalid.failed() && large.failed()))return 1;
    if(!check(partial.append(record()) && !partial.finish(output) && output.empty()
              && !partial.append(record(2))))return 1;
    for(unsigned mutation=0;mutation<8;++mutation) {
        fc067::ConsumerTape tape(1);auto r=record();
        switch(mutation) {
        case 0:r.identity[0]=2;break;case 1:r.identity[7]^=4;break;
        case 2:r.after[1]^=1;break;case 3:r.location[7]=1;break;
        case 4:r.location[4]=0xfffffffcu;break;case 5:r.location[3]=0;break;
        case 6:r.identity[4]=0;break;case 7:r.location[2]=1;break;
        }
        if(!check(!tape.append(r) && tape.failed() && !tape.finish(output) && output.empty()))return 1;
    }
    fc067::ConsumerTape full(fc067::ConsumerTape::capacity);
    for(unsigned i=1;i<=fc067::ConsumerTape::capacity;++i)if(!full.append(record(i)))return 1;
    if(!check(full.bytes()==1966080 && full.finish(output) && output.size()==1966096))return 1;
    if(!check(!full.append(record(fc067::ConsumerTape::capacity+1))))return 1;
    fc067::ConsumerTape single(1);
    if(!check(single.append(record()) && single.finish(output) && output.size()==176))return 1;
    std::fprintf(stderr,"compact-consumer checks=%u passed transport_only=true\n",checks);
    if(argc==2 && std::strcmp(argv[1],"--emit")==0) {
#ifdef _WIN32
        if(_setmode(_fileno(stdout),_O_BINARY)==-1)return 2;
#endif
        return std::fwrite(output.data(),1,output.size(),stdout)==output.size()?0:2;
    }
    return argc==1?0:2;
}
