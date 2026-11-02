#define NOMINMAX
#include "../src/texture_upload_metadata.hpp"
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("upload metadata eligibility check failed");}
int main(){try{unsigned checks{};
    for(bool vignette:{false,true}){
        D3D12_RESOURCE_DESC image{},buffer{};image.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;image.Width=vignette?128:1;image.Height=vignette?64:1;image.DepthOrArraySize=image.MipLevels=1;image.SampleDesc.Count=1;image.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=vignette?33280:516;
        require(textureUploadMetadata::compatibleWholeCopy(image,image));++checks;
        for(unsigned mutation=0;mutation<12;++mutation){auto changed=image;
            switch(mutation){case 0:changed.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;break;case 1:changed.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;break;case 2:changed.Width++;break;case 3:changed.Height++;break;case 4:changed.DepthOrArraySize=2;break;case 5:changed.MipLevels=2;break;case 6:changed.SampleDesc.Count=2;break;case 7:changed.SampleDesc.Quality=1;break;case 8:changed.Format=DXGI_FORMAT_UNKNOWN;break;case 9:changed.Width=0;break;case 10:changed.Height=0;break;case 11:changed.MipLevels=0;break;}
            require(!textureUploadMetadata::compatibleWholeCopy(image,changed)&&!textureUploadMetadata::compatibleWholeCopy(changed,image));checks+=2;
        }
        D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=reinterpret_cast<ID3D12Resource*>(1);destination.pResource=reinterpret_cast<ID3D12Resource*>(2);source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.PlacedFootprint.Offset=512;source.PlacedFootprint.Footprint={image.Format,UINT(image.Width),image.Height,1,vignette?512u:256u};D3D12_BOX full{0,0,0,UINT(image.Width),image.Height,1};
        require(textureUploadMetadata::eligible(image,buffer,destination,0,0,0,source,nullptr));require(textureUploadMetadata::eligible(image,buffer,destination,0,0,0,source,&full));checks+=2;
        auto mipImage=image; mipImage.MipLevels=vignette?8:1;
        require(textureUploadMetadata::eligible(mipImage,buffer,destination,0,0,0,source,nullptr));
        require(textureUploadMetadata::compatibleWholeCopy(mipImage,mipImage));
        auto bgra=image; bgra.Format=DXGI_FORMAT_B8G8R8A8_UNORM; auto bgraSource=source;bgraSource.PlacedFootprint.Footprint.Format=bgra.Format;
        require(textureUploadMetadata::eligible(bgra,buffer,destination,0,0,0,bgraSource,nullptr));
        require(textureUploadMetadata::compatibleWholeCopy(bgra,bgra)); checks+=4;
        for(unsigned mutation=0;mutation<26;++mutation){auto i=image,b=buffer;auto s=source,d=destination;auto box=full;UINT x{},y{},z{};
            switch(mutation){case 0:x=1;break;case 1:y=1;break;case 2:z=1;break;case 3:d.SubresourceIndex=1;break;case 4:d.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;break;case 5:s.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;break;case 6:s.pResource=nullptr;break;case 7:d.pResource=nullptr;break;case 8:s.pResource=d.pResource;break;case 9:i.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;break;case 10:i.MipLevels=9;break;case 11:i.DepthOrArraySize=2;break;case 12:i.SampleDesc.Count=2;break;case 13:i.SampleDesc.Quality=1;break;case 14:i.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;break;case 15:i.Width=17;break;case 16:b.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;break;case 17:s.PlacedFootprint.Footprint.Width++;break;case 18:s.PlacedFootprint.Footprint.Height++;break;case 19:s.PlacedFootprint.Footprint.Depth=2;break;case 20:s.PlacedFootprint.Footprint.RowPitch=255;break;case 21:s.PlacedFootprint.Offset=1;break;case 22:--b.Width;break;case 23:s.PlacedFootprint.Offset=UINT64_MAX-511;break;case 24:box.left=1;break;case 25:box.back=0;break;}
            require(!textureUploadMetadata::eligible(i,b,d,x,y,z,s,&box));++checks;
        }
    }
    std::printf("PASS: %u upload/full-copy metadata checks including exact final-row buffer bounds, crop/origin, format/subresource, pitch/offset alignment and overflow rejection; no COM/GPU/game/XR.\n",checks);return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}

