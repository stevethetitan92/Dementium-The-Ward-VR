#include "../src/selected_hunting_contact.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("hunting magazine failed");}
int main(int argc,char**argv){try{
 if(argc!=2)return 2;
 psxAssets::Pack pack;check(psxAssets::load(argv[1],pack));
 auto contact=selectedHunting::load(argv[1],pack);check(contact.ready&&selectedHunting::section==1);
 std::vector<unsigned char>b,g,f;auto folder=std::filesystem::path(argv[1]);
 check(psxAssets::read(folder/L"SelectedHuntingContact.bin",b,64));b.resize(48);
 check(psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024));check(psxAssets::read(folder/L"SelectedFrames.bin",f,4096));
 for(unsigned offset:{0u,4u,8u,16u}){auto bad=b;bad[offset]^=1;arsenal::RevolverContact out;check(!selectedHunting::decode(bad,g,f,out)&&!out.ready);}
 float badValue=std::numeric_limits<float>::quiet_NaN();auto bad=b;std::memcpy(bad.data()+24,&badValue,4);arsenal::RevolverContact out;check(!selectedHunting::decode(bad,g,f,out));
 manualReload::Visual visual;visual.valid=true;visual.leftInGun={{0,0,0,1},{.2f,.1f,0}};
 XrPosef gun{{0,0,0,1},{}};DirectX::XMMATRIX result;
 visual.stage=manualReload::Stage::EmptyWell;check(!selectedReload::transform(selectedReload::Role::Magazine,selectedHunting::geometry,visual,DirectX::XMMatrixIdentity(),gun,result));
 visual.stage=manualReload::Stage::MagazineHeld;check(selectedReload::transform(selectedReload::Role::Magazine,selectedHunting::geometry,visual,DirectX::XMMatrixIdentity(),gun,result));
 DirectX::XMFLOAT3 center;auto c=selectedHunting::geometry.magazineContactEditor;
 DirectX::XMStoreFloat3(&center,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(c.x,c.y,c.z,1),result));
 check(manualReload::distance({center.x,center.y,center.z},contact.contact(visual.leftInGun))<1e-5f);
 puts("PASS: actual hunting magazine binding, identity/NaN rejection, hide and held contact");return 0;
}catch(const std::exception&e){puts(e.what());return 1;}}
