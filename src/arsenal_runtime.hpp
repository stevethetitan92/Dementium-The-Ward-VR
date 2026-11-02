#pragma once
#include "arsenal_profiles.hpp"
#include "reload_runtime.hpp"
#include "revolver_contact.hpp"
#include "shotgun_pump_cursor.hpp"
#include "pump_sounds.hpp"
#include "support_pump_travel.hpp"
#include "hunting_scope_controls.hpp"
#include <intrin.h>
#include "mechanism_transition.hpp"
#include "rifle_cadence.hpp"
#include "cylinder_flick.hpp"
#include "selected_melee_contact.hpp"
#include "melee_impact_stop.hpp"
#include "melee_contact_trace.hpp"
namespace arsenal {
inline bool enabled=true,meleeEnabled=true,installed=false;
struct LoaderVisual {bool active{};unsigned first{},count{};};
inline LoaderVisual loader; // protected by manualReload::mutex
inline std::uintptr_t image{};inline std::uint64_t lastVrUse{};
inline Reload reloads[6];inline Melee melee[2];
inline Segment trackedPrevious[2]{},trackedBefore[2]{},trackedCurrent[2]{};
inline bool trackedHistory[2]{};
inline mechanismMotion::Transition cylinderTransition;
inline cylinderFlick::Detector cylinderFlickDetector;
inline XrVector3f cylinderClosingAxis{};
inline rifleCadence::Clock rifleClock;
inline bool rifleCadenceSupported{};
inline bool revolverTriggerSupported{};
inline semiFire::Edge revolverTriggerEdge;
inline semiFire::Edge huntingTriggerEdge;
// Read under manualReload::mutex; fixed-size samples avoid frame-path logging.
struct ReloadEvidence {Event event{};int loaded{-1},reserve{-1},capacity{-1};float pouch{};unsigned invalid{},pouchSamples{},gripPouch{},transitions{};};
inline std::array<ReloadEvidence,4> reloadEvidence{};
inline bool huntingTriggerSupported{};
inline bool opticalMovement{};
using StationaryScope=void(*)(void*,bool);
inline StationaryScope stationaryScope{};
inline StationaryScope huntingTransitions[2]{};
inline thread_local bool inHuntingTransition{};
template<unsigned I> void huntingTransitionHook(void* weapon,bool requested){
    const bool previous=inHuntingTransition;
    inHuntingTransition=true;
    huntingTransitions[I](weapon,opticalMovement?false:requested);
    inHuntingTransition=previous;
}
inline void stationaryScopeHook(void* player,bool requested){
 const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress())-image;
 stationaryScope(player,huntingScopeControls::stationary(requested,opticalMovement,caller,inHuntingTransition));
}
inline selectedShotgun::PumpCursor shotgunCursor;
inline std::uint64_t shotgunCursorTime{};
inline std::uint64_t serials[6]{};inline XrVector3f grabs[6]{};
inline manualReload::Visual visuals[6];inline Segment previousWorld[2]{};inline std::uintptr_t previousScene[2]{},previousPlayer[2]{};inline bool haveWorld[2]{};
inline manualReload::Alignment insertionAlignment[6];
using Update=void(*)(void*,void*,void*);using Fire=void(*)(void*);using ReloadFn=bool(*)(void*,bool);using Contact=void(*)(void*,void*);
inline Update updates[6]{};inline Fire fires[6]{};inline ReloadFn nativeReloads[6]{};inline Contact contacts[2]{};
inline unsigned transfers{},hits{},wallHits{};
inline std::atomic<unsigned> meleeActiveSamples{},meleeConvertedSamples{},meleeWorldQueries{},meleeWorldFailures{};
inline std::atomic<unsigned> meleeOwnedSamples{},meleeValidSamples{},meleeEntityFlags{};
inline std::atomic<unsigned> meleeReachBlocked{},meleeActorVisits{},meleeActorQueries{},meleeQueryContacts{},meleeDamageRejected{};
inline std::atomic<int> meleePreviousHealth{-1},meleeCurrentHealth{-1};
inline std::atomic<std::uintptr_t> meleeHealthTarget{};
inline std::atomic<unsigned> meleeHealthDrops{};
// Read-only evidence for the existing shared revolver firing hook.
inline std::atomic<unsigned> revolverFireCalls{},revolverFireBlocked{};
inline std::atomic<std::uintptr_t> revolverFireTable{};
inline std::atomic<std::uint64_t> shotTime[6]{};
using Ray=float*(*)(void*,float*);inline Ray rayAccessor{};
using WorldTrace=bool(*)(void*,float*,float*,float*,void**,bool);inline WorldTrace traceWorld{};
using SoundPlaying=bool(*)(void*);using SoundControl=void(*)(void*,int);
inline SoundPlaying soundPlaying{};inline SoundControl soundStop{},soundPlay{};
inline void playImpact(Kind kind,void* weapon){
    if(!soundPlaying||!soundStop||!soundPlay||!weapon||(kind!=Kind::Saw&&kind!=Kind::Nightstick))return;
    // Reuse each entity's own initialized impact voice and playback parameter.
    // These offsets/calls match 8F096..8F0C1 and 8CF12..8CF3E respectively.
    auto voice=static_cast<unsigned char*>(weapon)+(kind==Kind::Nightstick?0x140:0x180);
    if(!manualReload::readable(voice,0x28)||soundPlaying(voice))return;
    int parameter=*reinterpret_cast<int*>(voice+0x24);soundStop(voice,0);soundPlay(voice,parameter);
}
inline int index(int weapon){for(unsigned i=0;i<6;i++)if(static_cast<int>(profiles[i].kind)==weapon)return static_cast<int>(i);return -1;}
inline bool activeLocked(){auto now=GetTickCount64();const auto& h=manualReload::hands;
    bool active=installed&&h.session&&now>=h.time&&now-h.time<500;if(active)lastVrUse=h.time;return active;}
inline bool contactOwnedLocked(){
    if(activeLocked())return true;auto now=GetTickCount64();
    // Let queued native attack animations drain after VR loses focus. Native
    // desktop combat resumes once that short ownership window has expired.
    return installed&&lastVrUse&&now>=lastVrUse&&now-lastVrUse<500;
}
inline bool ownedLocked(unsigned i){return activeLocked()&&manualReload::hands.weapon==static_cast<int>(profiles[i].kind);}
inline bool consume(){std::lock_guard<std::mutex> lock(manualReload::mutex);int i=index(manualReload::hands.weapon);return i>=0&&i<4&&enabled&&ownedLocked(i)&&manualReload::hands.gameplay;}
inline bool block(){std::lock_guard<std::mutex> lock(manualReload::mutex);int i=index(manualReload::hands.weapon);return i>=0&&i<4&&enabled&&ownedLocked(i)&&(reloads[i].blocked()||(i==0&&cylinderTransition.value()>.001f));}
// MSH bind pivots transformed by each model calibration. The opened
// revolver cylinder also receives the same physical offset as its renderer.
struct Diagnostics {int weapon=-1,phase=-1;std::uint64_t age{};unsigned transfers{},hits{},wallHits{};bool session{},gameplay{},tracked{},toolActive{};};
inline Diagnostics diagnostics(std::uint64_t now){
    std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;Diagnostics d;
    d.weapon=h.weapon;int i=index(h.weapon);if(i>=0&&i<4)d.phase=static_cast<int>(reloads[i].state());
    if(h.weapon==1)d.phase=static_cast<int>(manualReload::gestures.transaction().stage());
    d.age=now>=h.time?now-h.time:~std::uint64_t{0};d.transfers=transfers;d.hits=hits;d.wallHits=wallHits;
    d.session=h.session;d.gameplay=h.gameplay;d.tracked=h.gunTracked;d.toolActive=melee[0].active()||melee[1].active();return d;
}
inline XrVector3f port(Kind k,float units=1){
    if(k==Kind::Sniper&&replacementHuntingContact.ready)return replacementHuntingContact.port;
    if(k==Kind::Shotgun&&replacementShotgunContact.ready)return replacementShotgunContact.port;
    if(k==Kind::Rifle&&replacementRifleContact.ready)return replacementRifleContact.port;
    if(k==Kind::Revolver&&replacementRevolverContact.ready)return replacementRevolverContact.port;
    auto p=k==Kind::Rifle?XrVector3f{.010498047f,-.072102487f,-.168990314f}:
        k==Kind::Revolver?XrVector3f{.005063355f,.026609898f,-.064526081f}:XrVector3f{.035f,-.025f,-.12f};
    p=manualReload::trackingAnchor(p,units);if(k==Kind::Revolver)p.x-=.035f;return p;
}
inline XrVector3f handle(Kind k,float units=1){if(k==Kind::Shotgun&&replacementShotgunContact.ready)return replacementShotgunHandle;if(k==Kind::Rifle&&replacementRifleContact.ready)return replacementRifleHandle;return manualReload::trackingAnchor(
    k==Kind::Shotgun?XrVector3f{-.001708984f,-.026235402f,-.290349066f}:XrVector3f{.035f,.035f,-.08f},units);}
inline void commit(unsigned i,void* entity){
    auto& r=reloads[i];auto token=r.token();if(!token)return;
    auto iface=static_cast<unsigned char*>(entity)+0xb8;auto table=*reinterpret_cast<std::uintptr_t*>(iface);
    if(table!=image+profiles[i].interfaceTable)return;
    using Get=int(*)(void*);using Set=void(*)(void*,int);
    auto fn=[&](unsigned slot){return *reinterpret_cast<std::uintptr_t*>(table+slot);};
    int a=reinterpret_cast<Get>(fn(0x70))(iface),b=reinterpret_cast<Get>(fn(0x88))(iface),cap=reinterpret_cast<Get>(fn(0x80))(iface);
    if(a<0||b<0||cap<=0||a>cap){r.acknowledge(token,false);return;}
    manualReload::Transfer result;if(!feed(a,b,cap,r.rounds(),result)){r.acknowledge(token,false);return;}
    if(result.amount>0){reinterpret_cast<Set>(fn(0x78))(iface,result.loaded);reinterpret_cast<Set>(fn(0x90))(iface,result.reserve);++transfers;}
    if(result.amount>0&&i==0)reloadAudio::play(0,reloadAudio::Action::SpeedloaderLoad);
    if(result.amount>0&&i==2)reloadAudio::play(3,reloadAudio::Action::ShellLoad);
    if(i==0&&loader.active)loader.count=0;
    r.acknowledge(token,true);vrFeedback::queue(0,.35f,20);
}
// The native enemy list and virtual contact/damage signatures are taken from
// HandheldEntityNightstick::contact (8ED50). No enemy health offsets are written.
inline bool worldLimit(float* ray,float& length,float clearance=.035f,bool* contact=nullptr){
    if(contact)*contact=false;
    if(!ray||!std::isfinite(length)||length<0||!std::isfinite(clearance)||clearance<0)return false;
    for(unsigned n=0;n<6;n++)if(!std::isfinite(ray[n]))return false;
    float norm=ray[3]*ray[3]+ray[4]*ray[4]+ray[5]*ray[5];
    if(norm<.98f||norm>1.02f)return false;
    const float maximum=length;
    auto level=*reinterpret_cast<unsigned char**>(image+0x3a5330);
    auto context=*reinterpret_cast<unsigned char**>(image+0x3fc480);
    if(!manualReload::readable(level,0x138)||!manualReload::readable(context,0x20))return false;
    auto scene=*reinterpret_cast<void**>(level+0x130);if(!manualReload::readable(scene,sizeof(void*)))return false;
    auto flags=reinterpret_cast<unsigned*>(context+0x1c);
    struct Restore {unsigned* p;unsigned high;~Restore(){*p=(*p&0x7fffffff)|high;}} restore{flags,*flags&0x80000000};
    *flags|=0x80000000;
    float point[3]{};void* object{};
    if(!traceWorld)return false;
    bool hit=traceWorld(scene,ray,&length,point,&object,true);
    if(!std::isfinite(length)||length<0)return false;
    if(contact)*contact=hit;
    length=(std::min)(length,maximum);
    if(hit)length=(std::max)(0.f,length-clearance);
    return true;
}
using ActorType=bool(*)(void*,unsigned);using ActorHealth=short(*)(void*);
inline bool sawEligible(void* actor,ActorType type,ActorHealth health){
    if(!actor||!type)return false;
    if(!type(actor,0x1020f))return true;
    // Native BoneSaw contact 8CE6F..8CEA2 uses this class test, health
    // accessor and transient contact lock before dispatching damage.
    return health&&manualReload::readable(actor,0x20a)&&health(actor)>0&&
        *reinterpret_cast<const short*>(static_cast<unsigned char*>(actor)+0x208)==0;
}
using ActorQuery=bool(*)(void*,float*,float*,float,float*,int*,int*);
using ActorDamage=bool(*)(void*,float*,float*,int,int,void*);
inline bool readEnemyHealth(void* actor,short& health){
    if(!image||!manualReload::readable(actor,sizeof(void*)))return false;
    auto table=*reinterpret_cast<std::uintptr_t*>(actor);
    if(table<image||table>image+0x400000||!manualReload::readable(reinterpret_cast<void*>(table),0xd0))return false;
    auto type=*reinterpret_cast<std::uintptr_t*>(table+8),get=*reinterpret_cast<std::uintptr_t*>(table+0xc8);
    if(type<image||type>=image+0x2e0000||get<image||get>=image+0x2e0000)return false;
    if(!reinterpret_cast<ActorType>(type)(actor,0x1020f))return false;
    health=reinterpret_cast<ActorHealth>(get)(actor);return true;
}
// Keep native contact values intact, but require a current gameplay decision
// after the collision callback and immediately before dispatching damage.
template<class Approve> bool applyContact(void* actor,ActorQuery query,ActorDamage damage,float* ray,float length,float radius,
    XrVector3f direction,void* weapon,Approve approve){
    if(!actor||!query||!damage||!ray||!std::isfinite(direction.x)||!std::isfinite(direction.y)||!std::isfinite(direction.z)||!std::isfinite(length)||length<0||!std::isfinite(radius)||radius<0)return false;
    float distance=length,point[3]{};int surface{},triangle{};
    meleeTrace::Event evidence{};evidence.actor=reinterpret_cast<std::uintptr_t>(actor);evidence.length=length;evidence.radius=radius;
    auto trace=[&](meleeTrace::Result result){if(!meleeTrace::buffer.enabled.load(std::memory_order_relaxed))return;evidence.time=GetTickCount64();evidence.result=static_cast<unsigned>(result);evidence.distance=distance;evidence.surface=surface;evidence.triangle=triangle;for(unsigned i=0;i<3;++i)evidence.point[i]=point[i];meleeTrace::buffer.push(evidence);};
    bool invalidContact=false;
    auto probe=[&](float* segment){
        distance=length;surface=triangle=0;point[0]=point[1]=point[2]=0;++meleeActorQueries;
        for(unsigned i=0;i<6;++i)evidence.ray[i]=segment[i];
        if(!query(actor,segment,&distance,radius,point,&surface,&triangle))return false;
        const bool valid=std::isfinite(distance)&&distance>=0&&distance<=length&&std::isfinite(point[0])&&std::isfinite(point[1])&&std::isfinite(point[2]);
        invalidContact|=!valid;return valid;
    };
    // The shaft is a volume, not a one-sided ray. Probe the same bounded
    // segment from both ends so entering from the tip is equally detectable.
    // The caller has already shortened this segment at world obstruction.
    if(!probe(ray)){
        float reverse[6]={ray[0]+ray[3]*length,ray[1]+ray[4]*length,ray[2]+ray[5]*length,-ray[3],-ray[4],-ray[5]};
        evidence.reverse=1;
        if(!probe(reverse)){trace(invalidContact?meleeTrace::Result::InvalidContact:meleeTrace::Result::Miss);return false;}
    }
    if(!approve()){trace(meleeTrace::Result::GateRejected);return false;}
    float impulse[]={direction.x,direction.y,direction.z};
    ++meleeQueryContacts;
    // Observe before damage only: a lethal callback may destroy its target.
    short health{};if(readEnemyHealth(actor,health)){
        evidence.health=health;
        auto target=reinterpret_cast<std::uintptr_t>(actor);
        int previous=meleeHealthTarget.load()==target?meleeCurrentHealth.load():-1;
        meleePreviousHealth=previous;meleeCurrentHealth=health;meleeHealthTarget=target;
        if(previous>=0&&health<previous)++meleeHealthDrops;
    }
    bool accepted=damage(actor,point,impulse,surface,triangle,weapon);
    if(!accepted)++meleeDamageRejected;
    trace(accepted?meleeTrace::Result::DamageAccepted:meleeTrace::Result::DamageRejected);
    return accepted;
}
inline void contactSweep(unsigned m,Kind kind,void* weapon,Segment old,Segment current,std::uint64_t now,float units=1){
    if(!std::isfinite(units)||units<.001f||units>100)return;
    {
        std::lock_guard<std::mutex> lock(manualReload::mutex);
        const auto& h=manualReload::hands;
        if(!meleeEnabled||!ownedLocked(m+4)||!h.gameplay||!h.gunTracked||
            GetTickCount64()<h.time||GetTickCount64()-h.time>=100||manualReload::menuState()!=0)return;
    }
    auto head=image+0x3e83e0;
    XrVector3f impulseDirection{current.tip.x-old.tip.x,current.tip.y-old.tip.y,current.tip.z-old.tip.z};
    float speed=manualReload::distance(impulseDirection,{0,0,0});if(speed>.001f){impulseDirection.x/=speed;impulseDirection.y/=speed;impulseDirection.z/=speed;}
    sweep(old,current,[&](Segment s){
        float length=manualReload::distance(s.base,s.tip);if(length<=.001f)return;
        float ray[6]={s.base.x,s.base.y,s.base.z,(s.tip.x-s.base.x)/length,(s.tip.y-s.base.y)/length,(s.tip.z-s.base.z)/length};
        float radius=(kind==Kind::Saw?selectedMeleeContact::sawRadius:.035f)*units;
        bool wall=false;
        ++meleeWorldQueries;
        if(!worldLimit(ray,length,radius,&wall)){++meleeWorldFailures;return;}
        // A controller pushed through a wall must not attack from its far side.
        float nativeRay[6]{};if(!rayAccessor)return;rayAccessor(static_cast<unsigned char*>(weapon)+0xb8,nativeRay);
        if(!std::isfinite(nativeRay[0])||!std::isfinite(nativeRay[1])||!std::isfinite(nativeRay[2]))return;
        float handDistance=manualReload::distance({nativeRay[0],nativeRay[1],nativeRay[2]},s.base);
        if(handDistance>.001f){
            float reach[6]={nativeRay[0],nativeRay[1],nativeRay[2],(s.base.x-nativeRay[0])/handDistance,(s.base.y-nativeRay[1])/handDistance,(s.base.z-nativeRay[2])/handDistance};
            float available=handDistance;if(!worldLimit(reach,available,.035f*units)||available+.001f<handDistance){++meleeReachBlocked;return;}
        }
        if(wall){
            bool impact=false;
            {
                std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;auto fresh=GetTickCount64();
                constexpr std::uintptr_t wallTarget=~std::uintptr_t{0};
                if(meleeEnabled&&ownedLocked(m+4)&&h.gameplay&&h.gunTracked&&fresh>=h.time&&fresh-h.time<100&&
                    manualReload::menuState()==0&&melee[m].canHit(wallTarget,now,kind)){
                    melee[m].landed(wallTarget,now);++wallHits;impact=true;vrFeedback::queue(h.primaryHand==0?0:1,kind==Kind::Saw?.25f:.5f,25);
                }
            }
            if(impact)playImpact(kind,weapon);
        }
        if(length<=.001f)return;
        auto link=*reinterpret_cast<std::uintptr_t*>(head);
        unsigned budget=0;
        while(link!=head&&budget++<4096){
            ++meleeActorVisits;
            if(!manualReload::readable(reinterpret_cast<void*>(link),16)||link<0x98)break;
            auto next=*reinterpret_cast<std::uintptr_t*>(link);auto enemy=reinterpret_cast<unsigned char*>(link-0x98);
            if(!manualReload::readable(enemy,16))break;
            auto vtable=*reinterpret_cast<std::uintptr_t*>(enemy);
            if(vtable<image||vtable>image+0x400000||!manualReload::readable(reinterpret_cast<void*>(vtable),0x58)){link=next;continue;}
            bool allowed;{std::lock_guard<std::mutex> lock(manualReload::mutex);allowed=melee[m].canHit(reinterpret_cast<std::uintptr_t>(enemy),now,kind);}
            if(allowed&&kind==Kind::Saw){
                auto type=*reinterpret_cast<std::uintptr_t*>(vtable+8);
                if(type<image||type>=image+0x2e0000)allowed=false;
                else{
                    ActorHealth health=nullptr;
                    if(manualReload::readable(reinterpret_cast<void*>(vtable),0xd0)){
                        auto accessor=*reinterpret_cast<std::uintptr_t*>(vtable+0xc8);
                        if(accessor>=image&&accessor<image+0x2e0000)health=reinterpret_cast<ActorHealth>(accessor);
                    }
                    allowed=sawEligible(enemy,reinterpret_cast<ActorType>(type),health);
                }
            }
            if(allowed||kind==Kind::Nightstick){
                auto query=*reinterpret_cast<std::uintptr_t*>(vtable+0x38),damage=*reinterpret_cast<std::uintptr_t*>(vtable+0x50);
                if(query>=image&&query<image+0x2e0000&&damage>=image&&damage<image+0x2e0000){
                    XrVector3f impulse=speed>.001f?impulseDirection:XrVector3f{ray[3],ray[4],ray[5]};
                    auto approve=[&]{
                        std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;auto fresh=GetTickCount64();
                        if(!meleeEnabled||!ownedLocked(m+4)||!h.gameplay||!h.gunTracked||fresh<h.time||fresh-h.time>=100||
                            manualReload::menuState()!=0)return false;
                        // Physical blocking survives per-stroke damage dedup and rest.
                        if(kind==Kind::Nightstick)meleeImpactStop::contact(trackedBefore[m],trackedCurrent[m],now,1);
                        return melee[m].canHit(reinterpret_cast<std::uintptr_t>(enemy),now,kind);
                    };
                    if(applyContact(enemy,reinterpret_cast<ActorQuery>(query),reinterpret_cast<ActorDamage>(damage),ray,length,radius,
                        impulse,static_cast<unsigned char*>(weapon)+0xb8,approve)){
                        {
                            std::lock_guard<std::mutex> lock(manualReload::mutex);
                            melee[m].landed(reinterpret_cast<std::uintptr_t>(enemy),now);
                            ++hits;vrFeedback::queue(manualReload::hands.primaryHand==0?0:1,kind==Kind::Saw?.3f:.7f,30);
                        }
                        playImpact(kind,weapon);
                    }
                }
            }
            link=next;
        }
    },units);
}
template<unsigned I> void updateHook(void* entity,void* step,void* context){
    dropSurface::service();
    Segment old{},world{};bool attack=false,contactCheck=false;float worldScale=1;std::uint64_t now=GetTickCount64();
    {
        std::lock_guard<std::mutex> lock(manualReload::mutex);auto h=manualReload::hands;auto kind=profiles[I].kind;
        bool valid=motionTracking::nearHead(h.gun,h.head)&&std::isfinite(h.scale)&&h.scale>=.001f&&h.scale<=100&&ownedLocked(I)&&h.gameplay&&manualReload::menuState()==0&&h.gunTracked&&now>=h.time&&now-h.time<100&&
            // Motion melee owns its attack decision. Native idle melee has
            // flags 0x0c: bit 0x10 belongs to the native attack path, not the
            // equipped visibility bit (0x08 used by the render hook).
            (*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(entity)+0x44)&(I>=4?0x08:0x18))==(I>=4?0x08:0x18);
        if constexpr(I<4){
            if(enabled){
                auto& r=reloads[I];auto local=motionTracking::relativePose(h.gun,h.left);auto pouch=motionTracking::relativePose(h.head,h.left);
                Event e;e.valid=valid&&h.tracked&&motionTracking::nearHead(h.left,h.head)&&motionTracking::validPose(h.left)&&
                    motionTracking::validPose(h.head)&&std::isfinite(h.scale)&&h.scale>=.001f&&h.scale<=100;e.release=h.release;e.grip=h.grip;
                e.atPouch=manualReload::pouchDistance(pouch.position)<.13f;
                if(e.valid){
                    using Get=int(*)(void*);auto iface=static_cast<unsigned char*>(entity)+0xb8;
                    int reserve=reinterpret_cast<Get>(image+profiles[I].ammo[3])(iface);
                    int loaded=reinterpret_cast<Get>(image+profiles[I].ammo[0])(iface);
                    // The hunting capacity override is on its interface slot;
                    // calling the shared original function bypasses it.
                    int capacity=I==3?5:reinterpret_cast<Get>(image+profiles[I].ammo[2])(iface);
                    reloadEvidence[I].loaded=loaded;reloadEvidence[I].reserve=reserve;reloadEvidence[I].capacity=capacity;
                    e.emptyOnGrab=loaded==0;
                    e.chamberLoaded=loaded>0&&loaded<=capacity;
                    e.atPouch=e.atPouch&&canGrabSpare(loaded,reserve,capacity,kind==Kind::Rifle||(kind==Kind::Sniper&&r.usesDetachableMagazine()));
                }
                auto insertionContact=kind==Kind::Revolver?replacementRevolverContact.contact(local):kind==Kind::Rifle?replacementRifleContact.contact(local):kind==Kind::Shotgun?replacementShotgunContact.contact(local):kind==Kind::Sniper&&r.usesDetachableMagazine()?replacementHuntingContact.contact(local):local.position;
                const float insertionDistance=manualReload::distance(insertionContact,port(kind,h.scale));
                e.atPort=insertionDistance<(kind==Kind::Revolver&&replacementRevolverContact.ready?.035f:.09f);
                e.seatContact=insertionDistance<=(kind==Kind::Revolver?.02f:.035f);
                if(kind==Kind::Revolver&&cylinderTransition.value()<.999f)e.atPort=e.seatContact=false;
                XrPosef displayLocal=local;
                const auto& alignmentContact=kind==Kind::Sniper?replacementHuntingContact:replacementRifleContact;
                bool aligning=e.valid&&(kind==Kind::Rifle||(kind==Kind::Sniper&&r.usesDetachableMagazine()))&&alignmentContact.ready&&r.state()==Phase::Held;
                const bool guidedMagazine=kind==Kind::Rifle||(kind==Kind::Sniper&&r.usesDetachableMagazine());
                bool aligned=insertionAlignment[I].update(local,insertionContact,alignmentContact.held,insertionDistance,now,aligning,displayLocal,guidedMagazine?&alignmentContact.port:nullptr,manualReload::magazineExitAxes[h.weapon],manualReload::magazineEntryClearance[h.weapon]);
                if(aligning&&!aligned)e.atPort=e.seatContact=false;
                if(aligned&&kind==Kind::Sniper){
                    // The well guide has already corrected lateral error and
                    // completed its outside-the-receiver orientation phase.
                    // Judge seating by that visible magazine contact, rather
                    // than requiring the raw controller to repeat the correction.
                    const float guidedDistance=manualReload::distance(alignmentContact.contact(displayLocal),alignmentContact.port);
                    e.atPort=guidedDistance<.09f;
                    e.seatContact=guidedDistance<=.035f;
                }
                auto gripPoint=handle(kind,h.scale);if(kind==Kind::Sniper)gripPoint.z+=r.boltOffset();
                auto mechanismContact=(kind==Kind::Rifle||kind==Kind::Shotgun)?insertionContact:local.position;
                // The pump is held by the support controller, not the shell's
                // fingertip insertion offset. Measure before two-hand aim so
                // the aim correction cannot absorb the physical pump stroke.
                const bool supportedPump=kind==Kind::Shotgun&&h.supportHeld;
                e.supportHeld=supportedPump;
                if(supportedPump)mechanismContact=h.supportPumpPosition;
                e.atHandle=manualReload::distance(mechanismContact,gripPoint)<.09f;
                if(supportedPump)e.atHandle=true;
                e.travel=supportedPump?supportPumpTravel::travel(h.supportPumpOrigin,mechanismContact):mechanismContact.z-grabs[I].z;
                if(supportedPump)e.valid=e.valid&&supportPumpTravel::valid(h.supportPumpOrigin,mechanismContact);
                else if(reloads[I].state()==Phase::Charging&&manualReload::distance({mechanismContact.x,mechanismContact.y,0},{grabs[I].x,grabs[I].y,0})>.15f)e.valid=false;
                e.valid=e.valid&&motionTracking::validPose(local)&&manualReload::distance(local.position,{0,0,0})<2;
                if(kind==Kind::Shotgun&&selectedShotgun::motion.ready){
                    float elapsed=shotgunCursorTime&&now>=shotgunCursorTime?float(now-shotgunCursorTime)/1000.f:0;
                    shotgunCursorTime=now;
                    shotgunCursor.update(selectedShotgun::motion,e.travel/.084087276f,elapsed,e.valid&&r.state()==Phase::Charging);
                    e.cycleComplete=shotgunCursor.complete();
                }
                if(h.serial!=serials[I]||!e.valid){
                    auto& evidence=reloadEvidence[I];evidence.event=e;evidence.pouch=manualReload::pouchDistance(pouch.position);
                    if(!e.valid)++evidence.invalid;if(e.atPouch)++evidence.pouchSamples;if(e.atPouch&&e.grip)++evidence.gripPouch;
                    if constexpr(I==0)e.closeFlick=cylinderFlickDetector.update(h.gun.orientation,cylinderClosingAxis,h.time,
                        e.valid&&r.state()==Phase::Open&&cylinderTransition.value()>=.999f);
                    auto before=r.state();r.update(kind,e);serials[I]=h.serial;
                    if(before!=Phase::Charging&&r.state()==Phase::Charging){
                        grabs[I]=supportedPump?h.supportPumpOrigin:mechanismContact;
                        e.travel=supportedPump?supportPumpTravel::travel(h.supportPumpOrigin,mechanismContact):mechanismContact.z-grabs[I].z;
                    }
                    static bool pullSound=false,returnSound=false;static float peakPull=0;
                    const auto after=r.state();
                    if(before!=after)++reloadEvidence[I].transitions;
                    if(kind==Kind::Shotgun){
                        static pumpSounds::Stroke sounds;
                        const auto events=sounds.update(e.valid,e.grip,before==Phase::Charging||after==Phase::Charging,e.travel);
                        if(events&pumpSounds::Stroke::Pull)reloadAudio::play(3,reloadAudio::Action::MechanismPull);
                        if(events&pumpSounds::Stroke::Return)reloadAudio::play(3,reloadAudio::Action::MechanismRelease);
                    }
                    if(e.valid){
                        using A=reloadAudio::Action;const unsigned w=unsigned(kind);
                        if((kind==Kind::Rifle||(kind==Kind::Sniper&&r.usesDetachableMagazine()))&&after==Phase::Open&&(before==Phase::Ready||before==Phase::Inserted||before==Phase::Charging))reloadAudio::play(w,A::MagazineDrop);
                        if((kind==Kind::Rifle||kind==Kind::Sniper)&&before==Phase::Held&&(after==Phase::Inserted||after==Phase::Pending))reloadAudio::play(w,A::MagazineInsert);
                        if(kind==Kind::Revolver&&before==Phase::Ready&&after==Phase::Open)reloadAudio::play(w,A::CylinderOpen);
                        if(kind==Kind::Revolver&&before!=Phase::Ready&&after==Phase::Ready)reloadAudio::play(w,A::CylinderClose);
                        if(after==Phase::Charging&&before!=after){pullSound=false;returnSound=false;peakPull=0;}
                        if(kind!=Kind::Shotgun&&after==Phase::Charging&&!pullSound&&e.travel>.01f){reloadAudio::play(w,A::MechanismPull);pullSound=true;}
                        if(after==Phase::Charging)peakPull=(std::max)(peakPull,e.travel);
                        if(before==Phase::Charging&&pullSound&&!returnSound&&
                           ((kind==Kind::Shotgun&&(after==Phase::Pending||(peakPull>=.07f&&e.travel<peakPull-.01f)))||(!e.grip&&after!=before))){reloadAudio::play(w,A::MechanismRelease);returnSound=true;}
                    }else {pullSound=false;returnSound=false;peakPull=0;}
                    if constexpr(I==0){
                        if(!e.valid||!e.grip||r.state()==Phase::Ready)loader={};
                        else if(before!=Phase::Held&&r.state()==Phase::Held){
                            using Get=int(*)(void*);auto iface=static_cast<unsigned char*>(entity)+0xb8;
                            int a=reinterpret_cast<Get>(image+profiles[0].ammo[0])(iface),b=reinterpret_cast<Get>(image+profiles[0].ammo[3])(iface);
                            if(a>=0&&a<=6&&b>=0)loader={true,unsigned(a),unsigned((std::min)(6-a,b))};
                        }
                    }
                    // Magazine insertion cannot play the composite rifle reload
                    // recording, which includes charging-handle movement.
                    if(before!=r.state()&&e.valid)vrFeedback::queue(0,.2f,15);
                }
                const bool handleHand=e.valid&&kind==Kind::Rifle&&r.state()==Phase::Charging;
                if(handleHand)displayLocal=rifleHandleHand(local,mechanismContact,grabs[I],replacementRiflePullAxis,e.travel);
                visuals[I].leftInGun=displayLocal;visuals[I].alignedHand=handleHand||insertionAlignment[I].active();visuals[I].slidePull=std::clamp((kind==Kind::Sniper?r.boltOffset():0.f)+(r.state()==Phase::Charging?e.travel:0.f),0.f,.12f);visuals[I].scale=h.scale;visuals[I].valid=e.valid;
                if(e.valid)commit(I,entity);
                if constexpr(I==0)cylinderTransition.update(r.state()!=Phase::Ready,now);
            }
        }else{
            constexpr unsigned M=I-4;
            if(meleeEnabled){
                if(ownedLocked(I)){++meleeOwnedSamples;meleeEntityFlags=*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(entity)+0x44);if(valid)++meleeValidSamples;}
                // Velocity is measured in tracking space, so joystick walking
                // or turning cannot manufacture a swing.
                Segment local;selectedMeleeContact::segment(M,h.gun,local.base,local.tip);
                trackedBefore[M]=trackedHistory[M]?trackedPrevious[M]:local;trackedCurrent[M]=local;
                trackedPrevious[M]=local;trackedHistory[M]=valid;
                attack=melee[M].update(kind,local,h.time,valid,h.trigger);
                if(attack)++meleeActiveSamples;
                bool converted=false;
                if(valid){
                    motionTracking::Sample sample;
                    auto level=*reinterpret_cast<unsigned char**>(image+0x3a5330);
                    auto scene=manualReload::readable(level,0x138)?*reinterpret_cast<std::uintptr_t*>(level+0x130):0;
                    auto player=*reinterpret_cast<unsigned char**>(image+0x3a5340);
                    if(scene&&motionTracking::read(now,sample)&&sample.time==h.time&&rayAccessor&&manualReload::readable(player,0xc0)&&*reinterpret_cast<std::uintptr_t*>(player+0xb8)==image+0x30bff8){
                        // Match nativeMotion::pose: tracked controllers use
                        // camera reference axes, not the player's model axes.
                        // The model basis may point backwards from the camera.
                        auto camera=*reinterpret_cast<unsigned char**>(image+0x3a5338);
                        float body[12],ray[6]{},view[12];
                        bool cameraValid=manualReload::readable(camera,0xbc+sizeof view);
                        if(cameraValid)std::memcpy(view,camera+0xbc,sizeof view);
                        if(cameraValid&&motionTracking::cameraWorldBasis(view,body)){
                            rayAccessor(static_cast<unsigned char*>(entity)+0xb8,ray);
                            XrVector3f start,tip;selectedMeleeContact::segment(M,sample.rightAim,start,tip);
                            world.base=motionTracking::worldPoint(body,{ray[0],ray[1],ray[2]},start,h.scale);
                            world.tip=motionTracking::worldPoint(body,{ray[0],ray[1],ray[2]},tip,h.scale);
                            if(std::isfinite(world.base.x)&&std::isfinite(world.base.y)&&std::isfinite(world.base.z)&&
                                std::isfinite(world.tip.x)&&std::isfinite(world.tip.y)&&std::isfinite(world.tip.z)){
                                if(haveWorld[M]&&(previousScene[M]!=scene||previousPlayer[M]!=reinterpret_cast<std::uintptr_t>(player))){melee[M].reset();attack=false;haveWorld[M]=false;}
                                old=haveWorld[M]?previousWorld[M]:world;previousWorld[M]=world;haveWorld[M]=true;converted=true;worldScale=h.scale;
                                previousScene[M]=scene;previousPlayer[M]=reinterpret_cast<std::uintptr_t>(player);
                            }
                        }else attack=false;
                    }else attack=false;
                }
                if(!converted){attack=false;haveWorld[M]=false;}
                else {contactCheck=kind==Kind::Nightstick;if(attack)++meleeConvertedSamples;}
            }
        }
    }
    int rifleBefore=-1;unsigned rifleSerial{};
    if constexpr(I==1){
        std::lock_guard<std::mutex> lock(manualReload::mutex);
        auto bytes=static_cast<unsigned char*>(entity);const auto& h=manualReload::hands;
        bool eligible=rifleCadenceSupported&&enabled&&ownedLocked(I)&&h.gameplay&&h.gunTracked&&
            now>=h.time&&now-h.time<100&&manualReload::menuState()==0&&!reloads[I].blocked()&&
            (*reinterpret_cast<unsigned*>(bytes+0x44)&0x18)==0x18;
        auto animation=*reinterpret_cast<unsigned char**>(bytes+0x50);
        if(eligible&&manualReload::readable(animation,0x18)){
            const auto state=*reinterpret_cast<unsigned short*>(bytes+0xd0);
            auto frame=reinterpret_cast<unsigned*>(animation);
            int replacement=rifleClock.beforeNativeFrame(state,*frame,now,true);
            if(replacement>=0)*frame=unsigned(replacement);
            using Get=int(*)(void*);rifleBefore=reinterpret_cast<Get>(image+profiles[I].ammo[0])(bytes+0xb8);
        }else rifleClock.reset();
        rifleSerial=rifleClock.shots;
    }
    if constexpr(I==3){
        bool trigger=false;
        {std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;
            auto bytes=static_cast<unsigned char*>(entity);using Get=int(*)(void*);
            bool valid=huntingTriggerSupported&&enabled&&ownedLocked(I)&&h.gameplay&&h.gunTracked&&
                now>=h.time&&now-h.time<100&&manualReload::menuState()==0&&!reloads[I].blocked()&&
                (*reinterpret_cast<unsigned*>(bytes+0x44)&0x18)==0x18&&
                reinterpret_cast<Get>(image+profiles[I].ammo[0])(bytes+0xb8)>0;
            trigger=huntingTriggerEdge.update(gamepadBridge::firePressSerial.load(),valid);
        }
        if(trigger&&soundPlaying&&soundStop&&*reinterpret_cast<std::uintptr_t*>(entity)==image+profiles[I].entity){
            // Native hunting Fire refuses a fresh pull while the previous
            // gunshot voice is playing. Release that voice through its native
            // API; normal Fire still performs every shot/debit and restarts
            // the original sound. Never stop reload audio or clear ammo state.
            auto voice=static_cast<unsigned char*>(entity)+0x108;
            if(manualReload::readable(voice,0x28)&&soundPlaying(voice))soundStop(voice,0);
        }
    }
    if constexpr(I==0){
        bool trigger=false;
        {std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;
            auto bytes=static_cast<unsigned char*>(entity);using Get=int(*)(void*);
            bool valid=revolverTriggerSupported&&enabled&&ownedLocked(I)&&h.gameplay&&h.gunTracked&&
                now>=h.time&&now-h.time<100&&manualReload::menuState()==0&&!reloads[I].blocked()&&cylinderTransition.value()<=.001f&&
                (*reinterpret_cast<unsigned*>(bytes+0x44)&0x18)==0x18&&
                reinterpret_cast<Get>(image+profiles[I].ammo[0])(bytes+0xb8)>0;
            trigger=revolverTriggerEdge.update(gamepadBridge::firePressSerial.load(),valid);
        }
        if(trigger&&*reinterpret_cast<std::uintptr_t*>(entity)==image+profiles[I].entity){
            auto bytes=static_cast<unsigned char*>(entity);auto state=*reinterpret_cast<unsigned short*>(bytes+0xd0);
            auto animation=*reinterpret_cast<unsigned char**>(bytes+0x50);
            if((state==0||state==2)&&manualReload::readable(animation,0x18)){
                // The recorded native revolver emits its shot at frame6.
                // Start its own firing clip, then let native Update advance
                // to that existing event (damage, debit, sound and flash).
                auto table=*reinterpret_cast<std::uintptr_t**>(entity);
                reinterpret_cast<void(*)(void*,unsigned,void*)>(table[0xe8/sizeof(std::uintptr_t)])(entity,2,nullptr);
                animation=*reinterpret_cast<unsigned char**>(bytes+0x50);
                if(*reinterpret_cast<unsigned short*>(bytes+0xd0)==2&&manualReload::readable(animation,0x18))*reinterpret_cast<unsigned*>(animation)=5;
            }
        }
    }
    auto observe=[&]{if constexpr(I<4){std::lock_guard<std::mutex> lock(manualReload::mutex);
        const auto& h=manualReload::hands;bool valid=enabled&&ownedLocked(I)&&h.gameplay&&h.gunTracked&&manualReload::menuState()==0&&
            (*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(entity)+0x44)&0x18)==0x18;
        using Get=int(*)(void*);auto iface=static_cast<unsigned char*>(entity)+0xb8;
        int a=valid?reinterpret_cast<Get>(image+profiles[I].ammo[0])(iface):0;
        int b=valid?reinterpret_cast<Get>(image+profiles[I].ammo[3])(iface):0;
        weaponShots::observers[unsigned(profiles[I].kind)].sample(a,b,GetTickCount64(),valid);}};
    observe();updates[I](entity,step,context);observe();
    if constexpr(I==1){
        if(rifleBefore>=0){using Get=int(*)(void*);int after=reinterpret_cast<Get>(image+profiles[I].ammo[0])(static_cast<unsigned char*>(entity)+0xb8);
            if(after<rifleBefore){std::lock_guard<std::mutex> lock(manualReload::mutex);
                if(rifleClock.shots==rifleSerial)rifleClock.shot(GetTickCount64());shotTime[I]=GetTickCount64();}}
    }
    if constexpr(I>=4){if(attack||contactCheck)contactSweep(I-4,profiles[I].kind,entity,old,world,now,worldScale);}
}
template<unsigned I> void fireHook(void* weapon){
    bool blocked=false;{std::lock_guard<std::mutex> lock(manualReload::mutex);
        bool matches=*reinterpret_cast<std::uintptr_t*>(weapon)==image+profiles[I].interfaceTable;
        if constexpr(I==0){++revolverFireCalls;revolverFireTable=*reinterpret_cast<std::uintptr_t*>(weapon)-image;}
        blocked=enabled&&matches&&ownedLocked(I)&&(!manualReload::hands.gunTracked||!manualReload::hands.gameplay||
            GetTickCount64()<manualReload::hands.time||GetTickCount64()-manualReload::hands.time>=100||
            manualReload::menuState()!=0||reloads[I].blocked()||(I==0&&cylinderTransition.value()>.001f)||
            (I==1&&rifleCadenceSupported&&!rifleClock.ready(GetTickCount64())));}
    if constexpr(I==0)if(blocked)++revolverFireBlocked;
    if(!blocked){
        int before=-1;using Get=int(*)(void*);
        if constexpr(I==1||I==2||I==3)before=reinterpret_cast<Get>(image+profiles[I].ammo[0])(weapon);
        fires[I](weapon);
        if constexpr(I==1){int after=reinterpret_cast<Get>(image+profiles[I].ammo[0])(weapon);
            if(after<before){std::lock_guard<std::mutex> lock(manualReload::mutex);if(enabled&&ownedLocked(I)){rifleClock.shot(GetTickCount64());shotTime[I]=GetTickCount64();}}}
        if constexpr(I==2||I==3){
            int after=reinterpret_cast<Get>(image+profiles[I].ammo[0])(weapon);
            if(after<before){shotTime[I]=GetTickCount64();std::lock_guard<std::mutex> lock(manualReload::mutex);if(enabled&&ownedLocked(I))reloads[I].requireCycle();}
        }
    }
}
template<unsigned I> bool reloadHook(void* weapon,bool automatic){
    {std::lock_guard<std::mutex> lock(manualReload::mutex);
        auto entity=static_cast<unsigned char*>(weapon)-0xb8;
        if(enabled&&activeLocked()&&(*reinterpret_cast<unsigned*>(entity+0x44)&0x18)==0x18)return false;}
    return nativeReloads[I](weapon,automatic);
}
template<unsigned M> void contactHook(void* entity,void* step){
    {std::lock_guard<std::mutex> lock(manualReload::mutex);if(meleeEnabled&&contactOwnedLocked()&&*reinterpret_cast<std::uintptr_t*>(entity)==image+profiles[M+4].entity)return;}
    contacts[M](entity,step);
}
inline void align(unsigned i,float* matrix){
    float old[12];std::memcpy(old,matrix,sizeof old);auto p=profiles[i].anchor;
    for(unsigned row=0;row<3;row++){
        for(unsigned c=0;c<3;c++){matrix[row*4+c]=0;for(unsigned k=0;k<3;k++)matrix[row*4+c]+=old[row*4+k]*calibration[i][k*3+c];}
        matrix[row*4+3]-=matrix[row*4]*p.x+matrix[row*4+1]*p.y+matrix[row*4+2]*p.z;
    }
}
inline XrVector3f modelOffset(unsigned i,XrVector3f p){const auto* r=calibration[i];return {r[0]*p.x+r[3]*p.y+r[6]*p.z,r[1]*p.x+r[4]*p.y+r[7]*p.z,r[2]*p.x+r[5]*p.y+r[8]*p.z};}
// Bone quaternions use DirectX row vectors; calibration maps model columns
// into controller coordinates. Conjugate the hand rotation through that basis.
inline void rotatePart(unsigned i,manualReload::Bone& bone,const XrQuaternionf& hand){
    using namespace DirectX;
    const auto* c=calibration[i];
    XMMATRIX basis=XMMatrixSet(c[0],c[3],c[6],0,c[1],c[4],c[7],0,c[2],c[5],c[8],0,0,0,0,1);
    XrQuaternionf original{bone.q[0]/4096.f,bone.q[1]/4096.f,bone.q[2]/4096.f,bone.q[3]/4096.f};
    auto transformed=motionTracking::rotation(original)*basis*motionTracking::rotation(hand)*XMMatrixTranspose(basis);
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(transformed)));
    float values[]={q.x,q.y,q.z,q.w};
    for(unsigned n=0;n<4;n++)bone.q[n]=static_cast<std::int32_t>(std::lround(values[n]*4096));
}
class Parts {
    manualReload::Bone* target{};manualReload::Bone saved[12]{};std::uint32_t* mask{};std::uint32_t oldMask{};unsigned count{};
public:
    Parts(unsigned i,void* entity){
        if(i>=4)return;std::lock_guard<std::mutex> lock(manualReload::mutex);
        if(!enabled||!ownedLocked(i)||!visuals[i].valid||reloads[i].state()==Phase::Ready||
            !std::isfinite(visuals[i].scale)||visuals[i].scale<.001f||visuals[i].scale>100||
            !motionTracking::validPose(visuals[i].leftInGun)||!std::isfinite(visuals[i].slidePull))return;
        auto& p=profiles[i];auto animation=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(entity)+0x50);
        if(!manualReload::readable(animation,0x34+p.bones*sizeof(manualReload::Bone))||animation[0x10]!=p.bones||p.bones>12)return;
        auto bones=reinterpret_cast<manualReload::Bone*>(animation+0x34);
        for(unsigned b=0;b<p.bones;b++)if(!manualReload::validBone(bones[b]))return;
        target=bones;count=p.bones;std::memcpy(saved,target,count*sizeof(*target));mask=reinterpret_cast<std::uint32_t*>(animation+0x18);oldMask=*mask;
        auto phase=reloads[i].state();auto& v=visuals[i];
        if(p.kind==Kind::Rifle&&p.magazine>=0){
            auto& mag=bones[p.magazine];*mask|=1u<<p.magazine;
            if(phase==Phase::Open)mag.p[1]=-40960000;
            if(phase==Phase::Held){rotatePart(i,mag,v.leftInGun.orientation);
                auto offset=modelOffset(i,v.leftInGun.position);
                mag.p[0]=static_cast<int>((p.anchor.x+offset.x*v.scale)*4096);
                mag.p[1]=static_cast<int>((p.anchor.y+offset.y*v.scale)*4096);
                mag.p[2]=static_cast<int>((p.anchor.z+offset.z*v.scale)*4096);}
        }
        if(p.kind==Kind::Revolver&&p.magazine>=0){
            auto& cylinder=bones[p.magazine];rotatePart(i,cylinder,{0,0,.5735764f,.819152f});
            auto offset=modelOffset(i,{-.035f*v.scale,0,0});
            cylinder.p[0]+=static_cast<int>(offset.x*4096);cylinder.p[1]+=static_cast<int>(offset.y*4096);cylinder.p[2]+=static_cast<int>(offset.z*4096);*mask|=1u<<p.magazine;
        }
        if(p.kind==Kind::Shotgun&&phase==Phase::Charging&&p.mechanism>=0){auto offset=modelOffset(i,{0,0,v.slidePull*v.scale});
            bones[p.mechanism].p[0]+=static_cast<int>(offset.x*4096);bones[p.mechanism].p[1]+=static_cast<int>(offset.y*4096);bones[p.mechanism].p[2]+=static_cast<int>(offset.z*4096);*mask|=1u<<p.mechanism;}
    }
    ~Parts(){if(target){std::memcpy(target,saved,count*sizeof(*target));*mask=oldMask;}}
    Parts(const Parts&)=delete;Parts& operator=(const Parts&)=delete;
};
inline bool verify(std::uintptr_t base){
    for(unsigned i=0;i<3;i++)if(std::memcmp(reinterpret_cast<void*>(base+impactAudioRva[i]),impactAudioCode[i].data(),16))return false;
    if(std::memcmp(reinterpret_cast<void*>(base+0xf2090),worldCode.data(),16))return false;
    for(unsigned i=0;i<6;i++){
        const auto& p=profiles[i];
        if(std::memcmp(reinterpret_cast<void*>(base+p.update),p.updateCode.data(),16))return false;
        if(p.fire&&std::memcmp(reinterpret_cast<void*>(base+p.fire),p.fireCode.data(),16))return false;
        if(p.reload&&std::memcmp(reinterpret_cast<void*>(base+p.reload),p.reloadCode.data(),16))return false;
        if(*reinterpret_cast<std::uintptr_t*>(base+p.entity+0x20)!=base+p.update)return false;
        if(i<4){unsigned slots[]={0x70,0x78,0x80,0x88,0x90};for(unsigned j=0;j<5;j++)if(*reinterpret_cast<std::uintptr_t*>(base+p.interfaceTable+slots[j])!=base+p.ammo[j])return false;}
    }
    for(unsigned m=0;m<2;m++)if(std::memcmp(reinterpret_cast<void*>(base+nativeContact[m]),contactCode[m].data(),16))return false;
    return true;
}
inline int huntingMagazineCapacity(void*){return 5;}
inline bool install(){
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));if(!verify(image))return false;
    wchar_t traceValue[8]{};meleeTrace::buffer.enabled=GetEnvironmentVariableW(L"DEMENTIUM_MELEE_TRACE",traceValue,8)==1&&traceValue[0]==L'1';
    // Gate every animation-frame assumption against the retained Steam build.
    const unsigned char increment[]={0x41,0x8b,0x01,0x41,0x0f,0xba,0xf0,0x11,0xff,0xc0};
    const unsigned char repeating[]={0xf7,0xc1,0xf7,0xff,0xff,0xff,0x75,0x78};
    const unsigned char initial[]={0x83,0x38,0x16,0x75,0x3d};
    rifleCadenceSupported=!std::memcmp(reinterpret_cast<void*>(image+0x7c82f),increment,sizeof increment)&&
        !std::memcmp(reinterpret_cast<void*>(image+0x8c3f2),repeating,sizeof repeating)&&
        !std::memcmp(reinterpret_cast<void*>(image+0x8c397),initial,sizeof initial);
    const unsigned char revolverEvent[]={0x83,0x38,0x06,0x75,0x52};
    revolverTriggerSupported=!std::memcmp(reinterpret_cast<void*>(image+0x7c82f),increment,sizeof increment)&&
        !std::memcmp(reinterpret_cast<void*>(image+0x8ade8),revolverEvent,sizeof revolverEvent);
    const unsigned char huntingVoiceGate[]={0x48,0x8d,0x4b,0x50,0xe8,0xff,0xc8,0x06,0x00,0x84,0xc0};
    huntingTriggerSupported=!std::memcmp(reinterpret_cast<void*>(image+0x90d98),huntingVoiceGate,sizeof huntingVoiceGate);
    traceWorld=reinterpret_cast<WorldTrace>(image+0xf2090);
    soundPlaying=reinterpret_cast<SoundPlaying>(image+impactAudioRva[0]);
    soundStop=reinterpret_cast<SoundControl>(image+impactAudioRva[1]);soundPlay=reinterpret_cast<SoundControl>(image+impactAudioRva[2]);
    Update u[]={updateHook<0>,updateHook<1>,updateHook<2>,updateHook<3>,updateHook<4>,updateHook<5>};
    Fire f[]={fireHook<0>,fireHook<1>,fireHook<2>,fireHook<3>};ReloadFn r[]={reloadHook<0>,reloadHook<1>,reloadHook<2>,reloadHook<3>};
    std::vector<void*> created;
    auto add=[&](std::uintptr_t rva,void* hook,void** original){auto address=reinterpret_cast<void*>(image+rva);if(MH_CreateHook(address,hook,original)!=MH_OK)return false;created.push_back(address);return true;};
    bool ok=true;
    for(unsigned i=0;i<6&&ok;i++){ok=add(profiles[i].update,reinterpret_cast<void*>(u[i]),reinterpret_cast<void**>(&updates[i]));
        if(i<4&&ok)ok=add(profiles[i].fire,reinterpret_cast<void*>(f[i]),reinterpret_cast<void**>(&fires[i]));
        if(i<4&&ok)ok=add(profiles[i].reload,reinterpret_cast<void*>(r[i]),reinterpret_cast<void**>(&nativeReloads[i]));}
    if(ok)ok=add(nativeContact[0],reinterpret_cast<void*>(contactHook<0>),reinterpret_cast<void**>(&contacts[0]));
    if(ok)ok=add(nativeContact[1],reinterpret_cast<void*>(contactHook<1>),reinterpret_cast<void**>(&contacts[1]));
    if(ok&&opticalMovement){
        const unsigned char entry[]={0x40,0x53,0x48,0x81,0xec,0xf0,0,0,0,0x4c,0x8b,0xc1};
        const unsigned char calls[3][5]={{0xe8,0x5a,0xb8,2,0},{0xe8,0x80,0xb6,2,0},{0xe8,0xc8,0xb4,2,0}};
        const std::uintptr_t sites[]={0x90a01,0x90bdb,0x90d93};
        ok=!std::memcmp(reinterpret_cast<void*>(image+0xbc260),entry,sizeof entry);
        for(unsigned i=0;i<3&&ok;++i)ok=!std::memcmp(reinterpret_cast<void*>(image+sites[i]),calls[i],5);
        // Door/equipment transitions tail-jump to the stationary function,
        // so its return address belongs to their caller. Preserve the entire
        // native transition and explicitly carry its hunting-only context.
        const unsigned char tail0[]={0xe9,0xc6,0xb5,2,0},tail1[]={0xe9,0x2b,0xb5,2,0};
        const unsigned char entry0[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x05,0x13,0x76,0x2e,0};
        const unsigned char entry1[]={0x40,0x53,0x48,0x83,0xec,0x20,0x33,0xc0};
        if(ok)ok=!std::memcmp(reinterpret_cast<void*>(image+0x90c95),tail0,sizeof tail0)&&!std::memcmp(reinterpret_cast<void*>(image+0x90d30),tail1,sizeof tail1)&&!std::memcmp(reinterpret_cast<void*>(image+0x90c30),entry0,sizeof entry0)&&!std::memcmp(reinterpret_cast<void*>(image+0x90ca0),entry1,sizeof entry1);
        if(ok)ok=add(0xbc260,reinterpret_cast<void*>(stationaryScopeHook),reinterpret_cast<void**>(&stationaryScope));
        if(ok)ok=add(0x90c30,reinterpret_cast<void*>(huntingTransitionHook<0>),reinterpret_cast<void**>(&huntingTransitions[0]));
        if(ok)ok=add(0x90ca0,reinterpret_cast<void*>(huntingTransitionHook<1>),reinterpret_cast<void**>(&huntingTransitions[1]));
    }
    // Only the hunting interface changes. Its native loaded-ammo setter also
    // queries this slot, so manual insertion and native clamping agree on five.
    if(ok){
        auto slot=reinterpret_cast<std::uintptr_t*>(image+profiles[3].interfaceTable+0x80);
        DWORD protection{};
        ok=VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&protection)!=0;
        if(ok){*slot=reinterpret_cast<std::uintptr_t>(&huntingMagazineCapacity);DWORD unused{};VirtualProtect(slot,sizeof(*slot),protection,&unused);}
    }
    if(!ok){for(auto address:created)MH_RemoveHook(address);return false;}installed=true;return true;
}
}

