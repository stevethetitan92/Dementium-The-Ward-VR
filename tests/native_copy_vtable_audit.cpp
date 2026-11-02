#define NOMINMAX
#include "../src/native_copy_forwarders.hpp"
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check(HRESULT result){if(FAILED(result))throw std::runtime_error("vtable audit API failed");}
int main(){try{
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    std::array<ComPtr<ID3D12CommandAllocator>,2> allocators;std::array<ComPtr<ID3D12GraphicsCommandList>,2> lists;
    for(unsigned i=0;i<2;++i){auto type=i?D3D12_COMMAND_LIST_TYPE_COPY:D3D12_COMMAND_LIST_TYPE_DIRECT;check(device->CreateCommandAllocator(type,IID_PPV_ARGS(&allocators[i])));check(device->CreateCommandList(0,type,allocators[i].Get(),nullptr,IID_PPV_ARGS(&lists[i])));check(lists[i]->Close());}
    std::array<const char*,60> names{};names[9]="Close";names[10]="Reset";names[16]="CopyTextureRegion";names[26]="ResourceBarrier";
#define NATIVE_COPY_NAME(slot,name) names[slot]=#name;
    NATIVE_COPY_INTERVENING_METHODS(NATIVE_COPY_NAME)
#undef NATIVE_COPY_NAME
    unsigned same{};
    auto direct=*reinterpret_cast<void***>(lists[0].Get()),copy=*reinterpret_cast<void***>(lists[1].Get());
    for(unsigned slot=9;slot<60;++slot){if(!names[slot])throw std::runtime_error("SDK manifest incomplete");same+=direct[slot]==copy[slot];std::printf("slot=%u method=%s direct=%p copy=%p shared=%d\n",slot,names[slot],direct[slot],copy[slot],direct[slot]==copy[slot]);}
    for(unsigned i=0;i<2;++i){auto table=*reinterpret_cast<void***>(lists[i].Get());std::map<void*,std::vector<std::string>> groups;for(unsigned slot=9;slot<60;++slot)groups[table[slot]].push_back(names[slot]);unsigned aliases{};for(const auto& group:groups)if(group.second.size()>1){++aliases;std::printf("alias type=%s target=%p methods=",i?"COPY":"DIRECT",group.first);for(const auto& name:group.second)std::printf("%s,",name.c_str());std::puts("");}ComPtr<ID3D12GraphicsCommandList1> newer;check(lists[i].As(&newer));std::printf("summary type=%s unique_targets=%zu alias_groups=%u newer_interface_same_identity=%d\n",i?"COPY":"DIRECT",groups.size(),aliases,newer.Get()==lists[i].Get());}
    ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    std::printf("PASS: read-only WARP debug-wrapper vtable audit shared_direct_copy=%u/51; no hooks installed or GPU commands/submissions/game/XR; hardware/game runtime coverage not established.\n",same);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
