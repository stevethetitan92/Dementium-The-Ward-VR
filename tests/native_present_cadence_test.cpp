#include "../src/native_present_cadence.hpp"
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
static unsigned calls[3]{},cleanups;
struct Callback {void** table;unsigned kind;};
static void call(Callback* c){++calls[c->kind];}
static void cleanup(Callback*,bool){++cleanups;}
struct Wrapper {unsigned char pad[56]{};Callback* callback;};
int main(int argc,char** argv){
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});assert(bytes.size()==322);
    void* memory=VirtualAlloc(nullptr,bytes.size(),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(memory);
    memcpy(memory,bytes.data(),bytes.size());DWORD old;assert(VirtualProtect(memory,bytes.size(),PAGE_EXECUTE_READ,&old));
    FlushInstructionCache(GetCurrentProcess(),memory,bytes.size());
    auto native=reinterpret_cast<nativePresentCadence::Function>(memory);
    void* table[5]={nullptr,nullptr,reinterpret_cast<void*>(call),nullptr,reinterpret_cast<void*>(cleanup)};
    Callback callbacks[3]={{table,0},{table,1},{table,2}};
    auto invoke=[&](nativePresentCadence::State& state){Wrapper w[3]{};for(int i=0;i<3;i++)w[i].callback=&callbacks[i];native(&state,&w[0],&w[1],&w[2]);};
    nativePresentCadence::State baseline{60,100,0,0,0};
    for(int i=0;i<300;i++)invoke(baseline);
    assert(calls[1]==300&&calls[2]==200&&cleanups==900);
    printf("native baseline: 300 render cycles, %u presents (%u repeat)\n",calls[1]+calls[2],calls[2]);
    unsigned cases=0;
    for(auto numerator:{30ull,60ull,75ull,90ull,100ull,120ull})for(auto denominator:{30ull,60ull,75ull,90ull,100ull,120ull})for(auto remainder:{0ull,7ull,29ull}){
        calls[0]=calls[1]=calls[2]=cleanups=0;
        nativePresentCadence::State state{numerator,denominator,remainder,0,0};
        for(int i=0;i<100;i++)nativePresentCadence::singlePresentation(state,[&]{invoke(state);});
        assert(calls[0]==0&&calls[1]==100&&calls[2]==0&&cleanups==300);
        assert(state.numerator==numerator&&state.denominator==denominator&&state.remainder==remainder);
        assert(state.rendered==100&&state.presented==100);++cases;
    }
    printf("PASS %u original-machine-code cases; one present per render, native cleanup preserved, ratio/remainder restored\n",cases);
    VirtualFree(memory,0,MEM_RELEASE);
}
