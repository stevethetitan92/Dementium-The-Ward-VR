#include "../src/native_copy_preflight.hpp"
#include <array>
#include <cstring>
#include <cstdio>
#include <stdexcept>
using namespace nativeCopyPreflight;
static void require(bool value){if(!value)throw std::runtime_error("native copy preflight check failed");}
int main(){try{
    unsigned compared{};
    for(bool upload:{true,false}){
        nativeCopyBarrierPolicy::Proof proof{true,true,true,true};D3D12_RESOURCE_DESC texture{};texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;texture.Width=texture.Height=8;texture.DepthOrArraySize=texture.MipLevels=1;texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;texture.SampleDesc.Count=1;
        BufferProof buffer;buffer.known=true;buffer.identity=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(0x200));buffer.description.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.description.Width=2560;buffer.heap=upload?D3D12_HEAP_TYPE_UPLOAD:D3D12_HEAP_TYPE_READBACK;buffer.rowBytesKnown=true;buffer.rowBytes=32;
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(0x100));barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON;barrier.Transition.StateAfter=upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE;
        std::array<unsigned char,512> frame{};D3D12_TEXTURE_COPY_LOCATION image{},linear{};image.pResource=barrier.Transition.pResource;image.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;linear.pResource=buffer.identity;linear.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;linear.PlacedFootprint.Offset=512;linear.PlacedFootprint.Footprint={texture.Format,8,8,1,256};D3D12_BOX box{0,0,0,8,8,1};
        auto writeFrame=[&]{std::memcpy(frame.data()+(upload?0x48:0xd0),&image,sizeof image);std::memcpy(frame.data()+(upload?0x78:0x20),&linear,sizeof linear);std::memcpy(frame.data()+(upload?0x20:0x58),&box,sizeof box);};writeFrame();
        constexpr std::uintptr_t address=0x100000;auto reader=[&](std::uintptr_t at,void* output,size_t count){if(at<address||at-address>frame.size()||count>frame.size()-(at-address))return false;std::memcpy(output,frame.data()+at-address,count);return true;};
        const auto caller=upload?0x1eabf1:0x1ea8eb;auto plan=capture(proof,caller,texture,1,&barrier,address,buffer,reader);require(plan.valid&&plan.source.pResource==(upload?buffer.identity:image.pResource)&&plan.destination.pResource==(upload?image.pResource:buffer.identity)&&plan.copyReturnRva==(upload?0x1eac36:0x1ea920)&&plan.endReturnRva==(upload?0x1eac72:0x1ea95a));++compared;
        for(unsigned mutation=0;mutation<20;++mutation){auto p=proof;auto t=texture;auto b=buffer;auto rb=barrier;auto frameAddress=address;auto oldImage=image,oldLinear=linear;auto oldBox=box;
            switch(mutation){case 0:p.signaturesVerified=false;break;case 1:b.known=false;break;case 2:b.heap=D3D12_HEAP_TYPE_DEFAULT;break;case 3:b.identity=image.pResource;break;case 4:b.description.Width=2048;break;case 5:t.MipLevels=2;break;case 6:t.DepthOrArraySize=2;break;case 7:t.Format=DXGI_FORMAT_UNKNOWN;break;case 8:image.SubresourceIndex=1;break;case 9:image.pResource=buffer.identity;break;case 10:linear.PlacedFootprint.Offset=1;break;case 11:linear.PlacedFootprint.Footprint.RowPitch=255;break;case 12:box.left=1;break;case 13:box.right=7;break;case 14:frameAddress=(std::numeric_limits<std::uintptr_t>::max)()-16;break;case 15:rb.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;break;case 16:b.rowBytesKnown=false;break;case 17:b.rowBytes=257;break;case 18:b.description.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;break;case 19:linear.PlacedFootprint.Footprint.Width=7;break;}
            writeFrame();require(!capture(p,caller,t,1,&rb,frameAddress,b,reader).valid);++compared;image=oldImage;linear=oldLinear;box=oldBox;writeFrame();
        }
        auto unavailable=[](std::uintptr_t,void*,size_t){return false;};require(!capture(proof,caller,texture,1,&barrier,address,buffer,unavailable).valid);++compared;
    }
    std::printf("PASS: %u read-only upload/readback preflights validate native stack layouts, resource roles, footprints, box/buffer bounds, heaps and failure/overflow cases; no commands changed or COM identities dereferenced.\n",compared);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
