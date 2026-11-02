#include "../src/mirror_present_policy.hpp"
#include <cassert>
#include <cstdio>
int main() {
    unsigned cases=0;
    for(unsigned mask=0;mask<256;++mask) {
        bool enabled=mask&1, stereo=mask&2, supported=mask&4, windowed=mask&8;
        UINT creation=(mask&16)?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
        UINT sync=(mask&32)?1:0;
        UINT original=(mask&64)?DXGI_PRESENT_TEST:0;
        if(mask&128) original|=DXGI_PRESENT_DO_NOT_WAIT;
        UINT got=mirrorPresentPolicy::flags(enabled,stereo,supported,windowed,creation,sync,original);
        bool allowed=enabled&&stereo&&supported&&windowed&&creation&&sync==0&&!(original&DXGI_PRESENT_TEST);
        assert(got==(original|(allowed?DXGI_PRESENT_ALLOW_TEARING:0)));
        assert((got&original)==original);
        ++cases;
    }
    assert(mirrorPresentPolicy::flags(false,false,false,false,0,1,DXGI_PRESENT_ALLOW_TEARING)==DXGI_PRESENT_ALLOW_TEARING);
    std::printf("PASS %u presentation capability/flag combinations; original flags preserved\n",cases);
}
