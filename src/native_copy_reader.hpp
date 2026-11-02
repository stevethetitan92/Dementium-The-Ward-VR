#pragma once
#include <windows.h>
#include <cstdint>
#include <limits>
namespace nativeCopyReader {
// Unused in-process reader for bounded code/frame evidence. Query checks avoid
// touching known guard pages; RPM fails closed on access changes/short reads.
// This is not an atomic snapshot or resource/COM lifetime validation.
struct Reader {
    bool operator()(std::uintptr_t source,void* output,size_t bytes)const{
        if(!bytes)return true;
        const auto maximum=(std::numeric_limits<std::uintptr_t>::max)();
        if(!source||!output||bytes>4096||bytes>maximum-source)return false;
        const auto end=source+bytes;auto cursor=source;
        while(cursor<end){MEMORY_BASIC_INFORMATION region{};
            if(!VirtualQuery(reinterpret_cast<const void*>(cursor),&region,sizeof region)||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
            const auto protection=region.Protect&0xff;
            if(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
            const auto base=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
            if(base>cursor||region.RegionSize>maximum-base||base+region.RegionSize<=cursor)return false;
            cursor=base+region.RegionSize;if(cursor>end)cursor=end;
        }
        SIZE_T copied{};return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(source),output,bytes,&copied)&&copied==bytes;
    }
};
}
