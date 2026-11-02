#pragma once
#include <array>
#include <cstdint>
namespace inventoryItems {
// Retail UISSItems ownership queries, in native item order (table RVA 2EA990).
inline constexpr std::array<unsigned,14> ids{32,8,33,34,36,37,38,40,45,42,43,44,41,46};
constexpr unsigned ownedMask(std::uint32_t low,std::uint32_t high){unsigned result=0;for(unsigned i=0;i<ids.size();++i){auto id=ids[i];if((id<32?low:high)&(1u<<(id%32)))result|=1u<<i;}return result;}
}
