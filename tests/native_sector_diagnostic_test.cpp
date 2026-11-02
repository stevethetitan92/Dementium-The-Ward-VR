#include "../src/native_sector_diagnostic.hpp"
#include "../src/geometry_isolation.hpp"
#include <cassert>
#include <stdexcept>
#include <cstdio>
int main(){
    geometryIsolation::set(geometryIsolation::NoSectorVisibility,1000);
    assert(geometryIsolation::mode(1001)==geometryIsolation::NoSectorVisibility);
    assert(geometryIsolation::mode(45999)==geometryIsolation::NoSectorVisibility);
    assert(geometryIsolation::mode(46000)==geometryIsolation::Normal);
    geometryIsolation::set(geometryIsolation::Normal,2000);
    assert(geometryIsolation::mode(2000)==geometryIsolation::Normal);
    int camera{},otherCamera{};void* slot=&camera;void* other=&otherCamera;unsigned stamp=7;
    nativeSectorDiagnostic::withoutCamera(&slot,[&]{assert(!slot&&other==&otherCamera);++stamp;});
    assert(slot==&camera&&stamp==8&&other==&otherCamera);
    try{nativeSectorDiagnostic::withoutCamera(&slot,[&]{assert(!slot);throw std::runtime_error("test unwind");});assert(false);}catch(const std::runtime_error&){}
    assert(slot==&camera);
    nativeSectorDiagnostic::withoutCamera(&slot,[&]{
        nativeSectorDiagnostic::withoutCamera(&slot,[&]{assert(!slot);});assert(!slot);
    });
    assert(slot==&camera);slot=nullptr;
    nativeSectorDiagnostic::withoutCamera(&slot,[&]{assert(!slot);});assert(!slot);
    std::puts("PASS: bounded-sector diagnostic restores the owning camera across normal, nested and unwind paths without undoing visibility output; no game/headset.");
}
