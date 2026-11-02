#include "../src/flashlight_profile_range.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
int main(){
    std::array<float,257> source{},result{};source.fill(.95f);
    for(unsigned i=228;i<256;++i)source[i]=.95f+.05f*float(i-227)/28;
    source[256]=source[255];const auto original=source;
    assert(flashlightProfileRange::normalizeObservedFloor(source.data(),result));
    assert(source==original && result[0]==0 && result[227]==0);
    assert(std::abs(result[255]-1)<.00001f && result[256]==result[255]);
    const auto accepted=result;
    source[4]=std::numeric_limits<float>::quiet_NaN();
    assert(!flashlightProfileRange::normalizeObservedFloor(source.data(),result));assert(result==accepted);
    source=original;source[256]=.95f;
    assert(!flashlightProfileRange::normalizeObservedFloor(source.data(),result));
    source.fill(1);assert(!flashlightProfileRange::normalizeObservedFloor(source.data(),result));
    source.fill(.95f);assert(!flashlightProfileRange::normalizeObservedFloor(source.data(),result));
    source.fill(0);assert(!flashlightProfileRange::normalizeObservedFloor(source.data(),result));
    assert(!flashlightProfileRange::normalizeObservedFloor(nullptr,result));
    puts("PASS: observed-floor normalization, source preservation, invalid and unrelated profile rejection");
}
