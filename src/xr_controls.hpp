#pragma once
#include "init_diagnostics.hpp"
#include "control_router.hpp"
#include "tracking_math.hpp"
#include "controller_pose.hpp"
#include <openxr/openxr.h>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <string>
namespace vrControls {
class OpenXRControls {
    XrInstance instance{};XrSession session{};PFN_xrGetInstanceProcAddr get{};
    XrActionSet set{};XrPath hands[2]{};
    XrAction stick{},click{},primary{},secondary{},trigger{},grip{},gripPose{},aimPose{};
    XrAction haptic{};PFN_xrApplyHapticFeedback applyHaptic{};
    XrSpace gripSpaces[2]{},aimSpaces[2]{};
    PFN_xrStringToPath stringToPath{};PFN_xrCreateActionSet createSet{};PFN_xrCreateAction createAction{};
    PFN_xrSuggestInteractionProfileBindings suggest{};PFN_xrAttachSessionActionSets attach{};
    PFN_xrCreateActionSpace createSpace{};PFN_xrSyncActions syncActions{};
    PFN_xrGetActionStateBoolean getBoolean{};PFN_xrGetActionStateFloat getFloat{};PFN_xrGetActionStateVector2f getVector{};
    PFN_xrLocateSpace locate{};PFN_xrDestroySpace destroySpace{};PFN_xrDestroyActionSet destroySet{};
    void check(XrResult result,const char* what){if(XR_FAILED(result)){char text[XR_MAX_RESULT_STRING_SIZE]="unavailable";PFN_xrResultToString describe{};if(instance&&get&&XR_SUCCEEDED(get(instance,"xrResultToString",reinterpret_cast<PFN_xrVoidFunction*>(&describe)))&&describe)describe(instance,result,text);initDiagnostics::write("OpenXR controls api=%s result=%d description=%s",what,int(result),text);throw std::runtime_error(std::string(what)+" result="+std::to_string(result));}}
    template<class T>void load(const char* name,T& function){check(get(instance,name,reinterpret_cast<PFN_xrVoidFunction*>(&function)),name);}
    XrPath path(const char* name){XrPath value{};check(stringToPath(instance,name,&value),"controller path");return value;}
    XrAction action(const char* name,const char* label,XrActionType type){
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};strcpy_s(info.actionName,name);strcpy_s(info.localizedActionName,label);info.actionType=type;info.countSubactionPaths=2;info.subactionPaths=hands;
        XrAction result{};check(createAction(set,&info,&result),"create controller action");return result;
    }
    XrActionStateGetInfo state(XrAction action,unsigned handIndex){XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=action;info.subactionPath=hands[handIndex];return info;}
    bool boolean(XrAction action,unsigned handIndex){auto info=state(action,handIndex);XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};check(getBoolean(session,&info,&value),"read controller button");return value.isActive&&value.currentState;}
    float scalar(XrAction action,unsigned handIndex){auto info=state(action,handIndex);XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};check(getFloat(session,&info,&value),"read controller axis");return value.isActive?value.currentState:0;}
public:
    struct Hand {XrPosef grip{{0,0,0,1},{0,0,0}},aim{{0,0,0,1},{0,0,0}};bool gripValid{},aimValid{};float trigger{},squeeze{};};
    Hand hand[2];PoseContinuity gripContinuity[2],aimContinuity[2];Input input{};Output output{};Router router;
    void rebase(const XrPosef& change){for(unsigned i=0;i<2;i++){gripContinuity[i].rebase(change);aimContinuity[i].rebase(change);hand[i].grip=gripContinuity[i].pose;hand[i].aim=aimContinuity[i].pose;}}
    void initialize(XrInstance xrInstance,XrSession xrSession,PFN_xrGetInstanceProcAddr loader){
        instance=xrInstance;session=xrSession;get=loader;
        load("xrStringToPath",stringToPath);load("xrCreateActionSet",createSet);load("xrCreateAction",createAction);
        load("xrSuggestInteractionProfileBindings",suggest);load("xrAttachSessionActionSets",attach);load("xrCreateActionSpace",createSpace);
        load("xrSyncActions",syncActions);load("xrGetActionStateBoolean",getBoolean);load("xrGetActionStateFloat",getFloat);load("xrGetActionStateVector2f",getVector);
        load("xrLocateSpace",locate);load("xrDestroySpace",destroySpace);load("xrDestroyActionSet",destroySet);
        hands[0]=path("/user/hand/left");hands[1]=path("/user/hand/right");
        XrActionSetCreateInfo info{XR_TYPE_ACTION_SET_CREATE_INFO};strcpy_s(info.actionSetName,"dementium_controls");strcpy_s(info.localizedActionSetName,"Dementium VR controls");info.priority=0;
        check(createSet(instance,&info,&set),"create controller action set");
        stick=action("stick","Move and turn",XR_ACTION_TYPE_VECTOR2F_INPUT);click=action("stick_click","Stick click",XR_ACTION_TYPE_BOOLEAN_INPUT);
        primary=action("primary_button","A or X",XR_ACTION_TYPE_BOOLEAN_INPUT);secondary=action("secondary_button","B or Y",XR_ACTION_TYPE_BOOLEAN_INPUT);
        trigger=action("trigger","Trigger",XR_ACTION_TYPE_FLOAT_INPUT);grip=action("squeeze","Grip",XR_ACTION_TYPE_FLOAT_INPUT);
        gripPose=action("grip_pose","Hand grip pose",XR_ACTION_TYPE_POSE_INPUT);aimPose=action("aim_pose","Hand aim pose",XR_ACTION_TYPE_POSE_INPUT);
        if(XR_SUCCEEDED(get(instance,"xrApplyHapticFeedback",reinterpret_cast<PFN_xrVoidFunction*>(&applyHaptic))))haptic=action("haptic","Weapon feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT);
        std::vector<XrActionSuggestedBinding> bindings;
        auto bind=[&](XrAction action,const char* binding){bindings.push_back({action,path(binding)});};
        for(auto side:{"left","right"}){
            std::string prefix=std::string("/user/hand/")+side+"/input/";
            bind(stick,(prefix+"thumbstick").c_str());bind(click,(prefix+"thumbstick/click").c_str());
            bind(trigger,(prefix+"trigger/value").c_str());bind(grip,(prefix+"squeeze/value").c_str());
            bind(gripPose,(prefix+"grip/pose").c_str());bind(aimPose,(prefix+"aim/pose").c_str());
            if(haptic)bind(haptic,(std::string("/user/hand/")+side+"/output/haptic").c_str());
        }
        bind(primary,"/user/hand/left/input/x/click");bind(primary,"/user/hand/right/input/a/click");
        bind(secondary,"/user/hand/left/input/y/click");bind(secondary,"/user/hand/right/input/b/click");
        XrInteractionProfileSuggestedBinding profile{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};profile.interactionProfile=path("/interaction_profiles/oculus/touch_controller");profile.countSuggestedBindings=static_cast<uint32_t>(bindings.size());profile.suggestedBindings=bindings.data();
        check(suggest(instance,&profile),"bind Quest Touch controls");
        XrSessionActionSetsAttachInfo attachment{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attachment.countActionSets=1;attachment.actionSets=&set;check(attach(session,&attachment),"attach controller actions");
        for(unsigned i=0;i<2;i++){XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};space.subactionPath=hands[i];space.poseInActionSpace.orientation.w=1;space.action=gripPose;check(createSpace(session,&space,&gripSpaces[i]),"create grip space");space.action=aimPose;check(createSpace(session,&space,&aimSpaces[i]),"create aim space");}
    }
    void update(XrTime predictedTime,XrSpace reference,uint64_t milliseconds){
        if(!set)return;
        XrActiveActionSet active{set,XR_NULL_PATH};XrActionsSyncInfo info{XR_TYPE_ACTIONS_SYNC_INFO};info.countActiveActionSets=1;info.activeActionSets=&active;
        XrResult result=syncActions(session,&info);if(result==XR_SESSION_NOT_FOCUSED){input={};output=router.step(input,milliseconds);for(auto& h:hand)h.gripValid=h.aimValid=false;return;}check(result,"sync controller actions");
        input={};input.active=true;
        for(unsigned i=0;i<2;i++){
            auto request=state(stick,i);XrActionStateVector2f value{XR_TYPE_ACTION_STATE_VECTOR2F};check(getVector(session,&request,&value),"read controller stick");
            if(i==0){input.leftX=value.isActive?value.currentState.x:0;input.leftY=value.isActive?value.currentState.y:0;}else{input.rightX=value.isActive?value.currentState.x:0;input.rightY=value.isActive?value.currentState.y:0;}
            auto& h=hand[i];h.trigger=scalar(trigger,i);h.squeeze=scalar(grip,i);
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};check(locate(gripSpaces[i],reference,predictedTime,&location),"locate controller grip");h.gripValid=gripContinuity[i].update(location);h.grip=gripContinuity[i].pose;
            location={XR_TYPE_SPACE_LOCATION};check(locate(aimSpaces[i],reference,predictedTime,&location),"locate controller aim");h.aimValid=aimContinuity[i].update(location);h.aim=aimContinuity[i].pose;
        }
        input.a=boolean(primary,1);input.x=boolean(primary,0);input.b=boolean(secondary,1);input.y=boolean(secondary,0);
        input.l3=boolean(click,0);input.r3=boolean(click,1);input.leftTrigger=hand[0].trigger;input.rightTrigger=hand[1].trigger;input.leftGrip=hand[0].squeeze;input.rightGrip=hand[1].squeeze;
        output=router.step(input,milliseconds);
    }
    void shutdown(){for(auto space:gripSpaces)if(space&&destroySpace)destroySpace(space);for(auto space:aimSpaces)if(space&&destroySpace)destroySpace(space);if(set&&destroySet)destroySet(set);set={};haptic=XR_NULL_HANDLE;input={};hand[0]={};hand[1]={};gripSpaces[0]=gripSpaces[1]=aimSpaces[0]=aimSpaces[1]=XR_NULL_HANDLE;}
    bool pulse(unsigned index,float amplitude,unsigned milliseconds){
        if(index>1||!haptic||!applyHaptic||!input.active||!hand[index].gripValid||!std::isfinite(amplitude)||amplitude<=0)return false;
        XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=haptic;info.subactionPath=hands[index];
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.amplitude=amplitude>1?1:amplitude;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;vibration.duration=static_cast<XrDuration>((std::min)(milliseconds,60u))*1000000;
        return XR_SUCCEEDED(applyHaptic(session,&info,reinterpret_cast<const XrHapticBaseHeader*>(&vibration)));
    }
};
}
