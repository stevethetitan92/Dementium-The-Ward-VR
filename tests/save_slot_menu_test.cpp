#include "../src/save_slot_menu.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace saveSlots;Menu m;std::array<bool,3> occupied{true,false,true};
 m.open(false,0);auto a=m.confirm(occupied);assert(a.selected&&!a.fresh&&a.slot==0);m.move(1);assert(!m.confirm(occupied).selected);m.move(1);assert(m.confirm(occupied).slot==2);m.move(1);assert(m.selection()==0);m.move(-1);assert(m.selection()==2);m.back();assert(m.page()==MenuPage::closed);
 m.open(true,0);assert(!m.confirm(occupied).selected&&m.page()==MenuPage::overwrite);assert(!m.confirm(occupied).selected&&m.page()==MenuPage::slots);m.confirm(occupied);m.move(1);a=m.confirm(occupied);assert(a.selected&&a.fresh&&a.overwrite&&a.slot==0);m.completed(false);assert(m.page()==MenuPage::error);m.back();m.move(1);a=m.confirm(occupied);assert(a.selected&&a.fresh&&!a.overwrite&&a.slot==1);m.completed(true);assert(m.page()==MenuPage::closed);
 std::cout<<"PASS: three slots, empty Continue refusal, wrap navigation, overwrite defaults to cancel, failure recovery\n";}
