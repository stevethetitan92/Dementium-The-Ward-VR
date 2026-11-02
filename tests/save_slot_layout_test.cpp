#include "../src/save_slot_layout.hpp"
#include <cassert>
int main(){saveSlotLayout::Layout p;assert(saveSlotLayout::parse(R"({"x":330,"y":178,"scale":0.62})",p));assert(p.x==330);assert(!saveSlotLayout::parse(R"({"x":600,"y":178,"scale":1})",p));assert(!saveSlotLayout::parse(R"({"x":0,"y":0,"scale":0})",p));assert(!saveSlotLayout::parse(R"({"x":NaN,"y":0,"scale":1})",p));assert(!saveSlotLayout::parse(R"({"x":0})",p));assert(p.x==330);assert(saveSlotLayout::parse(R"({"x":10,"y":20,"scale":1.2})",p));}
