#include "../src/wristband_native.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void* expectedEntity;static void* expectedStep;static void* expectedQuery;static unsigned forwarded;
static void nativeStub(void* entity,void* step,void* query){
    require(entity==expectedEntity&&step==expectedStep&&query==expectedQuery,"native update arguments changed by wrapper");++forwarded;
}
static std::array<unsigned char,256> nestedEntity,nestedCamera;
static std::array<unsigned char,512> nestedQuery;
static void* outerQuery;static void* outerEntity;static unsigned depth,reports;
static void scopedNativeStub(void* entity,void* step,void* query){
    require(pillBottle::pickupUpdateScope,"pickup owning scope absent");
    const auto camera=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(query)+0xc0);
    require(pillBottle::pickupFrustumScope==camera+0x1c,"pickup scope used another camera");
    require(!pillBottle::pickupCull.tested,"nested update inherited previous cull observation");
    pillBottle::pickupCull={true,depth?2.f:1.f,depth?4.f:3.f};
    pillBottle::pickupCull.sector.status=nativePickupSector::Status::CountBypass;
    if(!depth){
        require(query==outerQuery&&entity==outerEntity,"outer update forwarding changed");
        depth=1;pillBottle::observeUpdate(nestedEntity.data(),step,nestedQuery.data());depth=0;
        require(pillBottle::pickupFrustumScope==camera+0x1c&&pillBottle::pickupCull.nativeResult==1.f&&pillBottle::pickupCull.effectiveResult==3.f,"nested update damaged outer culling evidence");
        require(pillBottle::pickupCull.sector.status==nativePickupSector::Status::CountBypass,"nested update damaged outer sector evidence");
    }
}
static void report(void* entity,int id,bool visible,int,float,const pillBottle::CullObservation& cull){
    require(id==100&&visible&&cull.tested,"pickup visibility report lost native result");
    require(entity==(depth?nestedEntity.data():outerEntity),"pickup report mixed entity identities");
    require(cull.nativeResult==(depth?2.f:1.f)&&cull.effectiveResult==(depth?4.f:3.f),"pickup report mixed nested results");++reports;
    require(cull.sector.status==(depth?nativePickupSector::Status::StampDiffers:nativePickupSector::Status::StampMatches),"pickup report mixed owning sector snapshots");
}
int main(){try{
    std::array<unsigned char,1024> entity{},step{},query{};
    manualReload::enabled=false;manualReload::installed=false;manualReload::hands={};
    arsenal::enabled=arsenal::meleeEnabled=arsenal::installed=false;
    headLamp::installed=false;pillBottle::installed=false;wristband::adapterVerified=false;
    manualReload::originalUpdate=nativeStub;wristband::originalPlayerUpdate=nativeStub;pillBottle::originalUpdate=nativeStub;
    for(auto& original:arsenal::updates)original=nativeStub;
    using Update=void(*)(void*,void*,void*);
    const Update wrappers[]={manualReload::updateHook,wristband::playerUpdate,pillBottle::observeUpdate,arsenal::updateHook<0>,arsenal::updateHook<1>,arsenal::updateHook<2>,arsenal::updateHook<3>,arsenal::updateHook<4>,arsenal::updateHook<5>};
    const std::uintptr_t opaqueQueries[]={0,reinterpret_cast<std::uintptr_t>(query.data()),0x7ffffffffffffffcULL,UINT64_MAX};
    for(unsigned repeat=0;repeat<128;++repeat)for(auto value:opaqueQueries){
        expectedEntity=entity.data();expectedStep=step.data()+repeat;expectedQuery=reinterpret_cast<void*>(value);
        for(auto wrapper:wrappers)wrapper(expectedEntity,expectedStep,expectedQuery);
    }
    require(forwarded==128*4*9,"native forwarding call count differs");
    // Do not replace or sanitize an invalid opaque native argument in a
    // wrapper: these stubs establish forwarding, not native-query validity.
    auto image=static_cast<unsigned char*>(VirtualAlloc(nullptr,4*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(image!=nullptr,"scratch image allocation failed");
    struct FreeImage{void* address;~FreeImage(){VirtualFree(address,0,MEM_RELEASE);}} freeImage{image};
    const unsigned char entityCode[]={0x48,0x8b,0x87,0x90,0,0,0},countCode[]={0x8b,0x83,0x24,0x01,0,0,0x39,0x83,0xb8,0,0,0};std::memcpy(image+0x7c90a,entityCode,sizeof entityCode);std::memcpy(image+0x7c92c,countCode,sizeof countCode);
    pillBottle::image=reinterpret_cast<std::uintptr_t>(image);*reinterpret_cast<std::uintptr_t*>(entity.data())=pillBottle::image+0x30bc78;
    *reinterpret_cast<std::uintptr_t*>(nestedEntity.data())=pillBottle::image+0x30bc78;
    *reinterpret_cast<int*>(entity.data()+0xb8)=*reinterpret_cast<int*>(nestedEntity.data()+0xb8)=100;
    entity[0x44]=nestedEntity[0x44]=8;
    std::array<unsigned char,256> camera{};*reinterpret_cast<void**>(query.data()+0xc0)=camera.data();*reinterpret_cast<void**>(nestedQuery.data()+0xc0)=nestedCamera.data();
    std::array<unsigned char,32> outerSector{},innerSector{},manager{};std::array<unsigned char,128> records{};
    *reinterpret_cast<void**>(image+0x3fc340)=manager.data();*reinterpret_cast<void**>(manager.data()+8)=records.data();
    *reinterpret_cast<void**>(entity.data()+0x90)=outerSector.data();*reinterpret_cast<void**>(nestedEntity.data()+0x90)=innerSector.data();
    *reinterpret_cast<unsigned*>(outerSector.data()+0x18)=32;*reinterpret_cast<unsigned*>(innerSector.data()+0x18)=64;*reinterpret_cast<unsigned*>(records.data()+40)=17;*reinterpret_cast<unsigned*>(records.data()+72)=17;
    for(auto q:{query.data(),nestedQuery.data()}){*reinterpret_cast<unsigned*>(q+0xb8)=10;*reinterpret_cast<unsigned*>(q+0x124)=9;*reinterpret_cast<void**>(q+0xb0)=outerSector.data();}
    *reinterpret_cast<unsigned*>(query.data()+0xac)=17;*reinterpret_cast<unsigned*>(nestedQuery.data()+0xac)=18;
    outerEntity=entity.data();outerQuery=query.data();pillBottle::originalUpdate=scopedNativeStub;pillBottle::visibilityReport=report;
    pillBottle::pickupUpdateScope=true;pillBottle::pickupFrustumScope=step.data();pillBottle::pickupCull={true,7.f,8.f};
    pillBottle::pickupCull.sector.status=nativePickupSector::Status::NoEntitySector;
    pillBottle::observeUpdate(entity.data(),step.data(),query.data());
    require(reports==2&&pillBottle::pickupUpdateScope&&pillBottle::pickupFrustumScope==step.data()&&pillBottle::pickupCull.nativeResult==7.f&&pillBottle::pickupCull.effectiveResult==8.f,"owning-call scope not restored");
    require(pillBottle::pickupCull.sector.status==nativePickupSector::Status::NoEntitySector,"outer sector scope not restored");
    std::puts("PASS: actual nested pickup observer reads distinct matching/mismatching sector stamps from isolated scratch memory and restores parent observations; native visibility flag unchanged by snapshot.");
    std::puts("PASS: 4608 actual update-wrapper calls preserve all three native arguments; nested pickup scopes retain owning cameras, entity identities and cull results; stub originals only, no native engine/process/XR calls.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
