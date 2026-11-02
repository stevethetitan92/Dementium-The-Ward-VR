#pragma once
namespace medicine {
template<class ReadDeficit,class Eligible,class ApplyEffect>
bool heal(int id,ReadDeficit read,Eligible eligible,ApplyEffect apply){
    if(id<600||id>=1000)return false;
    const auto before=read();
    if(before==0||before>10||!eligible(id))return false;
    apply(id);
    return read()<before;
}
}
