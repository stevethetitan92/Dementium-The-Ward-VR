#pragma once
#include <windows.h>
#include <atomic>
#include <array>
#include <cstdint>
namespace drawCost {
enum Stage { Trace, Source, Stereo, UI, Driver, Count };
#ifdef DEMENTIUM_PROFILE_DRAWS
inline std::atomic<std::uint64_t> ticks[Count]{},calls[Count]{};
inline std::uint64_t now(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
inline double frequency(){static const double f=[](){LARGE_INTEGER q{};QueryPerformanceFrequency(&q);return double(q.QuadPart);}();return f;}
struct Sample {bool active;Sample(){static thread_local std::uint32_t n=0x6d2b79f5u;n^=n<<13;n^=n>>17;n^=n<<5;active=(n&15u)==0;}};
struct Scope {bool active;Stage stage;std::uint64_t start;Scope(const Sample& s,Stage which):active(s.active),stage(which),start(active?now():0){}~Scope(){if(active){ticks[stage].fetch_add(now()-start,std::memory_order_relaxed);calls[stage].fetch_add(1,std::memory_order_relaxed);}}};
// Present-thread only: report frame intervals as well as average FPS.
// Loading/focus stalls remain visible in max/over-budget counts, not discarded.
inline std::array<unsigned,201> frameBuckets{};
inline unsigned frameCount{},overBudget{},overRuntimeBudget{};
inline double frameMax{},frameSum{};
inline std::uint64_t previousPresent{};
inline void recordInterval(double ms,double runtimeBudget){frameSum+=ms;frameMax=(ms>frameMax?ms:frameMax);++frameCount;if(ms>1000./60.)++overBudget;if(runtimeBudget>0&&ms>runtimeBudget)++overRuntimeBudget;unsigned bucket=ms>=100.?200u:static_cast<unsigned>(ms*2.);++frameBuckets[bucket];}
inline void present(double runtimeBudget=0){auto t=now();if(previousPresent)recordInterval(double(t-previousPresent)*1000./frequency(),runtimeBudget);previousPresent=t;}
struct FrameWindow {double mean{},p95Upper{},p99Upper{},maximum{};unsigned count{},over60HzBudget{},overRuntimeBudget{};};
inline FrameWindow takeFrames(){FrameWindow w;w.count=frameCount;w.over60HzBudget=overBudget;w.overRuntimeBudget=overRuntimeBudget;w.maximum=frameMax;w.mean=frameCount?frameSum/frameCount:0;unsigned cumulative=0;for(unsigned i=0;frameCount&&i<frameBuckets.size();++i){cumulative+=frameBuckets[i];if(!w.p95Upper&&cumulative*100>=frameCount*95)w.p95Upper=i==200?frameMax:(i+1)*.5;if(!w.p99Upper&&cumulative*100>=frameCount*99)w.p99Upper=i==200?frameMax:(i+1)*.5;}frameBuckets.fill(0);frameCount=overBudget=overRuntimeBudget=0;frameMax=frameSum=0;return w;}
struct Window {std::array<double,Count> estimatedMsPerFrame{};std::array<std::uint64_t,Count> samples{};};
inline Window take(unsigned frames){Window w;for(unsigned i=0;i<Count;i++){w.estimatedMsPerFrame[i]=double(ticks[i].exchange(0))*16000./frequency()/frames;w.samples[i]=calls[i].exchange(0);}return w;}
#else
struct Sample {};struct Scope {Scope(const Sample&,Stage){}};
#endif
}
