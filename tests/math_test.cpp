#define WIN32_LEAN_AND_MEAN
#include "../src/stereo.hpp"
#include <cstdio>
#include <array>
static void close(float actual,float expected,const char* name) {
    if(fabsf(actual-expected)>0.0001f)throw std::runtime_error(std::string(name)+": actual="+std::to_string(actual)+" expected="+std::to_string(expected));
}
static std::array<float,300> camera() {
    std::array<float,300> d{};d[0]=.991187f;d[5]=1.767617f;d[10]=1.002045f;d[11]=.100204f;d[14]=-1;
    d[16]=d[21]=d[26]=1;return d;
}
static XrView view() {
    XrView v{XR_TYPE_VIEW};v.pose.orientation.w=1;v.fov={-.8f,.9f,.75f,-.85f};return v;
}
int main() {
    try {
        float lamp[112]{};lamp[7]=lamp[52]=lamp[60]=1;lamp[8]=42;lamp[16]=43;lamp[76]=.043478f;
        XrPosef head{{0,0,0,1},{0,0,0}},eye=head;eye.position.x=-.032f;
        if(!headLightConstants::center(lamp,head,eye,1))throw std::runtime_error("centered light rejected");
        close(lamp[4],.032f,"left eye beam parallax");close(lamp[5],0,"beam height");close(lamp[14],-1,"beam forward");
        close(lamp[8],42,"room position preserved");close(lamp[16],43,"room direction preserved");close(lamp[76],.043478f,"attenuation preserved");
        head.orientation=eye.orientation={0,.70710678f,0,.70710678f};eye.position={0,0,.032f};
        if(!headLightConstants::center(lamp,head,eye,1))throw std::runtime_error("rotated light rejected");
        close(lamp[4],.032f,"rotated eye parallax");close(lamp[12],0,"head direction rotation cancels");close(lamp[14],-1,"rotated beam forward");
        lamp[56]=1;headLightConstants::enabled=false;
        if(!headLightConstants::center(lamp,head,eye,1))throw std::runtime_error("light off rejected");
        close(lamp[52],0,"lamp disabled");close(lamp[56],1,"room remains enabled");close(lamp[8],42,"room position remains");close(lamp[16],43,"room direction remains");
        headLightConstants::enabled=true;lamp[52]=1;lamp[60]=0;if(headLightConstants::center(lamp,head,eye,1))throw std::runtime_error("non spotlight modified");
        stereo::origin={};stereo::origin.orientation.w=1;stereo::unitsPerMeter=1;
        auto v=view();auto d=camera();stereo::transform(d.data(),v);
        close(d[0]*tanf(v.fov.angleLeft)-d[2],-1,"left FOV boundary");
        close(d[0]*tanf(v.fov.angleRight)-d[2],1,"right FOV boundary");
        close(d[5]*tanf(v.fov.angleDown)-d[6],-1,"bottom FOV boundary");
        close(d[5]*tanf(v.fov.angleUp)-d[6],1,"top FOV boundary");
        auto depthAt=[&](float distance){return (-2*(-distance*d[10]+d[11])-distance)/distance;};
        close(depthAt(.02f),0,"VR two-centimetre near plane");
        float nativeFar=(-2*.100204f)/(-2*1.002045f+2);
        close(depthAt(nativeFar),1,"preserved native far plane");
        auto left=camera(),right=camera();v.pose.position.x=-.032f;stereo::transform(left.data(),v);
        v.pose.position.x=.032f;stereo::transform(right.data(),v);
        close(left[19],.032f,"left eye translation");close(right[19],-.032f,"right eye translation");
        // Recover view-ray slopes from the actual asymmetric projections. A
        // half-metre object must have ten times the disparity of one at 5 m.
        auto rayX=[](const std::array<float,300>& c,float distance) {
            float x=c[19]-distance*c[18],z=c[27]-distance*c[26];
            float ndc=(c[0]*x+c[2]*z)/(-z);
            return (ndc+c[2])/c[0];
        };
        close(rayX(left,.5f)-rayX(right,.5f),.128f,"near stereo disparity");
        close(rayX(left,5.f)-rayX(right,5.f),.0128f,"far stereo disparity");
        // Rolling the headset also rotates the physical eye baseline. It must
        // remain horizontal in eye coordinates, without vertical eye mismatch.
        v=view();v.pose.orientation={0,0,.707106781f,.707106781f};
        v.pose.position.y=-.032f;left=camera();stereo::transform(left.data(),v);
        v.pose.position.y=.032f;right=camera();stereo::transform(right.data(),v);
        close(left[19],.032f,"rolled left eye baseline");close(right[19],-.032f,"rolled right eye baseline");
        close(left[23]-right[23],0,"rolled vertical alignment");
        auto tiltedLeft=view(),tiltedRight=view();
        DirectX::XMFLOAT4 tilted;
        DirectX::XMStoreFloat4(&tilted,DirectX::XMQuaternionRotationRollPitchYaw(.4f,.7f,-.3f));
        tiltedLeft.pose.orientation=tiltedRight.pose.orientation={tilted.x,tilted.y,tilted.z,tilted.w};
        tiltedLeft.pose.position={1.f,1.7f,2.f};tiltedRight.pose.position={1.064f,1.7f,2.f};
        auto upright=stereo::recenterOrigin(tiltedLeft,tiltedRight);
        close(upright.position.x,1.032f,"recenter uses eye midpoint");
        close(upright.position.y,1.7f,"recenter height reference");
        close(upright.orientation.x,0,"recenter removes pitch");close(upright.orientation.z,0,"recenter removes roll");
        close(2*atan2f(upright.orientation.y,upright.orientation.w),.7f,"recenter preserves yaw");
        stereo::origin.orientation={0,.707106781f,0,.707106781f};v=view();v.pose.orientation=stereo::origin.orientation;v.pose.position.x=.2f;
        d=camera();stereo::transform(d.data(),v);
        close(d[19],0,"recentered displacement X");close(d[23],0,"recentered displacement Y");close(d[27],-.2f,"recentered displacement Z");
        v.pose.position={};float angle=100.f*DirectX::XM_PI/180;
        v.pose.orientation={0,sinf(angle*.5f),0,cosf(angle*.5f)};d=camera();stereo::transform(d.data(),v);
        float objectX=-d[18],objectZ=-d[26];
        close(atan2f(objectX,-objectZ),10.f*DirectX::XM_PI/180,"relative yaw after recenter");
        std::array<float,32> particle{};auto mesh=camera();
        memcpy(particle.data(),mesh.data()+16,48);memcpy(particle.data()+12,mesh.data(),64);
        particle[28]=2;particle[29]=3;particle[30]=4;particle[31]=5;
        stereo::transformConstants(mesh.data(),1200,v);stereo::transformConstants(particle.data(),128,v);
        for(unsigned i=0;i<12;i++)close(particle[i],mesh[16+i],"particle camera parity");
        for(unsigned i=0;i<16;i++)close(particle[12+i],mesh[i],"particle projection parity");
        for(unsigned i=0;i<4;i++)close(particle[28+i],float(2+i),"particle parameters preserved");
        stereo::origin={};stereo::origin.orientation.w=1;v=view();float aimX,aimY;
        if(!stereo::gunAimOffset(v,aimX,aimY))throw std::runtime_error("neutral aim hidden");
        close(aimX,0,"neutral gun aim X");close(aimY,0,"neutral gun aim Y");
        v.pose.orientation={0,sinf(DirectX::XM_PI/12),0,cosf(DirectX::XM_PI/12)};
        if(!stereo::gunAimOffset(v,aimX,aimY))throw std::runtime_error("visible gun aim hidden");
        close(aimX,.7f*tanf(DirectX::XM_PI/6),"crosshair remains on body aim when head turns");
        v.pose.orientation={0,1,0,0};if(stereo::gunAimOffset(v,aimX,aimY))throw std::runtime_error("gun behind head should hide crosshair");
        // A combined physical translation and pitch/yaw/roll places the
        // tracked eye at the camera origin and its forward ray on -Z.
        v=view();DirectX::XMFLOAT4 q;
        DirectX::XMStoreFloat4(&q,DirectX::XMQuaternionRotationRollPitchYaw(.3f,-.5f,.2f));
        v.pose.orientation={q.x,q.y,q.z,q.w};v.pose.position={.2f,.3f,-.1f};
        d=camera();stereo::transform(d.data(),v);
        auto position=v.pose.position;
        for(unsigned r=0;r<3;r++)close(d[16+r*4]*position.x+d[17+r*4]*position.y+d[18+r*4]*position.z+d[19+r*4],0,"combined 6DoF eye origin");
        auto forward=motionTracking::pointAhead(v.pose,2);
        for(unsigned r=0;r<3;r++)close(d[16+r*4]*forward.x+d[17+r*4]*forward.y+d[18+r*4]*forward.z+d[19+r*4],r==2?-2.f:0.f,"combined 6DoF forward ray");
        motionTracking::publish(stereo::origin,{{0,0,0,1},{.3f,-.2f,-.4f}},1,GetTickCount64(),true);
        v=view();if(!stereo::gunAimOffset(v,aimX,aimY))throw std::runtime_error("tracked gun aim hidden");
        close(aimX,.3f*1.4f/10.4f/2.f,"tracked gun crosshair X");
        close(aimY,.2f*1.4f/10.4f/(2.f*stereo::desktopHeight/stereo::desktopWidth),"tracked gun crosshair Y");
        float normalAim=aimX;auto normal=camera();stereo::transform(normal.data(),v);
        stereo::frameScopeFactor=3.f;d=camera();stereo::transform(d.data(),v);
        close(d[0],normal[0]*3,"scope horizontal magnification");close(d[5],normal[5]*3,"scope vertical magnification");
        close(d[2],normal[2],"scope asymmetric horizontal offset");close(d[6],normal[6],"scope asymmetric vertical offset");
        close(d[10],normal[10],"scope preserves depth A");close(d[11],normal[11],"scope preserves depth B");
        if(!stereo::gunAimOffset(v,aimX,aimY))throw std::runtime_error("scoped gun aim hidden");
        close(aimX,normalAim*3,"scope crosshair agrees with magnified scene");stereo::frameScopeFactor=1.f;
        motionTracking::publish(stereo::origin,{},1,GetTickCount64(),false);
        puts("PASS: asymmetric eye FOV, preserved depth, near/far stereo disparity, rolled eye alignment, recentered translation, relative yaw");return 0;
    }catch(const std::exception& e){printf("FAIL: %s\n",e.what());return 1;}
}
