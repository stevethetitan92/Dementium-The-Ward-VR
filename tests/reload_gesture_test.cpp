#include "../src/reload_runtime.hpp"
#include <cstdio>
#include <stdexcept>
#include <array>
#include <fstream>
#include <vector>
#include <string>
using namespace manualReload;
void checkAt(bool value,int line){if(!value)throw std::runtime_error("reload gesture/runtime validation failed at line "+std::to_string(line));}
#define check(value) checkAt((value),__LINE__)
static unsigned fireCalls{},reloadCalls{},updateCalls{};
void fakeFire(void*){++fireCalls;}
bool fakeReload(void*,bool){++reloadCalls;return true;}
void fakeUpdate(void*,void* step,void* context){check(step==reinterpret_cast<void*>(0x1234)&&context==reinterpret_cast<void*>(0x5678));++updateCalls;}
int fakeMenu(){return 0;}
int main(){try{
    Gestures g;Hands h;h.session=h.tracked=h.gameplay=h.pistol=true;h.time=1000;
    auto step=[&]{++h.serial;g.update(h,h.time);};
    step();h.release=true;step();check(g.transaction().stage()==Stage::EmptyWell);
    h.release=false;h.grip=true;step();check(g.transaction().stage()==Stage::EmptyWell);
    h.grip=false;step();h.left.position={-.23f,-.45f,.02f};h.grip=true;step();
    check(g.transaction().stage()==Stage::MagazineHeld);
    Bone source[12]{};for(auto& b:source)b.q[3]=4096;
    Bone animated[12];std::memcpy(animated,source,sizeof source);applyParts(animated,g.visual());
    check(animated[6].p[0]>1000&&animated[6].p[1]<-1800);
    check(!std::memcmp(animated,source,6*sizeof(Bone))&&!std::memcmp(animated+7,source+7,3*sizeof(Bone)));
    check(animated[11].p[0]==animated[6].p[0]&&animated[10].p[1]==-40960000);
    // Use a yawed gun to prove that insertion follows the hand, not the HMD.
    h.gun.orientation={0,.70710678f,0,.70710678f};
    h.gun.position={.2f,-.2f,-.4f};
    auto place=[&](XrVector3f local){
        using namespace DirectX;XMFLOAT3 p;
        XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(local.x,local.y,local.z,0),motionTracking::rotation(h.gun.orientation)));
        h.left.position={h.gun.position.x+p.x,h.gun.position.y+p.y,h.gun.position.z+p.z};h.left.orientation=h.gun.orientation;
    };
    // Inspect the held geometry outside the close push-to-seat threshold.
    place({magazineWell.x+.04f,magazineWell.y,magazineWell.z});step();
    std::memcpy(animated,source,sizeof source);applyParts(animated,g.visual());
    float body[12]={1,0,0,0,0,1,0,0,0,0,1,0},matrix[12];
    motionTracking::weaponMatrix(body,{0,0,0},h.gun,1,matrix);alignPistol(matrix);
    check(motionTracking::validBasis(matrix));
    auto world=motionTracking::worldPoint(matrix,{matrix[3],matrix[7],matrix[11]},
        {animated[6].p[0]/4096.f,animated[6].p[1]/4096.f,animated[6].p[2]/4096.f},1);
    check(distance(world,h.left.position)<.0005f);
    auto muzzle=motionTracking::worldPoint(matrix,{matrix[3],matrix[7],matrix[11]},
        {modelGrip.x,modelGrip.y,modelGrip.z+.1f},1);
    check(distance(muzzle,motionTracking::pointAhead(h.gun,.1f))<.00001f);
    h.head.orientation={0,1,0,0};h.head.position={.1f,0,0};
    h.grip=false;step();check(g.transaction().stage()==Stage::Inserted);
    h.head={ {0,0,0,1},{0,0,0} };
    place(slideHandle);h.grip=true;step();check(g.transaction().stage()==Stage::SlideHeld);
    place({slideHandle.x,slideHandle.y,slideHandle.z+.05f});step();
    std::memcpy(animated,source,sizeof source);applyParts(animated,g.visual());
    check(animated[8].p[2]<=-204&&animated[8].p[2]>=-206);
    h.grip=false;step();check(g.transaction().pendingToken()!=0);
    check(g.transaction().acknowledge(g.transaction().pendingToken(),true));
    check(!g.transaction().blocksFire());
    // Loss of tracking during a held slide cancels the grasp and rearms only
    // after release. A stale sample can never finish a rack or transfer ammo.
    h.release=true;step();h.release=false;h.left.position={-.23f,-.45f,.02f};h.grip=true;step();
    place(magazineWell);h.grip=false;step();place(slideHandle);h.grip=true;step();
    h.tracked=false;step();check(g.transaction().stage()==Stage::Inserted&&g.transaction().pendingToken()==0);
    h.tracked=true;step();check(g.transaction().stage()==Stage::Inserted);
    h.grip=false;step();h.grip=true;step();check(g.transaction().stage()==Stage::SlideHeld);
    place({slideHandle.x+.2f,slideHandle.y,slideHandle.z+.06f});step();
    check(g.transaction().stage()==Stage::Inserted&&g.transaction().pendingToken()==0);
    // Binding pivots are native model units, but the pouch and grasp travel
    // remain physical metres when world scale changes.
    for(float units:{.5f,2.f}){
        Gestures scaled;Hands sample;sample.session=sample.tracked=sample.gameplay=sample.pistol=true;sample.scale=units;sample.time=3000;
        auto tick=[&]{++sample.serial;scaled.update(sample,sample.time);};
        tick();sample.release=true;tick();sample.release=false;sample.left.position={-.23f,-.45f,.02f};sample.grip=true;tick();
        sample.left.position=trackingAnchor(magazineWell,units);sample.grip=false;tick();check(scaled.transaction().stage()==Stage::Inserted);
        sample.left.position=trackingAnchor(slideHandle,units);sample.grip=true;tick();check(scaled.transaction().stage()==Stage::SlideHeld);
        sample.left.position.z+=.05f;tick();sample.grip=false;tick();check(scaled.transaction().pendingToken()!=0);
    }
    // Replacement anchors stay in physical grip metres at every world scale.
    // Rack along a non-native axis; movement on the old Z axis must not rack.
    for(float units:{.5f,2.f})for(bool correctAxis:{false,true}){
        Gestures replacement;check(!replacement.setReplacementGeometry({},{},{0,0,0}));
        check(replacement.setReplacementGeometry({.2f,-.1f,0},{.2f,.1f,0},{1,0,0}));
        Hands sample;sample.session=sample.tracked=sample.gameplay=sample.pistol=true;sample.scale=units;sample.time=4000;
        auto tick=[&]{++sample.serial;sample.time+=20;replacement.update(sample,sample.time);};
        tick();sample.release=true;tick();
        check(!replacement.setReplacementGeometry({},{},{0,0,1}));
        sample.release=false;sample.left.position={-.23f,-.45f,.02f};sample.grip=true;tick();
        sample.left.position={.2f,-.1f,0};for(unsigned frame=0;frame<6;++frame)tick();sample.grip=false;tick();check(replacement.transaction().stage()==Stage::Inserted);
        sample.left.position={.2f,.1f,0};sample.grip=true;tick();check(replacement.transaction().stage()==Stage::SlideHeld);
        if(correctAxis)sample.left.position.x+=.05f;else sample.left.position.z+=.05f;
        tick();sample.grip=false;tick();check((replacement.transaction().pendingToken()!=0)==correctAxis);
    }
    {Gestures contact;check(contact.setReplacementGeometry({.3f,0,0},{.3f,.2f,0},{0,0,1},{.1f,0,0}));
     Hands sample;sample.session=sample.tracked=sample.gameplay=sample.pistol=true;sample.time=5000;
     auto tick=[&]{++sample.serial;sample.time+=20;contact.update(sample,sample.time);};
     tick();sample.release=true;tick();sample.release=false;sample.grip=true;sample.left.position={-.23f,-.45f,.02f};tick();
     // Controller at .2 plus finger-contact .1 reaches the well at .3.
     sample.left.position={.2f,0,0};for(unsigned frame=0;frame<6;++frame)tick();sample.grip=false;tick();check(contact.transaction().stage()==Stage::Inserted);
     sample.grip=true;tick();check(contact.transaction().stage()==Stage::Inserted);
     sample.left.position={.2f,.2f,0};tick();check(contact.transaction().stage()==Stage::Inserted);
     sample.grip=false;tick();sample.grip=true;tick();check(contact.transaction().stage()==Stage::SlideHeld);
     sample.left.position.z+=.02f;tick();sample.grip=false;tick();check(contact.transaction().stage()==Stage::Inserted&&!contact.transaction().pendingToken());
    }
    // Real Parts wrapper on an isolated native-layout fixture. Test the same
    // pointer/count checks and RAII restoration used by the live renderer.
    alignas(16) std::array<unsigned char,512> entity{},animation{};
    image=0;*reinterpret_cast<std::uintptr_t*>(entity.data())=0x30c640;
    *reinterpret_cast<unsigned char**>(entity.data()+0x50)=animation.data();animation[0x10]=12;
    std::memcpy(animation.data()+0x34,source,sizeof source);
    gestures=Gestures{};hands=Hands{};hands.session=hands.tracked=hands.gameplay=hands.pistol=true;hands.time=GetTickCount64();
    installed=true;enabled=true;hands.serial=1;gestures.update(hands,hands.time);
    hands.release=true;++hands.serial;gestures.update(hands,hands.time);
    auto saved=animation;
    {Parts parts(entity.data());check(reinterpret_cast<Bone*>(animation.data()+0x34)[6].p[1]==-40960000);}
    check(animation==saved);
    try{Parts parts(entity.data());throw 1;}catch(int){}check(animation==saved);
    animation[0x10]=11;saved=animation;{Parts parts(entity.data());check(animation==saved);}animation[0x10]=12;
    // Exercise actual suppression wrappers with fake native functions only.
    originalFire=fakeFire;originalReload=fakeReload;originalUpdate=fakeUpdate;menuState=fakeMenu;
    owned=true;hands.time=GetTickCount64();fireHook(nullptr);check(fireCalls==0);
    check(!reloadHook(nullptr,true)&&reloadCalls==0);
    hands.session=false;fireHook(nullptr);check(fireCalls==1);
    check(reloadHook(nullptr,false)&&reloadCalls==1);
    hands.session=true;hands.time=GetTickCount64()-1000;check(!consumesAction());
    check(reloadHook(nullptr,false)&&reloadCalls==2);
    hands.session=false;
    updateHook(entity.data(),reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678));check(updateCalls==1);
    // All four installed-code guards are checked against a data-only image.
    std::ifstream file("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});check(bytes.size()>4096);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
    std::vector<unsigned char> mapped(nt->OptionalHeader.SizeOfImage);
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){
        auto& s=sections[i];check(s.PointerToRawData+s.SizeOfRawData<=bytes.size()&&s.VirtualAddress+s.SizeOfRawData<=mapped.size());
        std::memcpy(mapped.data()+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    auto address=reinterpret_cast<std::uintptr_t>(mapped.data());check(matchesHooks(address));
    for(auto rva:{0x8b810,0x8b8d0,0x8b4f0,0x7c990}){mapped[rva]^=1;check(!matchesHooks(address));mapped[rva]^=1;}
    puts("PASS: tracked-hand insertion/racking, rotated poses, loss/stale gates, native fire/reload suppression and bone restoration.");
    return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
