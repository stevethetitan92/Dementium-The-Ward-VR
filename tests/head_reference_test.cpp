#include "../src/head_reference.hpp"
#include <cstdio>
#include <stdexcept>
using namespace headReference;
static void check(bool x){if(!x)throw std::runtime_error("head reference regression");}
static void close(float a,float b){check(std::fabs(a-b)<.0001f);}
static void same(const XrPosef& a,const XrPosef& b){close(a.position.x,b.position.x);close(a.position.y,b.position.y);close(a.position.z,b.position.z);float dot=a.orientation.x*b.orientation.x+a.orientation.y*b.orientation.y+a.orientation.z*b.orientation.z+a.orientation.w*b.orientation.w;close(std::fabs(dot),1);}
int main(){try{
 XrPosef p{{0,0,0,1},{.2f,1.15f,-.1f}},out{};Startup startup;
 for(unsigned t=100;t<1100;t+=50)check(!startup.update(p,t,false,out));
 for(unsigned t=1100;t<1850;t+=50)check(!startup.update(p,t,true,out));
 check(startup.update(p,1850,true,out));same(p,out);
 startup.reset();for(unsigned t=100;t<900;t+=50){p.position.x+=.1f;check(!startup.update(p,t,true,out));}
 check(!startup.update(p,900,false,out));check(!startup.update(p,1000,true,out));
 check(!startup.update(p,2000,true,out)); // A stale frame gap restarts settling.
 XrPosef origin{{0,.342897807f,0,.939372713f},{2,1.15f,-3}};
 XrPosef head{{0,.479425539f,0,.877582562f},{2.15f,1.25f,-3.2f}};
 XrPosef change{{0,-.247403959f,0,.968912422f},{10,-.42f,-20}};
 auto before=motionTracking::relativePose(origin,head);
 same(before,motionTracking::relativePose(rebase(origin,change),rebase(head,change)));
 auto oldLeft=head,oldRight=head;oldLeft.position.x-=.032f;oldRight.position.x+=.032f;
 auto left=rebase(oldLeft,change),right=rebase(oldRight,change);
 auto delta=motionTracking::relativePose(left,right).position;close(std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z),.064f);
 Postures posture;float offset;
 check(!posture.calibrate(Posture::Seated,0));check(posture.calibrate(Posture::Seated,1.15f));
 check(!posture.calibrate(Posture::Standing,1.2f));check(posture.calibrate(Posture::Standing,1.72f));
 posture.resetMode();check(!posture.update(1.15f,100,true,offset));check(posture.update(1.15f,3100,true,offset));close(offset,0);check(posture.mode()==Posture::Seated);
 check(!posture.update(1.72f,3200,true,offset));check(!posture.update(1.72f,6100,true,offset));check(posture.mode()==Posture::Seated);
 check(posture.update(1.72f,6200,true,offset));close(offset,.57f);check(posture.mode()==Posture::Standing);
 check(!posture.update(1.15f,6300,true,offset));check(!posture.update(1.72f,6500,true,offset));check(posture.mode()==Posture::Standing);
 check(!posture.update(1.15f,6600,true,offset));check(!posture.update(1.15f,9000,false,offset));check(!posture.update(1.15f,10000,true,offset));check(posture.mode()==Posture::Standing);
 check(!posture.update(.4f,15000,true,offset));check(posture.mode()==Posture::Standing);
 puts("PASS: stable worn-start baseline, focus/gap rejection, reference rebase continuity, stereo eye separation, measured posture calibration and hysteresis");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
