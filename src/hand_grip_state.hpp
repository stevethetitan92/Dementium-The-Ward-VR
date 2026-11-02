#pragma once
#include <cmath>
#include <cstdint>
namespace handGrip {
enum class Pose : unsigned {Relaxed,Weapon,Trigger,Bottle,Magazine,Shell,Nightstick,Buzzsaw,Support,Mechanism};
enum class Object : unsigned {None,Weapon,Magazine,Cartridge,Shell,Bottle,Mechanism};
enum class Reload : unsigned {Ready,Open,Held,Inserted,Charging,Pending,Opening};
struct Input {
    bool gameplay{},tracking{},rightGrip{},leftGrip{},trigger{};
    bool rightTracked{},leftTracked{};
    bool magazineReload{};
    int weapon=-1;
    Reload reload=Reload::Ready;
    Object leftObject=Object::None,rightObject=Object::None;
    float supportDistance=100;
    std::uint64_t time{},now{};
};
struct Selection {Pose left=Pose::Relaxed,right=Pose::Relaxed;Object leftObject=Object::None,rightObject=Object::None;bool valid{};};
inline Selection select(const Input& in){
    Selection out;
    if(!in.gameplay||!in.tracking||in.now<in.time||in.now-in.time>=100)return out;
    out.valid=true;
    if(in.rightTracked){
        out.rightObject=in.rightObject;
        if(in.rightObject==Object::Bottle)out.right=Pose::Bottle;
        else if(in.rightObject==Object::Magazine)out.right=Pose::Magazine;
        else if(in.rightObject==Object::Cartridge||in.rightObject==Object::Shell)out.right=Pose::Shell;
        else if(in.weapon>=0&&in.weapon<=6){out.rightObject=Object::Weapon;out.right=in.weapon==6?Pose::Nightstick:in.weapon==5?Pose::Buzzsaw:in.trigger?Pose::Trigger:Pose::Weapon;}
    }
    if(!in.leftTracked)return out;
    out.leftObject=in.leftObject;
    if(in.leftObject==Object::Bottle){out.left=Pose::Bottle;return out;}
    if(in.leftObject==Object::Magazine){out.left=Pose::Magazine;return out;}
    if(in.leftObject==Object::Cartridge||in.leftObject==Object::Shell){out.left=Pose::Shell;return out;}
    if(in.leftObject==Object::Mechanism){out.left=Pose::Mechanism;return out;}
    // Reload ownership outranks support. No state here changes ammo or consumes medicine.
    if(in.reload==Reload::Held){bool magazine=in.weapon==2||in.magazineReload;out.leftObject=magazine?Object::Magazine:in.weapon==3?Object::Shell:Object::Cartridge;out.left=magazine?Pose::Magazine:Pose::Shell;}
    else if(in.reload==Reload::Charging){out.leftObject=Object::Mechanism;out.left=Pose::Mechanism;}
    else if(in.reload==Reload::Ready&&in.leftGrip&&std::isfinite(in.supportDistance)&&in.supportDistance>=0&&in.supportDistance<.09f&&in.weapon>=2&&in.weapon<=5){out.leftObject=Object::Weapon;out.left=Pose::Support;}
    return out;
}
}
