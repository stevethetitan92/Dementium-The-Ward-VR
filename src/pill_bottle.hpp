#pragma once
#include "arsenal_runtime.hpp"
#include "pill_hold_core.hpp"
#include "medicine_inventory.hpp"
#include "medicine_effect.hpp"
#include "wristband_mesh.hpp"
#include "native_hand_clip.hpp"
#include "native_pickup_sector.hpp"
#include "support_aim.hpp"
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <cfloat>
#include <array>
namespace pillBottle {
struct Record {void* entity{};std::uintptr_t scene{};std::uint64_t time{};};
inline std::mutex mutex;inline std::unordered_map<int,Record> records;inline pillHold::Hold hold;
inline int heldId=-1;inline Record held;inline std::uintptr_t image{};inline bool installed{};
inline std::atomic<bool> holding{};
inline std::atomic<int> heldSide{-1};
struct Controls {bool valid{},grip[2]{},trigger[2]{},rightFree{};std::uint64_t time{};};
inline Controls controls;
inline constexpr XrVector3f pocketAnchor{0.f,-.55f,-.12f};
inline void publishControls(const Controls& value){std::lock_guard<std::mutex> lock(mutex);controls=value;}
inline bool previousRightGrip{};
inline medicine::Inventory supply;inline std::atomic<unsigned> carried{};inline std::atomic<bool> useRequested{};
inline bool pocketHeld{},previousGrip{};inline void(*restorePending)(){};
inline std::atomic<std::uintptr_t> useScene{},usePlayer{};inline std::atomic<std::uint64_t> useTime{};
inline void requestUse(){if(!installed||!image)return;useScene=*reinterpret_cast<std::uintptr_t*>(image+0x3a5330);usePlayer=*reinterpret_cast<std::uintptr_t*>(image+0x3a5340);useTime=GetTickCount64();useRequested=true;}

inline std::vector<wristband::Vertex> geometry;
inline bool load(const std::filesystem::path& file){
    geometry.clear();std::ifstream f(file,std::ios::binary|std::ios::ate);if(!f||f.tellg()!=static_cast<std::streamoff>(3840*sizeof(wristband::Vertex)))return false;f.seekg(0);
    std::vector<wristband::Vertex> candidate(3840);f.read(reinterpret_cast<char*>(candidate.data()),candidate.size()*sizeof(wristband::Vertex));if(!f)return false;
    for(auto& v:candidate){const float* p=&v.x;for(unsigned i=0;i<12;i++)if(!std::isfinite(p[i]))return false;if(std::fabs(v.x)>.1f||std::fabs(v.y)>.1f||std::fabs(v.z)>.1f)return false;}
    geometry=std::move(candidate);return true;
}
using Apply=bool(*)(int,void*);using Render=void(*)(void*,void*,unsigned,void*);using Remove=void(*)(void*);
inline Apply originalApply{};inline Render originalRender{};inline Remove removePickup{};
inline void(*applyEffect)(int,void*){};
inline std::atomic<bool> verifyEffectRequested{};
inline void(*effectVerificationReport)(unsigned,unsigned,unsigned,bool){};
inline void(*pickupReport)(int,bool){};
inline void(*pickupRenderReport)(void*,int){};
// Observe the native update on its owning thread, while both entity and
// scene-query pointers are alive. This deliberately leaves visibility intact.
using Update=void(*)(void*,void*,void*);
inline Update originalUpdate{};
inline thread_local bool pickupUpdateScope{};
inline thread_local const void* pickupFrustumScope{};
struct CullObservation {bool tested{};float nativeResult{},effectiveResult{};nativePickupSector::Snapshot sector;bool sphereKnown{},planesKnown{};std::array<float,4> sphere{};std::array<float,24> testedPlanes{};};
inline thread_local CullObservation pickupCull{};
inline void(*visibilityReport)(void*,int,bool,int,float,const CullObservation&){};
inline void observeUpdate(void* entity,void* argument,void* query){
    struct RestoreScope {bool active;const void* planes;CullObservation cull;~RestoreScope(){pickupUpdateScope=active;pickupFrustumScope=planes;pickupCull=cull;}} restore{pickupUpdateScope,pickupFrustumScope,pickupCull};
    pickupCull={};
    pickupUpdateScope=manualReload::readable(entity,0xbc)&&*reinterpret_cast<std::uintptr_t*>(entity)==image+0x30bc78;
    pickupFrustumScope=nullptr;
    if(pickupUpdateScope&&manualReload::readable(query,0xc8)){
        auto volume=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(query)+0xc0);
        if(manualReload::readable(volume,0x1c+24*sizeof(float)))pickupFrustumScope=volume+0x1c;
    }
    originalUpdate(entity,argument,query);
    if(!visibilityReport||!manualReload::readable(entity,0xbc)||
       *reinterpret_cast<std::uintptr_t*>(entity)!=image+0x30bc78)return;
    auto e=static_cast<unsigned char*>(entity);int plane=-1;float margin=0;
    if(manualReload::readable(query,0xc8)){
        auto q=static_cast<unsigned char*>(query);
        auto volume=*reinterpret_cast<unsigned char**>(q+0xc0);
        if(manualReload::readable(volume,0x1c+24*sizeof(float))){
            float sphere[4],planes[24];std::memcpy(sphere,e+0x78,sizeof sphere);
            std::memcpy(planes,volume+0x1c,sizeof planes);
            margin=FLT_MAX;
            for(int i=0;i<6;i++){
                auto p=planes+4*i;float d=p[0]*sphere[0]+p[1]*sphere[1]+p[2]*sphere[2]+p[3]+sphere[3];
                if(std::isfinite(d)&&d<margin){margin=d;plane=i;}
            }
        }
    }
    pickupCull.sector=nativePickupSector::capture(image,reinterpret_cast<std::uintptr_t>(entity),reinterpret_cast<std::uintptr_t>(query),[](std::uintptr_t address,void* output,size_t bytes){if(!manualReload::readable(reinterpret_cast<void*>(address),bytes))return false;std::memcpy(output,reinterpret_cast<void*>(address),bytes);return true;});
    visibilityReport(entity,*reinterpret_cast<int*>(e+0xb8),(e[0x44]&8)!=0,plane,margin,pickupCull);
}
inline bool installVisibilityObserver(){
    // The observer is independent of the optional pill mesh. load() can fail
    // before install() initializes image, so never use that earlier side effect.
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!image)return false;
    const unsigned char code[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9,0x49,0x8b,0xd8};
    auto target=reinterpret_cast<void*>(image+0x7c7f0);
    return manualReload::readable(target,sizeof code)&&!std::memcmp(target,code,sizeof code)&&MH_CreateHook(target,reinterpret_cast<void*>(observeUpdate),reinterpret_cast<void**>(&originalUpdate))==MH_OK;
}
inline bool nativeApply(int id,void* player){auto result=originalApply(id,player);if(pickupReport)pickupReport(id,result);return result;}
inline bool validEntity(const Record& r,int id){
    if(!r.entity||!*reinterpret_cast<void**>(image+0x3a5330)||r.scene!=*reinterpret_cast<std::uintptr_t*>(image+0x3a5330)||!manualReload::readable(r.entity,0xbc))return false;
    auto e=static_cast<unsigned char*>(r.entity);return *reinterpret_cast<std::uintptr_t*>(e)==image+0x30bc78&&*reinterpret_cast<int*>(e+0xb8)==id&&*reinterpret_cast<std::uintptr_t*>(e+0x98)&&*reinterpret_cast<std::uintptr_t*>(e+0xa0);
}
inline bool collect(const Record& record,int id){
    if(!validEntity(record,id))return false;
    {std::lock_guard<std::mutex> lock(mutex);if(!supply.collect(id))return false;carried=supply.count();records.erase(id);}
    removePickup(record.entity);reloadAudio::play(40);return true;
}

inline bool validHands(const manualReload::Hands& h,std::uint64_t now){auto drawn=nativeHandClip::drawn[0].load();return installed&&!geometry.empty()&&drawn&&now>=drawn&&now-drawn<100&&h.session&&h.gameplay&&h.tracked&&h.leftPoseTracked&&now>=h.time&&now-h.time<100&&motionTracking::nearHead(h.left,h.head)&&std::isfinite(h.scale)&&h.scale>=.001f&&h.scale<=100;}
inline bool handWorld(const manualReload::Hands& h,void* player,std::uint64_t now,XrVector3f& out){
    using namespace DirectX;motionTracking::Sample sample;
    if(!arsenal::rayAccessor||!motionTracking::read(now,sample)||sample.time!=h.time||!manualReload::readable(player,0x320)||!motionTracking::validPose(h.gun))return false;
    auto p=static_cast<unsigned char*>(player);if(*reinterpret_cast<std::uintptr_t*>(p)!=image+0x30c0f8)return false;
    float body[12];std::memcpy(body,p+8,sizeof body);if(!motionTracking::validBasis(body))return false;
    auto head=reinterpret_cast<std::uintptr_t>(p+0x308),link=*reinterpret_cast<std::uintptr_t*>(head);
    // The native flashlight can be the first equipment-list entry. Find the
    // actual weapon instead of treating that first entry as the held gun.
    const std::uintptr_t tables[]={0x30c500,0x30c640,0x30c900,0x30d2e0,0x30d668,0x30cb70,0x30d1a8};
    unsigned char* weapon=nullptr;
    for(unsigned count=0;count<16&&link!=head;++count){
        if(link<0x98||!manualReload::readable(reinterpret_cast<void*>(link-0x98),0xd0))return false;
        auto candidate=reinterpret_cast<unsigned char*>(link-0x98);auto table=*reinterpret_cast<std::uintptr_t*>(candidate);
        if(h.weapon>=0&&h.weapon<7&&table==image+tables[h.weapon]){weapon=candidate;break;}
        auto next=*reinterpret_cast<std::uintptr_t*>(link);if(next==link)return false;link=next;
    }
    if(!weapon)return false;
    float ray[6]{};arsenal::rayAccessor(weapon+0xb8,ray);for(float v:ray)if(!std::isfinite(v))return false;
    auto local=motionTracking::relativePose(h.gun,h.left);
    auto m=motionTracking::rotation(local.orientation)*XMMatrixTranslation(local.position.x,local.position.y,local.position.z)*motionTracking::rotation(sample.rightAim.orientation)*XMMatrixTranslation(sample.rightAim.position.x,sample.rightAim.position.y,sample.rightAim.position.z);
    XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,m);out=motionTracking::worldPoint(body,{ray[0],ray[1],ray[2]},{matrix._41,matrix._42,matrix._43},h.scale);
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
}
inline float handDistance(const Record& record,XrVector3f hand){auto transform=reinterpret_cast<const float*>(static_cast<unsigned char*>(record.entity)+8);return manualReload::distance(hand,{transform[3],transform[7],transform[11]});}
inline void render(void* entity,void* renderer,unsigned flags,void* options){
    bool hide=false;
    if((flags&4)&&manualReload::readable(entity,0xbc)&&*reinterpret_cast<std::uintptr_t*>(entity)==image+0x30bc78){
        int id=*reinterpret_cast<int*>(static_cast<unsigned char*>(entity)+0xb8);
        if(pickupRenderReport)pickupRenderReport(entity,id);
        Record candidate{entity,*reinterpret_cast<std::uintptr_t*>(image+0x3a5330),GetTickCount64()};
        if(id>=600&&id<1000&&validEntity(candidate,id)){std::lock_guard<std::mutex> lock(mutex);if(records.size()>512)records.clear();
            records[id]=candidate;hide=supply.contains(id)||(hold.active()&&id==heldId&&held.entity==entity);}
    }
    if(!hide)originalRender(entity,renderer,flags,options);
}
// 80110 is queried to offer Examine/Take, before the user accepts it.
// Never transfer inventory or remove an entity from this predicate.
inline bool apply(int id,void* player){
    if(id<600||id>=1000)return nativeApply(id,player);
    manualReload::Hands h;{std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;}
    if(h.session&&player==*reinterpret_cast<void**>(image+0x3a5340)){
        std::lock_guard<std::mutex> lock(mutex);
        if(supply.contains(id))return false; // Also suppress native immediate healing after removal.
        auto it=records.find(id);
        if(it!=records.end()&&validEntity(it->second,id))return supply.canCollect(id);
    }
    return nativeApply(id,player);
}
// Native Take/Give commits through A7CE0. The VR proximity-grab path already
// commits inventory itself and calls the original trampoline directly.
inline void commitPickup(void* entity){
    bool collected=false;
    manualReload::Hands h;{std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;}
    if(h.session){
        std::lock_guard<std::mutex> lock(mutex);
        for(auto it=records.begin();it!=records.end();++it){
            if(it->second.entity!=entity||!validEntity(it->second,it->first))continue;
            if(!supply.collect(it->first))return;
            carried=supply.count();records.erase(it);collected=true;break;
        }
    }
    removePickup(entity);if(collected)reloadAudio::play(40);
}

inline void tick(void* player){
    if(!installed)return;
    if(verifyEffectRequested.exchange(false)&&applyEffect&&player==*reinterpret_cast<void**>(image+0x3a5340)&&manualReload::menuState()==0){
        // Desktop-only diagnostic runs on the owning game thread and restores
        // health immediately. It never grants, consumes or saves inventory.
        auto& deficit=*reinterpret_cast<unsigned char*>(image+0x396340);
        const unsigned saved=deficit;
        if(saved<=10){
            deficit=4;
            const bool accepted=medicine::heal(600,[&]{return deficit;},[&](int id){return originalApply(id,player);},[&](int id){applyEffect(id,player);});
            const unsigned after=deficit;deficit=static_cast<unsigned char>(saved);
            if(effectVerificationReport)effectVerificationReport(4,after,deficit,accepted);
        }
    }
    if(restorePending&&player==*reinterpret_cast<void**>(image+0x3a5340))restorePending();
    // Deferred removal keeps render-list iteration intact and preserves collected
    // pickup identity even when native room data respawns an old entity.
    std::vector<Record> removed;
    {std::lock_guard<std::mutex> lock(mutex);for(auto i=records.begin();i!=records.end();)if(supply.contains(i->first)&&validEntity(i->second,i->first)){removed.push_back(i->second);i=records.erase(i);}else ++i;}
    for(auto& r:removed)removePickup(r.entity);
    manualReload::Hands h;{std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;}
    auto now=GetTickCount64();
    bool valid=validHands(h,now)&&player==*reinterpret_cast<void**>(image+0x3a5340);XrVector3f hand{};
    Controls input;{std::lock_guard<std::mutex> lock(mutex);input=controls;}
    const bool live=input.valid&&now>=input.time&&now-input.time<100;
    bool leftGrip=live?input.grip[0]:h.grip,leftUse=live?input.trigger[0]:h.confirm;
    bool located=valid&&handWorld(h,player,now,hand);bool gripEdge=leftGrip&&!previousGrip;previousGrip=leftGrip;
    bool rightEdge=live&&input.grip[1]&&!previousRightGrip;previousRightGrip=live&&input.grip[1];
    bool free=true;{std::lock_guard<std::mutex> lock(manualReload::mutex);int i=arsenal::index(h.weapon);if(i>=0&&i<4)free=arsenal::reloads[i].state()==arsenal::Phase::Ready;if(h.weapon==1)free=manualReload::gestures.transaction().stage()==manualReload::Stage::Ready;}
    free=free&&!supportAim::activeFor(h.weapon);
    bool rightValid=live&&input.rightFree&&h.session&&h.gameplay&&h.rightPoseTracked&&now>=h.time&&now-h.time<100&&motionTracking::nearHead(h.rightGrip,h.head);
    Record pickup{};int pickupId=-1;
    if(located&&free&&leftGrip&&!hold.active()){
        std::lock_guard<std::mutex> lock(mutex);float nearest=.13f*h.scale;
        for(auto& entry:records)if(now>=entry.second.time&&now-entry.second.time<500&&validEntity(entry.second,entry.first)){
            float distance=handDistance(entry.second,hand);if(distance<nearest){nearest=distance;pickupId=entry.first;pickup=entry.second;}}
    }
    if(pickupId<0&&rightValid&&input.grip[1]&&!hold.active()){
        auto rightHands=h;rightHands.left=h.rightGrip;XrVector3f rightWorld{};
        if(handWorld(rightHands,player,now,rightWorld)){
            std::lock_guard<std::mutex> lock(mutex);float nearest=.13f*h.scale;
            for(auto& entry:records)if(now>=entry.second.time&&now-entry.second.time<500&&validEntity(entry.second,entry.first)){
                float distance=handDistance(entry.second,rightWorld);if(distance<nearest){nearest=distance;pickupId=entry.first;pickup=entry.second;}}
        }
    }
    if(pickupId>=0){collect(pickup,pickupId);return;}
    // Front-hip supply uses the same upright body anchor as its display.
    auto chest=motionTracking::relativePose(motionTracking::uprightPose(h.head),h.left).position;
    if(valid&&free&&gripEdge&&carried.load()&&manualReload::distance(chest,pocketAnchor)<.18f){
        std::lock_guard<std::mutex> lock(mutex);if(hold.grab(true,true,leftUse)){pocketHeld=true;heldSide=0;holding=true;reloadAudio::play(40);}
    }
    auto rightChest=motionTracking::relativePose(motionTracking::uprightPose(h.head),h.rightGrip).position;
    if(rightValid&&rightEdge&&carried.load()&&manualReload::distance(rightChest,pocketAnchor)<.18f){
        std::lock_guard<std::mutex> lock(mutex);if(hold.grab(true,true,input.trigger[1])){pocketHeld=true;heldSide=1;holding=true;reloadAudio::play(40);}
    }
    bool consume=false;
    if(useRequested.load()){
        auto when=useTime.load();bool same=usePlayer.load()==reinterpret_cast<std::uintptr_t>(player)&&useScene.load()==*reinterpret_cast<std::uintptr_t*>(image+0x3a5330);
        if(!same||now<when||now-when>2000)useRequested=false;
        else if(h.gameplay&&useRequested.exchange(false))consume=true;
    }
    {std::lock_guard<std::mutex> lock(mutex);if(hold.active()){
        const bool right=heldSide.load()==1;
        auto action=hold.update(right?rightValid:valid&&free,right?input.grip[1]:leftGrip,right?input.trigger[1]:leftUse);
        if(action==pillHold::Action::Release){pocketHeld=false;holding=false;heldSide=-1;}
        if(action==pillHold::Action::Consume)consume=true;
    }}
    if(consume&&player==*reinterpret_cast<void**>(image+0x3a5340)){
        int id;{std::lock_guard<std::mutex> lock(mutex);id=supply.next();}
        // 80110 is the native eligibility check, not the pickup effect.
        // Only spend a bottle after the native effect actually improves health.
        bool accepted=applyEffect&&medicine::heal(id,
            []{return *reinterpret_cast<unsigned char*>(image+0x396340);},
            [player](int value){return originalApply(value,player);},
            [player](int value){applyEffect(value,player);});
        {std::lock_guard<std::mutex> lock(mutex);supply.consume(accepted);carried=supply.count();hold.acknowledge(accepted);holding=hold.active();if(accepted){pocketHeld=false;heldSide=-1;}}
        if(accepted)reloadAudio::play(41);
    }
}

inline bool install(){
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char applyCode[]={0x40,0x53,0x56,0x57,0x48,0x83,0xec,0x20,0x8d,0x41,0x9c,0x8b,0xf1,0x3d,0xf3,0x01};
    const unsigned char renderCode[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
    const unsigned char removeCode[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x05,0x3f,0xd6,0x2f};
    const unsigned char effectCode[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x55};
    if(std::memcmp(reinterpret_cast<void*>(image+0x80280),effectCode,sizeof effectCode))return false;
    if(std::memcmp(reinterpret_cast<void*>(image+0x80110),applyCode,16)||std::memcmp(reinterpret_cast<void*>(image+0x807b0),renderCode,16)||std::memcmp(reinterpret_cast<void*>(image+0xa7ce0),removeCode,16)||*reinterpret_cast<std::uintptr_t*>(image+0x30bc78+0x30)!=image+0x807b0)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(image+0x80110),reinterpret_cast<void*>(apply),reinterpret_cast<void**>(&originalApply))!=MH_OK)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(image+0x807b0),reinterpret_cast<void*>(render),reinterpret_cast<void**>(&originalRender))!=MH_OK){MH_RemoveHook(reinterpret_cast<void*>(image+0x80110));return false;}
    applyEffect=reinterpret_cast<void(*)(int,void*)>(image+0x80280);
    if(MH_CreateHook(reinterpret_cast<void*>(image+0xa7ce0),reinterpret_cast<void*>(commitPickup),reinterpret_cast<void**>(&removePickup))!=MH_OK){
        MH_RemoveHook(reinterpret_cast<void*>(image+0x807b0));MH_RemoveHook(reinterpret_cast<void*>(image+0x80110));return false;
    }
    return installed=true;
}
}
