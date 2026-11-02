#include "../src/desktop_test_pose.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    const auto neutral=desktopTestPose::make(0);
    assert(neutral.w==1&&neutral.x==0&&neutral.y==0&&neutral.z==0);
    const auto down=desktopTestPose::make(2),near=desktopTestPose::make(4),far=desktopTestPose::make(5);
    assert(near.pitchX==down.pitchX&&far.pitchX==down.pitchX);
    assert(near.w==down.w&&far.w==down.w&&near.y==far.y);
    assert(near.z<0&&far.z>0&&std::fabs((far.z-near.z)-.75f)<1e-6f);
    for(unsigned mode=0;mode<6;++mode){auto p=desktopTestPose::make(mode);assert(std::fabs(p.pitchX*p.pitchX+p.yawY*p.yawY+p.w*p.w-1)<1e-6f);}
    assert(desktopTestPose::make(3).x==.30f);
    assert(desktopTestPose::make(99).w==1&&desktopTestPose::make(99).z==0);
    puts("PASS: desktop near/far views preserve orientation, height and neutral reset");
}
