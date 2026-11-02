#include "../src/inventory_items.hpp"
#include <cassert>
#include <cstdio>
int main(){using namespace inventoryItems;assert(ownedMask(0,0)==0);assert(ownedMask(~0u,~0u)==0x3fff);for(unsigned id=0;id<64;id++){unsigned expected=0;for(unsigned i=0;i<ids.size();i++)if(ids[i]==id)expected=1u<<i;assert(ownedMask(id<32?1u<<id:0,id>=32?1u<<(id-32):0)==expected);}assert(ownedMask(0xff,0)==0);puts("PASS: all 64 native inventory bits map only to the fourteen source key items; weapon ownership does not create key items");}
