#define WIN32_LEAN_AND_MEAN
#include "../src/remote_export.hpp"
#include <cassert>
#include <cstdio>
struct Image {
    std::vector<unsigned char> data=std::vector<unsigned char>(8192);
    explicit Image(DWORD function){
        IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=128;put(0,dos);
        IMAGE_NT_HEADERS64 nt{};nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt.OptionalHeader.SizeOfImage=static_cast<DWORD>(data.size());nt.OptionalHeader.NumberOfRvaAndSizes=16;nt.OptionalHeader.DataDirectory[0]={512,256};put(128,nt);
        IMAGE_EXPORT_DIRECTORY ex{};ex.Base=37;ex.NumberOfFunctions=ex.NumberOfNames=2;ex.AddressOfFunctions=800;ex.AddressOfNames=820;ex.AddressOfNameOrdinals=840;put(512,ex);
        DWORD functions[]={function,6144},names[]={900,940};WORD ordinals[]={1,0};std::memcpy(data.data()+800,functions,sizeof functions);std::memcpy(data.data()+820,names,sizeof names);std::memcpy(data.data()+840,ordinals,sizeof ordinals);std::memcpy(data.data()+900,"CaptureEyes",12);std::memcpy(data.data()+940,"TestInput",10);
    }
    template<class T> void put(size_t at,T value){std::memcpy(data.data()+at,&value,sizeof value);}
    std::uintptr_t find(const char* name,DWORD limit=8192){return remoteExport::locate(0x180000000,limit,name,[&](DWORD rva,void* output,size_t bytes){if(rva>data.size()||bytes>data.size()-rva)return false;std::memcpy(output,data.data()+rva,bytes);return true;});}
};
int wmain(int argc,wchar_t** argv){
    Image older(4096),rebuilt(5120);assert(older.find("TestInput")==0x180001000&&rebuilt.find("TestInput")==0x180001400);assert(older.find("CaptureEyes")==0x180001800&&older.find("Missing")==0);
    assert(older.find("TestInput",1024)==0);
    Image forwarded(600);assert(forwarded.find("TestInput")==0);
    Image empty(0);assert(empty.find("TestInput")==0);
    Image invalidOrdinal(4096);invalidOrdinal.put<WORD>(842,9);assert(invalidOrdinal.find("TestInput")==0);
    Image invalidNames(4096);invalidNames.put<DWORD>(820,8191);assert(invalidNames.find("TestInput")==0);
    Image invalidHeader(4096);invalidHeader.put<WORD>(0,0);assert(invalidHeader.find("TestInput")==0);
    Image invalidFunctions(4096);invalidFunctions.put<DWORD>(512+28,8191);assert(invalidFunctions.find("TestInput")==0);
    if(argc==2){
        // Map only the local DLL image for reading. Do not resolve imports,
        // run DllMain, call any export, initialize OpenXR or attach to a game.
        HMODULE module=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        if(!module){std::puts("FAIL: local DLL image mapping unavailable");return 1;}
        auto base=reinterpret_cast<std::uintptr_t>(module);auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);auto bytes=nt->OptionalHeader.SizeOfImage;
        for(auto name:{"Initialize","TestInput","CaptureEyes","Recenter","CalibratePosture"}){
            auto expected=reinterpret_cast<std::uintptr_t>(GetProcAddress(module,name));
            auto actual=remoteExport::locate(base,bytes,name,[&](DWORD rva,void* output,size_t count){std::memcpy(output,reinterpret_cast<const void*>(base+rva),count);return true;});
            if(!expected||actual!=expected){FreeLibrary(module);std::puts("FAIL: export parser differs from Windows GetProcAddress on actual built DLL");return 1;}
        }
        FreeLibrary(module);std::puts("PASS: all five actual built DLL exports match Windows GetProcAddress; image mapping only, no initialization or export execution.");
    }
    std::puts("PASS: loaded-image exports resolve independently of rebuilt local offsets; name/ordinal mapping, missing/truncated/forwarded/invalid exports fail closed; no process attachment or game/headset.");
}
