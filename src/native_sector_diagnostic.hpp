#pragma once
namespace nativeSectorDiagnostic {
// Native96710 has an explicit camera-null branch that stamps every loaded
// sector visible. Disable only that call's camera, then restore it. Never
// change the native projection/view matrices or leave a global bypass active.
template<class Callback> inline void withoutCamera(void** camera,Callback callback){
    struct Restore {void** target;void* saved;~Restore(){*target=saved;}} restore{camera,*camera};
    *camera=nullptr;callback();
}
}
