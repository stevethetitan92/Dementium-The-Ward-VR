#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
// Diagnostic admission only. Never hold this mutex across capture, logging,
// native calls or GPU work. A successful reservation consumes one sample.
class BoundedSampleGate {
    std::mutex mutex;
    std::atomic<unsigned> samples{};
    std::uint64_t previous{};
    bool eligible(std::uint64_t now)const{return samples.load()<12&&now>=previous&&now-previous>1000;}
public:
    bool mayCapture(std::uint64_t now){
        if(samples.load()>=12)return false;
        std::lock_guard<std::mutex> guard(mutex);return eligible(now);
    }
    unsigned reserve(std::uint64_t now){
        if(samples.load()>=12)return 0;
        std::lock_guard<std::mutex> guard(mutex);if(!eligible(now))return 0;
        previous=now;return ++samples;
    }
};
