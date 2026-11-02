#pragma once
#include "native_uniform_diagnostic.hpp"
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <optional>
namespace nativeZeroConstantGuard {
inline std::array<bool,2> rootStages(const D3D12_ROOT_SIGNATURE_DESC& root){
    std::array<bool,2> result{};
    for(unsigned index=0;index<2&&index<root.NumParameters;++index){
        const auto& p=root.pParameters[index];
        result[index]=p.ParameterType==D3D12_ROOT_PARAMETER_TYPE_CBV&&p.Descriptor.ShaderRegister==0&&p.Descriptor.RegisterSpace==0&&
            p.ShaderVisibility==(index?D3D12_SHADER_VISIBILITY_PIXEL:D3D12_SHADER_VISIBILITY_VERTEX);
    }
    return result;
}
inline bool unusedRoot0(const D3D12_SHADER_BYTECODE& code,unsigned stage){
    if(stage>1||!code.pShaderBytecode||!code.BytecodeLength)return false;
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shader;
    if(FAILED(D3DReflect(code.pShaderBytecode,code.BytecodeLength,IID_PPV_ARGS(&shader))))return false;
    D3D12_SHADER_DESC description{};if(FAILED(shader->GetDesc(&description)))return false;
    if(D3D12_SHVER_GET_TYPE(description.Version)!=static_cast<UINT>(stage?D3D12_SHVER_PIXEL_SHADER:D3D12_SHVER_VERTEX_SHADER))return false;
    for(unsigned i=0;i<description.BoundResources;++i){D3D12_SHADER_INPUT_BIND_DESC resource{};
        if(FAILED(shader->GetResourceBindingDesc(i,&resource)))return false;
        if(resource.Type==D3D_SIT_CBUFFER&&resource.BindPoint==0&&resource.Space==0)return false;
    }
    return true;
}
struct Proof {
    unsigned parameter{};bool nativeCall{},stageOnlyRoot{},shaderDoesNotRead{},listDeviceOwned{},resourceDeviceOwned{},resourceUploadHeap{};
    std::uintptr_t list{},liveResource{};std::uint64_t liveBase{},liveBytes{};
};
// This policy does not suppress an arbitrary invalid CBV. It aliases ONLY a
// proven unused native zero-byte end binding to the same live ring's base.
inline std::optional<std::uint64_t> replacement(const nativeUniformDiagnostic::Snapshot& native,const Proof& proof,std::uint64_t address){
    if(proof.parameter>1||!proof.nativeCall||!proof.stageOnlyRoot||!proof.shaderDoesNotRead||!proof.listDeviceOwned||!proof.resourceDeviceOwned||!proof.resourceUploadHeap)return {};
    if(!native.ring||!native.shader||!native.commandList||native.commandList!=proof.list||!native.resource||native.resource!=proof.liveResource)return {};
    if(native.capacity!=0x10000000||native.cursor!=native.capacity||!native.gpu||(native.gpu&255)||!native.atEnd(address))return {};
    if(proof.liveBase!=native.gpu||proof.liveBytes!=native.capacity)return {};
    if(!(native.dirty&(proof.parameter?0x20:0x10)))return {};
    if(proof.parameter?(!native.pixelKnown||native.pixelBytes!=0):(!native.vertexKnown||native.vertexBytes!=0))return {};
    return native.gpu;
}
}
