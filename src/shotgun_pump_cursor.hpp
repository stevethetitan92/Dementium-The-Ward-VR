#pragma once
#include "selected_shotgun_motion.hpp"
namespace selectedShotgun {
// Source-time cursor follows a single authored pull/return cycle. Motion
// segments follow hand travel; plateau segments consume their authored time.
// This is visual state only and never commits ammunition.
class PumpCursor {
    size_t segment{};float seconds{};bool finished{};
public:
    void reset(){segment=0;seconds=0;finished=false;}
    float time()const{return seconds;}
    bool complete()const{return finished;}
    static bool validCycle(const Motion& m){
        if(!m.ready||m.keys.size()<2||std::fabs(m.keys.front().pump)>1e-4f||std::fabs(m.keys.back().pump)>1e-4f)return false;
        bool returning=false;float peak=0;
        for(size_t i=1;i<m.keys.size();i++){
            float delta=m.keys[i].pump-m.keys[i-1].pump;peak=(std::max)(peak,m.keys[i].pump);
            if(delta< -1e-5f)returning=true;
            if(returning&&delta>1e-5f)return false;
        }
        return returning&&peak>.999f;
    }
    float update(const Motion& m,float fraction,float elapsed,bool active){
        if(!active||!validCycle(m)||!std::isfinite(fraction)||!std::isfinite(elapsed)||elapsed<0||elapsed>.25f){reset();return 0;}
        fraction=std::clamp(fraction,0.f,1.f);
        if(finished)return seconds;
        // Reverse along the branch already reached, without jumping from an
        // incomplete pull to the fully-open return branch.
        bool returning=false;size_t branchStart=0;
        for(size_t i=0;i<=segment&&i+1<m.keys.size();i++){
            if(m.keys[i+1].pump<m.keys[i].pump-1e-5f){if(!returning)branchStart=i;returning=true;}
        }
        const auto& current=m.keys[segment];
        const auto& next=m.keys[(std::min)(segment+1,m.keys.size()-1)];
        float blend=next.seconds>current.seconds?(seconds-current.seconds)/(next.seconds-current.seconds):0;
        float position=current.pump+(next.pump-current.pump)*blend;
        if((!returning&&fraction<position-1e-5f)||(returning&&fraction>position+1e-5f)){
            for(size_t i=segment+1;i-->branchStart;){
                const auto& a=m.keys[i];const auto& b=m.keys[i+1];float delta=b.pump-a.pump;
                if(std::fabs(delta)<=1e-5f)continue;
                float amount=(fraction-a.pump)/delta;
                if(amount>=-1e-5f&&amount<=1.00001f){segment=i;seconds=a.seconds+std::clamp(amount,0.f,1.f)*(b.seconds-a.seconds);return seconds;}
            }
        }
        float budget=elapsed;
        while(segment+1<m.keys.size()){
            const auto& a=m.keys[segment];const auto& b=m.keys[segment+1];float delta=b.pump-a.pump;
            if(std::fabs(delta)<=1e-5f){
                // Do not advance a dwell until the hand reaches its position.
                if(std::fabs(fraction-a.pump)>.015f){
                    // A hand farther along the same stroke may wait here too.
                    bool returning=false;for(size_t i=1;i<=segment;i++)if(m.keys[i].pump<m.keys[i-1].pump-1e-5f)returning=true;
                    if((!returning&&fraction<a.pump)||(returning&&fraction>a.pump))break;
                }
                float step=(std::min)(budget,b.seconds-seconds);seconds+=step;budget-=step;
                if(seconds<b.seconds-1e-6f)break;
            }else{
                float amount=std::clamp((fraction-a.pump)/delta,0.f,1.f);
                float target=a.seconds+amount*(b.seconds-a.seconds);
                // No automatic advancement against the current stroke.
                seconds=(std::max)(seconds,target);
                if(seconds<b.seconds-1e-6f)break;
            }
            seconds=b.seconds;++segment;
        }
        finished=segment+1==m.keys.size()&&fraction<=.015f;
        return seconds;
    }
};
}
