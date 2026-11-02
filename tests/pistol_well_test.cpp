#include "../src/selected_pistol_well.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){assert(argc==2);std::filesystem::path folder=argv[1];psxAssets::Pack p;
 assert(psxAssets::load(folder,p));auto original=p;
 std::vector<unsigned char>b,g,f;assert(psxAssets::read(folder/L"SelectedPistolWell.bin",b,200000));assert(psxAssets::read(folder/L"SelectedGeometry.psx",g,64000000));assert(psxAssets::read(folder/L"SelectedFrames.bin",f,4096));
 std::vector<psxAssets::Vertex> out;assert(selectedPistolWell::decode(b,g,f,p,out)&&out.size()==3387);
 auto bad=b;bad.back()^=1;assert(!selectedPistolWell::decode(bad,g,f,p,out));bad=b;bad.pop_back();assert(!selectedPistolWell::decode(bad,g,f,p,out));
 auto wrong=g;wrong.back()^=1;assert(!selectedPistolWell::decode(b,wrong,f,p,out));wrong=f;wrong.back()^=1;assert(!selectedPistolWell::decode(b,g,wrong,p,out));
 assert(selectedPistolWell::load(folder,p)&&selectedPistolWell::applied);
 assert(p.sections.size()==original.sections.size());
 assert(std::memcmp(p.vertices.data(),original.vertices.data(),original.vertices.size()*sizeof(psxAssets::Vertex))==0);
 for(unsigned i=0;i<p.sections.size();++i)if(i!=59)assert(std::memcmp(&p.sections[i],&original.sections[i],sizeof(psxAssets::Section))==0);
 assert(p.sections[59].first==original.vertices.size()&&p.sections[59].count==3387);
 for(unsigned i=0;i<3387;++i)assert(std::memcmp(&p.vertices[p.sections[59].first+i],b.data()+32+i*sizeof(psxAssets::Vertex),sizeof(psxAssets::Vertex))==0);
 auto count=p.vertices.size();assert(!selectedPistolWell::load(folder,p));assert(p.vertices.size()==count);
 puts("PASS: exact audited cutout, source/frame/payload rejection, original vertices and other bindings unchanged, no duplicate application");
}
