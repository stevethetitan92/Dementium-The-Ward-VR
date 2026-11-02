#include <windows.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
using Fn=float(*)(const void*,const float*,float);
extern "C" Fn sphereTestTarget=nullptr;
extern "C" float nativeSphereEntry(const void*,const float*,float);
extern "C" float clobberSphere(const void*,const float*,float);
struct Args {const float* planes;const float* center;float radius;};
struct Result {std::uintptr_t rdx,r8,r9,r10,r11;float value;};
extern "C" void probeSphere(Fn,const Args*,Result*);
static bool preserved(const Result& r,const Args& a){return r.rdx==reinterpret_cast<std::uintptr_t>(a.center)&&r.r8==0x12345678&&r.r9==0x23456789&&r.r10==0x3456789a&&r.r11==0x456789ab;}
int main(int argc,char**argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(f)),{});assert(b.size()==0x110);
 void* code=VirtualAlloc(nullptr,b.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(code);memcpy(code,b.data(),b.size());DWORD old;assert(VirtualProtect(code,b.size(),PAGE_EXECUTE_READ,&old));FlushInstructionCache(GetCurrentProcess(),code,b.size());
 float planes[24]{},center[3]{};Args a{planes,center,.5f};Result direct{},safe{};
 sphereTestTarget=clobberSphere;probeSphere(clobberSphere,&a,&direct);assert(!preserved(direct,a)&&direct.r10==0);probeSphere(nativeSphereEntry,&a,&safe);assert(preserved(safe,a)&&safe.value==.5f);
 sphereTestTarget=reinterpret_cast<Fn>(code);
 for(int i=0;i<10000;++i){for(int p=0;p<6;++p){planes[p*4]=1;planes[p*4+3]=static_cast<float>((i+p)%31-15);}center[0]=static_cast<float>(i%23-11);a.radius=static_cast<float>(i%7)*.25f;probeSphere(sphereTestTarget,&a,&direct);probeSphere(nativeSphereEntry,&a,&safe);assert(preserved(direct,a)&&preserved(safe,a));assert(memcmp(&direct.value,&safe.value,4)==0);}
 DWORD64 base{};assert(RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(nativeSphereEntry)+16,&base,nullptr));
 VirtualFree(code,0,MEM_RELEASE);puts("PASS: unwrapped callback reproduces R10 loss; wrapper preserves five native registers; 10000 actual native-code visibility results identical; unwind metadata present.");
}
