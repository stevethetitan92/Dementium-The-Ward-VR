#include "../src/motion_native.hpp"
#include <cstdio>
#include <stdexcept>
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    // Isolated scratch memory, not a process attachment or executable image.
    auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,16*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    check(memory!=nullptr,"scratch allocation failed");
    struct Free {void* address;~Free(){VirtualFree(address,0,MEM_RELEASE);}} release{memory};
    nativeMotion::base=reinterpret_cast<std::uintptr_t>(memory);
    auto& index=*reinterpret_cast<int*>(memory+0x6bf284);auto& dirty=memory[0x6bf280];
    auto stack=reinterpret_cast<float*>(memory+0x6bf288);
    const float oldEntity[]={1,0,0,2,0,1,0,3,0,0,1,4};
    const float newEntity[]={1,0,0,7,0,1,0,8,0,0,1,9};
    const float parent[]={1,0,0,10,0,1,0,20,0,0,1,30};
    const float cached[]={1,0,0,12,0,1,0,23,0,0,1,34};
    unsigned char flag=0;index=3;dirty=0;std::memcpy(stack+3*12,parent,sizeof parent);
    {
        nativeMotion::DrawCache guard(&flag,oldEntity,newEntity);
        check(index==3&&flag==0&&dirty==0,"guard stole an uncached native stack push");
        // Native7C990 pushes; nativeF3D10 remains responsible for the pop.
        ++index;std::memcpy(stack+index*12,cached,sizeof cached);flag=1;
    }
    check(index==4&&flag==1,"guard undid the native caller's pop flag");
    --index;flag=0;check(!std::memcmp(stack+index*12,parent,sizeof parent),"uncached draw changed parent camera");
    index=4;flag=1;dirty=0;std::memcpy(stack+4*12,cached,sizeof cached);
    {
        nativeMotion::DrawCache guard(&flag,oldEntity,newEntity);
        check(stack[4*12+3]==17&&stack[4*12+7]==28&&stack[4*12+11]==39&&dirty==1,"cached entity was not replaced under its parent");
    }
    check(index==4&&flag==1&&!std::memcmp(stack+4*12,cached,sizeof cached),"cached entry was not restored");
    {
        nativeMotion::DrawCache guard(&flag,oldEntity,newEntity);
        // Pistol muzzle8B5EB pops and clears the shared flag itself.
        --index;flag=0;stack[4*12+3]=123;
    }
    check(index==3&&flag==0&&stack[4*12+3]==123,"guard resurrected or overwrote a popped muzzle entry");
    index=4;flag=1;std::memcpy(stack+4*12,cached,sizeof cached);
    {
        nativeMotion::DrawCache guard(&flag,oldEntity,newEntity);
        index=5;std::memcpy(stack+5*12,parent,sizeof parent);
    }
    check(index==5&&flag==1&&!std::memcmp(stack+5*12,parent,sizeof parent),"guard overwrote a different active stack entry");
    for(int invalid:{-1,32}){index=invalid;dirty=0;nativeMotion::DrawCache guard(&flag,oldEntity,newEntity);check(index==invalid&&dirty==0,"invalid index touched native stack");}
    nativeMotion::base=0;std::puts("PASS: production DrawCache preserves native push/pop ownership, parent camera, cached restore and muzzle pop; isolated scratch memory, no game/headset.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
