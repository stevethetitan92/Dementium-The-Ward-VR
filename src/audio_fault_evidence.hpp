#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "audio_list_diagnostic.hpp"

// Opt-in evidence for the already-recorded native audio crash. Never handles
// an exception, alters a register/list, calls native audio, or suppresses BugSplat.
// File is opened during initialization; callback uses fixed storage, no logging
// mutex, allocation, formatting, or minidump generation in the faulting process.
namespace audioFaultEvidence {
inline HANDLE file=INVALID_HANDLE_VALUE;
inline PVOID registration{};
inline std::uintptr_t image{};
inline volatile LONG captured{};
inline bool matches(DWORD code,std::uintptr_t pc,std::uintptr_t base){
    // Both dereferences have appeared in preserved native crash dumps:
    // voice+0x18, and the resulting playback object's +0x48 field.
    return code==EXCEPTION_ACCESS_VIOLATION&&(pc==base+0xfd6a4||pc==base+0xfd6b4);
}
inline LONG CALLBACK observe(EXCEPTION_POINTERS* exception){
    if(!exception||!exception->ExceptionRecord||!exception->ContextRecord||file==INVALID_HANDLE_VALUE)return EXCEPTION_CONTINUE_SEARCH;
    const auto& e=*exception->ExceptionRecord;const auto& c=*exception->ContextRecord;
    if(!matches(e.ExceptionCode,reinterpret_cast<std::uintptr_t>(e.ExceptionAddress),image)||InterlockedCompareExchange(&captured,1,0)!=0)return EXCEPTION_CONTINUE_SEARCH;
    // Version 2 keeps the original 24 words and adds bounded read-only memory
    // evidence. Saved BugSplat dumps omit the voice and its playback pointer.
    std::uint64_t record[128]{};record[0]=0x314556464f494455ULL;record[1]=2;
    FILETIME time{};GetSystemTimeAsFileTime(&time);record[2]=(std::uint64_t(time.dwHighDateTime)<<32)|time.dwLowDateTime;
    record[3]=GetCurrentProcessId();record[4]=GetCurrentThreadId();record[5]=image;
    record[6]=e.ExceptionCode;record[7]=c.Rip;record[8]=e.NumberParameters;
    if(e.NumberParameters>0)record[9]=e.ExceptionInformation[0];
    if(e.NumberParameters>1)record[10]=e.ExceptionInformation[1];
    record[11]=c.Rbx;record[12]=c.Rcx;record[13]=c.Rdx;record[14]=c.Rsp;record[15]=c.R14;
    record[16]=image+0x3a81c8;
    auto read=[](std::uintptr_t address,std::uintptr_t& value){SIZE_T bytes=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof value,&bytes)&&bytes==sizeof value;};
    std::uintptr_t first{};record[17]=read(record[16],first);record[18]=first;
    const auto result=audioListDiagnostic::snapshot(record[16],read);
    record[19]=static_cast<unsigned>(result.status);record[20]=result.address;record[21]=result.previous;record[22]=result.nodes;
    record[23]=1; // Other threads may mutate memory: not an atomic heap snapshot.
    // Each bit marks a successful word read; unreadable words stay zero.
    auto block=[&](std::uintptr_t address,unsigned firstWord,unsigned count,unsigned maskWord){
        if(!audioListDiagnostic::userAddress(address)||count>64||address>0x00007fffffffffffULL-count*8)return;
        for(unsigned i=0;i<count;i++){
            std::uintptr_t value{};
            if(read(address+i*8,value)){record[firstWord+i]=value;record[maskWord]|=1ULL<<i;}
        }
    };
    block(c.Rcx,32,11,24); // Native 0x58-byte voice, including +0x18 playback.
    // Both fault PCs follow only 'sub rsp,28h'; return address is at RSP+28h.
    if(c.Rsp<=0x00007fffffffffd7ULL)block(c.Rsp+0x28,48,8,25);
    // At the observed 75AF5 caller R14 owns four voices at +7E0, stride58.
    // Only interpret that register when both caller and voice offset agree.
    if((record[25]&1)&&record[48]==image+0x75af5&&
       audioListDiagnostic::userAddress(c.R14)&&c.R14<=0x00007ffffffff69fULL){
        const auto firstVoice=c.R14+0x7e0;
        if(c.Rcx>=firstVoice&&c.Rcx-firstVoice<4*0x58&&(c.Rcx-firstVoice)%0x58==0){
            record[26]=c.R14;record[27]=(c.Rcx-firstVoice)/0x58;
            block(firstVoice,64,44,28);
            block(c.R14+0x230,112,2,29); // Source mask/count, not ownership proof.
        }
    }
    DWORD written{};WriteFile(file,record,sizeof record,&written,nullptr);
    return EXCEPTION_CONTINUE_SEARCH;
}
inline bool install(const wchar_t* path){
    wchar_t enabled[8]{};if(GetEnvironmentVariableW(L"DEMENTIUM_AUDIO_FAULT_EVIDENCE",enabled,8)!=1||enabled[0]!=L'1')return false;
    if(registration)return true;
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    unsigned char actual[8]{};SIZE_T copied{};const unsigned char expected[]={0x48,0x83,0xec,0x28,0x48,0x8b,0x51,0x18};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+0xfd6a0),actual,sizeof actual,&copied)||copied!=sizeof actual||memcmp(actual,expected,sizeof actual))return false;
    // Never overwrite an earlier fault record; the test launcher supplies a
    // fresh run directory or archives this file before another run.
    file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    registration=AddVectoredExceptionHandler(1,observe);
    if(!registration){CloseHandle(file);file=INVALID_HANDLE_VALUE;return false;}
    return true;
}
}
