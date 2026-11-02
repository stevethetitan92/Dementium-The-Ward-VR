#include "../src/render_history.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
int main(){
    using namespace renderHistory;History<4> history;
    int first{},second{};
    history.each([](const Event&){assert(false);});
    history.add({Kind::Reset,Stage::Native,&first,1,0,false});
    history.add({Kind::Root,Stage::Native,&second,2,0,false});
    history.add({Kind::Pipeline,Stage::Stereo,&first,3,0,true});
    history.add({Kind::CBV,Stage::Stereo,&first,4,1,true});
    history.add({Kind::Indexed,Stage::Stereo,&first,5,9,true});
    std::vector<Event> result;history.each([&](Event e){result.push_back(e);});
    assert(result.size()==4&&result[0].list==&second&&result[0].value==2);
    assert(result[1].kind==Kind::Pipeline&&result[1].injected);
    assert(result[2].index==1&&result[3].kind==Kind::Indexed&&result[3].value==5);
    for(unsigned i=6;i<26;++i)history.add({Kind::Draw,Stage::UI,&second,i,0,true});
    unsigned expected=22;history.each([&](Event e){assert(e.value==expected++&&e.stage==Stage::UI&&e.list==&second);});
    assert(expected==26);
    struct CopyRecord {unsigned source{},destination{};bool injected{};};
    History<2,CopyRecord> copies;
    copies.add({1,2,false});copies.add({3,4,true});copies.add({5,6,false});
    unsigned pair=3;copies.each([&](const CopyRecord& copy){assert(copy.source==pair&&copy.destination==pair+1&&copy.injected==(pair==3));pair+=2;});assert(pair==7);
    std::puts("PASS: bounded render failure history preserves interleaved lists, injected calls, stage and chronological wraparound; no game/headset.");
}
