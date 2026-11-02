#include "../src/selected_pistol_passage.hpp"
#include "../src/selected_pistol_well.hpp"
#include "../src/pistol_rounds.hpp"
#include "../src/magazine_exit.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){assert(argc==2);std::filesystem::path folder=argv[1];psxAssets::Pack p;
 assert(psxAssets::load(folder,p)&&selectedFrames::load(folder)&&selectedReload::loadProfile(folder,p));
 assert(pistolRounds::load(folder));pistolRounds::append(p);assert(pistolRounds::feedReady);
 assert(selectedPistolWell::load(folder,p));assert(magazineExit::bind(p,1,77));
 const auto rail=magazineExit::travel[1],axis=manualReload::magazineExitAxes[1];const float clearance=manualReload::magazineEntryClearance[1];const auto feed=pistolRounds::feedOffset;
 auto original=p;std::vector<unsigned char>b,g,f;assert(psxAssets::read(folder/L"SelectedPistolPassage.bin",b,600000));assert(psxAssets::read(folder/L"SelectedGeometry.psx",g,64000000));assert(psxAssets::read(folder/L"SelectedFrames.bin",f,4096));
 std::vector<selectedPistolPassage::Replacement> out;assert(selectedPistolPassage::decode(b,g,f,p,out)&&out.size()==6);
 auto bad=b;bad.back()^=1;assert(!selectedPistolPassage::decode(bad,g,f,p,out));bad=b;bad.pop_back();assert(!selectedPistolPassage::decode(bad,g,f,p,out));
 auto wrong=g;wrong.back()^=1;assert(!selectedPistolPassage::decode(b,wrong,f,p,out));wrong=f;wrong.back()^=1;assert(!selectedPistolPassage::decode(b,g,wrong,p,out));
 auto wrongPack=p;wrongPack.sections[77].count-=3;assert(!selectedPistolPassage::decode(b,g,f,wrongPack,out));
 assert(selectedPistolPassage::load(folder,p)&&selectedPistolPassage::applied);
 assert(p.sections.size()==original.sections.size()&&std::memcmp(p.vertices.data(),original.vertices.data(),original.vertices.size()*sizeof(psxAssets::Vertex))==0);
 for(unsigned sid=0;sid<p.sections.size();++sid){bool changed=false;for(const auto& r:out)if(r.section==sid){changed=true;assert(p.sections[sid].count==r.vertices.size());assert(std::memcmp(p.vertices.data()+p.sections[sid].first,r.vertices.data(),r.vertices.size()*sizeof(psxAssets::Vertex))==0);}
  if(!changed)assert(std::memcmp(&p.sections[sid],&original.sections[sid],sizeof(psxAssets::Section))==0);
 }
 assert(std::memcmp(&rail,&magazineExit::travel[1],sizeof rail)==0&&std::memcmp(&axis,&manualReload::magazineExitAxes[1],sizeof axis)==0&&clearance==manualReload::magazineEntryClearance[1]&&std::memcmp(&feed,&pistolRounds::feedOffset,sizeof feed)==0);
 auto count=p.vertices.size();assert(!selectedPistolPassage::load(folder,p)&&p.vertices.size()==count);
 puts("PASS: actual package, atomic rejection, exact payload, original vertices/other sections preserved, rail/feed calibration preserved, duplicate rejected");
}
