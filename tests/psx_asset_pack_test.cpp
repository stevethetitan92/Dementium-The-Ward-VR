#include "../src/psx_asset_pack.hpp"
#include <iostream>
#include <limits>

int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::vector<unsigned char> data,texture;
    const std::filesystem::path folder=argv[1];
    if(!psxAssets::read(folder/"SelectedGeometry.psx",data,64*1024*1024)||
       !psxAssets::read(folder/"SelectedGeometry.png",texture,64*1024*1024))return 3;
    psxAssets::Pack pack;
    if(!psxAssets::decode(data,texture,pack))return 4;
    const auto expected=pack.vertices.size();unsigned rejected=0;
    const size_t headerBytes=pack.version==3?52:24;
    auto reject=[&](std::vector<unsigned char> damaged,std::vector<unsigned char> image){
        if(psxAssets::decode(damaged,image,pack)||pack.vertices.size()!=expected)std::exit(5);
        ++rejected;
    };
    auto changed=data;changed.pop_back();reject(changed,texture);
    changed=data;changed.push_back(0);reject(changed,texture);
    changed=data;changed[0]=0;reject(changed,texture);
    changed=data;changed[8]=0;reject(changed,texture); // Wrong coordinate frame.
    changed=data;changed[headerBytes]=7;reject(changed,texture); // Unknown weapon.
    changed=data;changed[headerBytes+4]=3;reject(changed,texture); // Unknown role.
    changed=data;changed[headerBytes+8]=1;reject(changed,texture); // Gap/overlap.
    changed=data;changed[headerBytes+12]=1;changed[headerBytes+13]=0;reject(changed,texture); // Incomplete triangle.
    changed=data;float nan=std::numeric_limits<float>::quiet_NaN();
    std::memcpy(changed.data()+headerBytes+pack.sections.size()*16,&nan,4);reject(changed,texture);
    auto image=texture;image[16]=1;reject(data,image); // Wrong atlas size.
    image=texture;image[0]=0;reject(data,image);
    // A failed asset update must leave the last valid pack untouched.
    std::cout<<"PASS: "<<expected<<" vertices, "<<pack.sections.size()<<" sections; "<<rejected<<" malformed packs rejected transactionally\n";
}


