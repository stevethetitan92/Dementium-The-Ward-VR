#include "../src/arsenal_runtime.hpp"
#include <fstream>
#include <vector>
#include <cstdio>
#include <stdexcept>
#include <string>
using namespace arsenal;
void checkAt(bool b,int line){if(!b)throw std::runtime_error("arsenal validation failed at line "+std::to_string(line));}
#define check(b) checkAt((b),__LINE__)
unsigned* worldFlags{};float reportedDistance=.2f;unsigned traceCalls{};
bool fakeWorld(void*,float*,float* distance,float*,void**,bool enabled){check(enabled&&(*worldFlags&0x80000000));++traceCalls;*distance=reportedDistance;*worldFlags|=0x20;return true;}
unsigned contactCalls{};
void fakeNativeContact(void*,void*){++contactCalls;}
unsigned fireCalls{};
void fakeFire(void*){++fireCalls;}
unsigned updateCalls{};
void fakeUpdate(void*,void* step,void* context){check(step==nullptr&&context==reinterpret_cast<void*>(0x5678));++updateCalls;}
int fakeMenu(){return 0;}
float* fakeRay(void*,float* out){for(unsigned i=0;i<6;i++)out[i]=0;out[5]=-1;return out;}
void* expectedActor{};void* expectedWeapon{};unsigned damageCalls{};bool queryAccepted=true;float queryDistance=.1f;
bool fakeQuery(void* actor,float* ray,float* distance,float radius,float* point,int* surface,int* triangle){
    check(actor==expectedActor&&std::fabs(ray[5])==1&&std::fabs(radius-.035f)<.00001f);
    *distance=queryDistance;point[0]=1;point[1]=2;point[2]=3;*surface=17;*triangle=29;return queryAccepted;
}
bool tipEntryQuery(void* actor,float* ray,float* distance,float radius,float* point,int* surface,int* triangle){
    if(ray[5]<0)return false;
    check(std::fabs(ray[2]+.5f)<.00001f); // Reverse starts exactly at the bounded tip.
    return fakeQuery(actor,ray,distance,radius,point,surface,triangle);
}
bool fakeDamage(void* actor,float* point,float* impulse,int surface,int triangle,void* weapon){
    check(actor==expectedActor&&weapon==expectedWeapon&&point[0]==1&&point[1]==2&&point[2]==3&&
        impulse[0]==0&&impulse[1]==0&&impulse[2]==-1&&surface==17&&triangle==29);++damageCalls;return true;
}
void* expectedVoice{};bool voicePlaying{};unsigned stops{},plays{};
bool fakePlaying(void* voice){check(voice==expectedVoice);return voicePlaying;}
void fakeStop(void* voice,int parameter){check(voice==expectedVoice&&parameter==0);++stops;}
void fakePlay(void* voice,int parameter){check(voice==expectedVoice&&parameter==37);++plays;}
bool enemyType=true;short enemyHealth=10;unsigned healthReads{};
bool fakeType(void*,unsigned code){check(code==0x1020f);return enemyType;}
short fakeHealth(void*){++healthReads;return enemyHealth;}
int main(int argc,char** argv){try{
 for(auto caller:{0x90a06u,0x90be0u,0x90d98u}){
  check(!huntingScopeControls::stationary(true,true,caller));
  check(huntingScopeControls::stationary(true,false,caller));
  check(!huntingScopeControls::stationary(false,true,caller));
 }
 check(huntingScopeControls::stationary(true,true,0x90a07));
 check(!huntingScopeControls::stationary(true,true,0x12345,true));
 check(huntingScopeControls::stationary(true,false,0x12345,true));
 check(!huntingScopeControls::stationary(false,true,0x12345,true));
 // Supported pump travel is invariant to unassisted wrist rotation. A wrist
 // turn alone must not pump, invalidate, or reset the retained stroke.
 {
  XrVector3f rest{0,0,-.4f};
  for(int i=0;i<360;i+=5){float a=float(i)*3.14159265f/180;
   XrVector3f p{.4f*std::sin(a),0,-.4f*std::cos(a)};
   check(supportPumpTravel::valid(rest,p));check(std::fabs(supportPumpTravel::travel(rest,p))<.00001f);
   p={.3f*std::sin(a),0,-.3f*std::cos(a)};
   check(std::fabs(supportPumpTravel::travel(rest,p)-.1f)<.00001f);
  }
  check(!supportPumpTravel::valid(rest,{0,0,0}));
  check(!supportPumpTravel::valid(rest,{0,0,-2}));
  check(!supportPumpTravel::valid(rest,{NAN,0,0}));
 }
 // Replacement hunting rifle: insert a magazine, with no bolt manipulation.
 {Reload r;r.useDetachableMagazine(true);Event e;e.valid=true;r.update(Kind::Sniper,e);
  e.release=true;r.update(Kind::Sniper,e);check(r.state()==Phase::Open&&r.boltOffset()==0);
  e.release=false;e.grip=e.atPouch=true;r.update(Kind::Sniper,e);check(r.state()==Phase::Held);
  e.grip=false;e.atPort=false;r.update(Kind::Sniper,e);check(r.state()==Phase::Open&&!r.token());
  e.grip=true;r.update(Kind::Sniper,e);e.grip=false;e.atPort=true;r.update(Kind::Sniper,e);
  check(r.token()&&r.rounds()==~0u);auto id=r.token();auto lost=r;Event invalid=e;invalid.valid=false;lost.update(Kind::Sniper,invalid);
  check(!lost.token()&&!lost.acknowledge(id,true));
  invalid.valid=true;invalid.grip=invalid.release=false;lost.update(Kind::Sniper,invalid);
  invalid.release=true;lost.update(Kind::Sniper,invalid);check(lost.state()==Phase::Open&&!lost.token());
  check(r.acknowledge(id,true)&&r.state()==Phase::Ready&&!r.acknowledge(id,true));
  r.requireCycle();check(r.state()==Phase::Ready);
 }
 ContextButton button;check(!button.update(true,true));check(!button.update(true,false));check(!button.update(false,false));check(button.update(true,false));
 vrFeedback::queue(0,.2f,15);vrFeedback::queue(0,.7f,200);auto pulse=vrFeedback::take(0);
 check((pulse>>16)==700&&(pulse&65535)==60&&vrFeedback::take(0)==0);
 for(int cap:{6,30,8,5})for(int a=0;a<=cap;a++)for(int b=0;b<=100;b++)for(unsigned amount:{0u,1u,~0u}){
    manualReload::Transfer t;check(feed(a,b,cap,amount,t));check(t.loaded+t.reserve==a+b&&t.loaded<=cap&&t.reserve>=0&&t.amount>=0);
    if(amount==0)check(t.loaded==a&&t.reserve==b);if(amount==1)check(t.amount<=1);
 }
 for(auto kind:{Kind::Revolver,Kind::Rifle,Kind::Sniper}){
    Reload r;Event e;e.valid=true;r.update(kind,e);e.release=true;r.update(kind,e);check(r.blocked());
    e.release=false;
    if(kind==Kind::Sniper){
        check(r.state()==Phase::Opening);e.grip=true;e.atHandle=true;r.update(kind,e);e.travel=.08f;r.update(kind,e);e.grip=false;r.update(kind,e);
        check(r.state()==Phase::Open&&std::fabs(r.boltOffset()-.08f)<.00001f);e.travel=0;
    }
    e.grip=true;e.atPouch=true;r.update(kind,e);check(r.state()==Phase::Held);
    e.grip=false;e.atPort=true;r.update(kind,e);
    if(kind!=Kind::Rifle){check(r.token()&&r.rounds()==1);
        auto interrupted=r;auto cancelledToken=interrupted.token();Event lost=e;lost.valid=false;interrupted.update(kind,lost);
        check(interrupted.state()==Phase::Open&&!interrupted.token()&&!interrupted.acknowledge(cancelledToken,true));
        auto id=r.token();check(r.acknowledge(id,true));check(!r.acknowledge(id,true));
        e.release=true;r.update(kind,e);e.release=false;r.update(kind,e);
        if(kind==Kind::Revolver){check(!r.blocked());continue;}
    }
    check(r.state()==Phase::Inserted);e.grip=true;e.atHandle=true;e.travel=0;r.update(kind,e);check(r.state()==Phase::Charging);
    e.travel=kind==Kind::Sniper?-.08f:.1f;r.update(kind,e);
    if(kind!=Kind::Rifle&&kind!=Kind::Sniper){e.travel=0;r.update(kind,e);}
    e.grip=false;r.update(kind,e);check(r.token()!=0);check(r.rounds()==(kind==Kind::Rifle?~0u:0));
    auto interrupted=r;auto cancelledToken=interrupted.token();Event lost=e;lost.valid=false;interrupted.update(kind,lost);
    check(interrupted.state()==Phase::Inserted&&!interrupted.token()&&!interrupted.acknowledge(cancelledToken,true));
    check(r.acknowledge(r.token(),true)&&!r.blocked());
    r.requireCycle();check(r.blocked());e.valid=false;r.update(kind,e);check(r.state()==Phase::Inserted);
 }
 {Reload r;Event e;e.valid=true;r.update(Kind::Rifle,e);e.release=true;r.update(Kind::Rifle,e);
  e.release=false;r.update(Kind::Rifle,e);e.release=true;r.update(Kind::Rifle,e);check(!r.token()&&r.blocked()&&r.state()==Phase::Open);}
 for(unsigned i=0;i<6;i++){
    float m[12]={1,0,0,0,0,1,0,0,0,0,1,0};align(i,m);check(motionTracking::validBasis(m));
    auto p=profiles[i].anchor;
    auto grip=motionTracking::worldPoint(m,{m[3],m[7],m[11]},p,1);
    check(manualReload::distance(grip,{0,0,0})<.001f);
    auto offset=modelOffset(i,{.1f,.2f,-.3f});
    auto mapped=motionTracking::worldPoint(m,{0,0,0},offset,1);
    check(manualReload::distance(mapped,{.1f,.2f,-.3f})<.001f);
 }
 for(float units:{.5f,1.f,2.f}){
    auto rifle=port(Kind::Rifle,units),cylinder=port(Kind::Revolver,units),pump=handle(Kind::Shotgun,units);
    check(std::fabs(rifle.z*units+.168990314f)<.000001f);
    check(std::fabs((cylinder.x+.035f)*units-.005063355f)<.000001f);
    check(std::fabs(pump.z*units+.290349066f)<.000001f);
 }
 // Independently compare transformed axes in controller space. The rifle
 // basis differs from the pistol's 180-degree flip and must not reuse it.
 for(unsigned i=0;i<6;i++)for(XrQuaternionf turn: {XrQuaternionf{0,0,0,1},XrQuaternionf{.38268343f,0,0,.92387953f},XrQuaternionf{0,.70710678f,0,.70710678f}}){
    using namespace DirectX;
    manualReload::Bone bone{};bone.q[3]=4096;rotatePart(i,bone,turn);check(manualReload::validBone(bone));
    XrQuaternionf q{bone.q[0]/4096.f,bone.q[1]/4096.f,bone.q[2]/4096.f,bone.q[3]/4096.f};
    auto actual=motionTracking::rotation(q);
    for(XrVector3f axis: {XrVector3f{1,0,0},XrVector3f{0,1,0},XrVector3f{0,0,1}}){
        XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformNormal(XMVectorSet(axis.x,axis.y,axis.z,0),actual));
        const auto* c=calibration[i];
        XrVector3f mapped{c[0]*v.x+c[1]*v.y+c[2]*v.z,c[3]*v.x+c[4]*v.y+c[5]*v.z,c[6]*v.x+c[7]*v.y+c[8]*v.z};
        auto start=XMVectorSet(c[0]*axis.x+c[1]*axis.y+c[2]*axis.z,c[3]*axis.x+c[4]*axis.y+c[5]*axis.z,c[6]*axis.x+c[7]*axis.y+c[8]*axis.z,0);
        XMStoreFloat3(&v,XMVector3TransformNormal(start,motionTracking::rotation(turn)));
        check(manualReload::distance(mapped,{v.x,v.y,v.z})<.002f);
    }
 }
 // The sniper must expose its breech before loading, retain that position
 // across a pause, and reject a closing stroke that stops short.
 {
    Reload r;Event e;e.valid=true;r.update(Kind::Sniper,e);e.release=true;r.update(Kind::Sniper,e);
    check(r.state()==Phase::Opening);e.release=false;e.grip=e.atPouch=true;r.update(Kind::Sniper,e);check(r.state()==Phase::Opening);
    e.grip=false;r.update(Kind::Sniper,e);e.grip=e.atHandle=true;r.update(Kind::Sniper,e);
    e.travel=.08f;r.update(Kind::Sniper,e);e.travel=0;e.grip=false;r.update(Kind::Sniper,e);check(r.state()==Phase::Opening&&!r.token());
    e.grip=true;r.update(Kind::Sniper,e);e.travel=.08f;r.update(Kind::Sniper,e);e.grip=false;r.update(Kind::Sniper,e);
    check(r.state()==Phase::Open&&r.boltOffset()>.079f);
    e.valid=false;r.update(Kind::Sniper,e);check(r.state()==Phase::Open&&r.boltOffset()>.079f);
    e.valid=true;e.travel=0;r.update(Kind::Sniper,e);e.release=true;r.update(Kind::Sniper,e);e.release=false;r.update(Kind::Sniper,e);
    e.grip=true;r.update(Kind::Sniper,e);e.travel=-.03f;r.update(Kind::Sniper,e);e.grip=false;r.update(Kind::Sniper,e);
    check(r.state()==Phase::Inserted&&!r.token()&&r.boltOffset()>.079f);
    e.travel=0;e.grip=true;r.update(Kind::Sniper,e);e.travel=-.08f;r.update(Kind::Sniper,e);e.grip=false;r.update(Kind::Sniper,e);
    check(r.token()&&r.rounds()==0);check(r.acknowledge(r.token(),true)&&r.state()==Phase::Ready&&r.boltOffset()==0);
 }
 // A support squeeze carried to the hip can grab a detachable replacement,
 // but opening the well while already inside the pouch cannot auto-grab.
 for(auto kind:{Kind::Rifle,Kind::Sniper}){
    Reload r;r.useDetachableMagazine(true);Event e;e.valid=true;r.update(kind,e);
    e.grip=true;r.update(kind,e);e.release=true;r.update(kind,e);check(r.state()==Phase::Open);
    e.release=false;e.atPouch=true;r.update(kind,e);check(r.state()==Phase::Held);
    e.valid=false;r.update(kind,e);e.valid=true;r.update(kind,e);check(r.state()==Phase::Open);
 }
 // Fast supported return strokes may skip over the narrow rest window.
 for(float forward:{-.025f,-.05f,-.075f}){
    Reload r;Event e;e.valid=true;r.update(Kind::Shotgun,e);r.requireCycle();
    e.grip=e.atHandle=e.supportHeld=true;r.update(Kind::Shotgun,e);
    e.travel=.09f;r.update(Kind::Shotgun,e);e.travel=forward;r.update(Kind::Shotgun,e);
    check(r.token()&&r.rounds()==0);check(r.acknowledge(r.token(),true)&&r.state()==Phase::Ready);
    r.requireCycle();e.travel=0;r.update(Kind::Shotgun,e);
    e.travel=.04f;r.update(Kind::Shotgun,e);e.travel=forward;r.update(Kind::Shotgun,e);
    check(!r.token()); // An incomplete rearward stroke must not chamber.
 }
 // A return stroke must finish near the handle, not far beyond it.
 for(auto kind:{Kind::Shotgun,Kind::Sniper}){
    Reload r;Event e;e.valid=true;r.update(kind,e);r.requireCycle();
    e.grip=true;e.atHandle=true;r.update(kind,e);e.travel=.1f;r.update(kind,e);
    e.travel=-.3f;e.grip=false;r.update(kind,e);check(!r.token()&&r.state()==Phase::Inserted);
 }
 Melee club;Segment s{{0,0,0},{0,0,-.5f}};
 check(!club.update(Kind::Nightstick,s,1000,true,0));check(!club.update(Kind::Nightstick,s,1010,true,0));
 s.tip.x=.04f;check(!club.update(Kind::Nightstick,s,1020,true,0));s.tip.x=.1f;check(club.update(Kind::Nightstick,s,1030,true,0));
 check(club.canHit(100,1030,Kind::Nightstick));club.landed(100,1030);check(!club.canHit(100,1040,Kind::Nightstick));
 check(club.update(Kind::Nightstick,s,1030,true,0)); // Duplicate game tick preserves the swing.
 // A broad physical stroke can reach an enemy after the old 220 ms window.
 // Continuing the stroke must detect that target without damaging an old one twice.
 for(unsigned t=1040;t<=1400;t+=10){s.tip.x+=.02f;check(club.update(Kind::Nightstick,s,t,true,0));}
 check(club.canHit(101,1400,Kind::Nightstick));check(!club.canHit(100,1400,Kind::Nightstick));
 check(!club.update(Kind::Nightstick,s,1410,true,0)); // Rest terminates contact.
 check(!club.canHit(101,1410,Kind::Nightstick));
 // Fast alternating strokes rearm without requiring a stationary frame.
 Melee rapid;Segment fast{{0,0,0},{0,0,-.5f}};
 rapid.update(Kind::Nightstick,fast,2000,true,0);rapid.update(Kind::Nightstick,fast,2010,true,0);
 for(unsigned t=2020;t<=2180;t+=10){fast.tip.x+=.02f;rapid.update(Kind::Nightstick,fast,t,true,0);}
 check(rapid.active());rapid.landed(100,2180);
 fast.tip.x-=.02f;rapid.update(Kind::Nightstick,fast,2190,true,0);check(!rapid.canHit(100,2190,Kind::Nightstick));
 for(unsigned t=2200;t<=2240;t+=10){fast.tip.x-=.02f;rapid.update(Kind::Nightstick,fast,t,true,0);}
 check(rapid.canHit(100,2240,Kind::Nightstick));rapid.landed(100,2240);
 check(!rapid.canHit(100,2241,Kind::Nightstick));
 // Fast rotational tip motion remains valid; spatial tracking jumps do not.
 Melee quick;Segment q{{0,0,0},{0,0,-.5f}};
 quick.update(Kind::Nightstick,q,4000,true,0);quick.update(Kind::Nightstick,q,4010,true,0);
 q.tip.x=.16f;check(quick.update(Kind::Nightstick,q,4020,true,0));
 check(quick.speedRejects==0);quick.landed(42,4020);
 q.tip.x=.32f;check(quick.update(Kind::Nightstick,q,4030,true,0));
 check(!quick.canHit(42,4030,Kind::Nightstick));
 q.base.x=1;check(!quick.update(Kind::Nightstick,q,4040,true,0));check(quick.speedRejects==1);
 meleeImpactStop::Stop stop;
 Segment stopBefore{{0,0,0},{0,0,-.5f}},after{{.02f,0,0},{.02f,0,-.5f}};
 stop.contact(stopBefore,after,3000,1);auto arrest=stop.correction(after,3010);check(std::fabs(arrest.x+.02f)<.0001f);
 // Repeated confirmed contact must retain the original surface, not creep inward.
 Segment deep{{.4f,0,0},{.4f,0,-.5f}};
 stop.contact(after,deep,3050,1);check(std::fabs(stop.correction(deep,3060).x+.4f)<.0001f);
 Segment withdrawn{{-.02f,0,0},{-.02f,0,-.5f}};
 check(stop.correction(withdrawn,3070).x==0&&stop.correction(after,3080).x==0);
 stop.contact(stopBefore,after,3000,1);check(stop.correction(after,3800).x==0);
 check(!club.update(Kind::Nightstick,s,1100,false,0));check(!club.canHit(101,1100,Kind::Nightstick));
 Melee saw;s={{0,0,0},{0,0,-.38f}};saw.update(Kind::Saw,s,2000,true,1);
 for(unsigned t=2010;t<2250;t+=10)check(!saw.update(Kind::Saw,s,t,true,1));
 saw.update(Kind::Saw,s,2250,true,0);
 for(unsigned t=2260;t<2460;t+=10)check(!saw.update(Kind::Saw,s,t,true,1));
 check(saw.update(Kind::Saw,s,2460,true,1));check(saw.canHit(200,2460,Kind::Saw));saw.landed(200,2460);
 check(!saw.canHit(200,2559,Kind::Saw)&&saw.canHit(200,2560,Kind::Saw));
 check(!saw.update(Kind::Saw,s,2470,true,0));
 unsigned samples=0;bool crossed=false;
 sweep({{-.2f,0,0},{-.2f,0,-.5f}},{{.2f,0,0},{.2f,0,-.5f}},[&](Segment p){++samples;if(std::fabs(p.tip.x)<.035f)crossed=true;});
 check(samples>10&&samples<32&&crossed);
 for(float units:{.5f,1.f,2.f}){
    unsigned count=0;sweep({{-.2f*units,0,0},{-.2f*units,0,-.5f*units}},{{.2f*units,0,0},{.2f*units,0,-.5f*units}},[&](Segment){++count;},units);
    check(count==samples);
    count=0;sweep({{0,0,0},{0,0,-.5f*units}},{{units,0,0},{units,0,-.5f*units}},[&](Segment){++count;},units);check(count==0);
 }
 // Data-only PE relocation: never load or execute the game.
 std::ifstream f(argc>1?argv[1]:"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);
 std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});check(bytes.size()>4096);
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
 std::vector<unsigned char> mapped(nt->OptionalHeader.SizeOfImage);auto sections=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){auto& s=sections[i];check(s.VirtualAddress+s.SizeOfRawData<=mapped.size()&&s.PointerToRawData+s.SizeOfRawData<=bytes.size());std::memcpy(mapped.data()+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);}
 auto base=reinterpret_cast<std::uintptr_t>(mapped.data());auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
 for(unsigned n=0;n<directory.Size;){auto block=reinterpret_cast<IMAGE_BASE_RELOCATION*>(mapped.data()+directory.VirtualAddress+n);check(block->SizeOfBlock>=8&&block->SizeOfBlock<=directory.Size-n);
   auto entries=reinterpret_cast<WORD*>(block+1);for(unsigned i=0;i<(block->SizeOfBlock-8)/2;i++)if((entries[i]>>12)==IMAGE_REL_BASED_DIR64){auto slot=reinterpret_cast<std::uintptr_t*>(mapped.data()+block->VirtualAddress+(entries[i]&4095));*slot+=base-nt->OptionalHeader.ImageBase;}n+=block->SizeOfBlock;}
 check(verify(base));for(const auto& p:profiles){mapped[p.update]^=1;check(!verify(base));mapped[p.update]^=1;}
 for(unsigned i=0;i<3;i++){mapped[impactAudioRva[i]]^=1;check(!verify(base));mapped[impactAudioRva[i]]^=1;}
 mapped[0xf2090]^=1;check(!verify(base));
 // Exercise the actual static-world wrapper with a fake trace callback. This
 // verifies collision distance clipping and native context-bit restoration.
 std::array<unsigned char,0x140> level{};std::array<unsigned char,0x20> context{};std::uintptr_t scene{};
 *reinterpret_cast<void**>(level.data()+0x130)=&scene;
 *reinterpret_cast<void**>(mapped.data()+0x3a5330)=level.data();*reinterpret_cast<void**>(mapped.data()+0x3fc480)=context.data();
 arsenal::image=base;traceWorld=fakeWorld;worldFlags=reinterpret_cast<unsigned*>(context.data()+0x1c);*worldFlags=4;
 bool wall=false;float ray[6]={0,0,0,0,0,-1},limit=.5f;check(worldLimit(ray,limit,.035f,&wall)&&wall&&std::fabs(limit-.165f)<.00001f&&*worldFlags==0x24);
 reportedDistance=2;limit=.5f;check(worldLimit(ray,limit)&&std::fabs(limit-.465f)<.00001f);
 reportedDistance=-1;limit=.5f;check(!worldLimit(ray,limit)&&*worldFlags==0x24);
 reportedDistance=std::numeric_limits<float>::quiet_NaN();limit=.5f;check(!worldLimit(ray,limit)&&*worldFlags==0x24);
 unsigned calls=traceCalls;limit=.5f;ray[3]=2;check(!worldLimit(ray,limit)&&traceCalls==calls);ray[3]=0;
 ray[0]=std::numeric_limits<float>::infinity();check(!worldLimit(ray,limit)&&traceCalls==calls);ray[0]=0;
 check(!worldLimit(nullptr,limit)&&!worldLimit(ray,limit,-1)&&traceCalls==calls);
 reportedDistance=.2f;limit=.5f;*worldFlags=0x80000004;check(worldLimit(ray,limit)&&*worldFlags==0x80000024);
 *reinterpret_cast<void**>(level.data()+0x130)=nullptr;check(!worldLimit(ray,limit));
 // Execute our hook against inert fake objects only: lost conversion must
 // preserve the original update and discard the previous world-space sweep.
 std::array<unsigned char,0x100> weapon{},player{};
 *reinterpret_cast<void**>(mapped.data()+0x3a5340)=player.data();
 *reinterpret_cast<std::uintptr_t*>(player.data()+0xb8)=base+0x30bff8;
 float identity[12]={1,0,0,0,0,1,0,0,0,0,1,0};std::memcpy(player.data()+8,identity,sizeof identity);
 std::array<unsigned char,0x100> camera{};
 std::memcpy(camera.data()+0xbc,identity,sizeof identity);
 *reinterpret_cast<void**>(mapped.data()+0x3a5338)=camera.data();
 // Equipped idle melee observed in the real game has 0x0c, without the
 // additional native 0x10 gate. It must still reach motion conversion.
 *reinterpret_cast<unsigned*>(weapon.data()+0x44)=0x0c;
 installed=true;manualReload::menuState=fakeMenu;updates[4]=fakeUpdate;rayAccessor=nullptr;
 auto& h=manualReload::hands;h={};h.time=GetTickCount64();h.session=h.gameplay=h.gunTracked=true;h.weapon=static_cast<int>(Kind::Saw);
 motionTracking::current={};motionTracking::current.valid=true;motionTracking::current.time=h.time;
 haveWorld[0]=true;updateHook<4>(weapon.data(),nullptr,reinterpret_cast<void*>(0x5678));check(updateCalls==1&&!haveWorld[0]);
 h.gunTracked=false;haveWorld[0]=true;updateHook<4>(weapon.data(),nullptr,reinterpret_cast<void*>(0x5678));check(updateCalls==2&&!haveWorld[0]);
 // Exercise the full wall sweep with an empty inert actor list. Feedback
 // is throttled, and putting the hand beyond a wall cannot attack from there.
 *reinterpret_cast<void**>(level.data()+0x130)=&scene;
 auto listHead=base+0x3e83e0;*reinterpret_cast<std::uintptr_t*>(listHead)=listHead;
 rayAccessor=fakeRay;traceWorld=fakeWorld;reportedDistance=.2f;
 h.time=GetTickCount64();h.weapon=static_cast<int>(Kind::Saw);h.gunTracked=h.gameplay=true;
 melee[0]=Melee{};Segment blade{{0,0,0},{0,0,-.38f}};
 melee[0].update(Kind::Saw,blade,10000,true,0);melee[0].update(Kind::Saw,blade,10010,true,0);
 for(auto t:{10020ull,10100ull,10180ull,10220ull})melee[0].update(Kind::Saw,blade,t,true,1);
 check(melee[0].active());vrFeedback::take(1);auto wallCount=wallHits;
 contactSweep(0,Kind::Saw,weapon.data(),blade,blade,10220);check(wallHits==wallCount+1&&vrFeedback::take(1)!=0);
 contactSweep(0,Kind::Saw,weapon.data(),blade,blade,10220);check(wallHits==wallCount+1&&vrFeedback::take(1)==0);
 Segment beyondWall{{1,0,0},{1,0,-.38f}};
 contactSweep(0,Kind::Saw,weapon.data(),beyondWall,beyondWall,10320);check(wallHits==wallCount+1);
 // Left-hand melee impacts must not vibrate the empty right controller.
 h.primaryHand=0;vrFeedback::take(0);vrFeedback::take(1);
 contactSweep(0,Kind::Saw,weapon.data(),blade,blade,10320);
 check(wallHits==wallCount+2&&vrFeedback::take(0)!=0&&vrFeedback::take(1)==0);h.primaryHand=1;
 // A collision-scene change cancels an active tool before the original
 // update returns; it cannot sweep the old room into the new one.
 auto frame=GetTickCount64();h.time=frame;h.scale=1;
 motionTracking::current.valid=true;motionTracking::current.time=frame;
 melee[0]=Melee{};melee[0].update(Kind::Saw,blade,frame-220,true,0);melee[0].update(Kind::Saw,blade,frame-210,true,0);
 for(auto t:{frame-200,frame-110,frame-20,frame})melee[0].update(Kind::Saw,blade,t,true,1);
 check(melee[0].active());haveWorld[0]=true;previousScene[0]=1;previousPlayer[0]=reinterpret_cast<std::uintptr_t>(player.data());
 auto beforeUpdate=updateCalls;wallCount=wallHits;
 updateHook<4>(weapon.data(),nullptr,reinterpret_cast<void*>(0x5678));
 check(updateCalls==beforeUpdate+1&&!melee[0].active()&&previousScene[0]==reinterpret_cast<std::uintptr_t>(&scene)&&wallHits==wallCount);
 // The saw respects native enemy health and transient hit locks, while
 // ordinary breakable objects do not require an enemy health accessor.
 std::array<unsigned char,0x210> target{};
 check(sawEligible(target.data(),fakeType,fakeHealth));enemyHealth=0;check(!sawEligible(target.data(),fakeType,fakeHealth));
 enemyHealth=-1;check(!sawEligible(target.data(),fakeType,fakeHealth));enemyHealth=10;
 *reinterpret_cast<short*>(target.data()+0x208)=1;check(!sawEligible(target.data(),fakeType,fakeHealth));
 *reinterpret_cast<short*>(target.data()+0x208)=0;check(!sawEligible(target.data(),fakeType,nullptr));
 enemyType=false;auto reads=healthReads;check(sawEligible(target.data(),fakeType,nullptr)&&healthReads==reads);
 check(!sawEligible(nullptr,fakeType,fakeHealth));enemyType=true;
 // Native callback ABI and final approval are exercised without game code.
 expectedActor=weapon.data();expectedWeapon=player.data();float contactRay[6]={0,0,0,0,0,-1};unsigned approvals=0;
 meleeTrace::buffer.enabled=true;
 auto approve=[&]{++approvals;return true;};
 check(applyContact(expectedActor,fakeQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,approve));
 check(damageCalls==1&&approvals==1);
 check(!applyContact(expectedActor,fakeQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,[]{return false;}));
 check(damageCalls==1);queryDistance=.6f;
 check(!applyContact(expectedActor,fakeQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,approve));
 queryDistance=std::numeric_limits<float>::quiet_NaN();
 check(!applyContact(expectedActor,fakeQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,approve));
 queryDistance=.1f;queryAccepted=false;
 check(!applyContact(expectedActor,fakeQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,approve));
 check(damageCalls==1&&approvals==1);
 // Reuse initialized native impact voices without restarting a playing one.
 queryAccepted=true;
 check(applyContact(expectedActor,tipEntryQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,approve));
 check(damageCalls==2&&approvals==2); // One damage callback even with two probes.
 queryDistance=.6f;
 check(!applyContact(expectedActor,tipEntryQuery,fakeDamage,contactRay,.5f,.035f,{0,0,-1},expectedWeapon,approve));
 check(damageCalls==2);queryDistance=.1f;
 std::array<meleeTrace::Event,256> traceEvents{};auto traceCount=meleeTrace::buffer.drain(traceEvents);
 check(traceCount==7);check(traceEvents[0].result==4&&traceEvents[1].result==2);
 check(traceEvents[2].result==1&&traceEvents[3].result==1&&traceEvents[4].result==0);
 check(traceEvents[5].result==4&&traceEvents[5].reverse==1&&traceEvents[6].result==1);
 check(traceEvents[0].actor==reinterpret_cast<std::uintptr_t>(expectedActor));
 meleeTrace::buffer.enabled=false;
 std::array<unsigned char,0x200> audioWeapon{};soundPlaying=fakePlaying;soundStop=fakeStop;soundPlay=fakePlay;
 for(auto kind:{Kind::Saw,Kind::Nightstick}){
    expectedVoice=audioWeapon.data()+(kind==Kind::Saw?0x180:0x140);*reinterpret_cast<int*>(static_cast<unsigned char*>(expectedVoice)+0x24)=37;
    auto before=plays;voicePlaying=false;playImpact(kind,audioWeapon.data());check(plays==before+1&&stops==plays);
    voicePlaying=true;playImpact(kind,audioWeapon.data());check(plays==before+1&&stops==plays);
 }
 auto before=plays;playImpact(Kind::Rifle,audioWeapon.data());playImpact(Kind::Saw,nullptr);check(plays==before);
 // Native firing must remain blocked through stale/lost tracking and modal
 // input, while unrelated interfaces still pass through the shared hook.
 h.time=GetTickCount64();h.weapon=static_cast<int>(Kind::Revolver);h.gunTracked=h.gameplay=true;
 reloads[0]=Reload{};fires[0]=fakeFire;
 *reinterpret_cast<std::uintptr_t*>(weapon.data()+0xb8)=base+profiles[0].interfaceTable;
 fireHook<0>(weapon.data()+0xb8);check(fireCalls==1);
 h.gunTracked=false;fireHook<0>(weapon.data()+0xb8);check(fireCalls==1);
 h.gunTracked=true;h.gameplay=false;fireHook<0>(weapon.data()+0xb8);check(fireCalls==1);
 h.gameplay=true;h.time=GetTickCount64()-150;fireHook<0>(weapon.data()+0xb8);check(fireCalls==1);
 h.time=GetTickCount64();Event opening;opening.valid=true;reloads[0].update(Kind::Revolver,opening);opening.release=true;reloads[0].update(Kind::Revolver,opening);
 fireHook<0>(weapon.data()+0xb8);check(fireCalls==1);
 *reinterpret_cast<std::uintptr_t*>(weapon.data()+0xb8)=0;fireHook<0>(weapon.data()+0xb8);check(fireCalls==2);
 // Queued legacy melee animation contacts must not resume on the first
 // focus-loss frame; ordinary desktop behavior returns after ownership ends.
 contacts[0]=fakeNativeContact;*reinterpret_cast<std::uintptr_t*>(weapon.data())=base+profiles[4].entity;
 h.time=GetTickCount64();h.session=true;contactHook<0>(weapon.data(),nullptr);check(contactCalls==0);
 h.session=false;contactHook<0>(weapon.data(),nullptr);check(contactCalls==0);
 lastVrUse=GetTickCount64()-501;contactHook<0>(weapon.data(),nullptr);check(contactCalls==1);
 h.session=true;
 // Renderer overrides must be temporary even if a downstream draw throws.
 for(unsigned i=0;i<4;i++){
    std::array<unsigned char,0x200> animation{};
    animation[0x10]=static_cast<unsigned char>(profiles[i].bones);
    auto bones=reinterpret_cast<manualReload::Bone*>(animation.data()+0x34);
    for(unsigned b=0;b<profiles[i].bones;b++){bones[b].q[3]=4096;bones[b].p[0]=static_cast<int>(b)*12;}
    *reinterpret_cast<unsigned*>(animation.data()+0x18)=0x40000000;
    *reinterpret_cast<void**>(weapon.data()+0x50)=animation.data();
    h.time=GetTickCount64();h.session=true;h.weapon=static_cast<int>(profiles[i].kind);
    reloads[i]=Reload{};Event e;e.valid=true;reloads[i].update(profiles[i].kind,e);e.release=true;reloads[i].update(profiles[i].kind,e);
    e.release=false;e.grip=true;e.atPouch=true;reloads[i].update(profiles[i].kind,e);
    visuals[i]={};visuals[i].valid=true;visuals[i].scale=1;visuals[i].leftInGun.orientation={0,.38268343f,0,.92387953f};visuals[i].leftInGun.position={-.2f,-.1f,.1f};
    auto saved=animation;
    try{Parts temporary(i,weapon.data());
        if(i<2)check(std::memcmp(animation.data(),saved.data(),animation.size())!=0);
        throw 7;
    }catch(int){}
    check(animation==saved);
    visuals[i].scale=std::numeric_limits<float>::infinity();
    {Parts invalid(i,weapon.data());check(animation==saved);}
    visuals[i].scale=1;bones[0].q[3]=0;saved=animation;
    {Parts invalid(i,weapon.data());check(animation==saved);}
 }
 installed=false;
 puts("PASS: all firearm reload sequences, pump/bolt cycles, melee swing and motor gates, hit cooldowns, swept coverage and six native profiles.");return 0;
 }catch(const std::exception& e){puts(e.what());return 1;}}

