#pragma once
#include <cstdint>
#include <cmath>
#include "control_router.hpp"
#include "inventory_layout.hpp"
namespace inventoryScreen {
inline constexpr int weapons[9]={0,1,3,2,4,5,6,-1,-2};
inline constexpr unsigned ids[9]={6,2,3,5,7,4,1,0,0};
struct Result{bool captured{},changed{},closed{},use{},toggleHaze{},toggleBlackFog{};int equip=-1;};
class Controller {
    bool shown{},previousY{},previousA{},previousB{},axisArmed=true;
    unsigned focus{},page{},setting{};bool previousChord{},previousLeftGrip{},previousRightGrip{},modOnly{};
public:
    bool visible()const{return shown;}
    unsigned selection()const{return focus;}
    unsigned menuPage()const{return page;}
    unsigned settingSelection()const{return setting;}
    void close(){shown=false;axisArmed=false;}
    Result step(const vrControls::Input& input,bool allowed,unsigned owned,unsigned pills){
        Result out;bool y=input.y&&!previousY,a=input.a&&!previousA,b=input.b&&!previousB;
        bool chord=input.l3&&input.r3,chordPress=chord&&!previousChord;
        bool left=input.leftGrip>.65f,right=input.rightGrip>.65f; bool leftPress=left&&!previousLeftGrip,rightPress=right&&!previousRightGrip;
        previousChord=chord;previousLeftGrip=left;previousRightGrip=right;
        previousY=input.y;previousA=input.a;previousB=input.b;
        if(!input.active||!allowed){out.closed=shown;close();return out;}
        bool was=shown;
        if(y||chordPress){shown=!shown;modOnly=chordPress;page=modOnly?1:0;axisArmed=false;out.changed=true;}
        out.captured=shown||was;out.closed=was&&!shown;
        if(!shown)return out;
        if(b){close();out.closed=true;return out;}
        bool pagePress=!y&&!chordPress&&(leftPress!=rightPress); if(pagePress){unsigned first=modOnly?1u:0u; page=rightPress?(page==3?first:page+1):(page==first?3:page-1);axisArmed=false;out.changed=true;}
        if(page>0){
            if(page==1){
                if(std::fabs(input.leftY)<.25f)axisArmed=true;
                if(axisArmed&&std::fabs(input.leftY)>.65f){setting=1-setting;axisArmed=false;out.changed=true;}
            }
            if(a&&!y&&!chordPress&&!pagePress&&page==1){if(setting==0)out.toggleHaze=true;else out.toggleBlackFog=true;}
            return out;
        }
        float x=input.leftX,z=input.leftY;
        if(std::fabs(x)<.25f&&std::fabs(z)<.25f)axisArmed=true;
        if(axisArmed&&(std::fabs(x)>.65f||std::fabs(z)>.65f)){
            using D=inventoryLayout::Direction;
            auto direction=std::fabs(x)>std::fabs(z)?(x>0?D::right:D::left):(z>0?D::up:D::down);
            unsigned next=inventoryLayout::neighbor(focus,direction);out.changed=next!=focus;focus=next;axisArmed=false;
        }
        // Opening while A is already held never equips/consumes an item.
        if(a&&!y&&!pagePress&&focus<9){
            int weapon=weapons[focus];
            if(weapon>=0&&(owned&(1u<<ids[focus]))){out.equip=weapon;close();out.closed=true;}
            else if(weapon==-2&&pills){out.use=true;close();out.closed=true;}
        }
        return out;
    }
};
}


