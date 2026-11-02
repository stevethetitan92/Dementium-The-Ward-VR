#include "../src/bounded_sample_gate.hpp"
#include <array>
#include <thread>
#include <stdexcept>
#include <cstdio>
#include <limits>
static void require(bool value){if(!value)throw std::runtime_error("sample gate check failed");}
int main(){try{
    BoundedSampleGate gate;require(!gate.mayCapture(1000)&&gate.mayCapture(1001)&&gate.mayCapture(1001));
    std::array<unsigned,64> results{};std::array<std::thread,64> threads;std::atomic<bool> start{};
    for(unsigned i=0;i<64;++i)threads[i]=std::thread([&,i]{while(!start.load())std::this_thread::yield();results[i]=gate.reserve(1001);});
    start=true;for(auto& thread:threads)thread.join();unsigned accepted{};for(auto value:results){require(value==0||value==1);accepted+=value!=0;}require(accepted==1);
    require(!gate.mayCapture(900)&&!gate.reserve(900)&&!gate.reserve(2001));
    for(unsigned expected=2;expected<=12;++expected)require(gate.reserve(std::uint64_t(expected)*1001)==expected);
    require(!gate.mayCapture(UINT64_MAX)&&!gate.reserve(UINT64_MAX));
    BoundedSampleGate boundary;require(boundary.reserve(UINT64_MAX-1500)==1&&boundary.reserve(UINT64_MAX-400)==2&&!boundary.reserve(1));
    std::puts("PASS:64 simultaneous diagnostic reservations admit one, repeated peeks do not consume,12-sample cap and strict interval/backward/overflow bounds; CPU threads only, no game/GPU/XR.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
