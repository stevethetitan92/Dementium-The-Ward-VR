#pragma once
#include "reload_gestures.hpp"
#include <array>
#include <unordered_map>
namespace arsenal {
enum class Kind { Revolver=0,Pistol=1,Rifle=2,Shotgun=3,Sniper=4,Saw=5,Nightstick=6 };
enum class Phase { Ready,Open,Held,Inserted,Charging,Pending,Opening };
inline bool usesReloadHand(Phase phase){return phase==Phase::Held||phase==Phase::Inserted||phase==Phase::Charging||phase==Phase::Pending;}
inline bool allowsSupport(Kind kind,Phase phase){
    return phase==Phase::Ready||(kind==Kind::Shotgun&&
        (phase==Phase::Inserted||phase==Phase::Charging||phase==Phase::Pending));
}
class ContextButton {
    bool suppressed{};
public:
    bool update(bool pressed,bool consumed){if(!pressed)suppressed=false;else if(consumed)suppressed=true;return pressed&&!suppressed;}
};
struct Event {bool valid{},release{},grip{},atPouch{},atPort{},atHandle{};float travel{};bool cycleComplete{true};bool seatContact{};bool emptyOnGrab{},closeFlick{},supportHeld{},chamberLoaded{};};
inline bool canGrabSpare(int loaded,int reserve,int capacity,bool magazine){
    return capacity>0&&loaded>=0&&loaded<=capacity&&reserve>=0&&
        (magazine?(loaded>0||reserve>0):(reserve>0&&loaded<capacity));
}
inline bool feed(int loaded,int reserve,int capacity,unsigned requested,manualReload::Transfer& out){
    if(loaded<0||reserve<0||capacity<=0||loaded>capacity)return false;
    int amount=(std::min)(capacity-loaded,reserve);
    if(requested!=~0u)amount=(std::min)(amount,static_cast<int>((std::min)(requested,static_cast<unsigned>(capacity))));
    out={loaded+amount,reserve-amount,amount};return true;
}
class Reload {
    Phase phase=Phase::Ready;bool armed{},b{},grip{},pulled{},openingStroke{},breechOpen{};unsigned request{},amount{};bool closeAfter{};
    bool detachableMagazine{},speedloader{};
    bool retainedChamber{},previousPouch{};
    bool tubeMode{};Phase shellReturn=Phase::Ready;
public:
    void useSpeedloader(bool enabled){speedloader=enabled;}
    void useDetachableMagazine(bool enabled){detachableMagazine=enabled;}
    bool usesDetachableMagazine()const{return detachableMagazine;}
    Phase state()const{return phase;}
    bool blocked()const{return phase!=Phase::Ready;}
    unsigned token()const{return phase==Phase::Pending?request:0;}
    unsigned rounds()const{return amount;}
    float boltOffset()const{return breechOpen?.08f:0.f;}
    void requireCycle(){if(!detachableMagazine&&phase==Phase::Ready)phase=Phase::Inserted;}
    void update(Kind kind,const Event& e){
        bool magazineSniper=kind==Kind::Sniper&&detachableMagazine;
        const bool enterPouch=e.atPouch&&!previousPouch;
        previousPouch=e.atPouch;
        bool tube=kind==Kind::Shotgun,loose=kind==Kind::Revolver||(kind==Kind::Sniper&&!magazineSniper)||tube;
        tubeMode=tube;
        if(!e.valid||!std::isfinite(e.travel)){
            if(tube&&(phase==Phase::Held||(phase==Phase::Pending&&amount>0))){phase=shellReturn;armed=false;pulled=false;return;}
            armed=false;pulled=false;if(phase==Phase::Pending)phase=closeAfter?Phase::Inserted:Phase::Open;if(phase==Phase::Held)phase=Phase::Open;if(phase==Phase::Charging)phase=openingStroke?Phase::Opening:Phase::Inserted;return;
        }
        if(!armed){
            // A transient invalid frame after a shot must not force a support
            // release. Reacquire at rest and require a new complete stroke.
            if(tube&&phase==Phase::Inserted&&e.supportHeld&&e.grip&&e.atHandle){armed=true;b=e.release;grip=true;pulled=false;phase=Phase::Charging;return;}
            if(!e.release&&!e.grip){armed=true;b=grip=false;}return;
        }
        bool click=!tube&&e.release&&!b,grab=e.grip&&!grip,drop=!e.grip&&grip;b=e.release;grip=e.grip;
        if(kind==Kind::Revolver&&phase==Phase::Open&&e.closeFlick){phase=Phase::Ready;pulled=false;return;}
        // Tube loading is available directly from the hip. Preserve whether
        // the chamber still needs a pump; topping up must not create that need.
        if(tube&&grab&&e.atPouch&&(phase==Phase::Ready||phase==Phase::Inserted||phase==Phase::Open)){
            shellReturn=(phase==Phase::Ready&&!e.emptyOnGrab)?Phase::Ready:Phase::Inserted;
            phase=Phase::Held;return;
        }
        // B toggles the revolver cylinder even with a loose round in hand.
        // Cancelling a pending insertion never transfers ammunition.
        if(kind==Kind::Revolver&&click){phase=phase==Phase::Ready?Phase::Open:Phase::Ready;pulled=false;return;}
        if((kind==Kind::Rifle||magazineSniper)&&click){if(phase==Phase::Ready)retainedChamber=e.chamberLoaded;phase=phase==Phase::Held?Phase::Held:Phase::Open;pulled=false;return;}
        switch(phase){
        case Phase::Ready:if(click){openingStroke=false;breechOpen=false;phase=kind==Kind::Sniper&&!magazineSniper?Phase::Opening:Phase::Open;}break;
        case Phase::Opening:
            if(click)phase=Phase::Ready;
            else if(grab&&e.atHandle){phase=Phase::Charging;openingStroke=true;pulled=false;}
            break;
        case Phase::Open:
            if(click){phase=(kind==Kind::Revolver||kind==Kind::Rifle)?Phase::Ready:Phase::Inserted;}
            else if(e.atPouch&&(grab||(e.grip&&enterPouch&&(kind==Kind::Rifle||magazineSniper))))phase=Phase::Held;
            break;
        case Phase::Held:
            // A close push seats while grip remains held. Retain the wider
            // release target, and retain grip history across the transition:
            // one squeeze cannot also grab another round or charging handle.
            if(drop||(e.grip&&e.seatContact)){
                if(!e.atPort)phase=tube?shellReturn:Phase::Open;
                else if(magazineSniper){phase=Phase::Pending;amount=~0u;closeAfter=true;++request;}
                else if(loose){phase=Phase::Pending;amount=kind==Kind::Revolver&&speedloader?~0u:1;closeAfter=false;++request;}
                else if(kind==Kind::Rifle&&retainedChamber){phase=Phase::Pending;amount=~0u;closeAfter=true;++request;}
                else phase=Phase::Inserted;
            }
            break;
        case Phase::Inserted:
            if(tube&&click){phase=Phase::Open;pulled=false;}
            else if(magazineSniper){if(click)phase=Phase::Open;}
            else if((grab||(tube&&e.grip))&&e.atHandle){phase=Phase::Charging;openingStroke=false;pulled=false;}
            break;
        case Phase::Charging:
            if(kind==Kind::Sniper&&openingStroke){
                if(e.grip&&e.travel>=.06f)pulled=true;
                if(drop){breechOpen=pulled&&e.travel>=.06f&&e.travel<=.14f;phase=breechOpen?Phase::Open:Phase::Opening;openingStroke=false;}
                break;
            }
            if(kind==Kind::Sniper&&breechOpen){
                if(e.grip&&e.travel<=-.06f)pulled=true;
                if(drop){
                    if(pulled&&std::fabs(.08f+e.travel)<=.02f){phase=Phase::Pending;amount=0;closeAfter=true;++request;}
                    else phase=Phase::Inserted;
                }
                break;
            }
            if(e.grip&&e.travel>=(tube?.08f:.045f))pulled=true;
            // A completed shotgun back/forward stroke chambers the next shell
            // while the support grip remains held, as a real pump does.
            // The controller can pass the forward stop between samples. Accept
            // a bounded overshoot after a full pull, without requiring a sample
            // inside the narrow rest window. Large jumps remain rejected.
            if(tube&&pulled&&e.grip&&e.travel>=-.08f&&e.travel<=.02f){
                phase=Phase::Pending;amount=0;closeAfter=true;++request;break;
            }
            if(drop){
                // Pump/bolt actions require a return stroke; a magazine rifle
                // charging handle springs forward on release.
                if(pulled&&((kind==Kind::Rifle)||(e.travel>=-.02f&&e.travel<=.02f))){
                    phase=Phase::Pending;amount=loose?0:~0u;closeAfter=true;++request;
                }else phase=Phase::Inserted;
            }
            break;
        case Phase::Pending:break;
        }
    }
    bool acknowledge(unsigned id,bool success){
        if(!id||id!=token())return false;
        if(tubeMode&&amount>0){phase=shellReturn;return true;}
        phase=success?(closeAfter?Phase::Ready:Phase::Open):(closeAfter?Phase::Inserted:Phase::Open);if(success&&closeAfter)breechOpen=false;return true;
    }
};
inline XrVector3f mix(XrVector3f a,XrVector3f b,float f){return {a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f};}
struct Segment {XrVector3f base{},tip{};};
class Melee {
    XrVector3f strikeDirection{};float reverseTravel{};
    Segment previous{};std::uint64_t time{},motorStart{},lastStrike{};bool initialized{},armed{},striking{},motor{},triggerArmed{};float travel{};
    std::unordered_map<std::uintptr_t,std::uint64_t> hit;
public:
    bool active()const{return striking||motor;}
    unsigned starts{},reversals{},speedRejects{},gapRejects{};
    void reset(){initialized=armed=striking=motor=triggerArmed=false;time=motorStart=lastStrike=0;travel=0;hit.clear();}
    bool update(Kind kind,Segment local,std::uint64_t now,bool valid,float trigger){
        if(!valid||!std::isfinite(trigger)||!std::isfinite(local.tip.x)||!std::isfinite(local.tip.y)||!std::isfinite(local.tip.z)||
           !std::isfinite(local.base.x)||!std::isfinite(local.base.y)||!std::isfinite(local.base.z)){reset();return false;}
        if(initialized&&now==time)return active();
        if(!initialized||now<time||now-time>100){if(initialized)++gapRejects;reset();previous=local;time=now;initialized=true;return false;}
        // Reject spatial tracking jumps, not legitimate fast rotational tip motion.
        float dt=(now-time)/1000.f,d=manualReload::distance(local.tip,previous.tip),speed=d/dt;
        if(!std::isfinite(speed)||d>.75f||manualReload::distance(local.base,previous.base)>.5f){++speedRejects;reset();return false;}
        XrVector3f direction{local.tip.x-previous.tip.x,local.tip.y-previous.tip.y,local.tip.z-previous.tip.z};
        if(d>0){direction.x/=d;direction.y/=d;direction.z/=d;}
        previous=local;time=now;
        if(kind==Kind::Saw){
            striking=false;
            if(trigger<.25f){triggerArmed=true;motor=false;motorStart=0;hit.clear();}
            else if(trigger>.65f&&triggerArmed){if(!motorStart)motorStart=now;motor=now-motorStart>=200;}
            return motor;
        }
        motor=false;
        if(speed<.3f){armed=true;striking=false;travel=reverseTravel=0;}
        if(striking&&speed>1.2f){
            float dot=direction.x*strikeDirection.x+direction.y*strikeDirection.y+direction.z*strikeDirection.z;
            if(dot<-.4f)reverseTravel+=d;else reverseTravel=0;
            if(reverseTravel>=.06f&&now-lastStrike>=120){++reversals;lastStrike=now;strikeDirection=direction;reverseTravel=0;hit.clear();}
        }
        if(armed&&speed>1.2f&&now-lastStrike>=120){travel+=d;if(travel>=.08f){++starts;armed=false;striking=true;lastStrike=now;strikeDirection=direction;reverseTravel=0;hit.clear();}}
        // Keep collision active for the physical stroke, including late contact.
        // Rest, invalid tracking or a discontinuity ends it; per-target history
        // still prevents repeated damage from one continuous swing.
        return striking;
    }
    bool canHit(std::uintptr_t target,std::uint64_t now,Kind kind)const{
        if(!active()||!target)return false;auto i=hit.find(target);
        return i==hit.end()||(kind==Kind::Saw&&now>=i->second&&now-i->second>=100);
    }
    void landed(std::uintptr_t target,std::uint64_t now){hit[target]=now;}
};
template<class Visitor> void sweep(Segment previous,Segment current,Visitor visit,float units=1){
    if(!std::isfinite(units)||units<.001f||units>100)return;
    float travel=(std::max)(manualReload::distance(previous.base,current.base),manualReload::distance(previous.tip,current.tip));
    if(!std::isfinite(travel)||travel>.75f*units)return;
    if(travel<.001f){visit(current);return;}
    unsigned steps=static_cast<unsigned>(std::ceil(travel/(.035f*units)))+1;
    for(unsigned i=0;i<=steps;i++){float f=float(i)/steps;visit(Segment{mix(previous.base,current.base,f),mix(previous.tip,current.tip,f)});}
}
}
