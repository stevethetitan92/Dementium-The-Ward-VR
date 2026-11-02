#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <limits>
namespace remoteExport {
// Resolve a named PE32+ export from the image that is actually loaded. The
// reader receives RVAs; no local DLL layout or process execution is required.
template<class Reader> inline std::uintptr_t locate(std::uintptr_t base,DWORD mappedBytes,const char* name,Reader reader){
    if(!base||!name||base>(std::numeric_limits<std::uintptr_t>::max)()-mappedBytes)return 0;
    auto read=[&](DWORD rva,void* output,size_t bytes){return rva<=mappedBytes&&bytes<=mappedBytes-rva&&reader(rva,output,bytes);};
    IMAGE_DOS_HEADER dos{};if(!read(0,&dos,sizeof dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0)return 0;
    IMAGE_NT_HEADERS64 nt{};if(!read(static_cast<DWORD>(dos.e_lfanew),&nt,sizeof nt)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.FileHeader.SizeOfOptionalHeader<sizeof(IMAGE_OPTIONAL_HEADER64)||nt.OptionalHeader.NumberOfRvaAndSizes<1)return 0;
    if(!nt.OptionalHeader.SizeOfImage||nt.OptionalHeader.SizeOfImage>mappedBytes)return 0;
    mappedBytes=nt.OptionalHeader.SizeOfImage;
    const auto& directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if(!directory.VirtualAddress||directory.Size<sizeof(IMAGE_EXPORT_DIRECTORY)||directory.VirtualAddress>mappedBytes||directory.Size>mappedBytes-directory.VirtualAddress)return 0;
    IMAGE_EXPORT_DIRECTORY exports{};if(!read(directory.VirtualAddress,&exports,sizeof exports)||!exports.NumberOfNames||!exports.NumberOfFunctions||exports.NumberOfNames>65536||exports.NumberOfFunctions>65536||!exports.AddressOfNames||!exports.AddressOfNameOrdinals||!exports.AddressOfFunctions)return 0;
    std::vector<DWORD> names(exports.NumberOfNames),functions(exports.NumberOfFunctions);std::vector<WORD> ordinals(exports.NumberOfNames);
    if(!read(exports.AddressOfNames,names.data(),names.size()*sizeof(DWORD))||!read(exports.AddressOfNameOrdinals,ordinals.data(),ordinals.size()*sizeof(WORD))||!read(exports.AddressOfFunctions,functions.data(),functions.size()*sizeof(DWORD)))return 0;
    size_t length=std::strlen(name);if(!length||length>=128)return 0;char candidate[128]{};
    for(size_t i=0;i<names.size();++i){
        if(!read(names[i],candidate,length+1))return 0;
        if(std::memcmp(candidate,name,length+1))continue;
        if(ordinals[i]>=functions.size())return 0;auto rva=functions[ordinals[i]];
        // Forwarder strings are not executable addresses in this module.
        if(!rva||rva>=mappedBytes||(rva>=directory.VirtualAddress&&rva-directory.VirtualAddress<directory.Size))return 0;
        return base+rva;
    }
    return 0;
}
}
