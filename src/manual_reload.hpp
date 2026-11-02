#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Pistol reload transaction. Distances are metres in weapon-local space.
// Native ammo is committed by the caller on the simulation thread only.
namespace manualReload {
enum class Stage { Ready, EmptyWell, MagazineHeld, Inserted, SlideHeld, CommitPending };
inline bool usesReloadHand(Stage stage){return stage!=Stage::Ready&&stage!=Stage::EmptyWell;}
struct Input {
    bool tracked{},gameplay{},pistol{},releaseMagazine{},leftGrip{};
    float magazineDistance=100,ammoPouchDistance=100,slideDistance=100,slidePull{};
    bool magazineAvailable=true;
    bool chamberLoaded=false;
};
struct Transfer {int loaded{},reserve{},amount{};};
inline bool transfer(int loaded,int reserve,int capacity,Transfer& out){
    if(loaded<0||reserve<0||capacity<=0||loaded>capacity)return false;
    int n=(std::min)(capacity-loaded,reserve);if(n<=0)return false;
    out={loaded+n,reserve-n,n};return true;
}
class Pistol {
    Stage state=Stage::Ready;
    bool armed=false,previousGrip=false,previousRelease=false,racked=false;
    bool retainedChamber=false;
    std::uint64_t token=0;
public:
    Stage stage() const{return state;}
    bool blocksFire() const{return state!=Stage::Ready;}
    // Interruptions never transfer ammo. Keep an inserted magazine, stow a
    // loose one and release a held slide. Require neutral before resuming.
    void update(const Input& in){
        if(!in.tracked||!in.gameplay||!in.pistol){
            armed=false;previousGrip=in.leftGrip;previousRelease=in.releaseMagazine;
            if(state==Stage::MagazineHeld)state=Stage::EmptyWell;
            if(state==Stage::SlideHeld||state==Stage::CommitPending){state=Stage::Inserted;racked=false;}
            return;
        }
        if(!std::isfinite(in.magazineDistance)||!std::isfinite(in.ammoPouchDistance)||!std::isfinite(in.slideDistance)||!std::isfinite(in.slidePull)||in.magazineDistance<0||in.ammoPouchDistance<0||in.slideDistance<0){
            armed=false;if(state==Stage::MagazineHeld)state=Stage::EmptyWell;if(state==Stage::SlideHeld||state==Stage::CommitPending){state=Stage::Inserted;racked=false;}return;
        }
        if(!armed){if(!in.leftGrip&&!in.releaseMagazine){armed=true;previousGrip=false;previousRelease=false;}return;}
        bool grab=in.leftGrip&&!previousGrip,drop=!in.leftGrip&&previousGrip;
        bool eject=in.releaseMagazine&&!previousRelease;
        previousGrip=in.leftGrip;previousRelease=in.releaseMagazine;
        // B always releases the seated magazine, including an interrupted rack.
        // It cannot complete a pending ammunition transaction.
        if(eject&&(state==Stage::Inserted||state==Stage::SlideHeld||state==Stage::CommitPending)){
            state=Stage::EmptyWell;racked=false;return;
        }
        switch(state){
        case Stage::Ready:if(eject){retainedChamber=in.chamberLoaded;state=Stage::EmptyWell;racked=false;}break;
        case Stage::EmptyWell:if(grab&&in.magazineAvailable&&in.ammoPouchDistance<=.12f)state=Stage::MagazineHeld;break;
        case Stage::MagazineHeld:
            // Seat on a deliberate close insertion while still holding grip.
            // The wider release tolerance remains available for accessibility.
            // previousGrip stays held, so seating cannot also grab the slide.
            if(in.leftGrip&&in.magazineDistance<=.035f)state=Stage::Inserted;
            else if(drop)state=in.magazineDistance<=.075f?Stage::Inserted:Stage::EmptyWell;
            if(state==Stage::Inserted&&retainedChamber){state=Stage::CommitPending;++token;}
            break;
        case Stage::Inserted:
            // Seating and racking are independent gestures. Release the grip
            // used for the magazine, then squeeze again at the slide.
            if(grab&&in.slideDistance<=.065f){state=Stage::SlideHeld;racked=false;}break;
        case Stage::SlideHeld:
            if(in.leftGrip&&in.slidePull>=.045f)racked=true;
            if(drop){state=racked?Stage::CommitPending:Stage::Inserted;if(racked)++token;}
            break;
        case Stage::CommitPending:break;
        }
    }
    std::uint64_t pendingToken() const{return state==Stage::CommitPending?token:0;}
    // Caller acknowledges exactly the request it applied; stale/double replies
    // cannot complete another reload. Failure leaves the magazine inserted.
    bool acknowledge(std::uint64_t request,bool committed){
        if(!request||state!=Stage::CommitPending||request!=token)return false;
        state=committed?Stage::Ready:Stage::Inserted;racked=false;return true;
    }
};
template<class Apply> bool completeReload(Pistol& reload,int loaded,int reserve,int capacity,Apply apply){
    auto request=reload.pendingToken();if(!request)return false;
    if(loaded>=0&&capacity>0&&loaded<=capacity&&reserve>=0&&(loaded==capacity||reserve==0)){
        reload.acknowledge(request,true);return true;
    }
    Transfer expected;
    if(!transfer(loaded,reserve,capacity,expected)){reload.acknowledge(request,false);return false;}
    bool applied=apply();reload.acknowledge(request,applied);return applied;
}
}
