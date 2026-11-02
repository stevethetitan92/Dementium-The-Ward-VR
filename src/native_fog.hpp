#pragma once
#include <windows.h>
#include <dxcapi.h>
#include <d3d12.h>
#include <cassert>
namespace nativeFogChecksum {
#include "../third_party/dxil_hash/DxilHash.cpp"
}
#include <wrl/client.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <atomic>
namespace nativeFog {
inline std::atomic<bool> enabled{false};
inline std::atomic<ULONGLONG> diagnosticUntil{};
inline bool active(ULONGLONG now){return enabled.load()||now<diagnosticUntil.load();}
// The native PS samples its fog LUT from SV_Position.z. Re-express that
// lookup in the native depth range, leaving VR raster depth and lighting alone.
inline bool coefficients(float* pixel,const float* native,const float* vr){
    if(!pixel||!native||!vr||std::fabs(native[14])<.01f||std::fabs(vr[14])<.01f||std::fabs(vr[11])<1e-8f)return false;
    float oldA=(-2*native[10]-native[14])/native[14],oldB=-2*native[11]/native[14];
    float newA=(-2*vr[10]-vr[14])/vr[14],newB=-2*vr[11]/vr[14];
    float scale=oldB/newB,bias=oldA-scale*newA;
    if(!std::isfinite(scale)||!std::isfinite(bias))return false;
    constexpr unsigned lookupBits=0x3eff01fc;float lookup;std::memcpy(&lookup,&lookupBits,4);
    pixel[109]=0;pixel[110]=lookup*scale;pixel[111]=lookup*(bias+1);return true;
}
inline bool patch(D3D12_SHADER_BYTECODE code,Microsoft::WRL::ComPtr<IDxcBlob>& result,const wchar_t* libraryPath=nullptr){
    using Microsoft::WRL::ComPtr;
    if(!code.pShaderBytecode||code.BytecodeLength<36||code.BytecodeLength>1024*1024)return false;
    std::vector<unsigned char> bytes(static_cast<const unsigned char*>(code.pShaderBytecode),static_cast<const unsigned char*>(code.pShaderBytecode)+code.BytecodeLength);
    auto read=[&](size_t at){unsigned v{};if(at+4<=bytes.size())std::memcpy(&v,bytes.data()+at,4);return v;};
    if(read(0)!=0x43425844||read(24)!=bytes.size()||read(28)>64||32ull+4ull*read(28)>bytes.size())return false;
    size_t match=0;unsigned found=0;
    constexpr unsigned before[]={0x09000032,0x100012,0,0x10102a,0,0x4001,0x3eff01fc,0x4001,0x3eff01fc,0x05000036,0x100022,0,0x4001,0};
    for(unsigned part=0;part<read(28);part++){
        size_t at=read(32+4*part);if(at+8>bytes.size())return false;
        auto tag=read(at),size=read(at+4);if(size%4||at+8+size>bytes.size())return false;
        if(tag!=0x58454853&&tag!=0x52444853)continue;
        for(size_t i=at+16;i<at+8+size;){
            unsigned count=(read(i)>>24)&127;if(!count||i+count*4>at+8+size)return false;
            if(i+sizeof(before)<=at+8+size&&!std::memcmp(bytes.data()+i,before,sizeof(before))){match=i;++found;}
            i+=4*count;
        }
    }
    if(found!=1)return false;
    // Load the validated original container before editing its pinned part.
    // DXC's serializer retains its hash algorithm and recomputes the checksum.
    static HMODULE compiler=[](){wchar_t file[32768];DWORD n=GetModuleFileNameW(nullptr,file,32768);if(!n||n>=32768)return HMODULE{};auto slash=wcsrchr(file,L'\\');if(!slash)return HMODULE{};wcscpy_s(slash+1,32768-(slash+1-file),L"dxcompiler.dll");return LoadLibraryExW(file,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);}();
    HMODULE selected=libraryPath?LoadLibraryExW(libraryPath,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS):compiler;
    if(!selected)return false;
    auto create=reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(selected,"DxcCreateInstance"));if(!create)return false;
    ComPtr<IDxcLibrary> library;ComPtr<IDxcContainerBuilder> builder;ComPtr<IDxcBlobEncoding> source;
    if(FAILED(create(CLSID_DxcLibrary,IID_PPV_ARGS(&library)))||FAILED(create(CLSID_DxcContainerBuilder,IID_PPV_ARGS(&builder)))||FAILED(library->CreateBlobWithEncodingFromPinned(bytes.data(),static_cast<UINT32>(bytes.size()),0,&source))||FAILED(builder->Load(source.Get())))return false;
    // r0.xy = depth * (scale,0) + (bias,0), replacing MAD x and MOV y.
    constexpr unsigned after[]={0x0b000032,0x100032,0,0x10102a,0,0x208566,0,27,0x208576,0,27,0x0100003a,0x0100003a,0x0100003a};
    static_assert(sizeof(before)==sizeof(after));std::memcpy(bytes.data()+match,after,sizeof(after));
    // Preserve the native flashlight's angular AND distance attenuation before
    // room-light/material code reuses r3. Inside its cone, allow illuminated
    // surfaces through the black fog; outside it the original fog is unchanged.
    std::vector<unsigned> shader;
    std::vector<unsigned char> rebuilt;
    ComPtr<IDxcBlobEncoding> shaderBlob;
    for(unsigned part=0;part<read(28);++part){
        const size_t at=read(32+4*part);const unsigned tag=read(at),size=read(at+4);
        if(tag!=0x58454853&&tag!=0x52444853)continue;
        constexpr unsigned lampStart[]={0x08000039,0x100082,1,0x20800a,0,13,0x4001,0};
        constexpr unsigned lampDiffuse[]={0x0b000032,0x100072,4,0x208246,0,7,0x100aa6,3,0x208246,0,5};
        constexpr unsigned fogBlend[]={0x09000000,0x1000e2,0,0x100906,2,0x80208906,0x41,0,23};
        shader={read(at+8),0};unsigned startCount=0,diffuseCount=0,blendCount=0,tempCount=0;
        for(size_t i=at+16;i<at+8+size;){
            const unsigned count=(read(i)>>24)&127;
            auto matches=[&](const auto& pattern){return i+sizeof(pattern)<=at+8+size&&!std::memcmp(bytes.data()+i,pattern,sizeof(pattern));};
            if(matches(lampStart)){shader.insert(shader.end(),{0x05000036,0x100012,7,0x4001,0});++startCount;}
            if(matches(lampDiffuse)){shader.insert(shader.end(),{0x05000036,0x100012,7,0x10003a,3});++diffuseCount;}
            if(matches(fogBlend)){shader.insert(shader.end(),{0x07002034,0x100012,0,0x10000a,0,0x10000a,7});++blendCount;}
            if(read(i)==0x02000068&&read(i+4)==7){shader.insert(shader.end(),{0x02000068,8});++tempCount;}
            else for(unsigned j=0;j<count;++j)shader.push_back(read(i+j*4));
            i+=count*4;
        }
        if(startCount!=1||diffuseCount!=1||blendCount!=1||tempCount!=1)return false;
        shader[1]=static_cast<unsigned>(shader.size());
        // Rebuild the container with the larger shader part, then let DXC
        // serialize its checksum. DXC AddPart rejects legacy SHEX parts.
        rebuilt.assign(bytes.begin(),bytes.begin()+32+4*read(28));
        for(unsigned k=0;k<read(28);++k){
            unsigned offset=static_cast<unsigned>(rebuilt.size());std::memcpy(rebuilt.data()+32+4*k,&offset,4);
            size_t old=read(32+4*k);unsigned oldSize=read(old+4);
            if(k==part){
                unsigned newSize=static_cast<unsigned>(shader.size()*4);
                auto append=[&](const void* p,size_t n){auto b=static_cast<const unsigned char*>(p);rebuilt.insert(rebuilt.end(),b,b+n);};
                append(&tag,4);append(&newSize,4);append(shader.data(),newSize);
            }else rebuilt.insert(rebuilt.end(),bytes.begin()+old,bytes.begin()+old+8+oldSize);
        }
        unsigned total=static_cast<unsigned>(rebuilt.size());std::memcpy(rebuilt.data()+24,&total,4);
        // Legacy SHEX containers require the retail checksum before DXC Load;
        // otherwise its serializer deliberately treats the shader as unsigned.
        nativeFogChecksum::ComputeHashRetail(rebuilt.data()+20,total-20,rebuilt.data()+4);
        builder.Reset();
        if(FAILED(create(CLSID_DxcContainerBuilder,IID_PPV_ARGS(&builder)))||
           FAILED(library->CreateBlobWithEncodingFromPinned(rebuilt.data(),total,0,&shaderBlob))||FAILED(builder->Load(shaderBlob.Get())))return false;
        break;
    }
    ComPtr<IDxcOperationResult> operation;HRESULT status{};
    return SUCCEEDED(builder->SerializeContainer(&operation))&&operation&&SUCCEEDED(operation->GetStatus(&status))&&SUCCEEDED(status)&&SUCCEEDED(operation->GetResult(&result))&&result;
}
}
