#include "../src/selected_shotgun_contact.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("shotgun contact failed");}
int main(int argc,char**argv){try{
 check(argc==3);std::filesystem::path folder=argv[1];std::vector<unsigned char>b,g,f;
 check(psxAssets::read(argv[2],b,60));check(psxAssets::read(folder/"SelectedGeometry.psx",g,64*1024*1024));check(psxAssets::read(folder/"SelectedFrames.bin",f,4096));
 selectedShotgun::Contact p;check(selectedShotgun::decodeContact(b,g,f,p));check(p.insertion.ready);
 for(unsigned offset:{0u,4u,8u,16u}){auto bad=b;bad[offset]^=1;selectedShotgun::Contact rejected;check(!selectedShotgun::decodeContact(bad,g,f,rejected));}
 auto bad=b;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+24,&nan,4);selectedShotgun::Contact rejected;check(!selectedShotgun::decodeContact(bad,g,f,rejected));
 XrPosef hand{};hand.orientation={0,0,1,0};hand.position={.1f,.2f,.3f};auto result=p.insertion.contact(hand);
 check(std::fabs(result.x-(.1f-p.insertion.held.x))<1e-6f);check(std::fabs(result.y-(.2f-p.insertion.held.y))<1e-6f);check(std::fabs(result.z-(.3f+p.insertion.held.z))<1e-6f);
 puts("PASS: actual shotgun contact, fingerprint/malformed rejection and rotated finger offset");return 0;
 }catch(const std::exception&e){puts(e.what());return 1;}}
