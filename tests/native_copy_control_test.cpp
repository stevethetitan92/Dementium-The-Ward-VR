#include "../src/native_copy_control.hpp"
#include <atomic>
#include <array>
#include <thread>
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("copy admission control check failed");}
int main(){try{
    nativeCopyControl::Control trial;require(trial.reserve(0)==0&&trial.reserve(100)==0);
    require(!trial.arm(10,0,1)&&!trial.reserve(10));require(!trial.arm(10,1,0)&&!trial.reserve(10));
    require(trial.arm(100,10,2));require(!trial.reserve(99));require(trial.reserve(100)==110&&trial.reserve(109)==110&&!trial.reserve(109)&&!trial.reserve(110));
    require(trial.arm(200,100,10));trial.cancel();require(!trial.reserve(201));
    require(trial.arm(300,1000000,1000000));std::atomic<unsigned> accepted{},wrong{};std::array<std::thread,8> workers;
    for(auto& worker:workers)worker=std::thread([&]{for(unsigned i=0;i<4096;++i){auto deadline=trial.reserve(301);if(deadline){++accepted;if(deadline!=300+trial.maximumDuration)++wrong;}}});
    for(auto& worker:workers)worker.join();require(accepted==trial.maximumAdmissions&&!wrong&&!trial.reserve(301));
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)();require(!trial.arm(maximum-3,4,1)&&!trial.reserve(maximum-2));require(trial.arm(maximum-3,3,1));require(trial.reserve(maximum-2)==maximum&&!trial.reserve(maximum));
    require(trial.arm(0,10,1));require(trial.reserve(0)==10);trial.cancel();require(!trial.reserve(0));
    std::puts("PASS: default-off/cancel/expiry/time reversal/zero/overflow and duration-budget caps; 32768 concurrent requests admit exactly128; no game/D3D/XR.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
