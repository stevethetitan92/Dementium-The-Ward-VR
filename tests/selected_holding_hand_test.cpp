#include "../src/selected_holding_hand.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool x){if(!x)throw std::runtime_error("holding hand failed");}
int main(int argc,char**argv){try{
 check(argc==3);psxAssets::Pack pack;check(psxAssets::load(argv[1],pack));
 std::vector<unsigned char>b,g,f;check(psxAssets::read(argv[2],b,2000000));check(psxAssets::read(std::filesystem::path(argv[1])/"SelectedGeometry.psx",g,64000000));check(psxAssets::read(std::filesystem::path(argv[1])/"SelectedFrames.bin",f,4096));
 std::vector<psxAssets::Vertex> out;check(selectedHoldingHand::decode(b,g,f,pack,out));
 auto reject=[&](std::vector<unsigned char> bad){std::vector<psxAssets::Vertex> untouched;check(!selectedHoldingHand::decode(bad,g,f,pack,untouched)&&untouched.empty());};
 for(unsigned offset:{0u,4u,8u,16u,24u}){auto bad=b;bad[offset]^=1;reject(bad);}
 auto bad=b;bad.pop_back();reject(bad);bad=b;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+28,&nan,4);reject(bad);
 bad=b;bad[28+9*4]^=1;reject(bad);
 bool cap=false;for(unsigned i=0;i<out.size();++i)if(out[i].value[11]==4){bad=b;bad[28+i*48]^=1;reject(bad);cap=true;break;}check(cap);
 auto original=pack.vertices;auto count=pack.sections.size();
 check(selectedHoldingHand::load(argv[1],pack));
 check(selectedHoldingHand::section==count);
 if(std::filesystem::exists(std::filesystem::path(argv[1])/L"SelectedRelaxedHand.bin")){
  check(selectedHoldingHand::relaxedSection==count+1);
  check(pack.sections[count].count==pack.sections[count+1].count);
 }
 check(!std::memcmp(original.data(),pack.vertices.data(),original.size()*sizeof(psxAssets::Vertex)));
 if(std::filesystem::exists(std::filesystem::path(argv[1])/L"SelectedFlashlightHand.bin")){
  const auto a=selectedHoldingHand::flashlightSection,b=selectedHoldingHand::flashlightPressSection;
  check(a<pack.sections.size()&&b<pack.sections.size()&&a!=b);
  const auto& rest=pack.sections[a];const auto& pressed=pack.sections[b];check(rest.count==pressed.count);
  unsigned moved=0;for(unsigned i=0;i<rest.count;++i){const auto* r=pack.vertices[rest.first+i].value;const auto* p=pack.vertices[pressed.first+i].value;
   check(!std::memcmp(r+6,p+6,6*sizeof(float)));if(std::memcmp(r,p,3*sizeof(float)))++moved;
   if(r[11]==4)check(!std::memcmp(r,p,12*sizeof(float)));}
  check(moved>0&&moved<rest.count/2);
 }
 selectedHoldingHand::section=99;for(int weapon=0;weapon<7;++weapon)for(bool valid:{false,true})for(auto stage:{manualReload::Stage::Ready,manualReload::Stage::EmptyWell,manualReload::Stage::MagazineHeld,manualReload::Stage::Inserted,manualReload::Stage::SlideHeld})check(selectedHoldingHand::use(weapon,valid,stage)==(weapon==1&&valid&&stage==manualReload::Stage::MagazineHeld));
 puts("PASS: actual holding mesh, cap/UV preservation, malformed rejection and pistol-held-only gate");return 0;
 }catch(const std::exception&e){puts(e.what());return 1;}}
