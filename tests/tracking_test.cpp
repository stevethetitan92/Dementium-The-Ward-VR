#include "../src/tracking_math.hpp"
#include "../src/interaction_gaze_math.hpp"
#include "../src/native_wrist_pose.hpp"
#include "../src/native_transform_cache.hpp"
#include "../src/geometry_isolation.hpp"
#include "../src/controller_pose.hpp"
#include <cstdio>
#include <stdexcept>
#include <limits>
using namespace motionTracking;
void near(float a,float b){if(std::fabs(a-b)>.0001f)throw std::runtime_error("tracking transform mismatch");}
XrQuaternionf yaw(float angle){return {0,std::sin(angle*.5f),0,std::cos(angle*.5f)};}
int main(){try{
    // Gaze follows head pitch/yaw, not controller orientation. Stale tracking
    // must leave the native direction untouched.
    float viewIdentity[]={1,0,0,0,0,1,0,0,0,0,1,0},gaze[3];
    Sample headSample;headSample.valid=true;headSample.time=100;
    if(!interactionGaze::direction(viewIdentity,headSample,100,gaze))throw std::runtime_error("gaze unavailable");
    near(gaze[0],0);near(gaze[1],0);near(gaze[2],-1);
    headSample.rightAim.orientation={-std::sqrt(.5f),0,0,std::sqrt(.5f)};
    interactionGaze::direction(viewIdentity,headSample,100,gaze);
    near(gaze[0],0);near(gaze[1],-1);near(gaze[2],0);
    headSample.rightAim.orientation=yaw(DirectX::XM_PIDIV2);
    interactionGaze::direction(viewIdentity,headSample,100,gaze);
    near(gaze[0],-1);near(gaze[1],0);near(gaze[2],0);
    if(interactionGaze::direction(viewIdentity,headSample,250,gaze)||interactionGaze::direction(viewIdentity,headSample,99,gaze))throw std::runtime_error("stale gaze used");
    headSample.rightAim.position={.2f,-.3f,.4f};headSample.scale=2;
    float nativePosition[]={10,2,30},eyePosition[3];
    if(!interactionGaze::position(viewIdentity,headSample,100,nativePosition,eyePosition))throw std::runtime_error("room-scale gaze unavailable");
    near(eyePosition[0],10.4f);near(eyePosition[1],1.4f);near(eyePosition[2],30.8f);
    near(nativePosition[0],10);near(nativePosition[1],2);near(nativePosition[2],30);
    float turnedView[]={0,0,1,0,0,1,0,0,-1,0,0,0};
    headSample.rightAim.orientation={0,0,0,1};
    interactionGaze::direction(turnedView,headSample,100,gaze);near(gaze[0],1);near(gaze[1],0);near(gaze[2],0);
    interactionGaze::position(turnedView,headSample,100,nativePosition,eyePosition);
    near(eyePosition[0],9.2f);near(eyePosition[1],1.4f);near(eyePosition[2],30.4f);
    float badView[12]={};
    if(interactionGaze::direction(badView,headSample,100,gaze)||interactionGaze::position(badView,headSample,100,nativePosition,eyePosition))throw std::runtime_error("invalid camera gaze accepted");
    headSample.valid=false;
    if(interactionGaze::direction(viewIdentity,headSample,100,gaze))throw std::runtime_error("invalid gaze used");
    vrControls::PoseContinuity leftPose,rightPose;
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};location.pose={{0,0,0,1},{-.3f,-.8f,0}};
    if(leftPose.update(location)||leftPose.have)throw std::runtime_error("uninitialized controller accepted");
    location.locationFlags=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;
    if(!leftPose.update(location)||!leftPose.have)throw std::runtime_error("valid inferred side pose rejected");
    location.pose.position.x=.3f;if(!rightPose.update(location))throw std::runtime_error("right inferred pose rejected");
    location.locationFlags=0;location.pose.position.x=100;
    if(leftPose.update(location)||!leftPose.have)throw std::runtime_error("lost pose continuity failed");
    near(leftPose.pose.position.x,-.3f);near(rightPose.pose.position.x,.3f);
    location.locationFlags=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;location.pose.position.x=std::numeric_limits<float>::quiet_NaN();
    if(leftPose.update(location))throw std::runtime_error("NaN pose accepted");near(leftPose.pose.position.x,-.3f);
    leftPose.rebase({{0,0,0,1},{1,0,0}});near(leftPose.pose.position.x,-1.3f);
    XrPosef displayOrigin{{0,0,0,1},{0,0,0}};
    motionTracking::publish(displayOrigin,rightPose.pose,1,100,false);
    motionTracking::publishDisplay(displayOrigin,rightPose.pose,1,100,true);motionTracking::Sample displaySample;
    if(motionTracking::read(100,displaySample)||!motionTracking::readDisplay(100,displaySample))throw std::runtime_error("display fallback leaked into gameplay tracking");
    for(unsigned i=1;i<=5;i++){auto mode=static_cast<geometryIsolation::Mode>(i);geometryIsolation::set(mode,1000);if(geometryIsolation::mode(1000)!=mode||geometryIsolation::mode(45999)!=mode||geometryIsolation::mode(46000)!=geometryIsolation::Normal)throw std::runtime_error("geometry diagnostic expiry failure");}geometryIsolation::set(geometryIsolation::Normal,50000);
    XrPosef origin{yaw(DirectX::XM_PIDIV2),{3,1.3f,4}};
    XrPosef hand{origin.orientation,{3,1.1f,3.7f}};
    auto relative=relativePose(origin,hand);
    near(relative.position.x,.3f);near(relative.position.y,-.2f);near(relative.position.z,0);
    near(relative.orientation.w,1);near(relative.orientation.y,0);
    auto tip=pointAhead(relative,10);near(tip.x,.3f);near(tip.y,-.2f);near(tip.z,-10);
    // Native body yaw and OpenXR recenter yaw compose once, not twice.
    float body[]={0,0,1,9,0,1,0,8,-1,0,0,7};
    if(!validBasis(body))throw std::runtime_error("valid native body rejected");
    float model[12];weaponMatrix(body,{10,20,30},relative,2,model);
    near(model[3],10);near(model[7],19.6f);near(model[11],29.4f);
    near(-model[2],-1);near(-model[6],0);near(-model[10],0);
    float walk=1,strafe=0;headRelativeAxes({{0,0,0,1},{0,0,0}},{yaw(DirectX::XM_PIDIV2),{0,0,0}},strafe,walk);
    near(strafe,-1);near(walk,0);
    // Physical sideways, vertical and forward motion retain independent axes.
    for(unsigned axis=0;axis<3;axis++){
        XrPosef p{{0,0,0,1},{0,0,0}};(&p.position.x)[axis]=.2f;
        auto local=relativePose({{0,0,0,1},{0,0,0}},p);
        for(unsigned a=0;a<3;a++)near((&local.position.x)[a],a==axis?.2f:0.f);
    }
    // Repeated samples never accumulate height or drift. Looking elsewhere is
    // absent from the hand transform and cannot alter its aiming direction.
    publish(origin,hand,1,1000,true);Sample sample;
    if(!read(1000,sample)||!read(1149,sample)||read(1150,sample)||read(999,sample))throw std::runtime_error("stale/future tracking accepted");
    for(unsigned i=0;i<100;i++){publish(origin,hand,1,1000+i,true);if(!read(1000+i,sample))throw std::runtime_error("fresh pose rejected");near(sample.rightAim.position.y,-.2f);}
    publish(origin,hand,1,1200,false);if(read(1200,sample))throw std::runtime_error("lost tracking accepted");
    hand.orientation.w=std::numeric_limits<float>::quiet_NaN();publish(origin,hand,1,1300,true);
    if(read(1300,sample))throw std::runtime_error("nonfinite pose accepted");
    body[0]=2;if(validBasis(body))throw std::runtime_error("scaled/sheared body accepted");
    XrPosef head{{0,0,0,1},{20,1,-30}},controller=head;controller.position.x+=.7f;
    if(!nearHead(controller,head))throw std::runtime_error("room-scale position rejected");
    controller.position.x+=4;if(nearHead(controller,head))throw std::runtime_error("detached controller outlier accepted");
    controller=head;controller.orientation={0,0,0,0};if(nearHead(controller,head))throw std::runtime_error("invalid controller rotation accepted");
    FireGate fire;
    if(fire.update(true,1,true))throw std::runtime_error("initial held trigger fired");
    fire.update(true,0,false);if(!fire.update(true,1,true))throw std::runtime_error("armed trigger suppressed");
    fire.update(false,1,true);if(fire.update(true,1,true))throw std::runtime_error("tracking recovery fired a held trigger");
    fire.update(true,0,false);if(!fire.update(true,1,true))throw std::runtime_error("trigger did not rearm");
    // A translated and rotated native world must cancel out of a wrist socket.
    float nativeAim[]={0,0,1,100,0,1,0,20,-1,0,0,-30};
    float nativeEntity[12];std::memcpy(nativeEntity,nativeAim,sizeof nativeAim);
    XrPosef bone{{0,0,0,1},{.03f,.05f,.07f}},socket;
    nativeWristPose::capture(nativeAim,nativeEntity,bone,1,2000);
    XrPosef stage{{0,0,0,1},{2,3,4}};
    if(!nativeWristPose::frame(stage,2000,socket))throw std::runtime_error("native wrist unavailable");
    near(socket.position.x,2.03f);near(socket.position.y,3.05f);near(socket.position.z,4.07f);
    XMFLOAT3 mapped;XMStoreFloat3(&mapped,XMVector3TransformNormal(XMVectorSet(1,2,3,0),rotation(socket.orientation)));
    near(mapped.x,-3);near(mapped.y,-1);near(mapped.z,2);
    XrPosef leftGrip=stage;leftGrip.position.x=-2;XrPosef leftSocket;
    if(!nativeWristPose::leftFrame(stage,stage,leftGrip,2000,true,leftSocket))throw std::runtime_error("mirrored wrist unavailable");
    near(leftSocket.position.x,-2.03f);near(leftSocket.position.y,3.05f);near(leftSocket.position.z,4.07f);
    if(!nativeWristPose::leftFrame(stage,stage,leftGrip,2300,false,leftSocket))throw std::runtime_error("right tracking loss changed left attachment");
    near(leftSocket.position.x,-2.03f);
    if(nativeWristPose::frame(stage,2100,socket))throw std::runtime_error("stale native wrist accepted");
    float oldObject[]={1,0,0,2,0,1,0,3,0,0,1,4};
    float newObject[]={0,0,1,7,0,1,0,8,-1,0,0,9};
    float cached[]={1,0,0,12,0,1,0,23,0,0,1,34},changed[12];
    if(!nativeTransformCache::replace(cached,oldObject,newObject,changed))throw std::runtime_error("cached native transform rejected");
    near(changed[3],17);near(changed[7],28);near(changed[11],39);
    near(changed[2],1);near(changed[8],-1);
    if(!nativeTransformCache::replace(changed,newObject,oldObject,changed))throw std::runtime_error("cache restoration failed");
    for(unsigned i=0;i<12;i++)near(changed[i],cached[i]);
    oldObject[0]=0;if(nativeTransformCache::replace(cached,oldObject,newObject,changed))throw std::runtime_error("singular cache accepted");
    // Melee endpoints must follow the drawn controller, including a rotated
    // camera whose reference axes differ from the player's model axes.
    float cameraView[]={0,0,-1,0,0,1,0,0,1,0,0,0},cameraBasis[12];
    if(!cameraWorldBasis(cameraView,cameraBasis))throw std::runtime_error("camera basis rejected");
    XrPosef meleePose{{0,0,0,1},{.2f,-.3f,-.4f}};
    for(float scale:{.5f,1.f,2.f}){
        float drawn[12];weaponMatrix(cameraBasis,{10,20,30},meleePose,scale,drawn);
        for(float length:{.08f,.18f,.38f,.52f}){
            auto contact=worldPoint(cameraBasis,{10,20,30},pointAhead(meleePose,length),scale);
            near(contact.x,drawn[3]-length*scale*drawn[2]);
            near(contact.y,drawn[7]-length*scale*drawn[6]);
            near(contact.z,drawn[11]-length*scale*drawn[10]);
        }
    }
    puts("PASS: tracking transforms, melee/render alignment and actual native wrist socket/world cancellation.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}

