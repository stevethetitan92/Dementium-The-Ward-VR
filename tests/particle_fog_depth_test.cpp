#include "../src/particle_fog_depth.hpp"
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <wrl/client.h>
#include <cassert>
#include <cstdio>
#include <array>
#include <limits>
using Microsoft::WRL::ComPtr;
int main(){
    std::array<unsigned char,1040> source{};for(unsigned i=0;i<source.size();++i)source[i]=static_cast<unsigned char>(i*13);
    auto original=source;std::array<unsigned char,1056> out{};
    unsigned enabled=1;std::memcpy(source.data()+12,&enabled,4);original=source;
    unsigned samples=0;
    for(float farPlane:{10.f,100.f,1000.f})for(float nearPlane:{.1f,.2f,.3f}){
        float native[16]{},eye[16]{};
        native[14]=eye[14]=-1;
        auto projection=[](float* p,float n,float f){float scale=f/(f-n);p[10]=(scale+1)*.5f;p[11]=n*scale*.5f;};
        projection(native,nearPlane,farPlane);projection(eye,.02f,farPlane);
        assert(particleFogDepth::prepare(out.data(),source.data(),native,eye,true));
        assert(!std::memcmp(out.data(),source.data(),1040));
        float remap[4];std::memcpy(remap,out.data()+1040,16);
        for(unsigned i=0;i<1000;++i){
            float z=-(.021f+(farPlane-.021f)*i/999.f);
            auto ratio=[z](const float* p){return (-2*(p[10]*z+p[11])-p[14]*z)/(p[14]*z);};
            assert(std::fabs(ratio(native)-(ratio(eye)*remap[0]+remap[1]))<.00002f);++samples;
        }
        assert(particleFogDepth::prepare(out.data(),source.data(),native,eye,false));
        for(unsigned i=0;i<1040;++i)assert(out[i]==((i>=12&&i<16)?0:source[i]));
    }
    assert(source==original);
    float invalid[16]{};assert(!particleFogDepth::prepare(out.data(),source.data(),invalid,invalid,true));
    assert(!particleFogDepth::prepare(nullptr,source.data(),invalid,invalid,true));
    assert(!particleFogDepth::supported(nullptr,1828));
    ComPtr<ID3DBlob> code,error;
    auto hr=D3DCompile(particleFogDepth::shader,sizeof(particleFogDepth::shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
    if(FAILED(hr)&&error)std::fputs(static_cast<const char*>(error->GetBufferPointer()),stderr);
    assert(SUCCEEDED(hr));
    ComPtr<ID3D12ShaderReflection> reflection;assert(SUCCEEDED(D3DReflect(code->GetBufferPointer(),code->GetBufferSize(),IID_PPV_ARGS(&reflection))));
    D3D12_SHADER_BUFFER_DESC cb{};auto buffer=reflection->GetConstantBufferByIndex(0);assert(SUCCEEDED(buffer->GetDesc(&cb))&&cb.Size==1056);
    D3D12_SHADER_VARIABLE_DESC remap{};assert(SUCCEEDED(buffer->GetVariableByName("depthRemap")->GetDesc(&remap))&&remap.StartOffset==1040);
    std::printf("PASS: %u native/VR depth comparisons, untouched native fog data, disable/fallback guards, shader compilation and layout\n",samples);
}
