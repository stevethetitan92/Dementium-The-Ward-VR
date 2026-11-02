#pragma once
#include <cstdio>
#include <set>
namespace nativeUiAudit {
inline std::mutex lock;
inline const wchar_t* directory(){static wchar_t path[32768]{};static bool once=[](){GetEnvironmentVariableW(L"DEMENTIUM_UI_AUDIT",path,32768);return true;}();(void)once;return path;}
inline FILE* file(const wchar_t* name){if(!*directory())return nullptr;std::wstring path=std::wstring(directory())+L"/"+name;FILE* out{};_wfopen_s(&out,path.c_str(),L"ab");return out;}
inline void draw(ID3D12GraphicsCommandList* list,UINT count,UINT instances,UINT first,INT base,bool indexed,UINT instance){
 if(!*directory()||instances!=1||!count||count>256)return;
 auto& s=stereo::lists[list];stereo::PipelineInfo info;
 {std::lock_guard<std::mutex> guard(stereo::mapMutex);auto p=stereo::pipelineInfo.find(s.pipeline);if(p==stereo::pipelineInfo.end())return;info=p->second;}
 if(!info.sprite||!info.layout.valid)return;
 std::vector<float> values;values.reserve(count*6);
 for(UINT i=0;i<count;++i){UINT vertex=first+i;if(indexed){UINT bytes=s.indices.Format==DXGI_FORMAT_R16_UINT?2:s.indices.Format==DXGI_FORMAT_R32_UINT?4:0;UINT index=0;if(!bytes||UINT64(vertex+1)*bytes>s.indices.SizeInBytes||!stereo::copyConstants(s.indices.BufferLocation+UINT64(vertex)*bytes,bytes,reinterpret_cast<unsigned char*>(&index))||INT64(index)+base<0)return;vertex=UINT(INT64(index)+base);}
  float p[4],uv[4];if(!stereo::spriteAttribute(s,info.layout.position,vertex,instance,p)||!stereo::spriteAttribute(s,info.layout.uv,vertex,instance,uv))return;
  values.insert(values.end(),p,p+4);values.insert(values.end(),uv,uv+2);
 }
 void* texture=nullptr;unsigned width=0,height=0;
 {std::lock_guard<std::mutex> guard(stereo::mapMutex);for(auto& h:s.heaps){auto gpu=s.textures[0].ptr;if(gpu<h.gpu||!h.stride)continue;auto delta=gpu-h.gpu;if(delta%h.stride||delta>=UINT64(h.count)*h.stride)continue;auto cpu=h.cpu+SIZE_T(delta);auto m=stereo::sampledViews.find(cpu);auto r=stereo::sampledResources.find(cpu);if(m!=stereo::sampledViews.end()){width=unsigned(m->second.resource.Width);height=m->second.resource.Height;}if(r!=stereo::sampledResources.end())texture=r->second;break;}}
 std::uint64_t hash=1469598103934665603ull;for(UINT i=0;i<count;i++)for(unsigned j=0;j<8;j++){hash^=reinterpret_cast<unsigned char*>(values.data()+i*6+4)[j];hash*=1099511628211ull;}hash^=reinterpret_cast<std::uintptr_t>(texture);
 float bounds[4],color[4];if(!stereo::spriteBounds(s,info.layout,count,first,base,indexed,instance,bounds,color))return;
 // A shared white texel is reused for many panels. UV-only deduplication
 // hid their different locations and colors, including the HUD backgrounds.
 for(float v:bounds){int q=int(std::round(v*1000));hash^=unsigned(q);hash*=1099511628211ull;}
 for(float v:color){int q=int(std::round(v*255));hash^=unsigned(q);hash*=1099511628211ull;}
 std::lock_guard<std::mutex> guard(lock);static std::set<std::uint64_t> seen;if(seen.size()>=2048||!seen.insert(hash).second)return;
 auto out=file(L"sprites.txt");if(!out)return;fprintf(out,"DRAW %llu count=%u bounds=%g,%g,%g,%g texture=%p size=%ux%u\n",hash,count,bounds[0],bounds[1],bounds[2],bounds[3],texture,width,height);fprintf(out,"COLOR %g %g %g %g textured=%d\n",color[0],color[1],color[2],color[3],info.usesTexture);
 for(UINT i=0;i<count;++i)fprintf(out,"%g %g %g %g uv %g %g\n",values[i*6],values[i*6+1],values[i*6+2],values[i*6+3],values[i*6+4],values[i*6+5]);fclose(out);
}
inline void upload(const D3D12_TEXTURE_COPY_LOCATION& dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION& src,const D3D12_BOX* box){
 if(!*directory()||x||y||z||box||dst.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||dst.SubresourceIndex||src.Type!=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)return;
 auto d=dst.pResource->GetDesc();auto b=src.pResource->GetDesc();auto f=src.PlacedFootprint;
 if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||d.Width>512||d.Height>512||!d.Width||!d.Height||b.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||f.Footprint.Width!=d.Width||f.Footprint.Height!=d.Height||f.Footprint.RowPitch<d.Width*4||f.Offset>b.Width||UINT64(d.Height-1)*f.Footprint.RowPitch+d.Width*4>b.Width-f.Offset)return;
 std::lock_guard<std::mutex> guard(lock);static std::set<void*> seen;if(seen.size()>=512||seen.count(dst.pResource))return;
 std::vector<unsigned char> pixels(size_t(d.Width)*d.Height*4);for(UINT row=0;row<d.Height;row++)if(!stereo::copyConstants(src.pResource->GetGPUVirtualAddress()+f.Offset+UINT64(row)*f.Footprint.RowPitch,unsigned(d.Width)*4,pixels.data()+row*d.Width*4))return;
 wchar_t name[100];swprintf_s(name,L"atlas-%p-%ux%u.rgba",dst.pResource,unsigned(d.Width),d.Height);auto out=file(name);if(out){fwrite(pixels.data(),1,pixels.size(),out);fclose(out);seen.insert(dst.pResource);}
}

}
