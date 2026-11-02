#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
using Microsoft::WRL::ComPtr;
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 1;
    HMODULE dxc=LoadLibraryExW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\dxcompiler.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!dxc) return 2;
    auto create=reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(dxc,"DxcCreateInstance"));
    ComPtr<IDxcLibrary> library; ComPtr<IDxcContainerReflection> container;
    if(FAILED(create(CLSID_DxcLibrary,IID_PPV_ARGS(&library)))||FAILED(create(CLSID_DxcContainerReflection,IID_PPV_ARGS(&container)))) return 3;
    HANDLE file=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) return 4;
    LARGE_INTEGER size; GetFileSizeEx(file,&size);
    HANDLE mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);
    auto data=static_cast<const unsigned char*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,0));
    if(!data) return 5;
    unsigned found=0;
    for(size_t offset=0;offset+32<static_cast<size_t>(size.QuadPart);offset++) {
        if(memcmp(data+offset,"DXBC",4)) continue;
        uint32_t bytes{},chunks{}; memcpy(&bytes,data+offset+24,4); memcpy(&chunks,data+offset+28,4);
        if(bytes<32||offset+bytes>static_cast<size_t>(size.QuadPart)||chunks>128||32+4*chunks>bytes) continue;
        ComPtr<ID3D12ShaderReflection> reflection;
        ComPtr<IDxcBlobEncoding> blob;
        if(SUCCEEDED(library->CreateBlobWithEncodingFromPinned(data+offset,bytes,0,&blob))&&SUCCEEDED(container->Load(blob.Get()))) {
            UINT32 part{};
            if(SUCCEEDED(container->FindFirstPartKind(0x4c495844,&part))) container->GetPartReflection(part,IID_PPV_ARGS(&reflection));
        }
        if(!reflection) D3DReflect(data+offset,bytes,IID_PPV_ARGS(&reflection));
        if(!reflection) continue;
        D3D12_SHADER_DESC desc{}; reflection->GetDesc(&desc);
        printf("shader offset=%zu bytes=%u version=%x inputs=%u resources=%u\n",offset,bytes,desc.Version,desc.InputParameters,desc.BoundResources);
        for(UINT i=0;i<desc.InputParameters;i++) { D3D12_SIGNATURE_PARAMETER_DESC p{}; reflection->GetInputParameterDesc(i,&p); printf(" input %s%u register=%u mask=%u\n",p.SemanticName,p.SemanticIndex,p.Register,p.Mask); }
        for(UINT i=0;i<desc.BoundResources;i++) { D3D12_SHADER_INPUT_BIND_DESC r{}; reflection->GetResourceBindingDesc(i,&r); printf(" resource %s type=%u register=%u space=%u\n",r.Name,r.Type,r.BindPoint,r.Space); }
        for(UINT i=0;i<desc.ConstantBuffers;i++) {
            auto cb=reflection->GetConstantBufferByIndex(i); D3D12_SHADER_BUFFER_DESC bd{}; cb->GetDesc(&bd);
            printf(" buffer %s bytes=%u\n",bd.Name,bd.Size);
            for(UINT j=0;j<bd.Variables;j++) { auto v=cb->GetVariableByIndex(j); D3D12_SHADER_VARIABLE_DESC vd{}; v->GetDesc(&vd); D3D12_SHADER_TYPE_DESC td{}; v->GetType()->GetDesc(&td); printf("  %s offset=%u bytes=%u rows=%u cols=%u class=%u\n",vd.Name,vd.StartOffset,vd.Size,td.Rows,td.Columns,td.Class); }
        }
        found++; offset+=bytes-1;
    }
    printf("reflected_shader_count=%u\n",found);
    UnmapViewOfFile(data); CloseHandle(mapping); CloseHandle(file); return found?0:6;
}
