#include "../src/selected_support_restore.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){using namespace DirectX;assert(argc==3);
 std::filesystem::path folder=argv[1];psxAssets::Pack p;assert(psxAssets::load(folder,p));assert(selectedFrames::load(folder));
 auto before=p.vertices;auto oldFrame=selectedFrames::frames[3];
 std::vector<unsigned char>b,g,f,saved;assert(psxAssets::read(folder/L"SelectedSupportRestore.bin",b,1024*1024));
 assert(psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024));assert(psxAssets::read(folder/L"SelectedFrames.bin",f,4096));
 assert(psxAssets::read(argv[2],saved,1024*1024));
 auto bad=b;bad[8]^=1;assert(!selectedSupportRestore::decode(bad,g,f,p));
 assert(!std::memcmp(p.vertices.data(),before.data(),before.size()*sizeof(before[0])));
 assert(selectedSupportRestore::decode(b,g,f,p));unsigned id;std::memcpy(&id,b.data()+24,4);const auto section=p.sections[id];
 for(unsigned i=0;i<p.vertices.size();++i){if(i<section.first||i>=section.first+section.count){assert(!std::memcmp(&p.vertices[i],&before[i],sizeof(before[i])));continue;}
   assert(!std::memcmp(p.vertices[i].value,saved.data()+4+(i-section.first)*48,12));
   assert(!std::memcmp(p.vertices[i].value+3,before[i].value+3,36));
   auto a=XMVector3TransformCoord(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(before[i].value)),XMLoadFloat4x4(&oldFrame.editorToFree));
   auto c=XMVector3TransformCoord(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(p.vertices[i].value)),XMLoadFloat4x4(&selectedFrames::frames[3].editorToFree));
   assert(XMVectorGetX(XMVector3Length(a-c))<1e-6f);
 }
 puts("PASS: exact saved shotgun support positions restored; free-hand pose, UV/materials and all other sections preserved; wrong package rejected");
}
