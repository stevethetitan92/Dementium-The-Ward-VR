#include "../src/native_copy_code.hpp"
#include <vector>
#include <fstream>
#include <cstdio>
#include <stdexcept>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){try{
    std::ifstream file(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});require(data.size()>4096,"native executable data unavailable");
    auto u16=[&](size_t at){require(at<=data.size()&&2<=data.size()-at,"truncated PE field");std::uint16_t value;std::memcpy(&value,data.data()+at,2);return value;};auto u32=[&](size_t at){require(at<=data.size()&&4<=data.size()-at,"truncated PE field");std::uint32_t value;std::memcpy(&value,data.data()+at,4);return value;};
    const auto pe=u32(60);require(u32(pe)==0x4550&&u16(pe+4)==0x8664&&u16(pe+24)==0x20b,"wrong PE architecture");const auto sectionStart=pe+24+u16(pe+20);const auto sectionCount=u16(pe+6);
    unsigned compared{};
    for(std::uintptr_t image:{std::uintptr_t(0x10000000),std::uintptr_t(0x7ff600000000)}){
        std::uintptr_t changed=0;bool unavailable=false;
        auto reader=[&](std::uintptr_t address,void* output,size_t count){
            if(address<image)return false;const auto rva=address-image;if(unavailable&&rva>=nativeCopyCode::readbackRva+64)return false;
            for(unsigned section=0;section<sectionCount;++section){const auto at=sectionStart+40*section,base=u32(at+12),bytes=u32(at+16),raw=u32(at+20);if(rva<base||rva-base>bytes||count>bytes-(rva-base))continue;const auto offset=raw+(rva-base);if(offset>data.size()||count>data.size()-offset)return false;std::memcpy(output,data.data()+offset,count);if(changed>=rva&&changed-rva<count)static_cast<unsigned char*>(output)[changed-rva]^=1;return true;}return false;
        };
        require(nativeCopyCode::verify(image,reader),"whole native helper pin differs from file");++compared;
        for(size_t byte=0;byte<nativeCopyCode::readback.size();++byte){changed=nativeCopyCode::readbackRva+byte;require(!nativeCopyCode::verify(image,reader),"modified readback helper accepted");++compared;}
        for(size_t byte=0;byte<nativeCopyCode::upload.size();++byte){changed=nativeCopyCode::uploadRva+byte;require(!nativeCopyCode::verify(image,reader),"modified upload helper accepted");++compared;}
        changed=0;unavailable=true;require(!nativeCopyCode::verify(image,reader),"unreadable helper accepted");++compared;
    }
    auto mustNotRead=[](std::uintptr_t,void*,size_t)->bool{throw std::runtime_error("overflow verification called reader");};require(!nativeCopyCode::verify(0,mustNotRead)&&!nativeCopyCode::verify((std::numeric_limits<std::uintptr_t>::max)()-16,mustNotRead),"invalid image arithmetic accepted");
    std::printf("PASS: %u whole-helper file/rebase/corruption/unreadable checks; every one of 2009 pinned native bytes rejects mutation, with overflow/zero guards; no image loading or native execution.\n",compared);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
