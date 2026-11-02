#include "../src/native_flashlight_profile_diagnostic.hpp"
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdio>
static void require(bool value){if(!value)throw std::runtime_error("flashlight profile diagnostic check failed");}
int main(){try{
    constexpr std::uintptr_t base=0x10000000;std::vector<unsigned char> image(0x3e8f60);unsigned reads{};
    auto write=[&](size_t offset,const auto& value){std::memcpy(image.data()+offset,&value,sizeof value);};
    const std::array<unsigned char,4> method{0xf3,0x0f,0x5c,0xe2},field{0x41,0x89,0x51,0x20};write(0x441b6,method);write(0x1e9fa4,field);
    write(0x3a56e0,base+0x305e80);write(0x305ef8,base+0x441a0);write(0x3a5a88,std::uintptr_t(0x1234));
    std::array<float,256> samples{};for(unsigned i=0;i<256;++i)samples[i]=.95f+.05f*(i/255.f);write(0x3a5ae8,samples);
    write(0x3a5a6c,7);std::array<std::uint32_t,13> sampler{};sampler[0]=0x15;sampler[1]=sampler[2]=3;sampler[3]=1;write(0x3e8b50+7*52,sampler);
    auto reader=[&](std::uintptr_t address,void* output,size_t count){++reads;if(address<base||address-base>image.size()||count>image.size()-(address-base))return false;std::memcpy(output,image.data()+address-base,count);return true;};
    nativeFlashlightProfileDiagnostic::Snapshot captured;require(nativeFlashlightProfileDiagnostic::capture(base,captured,reader));require(captured.finite&&captured.textureObject==0x1234&&captured.samples==samples&&captured.minimum==.95f&&captured.maximum==1);
    require(captured.slotKnown&&captured.resolvedSlot==7&&captured.samplerKnown&&captured.samplerWords==sampler);
    require(!captured.resourceKnown&&!captured.bindingIdKnown); // Inert non-readable wrapper.
    write(0x3a5a88,base+0x20000);write(0x20008,std::uintptr_t(0x55550000));write(0x20030,std::uint32_t(42));
    require(nativeFlashlightProfileDiagnostic::capture(base,captured,reader)&&captured.resourceKnown&&captured.resourceIdentity==0x55550000&&captured.bindingIdKnown&&captured.nativeBindingId==42);
    require(nativeFlashlightProfileDiagnostic::capture(base,captured,[&](std::uintptr_t address,void* output,size_t count){if(address==base+0x20008||address==base+0x20030){std::memset(output,0x11,count);return false;}return reader(address,output,count);})&&!captured.resourceKnown&&!captured.resourceIdentity&&!captured.bindingIdKnown&&!captured.nativeBindingId);
    write(0x3a5a88,UINTPTR_MAX-2);require(nativeFlashlightProfileDiagnostic::capture(base,captured,reader)&&!captured.resourceKnown&&!captured.bindingIdKnown);write(0x3a5a88,std::uintptr_t(0x1234));
    for(int slot:{-1,20,INT32_MAX}){write(0x3a5a6c,slot);require(nativeFlashlightProfileDiagnostic::capture(base,captured,reader)&&captured.slotKnown&&captured.resolvedSlot==slot&&!captured.samplerKnown&&captured.samplerWords==std::array<std::uint32_t,13>{});}write(0x3a5a6c,7);
    require(nativeFlashlightProfileDiagnostic::capture(base,captured,[&](std::uintptr_t address,void* output,size_t count){return address!=base+0x3e8b50+7*52&&reader(address,output,count);})&&captured.finite&&!captured.samplerKnown&&captured.samplerWords==std::array<std::uint32_t,13>{});
    require(nativeFlashlightProfileDiagnostic::capture(base,captured,[&](std::uintptr_t address,void* output,size_t count){if(address==base+0x3a5a6c){std::memset(output,0x11,count);return false;}return reader(address,output,count);})&&!captured.slotKnown&&captured.resolvedSlot==-1&&!captured.samplerKnown);
    image[0x441b6]^=1;require(!nativeFlashlightProfileDiagnostic::capture(base,captured,reader)&&!captured.textureObject);image[0x441b6]^=1;
    write(0x3a56e0,std::uintptr_t(0));require(!nativeFlashlightProfileDiagnostic::capture(base,captured,reader));write(0x3a56e0,base+0x305e80);
    image[0x1e9fa4]^=1;require(!nativeFlashlightProfileDiagnostic::capture(base,captured,reader));image[0x1e9fa4]^=1;
    write(0x305ef8,base+0x441a1);require(!nativeFlashlightProfileDiagnostic::capture(base,captured,reader));write(0x305ef8,base+0x441a0);
    require(!nativeFlashlightProfileDiagnostic::capture(base,captured,[&](std::uintptr_t address,void* output,size_t count){return address!=base+0x3a5ae8&&reader(address,output,count);}));require(!captured.textureObject&&!captured.finite&&captured.samples==std::array<float,256>{});
    samples[5]=std::numeric_limits<float>::quiet_NaN();write(0x3a5ae8,samples);require(nativeFlashlightProfileDiagnostic::capture(base,captured,reader)&&!captured.finite&&captured.minimum==0&&captured.maximum==0&&std::isnan(captured.samples[5]));
    const auto before=reads;require(!nativeFlashlightProfileDiagnostic::capture(UINTPTR_MAX-10,captured,reader)&&reads==before);require(!nativeFlashlightProfileDiagnostic::capture(0,captured,reader)&&reads==before);
    require(!nativeFlashlightProfileDiagnostic::capture(base,captured,[](std::uintptr_t,void*,size_t){return false;}));
    std::puts("PASS: pinned read-only profile capture retains256 values/range, rejects mismatched code/vptr/unreadable/overflow, preserves nonfinite evidence; allocated fixture only, no native/game/GPU/XR.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
