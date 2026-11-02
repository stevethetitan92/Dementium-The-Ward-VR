#pragma once
#include <cstdint>
#include <cstddef>

// Read-only snapshot of the list documented in work/game-disasm.txt at B6848,
// B6870 and B688C. Concurrent native mutation can invalidate a snapshot; this
// is evidence collection, never authorization to repair or skip a native node.
namespace audioListDiagnostic {
enum class Status { complete, unreadable, invalidAddress, cycle, budget };
struct Result { Status status; std::uintptr_t address; std::uintptr_t previous; unsigned nodes; };
inline bool userAddress(std::uintptr_t p) { return p>=0x10000 && p<=0x00007fffffffffffULL; }
template<class Read> Result snapshot(std::uintptr_t head,Read read) {
    std::uintptr_t seen[256]{}, current=head, previous=0;
    unsigned count=0;
    if(!userAddress(head))return {Status::invalidAddress,head,0,0};
    if(!read(head,current))return {Status::unreadable,head,0,0};
    while(current!=head){
        if(!userAddress(current))return {Status::invalidAddress,current,previous,count};
        for(unsigned i=0;i<count;++i)if(seen[i]==current)return {Status::cycle,current,previous,count};
        if(count==256)return {Status::budget,current,previous,count};
        seen[count++]=current;
        std::uintptr_t next=0;
        if(!read(current,next))return {Status::unreadable,current,previous,count};
        previous=current;current=next;
    }
    return {Status::complete,head,previous,count};
}
inline const char* name(Status s){switch(s){case Status::complete:return "complete";case Status::unreadable:return "unreadable";case Status::invalidAddress:return "invalid_address";case Status::cycle:return "cycle";default:return "budget";}}
}
