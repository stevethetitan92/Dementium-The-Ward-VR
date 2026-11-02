#pragma once
#include <d3d12.h>
namespace textureUploadMetadata {
inline bool supportedFormat(DXGI_FORMAT format){return format==DXGI_FORMAT_R8G8B8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM;}
inline bool supportedMips(const D3D12_RESOURCE_DESC& d){return d.MipLevels>=1&&d.MipLevels<=((d.Width==128&&d.Height==64)?8:1);}
inline bool compatibleWholeCopy(const D3D12_RESOURCE_DESC& a,const D3D12_RESOURCE_DESC& b){
    return a.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&b.Dimension==a.Dimension&&supportedFormat(a.Format)&&b.Format==a.Format&&
        ((a.Width==1&&a.Height==1)||(a.Width==128&&a.Height==64))&&b.Width==a.Width&&b.Height==a.Height&&a.DepthOrArraySize==1&&b.DepthOrArraySize==1&&supportedMips(a)&&b.MipLevels==a.MipLevels&&a.SampleDesc.Count==1&&b.SampleDesc.Count==1&&!a.SampleDesc.Quality&&!b.SampleDesc.Quality;
}
// Content shortcuts require a proven full upload of the exact simple texture.
// Other valid native copies still forward, but cannot establish these tags.
inline bool eligible(const D3D12_RESOURCE_DESC& image,const D3D12_RESOURCE_DESC& buffer,const D3D12_TEXTURE_COPY_LOCATION& destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION& source,const D3D12_BOX* box){
    if(x||y||z||destination.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||destination.SubresourceIndex||
       source.Type!=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT||!source.pResource||!destination.pResource||source.pResource==destination.pResource||
       image.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||image.DepthOrArraySize!=1||!supportedMips(image)||image.SampleDesc.Count!=1||image.SampleDesc.Quality||!supportedFormat(image.Format)||
       !((image.Width==1&&image.Height==1)||(image.Width==128&&image.Height==64))||buffer.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER)return false;
    const auto& placed=source.PlacedFootprint;const auto& footprint=placed.Footprint;
    if(footprint.Format!=image.Format||footprint.Width!=image.Width||footprint.Height!=image.Height||footprint.Depth!=1||footprint.RowPitch<image.Width*4||footprint.RowPitch%D3D12_TEXTURE_DATA_PITCH_ALIGNMENT||placed.Offset%D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT||placed.Offset>buffer.Width)return false;
    const UINT64 bytes=UINT64(footprint.Height-1)*footprint.RowPitch+image.Width*4;
    if(bytes>buffer.Width-placed.Offset)return false;
    return !box||(!box->left&&!box->top&&!box->front&&box->right==image.Width&&box->bottom==image.Height&&box->back==1);
}
}
