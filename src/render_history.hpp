#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace renderHistory {
enum class Kind {Reset,Root,Pipeline,CBV,Draw,Indexed,Heap,Table};
enum class Stage {Native,Source,Stereo,UI,Extra};
struct Event {
    Kind kind{};Stage stage{};const void* list{};
    std::uint64_t value{};unsigned index{};bool injected{};
};
// Per recording thread, fixed storage and no COM ownership or GPU state changes.
template<size_t Capacity=128,class Record=Event> struct History {
    static_assert(Capacity>0,"nonempty history");
    std::array<Record,Capacity> events{};size_t next{},count{};
    void add(Record event){events[next]=event;next=(next+1)%Capacity;if(count<Capacity)++count;}
    template<class F> void each(F callback) const {
        auto first=(next+Capacity-count)%Capacity;
        for(size_t i=0;i<count;++i)callback(events[(first+i)%Capacity]);
    }
};
inline const char* name(Kind k){switch(k){case Kind::Reset:return "reset";case Kind::Root:return "root";case Kind::Pipeline:return "pipeline";case Kind::CBV:return "cbv";case Kind::Draw:return "draw";case Kind::Indexed:return "indexed";case Kind::Heap:return "heap";case Kind::Table:return "table";}return "unknown";}
inline const char* name(Stage s){switch(s){case Stage::Native:return "native";case Stage::Source:return "source";case Stage::Stereo:return "stereo";case Stage::UI:return "ui";case Stage::Extra:return "extra";}return "unknown";}
}
