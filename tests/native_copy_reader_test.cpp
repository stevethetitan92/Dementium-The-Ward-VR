#define NOMINMAX
#include "../src/native_copy_reader.hpp"
#include "../src/native_copy_context.hpp"
#include <array>
#include <cstring>
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("native copy reader check failed");}
int main(){void* allocation{};try{
    SYSTEM_INFO system{};GetSystemInfo(&system);const auto page=system.dwPageSize;
    allocation=VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);require(allocation);auto memory=static_cast<unsigned char*>(allocation);std::memset(memory,0x6d,page*2);
    nativeCopyReader::Reader read;std::array<unsigned char,64> output{};auto address=reinterpret_cast<std::uintptr_t>(memory);
    require(read(address,output.data(),output.size()));for(auto value:output)require(value==0x6d);
    DWORD previous{};require(VirtualProtect(memory+page,page,PAGE_READONLY,&previous));require(read(address+page-32,output.data(),64));
    require(VirtualProtect(memory+page,page,PAGE_READWRITE|PAGE_GUARD,&previous));require(!read(address+page-32,output.data(),64));MEMORY_BASIC_INFORMATION info{};require(VirtualQuery(memory+page,&info,sizeof info)&&bool(info.Protect&PAGE_GUARD));
    require(VirtualProtect(memory+page,page,PAGE_NOACCESS,&previous));require(!read(address+page,output.data(),1));
    require(VirtualFree(memory+page,page,MEM_DECOMMIT));require(!read(address+page,output.data(),1));require(read(address,output.data(),64));
    require(!read(0,output.data(),1)&&!read(address,nullptr,1)&&!read(address,output.data(),4097));
    require(!read((std::numeric_limits<std::uintptr_t>::max)()-2,output.data(),4));require(read(0,nullptr,0));
    require(VirtualFree(allocation,0,MEM_RELEASE));allocation=nullptr;require(!read(address,output.data(),1));
    allocation=VirtualAlloc(nullptr,0x6bc000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);require(allocation);memory=static_cast<unsigned char*>(allocation);auto image=reinterpret_cast<std::uintptr_t>(allocation);
    std::memcpy(memory+nativeCopyCode::readbackRva,nativeCopyCode::readback.data(),nativeCopyCode::readback.size());std::memcpy(memory+nativeCopyCode::uploadRva,nativeCopyCode::upload.data(),nativeCopyCode::upload.size());auto list=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(1));std::memcpy(memory+0x6bb740,&list,sizeof list);
    for(bool upload:{true,false}){D3D12_TEXTURE_COPY_LOCATION texture{},linear{};texture.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(2));texture.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;linear.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(3));linear.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;auto source=upload?linear:texture,destination=upload?texture:linear;std::array<unsigned char,512> frame{};std::memcpy(frame.data()+(upload?0x78:0xd0),&source,sizeof source);std::memcpy(frame.data()+(upload?0x48:0x20),&destination,sizeof destination);
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={texture.pResource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE};auto caller=image+(upload?0x1eabf1:0x1ea8eb);auto context=nativeCopyContext::capture(image,caller,list,1,&barrier,reinterpret_cast<std::uintptr_t>(frame.data()),read);require(context.valid&&context.texture==texture.pResource&&context.staging==linear.pResource);
        memory[nativeCopyCode::readbackRva]^=1;require(!nativeCopyContext::capture(image,caller,list,1,&barrier,reinterpret_cast<std::uintptr_t>(frame.data()),read).valid);memory[nativeCopyCode::readbackRva]^=1;
    }
    LARGE_INTEGER frequency{},start{},finish{};require(QueryPerformanceFrequency(&frequency)&&QueryPerformanceCounter(&start));
    for(unsigned attempt=0;attempt<128;++attempt)require(nativeCopyCode::verify(image,read));
    require(QueryPerformanceCounter(&finish));const double elapsed=1000.0*double(finish.QuadPart-start.QuadPart)/double(frequency.QuadPart);
    std::printf("INFO:128 bounded current-process full-code verifications %.3f ms total / %.3f us mean; isolated writable fixture pages, not game FPS or a performance gate.\n",elapsed,elapsed*1000/128);
    require(VirtualFree(allocation,0,MEM_RELEASE));allocation=nullptr;
    std::puts("PASS: bounded current-process reader handles readable/protection-crossing/guard/noaccess/decommitted/freed/null/overflow/oversize ranges; guard remains armed; no game attachment/D3D/XR.");return 0;
}catch(const std::exception& error){if(allocation)VirtualFree(allocation,0,MEM_RELEASE);std::puts(error.what());return 1;}}
