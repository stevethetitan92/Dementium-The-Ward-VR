#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
namespace meleeTrace {
// Opt-in evidence only: never writes actor state or participates in hit decisions.
enum class Result:unsigned { Miss, InvalidContact, GateRejected, DamageRejected, DamageAccepted };
struct Event { std::uint64_t time{};std::uintptr_t actor{};unsigned result{},reverse{};float ray[6]{},length{},radius{},distance{},point[3]{};int surface{},triangle{},health{-1}; };
template<unsigned Capacity> class Buffer {
 static_assert(Capacity>0,"nonempty trace");
 std::array<Event,Capacity> events{};unsigned count{};std::mutex mutex;
public:
 std::atomic<bool> enabled{false};std::atomic<unsigned> dropped{0};
 void push(const Event& event){if(!enabled.load(std::memory_order_relaxed))return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock||count==Capacity){++dropped;return;}events[count++]=event;}
 unsigned drain(std::array<Event,Capacity>& out){std::lock_guard<std::mutex> lock(mutex);unsigned n=count;for(unsigned i=0;i<n;++i)out[i]=events[i];count=0;return n;}
};
inline Buffer<256> buffer;
}
