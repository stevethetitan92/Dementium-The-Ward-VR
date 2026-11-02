#include "../src/selected_melee_contact.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
    using namespace DirectX;
    psxAssets::Pack fixture;
    for(float y:{0.f,.25f,.5f})for(float x:{-.01f,.01f}){
        psxAssets::Vertex v{};v.value[0]=x;v.value[1]=y;fixture.vertices.push_back(v);
    }
    fixture.sections.push_back({6,psxAssets::Weapon,0,6});
    selectedFrames::ready=true;XMStoreFloat4x4(&selectedFrames::frames[6].editorToAim,XMMatrixIdentity());
    assert(selectedMeleeContact::load(fixture));
    XrPosef p{{0,0,.70710678f,.70710678f},{1,2,3}};XrVector3f a,b;
    selectedMeleeContact::segment(1,p,a,b);
    assert(std::fabs(a.x-1)<.001f&&std::fabs(b.x-.5f)<.001f&&std::fabs(b.y-2)<.001f&&std::fabs(b.z-3)<.001f);
    if(argc!=2)return 2;
    psxAssets::Pack actual;assert(psxAssets::load(argv[1],actual)&&selectedFrames::load(argv[1]));
    assert(selectedMeleeContact::load(actual));
    p={{0,0,0,1},{0,0,0}};selectedMeleeContact::segment(1,p,a,b);
    assert(b.y>.3f&&b.y<.45f&&b.z<-.15f&&b.z>-.3f&&std::fabs(b.x)<.1f);
    assert(a.y<0&&a.y>-.15f);
    selectedMeleeContact::segment(0,p,a,b);
    assert(selectedMeleeContact::sawReady&&selectedMeleeContact::sawRadius>.05f&&selectedMeleeContact::sawRadius<.07f);
    assert(std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z))<.04f);
    assert(std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z)>.4f);
    printf("PASS actual fitted nightstick base %.4f %.4f %.4f tip %.4f %.4f %.4f; rotated controller alignment\n",a.x,a.y,a.z,b.x,b.y,b.z);
}
