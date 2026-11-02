#define NOMINMAX
#include "../src/native_uniform_diagnostic.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <fstream>
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void check(HRESULT result){require(SUCCEEDED(result),"native uniform diagnostic D3D12 operation failed");}
static DWORD recordBoundary(ID3D12GraphicsCommandList* list,D3D12_GPU_VIRTUAL_ADDRESS address){
    __try{list->SetGraphicsRootConstantBufferView(0,address);}
    __except(GetExceptionCode()==0x87d?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return GetExceptionCode();}
    return 0;
}
int main(){try{
    setvbuf(stdout,nullptr,_IONBF,0);
    std::ifstream file(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary|std::ios::ate);require(bool(file),"native executable unavailable for read-only signature validation");
    auto length=file.tellg();require(length>4096,"native executable truncated");std::vector<unsigned char> exe(static_cast<size_t>(length));file.seekg(0);require(bool(file.read(reinterpret_cast<char*>(exe.data()),exe.size())),"native executable read failed");
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(exe.data());require(dos->e_magic==IMAGE_DOS_SIGNATURE&&dos->e_lfanew>0&&static_cast<size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)<=exe.size(),"native executable header changed");
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(exe.data()+dos->e_lfanew);require(nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64,"native executable architecture changed");
    auto sections=IMAGE_FIRST_SECTION(nt);
    auto code=[&](DWORD rva,size_t count){for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){const auto& s=sections[i];if(rva<s.VirtualAddress||rva-s.VirtualAddress>=s.SizeOfRawData)continue;const auto at=size_t(s.PointerToRawData)+rva-s.VirtualAddress;require(count<=s.SizeOfRawData-(rva-s.VirtualAddress)&&at<=exe.size()&&count<=exe.size()-at,"native code range truncated");return std::vector<unsigned char>(exe.begin()+at,exe.begin()+at+count);}throw std::runtime_error("native code section missing");};
    const std::uintptr_t image=0x100000000ULL,ringAddress=image+0x3e89c0,shader=image+0x1000,vertex=image+0x2000,pixel=image+0x3000;
    std::map<std::uintptr_t,std::vector<unsigned char>> memory;
    auto put=[&](std::uintptr_t at,const auto& value){auto bytes=reinterpret_cast<const unsigned char*>(&value);memory[at]={bytes,bytes+sizeof value};};
    memory[image+0x1e9428]=code(0x1e9428,18);memory[image+0x1e77fe]=code(0x1e77fe,8);
    // Check the unsigned strict-greater comparison actually used at the edge.
    require(code(0x1e944d,8)==std::vector<unsigned char>({0x48,0x3b,0x47,0x08,0x48,0x0f,0x47,0xde}),"native ring wrap predicate changed");
    const std::array<std::uint64_t,6> ring={image+0x31e738,0x10000000,0x10000000,0x12340000,0x23450000,0x07000000};put(ringAddress,ring);put(image+0x6bb798,ringAddress);put(image+0xc1acb0,shader);put(shader+0x18,vertex);put(shader+0x20,pixel);
    const int vertexBytes=0,pixelBytes=448,frame=2;put(vertex+0x18,vertexBytes);put(pixel+0x18,pixelBytes);put(image+0x6945a8,frame);
    auto read=[&](std::uintptr_t at,void* out,size_t count){auto it=memory.upper_bound(at);if(it==memory.begin())return false;--it;const auto offset=at-it->first;if(offset>it->second.size()||count>it->second.size()-offset)return false;std::memcpy(out,it->second.data()+offset,count);return true;};
    nativeUniformDiagnostic::Snapshot snapshot;require(nativeUniformDiagnostic::capture(image,snapshot,read),"native diagnostic rejected installed byte signatures");
    require(snapshot.vertexKnown&&snapshot.vertexBytes==0&&snapshot.pixelKnown&&snapshot.pixelBytes==448&&snapshot.frame==2&&snapshot.atEnd(0x17000000)&&!snapshot.contains(0x17000000)&&snapshot.contains(0x16ffffff),"native ring diagnostic boundary/layout mismatch");
    memory[image+0x1e9428][0]^=1;require(!nativeUniformDiagnostic::capture(image,snapshot,read)&&!snapshot.ring,"diagnostic accepted mismatched executable");memory[image+0x1e9428][0]^=1;
    memory.erase(ringAddress);require(!nativeUniformDiagnostic::capture(image,snapshot,read),"diagnostic accepted absent ring memory");
    nativeUniformDiagnostic::Snapshot overflow;overflow.gpu=UINT64_MAX-255;overflow.capacity=256;require(!overflow.atEnd(0),"native end predicate overflowed");
    // Independent translation of the verified allocation instructions, not a
    // replacement allocator and not a claim about the unavailable dump state.
    auto allocate=[](std::uint64_t cursor,std::uint64_t requested){auto offset=(cursor+255)&~std::uint64_t(255);if(offset+requested>0x10000000)offset=0;return 0x07000000+offset;};
    for(std::uint64_t cursor=0x10000000-255;cursor<=0x10000000;++cursor){require(allocate(cursor,0)==0x17000000,"zero-size boundary reproduction changed");require(allocate(cursor,1)==0x07000000,"nonzero allocation failed native wrap");}
    require(allocate(0x10000000-1280,1280)==0x17000000-1280,"native exact-fit nonzero allocation should remain in bounds");
    std::puts("native_zero_byte_boundary base=0x07000000 capacity=0x10000000 cursor=0x10000000 bytes=0 address=0x17000000 (one past end); 256 cursor-edge cases reproduced.");
    // CPU-side SDK validation only: deliberately record an invalid CBV in an
    // isolated software device. Never draw, close, or submit this command list.
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));check(messages->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,FALSE));check(messages->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,FALSE));
    D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;D3D12_ROOT_SIGNATURE_DESC rootDesc{1,&parameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> serialized,error;check(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error));ComPtr<ID3D12RootSignature> root;check(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=65536;desc.Height=1;desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ComPtr<ID3D12Resource> buffer;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&buffer)));
    ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));list->SetGraphicsRootSignature(root.Get());messages->ClearStoredMessages();list->SetGraphicsRootConstantBufferView(0,buffer->GetGPUVirtualAddress());require(!messages->GetNumStoredMessages(),"valid CBV fixture produced SDK messages");
    const auto validationException=recordBoundary(list.Get(),buffer->GetGPUVirtualAddress()+desc.Width);bool found710=false;
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T count{};check(messages->GetMessage(i,nullptr,&count));std::vector<unsigned char> storage(count);auto message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());check(messages->GetMessage(i,message,&count));if(message->ID==710)found710=true;else if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    require(found710,"one-past-end CBV did not reproduce SDK id710; revisit allocation hypothesis");printf("native_boundary_validation_exception=%lx\n",validationException);
    std::puts("PASS: installed native allocation signatures, bounded ring diagnostics and zero-byte edge reproduce SDK id710 on isolated WARP; invalid list never submitted, no game/process/XR calls. Actual crash ring state remains unproven.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
