#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
namespace nativeCopyEpoch {
// Unused recording-identity prerequisite. Tokens come from one shared source,
// so resetting registry nodes or reusing native addresses cannot reuse an epoch.
// Exhaustion disables admission permanently rather than wrapping to old tokens.
class Source {
    std::atomic<std::uint64_t> next;
public:
    explicit Source(std::uint64_t first=1):next(first){}
    std::uint64_t acquire(){
        auto value=next.load(std::memory_order_relaxed);
        for(;;){if(!value||value==(std::numeric_limits<std::uint64_t>::max)())return 0;
            if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
        }
    }
};
class Recording {
    std::uint64_t token{};
public:
    std::uint64_t value()const{return token;}
    void created(Source& source){token=source.acquire();}
    void reset(Source& source,bool succeeded){if(succeeded)token=source.acquire();}
};
}
