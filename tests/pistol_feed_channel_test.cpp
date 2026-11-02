#include "../src/selected_pistol_feed_channel.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){assert(argc==2);std::filesystem::path folder=argv[1];psxAssets::Pack p;
 assert(psxAssets::load(folder,p));auto original=p;
 std::vector<unsigned char>b,g,f;assert(psxAssets::read(folder/L"SelectedPistolFeedChannel.bin",b,300000));assert(psxAssets::read(folder/L"SelectedGeometry.psx",g,64000000));assert(psxAssets::read(folder/L"SelectedFrames.bin",f,4096));
 std::vector<psxAssets::Vertex> out;assert(selectedPistolFeedChannel::decode(b,g,f,p,out)&&out.size()==4923);
 auto bad=b;bad.back()^=1;assert(!selectedPistolFeedChannel::decode(bad,g,f,p,out));bad=b;bad.pop_back();assert(!selectedPistolFeedChannel::decode(bad,g,f,p,out));
 auto wrong=g;wrong.back()^=1;assert(!selectedPistolFeedChannel::decode(b,wrong,f,p,out));wrong=f;wrong.back()^=1;assert(!selectedPistolFeedChannel::decode(b,g,wrong,p,out));
 assert(selectedPistolFeedChannel::load(folder,p)&&selectedPistolFeedChannel::applied);
 assert(p.sections.size()==original.sections.size());
 assert(std::memcmp(p.vertices.data(),original.vertices.data(),original.vertices.size()*sizeof(psxAssets::Vertex))==0);
 for(unsigned i=0;i<p.sections.size();++i)if(i!=60)assert(std::memcmp(&p.sections[i],&original.sections[i],sizeof(psxAssets::Section))==0);
 assert(p.sections[60].first==original.vertices.size()&&p.sections[60].count==4923);
 for(unsigned i=0;i<4923;++i)assert(std::memcmp(&p.vertices[p.sections[60].first+i],b.data()+32+i*sizeof(psxAssets::Vertex),sizeof(psxAssets::Vertex))==0);
 auto count=p.vertices.size();assert(!selectedPistolFeedChannel::load(folder,p));assert(p.vertices.size()==count);
 puts("PASS: exact audited cutout, source/frame/payload rejection, original vertices and other bindings unchanged, no duplicate application");
}

