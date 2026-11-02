#pragma once
#include "selected_surface_maps.hpp"
namespace selectedFlashlight {
using Microsoft::WRL::ComPtr;
inline std::array<std::vector<unsigned char>,3> png;
inline std::array<ComPtr<ID3D12Resource>,3> textures,uploads;
inline ComPtr<ID3D12DescriptorHeap> heap;
inline unsigned section=~0u,glassSection=~0u;inline bool ready{};
inline bool contains(unsigned id){return id==section||id==glassSection;}
inline bool load(const std::filesystem::path& folder,psxAssets::Pack& pack){
 ready=false;section=glassSection=~0u;std::vector<unsigned char>b;
 if(!psxAssets::read(folder/L"SelectedFlashlight.bin",b,5000000)||b.size()<16)return false;
 unsigned h[4];std::memcpy(h,b.data(),16);
 if(h[0]!=0x54585044||h[1]!=2||!h[2]||h[2]>100000||h[2]%3||!h[3]||h[3]>=h[2]||h[3]%3||b.size()!=16+size_t(h[2])*sizeof(psxAssets::Vertex))return false;
 std::vector<psxAssets::Vertex> v(h[2]);std::memcpy(v.data(),b.data()+16,v.size()*sizeof(v[0]));
 for(unsigned j=0;j<v.size();++j){const float* p=v[j].value;for(unsigned k=0;k<12;++k)if(!std::isfinite(p[k]))return false;
  if(std::fabs(p[0])>.1f||std::fabs(p[1])>.1f||std::fabs(p[2])>.3f||p[9]<0||p[9]>1||p[10]<0||p[10]>1)return false;
  if(j<h[3]?p[11]!=7:(p[11]!=7.125f&&p[11]!=7.25f))return false;
  float n=p[3]*p[3]+p[4]*p[4]+p[5]*p[5];if(n<.99f||n>1.01f)return false;
 }
 const wchar_t* names[]={L"FlashlightAlbedo.png",L"FlashlightNormal.png",L"FlashlightSurface.png"};
 for(unsigned i=0;i<3;++i)if(!psxAssets::read(folder/names[i],png[i],32000000))return false;
 section=unsigned(pack.sections.size());pack.sections.push_back({7,psxAssets::Weapon,unsigned(pack.vertices.size()),h[3]});
 glassSection=unsigned(pack.sections.size());pack.sections.push_back({7,psxAssets::Weapon,unsigned(pack.vertices.size())+h[3],h[2]-h[3]});
 pack.vertices.insert(pack.vertices.end(),v.begin(),v.end());return ready=true;
}
inline bool create(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12DescriptorHeap* descriptors){
 for(unsigned i=0;i<3;++i){
  UINT width=1,height=1;std::vector<unsigned char> pixels=i?std::vector<unsigned char>{255,255,0,0}:std::vector<unsigned char>{128,128,255,0};
  if(ready){
   HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);struct Scope{bool own;~Scope(){if(own)CoUninitialize();}} scope{SUCCEEDED(com)};
   ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
   if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromMemory(png[i].data(),DWORD(png[i].size())))||FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||FAILED(converter->GetSize(&width,&height))||width!=4096||height!=4096)return false;
   pixels.resize(size_t(width)*height*4);if(FAILED(converter->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data())))return false;
  }
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC tex{};tex.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;tex.Width=width;tex.Height=height;tex.DepthOrArraySize=tex.MipLevels=1;tex.Format=DXGI_FORMAT_R8G8B8A8_UNORM;tex.SampleDesc.Count=1;
  if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&tex,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&textures[i]))))return false;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 size{};device->GetCopyableFootprints(&tex,0,1,0,&fp,nullptr,nullptr,&size);
  D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=size;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;heap.Type=D3D12_HEAP_TYPE_UPLOAD;
  if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&uploads[i]))))return false;
  void* dst{};D3D12_RANGE none{};if(FAILED(uploads[i]->Map(0,&none,&dst)))return false;
  for(unsigned y=0;y<height;++y)std::memcpy(static_cast<unsigned char*>(dst)+fp.Offset+y*fp.Footprint.RowPitch,pixels.data()+size_t(y)*width*4,width*4);uploads[i]->Unmap(0,nullptr);
  D3D12_TEXTURE_COPY_LOCATION source{},target{};source.pResource=uploads[i].Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=fp;target.pResource=textures[i].Get();target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&target,0,0,0,&source,nullptr);
  D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={textures[i].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&barrier);
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=tex.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
  auto handle=descriptors->GetCPUDescriptorHandleForHeapStart();handle.ptr+=(1+i)*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(textures[i].Get(),&srv,handle);
 }
 return true;
}
inline bool initialize(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12Resource* atlas){
 if(!ready)return true;
 if(!atlas)return false;
 D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;d.NumDescriptors=4;d.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 if(FAILED(device->CreateDescriptorHeap(&d,IID_PPV_ARGS(&heap))))return false;
 // Shader-visible heaps are not valid descriptor copy sources. Recreate the
 // same one-mip atlas view used by createAlbedo directly in the destination.
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=atlas->GetDesc().Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
 device->CreateShaderResourceView(atlas,&srv,heap->GetCPUDescriptorHandleForHeapStart());
 return create(device,list,heap.Get());
}
}
