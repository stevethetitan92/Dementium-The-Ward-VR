#include "../src/particle_fog_depth.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
using Microsoft::WRL::ComPtr;
static void check(HRESULT h){if(FAILED(h)){std::printf("HRESULT %08lx\n",h);std::abort();}}
int main(int argc,char** argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<char> original((std::istreambuf_iterator<char>(f)),{});
 assert(particleFogDepth::supported(original.data(),original.size()));
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;
 check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&ctx));
 const char* vs=R"(cbuffer Test:register(b0){float4 tint;float4 depth;};
 struct Out{float4 pos:SV_POSITION;float2 uv:TEXCOORD0;float4 color:COLOR0;float4 clip:POSITION2;};
 Out main(uint id:SV_VertexID){Out o;float2 p=float2((id<<1)&2,id&2);o.pos=float4(p*float2(2,-2)+float2(-1,1),0,1);o.uv=float2(.5,.5);o.color=tint;o.clip=float4(0,0,depth.x,1);return o;})";
 ComPtr<ID3DBlob> vcode,pcode,err;check(D3DCompile(vs,strlen(vs),nullptr,nullptr,nullptr,"main","vs_5_0",0,0,&vcode,&err));
 check(D3DCompile(particleFogDepth::shader,sizeof(particleFogDepth::shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pcode,&err));
 ComPtr<ID3D11VertexShader> vertex;ComPtr<ID3D11PixelShader> native,patched;
 check(device->CreateVertexShader(vcode->GetBufferPointer(),vcode->GetBufferSize(),nullptr,&vertex));
 check(device->CreatePixelShader(original.data(),original.size(),nullptr,&native));
 check(device->CreatePixelShader(pcode->GetBufferPointer(),pcode->GetBufferSize(),nullptr,&patched));
 D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 float white[]={.8f,.7f,.6f,.5f};D3D11_SUBRESOURCE_DATA initial{white,16,16};ComPtr<ID3D11Texture2D> texture,target,readback;
 check(device->CreateTexture2D(&td,&initial,&texture));ComPtr<ID3D11ShaderResourceView> srv;check(device->CreateShaderResourceView(texture.Get(),nullptr,&srv));
 td.BindFlags=D3D11_BIND_RENDER_TARGET;check(device->CreateTexture2D(&td,nullptr,&target));ComPtr<ID3D11RenderTargetView> rtv;check(device->CreateRenderTargetView(target.Get(),nullptr,&rtv));
 td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&td,nullptr,&readback));
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=1056;bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ComPtr<ID3D11Buffer> pcb,vcb;
 check(device->CreateBuffer(&bd,nullptr,&pcb));bd.ByteWidth=32;check(device->CreateBuffer(&bd,nullptr,&vcb));
 D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;ComPtr<ID3D11SamplerState> sampler;check(device->CreateSamplerState(&sd,&sampler));
 ctx->VSSetShader(vertex.Get(),nullptr,0);ctx->VSSetConstantBuffers(0,1,vcb.GetAddressOf());ctx->PSSetConstantBuffers(0,1,pcb.GetAddressOf());ctx->PSSetShaderResources(0,1,srv.GetAddressOf());ctx->PSSetSamplers(0,1,sampler.GetAddressOf());ctx->OMSetRenderTargets(1,rtv.GetAddressOf(),nullptr);
 D3D11_VIEWPORT vp{0,0,1,1,0,1};ctx->RSSetViewports(1,&vp);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 std::array<float,264> source{},mapped{};float v[8]={.6f,.025f,.01f,.8f,0,0,0,0};
 auto render=[&](ID3D11PixelShader* ps,const float* constants,float ratio){v[4]=ratio;ctx->UpdateSubresource(vcb.Get(),0,nullptr,v,0,0);ctx->UpdateSubresource(pcb.Get(),0,nullptr,constants,0,0);ctx->PSSetShader(ps,nullptr,0);const float clear[4]={-1,-1,-1,-1};ctx->ClearRenderTargetView(rtv.Get(),clear);ctx->Draw(3,0);ctx->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE r{};check(ctx->Map(readback.Get(),0,D3D11_MAP_READ,0,&r));std::array<float,4> result;std::memcpy(result.data(),r.pData,16);ctx->Unmap(readback.Get(),0);return result;};
 unsigned cases=0;float worst=0;
 for(unsigned fog=0;fog<2;++fog)for(unsigned color=0;color<3;++color){
  source[0]=color==0?0:.9f;source[1]=color==2?.8f:0;source[2]=color==2?.7f:0;std::memcpy(&source[3],&fog,4);
  for(unsigned i=0;i<128;++i){source[4+i]=1.f-i/128.f;source[132+i]=-1.f/128;}
  for(float z:{-.021f,-.1f,-.2f,-.4f,-1.f,-4.f,-30.f,-100.f}){
   float n[16]{},e[16]{};n[14]=e[14]=-1;float ns=100/(100-.2f),es=100/(100-.02f);n[10]=(ns+1)*.5f;n[11]=.2f*ns*.5f;e[10]=(es+1)*.5f;e[11]=.02f*es*.5f;
   auto ratio=[z](const float* p){return (-2*(p[10]*z+p[11])-p[14]*z)/(p[14]*z);};
   assert(particleFogDepth::prepare(mapped.data(),source.data(),n,e,true));
   auto a=render(native.Get(),source.data(),ratio(n));auto b=render(patched.Get(),mapped.data(),ratio(e));
   for(unsigned c=0;c<4;++c){float delta=std::fabs(a[c]-b[c]);worst=(std::max)(worst,delta);assert(delta<.00003f);}
   assert(std::fabs(b[3]-.4f)<.000001f);++cases;
  }
 }
 std::printf("PASS %u actual native/patch WARP pixel pairs; max RGBA difference %.9g; alpha preserved\n",cases,worst);
}
