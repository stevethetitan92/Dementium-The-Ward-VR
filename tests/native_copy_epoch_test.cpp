#include "../src/native_copy_epoch.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <thread>
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("native copy epoch check failed");}
int main(){try{
    nativeCopyEpoch::Source source;nativeCopyEpoch::Recording first;
    require(first.value()==0);first.reset(source,false);require(first.value()==0);
    first.created(source);auto original=first.value();require(original!=0);
    first.reset(source,false);require(first.value()==original);
    first.reset(source,true);require(first.value()!=original&&first.value()!=0);
    auto previous=first.value();first={};first.created(source);require(first.value()!=previous&&first.value()!=original);
    std::array<std::vector<std::uint64_t>,8> values;std::array<std::thread,8> threads;
    for(unsigned index=0;index<threads.size();++index)threads[index]=std::thread([&,index]{for(unsigned i=0;i<4096;++i)values[index].push_back(source.acquire());});
    for(auto& thread:threads)thread.join();std::vector<std::uint64_t> all;
    for(auto& group:values)all.insert(all.end(),group.begin(),group.end());std::sort(all.begin(),all.end());
    require(all.front()>first.value()&&std::adjacent_find(all.begin(),all.end())==all.end());
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)();nativeCopyEpoch::Source edge(maximum-1);require(edge.acquire()==maximum-1);for(unsigned i=0;i<32;++i)require(edge.acquire()==0);
    nativeCopyEpoch::Recording exhausted;exhausted.created(edge);require(exhausted.value()==0);exhausted.reset(edge,true);require(exhausted.value()==0);
    nativeCopyEpoch::Source disabled(0);require(disabled.acquire()==0);
    std::puts("PASS: 32768 concurrent recording tokens unique; failed Reset preserves identity, successful Reset and recreated nodes change identity; exhaustion never wraps; no D3D/game/XR.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
