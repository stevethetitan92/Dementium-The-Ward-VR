#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <cassert>
#include <wincodec.h>
#include <algorithm>
#include "../src/psx_asset_pack.hpp"
#include "../src/selected_hunting_texture.hpp"
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")
using namespace DirectX;using Microsoft::WRL::ComPtr;
static void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D material render failed: "+std::to_string(unsigned(hr)));}
struct Vertex{float p[3],n[3],c[3],uv[2],mode,nextP[3],nextN[3];};
int main(int argc,char** argv){try{
 const bool weaponMode=argc>4&&std::string(argv[1])=="--weapon";
 const unsigned weapon=weaponMode?std::stoul(argv[3]):0;
 psxAssets::Pack pack;
 if(weaponMode&&!psxAssets::load(argv[2],pack))throw std::runtime_error("Invalid selected pack");
 if(weaponMode)selectedHuntingTexture::apply(argv[2],pack);
 std::ifstream input("src/wristband_renderer.hpp");std::string text((std::istreambuf_iterator<char>(input)),{});
 auto begin=text.find("inline constexpr char shader[]=R\"(");assert(begin!=std::string::npos);begin+=34;auto shader=text.substr(begin,text.find(")\";",begin)-begin);
 ComPtr<ID3DBlob> vs,ps,error;for(auto entry:{"vs","ps"}){auto profile=std::string(entry)+"_5_0";auto& target=entry[0]=='v'?vs:ps;
  auto hr=D3DCompile(shader.data(),shader.size(),nullptr,nullptr,nullptr,entry,profile.c_str(),D3DCOMPILE_OPTIMIZATION_LEVEL3,0,target.GetAddressOf(),&error);if(FAILED(hr)&&error)puts(static_cast<const char*>(error->GetBufferPointer()));check(hr);}
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
 ComPtr<ID3D11VertexShader> vertexShader;ComPtr<ID3D11PixelShader> pixelShader;check(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertexShader));check(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixelShader));
 D3D11_INPUT_ELEMENT_DESC layout[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,36,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,44,D3D11_INPUT_PER_VERTEX_DATA,0},{"POSITION",1,DXGI_FORMAT_R32G32B32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",1,DXGI_FORMAT_R32G32B32_FLOAT,0,60,D3D11_INPUT_PER_VERTEX_DATA,0}};
 ComPtr<ID3D11InputLayout> il;check(device->CreateInputLayout(layout,7,vs->GetBufferPointer(),vs->GetBufferSize(),&il));
 const unsigned width=1000,height=weaponMode?600:240;D3D11_TEXTURE2D_DESC td{};td.Width=width;td.Height=height;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> color,depth,readback;ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DepthStencilView> dsv;check(device->CreateTexture2D(&td,nullptr,&color));check(device->CreateRenderTargetView(color.Get(),nullptr,&rtv));
 td.Format=DXGI_FORMAT_D32_FLOAT;td.BindFlags=D3D11_BIND_DEPTH_STENCIL;check(device->CreateTexture2D(&td,nullptr,&depth));check(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv));
 td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&td,nullptr,&readback));
 std::array<ComPtr<ID3D11Texture2D>,4> textures;std::array<ComPtr<ID3D11ShaderResourceView>,4> srvs;
 const unsigned char colors[20]={190,190,190,255,190,190,190,255,205,151,62,255,38,38,38,255,119,64,29,255};
 const unsigned char normals[20]={128,128,255,255,128,128,255,255,128,128,255,255,128,128,255,255,128,128,255,255};
 const unsigned char materials[20]={255,38,255,255,255,217,255,255,255,75,255,255,255,217,0,255,255,180,0,255};
 for(unsigned i=0;i<4;++i){D3D11_TEXTURE2D_DESC d{};d.Width=5;d.Height=d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{i==2?normals:i==3?materials:colors,20,0};check(device->CreateTexture2D(&d,&data,&textures[i]));check(device->CreateShaderResourceView(textures[i].Get(),nullptr,&srvs[i]));}
 if(weaponMode){
  check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
  const wchar_t* names[]={L"SelectedGeometry.png",L"SelectedGeometry.png",L"SelectedNormal.png",L"SelectedSurface.png"};
  for(unsigned i=0;i<4;++i){
   ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
   auto path=std::filesystem::path(argv[2])/names[i];check(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));check(decoder->GetFrame(0,&frame));check(factory->CreateFormatConverter(&converter));check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
   UINT w,h;check(converter->GetSize(&w,&h));std::vector<unsigned char> pixels(size_t(w)*h*4);check(converter->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data()));
   D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0};textures[i].Reset();srvs[i].Reset();check(device->CreateTexture2D(&d,&data,&textures[i]));check(device->CreateShaderResourceView(textures[i].Get(),nullptr,&srvs[i]));
  }
 }
 D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;ComPtr<ID3D11SamplerState> smp;check(device->CreateSamplerState(&sampler,&smp));
 std::vector<Vertex> vertices;for(unsigned sample=0;sample<5;++sample){auto point=[&](unsigned y,unsigned x){float theta=y*XM_PI/40,phi=x*XM_2PI/80;Vertex v{};v.n[0]=std::sin(theta)*std::cos(phi);v.n[1]=std::cos(theta);v.n[2]=std::sin(theta)*std::sin(phi);for(unsigned j=0;j<3;++j){v.p[j]=v.n[j]*.42f;v.c[j]=!weaponMode&&argc>2?std::stof(argv[2]):1;v.nextP[j]=v.p[j];v.nextN[j]=v.n[j];}v.uv[0]=(sample+.5f)/5;v.uv[1]=.5;v.mode=7;return v;};for(unsigned y=0;y<40;++y)for(unsigned x=0;x<80;++x){vertices.push_back(point(y,x));vertices.push_back(point(y+1,x));vertices.push_back(point(y,x+1));vertices.push_back(point(y,x+1));vertices.push_back(point(y+1,x));vertices.push_back(point(y+1,x+1));}}
 XMMATRIX weaponView=XMMatrixIdentity();float weaponWidth=1,weaponHeight=1;
 if(weaponMode){
  vertices.clear();for(const auto& section:pack.sections)if(section.weapon==weapon&&section.kind==0)for(unsigned j=0;j<section.count;++j){Vertex v{};std::memcpy(&v,pack.vertices[section.first+j].value,48);std::memcpy(v.nextP,v.p,12);std::memcpy(v.nextN,v.n,12);if(weapon==5&&v.mode==4){v.mode=8;v.uv[0]=4090.5f/8192;v.uv[1]=8190.5f/8192;}vertices.push_back(v);}
  if(vertices.empty())throw std::runtime_error("No weapon vertices");
  auto rotation=XMMatrixRotationY(argc>5?std::stof(argv[5]):1.2f)*XMMatrixRotationX(-.15f);
  XMVECTOR lo=XMVectorReplicate(1e9f),hi=XMVectorReplicate(-1e9f);
  for(auto& v:vertices){auto q=XMVector3TransformCoord(XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(v.p)),rotation);lo=XMVectorMin(lo,q);hi=XMVectorMax(hi,q);}
  XMFLOAT3 center,extent;XMStoreFloat3(&center,(lo+hi)*.5f);XMStoreFloat3(&extent,hi-lo);
  weaponHeight=std::max(extent.y*1.15f,extent.x*.6f*1.15f);weaponWidth=weaponHeight/ .6f;
  weaponView=rotation*XMMatrixTranslation(-center.x,-center.y,-center.z-3);
 }
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(vertices.size()*sizeof(Vertex));bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA data{vertices.data(),0,0};ComPtr<ID3D11Buffer> vb,cb;check(device->CreateBuffer(&bd,&data,&vb));bd.ByteWidth=144;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(device->CreateBuffer(&bd,nullptr,&cb));
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;ComPtr<ID3D11RasterizerState> rs;check(device->CreateRasterizerState(&raster,&rs));context->RSSetState(rs.Get());
 D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);auto target=rtv.Get();context->OMSetRenderTargets(1,&target,dsv.Get());float clear[]={.025f,.025f,.025f,1};context->ClearRenderTargetView(target,clear);context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
 context->VSSetShader(vertexShader.Get(),nullptr,0);context->PSSetShader(pixelShader.Get(),nullptr,0);context->IASetInputLayout(il.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);auto vertexBuffer=vb.Get();unsigned stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);auto constantBuffer=cb.Get();context->VSSetConstantBuffers(0,1,&constantBuffer);auto samplerPtr=smp.Get();context->PSSetSamplers(0,1,&samplerPtr);ID3D11ShaderResourceView* views[]={srvs[0].Get(),srvs[1].Get(),srvs[2].Get(),srvs[3].Get()};context->PSSetShaderResources(0,4,views);
 for(unsigned i=0;i<(weaponMode?1u:5u);++i){float constants[36]{};auto mv=weaponMode?weaponView:XMMatrixTranslation(float(i)-2,0,-3);XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(constants),mv*XMMatrixOrthographicRH(weaponMode?weaponWidth:5,weaponMode?weaponHeight:1.2f,.1f,20));XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(constants+20),mv);context->UpdateSubresource(cb.Get(),0,nullptr,constants,0,0);context->Draw(UINT(vertices.size()/(weaponMode?1:5)),UINT(i*vertices.size()/5));}
 context->CopyResource(readback.Get(),color.Get());D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));std::ofstream output(weaponMode?argv[4]:argc>1?argv[1]:"bin/material-response.ppm",std::ios::binary);output<<"P6\n"<<width<<" "<<height<<"\n255\n";
 std::array<unsigned long long,5> sums{};for(unsigned y=0;y<height;++y){auto row=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch;for(unsigned x=0;x<width;++x){output.write(reinterpret_cast<char*>(row+x*4),3);if(y>50&&y<190)sums[x/200]+=row[x*4]+row[x*4+1]+row[x*4+2];}}context->Unmap(readback.Get(),0);output.close();if(weaponMode){puts("Selected weapon rendered with runtime shader and bound atlas maps");return 0;}assert(sums[0]!=sums[1]);assert(sums[2]!=sums[3]);for(auto s:sums)assert(s>200000);puts("Actual material shaders rendered on WARP: polished metal, rough metal, brass, rubber and wood differ; readback saved");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
