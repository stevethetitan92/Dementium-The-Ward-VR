#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
namespace vrFeedback {
inline std::atomic<std::uint32_t> pending[2]{};
inline void queue(unsigned hand,float amplitude,unsigned milliseconds){
    if(hand>1||!std::isfinite(amplitude)||amplitude<=0||!milliseconds)return;
    if(amplitude>1)amplitude=1;if(milliseconds>60)milliseconds=60;
    std::uint32_t packed=(static_cast<unsigned>(amplitude*1000)<<16)|milliseconds;
    auto old=pending[hand].load();while(old<packed&&!pending[hand].compare_exchange_weak(old,packed)){}
}
inline std::uint32_t take(unsigned hand){return hand<2?pending[hand].exchange(0):0;}
}
