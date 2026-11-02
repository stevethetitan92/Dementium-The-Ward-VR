#pragma once
namespace pillHold {
enum class Action {None,Release,Consume};
class Hold {
    bool holding{},previousConfirm{};
public:
    bool active()const{return holding;}
    bool grab(bool valid,bool grip,bool confirm){if(holding||!valid||!grip)return false;holding=true;previousConfirm=confirm;return true;}
    Action update(bool valid,bool grip,bool confirm){
        if(!holding)return Action::None;
        if(!valid||!grip){holding=false;previousConfirm=confirm;return Action::Release;}
        bool edge=confirm&&!previousConfirm;previousConfirm=confirm;return edge?Action::Consume:Action::None;
    }
    void acknowledge(bool consumed){if(consumed)holding=false;}
};
}
