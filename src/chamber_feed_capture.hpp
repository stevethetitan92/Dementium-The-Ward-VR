#pragma once
#include <atomic>
#include <cstdint>
namespace chamberFeedCapture {
// One bounded desktop diagnostic capture during the actual spring return.
inline std::atomic<std::uint64_t> until{};
inline void(*capture)(){};
inline void sample(float fraction,std::uint64_t now){
 if(fraction>.25f&&fraction<.85f&&now<until.load()&&until.exchange(0)&&capture)capture();
}
}
