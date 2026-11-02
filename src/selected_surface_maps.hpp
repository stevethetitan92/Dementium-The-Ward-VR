#pragma once
#include "selected_reload_profile.hpp"
namespace selectedSurface {
using Microsoft::WRL::ComPtr;
inline std::array<std::vector<unsigned char>,2> png;
inline std::array<ComPtr<ID3D12Resource>,2> textures,uploads;
inline bool bound{};
inline void load(const std::filesystem::path& folder,const psxAssets::Pack& pack){
 bound=false;for(auto& p:png)p.clear();std::vector<unsigned char> b,g;
 if(!psxAssets::read(folder/L"SelectedSurfaceBinding.bin",b,24)||b.size()!=24||!psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024))return;
 unsigned h[2];std::uint64_t hashes[2];std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);
 if(h[0]!=0x4d585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(pack.texture))return;
 const wchar_t* names[]={L"SelectedNormal.png",L"SelectedSurface.png"};
 for(unsigned i=0;i<2;++i)if(!psxAssets::read(folder/names[i],png[i],64*1024*1024))return;
 bound=true;
}
inline bool create(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12DescriptorHeap* descriptors){
 for(unsigned i=0;i<2;++i){
  UINT width=1,height=1;std::vector<unsigned char> pixels=i?std::vector<unsigned char>{255,255,0,0}:std::vector<unsigned char>{128,128,255,0};
  if(bound){
   HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);struct Scope{bool own;~Scope(){if(own)CoUninitialize();}} scope{SUCCEEDED(com)};
   ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
   if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromMemory(png[i].data(),DWORD(png[i].size())))||FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||FAILED(converter->GetSize(&width,&height))||width!=8192||height!=8192)return false;
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
  auto handle=descriptors->GetCPUDescriptorHandleForHeapStart();handle.ptr+=(2+i)*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(textures[i].Get(),&srv,handle);
 }
 return true;
}
}
