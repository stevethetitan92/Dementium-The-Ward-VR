#pragma once
#include <cstdint>
#include <limits>
#include <mutex>
namespace nativeCopyControl {
// Unused/default-off trial admission budget. It never touches pending sessions:
// cancellation/expiry stops NEW reservations. Already reserved admission
// attempts may finish; admitted pairs must still finish/replay safely.
class Control {
    std::mutex mutex;std::uint64_t started{},until{};unsigned remaining{};
public:
    static constexpr std::uint64_t maximumDuration=120000;
    static constexpr unsigned maximumAdmissions=128;
    bool arm(std::uint64_t now,std::uint64_t duration,unsigned budget){
        std::lock_guard<std::mutex> guard(mutex);started=until=0;remaining=0;
        if(!duration||!budget)return false;
        if(duration>maximumDuration)duration=maximumDuration;
        if(duration>(std::numeric_limits<std::uint64_t>::max)()-now)return false;
        started=now;until=now+duration;remaining=budget>maximumAdmissions?maximumAdmissions:budget;return true;
    }
    void cancel(){std::lock_guard<std::mutex> guard(mutex);started=until=0;remaining=0;}
    // Reserve immediately before attempting new admission. Failed admission
    // may consume a slot; it must never refund across cancellation/rearming.
    std::uint64_t reserve(std::uint64_t now){
        std::lock_guard<std::mutex> guard(mutex);
        if(!until||now<started||now>=until||!remaining)return 0;
        --remaining;return until;
    }
};
}
