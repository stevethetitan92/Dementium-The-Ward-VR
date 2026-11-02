#pragma once
namespace selectedHands {
inline bool attachSupport(bool rightTracked,bool hasSupport,bool supportAcquired,bool grip,bool reloadOwnsHand){
    return rightTracked&&hasSupport&&supportAcquired&&grip&&!reloadOwnsHand;
}
}
