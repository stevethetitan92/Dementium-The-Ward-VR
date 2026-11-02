#include "../src/fence_completion.hpp"
#include <cassert>
#include <cstdio>
#include <string>
int main(){
    unsigned reads{},arms{},waits{},removals{};
    std::uint64_t value=4;
    auto read=[&]{++reads;return value;};
    auto arm=[&](std::uint64_t target){assert(target==4);++arms;};
    auto wake=[&]{++waits;value=4;};
    auto removed=[&]{++removals;};
    fenceCompletion::wait(4,read,arm,wake,removed);
    assert(reads==1&&arms==0&&waits==0);
    value=3;fenceCompletion::wait(4,read,arm,wake,removed);
    assert(reads==3&&arms==1&&waits==1);
    auto fails=[&](auto action,const char* message){
        try{action();assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what()).find(message)!=std::string::npos);}
    };
    value=UINT64_MAX;
    fails([&]{fenceCompletion::wait(4,read,arm,wake,removed);},"device removed");
    assert(removals==1&&arms==1&&waits==1);
    value=3;
    fails([&]{fenceCompletion::wait(4,read,arm,[&]{++waits;value=UINT64_MAX;},removed);},"device removed");
    assert(removals==2);
    value=3;
    fails([&]{fenceCompletion::wait(4,read,arm,[]{},removed);},"before completion");
    fails([&]{fenceCompletion::wait(4,read,arm,[]{throw std::runtime_error("timeout");},removed);},"timeout");
    fails([&]{fenceCompletion::wait(4,read,[](auto){throw std::runtime_error("arm failed");},wake,removed);},"arm failed");
    const auto previousReads=reads;
    fails([&]{fenceCompletion::wait(UINT64_MAX,read,arm,wake,removed);},"invalid fence target");
    assert(reads==previousReads);
    std::puts("PASS: fence completion rejects device removal before/after wait, premature wake, invalid targets and failed waits; completed work avoids waiting.");
}
