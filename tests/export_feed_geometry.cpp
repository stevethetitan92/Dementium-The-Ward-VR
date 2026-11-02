#include "../src/pistol_rounds.hpp"
#include "../src/rifle_rounds.hpp"
#include "../src/selected_pistol_well.hpp"
#include "../src/selected_pistol_feed_channel.hpp"
#include "../src/selected_pistol_passage.hpp"
#include <cassert>
#include <fstream>
#include <string>
// Offline evidence exporter: actual runtime loaders, fitted vertices and feed transforms.
int main(int argc,char** argv){
 assert(argc==3||argc==4);psxAssets::Pack p;std::filesystem::path folder(argv[1]),out(argv[2]);
 const bool passage=argc==4&&std::string(argv[3])=="--passage";
 const bool feedChannel=argc==4; if(feedChannel)assert(passage||std::string(argv[3])=="--feed-channel");
 assert(psxAssets::load(folder,p)&&selectedFrames::load(folder));
 assert(selectedReload::loadProfile(folder,p)&&selectedRifle::load(folder,p));
 assert(selectedRifle::fitMagazine(folder,p));
 assert(selectedPistolWell::load(folder,p));
 if(feedChannel)assert(selectedPistolFeedChannel::load(folder,p)&&selectedPistolFeedChannel::applied);
 assert(pistolRounds::load(folder));pistolRounds::append(p);assert(pistolRounds::feedReady);
 assert(rifleRounds::append(p));
 if(passage)assert(selectedPistolPassage::load(folder,p)&&selectedPistolPassage::applied);
 for(unsigned w:{1u,2u}){
  std::ofstream f(out/("feed-geometry-"+std::to_string(w)+".bin"),std::ios::binary);
  auto write=[&](const auto& value){f.write(reinterpret_cast<const char*>(&value),sizeof value);};
  auto frame=DirectX::XMLoadFloat4x4(&selectedFrames::frames[w].editorToAim);
  unsigned first=w==1?pistolRounds::first:rifleRounds::first;write(first);
  auto offset=w==1?pistolRounds::feedOffset:rifleRounds::feedOffset;write(offset);
  auto axis=w==1?selectedReload::profile.geometry.pullAxisGrip:selectedRifle::profile.geometry.pullAxisGrip;write(axis);
  for(unsigned i=0;i<p.sections.size();++i){const auto s=p.sections[i];
   if(s.weapon!=w||s.kind!=psxAssets::Weapon)continue;
   if((pistolRounds::contains(i)||rifleRounds::contains(i))&&i!=first)continue;
   if(w==2&&i==selectedRifle::profile.sections[1])continue;
   unsigned slide=w==1?selectedReload::profile.role(i)==selectedReload::Role::Slide:(i==selectedRifle::boltSection||selectedRifle::profile.role(i)==selectedReload::Role::Slide);
   write(i);write(slide);write(s.count);
   for(unsigned j=0;j<s.count;++j){auto v=p.vertices[s.first+j].value;DirectX::XMFLOAT3 q;
    DirectX::XMStoreFloat3(&q,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(v[0],v[1],v[2],1),frame));write(q);}
  }
  assert(f.good());
 }
}
