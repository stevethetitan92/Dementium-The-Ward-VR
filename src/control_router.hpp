#pragma once
#include <cmath>
#include <cstdint>
namespace vrControls {
enum class Context { gameplay,inventory,physicalMap,modMenu };
struct Input {
    float leftX{},leftY{},rightX{},rightY{},rightTrigger{},leftTrigger{},rightGrip{},leftGrip{};
    bool a{},b{},x{},y{},l3{},r3{},active{};
};
struct Output {
    float walk{},strafe{},turn{},navigateX{},navigateY{},rightGrip{},leftGrip{};
    bool menuCapture{},confirm{},back{},weaponAction{},flashlight{},fire{},nextItem{},toggleMap{},toggleInventory{},toggleModMenu{},previousPage{},nextPage{},openMap{};
    Context context=Context::gameplay;
};
class Router {
    Input previous{};
    Context context=Context::gameplay,resumeContext=Context::gameplay;
    bool pendingLight{},chordLatched{},fireArmed=true;
    static float axis(float value){return std::fabs(value)<.2f?0:value;}
public:
    void reset(){*this=Router{};}
    Context currentContext()const{return context;}
    void setContext(Context value){context=value;pendingLight=false;}
    Output step(const Input& input,uint64_t milliseconds){
        (void)milliseconds;
        Output out{};
        if(!input.active){previous={};pendingLight=false;chordLatched=false;out.context=context;return out;}
        bool a=input.a&&!previous.a,b=input.b&&!previous.b,x=input.x&&!previous.x,y=input.y&&!previous.y;
        if(input.l3&&!previous.l3)pendingLight=true;
        // Defer L3 alone so either order of the chord wins without toggling the light.
        if(input.l3&&input.r3&&!chordLatched){
            pendingLight=false;chordLatched=true;out.toggleModMenu=true;
            if(context==Context::modMenu)context=resumeContext;else{resumeContext=context;context=Context::modMenu;}
        }
        if(!input.l3&&!input.r3)chordLatched=false;
        if(pendingLight&&!input.r3&&!input.l3){
            pendingLight=false;
            if(context==Context::gameplay)out.flashlight=true;
        }
        if(context!=Context::modMenu&&y){out.toggleInventory=true;context=context==Context::inventory?Context::gameplay:Context::inventory;}
        out.openMap=false;
        if(out.openMap)context=Context::inventory;
        bool modal=context==Context::inventory||context==Context::modMenu;
        if(modal){out.navigateX=axis(input.leftX);out.navigateY=axis(input.leftY);out.confirm=a;out.back=b;
            out.previousPage=input.leftGrip>.55f&&previous.leftGrip<=.55f;out.nextPage=input.rightGrip>.55f&&previous.rightGrip<=.55f;
            if(b){if(context==Context::modMenu)out.toggleModMenu=true;context=Context::gameplay;}}
        else{out.walk=axis(input.leftY);out.strafe=axis(input.leftX);out.turn=axis(input.rightX);out.confirm=a;out.weaponAction=b;out.rightGrip=input.rightGrip;out.leftGrip=input.leftGrip;}
        out.nextItem=x&&!modal&&context==Context::gameplay;
        if(modal||context!=Context::gameplay)fireArmed=false;
        else if(input.rightTrigger<.25f)fireArmed=true;
        out.fire=!modal&&context==Context::gameplay&&fireArmed&&input.rightTrigger>.55f;
        // Left trigger remains unassigned until a verified secondary action exists.
        previous=input;out.context=context;return out;
    }
};
}


