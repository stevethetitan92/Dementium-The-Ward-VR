#include "../src/descriptor_bindings.hpp"
#include <cassert>
#include <cstdio>
int main(){
    descriptorBindings::State state;int first{},second{},sampler{},otherSampler{};
    auto empty=[&]{for(auto table:state.textures)assert(!table.ptr);};
    empty();state.setHeaps({&first,&sampler});empty();
    for(unsigned i=0;i<state.textures.size();++i)state.textures[i].ptr=100+i;
    state.setHeaps({&first,&sampler});
    for(unsigned i=0;i<state.textures.size();++i)assert(state.textures[i].ptr==100+i);
    state.setHeaps({&second,&sampler});empty();
    state.textures[3].ptr=900;state.setHeaps({&second,&otherSampler});empty();
    state.textures[3].ptr=901;state.setHeaps({&second,nullptr});empty();
    state.textures[0].ptr=902;state.setHeaps({});empty();
    assert(!state.descriptorHeaps[0]&&!state.descriptorHeaps[1]);
    descriptorBindings::State independent;independent.setHeaps({&first,&sampler});independent.textures[2].ptr=400;
    state.setHeaps({&second,nullptr});assert(independent.textures[2].ptr==400);
    independent={};for(auto table:independent.textures)assert(!table.ptr);assert(!independent.descriptorHeaps[0]);
    std::puts("PASS: descriptor table tracking invalidates changed/unbound heaps, preserves redundant binds, isolates lists and resets cleanly.");
}
