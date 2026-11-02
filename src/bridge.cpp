#include "stereo_pipeline_samples.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "init_diagnostics.hpp"
#include <d3d12.h>
#include "texture_upload_metadata.hpp"
#include "flashlight_profile_upload.hpp"
#include "pickup_visibility_history.hpp"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <d3d12sdklayers.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <MinHook.h>
#include "debug_layer_startup.hpp"
#include "native_swarm_bounds.hpp"
#include "native_present_cadence.hpp"
#include "native_simulation_clock.hpp"
#include "draw_cost_profile.hpp"
#include "frame_tail_profile.hpp"
#include "mirror_present_policy.hpp"
#include <cstdio>
#include <mutex>
#include <atomic>
#include <cstdarg>
#include <unordered_map>
#include <share.h>
#include <intrin.h>
#include "stereo.hpp"
#include "native_ui_audit.hpp"
#include "mirror.hpp"
#include "gamepad_bridge.hpp"
#include "weapon_comfort.hpp"
#include "motion_native.hpp"
#include "interaction_gaze.hpp"
#include "arsenal_props.hpp"
#include "wristband_native.hpp"
#include "wristband_renderer.hpp"
#include "pill_inventory_ui.hpp"
#include "native_hud_policy.hpp"
#include "medicine_saves.hpp"
#include "swapchain_queue.hpp"
#include "sphere_frustum.hpp"
#include "render_history.hpp"
#include "native_sector_diagnostic.hpp"
#include "native_uniform_diagnostic.hpp"
#include "native_flashlight_profile_diagnostic.hpp"
#include "native_copy_reader.hpp"
#include "bounded_sample_gate.hpp"
#include "native_zero_constant_guard.hpp"
#include "native_copy_forwarders.hpp"
#include "native_copy_install.hpp"
#include "native_copy_endpoints.hpp"
#include "audio_list_diagnostic.hpp"
#include "audio_fault_evidence.hpp"
#include "left_hand_alignment.hpp"
#include "save_slot_ui.hpp"
using Microsoft::WRL::ComPtr;
static HMODULE self{};
static FILE* logFile{};
static std::mutex logMutex;
static std::atomic<unsigned> presents{},draws{},pipelines{};
static std::atomic<std::uint64_t> meleeReplayStarted{},meleeReplayUntil{};
static std::atomic<std::uint64_t> ejectionSideViewUntil{};
// Publish ticket and requested presentation together. A late recording thread
// must not revive a one-frame diagnostic after the requested frame has ended.
static std::atomic<std::uint64_t> sceneEvidenceRequest{};
// The native zero-byte binding can reach the ring end during ordinary play
// (2026-10-26 crash). Keep the exact-proof guard active for the session;
// diagnostic commands may still temporarily enable or explicitly disable it.
static std::atomic<ULONGLONG> zeroConstantGuardUntil{(std::numeric_limits<ULONGLONG>::max)()};
static std::atomic<unsigned> zeroConstantRedirects{};
static bool useOriginalSniper=true;
static bool useOriginalHands=true;
static wchar_t trackingProfile[MAX_PATH]{};
static std::atomic<bool> leftAlignmentRequested{};
static std::atomic<bool> ammoPouchRequested{};
static XrQuaternionf leftAlignmentCorrection{0,0,0,1};
using NativeSphereTest=float(*)(const void*,const float*,float);
static NativeSphereTest originalSphereTest{};
static std::atomic<unsigned> diagnosticSphereRejections{};
static std::atomic<unsigned> stereoPickupRestores{};
// Exclusive outcomes for scoped native rejections. Restores alone cannot tell
// whether a remaining disappearance came from clipping or an unavailable view.
static std::atomic<unsigned> pickupTrialQueries{},pickupTrialBusy{},pickupTrialNoView{},pickupTrialBadCamera{},pickupTrialOutside{};
// Candidate remains opt-in until live stereo/frame-coherence acceptance.
// Offline math tests cannot establish the recorded scene behavior.
static std::atomic<bool> stereoPickupCullingTrial{};
using NativeSectorVisibility=void(*)(void*,float,float,void*);
static NativeSectorVisibility originalSectorVisibility{};
static std::atomic<unsigned> diagnosticSectorCalls{};
// Remember only the camera explicitly supplied by the native sector query.
// A later sphere query must use that exact plane block, in the same short
// frame window. Never infer a camera from arbitrary plane-array memory.
static thread_local const unsigned char* worldVisibilityCamera{};
static thread_local ULONGLONG worldVisibilityCameraAt{};
static std::atomic<bool> stereoWorldCullingTrial{};
static std::atomic<bool> stereoSectorVisibility{};
static void sectorVisibilityDiagnostic(void* query,float first,float second,void* options){
    worldVisibilityCamera=nullptr;
    if(manualReload::readable(query,0xc8)){
        const unsigned char* camera{};
        std::memcpy(&camera,static_cast<unsigned char*>(query)+0xc0,sizeof camera);
        if(camera&&manualReload::readable(camera,0xec)){
            worldVisibilityCamera=camera;worldVisibilityCameraAt=GetTickCount64();
        }
    }
    auto native=[&]{originalSectorVisibility(query,first,second,options);};
    bool headsetSectors=false;
    if(stereoSectorVisibility.load()){
        std::shared_lock<std::shared_mutex> guard(stereo::gate,std::try_to_lock);
        headsetSectors=guard.owns_lock()&&stereo::active&&stereo::originSet&&stereo::xr&&stereo::xr->headTracked;
    }
    // The original angular portal traversal uses a single desktop camera.
    // Consider loaded sectors in VR; the downstream per-object headset-eye
    // frustum still rejects objects outside both eyes. Native RAII restores
    // the query camera immediately, including exceptional exits.
    if((!headsetSectors&&geometryIsolation::mode(GetTickCount64())!=geometryIsolation::NoSectorVisibility)||!manualReload::readable(query,0xc8)){native();return;}
    auto camera=reinterpret_cast<void**>(static_cast<unsigned char*>(query)+0xc0);
    nativeSectorDiagnostic::withoutCamera(camera,native);++diagnosticSectorCalls;
}
static bool installSectorDiagnostic(){
    auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char code[]={0x4c,0x89,0x4c,0x24,0x20,0x55,0x53,0x56,0x57,0x48,0x8d,0xac,0x24,0xc8,0xfb,0xff,0xff,0x48,0x81,0xec,0x38,0x05,0,0};
    auto target=reinterpret_cast<void*>(image+0x96710);
    return !std::memcmp(target,code,sizeof code)&&MH_CreateHook(target,reinterpret_cast<void*>(sectorVisibilityDiagnostic),reinterpret_cast<void**>(&originalSectorVisibility))==MH_OK;
}
static pickupVisibilityHistory::History pickupTransitions;
static std::atomic<unsigned> pickupDumpSerial{};
static std::atomic<bool> desktopInventoryToggle{};
static std::atomic<std::uint64_t> inventoryTestUntil{};
static std::atomic<unsigned> desktopInventoryAction{};
extern "C" float nativeSphereEntry(const void*,const float*,float);
extern "C" float sphereTestDiagnostic(const void* planes,const float* center,float radius){
    std::array<float,4> observedSphere{};std::array<float,24> observedPlanes{};bool sphereKnown=false,planesKnown=false;
    if(pillBottle::pickupUpdateScope&&planes==pillBottle::pickupFrustumScope&&pickupTransitions.armed()&&pickupTransitions.mayCapture(GetTickCount64())){
        if(manualReload::readable(center,3*sizeof(float))){std::memcpy(observedSphere.data(),center,3*sizeof(float));observedSphere[3]=radius;sphereKnown=true;}
        if(manualReload::readable(planes,sizeof observedPlanes)){std::memcpy(observedPlanes.data(),planes,sizeof observedPlanes);planesKnown=true;}
    }
    float result=originalSphereTest(planes,center,radius);
    auto finish=[&](float effective){
        if(pillBottle::pickupUpdateScope&&planes==pillBottle::pickupFrustumScope){
            pillBottle::pickupCull={true,result,effective};
            pillBottle::pickupCull.sphereKnown=sphereKnown;pillBottle::pickupCull.sphere=observedSphere;
            pillBottle::pickupCull.planesKnown=planesKnown;pillBottle::pickupCull.testedPlanes=observedPlanes;
        }
        return effective;
    };
    if(result==0&&geometryIsolation::mode(GetTickCount64())==geometryIsolation::NoFrustum){
        ++diagnosticSphereRejections;return finish(.0001f);
    }
    const bool pickupCamera=stereoPickupCullingTrial.load()&&pillBottle::pickupUpdateScope&&planes==pillBottle::pickupFrustumScope;
    const bool worldCamera=stereoWorldCullingTrial.load()&&worldVisibilityCamera&&
        planes==worldVisibilityCamera+0x1c&&GetTickCount64()-worldVisibilityCameraAt<100;
    if(result==0&&(pickupCamera||worldCamera)){
        ++pickupTrialQueries;
        std::shared_lock<std::shared_mutex> guard(stereo::gate,std::try_to_lock);
        if(!guard.owns_lock()){++pickupTrialBusy;return finish(result);}
        if(stereo::active&&stereo::originSet&&stereo::xr&&stereo::xr->headTracked){
            // Native 7C95C passes query->camera +1C to this exact predicate.
            // Projection +7C and view +BC belong to that SAME camera block.
            // LoadSection embeds the query at +8; native3A118/3A526 store its
            // camera at outer+C8 (query+C0). Do not use an unrelated global
            // camera or retain this owning-call pointer after the scope ends.
            auto camera=static_cast<const unsigned char*>(planes)-0x1c;
            if(manualReload::readable(camera,0xec)){
                float native[28];std::memcpy(native,camera+0x7c,64);std::memcpy(native+16,camera+0xbc,48);
                for(const auto& eye:stereo::xr->eyes){
                    float data[28];std::memcpy(data,native,sizeof data);stereo::transform(data,eye.view);
                    if(sphereFrustum::contains(data,data+16,center,radius)){++stereoPickupRestores;return finish(.0001f);}
                }
                ++pickupTrialOutside;
            }else ++pickupTrialBadCamera;
        }else ++pickupTrialNoView;
    }
    return finish(result);
}
static bool installSphereDiagnostic(){
    auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char code[]={0x48,0x83,0xec,0x28,0xf3,0x0f,0x10,0x62,0x08,0x33,0xc0,0xf3,0x0f,0x10,0x2a,0x0f};
    auto target=reinterpret_cast<void*>(image+0x7b7f0);
    return !std::memcmp(target,code,sizeof code)&&MH_CreateHook(target,reinterpret_cast<void*>(nativeSphereEntry),reinterpret_cast<void**>(&originalSphereTest))==MH_OK;
}
static thread_local bool internal=false;
struct BindingTrace {void* list{};UINT index{};UINT64 address{};bool injected{};};
static thread_local BindingTrace recentBindings[16];
static thread_local unsigned bindingCursor{};
static thread_local renderHistory::History<> renderEvents;
static thread_local renderHistory::Stage renderStage=renderHistory::Stage::Native;
// Copy-layout errors need queue/copy provenance, not only graphics bindings.
// Raw identities and by-value arguments only: no resource lifetime ownership,
// barriers, extra submissions or per-copy logging.
using TextureCopyTrace=copyProvenance::Trace;
static thread_local renderHistory::History<16,TextureCopyTrace> recentTextureCopies;
using BarrierTrace=copyProvenance::BarrierTrace;
static thread_local renderHistory::History<16,BarrierTrace> recentBarriers;
struct RenderStageScope {
    renderHistory::Stage previous=renderStage;
    explicit RenderStageScope(renderHistory::Stage stage){renderStage=stage;}
    ~RenderStageScope(){renderStage=previous;}
};
struct InternalScope {
    bool previous=internal;
    InternalScope(){internal=true;}
    ~InternalScope(){internal=previous;}
};
static void log(const char* format,...) {
    std::lock_guard<std::mutex> guard(logMutex);
    if(initDiagnostics::observing){char text[2048]{};va_list diagnosticArgs;va_start(diagnosticArgs,format);_vsnprintf_s(text,sizeof text,_TRUNCATE,format,diagnosticArgs);va_end(diagnosticArgs);initDiagnostics::write("%s",text);}
    if(!logFile) return;
    va_list args; va_start(args,format); vfprintf(logFile,format,args); va_end(args);
    fputc('\n',logFile); fflush(logFile);
}
// Isolated, opt-in evidence collection only. Sampling does not alter the list.
// Use time rather than the 300-frame summary: short-lived voices were absent
// from every previous summary snapshot. Still not a complete event trace.
static void sampleAudioList() {
    static const bool enabled=[](){wchar_t value[8]{};return GetEnvironmentVariableW(L"DEMENTIUM_AUDIO_LIST_AUDIT",value,8)==1&&value[0]==L'1';}();
    if(!enabled)return;
    static std::mutex sampleMutex;std::unique_lock<std::mutex> lock(sampleMutex,std::try_to_lock);
    if(!lock.owns_lock())return;
    static ULONGLONG sampled{},reported{};const auto now=GetTickCount64();
    if(sampled&&now-sampled<100)return;sampled=now;
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    static const bool matches=[base](){
        unsigned char signature[8]{};SIZE_T copied=0;
        const unsigned char expected[]={0x48,0x83,0xec,0x28,0x48,0x8b,0x51,0x18};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+0xfd6a0),signature,sizeof signature,&copied)&&copied==sizeof signature&&!memcmp(signature,expected,sizeof signature);
    }();
    if(!matches){if(!reported)log("audio_list_snapshot signature_mismatch=1");reported=now;return;}
    const auto result=audioListDiagnostic::snapshot(base+0x3a81c8,[](std::uintptr_t address,std::uintptr_t& value){SIZE_T bytes=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof value,&bytes)&&bytes==sizeof value;});
    static audioListDiagnostic::Result previous{};static unsigned samples{},nonempty{},anomalies{},peak{};
    ++samples;if(result.nodes)++nonempty;if(result.status!=audioListDiagnostic::Status::complete)++anomalies;
    peak=(std::max)(peak,result.nodes);
    if(!reported||now-reported>=5000||result.status!=previous.status||result.nodes!=previous.nodes||result.address!=previous.address){
        log("audio_list_snapshot status=%s nodes=%u address=%llx previous=%llx samples=%u nonempty=%u anomalies=%u peak=%u interval_ms=100 concurrent_mutation_possible=1",audioListDiagnostic::name(result.status),result.nodes,result.address,result.previous,samples,nonempty,anomalies,peak);
        reported=now;
    }
    previous=result;
}
static void CALLBACK graphicsMessage(D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID id,LPCSTR description,void*) {
    if(severity>D3D12_MESSAGE_SEVERITY_WARNING)return;
    static std::mutex countsMutex;static std::unordered_map<int,unsigned> counts;
    unsigned count;{std::lock_guard<std::mutex> guard(countsMutex);count=++counts[int(id)];}
    // Preserve first occurrences and exponential recurrence counts, including
    // errors; avoid synchronous disk flushing thousands of identical messages.
    if(count<=3||(count&(count-1))==0)log("graphics_message severity=%d id=%d count=%u %s",int(severity),int(id),count,description);
    if((id==201||id==710)&&count<=3){
        renderEvents.each([](const renderHistory::Event& event){
            log("render_failure_history event=%s stage=%s list=%p value=%llx index=%u mod_call=%d",renderHistory::name(event.kind),renderHistory::name(event.stage),event.list,event.value,event.index,event.injected);
        });
    }
    if(id==1334&&count<=3){
        recentBarriers.each([](const BarrierTrace& event){const auto& barrier=event.barrier;
            log("copy_failure_barrier list=%p caller=%llx stage=%s mod_call=%d requested=%u index=%u known=%d type=%u flags=%u resource=%p before=%u after=%u subresource=%u",event.list,static_cast<unsigned long long>(event.caller),renderHistory::name(event.stage),event.injected,event.requested,event.index,event.known,static_cast<unsigned>(barrier.Type),static_cast<unsigned>(barrier.Flags),event.known&&barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION?barrier.Transition.pResource:nullptr,event.known&&barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION?static_cast<unsigned>(barrier.Transition.StateBefore):0,event.known&&barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION?static_cast<unsigned>(barrier.Transition.StateAfter):0,event.known&&barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION?barrier.Transition.Subresource:UINT_MAX);
        });
        if(const auto* submission=copyProvenance::active){
            log("copy_failure_submission queue=%p queue_type=%u submit_thread=%u requested=%u captured=%u omitted=%u active_call=1",submission->queue,submission->queueType,submission->thread,submission->requested,submission->captured,submission->requested-submission->captured);
            for(unsigned i=0;i<submission->captured;++i){const auto& list=submission->lists[i];log("copy_failure_submitted_list index=%u list=%p known=%d recorded_copies=%zu",i,list.identity,list.known,list.copies.count);
                log("copy_failure_submitted_barriers list=%p known=%d recorded_barriers=%zu",list.identity,list.barriersKnown,list.barriers.count);
                list.barriers.each([](const BarrierTrace& event){const auto& barrier=event.barrier;if(event.known&&barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)log("copy_failure_submitted_transition list=%p caller=%llx stage=%s mod_call=%d requested=%u index=%u resource=%p before=%u after=%u subresource=%u flags=%u",event.list,static_cast<unsigned long long>(event.caller),renderHistory::name(event.stage),event.injected,event.requested,event.index,barrier.Transition.pResource,static_cast<unsigned>(barrier.Transition.StateBefore),static_cast<unsigned>(barrier.Transition.StateAfter),barrier.Transition.Subresource,static_cast<unsigned>(barrier.Flags));});
                list.copies.each([](const TextureCopyTrace& copy){log("copy_failure_submitted_copy list=%p record_thread=%u stage=%s mod_call=%d dst=%p dst_type=%u src=%p src_type=%u",copy.list,copy.recordingThread,renderHistory::name(copy.stage),copy.injected,copy.destination.pResource,copy.destination.Type,copy.source.pResource,copy.source.Type);});
            }
        }else log("copy_failure_submission active_call=0 correlation_unavailable=1");
        recentTextureCopies.each([](const TextureCopyTrace& copy){
            log("copy_failure_history list=%p queue_type=%u stage=%s mod_call=%d dst=%p dst_type=%u dst_sub=%u dst_offset=%llu src=%p src_type=%u src_sub=%u src_offset=%llu xyz=%u,%u,%u box=%d:%u,%u,%u,%u,%u,%u",
                copy.list,static_cast<unsigned>(copy.queueType),renderHistory::name(copy.stage),copy.injected,
                copy.destination.pResource,static_cast<unsigned>(copy.destination.Type),copy.destination.Type==D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX?copy.destination.SubresourceIndex:UINT_MAX,copy.destination.Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT?copy.destination.PlacedFootprint.Offset:0,
                copy.source.pResource,static_cast<unsigned>(copy.source.Type),copy.source.Type==D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX?copy.source.SubresourceIndex:UINT_MAX,copy.source.Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT?copy.source.PlacedFootprint.Offset:0,
                copy.x,copy.y,copy.z,copy.hasBox,copy.box.left,copy.box.top,copy.box.front,copy.box.right,copy.box.bottom,copy.box.back);
        });
    }
    if(id==710&&count<=3){
        // Preserve the incoming call and nested debug-wrapper forwarding, not
        // merely the last periodic sample before a fatal validation exception.
        for(unsigned i=0;i<16;i++){const auto& b=recentBindings[(bindingCursor+i)%16];if(b.list)log("cbv_failure_history list=%p index=%u address=%llx mod_call=%d",b.list,b.index,b.address,b.injected);}
        auto address=recentBindings[(bindingCursor-1)%16].address;
        nativeUniformDiagnostic::Snapshot native;
        if(nativeUniformDiagnostic::capture(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),native,[](std::uintptr_t source,void* output,size_t bytes){
            if(!manualReload::readable(reinterpret_cast<void*>(source),bytes))return false;
            std::memcpy(output,reinterpret_cast<void*>(source),bytes);return true;
        }))log("cbv_failure_native_ring ring=%p frame=%d capacity=%llx cursor=%llx gpu=%llx resource=%p cpu=%p native_list=%p shader=%p vs_known=%d vs_bytes=%d ps_known=%d ps_bytes=%d dirty=%llx address_at_end=%d address_inside=%d",
            reinterpret_cast<void*>(native.ring),native.frame,native.capacity,native.cursor,native.gpu,reinterpret_cast<void*>(native.resource),reinterpret_cast<void*>(native.cpu),reinterpret_cast<void*>(native.commandList),reinterpret_cast<void*>(native.shader),native.vertexKnown,native.vertexBytes,native.pixelKnown,native.pixelBytes,native.dirty,native.atEnd(address),native.contains(address));
        else log("cbv_failure_native_ring unavailable_or_signature_mismatch=1");
        std::lock_guard<std::mutex> guard(stereo::mapMutex);
        for(const auto& item:stereo::uploads){const auto& u=item.second;if(item.first<=address&&address-item.first<u.size)log("cbv_failure_upload registered=%llx current=%llx size=%llu resource=%p",item.first,u.resource?u.resource->GetGPUVirtualAddress():0,u.size,u.resource.Get());}
    }
}
static void reportNativePickup(int id,bool accepted){
    static std::mutex mutex;static std::map<int,bool> seen;std::lock_guard<std::mutex> guard(mutex);
    auto found=seen.find(id);if(found!=seen.end()&&found->second==accepted)return;
    if(seen.size()>1024)seen.clear();seen[id]=accepted;
    log("native_pickup_attempt id=%d accepted=%d",id,accepted);
}
static void reportPickupVisibility(void* entity,int id,bool visible,int plane,float margin,const pillBottle::CullObservation& cull){
    struct Seen {bool visible{},tested{},rejected{};std::uint64_t time{};nativePickupSector::Status sector=nativePickupSector::Status::Unknown;};
    using Key=std::pair<std::uintptr_t,std::uintptr_t>;
    static std::mutex mutex;static std::map<Key,Seen> seen;
    if(id<100||id>=1000)return;
    auto now=GetTickCount64();std::lock_guard<std::mutex> guard(mutex);
    auto scene=*reinterpret_cast<std::uintptr_t*>(pillBottle::image+0x3a5330);
    if(pickupTransitions.mayCapture(now)){
        pickupVisibilityHistory::Event event;event.scene=scene;event.entity=reinterpret_cast<std::uintptr_t>(entity);event.id=id;event.time=now;event.thread=GetCurrentThreadId();event.visible=visible;event.tested=cull.tested;event.nativeResult=cull.nativeResult;event.effectiveResult=cull.effectiveResult;event.plane=plane;event.margin=margin;event.sector=cull.sector;event.sphereKnown=cull.sphereKnown;event.sphere=cull.sphere;event.planesKnown=cull.planesKnown;event.testedPlanes=cull.testedPlanes;pickupTransitions.observe(event);
    }
    Key key{scene,reinterpret_cast<std::uintptr_t>(entity)};auto it=seen.find(key);
    bool rejected=cull.tested&&cull.nativeResult==0;
    if(it!=seen.end()&&((it->second.visible==visible&&it->second.tested==cull.tested&&it->second.rejected==rejected&&it->second.sector==cull.sector.status)||now-it->second.time<500))return;
    if(seen.size()>1024)seen.clear();seen[key]={visible,cull.tested,rejected,now,cull.sector.status};
    log("pickup_native_cull scene=%llx entity=%p id=%d visible=%d sphere_tested=%d native_result=%.6f effective_result=%.6f limiting_plane=%d sphere_margin=%.5f",scene,entity,id,visible,cull.tested,cull.nativeResult,cull.effectiveResult,plane,margin);
    const auto& sector=cull.sector;
    log("pickup_sector_snapshot scene=%llx entity=%p id=%d after_update=1 gate=%s camera=%p entity_sector=%p current_sector=%p count=%u limit=%u revision=%u record_relative=%x record=%p stamp=%u",scene,entity,id,nativePickupSector::name(sector.status),reinterpret_cast<void*>(sector.camera),reinterpret_cast<void*>(sector.entitySector),reinterpret_cast<void*>(sector.currentSector),sector.count,sector.limit,sector.revision,sector.relative,reinterpret_cast<void*>(sector.record),sector.stamp);
}
struct PickupDraw {pillBottle::Record record;int id{};unsigned count{},flags{};XrVector3f position{};};
static std::mutex pickupDrawMutex;
static std::map<std::pair<std::uintptr_t,std::uintptr_t>,PickupDraw> pickupDraws;
static void recordPickupDraw(void* entity,int id){
    if(id<0||id>=1000)return;
    std::lock_guard<std::mutex> guard(pickupDrawMutex);if(pickupDraws.size()>512)pickupDraws.clear();
    auto scene=*reinterpret_cast<std::uintptr_t*>(pillBottle::image+0x3a5330);
    auto& item=pickupDraws[{scene,reinterpret_cast<std::uintptr_t>(entity)}];item.id=id;item.record={entity,scene,GetTickCount64()};++item.count;
    auto e=static_cast<unsigned char*>(entity);float m[12];std::memcpy(m,e+8,sizeof m);item.position={m[3],m[7],m[11]};item.flags=e[0x44];
}
static void reportPickupDraws(){
    if(!pillBottle::installed)return;
    auto player=*reinterpret_cast<unsigned char**>(pillBottle::image+0x3a5340);if(!manualReload::readable(player,56))return;
    float body[12];std::memcpy(body,player+8,sizeof body);if(!motionTracking::validBasis(body))return;
    auto now=GetTickCount64();std::lock_guard<std::mutex> guard(pickupDrawMutex);unsigned reported=0;
    auto scene=*reinterpret_cast<std::uintptr_t*>(pillBottle::image+0x3a5330);
    for(const auto& entry:pickupDraws){const auto& item=entry.second;if(item.record.scene!=scene)continue;
        auto p=item.position;float dx=p.x-body[3],dy=p.y-body[7],dz=p.z-body[11],distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(!std::isfinite(distance)||distance>5||reported++>=8)continue;
        log("pickup_visibility scene=%llx entity=%p id=%d player_distance=%.3f render_age_ms=%llu render_calls=%u captured_flags=%02x position=%.3f,%.3f,%.3f",scene,item.record.entity,item.id,distance,now-item.record.time,item.count,item.flags,p.x,p.y,p.z);
    }
}
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Execute=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList*const*);
using Pipeline=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,REFIID,void**);
using Root=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,const void*,SIZE_T,REFIID,void**);
using Draw=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,UINT,UINT,UINT);
using DrawIndexed=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,UINT,UINT,INT,UINT);
using RootCBV=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,D3D12_GPU_VIRTUAL_ADDRESS);
using SetRootSignature=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12RootSignature*);
static SetRootSignature originalSetRootSignature{};
using RootTable=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,D3D12_GPU_DESCRIPTOR_HANDLE);
static Present originalPresent{}; static Execute originalExecute{}; static Pipeline originalPipeline{};
#ifdef DEMENTIUM_PROFILE_DRAWS
static thread_local double nativeSubmitMilliseconds{},nativeSubmitMaxMilliseconds{};
static thread_local unsigned nativeSubmitCalls{};
#endif
static Root originalRoot{}; static Draw originalDraw{}; static DrawIndexed originalIndexed{};
static RootCBV originalCBV{}; static RootTable originalTable{};
using Map=HRESULT(STDMETHODCALLTYPE*)(ID3D12Resource*,UINT,const D3D12_RANGE*,void**);
using Unmap=void(STDMETHODCALLTYPE*)(ID3D12Resource*,UINT,const D3D12_RANGE*);
using SetPipeline=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12PipelineState*);
using Viewports=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_VIEWPORT*);
using Scissors=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_RECT*);
using Targets=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,BOOL,const D3D12_CPU_DESCRIPTOR_HANDLE*);
using Reset=HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12CommandAllocator*,ID3D12PipelineState*);
using CreateCommandList=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,D3D12_COMMAND_LIST_TYPE,ID3D12CommandAllocator*,ID3D12PipelineState*,REFIID,void**);
static CreateCommandList originalCreateCommandList{};
using CreateRTV=void(STDMETHODCALLTYPE*)(ID3D12Device*,ID3D12Resource*,const D3D12_RENDER_TARGET_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);
static Map originalMap{};static Unmap originalUnmap{};static SetPipeline originalSetPipeline{};
static Viewports originalViewports{};static Scissors originalScissors{};static Targets originalTargets{};static Reset originalReset{};
static CreateRTV originalCreateRTV{};
using CreateSRV=void(STDMETHODCALLTYPE*)(ID3D12Device*,ID3D12Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);
using CopySimple=void(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_DESCRIPTOR_HEAP_TYPE);
using CopyDescriptors=void(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,D3D12_DESCRIPTOR_HEAP_TYPE);
using SetHeaps=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,ID3D12DescriptorHeap*const*);
static CreateSRV originalCreateSRV{};static CopySimple originalCopySimple{};
static CopyDescriptors originalCopyDescriptors{};static SetHeaps originalSetHeaps{};
using CopyResource=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12Resource*,ID3D12Resource*);
using CopyTexture=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,const D3D12_TEXTURE_COPY_LOCATION*,UINT,UINT,UINT,const D3D12_TEXTURE_COPY_LOCATION*,const D3D12_BOX*);
using ResourceBarriers=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_RESOURCE_BARRIER*);
static ResourceBarriers originalResourceBarriers{};
using Resolve=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12Resource*,UINT,ID3D12Resource*,UINT,DXGI_FORMAT);
using ClearRTV=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,const FLOAT*,UINT,const D3D12_RECT*);
static CopyResource originalCopyResource{};static CopyTexture originalCopyTexture{};static Resolve originalResolve{};static ClearRTV originalClearRTV{};
using VertexBuffers=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,UINT,const D3D12_VERTEX_BUFFER_VIEW*);
using IndexBuffer=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,const D3D12_INDEX_BUFFER_VIEW*);
static VertexBuffers originalVertexBuffers{};static IndexBuffer originalIndexBuffer{};
using CopyBuffer=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12Resource*,UINT64,ID3D12Resource*,UINT64,UINT64);
static CopyBuffer originalCopyBuffer{};
using CreateSwap=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateSwapHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
static CreateSwap originalCreateSwap{};static CreateSwapHwnd originalCreateSwapHwnd{};
static HRESULT STDMETHODCALLTYPE createSwapHook(IDXGIFactory* factory,IUnknown* queue,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** out){
    auto result=originalCreateSwap(factory,queue,desc,out);if(SUCCEEDED(result)&&out&&*out)log("swapchain_queue_bound=%d chain=%p queue=%p",swapchainQueue::bind(*out,queue),*out,queue);return result;
}
static HRESULT STDMETHODCALLTYPE createSwapHwndHook(IDXGIFactory2* factory,IUnknown* queue,HWND window,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGIOutput* output,IDXGISwapChain1** out){
    auto result=originalCreateSwapHwnd(factory,queue,window,desc,fullscreen,output,out);if(SUCCEEDED(result)&&out&&*out)log("swapchain_queue_bound=%d chain=%p queue=%p",swapchainQueue::bind(*out,queue),*out,queue);return result;
}
static UINT desktopPresentFlags(IDXGISwapChain* chain,UINT sync,UINT flags) {
    static const bool enabled=[] { char value[4]{};return GetEnvironmentVariableA("DEMENTIUM_TEST_UNCAPPED_MIRROR",value,sizeof(value))==1&&value[0]=='1'; }();
    if(!enabled||!stereo::active||stereo::failed||(flags&DXGI_PRESENT_TEST))return flags;
    // Restrict this optional policy to the native game presentation queue.
    if(swapchainQueue::lookup(chain).Get()!=stereo::gameQueue.Get())return flags;
    static const bool supported=[chain] {
        ComPtr<IDXGIFactory5> factory;BOOL allow=FALSE;
        return SUCCEEDED(chain->GetParent(IID_PPV_ARGS(&factory)))&&
            SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&allow,sizeof(allow)))&&allow;
    }();
    DXGI_SWAP_CHAIN_DESC desc{};BOOL fullscreen=TRUE;
    const bool known=SUCCEEDED(chain->GetDesc(&desc))&&SUCCEEDED(chain->GetFullscreenState(&fullscreen,nullptr));
    const UINT selected=mirrorPresentPolicy::flags(enabled,true,supported,known&&!fullscreen,desc.Flags,sync,flags);
    static bool logged=false;
    if(!logged){logged=true;log("mirror_present_policy test=1 supported=%d known=%d fullscreen=%d creation_flags=%u sync=%u original_flags=%u selected_flags=%u",supported,known,fullscreen,desc.Flags,sync,flags,selected);}
    return selected;
}
static HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* chain,UINT sync,UINT flags) {
#ifdef DEMENTIUM_PROFILE_DRAWS
    frameTail::tracker.begin(stereo::preciseMilliseconds());
    drawCost::present(stereo::xr?double(stereo::xr->predictedPeriod)/1000000.:0.);
#endif

    static double hookTime{},desktopTime{};double hookStarted=stereo::preciseMilliseconds();double tickMilliseconds{},xrEndMilliseconds{};
    if(!internal&&!(flags&DXGI_PRESENT_TEST)) {
        internal=true;
        sampleAudioList();
        if(!stereo::gameQueue){auto owner=swapchainQueue::lookup(chain);if(owner){std::unique_lock<std::shared_mutex> lock(stereo::gate);stereo::gameQueue=owner;owner->GetDevice(IID_PPV_ARGS(&stereo::gameDevice));log("presentation_queue=%p",owner.Get());}}
        if(!stereo::xr) {
            DXGI_SWAP_CHAIN_DESC desc{};chain->GetDesc(&desc);
#ifdef DEMENTIUM_PROFILE_DRAWS
            BOOL fullscreen=FALSE;ComPtr<IDXGIOutput> output;DXGI_OUTPUT_DESC outputDesc{};
            const HRESULT fullscreenStatus=chain->GetFullscreenState(&fullscreen,nullptr);
            if(SUCCEEDED(chain->GetContainingOutput(&output)))output->GetDesc(&outputDesc);
            DEVMODEW display{};display.dmSize=sizeof(display);const BOOL modeKnown=*outputDesc.DeviceName&&EnumDisplaySettingsW(outputDesc.DeviceName,ENUM_CURRENT_SETTINGS,&display);
            log("presentation_display output=%ls mode_known=%d refresh=%lu fullscreen_status=%08lx fullscreen=%d swap_refresh=%u/%u flags=%u effect=%u buffers=%u",outputDesc.DeviceName,modeKnown,modeKnown?display.dmDisplayFrequency:0,fullscreenStatus,fullscreen,desc.BufferDesc.RefreshRate.Numerator,desc.BufferDesc.RefreshRate.Denominator,desc.Flags,desc.SwapEffect,desc.BufferCount);
#endif
            stereo::desktopWidth=desc.BufferDesc.Width;stereo::desktopHeight=desc.BufferDesc.Height;
            
            std::lock_guard<std::mutex> guard(stereo::mapMutex);
            for(UINT i=0;i<desc.BufferCount;i++) {ComPtr<ID3D12Resource> buffer;if(SUCCEEDED(chain->GetBuffer(i,IID_PPV_ARGS(&buffer)))){stereo::backbuffers.insert(buffer.Get());buffer->SetPrivateData(stereo::backbufferTag,sizeof(i),&i);}}
        }
        if(GetAsyncKeyState(VK_F6)&1)stereo::recenterRequested=true;
        if(GetAsyncKeyState(VK_F8)&1)stereo::captureRequested=true;
        if(stereo::xr){
            const auto& controls=stereo::xr->controls;
            bool available=nativeMotion::enabled&&stereo::active&&stereo::originSet&&stereo::xr->headTracked&&stereo::xr->state==XR_SESSION_STATE_FOCUSED&&controls.hand[1].aimValid&&gamepadBridge::menuState()==0&&!pillInventory::open();
            sniperScope::update(GetTickCount64(),available&&!arsenal::block()&&!bodyHolsters::hidden.load(),controls.input.leftTrigger,controls.input.b||controls.output.nextItem);
        }else sniperScope::update(GetTickCount64(),false,0,false);
        try {if(stereo::xr){int menu=gamepadBridge::menuState();if(menu>=0){auto desired=(menu||pillInventory::open())?vrControls::Context::inventory:vrControls::Context::gameplay;if(stereo::xr->controls.router.currentContext()!=desired)stereo::xr->controls.router.setContext(desired);}}unsigned eyeDraws=0;const double tickStarted=stereo::preciseMilliseconds(),endBefore=stereo::xr?stereo::xr->endMilliseconds:0.;const bool tickReady=stereo::tick(eyeDraws,chain);tickMilliseconds=stereo::preciseMilliseconds()-tickStarted;xrEndMilliseconds=stereo::xr?stereo::xr->endMilliseconds-endBefore:0.;if(tickReady&&(stereo::frames<5||stereo::frames%300==0)) {
            if(stereo::frames<5&&stereo::constants)log("vr_constants_va=%llx bytes=%llu",stereo::constants->GetGPUVirtualAddress(),stereo::capacity);
            static ULONGLONG previousTime=GetTickCount64();static unsigned previousFrame=stereo::frames;auto now=GetTickCount64();if(now>previousTime)log("frame_rate=%.1f",1000.*(stereo::frames-previousFrame)/(now-previousTime));previousTime=now;previousFrame=stereo::frames;
            log("xr_frame=%u eye_draws=%u ui_draws=%u active=%d direct=%u bundle=%u",stereo::frames,eyeDraws,stereo::lastUIDraws,stereo::active,stereo::directDraws.load(),stereo::bundleDraws.load());
#ifdef DEMENTIUM_PROFILE_DRAWS
            log("world_refresh_ticks fresh=%u cached=%u non_gameplay=%u scope=gameplay_presents_with_new_world_draws_not_compositor_frames",stereo::freshWorldTicks,stereo::cachedWorldTicks,stereo::nonGameplayTicks);
            log("world_refresh_sequence chain=%p thread=%lu sequence=%s",chain,GetCurrentThreadId(),stereo::worldCadence);
            log("native_present_cadence corrected=%llu original_ratio=%llu/%llu",nativePresentCadence::corrected,nativePresentCadence::observedNumerator,nativePresentCadence::observedDenominator);
            log("native_simulation_clock hz=%.6f updates=%llu extra_renders=%llu seconds=%.6f",nativeSimulationClock::nativeHz,nativeSimulationClock::updates,nativeSimulationClock::extraRenders,nativeSimulationClock::seconds());
            stereo::worldCadenceCount=0;stereo::worldCadence[0]=0;
            {static bool reported[2]{};for(unsigned k=0;k<2;k++)if(!reported[k]&&stereo::worldCallerDepth[k]){for(unsigned j=0;j<stereo::worldCallerDepth[k];j++)log("world_caller kind=%s depth=%u address=%p game_base=%p",k?"fresh":"cached",j,stereo::worldCallerStacks[k][j],GetModuleHandleW(nullptr));reported[k]=true;}}
            log("world_execution_ticks cached_with_executed_world=%u unknown_lists=%u scope=diagnostic_command_list_reuse",stereo::cachedWithExecutedWorld,stereo::unknownExecutedLists.exchange(0));
            stereo::freshWorldTicks=stereo::cachedWorldTicks=stereo::nonGameplayTicks=stereo::cachedWithExecutedWorld=0;
#endif
            reportPickupDraws();
            log("timing_gpu_ms_per_frame=%.3f xr_wait_ms_per_frame=%.3f origin_y=%.3f head_y=%.3f",stereo::gpuWaitMilliseconds/300,stereo::xrWaitMilliseconds/300,stereo::origin.position.y,stereo::xr->eyes[0].view.pose.position.y);stereo::gpuWaitMilliseconds=stereo::xrWaitMilliseconds=0;
            log("head_reference committed=%d floor_valid=%d floor_height=%.4f posture=%s seated=%.4f standing=%.4f calibrated=%d rebases=%u",stereo::originSet,stereo::xr->floorHeightValid,stereo::xr->floorHeight,headReference::name(stereo::postures.mode()),stereo::postures.height(headReference::Posture::Seated),stereo::postures.height(headReference::Posture::Standing),stereo::postures.ready(),stereo::referenceRebases);
            for(auto mode:{headReference::Posture::Seated,headReference::Posture::Standing}){static float saved[2]{};unsigned i=mode==headReference::Posture::Standing;float h=stereo::postures.height(mode);if(h>.5f&&std::fabs(h-saved[i])>.0001f){wchar_t number[32];swprintf_s(number,L"%.5f",h);if(WritePrivateProfileStringW(L"Tracking",i?L"StandingHeight":L"SeatedHeight",number,trackingProfile))saved[i]=h;}}
            log("timing_hook_ms=%.3f desktop_present_ms=%.3f",hookTime/300,desktopTime/300);hookTime=desktopTime=0;
#ifdef DEMENTIUM_PROFILE_DRAWS
            {auto tail=frameTail::tracker.take();log("slowest_interval_parts_ms total=%.3f hook=%.3f desktop_present=%.3f outside_present=%.3f xr_begin_in_hook=%.3f world_completion_in_hook=%.3f stereo_tick_in_hook=%.3f xr_end_in_tick=%.3f scope=same_interval_cpu_wall_time",tail.interval,tail.hook,tail.desktopPresent,tail.outsidePresent,tail.xrBegin,tail.worldCompletion,tail.stereoTick,tail.xrEnd);}
#endif
#ifdef DEMENTIUM_PROFILE_DRAWS
            {log("gpu_mod_batches_ms_per_frame=%.3f batches=%u timestamp_failures=%u scope=mod_owned_lists_not_native_world",stereo::batchGpuTimer.totalMilliseconds/300.,stereo::batchGpuTimer.samples,stereo::batchGpuTimer.failures);stereo::batchGpuTimer.resetTotals();if(stereo::xr)log("runtime_identity name=%s headset=%s eye_size=%ux%u samples=%u",stereo::xr->runtimeName,stereo::xr->systemName,stereo::xr->eyes[0].width,stereo::xr->eyes[0].height,stereo::sceneSamples);log("runtime_frame_period_ms=%.3f desktop_diagnostic=%d",stereo::xr?double(stereo::xr->predictedPeriod)/1000000.:0.,stereo::xr?int(stereo::xr->desktopDiagnostic):0);auto intervals=drawCost::takeFrames();log("frame_intervals_ms mean=%.3f p95_upper=%.3f p99_upper=%.3f max=%.3f over_60hz_budget=%u count=%u",intervals.mean,intervals.p95Upper,intervals.p99Upper,intervals.maximum,intervals.over60HzBudget,intervals.count);log("runtime_budget_overruns=%u count=%u scope=present_intervals_not_compositor_drops",intervals.overRuntimeBudget,intervals.count);log("fence_cpu_wait native_queue_ms_per_frame=%.3f owned_queue_ms_per_frame=%.3f max_single_wait_ms=%.3f native_calls=%u owned_calls=%u",stereo::nativeQueueWaitMilliseconds/300.,stereo::ownedQueueWaitMilliseconds/300.,stereo::maxQueueWaitMilliseconds,stereo::nativeQueueWaitCalls,stereo::ownedQueueWaitCalls);stereo::nativeQueueWaitMilliseconds=stereo::ownedQueueWaitMilliseconds=stereo::maxQueueWaitMilliseconds=0;stereo::nativeQueueWaitCalls=stereo::ownedQueueWaitCalls=0;log("world_completion_cpu_wait_ms_per_frame=%.3f",stereo::worldCompletionWaitMilliseconds/300.);stereo::worldCompletionWaitMilliseconds=0;log("native_gpu_batches_ms_per_frame=%.3f samples=%u failures=%u capacity_skips=%u foreign_thread_skips=%u enabled=%d scope=game_lists_including_injected_stereo_with_marker_overhead",stereo::nativeGpuTimer.milliseconds/300.,stereo::nativeGpuTimer.samples,stereo::nativeGpuTimer.failures,stereo::nativeGpuTimer.capacitySkips,stereo::nativeGpuTimer.foreignThreadSkips.load(),int(stereo::nativeGpuProfiling()));stereo::nativeGpuTimer.resetTotals();auto cost=drawCost::take(300);log("draw_cost_estimated_ms trace=%.3f source=%.3f stereo=%.3f ui=%.3f driver_inclusive=%.3f samples=%llu,%llu,%llu,%llu,%llu sampling=1in16",cost.estimatedMsPerFrame[0],cost.estimatedMsPerFrame[1],cost.estimatedMsPerFrame[2],cost.estimatedMsPerFrame[3],cost.estimatedMsPerFrame[4],cost.samples[0],cost.samples[1],cost.samples[2],cost.samples[3],cost.samples[4]);}
#endif
            log("capture_io busy=%d completed=%u failed=%u mode=cpu_worker_atomic_files",int(asyncPpmCapture::busy.load()),asyncPpmCapture::completed.load(),asyncPpmCapture::failed.load());
#ifdef DEMENTIUM_PROFILE_DRAWS
            log("native_submit_cpu_ms_per_frame=%.3f max_call_ms=%.3f calls=%u scope=presentation_thread_native_queue_including_optional_marker_overhead",nativeSubmitMilliseconds/300,nativeSubmitMaxMilliseconds,nativeSubmitCalls);nativeSubmitMilliseconds=nativeSubmitMaxMilliseconds=0;nativeSubmitCalls=0;
#endif
            log("timing_release_ms=%.3f xr_end_ms=%.3f",stereo::xr->releaseMilliseconds/300,stereo::xr->endMilliseconds/300);stereo::xr->releaseMilliseconds=stereo::xr->endMilliseconds=0;
            const auto& controls=stereo::xr->controls;log("controllers active=%d left_pose=%d right_pose=%d left_trigger=%.2f right_trigger=%.2f",controls.input.active,controls.hand[0].gripValid,controls.hand[1].gripValid,controls.input.leftTrigger,controls.input.rightTrigger);log("controller_axes left=%.2f,%.2f right=%.2f,%.2f context=%d native_rx=%d native_rt=%u native_buttons=%04x",controls.input.leftX,controls.input.leftY,controls.input.rightX,controls.input.rightY,static_cast<int>(controls.output.context),gamepadBridge::current.Gamepad.sThumbRX,gamepadBridge::current.Gamepad.bRightTrigger,gamepadBridge::current.Gamepad.wButtons);
            log("motion_aim_rays=%u motion_weapon_draws=%u head_tracked=%d",nativeMotion::aimedRays.load(),nativeMotion::trackedDraws.load(),stereo::xr->headTracked);log("native_menu=%d native_key_callbacks=%d",gamepadBridge::menuState(),gamepadBridge::keyDown!=nullptr);log("gamepad_native_polls=%u vr_polls=%u engine_polls=%u engine_bound=%d unsupported=%d",gamepadBridge::nativePolls.load(),gamepadBridge::vrPolls.load(),gamepadBridge::enginePolls.load(),gamepadBridge::engineBound,gamepadBridge::engineUnsupported);
            log("optical_scope enabled=%d ready=%d active=%d error=%s",opticScope::enabled,opticScope::ready,opticScope::active,opticScope::initializationError.c_str());
            {std::lock_guard<std::mutex> lock(manualReload::mutex);for(unsigned i:{2u,3u}){const auto& d=arsenal::reloadEvidence[i];log("reload_gate weapon_index=%u phase=%u valid=%d grip=%d support=%d handle=%d travel=%.3f pouch_distance=%.3f pouch=%d ammo=%d/%d capacity=%d invalid=%u pouch_samples=%u grip_pouch=%u transitions=%u hunting_round_asset=%u",i,unsigned(arsenal::reloads[i].state()),d.event.valid,d.event.grip,d.event.supportHeld,d.event.atHandle,d.event.travel,d.pouch,d.event.atPouch,d.loaded,d.reserve,d.capacity,d.invalid,d.pouchSamples,d.gripPouch,d.transitions,huntingRounds::first);}}
            log("weapon_visibility hidden=%d hidden_draws=%u unavailable_pose_draws=%u cached_transforms=%u uncached_transforms=%u",bodyHolsters::hidden.load(),nativeMotion::hiddenDraws.load(),nativeMotion::unavailablePoseDraws.load(),nativeMotion::cachedTransforms.load(),nativeMotion::uncachedTransforms.load());
            {auto test=gamepadBridge::readTestEvidence();log("desktop_input_evidence sequence=%u action=%u published=%u menu_suppressed=%u polled=%u nonzero_polls=%u",test.sequence,test.action,test.published,test.suppressedByMenu,test.polled,test.nonzeroPolls);}
            if(stereo::xr->desktopDiagnostic){auto view=stereo::readNativeViewEvidence();const auto& m=view.matrix;log("desktop_native_view valid=%d frame=%u matrix=%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",view.valid,view.frame,m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11]);}
            log("weapon_body_anchor trial=%d corrected=%u rejected=%u",nativeMotion::bodyAnchorTrial,nativeMotion::anchoredBodies.load(),nativeMotion::rejectedBodyAnchors.load());
            log("support_grip active_weapon=%d attachments=%u detachments=%u hand_bank=%u left_pose=%u left_section=%u",supportAim::activeWeapon.load(),supportAim::attachments.load(),supportAim::detachments.load(),replacementHands::sections,static_cast<unsigned>(wristband::replacementGrip.left),wristband::gripTransition[0].to);
            log("weapon_menu_native_draws_suppressed=%u",nativeMotion::suppressedMenuDraws.load());
            log("hand_clip_path requested=%u source_applied=%u stereo_pairs_applied=%u fallback_unknown=%u fallback_layout=%u fallback_shader=%u fallback_binding=%u fallback_copy=%u fallback_capacity=%u",nativeHandClip::sourceRequests.load(),nativeHandClip::sourceApplied.load(),nativeHandClip::eyeApplied.load(),nativeHandClip::sourceFallback[0].load(),nativeHandClip::sourceFallback[1].load(),nativeHandClip::sourceFallback[2].load(),nativeHandClip::sourceFallback[3].load(),nativeHandClip::sourceFallback[4].load(),nativeHandClip::sourceFallback[5].load());
            {std::lock_guard<std::mutex> lock(nativeWristPose::mutex);auto& p=nativeWristPose::local;auto t=GetTickCount64();log("native_wrist age_ms=%llu local=%.4f,%.4f,%.4f right_visible=%d pose=%.4f,%.4f,%.4f",nativeWristPose::captured?t-nativeWristPose::captured:~0ull,p.position.x,p.position.y,p.position.z,wristband::replacementVisible[1],wristband::replacementPose[1].position.x,wristband::replacementPose[1].position.y,wristband::replacementPose[1].position.z);}
            log("skips inactive=%u pipeline=%u shader=%u format=%u state=%u mapping=%u projection=%u capacity=%u particles=%u",stereo::skips[0].load(),stereo::skips[1].load(),stereo::skips[2].load(),stereo::skips[3].load(),stereo::skips[4].load(),stereo::skips[5].load(),stereo::skips[6].load(),stereo::skips[7].load(),stereo::particleDraws.load());
            log("particle_fog_draws=%u",stereo::particleFogDraws.load());
            log("geometry_isolation mode=%u sphere_overrides=%u sector_calls=%u pickup_trial=%d pickup_restores=%u",static_cast<unsigned>(geometryIsolation::mode(GetTickCount64())),diagnosticSphereRejections.load(),diagnosticSectorCalls.load(),stereoPickupCullingTrial.load(),stereoPickupRestores.load());
            log("pickup_trial_outcomes queries=%u busy=%u no_view=%u bad_camera=%u outside_both_eyes=%u restored=%u",pickupTrialQueries.load(),pickupTrialBusy.load(),pickupTrialNoView.load(),pickupTrialBadCamera.load(),pickupTrialOutside.load(),stereoPickupRestores.load());
            {
                UINT64 retained=0,shadow=0,unreadBytes=0;size_t entries=0,mapped=0,ranges=0,mapSources=0,copySources=0,unread=0,smallBuffers=0;
                {std::lock_guard<std::mutex> guard(stereo::mapMutex);entries=stereo::uploads.size();for(const auto& entry:stereo::uploads){const auto& u=entry.second;retained+=u.resource?u.size:0;shadow+=u.shadow.capacity();mapped+=u.mappings!=0;ranges+=u.known.size();mapSources+=u.observedMap;copySources+=u.observedCopy;smallBuffers+=u.size<=4096;if(!u.observedRead){++unread;unreadBytes+=u.size;}}}
                // Count only pinned resources as retained buffer bytes. Record their
                // growth without mistaking CPU shadow size for GPU allocation.
                log("capture_registry entries=%zu retained_buffer_bytes=%llu shadow_capacity_bytes=%llu mapped=%zu known_ranges=%zu",entries,retained,shadow,mapped,ranges);
                log("capture_registry_usage map_sources=%zu copy_sources=%zu never_read=%zu never_read_bytes=%llu buffers_at_most_4k=%zu",mapSources,copySources,unread,unreadBytes,smallBuffers);
            }
        }if(stereo::xr){auto& controls=stereo::xr->controls;const auto& c=controls.input;
            const auto replayNow=GetTickCount64();
            const unsigned supportReplay=stereo::xr->desktopDiagnostic&&replayNow<supportAim::diagnosticUntil.load()?supportAim::diagnosticMode.load():0;
            const unsigned holsterReplay=stereo::xr->desktopDiagnostic&&replayNow<bodyHolsters::diagnosticUntil.load()?bodyHolsters::diagnosticMode.load():0;
            static bool holsterWasInjected=false;
            if(!holsterReplay&&holsterWasInjected)controls.input.leftGrip=controls.input.rightGrip=0;
            holsterWasInjected=holsterReplay!=0;
            static bool supportWasInjected=false;
            if(!supportReplay&&supportWasInjected){controls.input.leftGrip=0;for(auto& hand:controls.hand)hand.gripValid=hand.aimValid=false;}
            supportWasInjected=supportReplay!=0;
            const bool meleeReplay=stereo::xr->desktopDiagnostic&&replayNow<meleeReplayUntil.load();
            const bool ejectionSideView=stereo::xr->desktopDiagnostic&&replayNow<ejectionSideViewUntil.load();
            static bool meleeWasInjected=false;
            if(!meleeReplay&&meleeWasInjected)controls.input.leftTrigger=controls.input.rightTrigger=0;
            meleeWasInjected=meleeReplay;
            bool geometryReplay=stereo::xr->desktopDiagnostic&&(geometryIsolation::replay(replayNow)||supportReplay||holsterReplay||meleeReplay||ejectionSideView)&&stereo::xr->headTracked&&stereo::xr->state==XR_SESSION_STATE_FOCUSED;
            if(geometryReplay){
                // Diagnostic poses only: neither runtime tracking claims nor
                // buttons/actions are synthesized. xrSync/locate replaces these
                // on every following frame before startup centering runs.
                auto center=headReference::center(stereo::xr->eyes[0].view,stereo::xr->eyes[1].view);
                // A fitting preview must be steady. Only the explicit support
                // sweep below animates a synthetic controller.
                for(unsigned side=0;side<2;side++){auto p=motionTracking::pointAhead(center,.35f);DirectX::XMFLOAT3 offset;DirectX::XMStoreFloat3(&offset,DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(side?.20f:-.20f,-.25f,0,0),motionTracking::rotation(center.orientation)));p.x+=offset.x;p.y+=offset.y;p.z+=offset.z;auto& h=controls.hand[side];h.grip=h.aim={center.orientation,p};h.gripValid=h.aimValid=true;}
                if(holsterReplay){
                    controls.input.leftGrip=controls.input.rightGrip=0;
                    if(holsterReplay!=3){
                        const unsigned side=holsterReplay<3?holsterReplay-1:1;
                        const auto a=bodyHolsters::anchor(holsterReplay==4?0:holsterReplay==5?1:2);DirectX::XMFLOAT3 position;
                        DirectX::XMStoreFloat3(&position,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(a.x,a.y,a.z,1),selectedFrames::pose(motionTracking::uprightPose(center))));
                        controls.hand[side].grip.position=controls.hand[side].aim.position={position.x,position.y,position.z};
                        if(side==0)controls.input.leftGrip=.8f;else controls.input.rightGrip=.8f;
                    }
                }
                if(ejectionSideView){
                    using namespace DirectX;
                    XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(-.35f,-.15f,-.55f,1),selectedFrames::pose(center)));
                    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(XMMatrixRotationY(-1.3f)*motionTracking::rotation(center.orientation)));
                    controls.hand[1].grip=controls.hand[1].aim={{q.x,q.y,q.z,q.w},{p.x,p.y,p.z}};
                }
                if(meleeReplay){
                    const int weapon=sniperScope::visibleWeapon.load();
                    if(weapon==5||weapon==6){
                        using namespace DirectX;
                        float t=float(replayNow-meleeReplayStarted.load())*.001f;
                        float x=-.25f+.50f*std::clamp((t-.5f)/.30f,0.f,1.f);
                        XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(x,-.15f,-.55f,1),selectedFrames::pose(center)));
                        unsigned side=bodyHolsters::heldHand.load()==0?0:1;
                        controls.hand[side].grip=controls.hand[side].aim={center.orientation,{p.x,p.y,p.z}};
                        controls.input.leftTrigger=controls.input.rightTrigger=0;
                        if(weapon==5&&t>=.5f&&t<1.7f){if(side==0)controls.input.leftTrigger=.9f;else controls.input.rightTrigger=.9f;}
                    }
                }
                if(supportReplay){
                    using namespace DirectX;
                    const int replayWeapon=sniperScope::visibleWeapon.load();
                    const auto* fit=measuredSupport::find(replayWeapon);
                    // Exercise the same contact used by the support solver.
                    // Legacy contacts do not describe the user's selected fits.
                    const auto* replayContact=wristband::selectedMode?selectedFrames::contact(replayWeapon):(fit?&fit->contact:nullptr);
                    auto offset=[&](XrVector3f value){XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(value.x,value.y,value.z,0),motionTracking::rotation(center.orientation)));return XrVector3f{center.position.x+p.x,center.position.y+p.y,center.position.z+p.z};};
                    const auto primary=offset({.15f,-.15f,-.45f});
                    controls.hand[1].grip=controls.hand[1].aim={center.orientation,primary};
                    auto contact=replayContact?*replayContact:XrVector3f{-.2f,0,-.25f};
                    if(supportReplay==2){const float phase=float(replayNow-supportAim::diagnosticStarted.load())*.001f;contact.x+=.10f*std::sin(phase);contact.y+=.04f*std::sin(phase*.7f);}
                    if(supportReplay==4){
                        float t=float(replayNow-supportAim::diagnosticStarted.load())*.001f;
                        // One physical controller stroke, grip continuously held.
                        // No reload state or ammunition is injected by this test.
                        contact.z+=t<.4f?0:t<1.0f?.09f*(t-.4f)/.6f:t<1.3f?.09f:t<1.9f?.09f*(1.9f-t)/.6f:0;
                    }
                    auto left=offset({.15f+contact.x,-.15f+contact.y,-.45f+contact.z});
                    controls.hand[0].grip=controls.hand[0].aim={center.orientation,left};
                    controls.input.leftGrip=supportReplay==3?0.f:.8f;
                }
            }
            const int primaryWeapon=sniperScope::visibleWeapon.load();
            const unsigned primaryHand=bodyHolsters::heldHand.load()==0&&(primaryWeapon==5||primaryWeapon==6)?0:1;
            bool motionValid=nativeMotion::enabled&&stereo::active&&(stereo::originSet||geometryReplay)&&stereo::xr->headTracked&&stereo::xr->state==XR_SESSION_STATE_FOCUSED&&controls.hand[primaryHand].aimValid&&gamepadBridge::menuState()==0&&!pillInventory::open();
            nativeMotion::renderInVR=stereo::active&&nativeMotion::enabled;
            auto now=GetTickCount64();
            bool pistol=sniperScope::visibleWeapon.load()==1&&now>=sniperScope::weaponTime.load()&&now-sniperScope::weaponTime.load()<150;
            const int supportWeapon=(now>=sniperScope::weaponTime.load()&&now-sniperScope::weaponTime.load()<150)?sniperScope::visibleWeapon.load():-1;
            static bool reloadGrip=false;
            if(c.leftGrip>.65f)reloadGrip=true;else if(c.leftGrip<.35f)reloadGrip=false;
            auto gun=controls.hand[primaryHand].aim;
            if((wristband::selectedMode||(pistol&&manualReload::enabled&&manualReload::installed)||arsenal::installed)&&controls.gripContinuity[primaryHand].have)gun.position=controls.hand[primaryHand].grip.position;
            motionValid=motionValid&&motionTracking::nearHead(gun,stereo::xr->eyes[0].view.pose);
            bool supportReady=false;
            {std::lock_guard<std::mutex> lock(manualReload::mutex);const int i=arsenal::index(supportWeapon);
                supportReady=(i>=0&&i<4&&arsenal::allowsSupport(static_cast<arsenal::Kind>(supportWeapon),arsenal::reloads[i].state()))||(wristband::selectedMode&&supportWeapon==5);}
            const bool supportAllowed=primaryHand==1&&motionValid&&controls.hand[0].gripValid&&controls.hand[1].gripValid&&
                motionTracking::nearHead(controls.hand[0].grip,stereo::xr->eyes[0].view.pose)&&
                (wristband::selectedMode?selectedFrames::contact(supportWeapon)!=nullptr:replacementHands::measuredSupport(supportWeapon))&&nativeMotion::bodyAnchorTrial&&supportReady&&
                !pillBottle::holding.load()&&!bodyHolsters::hidden.load();
            const auto unassistedGun=gun;
            gun=supportAim::controller.update(gun,controls.hand[0].grip,supportWeapon,reloadGrip,supportAllowed);
            supportAim::publish(supportWeapon,supportAim::controller.active());
            motionTracking::publish(stereo::origin,gun,stereo::unitsPerMeter,now,motionValid);
            auto interactionHead=stereo::xr->eyes[0].view.pose;
            interactionHead.position={(interactionHead.position.x+stereo::xr->eyes[1].view.pose.position.x)*.5f,(interactionHead.position.y+stereo::xr->eyes[1].view.pose.position.y)*.5f,(interactionHead.position.z+stereo::xr->eyes[1].view.pose.position.z)*.5f};
            interactionGaze::publish(stereo::origin,interactionHead,now,stereo::unitsPerMeter,stereo::active&&stereo::originSet&&stereo::xr->headTracked&&stereo::xr->state==XR_SESSION_STATE_FOCUSED&&!pillInventory::open());
            // The scene already uses the provisional origin while startup settles.
            bool displaySession=nativeMotion::enabled&&stereo::active&&motionTracking::validPose(stereo::origin)&&stereo::xr->headTracked&&gamepadBridge::menuState()==0&&!pillInventory::open();
            motionTracking::publishDisplay(stereo::origin,gun,stereo::unitsPerMeter,now,displaySession&&(geometryReplay||(controls.aimContinuity[primaryHand].have&&controls.gripContinuity[primaryHand].have)));
            manualReload::Hands reloadHands;reloadHands.gun=gun;reloadHands.left=controls.hand[0].grip;
            reloadHands.primaryHand=primaryHand;
            reloadHands.supportHeld=supportAim::controller.active();
            reloadHands.supportPumpOrigin=supportAim::controller.pumpRest();
            {
                using namespace DirectX;
                const auto& p=controls.hand[0].grip.position;const auto& g=unassistedGun.position;
                XMFLOAT3 local;
                XMStoreFloat3(&local,XMVector3TransformNormal(XMVectorSet(p.x-g.x,p.y-g.y,p.z-g.z,0),XMMatrixTranspose(motionTracking::rotation(unassistedGun.orientation))));
                reloadHands.supportPumpPosition={local.x,local.y,local.z};
            }
            reloadHands.head=stereo::xr->eyes[0].view.pose;reloadHands.rightGrip=controls.hand[1].grip;
            reloadHands.leftPoseTracked=controls.hand[0].gripValid;reloadHands.rightPoseTracked=controls.hand[1].gripValid;
            reloadHands.leftDisplayAvailable=controls.gripContinuity[0].have;reloadHands.rightDisplayAvailable=controls.gripContinuity[1].have&&controls.aimContinuity[1].have;
            auto ahead=motionTracking::pointAhead(stereo::origin,1);
            float yaw=std::atan2(stereo::origin.position.x-ahead.x,stereo::origin.position.z-ahead.z);
            reloadHands.head.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
            if(leftAlignmentRequested.exchange(false)){
                if(wristband::selectedMode&&selectedReload::profile.ready&&motionValid&&controls.hand[0].gripValid){
                    const auto reference=stereo::xr->eyes[0].view.pose;
                    const auto forward=motionTracking::pointAhead(reference,1);
                    const float facing=std::atan2(reference.position.x-forward.x,reference.position.z-forward.z);
                    const XrQuaternionf headYaw{0,std::sin(facing*.5f),0,std::cos(facing*.5f)};
                    leftAlignmentCorrection=leftHandAlignment::calibrate(reloadHands.left.orientation,headYaw);
                    const auto& q=leftAlignmentCorrection;
                    log("left_hand_alignment applied=1 xyzw=%.9f,%.9f,%.9f,%.9f",q.x,q.y,q.z,q.w);
                    vrFeedback::queue(0,.4f,80);
                }else log("left_hand_alignment applied=0 reason=tracking_or_gameplay_unavailable");
            }
            if(wristband::selectedMode)reloadHands.left=leftHandAlignment::apply(reloadHands.left,leftAlignmentCorrection);
            if(ammoPouchRequested.exchange(false)){
                auto p=motionTracking::relativePose(reloadHands.head,reloadHands.left).position;
                std::lock_guard<std::mutex> lock(manualReload::mutex);
                const bool applied=motionValid&&controls.hand[0].gripValid&&manualReload::setAmmoPouch(p);
                log("ammo_pouch_calibration applied=%d xyz=%.6f,%.6f,%.6f",applied,p.x,p.y,p.z);
                if(applied)vrFeedback::queue(0,.4f,80);
            }
            reloadHands.tracked=motionValid&&controls.hand[0].gripValid&&controls.hand[1].gripValid&&motionTracking::nearHead(reloadHands.left,reloadHands.head);
            reloadHands.gunTracked=motionValid&&controls.hand[primaryHand].gripValid;
            reloadHands.session=nativeMotion::enabled&&stereo::active&&stereo::xr->state==XR_SESSION_STATE_FOCUSED;
            reloadHands.gameplay=gamepadBridge::menuState()==0&&!pillInventory::open();reloadHands.pistol=pistol;
            nativeHandClip::vrGameplay=reloadHands.session&&reloadHands.gameplay&&replacementHands::ready;
            static arsenal::ContextButton reloadButton;
            reloadHands.release=reloadButton.update(c.b,sniperScope::consumedCancel||sniperScope::engaged.load());reloadHands.grip=reloadGrip;
            reloadHands.weapon=(now>=sniperScope::weaponTime.load()&&now-sniperScope::weaponTime.load()<150)?sniperScope::visibleWeapon.load():-1;
            reloadHands.trigger=primaryHand==0?c.leftTrigger:c.rightTrigger;reloadHands.confirm=c.a;
            reloadHands.scale=stereo::unitsPerMeter;reloadHands.time=now;
            int holsterEquip=-1;bool holsterChanged=false;
            auto gameBase=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
            const bool rightBottle=pillBottle::holding.load()&&pillBottle::heldSide.load()==1;
            {std::lock_guard<std::mutex> lock(manualReload::mutex);bodyHolsters::hipHeight=manualReload::ammoPouch.y;}
            bodyHolsters::publish(reloadHands.session&&reloadHands.gameplay&&reloadHands.rightPoseTracked&&!rightBottle,bodyHolsters::grip.update(c.rightGrip),reloadHands.head,reloadHands.rightGrip,reloadHands.weapon,*reinterpret_cast<unsigned char*>(gameBase+0x396380),holsterEquip,holsterChanged);
            int leftEquip=-1;bool leftChanged=false;
            bool leftFree=!pillBottle::holding.load()&&!supportAim::controller.active();
            {std::lock_guard<std::mutex> lock(manualReload::mutex);
                leftFree=leftFree&&(manualReload::gestures.transaction().stage()==manualReload::Stage::Ready);
                const int index=arsenal::index(reloadHands.weapon);
                if(index>=0&&index<4)leftFree=leftFree&&arsenal::allowsSupport(static_cast<arsenal::Kind>(reloadHands.weapon),arsenal::reloads[index].state());
            }
            bodyHolsters::publish(reloadHands.session&&reloadHands.gameplay&&reloadHands.leftPoseTracked&&!holsterChanged,
                bodyHolsters::leftGrip.update(c.leftGrip),reloadHands.head,reloadHands.left,reloadHands.weapon,
                *reinterpret_cast<unsigned char*>(gameBase+0x396380),leftEquip,leftChanged,0,leftFree);
            if(leftChanged){holsterEquip=leftEquip;holsterChanged=true;vrFeedback::queue(0,.35f,35);}
            pillBottle::publishControls({reloadHands.session&&reloadHands.gameplay,{primaryHand==0?false:reloadGrip,c.rightGrip>.65f},{c.leftTrigger>.65f,c.rightTrigger>.65f},bodyHolsters::hidden.load()||primaryHand==0,now});
            if(primaryHand==0)reloadHands.grip=false;
            if(pillBottle::holding.load()&&pillBottle::heldSide.load()==0)reloadHands.grip=false;
            if(holsterEquip>=0)weaponGroups::equip(reloadHands.weapon,holsterEquip,now);
            if(holsterChanged&&!leftChanged)vrFeedback::queue(1,.35f,35);
            if(bodyHolsters::hidden.load())reloadHands.gunTracked=false;
            if(stereo::xr->desktopDiagnostic&&geometryReplay)selectedReload::replay(reloadHands,now);
            if(stereo::xr->desktopDiagnostic&&geometryReplay&&now<opticScope::diagnosticUntil.load()&&reloadHands.weapon==4){
                const auto eye=stereo::xr->eyes[1].view.pose;reloadHands.gun=eye;
                DirectX::XMFLOAT3 delta;DirectX::XMStoreFloat3(&delta,DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(-opticScope::ocular.x,-opticScope::ocular.y,-.10f-opticScope::ocular.z,0),motionTracking::rotation(eye.orientation)));
                reloadHands.gun.position={eye.position.x+delta.x,eye.position.y+delta.y,eye.position.z+delta.z};
            }
            manualReload::publish(reloadHands);
            opticScope::publish(reloadHands.gun,now,wristband::selectedMode&&reloadHands.session&&reloadHands.gameplay&&reloadHands.gunTracked&&reloadHands.weapon==4);
            headLamp::vrPresentation=nativeMotion::enabled;
            headLamp::publish({stereo::xr->eyes[0].view.pose,stereo::origin,stereo::unitsPerMeter,now,reloadHands.session&&stereo::originSet&&stereo::xr->headTracked});

            for(unsigned hand=0;hand<2;hand++){auto pulse=vrFeedback::take(hand);if(pulse&&stereo::xr->state==XR_SESSION_STATE_FOCUSED)stereo::xr->controls.pulse(hand,float(pulse>>16)/1000,pulse&65535);}
            auto routed=stereo::active?controls.output:vrControls::Output{};
            if(primaryHand==0)routed.fire=c.leftTrigger>.65f;
            if(pillBottle::holding.load())routed.confirm=false;
            if(bodyHolsters::hidden.load()||rightBottle)routed.fire=false;
            auto inventoryInput=c;
            // Explicit inventory diagnostics inject button edges only; headset poses remain real.
            // Exercise the real inventory routing, counts and rendering paths.
            if((stereo::xr->desktopDiagnostic&&geometryReplay)||now<inventoryTestUntil.load()){
                inventoryInput.active=true;
                inventoryInput.y=desktopInventoryToggle.exchange(false);
                // One-frame controller edges, consumed by the actual inventory
                // controller. Native keyboard test input does not reach it.
                const auto action=desktopInventoryAction.exchange(0);
                inventoryInput.leftX=action==49?-1.f:action==50?1.f:0.f;
                inventoryInput.leftY=action==51?1.f:action==52?-1.f:0.f;
                inventoryInput.a=action==53;
                inventoryInput.b=action==54;
            }
            pillInventory::route(inventoryInput,routed,stereo::active&&stereo::haveWorldFrame&&(gamepadBridge::menuState()==0||pillInventory::ownsMap));
            if(routed.flashlight&&reloadHands.gameplay&&!pillInventory::open()&&weaponGroups::lampOwned.load())headLamp::controllerToggle();
            routed.flashlight=false; // Handled once here, never forwarded as a native keyboard toggle.
            if(pillInventory::open())controls.router.setContext(vrControls::Context::inventory);
            else if(!pillInventory::ownsMap&&gamepadBridge::menuState()==0&&controls.router.currentContext()==vrControls::Context::inventory)controls.router.setContext(vrControls::Context::gameplay);
            {static int last=-1;int state=int(pillInventory::open())+2*int(pillInventory::ownsMap)+4*int(pillInventory::closingMap)+8*(gamepadBridge::menuState()+1);if(state!=last){log("inventory_state=%d open=%d owns_map=%d closing=%d native_map=%d queued=%d",state,pillInventory::open(),pillInventory::ownsMap,pillInventory::closingMap,gamepadBridge::mapMenuActive(),pillInventory::queuedEquip);last=state;}}
            if(routed.nextItem){weaponGroups::request(reloadHands.weapon,now);routed.nextItem=false;}
            if(sniperScope::consumedCancel)routed.weaponAction=false;
            if(manualReload::consumesAction())routed.weaponAction=false;
            if(manualReload::blocked())routed.fire=false;
            if(arsenal::consume())routed.weaponAction=false;
            if(arsenal::block()||(arsenal::installed&&arsenal::meleeEnabled&&reloadHands.weapon==6))routed.fire=false;
            if(nativeMotion::headRelativeMovement&&stereo::active&&stereo::originSet&&stereo::xr->headTracked&&routed.context==vrControls::Context::gameplay)
                motionTracking::headRelativeAxes(stereo::origin,stereo::xr->eyes[0].view.pose,routed.strafe,routed.walk);
            static motionTracking::FireGate motionFire;
            static unsigned motionFireHand=1;
            if(motionFireHand!=primaryHand){motionFire.update(false,0,false);motionFireHand=primaryHand;}
            if(nativeMotion::enabled)routed.fire=motionFire.update(motionValid,reloadHands.trigger,routed.fire);
            gamepadBridge::publish(stereo::active?c:vrControls::Input{},routed,controls.hand[0].gripValid||controls.hand[1].gripValid,(gamepadBridge::menuState()>=0?gamepadBridge::menuState()!=0:!stereo::submittedWorld),stereo::haveWorldFrame,manualReload::blocked()||arsenal::block());unsigned mask=static_cast<unsigned>(c.a)|(c.b<<1)|(c.x<<2)|(c.y<<3)|(c.l3<<4)|(c.r3<<5)|((c.rightTrigger>.55f)<<6)|((c.leftGrip>.55f)<<7)|((c.rightGrip>.55f)<<8);static unsigned previousMask=0;if(mask!=previousMask){log("controller_buttons=%03x context=%d world=%d turn=%.2f fire=%d confirm=%d back=%d action=%d",mask,static_cast<int>(controls.output.context),stereo::submittedWorld,controls.output.turn,controls.output.fire,controls.output.confirm,controls.output.back,controls.output.weaponAction);previousMask=mask;}}desktopMirror::render(chain);if(desktopMirror::rendered&&desktopMirror::rendered<4)log("mirror_rendered=%u",desktopMirror::rendered);}
        catch(const std::exception& e) {
            nativeHandClip::vrGameplay=false;
            nativeMotion::renderInVR=false;
            stereo::failed=true;initDiagnostics::write("stereo_error=%s stereo_stage=%s",e.what(),stereo::stage);log("stereo_error=%s stage=%s",e.what(),stereo::stage);
            log("device_removed=%08lx",stereo::gameDevice?stereo::gameDevice->GetDeviceRemovedReason():E_POINTER);
            ComPtr<ID3D12InfoQueue> info;
            if(stereo::gameDevice&&SUCCEEDED(stereo::gameDevice.As(&info))) {
                UINT64 count=info->GetNumStoredMessagesAllowedByRetrievalFilter();
                for(UINT64 i=0;i<count;i++) {SIZE_T bytes=0;info->GetMessage(i,nullptr,&bytes);std::vector<unsigned char> buffer(bytes);auto m=reinterpret_cast<D3D12_MESSAGE*>(buffer.data());if(SUCCEEDED(info->GetMessage(i,m,&bytes))&&m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING)log("D3D12: %s",m->pDescription);}
            }
        }
        internal=false;
    }
    if(!(flags&DXGI_PRESENT_TEST)) {
        unsigned n=++presents;
        if(n==1||n%300==0) {
            DXGI_SWAP_CHAIN_DESC d{}; chain->GetDesc(&d);
            log("present=%u draws=%u pipelines=%u size=%ux%u format=%u",n,draws.load(),pipelines.load(),d.BufferDesc.Width,d.BufferDesc.Height,d.BufferDesc.Format);
            log("headlamp active=%d blocked_toggles=%u forced_on=%u brightness_restored=%u",headLamp::active(),headLamp::blockedToggles.load(),headLamp::forcedOn.load(),headLamp::brightnessRestored.load());
            log("handheld_light ready=%d held=%d enabled=%d switches=%u native_sound_calls=%u",selectedFlashlight::ready,handheldLight::held.load(),headLightConstants::enabled.load(),handheldLight::switchCount.load(),handheldLight::soundCount.load());
            auto combat=arsenal::diagnostics(GetTickCount64());
            {std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;const auto v=manualReload::gestures.visual();
             log("pistol_reload_evidence profile=%d stage=%u visual_valid=%d owned=%d pistol=%d grip=%d release=%d left_tracked=%d right_tracked=%d",selectedReload::profile.ready,unsigned(manualReload::gestures.transaction().stage()),v.valid,manualReload::ownsLocked(),h.pistol,h.grip,h.release,h.leftPoseTracked,h.rightPoseTracked);}
            {std::lock_guard<std::mutex> lock(manualReload::mutex);
             log("shot_effect_evidence revolver=%u pistol=%u m16=%u shotgun=%u hunting=%u m16_rate_supported=%d m16_rate_target=800",
                 weaponShots::observers[0].serial,weaponShots::observers[1].serial,weaponShots::observers[2].serial,weaponShots::observers[3].serial,weaponShots::observers[4].serial,arsenal::rifleCadenceSupported);log("casing_effect_evidence emitted=%u active=%llu",arsenalProps::emittedCases,static_cast<unsigned long long>(arsenalProps::flight.size()));}
            log("reload_effect_evidence floor_queries=%u floor_hits=%u sounds_played=%u sounds_failed=%u",dropSurface::queries.load(),dropSurface::hits.load(),reloadAudio::played.load(),reloadAudio::failed.load());
            log("motion_combat weapon=%d phase=%d input_age_ms=%llu session=%d gameplay=%d tracked=%d reloads=%u pistol_reloads=%u enemy_hits=%u wall_hits=%u tool_active=%d",
                combat.weapon,combat.phase,static_cast<unsigned long long>(combat.age),combat.session,combat.gameplay,combat.tracked,
                combat.transfers,manualReload::completed.load(),combat.hits,combat.wallHits,combat.toolActive);
            log("melee_evidence active_samples=%u converted_samples=%u world_queries=%u world_failures=%u",arsenal::meleeActiveSamples.load(),arsenal::meleeConvertedSamples.load(),arsenal::meleeWorldQueries.load(),arsenal::meleeWorldFailures.load());
            log("melee_gate owned_samples=%u valid_samples=%u entity_flags=%x",arsenal::meleeOwnedSamples.load(),arsenal::meleeValidSamples.load(),arsenal::meleeEntityFlags.load());
            log("melee_contacts reach_blocked=%u actor_visits=%u actor_queries=%u contacts=%u damage_rejected=%u",arsenal::meleeReachBlocked.load(),arsenal::meleeActorVisits.load(),arsenal::meleeActorQueries.load(),arsenal::meleeQueryContacts.load(),arsenal::meleeDamageRejected.load());
            if(meleeTrace::buffer.enabled){
                std::array<meleeTrace::Event,256> events{};const auto count=meleeTrace::buffer.drain(events);
                log("melee_trace_batch count=%u dropped=%u",count,meleeTrace::buffer.dropped.load());
                for(unsigned i=0;i<count;++i){const auto& e=events[i];
                    log("melee_trace t=%llu actor=%llx result=%u reverse=%u ray=%.6f,%.6f,%.6f,%.6f,%.6f,%.6f length=%.6f radius=%.6f distance=%.6f point=%.6f,%.6f,%.6f surface=%d triangle=%d health_before=%d",
                        static_cast<unsigned long long>(e.time),static_cast<unsigned long long>(e.actor),e.result,e.reverse,e.ray[0],e.ray[1],e.ray[2],e.ray[3],e.ray[4],e.ray[5],e.length,e.radius,e.distance,e.point[0],e.point[1],e.point[2],e.surface,e.triangle,e.health);
                }
            }
            log("melee_health previous_contact=%d current_contact=%d reductions=%u",arsenal::meleePreviousHealth.load(),arsenal::meleeCurrentHealth.load(),arsenal::meleeHealthDrops.load());
            {std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& m=arsenal::melee[1];
             log("club_strokes starts=%u reversals=%u speed_rejects=%u gap_rejects=%u",m.starts,m.reversals,m.speedRejects,m.gapRejects);}
        }
    }
    const UINT presentSync=stereo::active&&!stereo::failed?0:sync;
    static bool rejectedMirrorPolicy=false;
    const UINT presentFlags=rejectedMirrorPolicy?flags:desktopPresentFlags(chain,presentSync,flags);
    double beforePresent=stereo::preciseMilliseconds();hookTime+=beforePresent-hookStarted;HRESULT result=originalPresent(chain,presentSync,presentFlags);
    if(result==DXGI_ERROR_INVALID_CALL&&presentFlags!=flags){
        rejectedMirrorPolicy=true;log("mirror_present_policy rejected; restoring original desktop flags");
        result=originalPresent(chain,presentSync,flags);
    }
    const double afterPresent=stereo::preciseMilliseconds();desktopTime+=afterPresent-beforePresent;
#ifdef DEMENTIUM_PROFILE_DRAWS
    frameTail::tracker.finish(afterPresent,beforePresent-hookStarted,afterPresent-beforePresent,stereo::lastXrBeginMilliseconds,stereo::lastWorldCompletionMilliseconds,tickMilliseconds,xrEndMilliseconds);
#endif
    return result;
}
static void STDMETHODCALLTYPE executeHook(ID3D12CommandQueue* q,UINT n,ID3D12CommandList*const* lists) {
#ifdef DEMENTIUM_PROFILE_DRAWS
    const bool profileSubmission=!internal&&q==stereo::gameQueue.Get();
    const double submissionStarted=profileSubmission?stereo::preciseMilliseconds():0;
    if(profileSubmission&&lists)for(UINT i=0;i<n;++i){
        if(auto state=stereo::lists.find(reinterpret_cast<ID3D12GraphicsCommandList*>(lists[i])))stereo::executedWorldDraws+=state->diagnosticWorldDraws;
        else ++stereo::unknownExecutedLists;
    }
#endif
    // A runtime or upload queue may execute before the game's presentation
    // queue. Only the swapchain creation association establishes ownership.
    static std::atomic<bool> first=true;
    if(first.exchange(false)) {
        ComPtr<ID3D12Device> device; q->GetDevice(IID_PPV_ARGS(&device));
        ComPtr<ID3D12InfoQueue1> info;DWORD cookie{};
        if(SUCCEEDED(device.As(&info)))log("graphics_callback=%08lx",info->RegisterMessageCallback(graphicsMessage,D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&cookie));
        auto luid=device->GetAdapterLuid(); auto d=q->GetDesc();
        log("game_queue=%p type=%u GPU=%08lx:%08lx",q,d.Type,luid.HighPart,luid.LowPart);
    }
    copyProvenance::Scope evidence(q,static_cast<unsigned>(q->GetDesc().Type),GetCurrentThreadId(),n,lists,[](ID3D12CommandList* list)->const copyProvenance::History*{auto state=stereo::lists.find(reinterpret_cast<ID3D12GraphicsCommandList*>(list));return state?&state->textureCopies:nullptr;},[](ID3D12CommandList* list)->const copyProvenance::BarrierHistory*{auto state=stereo::lists.find(reinterpret_cast<ID3D12GraphicsCommandList*>(list));return state?&state->barriers:nullptr;});
#ifdef DEMENTIUM_PROFILE_DRAWS
    if(!internal&&stereo::nativeGpuProfiling()&&q==stereo::gameQueue.Get())
        stereo::nativeGpuTimer.execute(stereo::gameDevice.Get(),q,n,lists,originalExecute);
    else
#endif
    originalExecute(q,n,lists);
#ifdef DEMENTIUM_PROFILE_DRAWS
    if(profileSubmission){const double elapsed=stereo::preciseMilliseconds()-submissionStarted;nativeSubmitMilliseconds+=elapsed;++nativeSubmitCalls;if(elapsed>nativeSubmitMaxMilliseconds)nativeSubmitMaxMilliseconds=elapsed;}
#endif
}
static unsigned reflect(const D3D12_SHADER_BYTECODE& code,bool& sprite) {
    if(!code.pShaderBytecode||!code.BytecodeLength) return 0;
    ComPtr<ID3D12ShaderReflection> reflection;
    HRESULT r=E_FAIL;
    {
        HMODULE module=GetModuleHandleW(L"dxcompiler.dll");
        auto create=module?reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module,"DxcCreateInstance")):nullptr;
        if(create) {
            ComPtr<IDxcLibrary> lib; ComPtr<IDxcContainerReflection> container; ComPtr<IDxcBlobEncoding> blob;
            if(SUCCEEDED(create(CLSID_DxcLibrary,IID_PPV_ARGS(&lib)))&&SUCCEEDED(create(CLSID_DxcContainerReflection,IID_PPV_ARGS(&container)))&&
                code.BytecodeLength<=UINT_MAX&&SUCCEEDED(lib->CreateBlobWithEncodingFromPinned(code.pShaderBytecode,static_cast<UINT>(code.BytecodeLength),0,&blob))&&SUCCEEDED(container->Load(blob.Get()))) {
                UINT32 part{}; if(SUCCEEDED(container->FindFirstPartKind(0x4c495844,&part))) container->GetPartReflection(part,IID_PPV_ARGS(&reflection));
            }
        }
    }
    if(!reflection) r=D3DReflect(code.pShaderBytecode,code.BytecodeLength,IID_PPV_ARGS(&reflection));
    if(!reflection) {log("VS reflection unavailable hr=%08lx",r);return 0;}
    D3D12_SHADER_DESC sd{}; reflection->GetDesc(&sd);
    sprite=sd.InputParameters==13;
    log("VS bytes=%zu inputs=%u buffers=%u version=%x",code.BytecodeLength,sd.InputParameters,sd.ConstantBuffers,sd.Version);
    unsigned constants=0;
    for(UINT i=0;i<sd.ConstantBuffers;i++) {
        auto buffer=reflection->GetConstantBufferByIndex(i); D3D12_SHADER_BUFFER_DESC bd{}; buffer->GetDesc(&bd);
        D3D12_SHADER_INPUT_BIND_DESC binding{}; reflection->GetResourceBindingDescByName(bd.Name,&binding);
        log("VS cbuffer=%s bytes=%u register=b%u space=%u",bd.Name,bd.Size,binding.BindPoint,binding.Space);
        if(bd.Size==1200&&binding.BindPoint==0&&binding.Space==0) constants=1200;
        if(bd.Size==128&&binding.BindPoint==0&&binding.Space==0) {
            D3D12_SHADER_VARIABLE_DESC view{},projection{};
            if(SUCCEEDED(buffer->GetVariableByName("uViewMtx")->GetDesc(&view))&&
               SUCCEEDED(buffer->GetVariableByName("uProjection")->GetDesc(&projection))&&
               view.StartOffset==0&&view.Size==48&&projection.StartOffset==48&&projection.Size==64)constants=128;
        }
        for(UINT j=0;j<bd.Variables;j++) { D3D12_SHADER_VARIABLE_DESC v{}; buffer->GetVariableByIndex(j)->GetDesc(&v); log("  variable=%s offset=%u bytes=%u",v.Name,v.StartOffset,v.Size); }
    }
    return constants;
}
static HRESULT STDMETHODCALLTYPE pipelineHook(ID3D12Device* device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,REFIID iid,void** out) {
    HRESULT r=originalPipeline(device,desc,iid,out);
    if(SUCCEEDED(r)&&!internal) {
        ++pipelines;log("pipeline=%p root=%p rtv=%u dsv=%u samples=%u",*out,desc->pRootSignature,desc->RTVFormats[0],desc->DSVFormat,desc->SampleDesc.Count);
        bool sprite=false;unsigned constants=reflect(desc->VS,sprite);
        if(constants==1200){
            static bool dumped=false;
            if(!dumped){dumped=true;wchar_t path[MAX_PATH]{};GetModuleFileNameW(self,path,MAX_PATH);auto slash=wcsrchr(path,L'\\');if(slash)wcscpy_s(slash+1,MAX_PATH-(slash+1-path),L"scene-light-shaders.txt");
                FILE* file=nullptr;_wfopen_s(&file,path,L"wb");if(file){for(auto code:{desc->VS,desc->PS}){ComPtr<ID3DBlob> assembly;if(SUCCEEDED(D3DDisassemble(code.pShaderBytecode,code.BytecodeLength,0,nullptr,&assembly)))fwrite(assembly->GetBufferPointer(),1,assembly->GetBufferSize(),file);}fclose(file);}
            }
        }
        bool headLightLayout=false;
        if(constants==1200){ComPtr<ID3D12ShaderReflection> lighting;
            if(SUCCEEDED(D3DReflect(desc->PS.pShaderBytecode,desc->PS.BytecodeLength,IID_PPV_ARGS(&lighting)))){
                auto cb=lighting->GetConstantBufferByIndex(0);D3D12_SHADER_BUFFER_DESC description{};D3D12_SHADER_INPUT_BIND_DESC binding{};
                if(SUCCEEDED(cb->GetDesc(&description))&&description.Size==448&&SUCCEEDED(lighting->GetResourceBindingDescByName(description.Name,&binding))&&binding.BindPoint==0&&binding.Space==0){
                    headLightLayout=true;
                    for(auto field: {std::pair<const char*,UINT>{"lruLightPos",16},{"lruLightSpotDir",48},{"lruLightEnabled",208},{"light_spotmode",240}}){D3D12_SHADER_VARIABLE_DESC variable{};
                        if(FAILED(cb->GetVariableByName(field.first)->GetDesc(&variable))||variable.StartOffset!=field.second||variable.Size!=32)headLightLayout=false;}
                }
            }
            log("head_light_pixel_layout=%d",headLightLayout);
        }
        bool particleFogLayout=false;
        if(constants==128){ComPtr<ID3D12ShaderReflection> reflection;
            if(SUCCEEDED(D3DReflect(desc->PS.pShaderBytecode,desc->PS.BytecodeLength,IID_PPV_ARGS(&reflection)))){
                auto cb=reflection->GetConstantBufferByIndex(0);D3D12_SHADER_BUFFER_DESC d{};D3D12_SHADER_INPUT_BIND_DESC binding{};
                if(SUCCEEDED(cb->GetDesc(&d))&&d.Size==1040&&SUCCEEDED(reflection->GetResourceBindingDescByName(d.Name,&binding))&&binding.BindPoint==0&&binding.Space==0){
                    particleFogLayout=true;
                    for(auto field:{std::pair<const char*,UINT>{"uFogColor",0},{"uFogEnabled",12},{"lutFOG",16}}){D3D12_SHADER_VARIABLE_DESC v{};
                        unsigned size=field.second==0?12:field.second==12?4:1024;
                        if(FAILED(cb->GetVariableByName(field.first)->GetDesc(&v))||v.StartOffset!=field.second||v.Size!=size)particleFogLayout=false;}
                }
            }
            log("particle_fog_pixel_layout=%d",particleFogLayout);
        }
        bool usesTexture=true;UINT textureMask=1;
        ComPtr<ID3D12ShaderReflection> pixel;
        if(sprite&&SUCCEEDED(D3DReflect(desc->PS.pShaderBytecode,desc->PS.BytecodeLength,IID_PPV_ARGS(&pixel)))) {
            usesTexture=false;textureMask=0;D3D12_SHADER_DESC pd{};pixel->GetDesc(&pd);
            for(UINT i=0;i<pd.BoundResources;i++){D3D12_SHADER_INPUT_BIND_DESC resource{};pixel->GetResourceBindingDesc(i,&resource);if(resource.Type==D3D_SIT_TEXTURE){usesTexture=true;for(UINT j=0;j<resource.BindCount&&resource.BindPoint+j<16;j++)textureMask|=1u<<(resource.BindPoint+j);}}
        }
        if(sprite) {
            log("sprite_texture_mask=%x",textureMask);
            wchar_t dumpPath[MAX_PATH]{};GetModuleFileNameW(self,dumpPath,MAX_PATH);auto slash=wcsrchr(dumpPath,L'\\');if(slash)wcscpy_s(slash+1,MAX_PATH-(slash+1-dumpPath),L"sprite-shaders.txt");
            FILE* dump=nullptr;_wfopen_s(&dump,dumpPath,L"ab");if(dump){
                fprintf(dump,"\nPIPELINE %p blend=%d src=%u dst=%u\n",*out,desc->BlendState.RenderTarget[0].BlendEnable,desc->BlendState.RenderTarget[0].SrcBlend,desc->BlendState.RenderTarget[0].DestBlend);
                for(auto code:{desc->VS,desc->PS}){ComPtr<ID3DBlob> assembly;if(SUCCEEDED(D3DDisassemble(code.pShaderBytecode,code.BytecodeLength,0,nullptr,&assembly)))fwrite(assembly->GetBufferPointer(),1,assembly->GetBufferSize(),dump);}
                for(UINT i=0;i<desc->InputLayout.NumElements;i++){auto e=desc->InputLayout.pInputElementDescs[i];fprintf(dump,"LAYOUT %s%u slot=%u offset=%u format=%u\n",e.SemanticName,e.SemanticIndex,e.InputSlot,e.AlignedByteOffset,e.Format);}fclose(dump);
            }
        }
        stereo::SpriteLayout spriteLayout{};UINT foundElements=0;UINT offsets[32]{};
        if(sprite||constants==128)for(UINT i=0;i<desc->InputLayout.NumElements;i++) {
            auto element=desc->InputLayout.pInputElementDescs[i];if(element.InputSlot>=32)continue;
            if(element.AlignedByteOffset==D3D12_APPEND_ALIGNED_ELEMENT)element.AlignedByteOffset=offsets[element.InputSlot];
            UINT bytes=element.Format==DXGI_FORMAT_R32G32B32A32_FLOAT?16:element.Format==DXGI_FORMAT_R32G32B32_FLOAT?12:element.Format==DXGI_FORMAT_R32G32_FLOAT?8:4;
            offsets[element.InputSlot]=element.AlignedByteOffset+bytes;
            stereo::SpriteElement value{element.InputSlot,element.AlignedByteOffset,element.InputSlotClass==D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA?element.InstanceDataStepRate:0,element.Format};
            // The recorded native particle shader passes COLOR0 through.
            // Retain its declared layout for opt-in colour evidence only.
            if(constants==128){if(!_stricmp(element.SemanticName,"COLOR")&&element.SemanticIndex==0)spriteLayout.color=value;continue;}
            if(!_stricmp(element.SemanticName,"POSITION")&&element.SemanticIndex==0){spriteLayout.position=value;foundElements|=1;}
            if(!_stricmp(element.SemanticName,"TEXCOORD")&&element.SemanticIndex==0)spriteLayout.uv=value;
            if(!_stricmp(element.SemanticName,"TEXCOORD")&&element.SemanticIndex>=2&&element.SemanticIndex<=5){spriteLayout.rows[element.SemanticIndex-2]=value;foundElements|=2u<<(element.SemanticIndex-2);}
            if(!_stricmp(element.SemanticName,"COLOR")&&element.SemanticIndex==5)spriteLayout.color=value;
            if(!_stricmp(element.SemanticName,"COLOR")&&element.SemanticIndex<4)spriteLayout.colorRows[element.SemanticIndex]=value;
            if(!_stricmp(element.SemanticName,"COLOR")&&element.SemanticIndex==4)spriteLayout.colorBias=value;
        }
        spriteLayout.valid=foundElements==31;
        ComPtr<ID3D12PipelineState> uiPipeline,promptPipeline;
        if(sprite&&desc->SampleDesc.Count==1&&desc->RTVFormats[0]==DXGI_FORMAT_R8G8B8A8_UNORM) {
            auto uiDesc=*desc;auto& blend=uiDesc.BlendState.RenderTarget[0];
            uiDesc.DepthStencilState.DepthEnable=FALSE;uiDesc.DepthStencilState.StencilEnable=FALSE;uiDesc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
            if(!blend.BlendEnable){blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ZERO;blend.BlendOp=D3D12_BLEND_OP_ADD;}
            blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
            blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
            internal=true;HRESULT uiResult=originalPipeline(device,&uiDesc,IID_PPV_ARGS(&uiPipeline));internal=false;
            if(FAILED(uiResult))log("ui_pipeline_error=%08lx",uiResult);
            if(spriteLayout.valid&&usesTexture&&textureMask==1){
                ComPtr<ID3DBlob> pixelCode,error;
                if(SUCCEEDED(D3DCompile(nativeUiText::buttonShader,sizeof(nativeUiText::buttonShader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pixelCode,&error))){
                    auto promptDesc=uiDesc;promptDesc.PS={pixelCode->GetBufferPointer(),pixelCode->GetBufferSize()};promptDesc.CachedPSO={};
                    internal=true;auto result=originalPipeline(device,&promptDesc,IID_PPV_ARGS(&promptPipeline));internal=false;
                    if(FAILED(result))log("interaction_A_pipeline_error=%08lx",result);
                }else log("interaction_A_shader_error=1");
            }

        }
        // Keep native PSOs intact. Only injected world draws use these variants.
        ComPtr<ID3D12PipelineState> stereoBase,stereoHand,stereoFog,stereoHandFog,stereoParticle;
        const bool adaptSamples=stereoPipelineSamples::eligible(constants,*desc)&&
            (desc->SampleDesc.Count!=stereo::sceneSamples||desc->SampleDesc.Quality!=0);
        bool stereoSamplesReady=true;
        auto createStereoVariant=[&](const D3D12_GRAPHICS_PIPELINE_STATE_DESC& source,ComPtr<ID3D12PipelineState>& target){
            if(!adaptSamples)return;
            auto adapted=stereoPipelineSamples::adapt(source,stereo::sceneSamples);
            internal=true;auto result=originalPipeline(device,&adapted,IID_PPV_ARGS(&target));internal=false;
            log("stereo_sample_pipeline native_samples=%u native_quality=%u eye_samples=%u result=%08lx",desc->SampleDesc.Count,desc->SampleDesc.Quality,stereo::sceneSamples,result);
            if(FAILED(result))stereoSamplesReady=false;
        };
        createStereoVariant(*desc,stereoBase);
        ComPtr<ID3D12PipelineState> particleFogPipeline;
        if(particleFogLayout&&particleFogDepth::supported(desc->PS.pShaderBytecode,desc->PS.BytecodeLength)){
            ComPtr<ID3DBlob> code,error;
            HRESULT result=D3DCompile(particleFogDepth::shader,sizeof(particleFogDepth::shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
            if(SUCCEEDED(result)){
                auto candidate=*desc;candidate.PS={code->GetBufferPointer(),code->GetBufferSize()};candidate.CachedPSO={};
                internal=true;result=originalPipeline(device,&candidate,IID_PPV_ARGS(&particleFogPipeline));internal=false;
                if(SUCCEEDED(result))createStereoVariant(candidate,stereoParticle);
            }
            log("particle_depth_pipeline=%08lx",result);
        }
        ComPtr<ID3D12PipelineState> handPipeline,fogPipeline,handFogPipeline;
        ComPtr<IDxcBlob> fogPixel;
        if(headLightLayout&&nativeFog::patch(desc->PS,fogPixel)){
            auto fogDesc=*desc;fogDesc.PS={fogPixel->GetBufferPointer(),fogPixel->GetBufferSize()};fogDesc.CachedPSO={};
            internal=true;auto fogResult=originalPipeline(device,&fogDesc,IID_PPV_ARGS(&fogPipeline));internal=false;
            log("native_fog_pipeline=%08lx",fogResult);
            if(SUCCEEDED(fogResult))createStereoVariant(fogDesc,stereoFog);
        }
        if(constants==1200&&!sprite){
            for(UINT i=0;i<desc->InputLayout.NumElements;i++){
                const auto& e=desc->InputLayout.pInputElementDescs[i];
                log("native_geometry_input semantic=%s index=%u slot=%u offset=%u format=%u",e.SemanticName,e.SemanticIndex,e.InputSlot,e.AlignedByteOffset,static_cast<unsigned>(e.Format));
            }
            ComPtr<ID3DBlob> vs,error;
            if(SUCCEEDED(D3DCompile(nativeHandClip::shader,sizeof(nativeHandClip::shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error))){
                auto handDesc=*desc;handDesc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};handDesc.CachedPSO={};
                internal=true;auto handResult=originalPipeline(device,&handDesc,IID_PPV_ARGS(&handPipeline));internal=false;
                if(FAILED(handResult))log("native_hand_clip_pipeline=%08lx",handResult);
                else createStereoVariant(handDesc,stereoHand);
                if(fogPipeline){handDesc.PS={fogPixel->GetBufferPointer(),fogPixel->GetBufferSize()};internal=true;auto fogResult=originalPipeline(device,&handDesc,IID_PPV_ARGS(&handFogPipeline));internal=false;if(FAILED(fogResult))log("native_hand_fog_pipeline=%08lx",fogResult);else createStereoVariant(handDesc,stereoHandFog);}
            }
        }
        std::lock_guard<std::mutex> guard(stereo::mapMutex);
        stereo::pipelineInfo[reinterpret_cast<ID3D12PipelineState*>(*out)]={constants,desc->RTVFormats[0],desc->DSVFormat,desc->SampleDesc.Count,sprite,uiPipeline,usesTexture,textureMask,spriteLayout,!desc->BlendState.RenderTarget[0].BlendEnable,handPipeline,headLightLayout,fogPipeline,handFogPipeline};
        auto& registered=stereo::pipelineInfo[reinterpret_cast<ID3D12PipelineState*>(*out)];
        registered.sampleQuality=desc->SampleDesc.Quality;
        if(adaptSamples&&stereoSamplesReady){
            registered.stereoBase=stereoBase;registered.stereoHand=stereoHand;
            registered.stereoFog=stereoFog;registered.stereoHandFog=stereoHandFog;registered.stereoParticle=stereoParticle;
        }
        stereo::pipelineInfo[reinterpret_cast<ID3D12PipelineState*>(*out)].promptPipeline=promptPipeline;
        stereo::pipelineInfo[reinterpret_cast<ID3D12PipelineState*>(*out)].particleFogLayout=particleFogLayout;
        stereo::pipelineInfo[reinterpret_cast<ID3D12PipelineState*>(*out)].particleFogPipeline=particleFogPipeline;
        stereo::pipelineInfo[reinterpret_cast<ID3D12PipelineState*>(*out)].unusedStageCBV={nativeZeroConstantGuard::unusedRoot0(desc->VS,0),nativeZeroConstantGuard::unusedRoot0(desc->PS,1)};
    }
    return r;
}
static HRESULT STDMETHODCALLTYPE rootHook(ID3D12Device* device,UINT node,const void* data,SIZE_T size,REFIID iid,void** out) {
    HRESULT r=originalRoot(device,node,data,size,iid,out);
    if(SUCCEEDED(r)&&!internal) {
        {std::lock_guard<std::mutex> guard(stereo::mapMutex);stereo::stageOnlyCBV.erase(reinterpret_cast<ID3D12RootSignature*>(*out));}
        ComPtr<ID3D12RootSignatureDeserializer> deserializer;
        if(SUCCEEDED(D3D12CreateRootSignatureDeserializer(data,size,IID_PPV_ARGS(&deserializer)))) {
            auto d=deserializer->GetRootSignatureDesc(); log("root=%p parameters=%u",*out,d->NumParameters);
            {std::lock_guard<std::mutex> guard(stereo::mapMutex);stereo::stageOnlyCBV[reinterpret_cast<ID3D12RootSignature*>(*out)]=nativeZeroConstantGuard::rootStages(*d);}
            for(UINT i=0;i<d->NumParameters;i++) {
                auto& p=d->pParameters[i]; log(" parameter=%u type=%u visibility=%u",i,p.ParameterType,p.ShaderVisibility);
                if(p.ParameterType==D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) for(UINT j=0;j<p.DescriptorTable.NumDescriptorRanges;j++) { auto& range=p.DescriptorTable.pDescriptorRanges[j]; log("  range type=%u base=%u count=%u space=%u offset=%u",range.RangeType,range.BaseShaderRegister,range.NumDescriptors,range.RegisterSpace,range.OffsetInDescriptorsFromTableStart); }
                else if(p.ParameterType==D3D12_ROOT_PARAMETER_TYPE_CBV) log("  CBV register=%u space=%u",p.Descriptor.ShaderRegister,p.Descriptor.RegisterSpace);
            }
        }
    }
    return r;
}
static void traceSpriteTexture(const stereo::ListState& state) {
    void* resource=nullptr;stereo::SampledViewInfo metadata{};bool found=false;
    {
        std::lock_guard<std::mutex> guard(stereo::mapMutex);
        for(const auto& heap:state.heaps){auto handle=state.textures[0].ptr;if(handle<heap.gpu||!heap.stride)continue;auto delta=handle-heap.gpu;if(delta%heap.stride||delta>=UINT64(heap.count)*heap.stride)continue;auto cpu=heap.cpu+static_cast<SIZE_T>(delta);auto entry=stereo::sampledResources.find(cpu);if(entry!=stereo::sampledResources.end()){found=true;resource=entry->second;}auto layout=stereo::sampledViews.find(cpu);if(layout!=stereo::sampledViews.end())metadata=layout->second;break;}
    }
    if(!found)return;
    if(metadata.resourceKnown){const auto& d=metadata.resource;log("ui_texture=%p size=%llux%u flags=%u format=%u",resource,d.Width,d.Height,d.Flags,d.Format);}
    else log("ui_texture=%p resource_layout_known=0",resource);
}
static void traceSceneDraw(ID3D12GraphicsCommandList* list,UINT count,UINT instances,bool indexed,UINT first,INT base,UINT firstInstance) {
    static std::atomic<unsigned> traceFrame{UINT_MAX};
    auto& s=stereo::lists[list];stereo::PipelineInfo info{};
    {std::lock_guard<std::mutex> guard(stereo::mapMutex);auto i=stereo::pipelineInfo.find(s.pipeline);if(i==stereo::pipelineInfo.end())return;info=i->second;}
    if(nativeMotion::drawEvidence&&info.constants==1200){
        static BoundedSampleGate gunSamples;const auto now=GetTickCount64();
        if(gunSamples.mayCapture(now)){
            float constants[300]{};
            if(stereo::copyConstants(s.cbv,sizeof constants,reinterpret_cast<unsigned char*>(constants))){
                const auto sample=gunSamples.reserve(now);
                if(sample){const auto& e=*nativeMotion::drawEvidence;
                    log("gun_draw_evidence sample=%u frame=%u thread=%lu weapon=%u flags=%u mask=%x count=%u source_eye=%.9g,%.9g,%.9g",sample,presents.load(),GetCurrentThreadId(),e.weapon,e.flags,nativeHandClip::currentMask,count,e.sourceEye[0],e.sourceEye[1],e.sourceEye[2]);
                    for(unsigned row=0;row<3;++row)log("gun_draw_model sample=%u row=%u %.9g %.9g %.9g %.9g",sample,row,e.requested[row*4],e.requested[row*4+1],e.requested[row*4+2],e.requested[row*4+3]);
                    for(unsigned row=0;row<7;++row)log("gun_draw_camera sample=%u row=%u %.9g %.9g %.9g %.9g",sample,row,constants[row*4],constants[row*4+1],constants[row*4+2],constants[row*4+3]);
                    for(unsigned row=0;row<3;++row)log("gun_draw_texture sample=%u row=%u %.9g %.9g %.9g %.9g",sample,row,constants[28+row*4],constants[29+row*4],constants[30+row*4],constants[31+row*4]);
                    const auto& vb=s.vertices[0];float prefix[32]{};
                    if(vb.StrideInBytes&&vb.StrideInBytes<=sizeof prefix&&vb.StrideInBytes%4==0&&stereo::copyConstants(vb.BufferLocation,vb.StrideInBytes,reinterpret_cast<unsigned char*>(prefix)))
                        for(unsigned row=0;row<vb.StrideInBytes/sizeof(float);row+=4)log("gun_draw_vertex sample=%u stride=%u float_offset=%u %.9g %.9g %.9g %.9g",sample,vb.StrideInBytes,row,prefix[row],prefix[row+1],prefix[row+2],prefix[row+3]);
                    log("gun_draw_wrist sample=%u q=%.9g,%.9g,%.9g,%.9g p=%.9g,%.9g,%.9g scale=%.9g,%.9g,%.9g",sample,constants[60],constants[61],constants[62],constants[63],constants[64],constants[65],constants[66],constants[292],constants[293],constants[294]);
                }
            }
        }
    }
    // Explicit location-specific capture, independent of startup's sample cap.
    // Capture the particle layout too. The scene-light trace below cannot
    // diagnose an enemy cloud: it only accepts the 1200-byte mesh layout.
    // This is read-only, opt-in, and bounded to the first particle frame.
    if(info.constants==128){
        static thread_local unsigned ticket{},frame{},records{};
        const auto requested=static_cast<unsigned>(sceneEvidenceRequest.load()>>32);
        const auto sourceFrame=presents.load();
        if(requested!=ticket){ticket=requested;frame=sourceFrame;records=0;}
        if(ticket&&frame==sourceFrame&&records<12){
            ++records;alignas(16) float values[32]{};
            const bool copied=stereo::copyConstants(s.cbv,sizeof values,reinterpret_cast<unsigned char*>(values));
            log("particle_evidence ticket=%u frame=%u thread=%lu sample=%u pipeline=%p indexed=%d count=%u instances=%u camera_copied=%d vs_cbv=%llx ps_cbv=%llx",ticket,frame,GetCurrentThreadId(),records,s.pipeline,indexed,count,instances,copied,s.cbv,s.pixelCBV);
            if(copied)for(unsigned row=0;row<8;row++)log("particle_evidence_constants ticket=%u thread=%lu sample=%u row=%u %.9g %.9g %.9g %.9g",ticket,GetCurrentThreadId(),records,row,values[row*4],values[row*4+1],values[row*4+2],values[row*4+3]);
            // Blood and death clouds share the native particle shader. Record
            // its incoming fog and LUT rather than guessing a replacement tint.
            // Same opt-in, first-frame/12-draw budget as the existing trace.
            float pixel[260]{};unsigned fogFlag{};
            const bool pixelKnown=stereo::copyConstants(s.pixelCBV,sizeof pixel,reinterpret_cast<unsigned char*>(pixel));
            if(pixelKnown)std::memcpy(&fogFlag,pixel+3,sizeof fogFlag);
            log("particle_evidence_fog ticket=%u thread=%lu sample=%u known=%d correction_available=%d enabled=%u rgb=%.9g,%.9g,%.9g",ticket,GetCurrentThreadId(),records,pixelKnown,info.particleFogPipeline!=nullptr,fogFlag,pixel[0],pixel[1],pixel[2]);
            if(pixelKnown)for(unsigned row=0;row<64;++row)log("particle_evidence_lut ticket=%u thread=%lu sample=%u row=%u %.9g %.9g %.9g %.9g",ticket,GetCurrentThreadId(),records,row,pixel[4+row*4],pixel[5+row*4],pixel[6+row*4],pixel[7+row*4]);
            UINT vertex=first;bool vertexKnown=count&&instances;
            if(indexed){
                const unsigned bytes=s.indices.Format==DXGI_FORMAT_R16_UINT?2:s.indices.Format==DXGI_FORMAT_R32_UINT?4:0;
                UINT index{};vertexKnown=vertexKnown&&bytes&&(UINT64(first)+1)*bytes<=s.indices.SizeInBytes&&
                    stereo::copyConstants(s.indices.BufferLocation+UINT64(first)*bytes,bytes,reinterpret_cast<unsigned char*>(&index));
                const auto resolved=INT64(index)+base;
                vertexKnown=vertexKnown&&resolved>=0&&resolved<=UINT_MAX;
                if(vertexKnown)vertex=static_cast<UINT>(resolved);
            }
            float color[4]{};
            const bool colorKnown=vertexKnown&&stereo::spriteAttribute(s,info.layout.color,vertex,firstInstance,color);
            log("particle_evidence_color ticket=%u thread=%lu sample=%u known=%d vertex=%u instance=%u slot=%u offset=%u format=%u rgba=%.9g,%.9g,%.9g,%.9g",ticket,GetCurrentThreadId(),records,colorKnown,vertex,firstInstance,info.layout.color.slot,info.layout.color.offset,info.layout.color.format,color[0],color[1],color[2],color[3]);
            // Descriptor identities and recorded metadata only: never dereference
            // resource pointers or introduce GPU barriers during a source draw.
            std::lock_guard<std::mutex> guard(stereo::mapMutex);
            for(unsigned slot=0;slot<8;slot++){
                stereo::SampledViewInfo layout{};
                for(const auto& heap:s.heaps){const auto handle=s.textures[slot].ptr;if(handle<heap.gpu||!heap.stride)continue;const auto delta=handle-heap.gpu;if(delta%heap.stride||delta>=UINT64(heap.count)*heap.stride)continue;const auto found=stereo::sampledViews.find(heap.cpu+static_cast<SIZE_T>(delta));if(found!=stereo::sampledViews.end())layout=found->second;break;}
                log("particle_evidence_texture ticket=%u thread=%lu sample=%u slot=%u descriptor=%llx known=%d width=%llu height=%u format=%u view_known=%d component_mapping=%u",ticket,GetCurrentThreadId(),records,slot,s.textures[slot].ptr,layout.resourceKnown,layout.resource.Width,layout.resource.Height,layout.resource.Format,layout.viewKnown,layout.view.Shader4ComponentMapping);
            }
        }
    }
    // First eligible source frame and at most12 draws per recording thread; no GPU state
    // changes, texture readback barriers or permanent per-draw disk logging.
    if(info.constants==1200&&info.headLightLayout){
        static thread_local unsigned ticket{},frame{},records{};
        auto request=sceneEvidenceRequest.load();auto requested=static_cast<unsigned>(request>>32);auto sourceFrame=presents.load();
        if(requested!=ticket){ticket=requested;frame=sourceFrame;records=0;}
        if(ticket&&frame==sourceFrame&&records<12){
            ++records;float vertex[300]{},pixel[112]{};
            if(records==1){
                nativeFlashlightProfileDiagnostic::Snapshot profile;
                const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
                const bool known=nativeFlashlightProfileDiagnostic::capture(image,profile,nativeCopyReader::Reader{});
                log("scene_evidence_cpu_profile ticket=%u frame=%u thread=%lu known=%d finite=%d minimum=%.9g maximum=%.9g native_wrapper=%p atomic_snapshot=0 gpu_contents_verified=0",ticket,frame,GetCurrentThreadId(),known,profile.finite,profile.minimum,profile.maximum,reinterpret_cast<void*>(profile.textureObject));
                if(known)log("scene_evidence_native_sampler ticket=%u frame=%u thread=%lu slot_known=%d resolved_slot=%d sampler_known=%d raw_filter=%x address_u=%u address_v=%u address_w=%u max_anisotropy=%u gpu_descriptor_verified=0",ticket,frame,GetCurrentThreadId(),profile.slotKnown,profile.resolvedSlot,profile.samplerKnown,profile.samplerWords[0],profile.samplerWords[1],profile.samplerWords[2],profile.samplerWords[3],profile.samplerWords[5]);
                if(known)log("scene_evidence_profile_resource ticket=%u frame=%u thread=%lu resource_known=%d resource_pointer=%p binding_id_known=%d native_binding_id=%u lifetime_verified=0 gpu_contents_verified=0",ticket,frame,GetCurrentThreadId(),profile.resourceKnown,reinterpret_cast<void*>(profile.resourceIdentity),profile.bindingIdKnown,profile.nativeBindingId);
                if(known)for(unsigned row=0;row<64;++row)log("scene_evidence_cpu_profile_values ticket=%u frame=%u thread=%lu row=%u %.9g %.9g %.9g %.9g",ticket,frame,GetCurrentThreadId(),row,profile.samples[row*4],profile.samples[row*4+1],profile.samples[row*4+2],profile.samples[row*4+3]);
            }
            bool vertexOK=stereo::copyConstants(s.cbv,1200,reinterpret_cast<unsigned char*>(vertex));
            bool pixelOK=stereo::copyConstants(s.pixelCBV,448,reinterpret_cast<unsigned char*>(pixel));
            log("scene_evidence ticket=%u frame=%u thread=%lu sample=%u pipeline=%p root=%p indexed=%d count=%u instances=%u vertex_ok=%d pixel_ok=%d vs_cbv=%llx ps_cbv=%llx",ticket,frame,GetCurrentThreadId(),records,s.pipeline,s.rootSignature,indexed,count,instances,vertexOK,pixelOK,s.cbv,s.pixelCBV);
            if(vertexOK)for(unsigned row=0;row<7;++row)log("scene_evidence_camera ticket=%u frame=%u thread=%lu sample=%u row=%u %.9g %.9g %.9g %.9g",ticket,frame,GetCurrentThreadId(),records,row,vertex[row*4],vertex[row*4+1],vertex[row*4+2],vertex[row*4+3]);
            if(pixelOK){
                for(unsigned row=0;row<28;++row)log("scene_evidence_light ticket=%u frame=%u thread=%lu sample=%u row=%u %.9g %.9g %.9g %.9g",ticket,frame,GetCurrentThreadId(),records,row,pixel[row*4],pixel[row*4+1],pixel[row*4+2],pixel[row*4+3]);
                unsigned fog{},combiner{},crush{};std::memcpy(&fog,pixel+96,4);std::memcpy(&combiner,pixel+98,4);std::memcpy(&crush,pixel+108,4);
                log("scene_evidence_flags ticket=%u frame=%u thread=%lu sample=%u fog_bits=%x combiner_bits=%x alpha_crush_bits=%x",ticket,frame,GetCurrentThreadId(),records,fog,combiner,crush);
            }
            std::lock_guard<std::mutex> guard(stereo::mapMutex);
            for(unsigned slot=0;slot<8;++slot){
                void* resource=nullptr;stereo::SampledViewInfo layout{};
                for(const auto& heap:s.heaps){auto handle=s.textures[slot].ptr;if(handle<heap.gpu||!heap.stride)continue;auto delta=handle-heap.gpu;if(delta%heap.stride||delta>=UINT64(heap.count)*heap.stride)continue;auto cpu=heap.cpu+static_cast<SIZE_T>(delta);auto it=stereo::sampledResources.find(cpu);if(it!=stereo::sampledResources.end())resource=it->second;auto metadata=stereo::sampledViews.find(cpu);if(metadata!=stereo::sampledViews.end())layout=metadata->second;break;}
                log("scene_evidence_texture ticket=%u frame=%u thread=%lu sample=%u slot=%u descriptor=%llx resource=%p",ticket,frame,GetCurrentThreadId(),records,slot,s.textures[slot].ptr,resource);
                log("scene_evidence_texture_layout ticket=%u frame=%u thread=%lu sample=%u slot=%u resource_known=%d dimension=%u width=%llu height=%u array_depth=%u format=%u view_known=%d view_format=%u view_dimension=%u component_mapping=%u",ticket,frame,GetCurrentThreadId(),records,slot,layout.resourceKnown,layout.resource.Dimension,layout.resource.Width,layout.resource.Height,layout.resource.DepthOrArraySize,layout.resource.Format,layout.viewKnown,layout.view.Format,layout.view.ViewDimension,layout.view.Shader4ComponentMapping);
            }
        }
    }
    if(info.constants==1200){
        static BoundedSampleGate lightSamples;
        if(lightSamples.mayCapture(GetTickCount64())){float values[112]{};
            if(stereo::copyConstants(s.pixelCBV,448,reinterpret_cast<unsigned char*>(values))&&values[56]!=0){
                const auto sample=lightSamples.reserve(GetTickCount64());
                if(sample)for(int row=1;row<20;row++)log("live_light sample=%u row=%d %.6f %.6f %.6f %.6f",sample,row,values[row*4],values[row*4+1],values[row*4+2],values[row*4+3]);
            }
        }
    }
    unsigned unset=UINT_MAX;
    // Desktop-only sessions do not advance the OpenXR frame counter. Bound
    // this trace by the presentation counter so they cannot log every draw.
    auto sourceFrame=presents.load();
    if(info.constants==1200&&traceFrame.compare_exchange_strong(unset,sourceFrame)) {
        alignas(16) float data[300]{};
        if(stereo::copyConstants(s.cbv,1200,reinterpret_cast<unsigned char*>(data))) {
            for(int row=0;row<7;row++)log("camera_row=%d %.6f %.6f %.6f %.6f",row,data[row*4],data[row*4+1],data[row*4+2],data[row*4+3]);
        }
        float lighting[112]{};
        if(stereo::copyConstants(s.pixelCBV,448,reinterpret_cast<unsigned char*>(lighting)))
            for(int row=0;row<20;row++)log("light_row=%d %.6f %.6f %.6f %.6f",row,lighting[row*4],lighting[row*4+1],lighting[row*4+2],lighting[row*4+3]);
    }
    if(traceFrame.load()==sourceFrame)log("scene_draw frame=%u constants=%u samples=%u indexed=%d count=%u instances=%u target=%llx cbv=%llx",stereo::frames,info.constants,info.samples,indexed,count,instances,static_cast<UINT64>(s.rtvs[0].ptr),s.cbv);
}
static void STDMETHODCALLTYPE drawHook(ID3D12GraphicsCommandList* list,UINT a,UINT b,UINT c,UINT d) {
    drawCost::Sample sample;
    auto emit=[&]{renderEvents.add({renderHistory::Kind::Draw,renderStage,list,a,b,internal});drawCost::Scope cost(sample,drawCost::Driver);originalDraw(list,a,b,c,d);};
    if(internal){RenderStageScope stage(renderHistory::Stage::Extra);emit();return;}
    InternalScope scope;{drawCost::Scope cost(sample,drawCost::Trace);traceSceneDraw(list,a,b,false,c,0,d);
    nativeUiAudit::draw(list,a,b,c,0,false,d);}
    {drawCost::Scope cost(sample,drawCost::Source);RenderStageScope stage(renderHistory::Stage::Source);stereo::sourceDraw(list,emit);}++draws;
    {drawCost::Scope cost(sample,drawCost::Stereo);RenderStageScope stage(renderHistory::Stage::Stereo);stereo::duplicate(list,emit);}
    {drawCost::Scope cost(sample,drawCost::UI);RenderStageScope stage(renderHistory::Stage::UI);stereo::duplicateUI(list,a,b,emit,c,0,false,d);}
}
static void STDMETHODCALLTYPE indexedHook(ID3D12GraphicsCommandList* list,UINT a,UINT b,UINT c,INT d,UINT e) {
    drawCost::Sample sample;
    auto emit=[&]{renderEvents.add({renderHistory::Kind::Indexed,renderStage,list,a,b,internal});drawCost::Scope cost(sample,drawCost::Driver);originalIndexed(list,a,b,c,d,e);};
    if(internal){RenderStageScope stage(renderHistory::Stage::Extra);emit();return;}
    InternalScope scope;{drawCost::Scope cost(sample,drawCost::Trace);traceSceneDraw(list,a,b,true,c,d,e);
    nativeUiAudit::draw(list,a,b,c,d,true,e);}
    {drawCost::Scope cost(sample,drawCost::Source);RenderStageScope stage(renderHistory::Stage::Source);stereo::sourceDraw(list,emit);}++draws;
    {drawCost::Scope cost(sample,drawCost::Stereo);RenderStageScope stage(renderHistory::Stage::Stereo);stereo::duplicate(list,emit);}
    {drawCost::Scope cost(sample,drawCost::UI);RenderStageScope stage(renderHistory::Stage::UI);stereo::duplicateUI(list,a,b,emit,c,d,true,e);}
}
static D3D12_GPU_VIRTUAL_ADDRESS guardedZeroConstant(ID3D12GraphicsCommandList* list,UINT index,D3D12_GPU_VIRTUAL_ADDRESS address,std::uintptr_t image){
    const auto until=zeroConstantGuardUntil.load();
    if(!until||GetTickCount64()>=until||internal||index>1||!stereo::gameList(list))return address;
    nativeZeroConstantGuard::Proof proof;proof.parameter=index;proof.nativeCall=true;proof.listDeviceOwned=true;proof.list=reinterpret_cast<std::uintptr_t>(list);
    ComPtr<ID3D12Resource> pinned;
    {
        std::lock_guard<std::mutex> guard(stereo::mapMutex);const auto& state=stereo::lists[list];
        auto root=stereo::stageOnlyCBV.find(state.rootSignature);auto pipeline=stereo::pipelineInfo.find(state.pipeline);
        if(root==stereo::stageOnlyCBV.end()||pipeline==stereo::pipelineInfo.end()||!root->second[index]||!pipeline->second.unusedStageCBV[index])return address;
        auto upload=stereo::uploads.lower_bound(address);
        if(upload==stereo::uploads.begin()||(upload!=stereo::uploads.end()&&upload->first==address))return address;
        --upload;
        if(address<upload->first||address-upload->first!=upload->second.size||!upload->second.resource)return address;
        proof.stageOnlyRoot=proof.shaderDoesNotRead=true;proof.liveBase=upload->first;proof.liveBytes=upload->second.size;pinned=upload->second.resource;proof.liveResource=reinterpret_cast<std::uintptr_t>(pinned.Get());
    }
    // Recheck ownership at the candidate boundary rather than relying only
    // on a list cache that could predate device replacement.
    proof.listDeviceOwned=deviceOwnership::belongs(list,stereo::gameDevice.Get());
    proof.resourceDeviceOwned=deviceOwnership::belongs(pinned.Get(),stereo::gameDevice.Get());
    D3D12_HEAP_PROPERTIES heap{};D3D12_HEAP_FLAGS heapFlags{};
    proof.resourceUploadHeap=SUCCEEDED(pinned->GetHeapProperties(&heap,&heapFlags))&&heap.Type==D3D12_HEAP_TYPE_UPLOAD;
    const auto description=pinned->GetDesc();
    if(pinned->GetGPUVirtualAddress()!=proof.liveBase||description.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||description.Width!=proof.liveBytes)return address;
    nativeUniformDiagnostic::Snapshot native;
    if(!nativeUniformDiagnostic::capture(image,native,[](std::uintptr_t source,void* out,size_t bytes){if(!manualReload::readable(reinterpret_cast<void*>(source),bytes))return false;std::memcpy(out,reinterpret_cast<void*>(source),bytes);return true;}))return address;
    const auto result=nativeZeroConstantGuard::replacement(native,proof,address);if(!result)return address;
    unsigned count=++zeroConstantRedirects;if(count<=3||(count&(count-1))==0)log("native_zero_constant_redirect count=%u parameter=%u incoming=%llx effective=%llx native_bytes=0 stage_unused=1 resource=%p",count,index,address,*result,pinned.Get());
    return *result;
}
static void STDMETHODCALLTYPE cbvHook(ID3D12GraphicsCommandList* list,UINT index,D3D12_GPU_VIRTUAL_ADDRESS address) {
    auto until=zeroConstantGuardUntil.load();
    if(until&&GetTickCount64()<until)address=guardedZeroConstant(list,index,address,reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    else if(until)zeroConstantGuardUntil.compare_exchange_strong(until,0);
    renderEvents.add({renderHistory::Kind::CBV,renderStage,list,address,index,internal});
    recentBindings[bindingCursor++%16]={list,index,address,internal};
    if(!internal&&index==0) stereo::lists[list].cbv=address;
    if(!internal&&index==1) stereo::lists[list].pixelCBV=address;
    static std::atomic<unsigned> n{}; if(n++<24) log("rootCBV list=%p parameter=%u address=%llx",list,index,address);
    originalCBV(list,index,address);
}
static void STDMETHODCALLTYPE setRootSignatureHook(ID3D12GraphicsCommandList* list,ID3D12RootSignature* signature) {
    renderEvents.add({renderHistory::Kind::Root,renderStage,list,reinterpret_cast<std::uintptr_t>(signature),0,internal});
    if(!internal){
        auto& state=stereo::lists[list];
        // Changing the root signature invalidates its arguments. Do not replay
        // a buffer or descriptor table inherited from the previous signature.
        if(state.rootSignature!=signature){state.cbv=state.pixelCBV=0;state.textures={};state.rootSignature=signature;}
    }
    originalSetRootSignature(list,signature);
}
static HRESULT STDMETHODCALLTYPE mapHook(ID3D12Resource* resource,UINT subresource,const D3D12_RANGE* range,void** out) {
    HRESULT r=originalMap(resource,subresource,range,out);
    if(SUCCEEDED(r)&&!internal) {
        auto d=resource->GetDesc();D3D12_HEAP_PROPERTIES hp{};D3D12_HEAP_FLAGS flags{};
        if(d.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER&&SUCCEEDED(resource->GetHeapProperties(&hp,&flags))&&hp.Type==D3D12_HEAP_TYPE_UPLOAD) {
            std::lock_guard<std::mutex> guard(stereo::mapMutex);
            auto& upload=stereo::uploads[resource->GetGPUVirtualAddress()];upload.resource=resource;upload.observedMap=true;upload.mapped(out?*out:nullptr,static_cast<size_t>(d.Width));
            stereo::trackCaptureLifetime(resource->GetGPUVirtualAddress(),upload,resource);
        }
    }
    return r;
}
static void STDMETHODCALLTYPE unmapHook(ID3D12Resource* resource,UINT subresource,const D3D12_RANGE* range) {
    ComPtr<ID3D12Resource> releasedCapture;
    if(!internal) {
        std::lock_guard<std::mutex> guard(stereo::mapMutex);auto i=stereo::uploads.find(resource->GetGPUVirtualAddress());
        if(i!=stereo::uploads.end()) {
            auto& upload=i->second;
            upload.unmapped(range?range->Begin:0,range?range->End:upload.size);
            if(upload.lifetimeTracked&&!upload.mappings&&!upload.observedRead&&!upload.observedCopy&&upload.cpu==upload.shadow.data()&&upload.shadow.size()==upload.size)
                releasedCapture=std::move(upload.resource);
        }
    }
    originalUnmap(resource,subresource,range);
    // Release outside mapMutex: final destruction invokes the metadata callback.
    releasedCapture.Reset();
}
static void STDMETHODCALLTYPE setPipelineHook(ID3D12GraphicsCommandList* list,ID3D12PipelineState* pso) {renderEvents.add({renderHistory::Kind::Pipeline,renderStage,list,reinterpret_cast<std::uintptr_t>(pso),0,internal});if(!internal)stereo::lists[list].pipeline=pso;originalSetPipeline(list,pso);}
static HRESULT STDMETHODCALLTYPE resetHook(ID3D12GraphicsCommandList* list,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pso) {
    renderEvents.add({renderHistory::Kind::Reset,renderStage,list,reinterpret_cast<std::uintptr_t>(pso),0,internal});
    HRESULT r=originalReset(list,allocator,pso);if(SUCCEEDED(r)){
        if(!internal){stereo::lists[list]={};stereo::lists[list].pipeline=pso;}
        else if(auto state=stereo::lists.find(list)){state->textureCopies={};state->barriers={};}
    }return r;
}
static HRESULT STDMETHODCALLTYPE createCommandListHook(ID3D12Device* device,UINT node,D3D12_COMMAND_LIST_TYPE type,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pso,REFIID iid,void** output) {
    HRESULT result=originalCreateCommandList(device,node,type,allocator,pso,iid,output);
    if(SUCCEEDED(result)&&output&&*output){
        ComPtr<ID3D12GraphicsCommandList> list;
        if(SUCCEEDED(static_cast<IUnknown*>(*output)->QueryInterface(IID_PPV_ARGS(&list)))){
            if(!internal){stereo::lists.reset(list.Get());stereo::lists[list.Get()].pipeline=pso;}
            else if(auto state=stereo::lists.find(list.Get())){state->textureCopies={};state->barriers={};}
        }
    }
    return result;
}
static void STDMETHODCALLTYPE viewportsHook(ID3D12GraphicsCommandList* list,UINT n,const D3D12_VIEWPORT* v) {
    if(!internal&&n<=16&&(!n||v)) {auto& s=stereo::lists[list];s.viewportCount=n;std::copy_n(v,n,s.viewports.begin());}originalViewports(list,n,v);
}
static void STDMETHODCALLTYPE scissorsHook(ID3D12GraphicsCommandList* list,UINT n,const D3D12_RECT* v) {
    if(!internal&&n<=16&&(!n||v)) {auto& s=stereo::lists[list];s.scissorCount=n;std::copy_n(v,n,s.scissors.begin());}originalScissors(list,n,v);
}
static void STDMETHODCALLTYPE targetsHook(ID3D12GraphicsCommandList* list,UINT n,const D3D12_CPU_DESCRIPTOR_HANDLE* rt,BOOL contiguous,const D3D12_CPU_DESCRIPTOR_HANDLE* depth) {
    if(!internal&&n<=8&&(!n||rt)) {
        UINT stride=0;if(n&&contiguous){ComPtr<ID3D12Device> device;if(FAILED(list->GetDevice(IID_PPV_ARGS(&device)))||!device){originalTargets(list,n,rt,contiguous,depth);return;}stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);}
        auto& s=stereo::lists[list];s.rtvCount=n;s.hasDepth=depth!=nullptr;if(depth)s.dsv=*depth;
        for(UINT i=0;i<n;i++)s.rtvs[i]=contiguous?D3D12_CPU_DESCRIPTOR_HANDLE{rt[0].ptr+i*stride}:rt[i];
    }originalTargets(list,n,rt,contiguous,depth);
}
static void STDMETHODCALLTYPE createRTVHook(ID3D12Device* device,ID3D12Resource* resource,const D3D12_RENDER_TARGET_VIEW_DESC* desc,D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    originalCreateRTV(device,resource,desc,handle);
    if(!internal){std::lock_guard<std::mutex> guard(stereo::mapMutex);stereo::renderTargetResources[handle.ptr]=resource;}
}
static void STDMETHODCALLTYPE tableHook(ID3D12GraphicsCommandList* list,UINT index,D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    renderEvents.add({renderHistory::Kind::Table,renderStage,list,handle.ptr,index,internal});
    if(!internal&&index>=2&&index<18)stereo::lists[list].textures[index-2]=handle;
    static std::atomic<unsigned> n{}; if(n++<24) log("rootTable list=%p parameter=%u handle=%llx",list,index,handle.ptr);
    originalTable(list,index,handle);
}
static void STDMETHODCALLTYPE srvHook(ID3D12Device* device,ID3D12Resource* resource,const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    originalCreateSRV(device,resource,desc,handle);
    if(!internal){std::lock_guard<std::mutex> guard(stereo::mapMutex);
        stereo::sampledResources[handle.ptr]=resource;
        stereo::SampledViewInfo metadata;if(resource){metadata.resource=resource->GetDesc();metadata.resourceKnown=true;}if(desc){metadata.view=*desc;metadata.viewKnown=true;}stereo::sampledViews[handle.ptr]=metadata;
    }
}
static void copyDescriptorMetadata(SIZE_T dest,SIZE_T source) {
    auto entry=stereo::sampledResources.find(source);
    if(entry==stereo::sampledResources.end())stereo::sampledResources.erase(dest);
    else stereo::sampledResources[dest]=entry->second;
    auto layout=stereo::sampledViews.find(source);
    if(layout==stereo::sampledViews.end())stereo::sampledViews.erase(dest);
    else stereo::sampledViews[dest]=layout->second;
}
static void STDMETHODCALLTYPE copySimpleHook(ID3D12Device* device,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE dest,D3D12_CPU_DESCRIPTOR_HANDLE source,D3D12_DESCRIPTOR_HEAP_TYPE type) {
    originalCopySimple(device,n,dest,source,type);
    if(!internal&&n&&type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV){std::lock_guard<std::mutex> guard(stereo::mapMutex);UINT stride=device->GetDescriptorHandleIncrementSize(type);for(UINT i=0;i<n;i++)copyDescriptorMetadata(dest.ptr+SIZE_T(i)*stride,source.ptr+SIZE_T(i)*stride);}
}
static UINT64 descriptorRangeTotal(UINT ranges,const UINT* sizes){if(!sizes)return ranges;UINT64 total=0;for(UINT i=0;i<ranges;++i)total+=sizes[i];return total;}
static void STDMETHODCALLTYPE copyDescriptorsHook(ID3D12Device* device,UINT dn,const D3D12_CPU_DESCRIPTOR_HANDLE* dest,const UINT* ds,UINT sn,const D3D12_CPU_DESCRIPTOR_HANDLE* source,const UINT* ss,D3D12_DESCRIPTOR_HEAP_TYPE type) {
    originalCopyDescriptors(device,dn,dest,ds,sn,source,ss,type);
    // Range starts are required for positive counts; size arrays are optional.
    // Keep original native validation/forwarding, but never dereference known
    // missing ranges or query a device for an empty metadata operation.
    if(!internal&&dn&&sn&&dest&&source&&type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV){
        // Native validation requires equal descriptor totals, not equal range
        // counts. Never create a partial shadow copy for a known invalid call.
        const auto total=descriptorRangeTotal(dn,ds);if(!total||total!=descriptorRangeTotal(sn,ss))return;
        std::lock_guard<std::mutex> guard(stereo::mapMutex);UINT stride=device->GetDescriptorHandleIncrementSize(type),sr=0,si=0;
        for(UINT dr=0;dr<dn;dr++)for(UINT di=0;di<(ds?ds[dr]:1);di++){
            while(sr<sn&&si>=(ss?ss[sr]:1)){++sr;si=0;}
            if(sr>=sn)return;
            copyDescriptorMetadata(dest[dr].ptr+SIZE_T(di)*stride,source[sr].ptr+SIZE_T(si++)*stride);
        }
    }
}
static void STDMETHODCALLTYPE heapsHook(ID3D12GraphicsCommandList* list,UINT n,ID3D12DescriptorHeap*const* heaps) {
    // Preserve invalid native calls for native validation without querying a
    // known-null object/array first. Two-pass check precedes every COM query.
    if(n>2||(n&&!heaps)){originalSetHeaps(list,n,heaps);return;}
    for(UINT i=0;i<n;++i)if(!heaps[i]){originalSetHeaps(list,n,heaps);return;}
    if(!n)renderEvents.add({renderHistory::Kind::Heap,renderStage,list,0,UINT_MAX,internal});
    std::array<const void*,2> identities{};
    std::array<stereo::ListState::Heap,2> metadata{};UINT metadataCount=0;
    for(UINT i=0;i<n;i++){
        const auto desc=heaps[i]->GetDesc();const auto type=desc.Type;
        renderEvents.add({renderHistory::Kind::Heap,renderStage,list,reinterpret_cast<std::uintptr_t>(heaps[i]),static_cast<unsigned>(type),internal});
        if(type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)identities[0]=heaps[i];
        else if(type==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)identities[1]=heaps[i];
        if(!internal&&type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV){
            ComPtr<ID3D12Device> device;if(FAILED(heaps[i]->GetDevice(IID_PPV_ARGS(&device)))||!device){originalSetHeaps(list,n,heaps);return;}
            metadata[metadataCount++]={heaps[i]->GetGPUDescriptorHandleForHeapStart().ptr,heaps[i]->GetCPUDescriptorHandleForHeapStart().ptr,desc.NumDescriptors,device->GetDescriptorHandleIncrementSize(type)};
        }
    }
    if(!internal){auto& state=stereo::lists[list];state.heaps.assign(metadata.begin(),metadata.begin()+metadataCount);
        state.setHeaps(identities);
    }originalSetHeaps(list,n,heaps);
}
enum class ContentTransfer {Invalidate,Preserve,WholeResource};
static void copySceneMetadata(ID3D12Resource* dest,ID3D12Resource* source,ContentTransfer transfer=ContentTransfer::Invalidate) {
    if(internal)return;
    std::lock_guard<std::mutex> guard(stereo::mapMutex);
    if(transfer!=ContentTransfer::Preserve){
        const bool vignette=transfer==ContentTransfer::WholeResource&&stereo::vignetteTextures.count(source);
        auto solid=stereo::solidTexels.find(source);const bool knownSolid=transfer==ContentTransfer::WholeResource&&solid!=stereo::solidTexels.end();
        const auto color=knownSolid?solid->second:std::array<float,4>{};
        stereo::vignetteTextures.erase(dest);stereo::solidTexels.erase(dest);
        if(vignette)stereo::vignetteTextures.insert(dest);if(knownSolid)stereo::solidTexels[dest]=color;
    }
    if(stereo::sceneTextures.count(source))stereo::sceneTextures.insert(dest);else stereo::sceneTextures.erase(dest);
    if(stereo::capturedUITextures.count(source))stereo::capturedUITextures.insert(dest);else stereo::capturedUITextures.erase(dest);
}
static void STDMETHODCALLTYPE resourceCopyHook(ID3D12GraphicsCommandList* list,ID3D12Resource* dest,ID3D12Resource* source) {
    auto transfer=ContentTransfer::Invalidate;
    if(!internal&&dest&&source&&textureUploadMetadata::compatibleWholeCopy(dest->GetDesc(),source->GetDesc()))transfer=ContentTransfer::WholeResource;
    copySceneMetadata(dest,source,transfer);originalCopyResource(list,dest,source);
}
static void STDMETHODCALLTYPE textureCopyHook(ID3D12GraphicsCommandList* list,const D3D12_TEXTURE_COPY_LOCATION* dest,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box) {
    // Preserve invalid native calls for D3D12's own validation. Diagnostic
    // capture must not dereference missing locations/resources first.
    if(!dest||!source||!dest->pResource||!source->pResource){originalCopyTexture(list,dest,x,y,z,source,box);return;}
    if(!internal)nativeUiAudit::upload(*dest,x,y,z,*source,box);
    TextureCopyTrace trace;trace.list=list;trace.queueType=list->GetType();trace.source=*source;trace.destination=*dest;trace.x=x;trace.y=y;trace.z=z;trace.hasBox=box!=nullptr;if(box)trace.box=*box;trace.injected=internal;trace.stage=renderStage;trace.recordingThread=GetCurrentThreadId();recentTextureCopies.add(trace);stereo::lists[list].textureCopies.add(trace);
    // A later write invalidates content-derived classifications, even when its
    // upload bytes cannot be captured. Reclassify only from current evidence.
    if(!internal&&(dest->Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||dest->SubresourceIndex==0)){std::lock_guard<std::mutex> guard(stereo::mapMutex);stereo::vignetteTextures.erase(dest->pResource);stereo::solidTexels.erase(dest->pResource);}
    bool classifyUpload=false;
    D3D12_TEXTURE_COPY_LOCATION correctedProfile{};
    const D3D12_TEXTURE_COPY_LOCATION* effectiveSource=source;
    // Bounded read-only evidence of scalar LUT upload bytes. CPU profile values
    // alone do not prove what is submitted to the texture sampled by the PS.
    if(!internal&&source->Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT&&
       dest->Type==D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX&&!dest->SubresourceIndex&&!x&&!y&&!z){
        const auto image=dest->pResource->GetDesc(),buffer=source->pResource->GetDesc();
        const auto& placed=source->PlacedFootprint;const auto& f=placed.Footprint;
        if(image.Width==129&&image.Height==1&&image.Format==DXGI_FORMAT_R32_FLOAT&&
           f.Format==DXGI_FORMAT_R32_FLOAT&&f.Width==129&&f.Height==1&&f.Depth==1&&
           placed.Offset<=buffer.Width&&516<=buffer.Width-placed.Offset){
            struct FogUpload {ComPtr<ID3D12Resource> resource;std::array<float,129> original{},last{};bool retained=false;};
            static std::mutex fogMutex;static std::vector<FogUpload> fogUploads;
            std::array<float,129> values{};
            if(stereo::copyConstants(source->pResource->GetGPUVirtualAddress()+placed.Offset,sizeof values,reinterpret_cast<unsigned char*>(values.data()))){
                std::lock_guard<std::mutex> fogLock(fogMutex);
                auto entry=std::find_if(fogUploads.begin(),fogUploads.end(),[&](const FogUpload& e){return e.resource.Get()==dest->pResource;});
                if(entry==fogUploads.end()&&fogUploads.size()<16){FogUpload e;e.resource=dest->pResource;e.last.fill(-1);fogUploads.push_back(std::move(e));entry=std::prev(fogUploads.end());}
                bool valid=true,flat=true;for(float v:values){valid=valid&&std::isfinite(v)&&v>=0&&v<=1;flat=flat&&v>=.99999f;}
                const bool full=!box||(box->left==0&&box->top==0&&box->front==0&&box->right==129&&box->bottom==1&&box->back==1);
                if(entry!=fogUploads.end()&&full&&valid){
                    if(!entry->retained&&values.front()>.99f&&values.back()<.01f){entry->original=values;entry->retained=true;}
                    if(nativeFog::active(GetTickCount64())&&flat&&entry->retained){
                        std::array<float,257> padded{};std::copy(entry->original.begin(),entry->original.end(),padded.begin());
                        if(auto* upload=flashlightProfileUpload::get(dest->pResource,padded)){
                            correctedProfile.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;correctedProfile.pResource=upload;
                            correctedProfile.PlacedFootprint.Footprint={DXGI_FORMAT_R32_FLOAT,129,1,1,1280};effectiveSource=&correctedProfile;
                            static unsigned retainedReports{};
                            if(entry->last!=values&&retainedReports<4){++retainedReports;log("native_fog_retained original_black_curve resource=%p",dest->pResource);}
                        }
                    }
                }
                static unsigned fogSamples{};
                if(entry!=fogUploads.end()&&entry->last!=values&&fogSamples<32){
                    const auto sample=fogSamples++;
                    log("native_fog_lut_upload sample=%u resource=%p",sample,dest->pResource);
                    for(unsigned i=0;i<129;i+=4)log("native_fog_lut_values sample=%u index=%u %.9g %.9g %.9g %.9g",sample,i,values[i],values[(std::min)(i+1,128u)],values[(std::min)(i+2,128u)],values[(std::min)(i+3,128u)]);
                }
                if(entry!=fogUploads.end())entry->last=values;
            }
        }
        const bool fullRegion=!box||(box->left==0&&box->top==0&&box->front==0&&box->right==257&&box->bottom==1&&box->back==1);
        if(image.Width==257&&image.Height==1){
            static std::atomic<unsigned> descriptions{};
            if(descriptions.fetch_add(1)<16)log("scalar_lut_copy_shape resource=%p format=%u source_format=%u source_size=%ux%ux%u pitch=%u box=%d full_region=%d internal=%d",dest->pResource,unsigned(image.Format),unsigned(f.Format),f.Width,f.Height,f.Depth,f.RowPitch,box!=nullptr,fullRegion,internal);
        }
        if(image.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&image.Width==257&&image.Height==1&&image.DepthOrArraySize==1&&image.Format==DXGI_FORMAT_R32_FLOAT&&
           fullRegion&&
           buffer.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER&&f.Format==image.Format&&f.Width==257&&f.Height==1&&f.Depth==1&&f.RowPitch>=1028&&
           placed.Offset<=buffer.Width&&1028<=buffer.Width-placed.Offset){
            // Explicit staged-session trial, scoped to the audited flashlight
            // resource. Native bytes, other LUTs and room lights stay untouched.
            if(flashlightProfileUpload::enabled.load(std::memory_order_relaxed)){
                nativeFlashlightProfileDiagnostic::Snapshot identity;
                if(nativeFlashlightProfileDiagnostic::capture(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),identity,nativeCopyReader::Reader{})&&identity.resourceKnown&&identity.resourceIdentity==reinterpret_cast<std::uintptr_t>(dest->pResource)){
                    float input[257]{};std::array<float,257> normalized{};
                    if(stereo::copyConstants(source->pResource->GetGPUVirtualAddress()+placed.Offset,sizeof input,reinterpret_cast<unsigned char*>(input))&&flashlightProfileRange::normalizeObservedFloor(input,normalized)){
                        if(auto* upload=flashlightProfileUpload::get(dest->pResource,normalized)){
                            correctedProfile.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;correctedProfile.pResource=upload;
                            correctedProfile.PlacedFootprint.Footprint={DXGI_FORMAT_R32_FLOAT,257,1,1,1280};effectiveSource=&correctedProfile;
                            static std::atomic<unsigned> applied{};if(applied.fetch_add(1)<4)log("flashlight_profile_trial applied resource=%p immutable_upload=%p",dest->pResource,upload);
                        }
                    }
                }
            }
            static std::atomic<unsigned> samples{};
            const auto sample=samples.fetch_add(1);
            static thread_local unsigned uploadTicket{},requestedSamples{};
            const auto requested=static_cast<unsigned>(sceneEvidenceRequest.load()>>32);
            if(requested!=uploadTicket){uploadTicket=requested;requestedSamples=0;}
            bool targeted=false;
            if(uploadTicket&&requestedSamples<4){
                nativeFlashlightProfileDiagnostic::Snapshot profile;
                targeted=nativeFlashlightProfileDiagnostic::capture(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),profile,nativeCopyReader::Reader{})&&profile.resourceKnown&&profile.resourceIdentity==reinterpret_cast<std::uintptr_t>(dest->pResource);
                if(targeted)++requestedSamples;
            }
            if(sample<64||targeted){
                float values[257]{};const bool copied=stereo::copyConstants(source->pResource->GetGPUVirtualAddress()+placed.Offset,sizeof values,reinterpret_cast<unsigned char*>(values));
                log("scalar_lut_upload sample=%u resource=%p list=%p copied=%d ticket=%u targeted=%d gpu_execution_verified=0",sample,dest->pResource,list,copied,uploadTicket,targeted);
                if(copied)for(unsigned row=0;row<257;row+=4)log("scalar_lut_upload_values sample=%u offset=%u %.9g %.9g %.9g %.9g",sample,row,values[row],row+1<257?values[row+1]:0,row+2<257?values[row+2]:0,row+3<257?values[row+3]:0);
            }
        }
    }
    if(!internal&&source->Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)classifyUpload=textureUploadMetadata::eligible(dest->pResource->GetDesc(),source->pResource->GetDesc(),*dest,x,y,z,*source,box);
    if(!internal&&*nativeUiAudit::directory()&&source->Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT){
        auto d=dest->pResource->GetDesc();if((d.Width==128&&d.Height==64)||(d.Width==1&&d.Height==1)){
            const auto& f=source->PlacedFootprint;log("hud_upload resource=%p size=%llux%u format=%u mips=%u sub=%u pitch=%u offset=%llu eligible=%d box=%d",dest->pResource,d.Width,d.Height,unsigned(d.Format),d.MipLevels,dest->SubresourceIndex,f.Footprint.RowPitch,f.Offset,classifyUpload,box!=nullptr);
        }
    }
    if(classifyUpload){auto d=dest->pResource->GetDesc();if(d.Width==1&&d.Height==1&&textureUploadMetadata::supportedFormat(d.Format)){unsigned char texel[4];if(stereo::copyConstants(source->pResource->GetGPUVirtualAddress()+source->PlacedFootprint.Offset,4,texel)){std::lock_guard<std::mutex> guard(stereo::mapMutex);if(d.Format==DXGI_FORMAT_B8G8R8A8_UNORM)std::swap(texel[0],texel[2]);stereo::solidTexels[dest->pResource]={texel[0]/255.f,texel[1]/255.f,texel[2]/255.f,texel[3]/255.f};}}}
    if(classifyUpload){auto d=dest->pResource->GetDesc();if(d.Width==128&&d.Height==64&&d.Format==DXGI_FORMAT_R8G8B8A8_UNORM){
        auto footprint=source->PlacedFootprint;std::vector<unsigned char> pixels(128*64*4);bool copied=true;
        for(UINT row=0;row<64;row++)copied=stereo::copyConstants(source->pResource->GetGPUVirtualAddress()+footprint.Offset+UINT64(row)*footprint.Footprint.RowPitch,512,pixels.data()+row*512)&&copied;
        if(copied&&pixels[0]==255&&pixels[1]==255&&pixels[2]==255&&pixels[3]==107&&pixels[(32*128+64)*4+3]==0){std::lock_guard<std::mutex> guard(stereo::mapMutex);stereo::vignetteTextures.insert(dest->pResource);}
    }}
    copySceneMetadata(dest->pResource,source->pResource,ContentTransfer::Preserve);originalCopyTexture(list,dest,x,y,z,effectiveSource,box);
}
static void STDMETHODCALLTYPE resourceBarriersHook(ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
    // Read-only bounded evidence before native validation can report an error.
    // No resource COM calls, state rewriting, command omission or extra calls.
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    auto& history=stereo::lists[list].barriers;
    if(!barriers||!count){BarrierTrace event;event.list=list;event.caller=caller;event.requested=count;event.injected=internal;event.stage=renderStage;recentBarriers.add(event);history.add(event);}
    else for(UINT index=0;index<count&&index<16;++index){BarrierTrace event;event.list=list;event.caller=caller;event.barrier=barriers[index];event.requested=count;event.index=index;event.known=true;event.injected=internal;event.stage=renderStage;recentBarriers.add(event);history.add(event);}
    originalResourceBarriers(list,count,barriers);
}
static void STDMETHODCALLTYPE resolveHook(ID3D12GraphicsCommandList* list,ID3D12Resource* dest,UINT destSubresource,ID3D12Resource* source,UINT sourceSubresource,DXGI_FORMAT format) {
    copySceneMetadata(dest,source);originalResolve(list,dest,destSubresource,source,sourceSubresource,format);
}
static void STDMETHODCALLTYPE clearRTVHook(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE target,const FLOAT* color,UINT n,const D3D12_RECT* rects) {
    if(!internal){std::lock_guard<std::mutex> guard(stereo::mapMutex);auto found=stereo::renderTargetResources.find(target.ptr);if(found!=stereo::renderTargetResources.end()){stereo::sceneTextures.erase(found->second);stereo::capturedUITextures.erase(found->second);stereo::vignetteTextures.erase(found->second);stereo::solidTexels.erase(found->second);}}
    originalClearRTV(list,target,color,n,rects);
}
static void STDMETHODCALLTYPE verticesHook(ID3D12GraphicsCommandList* list,UINT start,UINT count,const D3D12_VERTEX_BUFFER_VIEW* views) {
    // Validate by subtraction: start+count can wrap and admit an out-of-bounds
    // metadata write before D3D12 gets the original invalid call.
    if(!internal&&start<=32&&count<=32-start)for(UINT i=0;i<count;i++)stereo::lists[list].vertices[start+i]=views?views[i]:D3D12_VERTEX_BUFFER_VIEW{};
    originalVertexBuffers(list,start,count,views);
}
static void STDMETHODCALLTYPE indicesHook(ID3D12GraphicsCommandList* list,const D3D12_INDEX_BUFFER_VIEW* view) {
    if(!internal)stereo::lists[list].indices=view?*view:D3D12_INDEX_BUFFER_VIEW{};originalIndexBuffer(list,view);
}
static void STDMETHODCALLTYPE bufferCopyHook(ID3D12GraphicsCommandList* list,ID3D12Resource* dest,UINT64 destOffset,ID3D12Resource* source,UINT64 sourceOffset,UINT64 bytes) {
    // Invalid native copies must still reach D3D12 validation, but must not
    // manufacture apparently valid CPU capture metadata before that happens.
    if(internal||!dest||!source){originalCopyBuffer(list,dest,destOffset,source,sourceOffset,bytes);return;}
    auto destination=dest->GetDesc(),origin=source->GetDesc();
    if(bytes&&bytes<=64*1024*1024&&destination.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER&&origin.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER&&destination.Width<=64*1024*1024&&destOffset<=destination.Width&&bytes<=destination.Width-destOffset&&sourceOffset<=origin.Width&&bytes<=origin.Width-sourceOffset) {
        std::vector<unsigned char> copy(static_cast<size_t>(bytes));
        if(stereo::copyConstants(source->GetGPUVirtualAddress()+sourceOffset,static_cast<unsigned>(bytes),copy.data())) {
            std::lock_guard<std::mutex> guard(stereo::mapMutex);auto& target=stereo::uploads[dest->GetGPUVirtualAddress()];
            if(!target.mappings){
                target.resource=dest;target.observedCopy=true;target.copied(static_cast<size_t>(dest->GetDesc().Width),static_cast<size_t>(destOffset),copy.data(),static_cast<size_t>(bytes));
            }
        }
    }originalCopyBuffer(list,dest,destOffset,source,sourceOffset,bytes);
}
// Unused candidate registration builder; Initialize still installs the original
// hooks below. Configure only while recording is quiescent, before enabling.
static auto candidateInterveningBindings(ID3D12GraphicsCommandList* list){
    std::array<nativeCopyInstall::Binding,47> result{};if(!list)return result;
    auto entries=nativeCopyForwarders::entries();auto table=*reinterpret_cast<void***>(list);
    for(size_t i=0;i<entries.size();++i)result[i]={table[entries[i].slot],entries[i].hook,entries[i].original};
    auto bind=[&](unsigned slot,void** storage){for(size_t i=0;i<entries.size();++i)if(entries[i].slot==slot)result[i].original=storage;};
#define NATIVE_COPY_BRIDGE_CHAIN(slot,method,body,storage) bind(slot,nativeCopyForwarders::chain<&ID3D12GraphicsCommandList::method>(&body,storage));
    NATIVE_COPY_BRIDGE_CHAIN(12,DrawInstanced,drawHook,originalDraw)
    NATIVE_COPY_BRIDGE_CHAIN(13,DrawIndexedInstanced,indexedHook,originalIndexed)
    NATIVE_COPY_BRIDGE_CHAIN(15,CopyBufferRegion,bufferCopyHook,originalCopyBuffer)
    NATIVE_COPY_BRIDGE_CHAIN(17,CopyResource,resourceCopyHook,originalCopyResource)
    NATIVE_COPY_BRIDGE_CHAIN(19,ResolveSubresource,resolveHook,originalResolve)
    NATIVE_COPY_BRIDGE_CHAIN(21,RSSetViewports,viewportsHook,originalViewports)
    NATIVE_COPY_BRIDGE_CHAIN(22,RSSetScissorRects,scissorsHook,originalScissors)
    NATIVE_COPY_BRIDGE_CHAIN(25,SetPipelineState,setPipelineHook,originalSetPipeline)
    NATIVE_COPY_BRIDGE_CHAIN(28,SetDescriptorHeaps,heapsHook,originalSetHeaps)
    NATIVE_COPY_BRIDGE_CHAIN(30,SetGraphicsRootSignature,setRootSignatureHook,originalSetRootSignature)
    NATIVE_COPY_BRIDGE_CHAIN(32,SetGraphicsRootDescriptorTable,tableHook,originalTable)
    NATIVE_COPY_BRIDGE_CHAIN(38,SetGraphicsRootConstantBufferView,cbvHook,originalCBV)
    NATIVE_COPY_BRIDGE_CHAIN(43,IASetIndexBuffer,indicesHook,originalIndexBuffer)
    NATIVE_COPY_BRIDGE_CHAIN(44,IASetVertexBuffers,verticesHook,originalVertexBuffers)
    NATIVE_COPY_BRIDGE_CHAIN(46,OMSetRenderTargets,targetsHook,originalTargets)
    NATIVE_COPY_BRIDGE_CHAIN(48,ClearRenderTargetView,clearRTVHook,originalClearRTV)
#undef NATIVE_COPY_BRIDGE_CHAIN
    return result;
}
// Unused complete legacy candidate table. Runtime/context/dispatch configuration
// and transactional installation remain separate; Initialize does not call it.
static auto candidateCommandBindings(ID3D12GraphicsCommandList* list){
    std::array<nativeCopyInstall::Binding,51> result{};if(!list)return result;
    auto entries=nativeCopyForwarders::entries();auto intervening=candidateInterveningBindings(list);auto table=*reinterpret_cast<void***>(list);
    for(size_t i=0;i<entries.size();++i)result[entries[i].slot-9]=intervening[i];
    nativeCopyEndpoints::originalReset=&resetHook;nativeCopyEndpoints::originalCopy=&textureCopyHook;nativeCopyEndpoints::originalBarrier=&resourceBarriersHook;
    auto bind=[&](unsigned slot,void* detour,void** storage){result[slot-9]={table[slot],detour,storage};};
#define NATIVE_COPY_BRIDGE_ENDPOINT(slot,method,storage) bind(slot,reinterpret_cast<void*>(nativeCopyEndpoints::method),reinterpret_cast<void**>(&storage));
    NATIVE_COPY_BRIDGE_ENDPOINT(9,close,nativeCopyEndpoints::originalClose)
    NATIVE_COPY_BRIDGE_ENDPOINT(10,reset,originalReset)
    NATIVE_COPY_BRIDGE_ENDPOINT(16,copy,originalCopyTexture)
    NATIVE_COPY_BRIDGE_ENDPOINT(26,barrier,originalResourceBarriers)
#undef NATIVE_COPY_BRIDGE_ENDPOINT
    return result;
}
template<class T> static bool hook(void* object,unsigned slot,void* detour,T& original) {
    void* target=(*reinterpret_cast<void***>(object))[slot];
    HMODULE owner{};wchar_t ownerPath[MAX_PATH]{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(target),&owner);
    if(owner)GetModuleFileNameW(owner,ownerPath,MAX_PATH);
    log("hook_target slot=%u module=%ls",slot,ownerPath);
    MH_STATUS r=MH_CreateHook(target,detour,reinterpret_cast<void**>(&original));
    if(r!=MH_OK) {log("hook slot=%u error=%s",slot,MH_StatusToString(r));return false;}
    return true;
}
static DWORD initializeBody() {
    initDiagnostics::mark("configuration");
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(self,path,MAX_PATH);
    auto slash=wcsrchr(path,L'\\'); if(!slash) return 1; wcscpy_s(slash+1,MAX_PATH-(slash+1-path),L"bridge.log");
    logFile=_wfsopen(path,L"w",_SH_DENYNO);if(!logFile)initDiagnostics::write("legacy_bridge_log_unavailable errno=%d path=%ls diagnostic_log_remains_active=1",errno,path);
    initDiagnostics::mark("audioFaultEvidence::install");
    {wchar_t evidencePath[MAX_PATH]{};wcscpy_s(evidencePath,path);auto name=wcsrchr(evidencePath,L'\\');if(name){wcscpy_s(name+1,MAX_PATH-(name+1-evidencePath),L"audio-fault-evidence.bin");log("audio_fault_evidence_enabled=%d",audioFaultEvidence::install(evidencePath));}}
    wcscpy_s(slash+1,MAX_PATH-(slash+1-path),L"DementiumOpenXR.ini");
    wcscpy_s(trackingProfile,path);
    {wchar_t pouch[128]{};GetPrivateProfileStringW(L"Tracking",L"AmmoPouch",L"",pouch,128,path);
     XrVector3f p{};if(swscanf_s(pouch,L"%f,%f,%f",&p.x,&p.y,&p.z)==3&&manualReload::setAmmoPouch(p))
         log("ammo_pouch_calibration loaded=1 xyz=%.6f,%.6f,%.6f",p.x,p.y,p.z);}
    {wchar_t alignment[160]{};GetPrivateProfileStringW(L"Tracking",L"LeftHandAlignment",L"",alignment,160,path);
     XrPosef candidate{{0,0,0,1},{}};auto& q=candidate.orientation;
     if(swscanf_s(alignment,L"%f,%f,%f,%f",&q.x,&q.y,&q.z,&q.w)==4&&motionTracking::validPose(candidate)){
         leftAlignmentCorrection=q;log("left_hand_alignment loaded=1 xyzw=%.9f,%.9f,%.9f,%.9f",q.x,q.y,q.z,q.w);
     }}
    for(auto mode:{headReference::Posture::Seated,headReference::Posture::Standing}){wchar_t height[32]{};GetPrivateProfileStringW(L"Tracking",mode==headReference::Posture::Standing?L"StandingHeight":L"SeatedHeight",L"0",height,32,path);stereo::postures.calibrate(mode,static_cast<float>(wcstod(height,nullptr)));}stereo::postures.resetMode();
    wchar_t value[64]{};GetPrivateProfileStringW(L"VR",L"UnitsPerMeter",L"1.0",value,64,path);
    double scale=wcstod(value,nullptr);if(std::isfinite(scale)&&scale>=0.01&&scale<=1000)stereo::unitsPerMeter=static_cast<float>(scale);
    gamepadBridge::enabled=GetPrivateProfileIntW(L"VR",L"EnableControllers",1,path)!=0;
    nativeMotion::enabled=GetPrivateProfileIntW(L"VR",L"EnableMotionAim",1,path)!=0;
    nativeMotion::bodyAnchorTrial=GetPrivateProfileIntW(L"Diagnostics",L"WeaponBodyAnchor",0,path)!=0;
    nativeFog::enabled=GetPrivateProfileIntW(L"VR",L"EnableDistanceFog",0,path)!=0;
    stereoWorldCullingTrial=GetPrivateProfileIntW(L"VR",L"StereoWorldCulling",0,path)!=0;
    stereoSectorVisibility=GetPrivateProfileIntW(L"VR",L"StereoSectorVisibility",0,path)!=0;
    flashlightProfileUpload::enabled=GetPrivateProfileIntW(L"VR",L"CorrectFlashlightFalloff",0,path)!=0;
    char profileTrial[8]{};
    if(GetEnvironmentVariableA("DEMENTIUM_FLASHLIGHT_PROFILE_TRIAL",profileTrial,sizeof profileTrial)==1)
        flashlightProfileUpload::enabled=profileTrial[0]=='1';
    log("flashlight_falloff_correction=%d",flashlightProfileUpload::enabled.load());
    stereoPickupCullingTrial=GetPrivateProfileIntW(L"Diagnostics",L"StereoPickupCulling",0,path)!=0;
    log("stereo_pickup_culling_trial=%d",stereoPickupCullingTrial.load());
    useOriginalHands=GetPrivateProfileIntW(L"VR",L"UseOriginalHands",1,path)!=0;
    useOriginalSniper=GetPrivateProfileIntW(L"VR",L"UseOriginalSniper",1,path)!=0;
    nativeMotion::headRelativeMovement=GetPrivateProfileIntW(L"VR",L"HeadRelativeMovement",1,path)!=0;
    manualReload::enabled=GetPrivateProfileIntW(L"VR",L"EnableManualReload",1,path)!=0;
    arsenal::enabled=manualReload::enabled;
    wristband::settings.enabled=GetPrivateProfileIntW(L"Wristband",L"Enabled",1,path)!=0;
    wristband::settings.wrist=GetPrivateProfileIntW(L"Wristband",L"Wrist",0,path)==1?1:0;
    wristband::settings.always=GetPrivateProfileIntW(L"Wristband",L"AlwaysVisible",0,path)!=0;
    for(unsigned hand=0;hand<2;hand++)for(unsigned axis=0;axis<3;axis++){
        wchar_t key[32],number[48];swprintf_s(key,L"%sOffset%c",hand?L"Right":L"Left",L'X'+axis);
        wchar_t fallback[32];swprintf_s(fallback,L"%.6f",(&wristband::settings.offset[hand].x)[axis]);GetPrivateProfileStringW(L"Wristband",key,fallback,number,48,path);float offset=static_cast<float>(wcstod(number,nullptr));
        if(std::isfinite(offset)&&std::fabs(offset)<=.2f)(&wristband::settings.offset[hand].x)[axis]=offset;
        swprintf_s(key,L"%sRotation%c",hand?L"Right":L"Left",L'X'+axis);GetPrivateProfileStringW(L"Wristband",key,L"0",number,48,path);float angle=static_cast<float>(wcstod(number,nullptr));
        if(std::isfinite(angle)&&std::fabs(angle)<=180)(&wristband::settings.rotation[hand].x)[axis]=angle*.01745329252f;
    }
    arsenal::meleeEnabled=GetPrivateProfileIntW(L"VR",L"EnableMotionMelee",1,path)!=0;
    sniperScope::enabled=GetPrivateProfileIntW(L"VR",L"EnableSniperScope",0,path)!=0;
    opticScope::enabled=GetPrivateProfileIntW(L"VR",L"EnableOpticalScope",0,path)!=0;
    arsenal::opticalMovement=opticScope::enabled;
    if(opticScope::enabled)sniperScope::enabled=false;
    wchar_t scopeValue[32]{};GetPrivateProfileStringW(L"VR",L"SniperMagnification",L"3.0",scopeValue,32,path);
    float zoom=static_cast<float>(wcstod(scopeValue,nullptr));if(std::isfinite(zoom)&&zoom>=1&&zoom<=8)sniperScope::zoom=zoom;
    log("DementiumOpenXR experimental scene stereo bridge");
    initDiagnostics::mark("weaponComfort::install");
    log("gunfire_pushback_removed=%d",weaponComfort::install());
    internal=true;
    // The game enables the D3D12 debug layer. Hook its public COM entry points,
    // keeping queues, devices, fences and resources in the same wrapper domain.
    // Hooking the underlying driver methods mixes wrapped and unwrapped objects.
    ComPtr<ID3D12Debug> debug;
    char debugExperiment[4]{};
    const bool withoutValidation=GetEnvironmentVariableA("DEMENTIUM_TEST_NO_D3D12_DEBUG",debugExperiment,sizeof(debugExperiment))==1&&debugExperiment[0]=='1';
    initDiagnostics::mark("D3D12GetDebugInterface");
    const bool haveDebug=SUCCEEDED(initDiagnostics::result("D3D12GetDebugInterface",D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));
    if(withoutValidation){
        // Fresh suspended launches only: bootstrap and later native devices
        // must both stay unwrapped. Never disable validation on a live device.
    initDiagnostics::mark("MH_Initialize");
        if(!haveDebug||MH_Initialize()!=MH_OK)return 8;
        if(!debugLayerStartup::suppressEnable(debug.Get())){MH_Uninitialize();return 8;}
        log("bootstrap_debug_layer=disabled_startup_experiment");
    }else if(haveDebug){debug->EnableDebugLayer();log("bootstrap_debug_layer=enabled");}
    initDiagnostics::mark("D3D12CreateDevice");
    ComPtr<ID3D12Device> device; HRESULT r=initDiagnostics::result("D3D12CreateDevice",D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    if(SUCCEEDED(r)){auto luid=device->GetAdapterLuid();initDiagnostics::write("bootstrap_adapter_luid=%08lx:%08lx",luid.HighPart,luid.LowPart);}
    if(FAILED(r)) {log("bootstrap device failed %08lx",r);return 2;}
    ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC qd{};
    initDiagnostics::mark("CreateCommandQueue");
    if(FAILED(initDiagnostics::result("device->CreateCommandQueue",device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue))))) return 3;
    ComPtr<ID3D12CommandAllocator> allocator;
    initDiagnostics::mark("CreateCommandAllocator");
    if(FAILED(initDiagnostics::result("device->CreateCommandAllocator",device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))))) return 4;
    ComPtr<ID3D12GraphicsCommandList> list;
    initDiagnostics::mark("CreateCommandList");
    if(FAILED(initDiagnostics::result("device->CreateCommandList",device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))))) return 5;
    if(FAILED(initDiagnostics::result("bootstrap Close",list->Close())))return 5;
    ComPtr<ID3D12Resource> upload;D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=256;bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    initDiagnostics::mark("CreateCommittedResource");
    if(FAILED(initDiagnostics::result("device->CreateCommittedResource",device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)))))return 10;
    ComPtr<IDXGIFactory4> factory;
    initDiagnostics::mark("CreateDXGIFactory1");
    if(FAILED(initDiagnostics::result("CreateDXGIFactory1",CreateDXGIFactory1(IID_PPV_ARGS(&factory))))) return 6;
    WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=self; wc.lpszClassName=L"DementiumXRBootstrap";
    RegisterClassW(&wc);
    initDiagnostics::mark("CreateWindowW");
    HWND window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,self,nullptr);
    if(!window){initDiagnostics::os("CreateWindowW",GetLastError());return 7;}
    DXGI_SWAP_CHAIN_DESC1 sc{}; sc.Width=64; sc.Height=64; sc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.SampleDesc.Count=1; sc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sc.BufferCount=2; sc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    initDiagnostics::mark("CreateSwapChainForHwnd");
    ComPtr<IDXGISwapChain1> chain; r=initDiagnostics::result("factory->CreateSwapChainForHwnd",factory->CreateSwapChainForHwnd(queue.Get(),window,&sc,nullptr,nullptr,&chain));
    if(FAILED(r)) {log("bootstrap swapchain failed %08lx",r);DestroyWindow(window);return 7;}
    initDiagnostics::mark("MH_Initialize");
    if(!withoutValidation&&MH_Initialize()!=MH_OK) return 8;
    initDiagnostics::mark("nativeSwarmBounds::install");
    log("native_swarm_bounds_guard=%d",nativeSwarmBounds::install());
    {char value[4]{};if(GetEnvironmentVariableA("DEMENTIUM_TEST_NATIVE_PRESENT_CADENCE",value,sizeof value)==1&&value[0]=='1'){
        log("native_present_cadence_rejected_legacy_mode=1");
    }}
    {char value[4]{};if(GetEnvironmentVariableA("DEMENTIUM_TEST_TIMED_PRESENT_CADENCE",value,sizeof value)==1&&value[0]=='1'){
    initDiagnostics::mark("nativeSimulationClock::install");
        const bool timingInstalled=nativeSimulationClock::install();
        log("native_simulation_clock_installed=%d",timingInstalled);
        if(timingInstalled){
            nativeSimulationClock::enabled=[](){return stereo::active&&!stereo::failed&&stereo::xr&&!stereo::xr->desktopDiagnostic&&stereo::xr->running&&gamepadBridge::menuState()==0&&!pillInventory::open();};
            nativePresentCadence::enabled=[](){return nativeSimulationClock::inFrame;};
            nativePresentCadence::sourceRateObserved=[](std::uint64_t n,std::uint64_t d){const double hz=double(n)/double(d);if(hz>=30&&hz<=120)nativeSimulationClock::nativeHz=hz;log("native_simulation_source_rate=%llu/%llu hz=%.9f",n,d,hz);};
    initDiagnostics::mark("nativePresentCadence::install");
            const bool cadenceInstalled=nativePresentCadence::install();
            log("native_present_cadence_installed=%d",cadenceInstalled);
            if(!cadenceInstalled)nativeSimulationClock::enabled=nullptr;
        }
    }}
    interactionGaze::evidence=[](const float* native,const float* gaze,unsigned before,unsigned after,const float* nativePos,const float* eyePos,bool prompt){static ULONGLONG last=0;auto now=GetTickCount64();if(now-last<1000)return;last=now;log("interaction_gaze native=%.4f,%.4f,%.4f gaze=%.4f,%.4f,%.4f triggers=%u/%u native_pos=%.3f,%.3f,%.3f eye_pos=%.3f,%.3f,%.3f prompt=%d",native[0],native[1],native[2],gaze[0],gaze[1],gaze[2],before,after,nativePos[0],nativePos[1],nativePos[2],eyePos[0],eyePos[1],eyePos[2],prompt);};
    initDiagnostics::mark("interactionGaze::install");
    log("interaction_gaze_hook=%d",interactionGaze::install());
    initDiagnostics::mark("nativeMotion::install");
    bool motionHooks=nativeMotion::install();log("native_motion_hooks=%d enabled=%d",motionHooks,nativeMotion::enabled);
    weaponGroups::equipmentObserved=[](int weapon,std::uint64_t now){sniperScope::visibleWeapon=weapon;sniperScope::weaponTime=now;};
    log("native_sphere_diagnostic=%d",installSphereDiagnostic());
    log("native_sector_diagnostic=%d",installSectorDiagnostic());
    stereo::captureObserved=[](unsigned frame,bool world,unsigned draws,std::uint64_t age){
        log("geometry_capture pid=%lu frame=%u world=%d draws=%u world_age_ms=%llu mode=%u sphere_overrides=%u sector_calls=%u synthetic=%d",GetCurrentProcessId(),frame,world,draws,static_cast<unsigned long long>(age),static_cast<unsigned>(geometryIsolation::mode(GetTickCount64())),diagnosticSphereRejections.load(),diagnosticSectorCalls.load(),stereo::xr&&stereo::xr->desktopDiagnostic);
    };
    wchar_t gameExecutable[MAX_PATH]{};GetModuleFileNameW(nullptr,gameExecutable,MAX_PATH);
    wchar_t modFile[MAX_PATH]{};GetModuleFileNameW(self,modFile,MAX_PATH);
    log("original_hands_trial=%d",useOriginalHands);
    initDiagnostics::mark("replacementHands::load");
    log("replacement_hand_assets=%d",replacementHands::load(std::filesystem::path(modFile).parent_path()/(useOriginalHands?L"NativeHands.bin":L"FloatingHands.bin")));
    initDiagnostics::mark("weaponGripHands::load");
    bool gripHands=weaponGripHands::load(std::filesystem::path(gameExecutable).parent_path()/L"rom.zar");
    initDiagnostics::mark("huntingRifle::load");
    log("hunting_rifle_assets=%d",huntingRifle::load(std::filesystem::path(modFile).parent_path()/L"HuntingRifle.bin"));
    nativeMotion::replacementDraw=huntingRifle::replaceNative;
    initDiagnostics::mark("wristband::loadSelected");
    const bool selectedAssets=wristband::loadSelected(std::filesystem::path(modFile).parent_path()/L"SelectedAssets");
    log("selected_optional_profiles pistol=%d holding=%d revolver=%d rifle=%d shotgun_motion=%d pistol_well=%d pistol_feed_channel=%d pistol_passage=%d",selectedReload::profile.ready,selectedHoldingHand::section!=~0u,selectedRevolver::profile.ready,selectedRifle::profile.ready,selectedShotgun::motion.ready,selectedPistolWell::applied,selectedPistolFeedChannel::applied,selectedPistolPassage::applied);
    log("saved_psx_selected_assets=%d",selectedAssets);
    if(selectedAssets){nativeMotion::replacementDraw=wristband::replaceSelectedNative;supportAim::selectedContact=selectedFrames::contact;}
    nativeMotion::flashEvidence=[](unsigned w,const float* local,const float* tip,const float* m){static unsigned count=0;if(count++<8)log("flash_alignment weapon=%u local=%.5f,%.5f,%.5f tip=%.5f,%.5f,%.5f matrix=%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f",w,local[0],local[1],local[2],tip[0],tip[1],tip[2],m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11]);};
    nativeMotion::gripPoseSink=weaponGripHands::capture;log("missing_grip_hands_assets=%d",gripHands);
    initDiagnostics::mark("manualReload::install");
    bool reloadHooks=manualReload::install();log("manual_reload_hooks=%d enabled=%d",reloadHooks,manualReload::enabled);
    arsenal::rayAccessor=nativeMotion::originalRay;
    initDiagnostics::mark("pillBottle::load");
    log("pill_bottle_adapter=%d",(replacementHands::ready||selectedAssets)&&pillBottle::load(std::filesystem::path(modFile).parent_path()/L"PillBottle.mesh")&&pillBottle::install());
    log("selected_finish materials=%d wrist_ends=%u textured_cases=%u",selectedSurface::bound,selectedWristFinish::shortened,unsigned(arsenalProps::texturedCases[1])+unsigned(arsenalProps::texturedCases[2])+unsigned(arsenalProps::texturedCases[3])+unsigned(arsenalProps::texturedCases[4]));
    pillBottle::pickupReport=reportNativePickup;
    pillBottle::pickupRenderReport=recordPickupDraw;
    pillBottle::visibilityReport=reportPickupVisibility;
    log("pickup_visibility_observer=%d",pillBottle::installVisibilityObserver());
    initDiagnostics::mark("ammoLimits::install");
    bool ammoCaps=ammoLimits::install(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));log("three_magazine_reserve_limits=%d",ammoCaps);
    initDiagnostics::mark("arsenal::install");
    bool arsenalHooks=arsenal::install();log("arsenal_hooks=%d reload=%d melee=%d",arsenalHooks,arsenal::enabled,arsenal::meleeEnabled);
    huntingRifle::enabled=arsenalHooks&&!useOriginalSniper;
    arsenal::reloads[3].useDetachableMagazine(!useOriginalSniper);
    arsenal::reloads[0].useSpeedloader(true);
    log("original_sniper_model=%d",useOriginalSniper);
    stereo::extraSceneDraw=wristband::drawExtras;
    stereo::warmSceneResources=[](ID3D12Device* device,ID3D12GraphicsCommandList* list,unsigned samples){if(!wristband::selectedMode)return;static bool reported=false;const auto start=GetTickCount64();if(wristband::initialize(device,list,samples)&&!reported){reported=true;log("startup_resources_ready in_menu=1 duration_ms=%llu tick=%llu",GetTickCount64()-start,GetTickCount64());}};
    initDiagnostics::mark("reloadAudio::load");
    reloadAudio::load(std::filesystem::path(modFile).parent_path()/L"ReloadSounds");
    initDiagnostics::mark("modPreferences::load");
    modPreferences::load(std::filesystem::path(modFile).parent_path()/L"ModPreferences.ini");
    arsenalProps::loadCases(std::filesystem::path(modFile).parent_path()/L"SelectedAssets");
    selectedHuntingFlash::capture=[](){stereo::captureRequested=true;};
    handheldLight::capture=[](){stereo::captureRequested=true;};
    magazineDrop::capture=[](){stereo::captureRequested=true;};
    chamberFeedCapture::capture=[](){stereo::captureRequested=true;};
    casingFlight::capture=[](){stereo::captureRequested=true;};
    dropSurface::trace=[](const dropSurface::Query& q,float& floor)->bool{
        if(!arsenal::installed||gamepadBridge::menuState()!=0)return false;
        auto level=*reinterpret_cast<unsigned char**>(arsenal::image+0x3a5330);
        if(!manualReload::readable(level,0x138)||*reinterpret_cast<std::uintptr_t*>(level+0x130)!=q.scene)return false;
        float ray[]={q.x,q.y,q.z,0,-1,0},length=q.range;bool hit=false;
        if(!arsenal::worldLimit(ray,length,0,&hit)||!hit)return false;
        floor=q.y-length+q.clearance;return true;
    };
    arsenalProps::worldFrame=[](DirectX::XMMATRIX& mapping,std::uintptr_t& scene)->bool{
        using namespace DirectX;
        if(!stereo::originSet||!motionTracking::validPose(stereo::origin)||!std::isfinite(stereo::unitsPerMeter)||stereo::unitsPerMeter<=0)return false;
        auto camera=*reinterpret_cast<unsigned char**>(nativeMotion::base+0x3a5338);
        auto level=*reinterpret_cast<unsigned char**>(nativeMotion::base+0x3a5330);
        if(!manualReload::readable(camera,0xbc+48)||!manualReload::readable(level,0x138))return false;
        scene=*reinterpret_cast<std::uintptr_t*>(level+0x130);if(!scene)return false;
        float v[12];std::memcpy(v,camera+0xbc,sizeof v);if(!motionTracking::validBasis(v))return false;
        auto view=XMMatrixSet(v[0],v[4],v[8],0,v[1],v[5],v[9],0,v[2],v[6],v[10],0,v[3],v[7],v[11],1);
        const auto& o=stereo::origin;auto origin=motionTracking::rotation(o.orientation)*XMMatrixTranslation(o.position.x,o.position.y,o.position.z);
        mapping=XMMatrixInverse(nullptr,origin)*XMMatrixScaling(stereo::unitsPerMeter,stereo::unitsPerMeter,stereo::unitsPerMeter)*XMMatrixInverse(nullptr,view);
        return true;
    };
    medicineSaves::loadoutRestored=[](int saved,int current,bool request){log("save_loadout_restored saved=%d current=%d equip_requested=%d tick=%llu",saved,current,request,GetTickCount64());};
    stereo::presentationObserved=[](bool gameplay,bool world,bool desktop){static int previous=-1;const int state=int(gameplay)*4+int(world)*2+int(desktop);if(state!=previous){previous=state;log("vr_presentation gameplay=%d stereo_world=%d native_desktop=%d tick=%llu",gameplay,world,desktop,GetTickCount64());}};
    wristband::selectedVisibilityObserved=[](int weapon,bool right,bool left){static int previous=-1;const int state=(weapon+1)*8+int(right)*4+int(left)*2+int(stereo::originSet);if(state!=previous){previous=state;log("startup_visible weapon=%d right=%d left=%d origin_committed=%d tick=%llu",weapon,right,left,stereo::originSet,GetTickCount64());}};
    wristband::selectedLoadoutDrawn=[](){if(medicineSaves::startupLoading.exchange(false))log("startup_loadout_drawn tick=%llu",GetTickCount64());};
    stereo::gameplayActive=[](){return gamepadBridge::menuState()==0;};
    stereo::extraUIDraw=[](ID3D12Device* d,ID3D12GraphicsCommandList* l,ID3D12Resource* t,unsigned w,unsigned h){return saveSlotUI::draw(d,l,t,w,h)||pillInventory::draw(d,l,t,w,h);};stereo::extraUICapture=pillInventory::captureMap;stereo::extraUISuppressFade=pillInventory::suppressFade;stereo::extraUIVisible=[](){return saveSlotMenu::visible()||pillInventory::open()||pillInventory::closingMap;};
    stereo::prepareSceneExtras=wristband::prepareFrame;
    log("wristband_adapter=%d",wristband::installAdapter());
    initDiagnostics::mark("headLamp::install");
    log("head_lamp_adapter=%d",headLamp::install());
    headLamp::vrPresentation=nativeMotion::enabled;
    initDiagnostics::mark("medicineSaves::install");
    log("medicine_save_adapter=%d",medicineSaves::install(std::filesystem::path(modFile).parent_path()/L"medicine-saves"));
    if(std::filesystem::exists(std::filesystem::path(modFile).parent_path()/L"EnableThreeSaveSlots.txt")){
        wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    initDiagnostics::mark("saveSlotFiles::install");
        bool slots=*local&&saveSlotFiles::install(std::filesystem::path(modFile).parent_path()/L"save-slots",std::filesystem::path(local)/L"Dementium - The Ward"/L"saves");
        log("three_save_slots=%d error=%s",slots,saveSlotFiles::installError);
    initDiagnostics::mark("saveSlotMenu::install");
        if(!slots||!saveSlotMenu::install()){MH_Uninitialize();return 10;}
        saveSlotLayout::path=std::filesystem::path(modFile).parent_path()/L"SaveSlotLayout.json";
        gamepadBridge::captureSaveMenu=saveSlotMenu::input;
    }
    medicineSaves::fileObserved=[](const wchar_t* path,bool write){static std::atomic<unsigned> count{};if(count++<40)log("save_io_observed write=%d path=%ls",write,path);};
    if(!motionHooks)nativeMotion::enabled=false;
    bool ok=hook(factory.Get(),10,reinterpret_cast<void*>(createSwapHook),originalCreateSwap);
    ok=hook(factory.Get(),15,reinterpret_cast<void*>(createSwapHwndHook),originalCreateSwapHwnd)&&ok;
    ok=hook(chain.Get(),8,reinterpret_cast<void*>(presentHook),originalPresent)&&ok;
    ok=hook(queue.Get(),10,reinterpret_cast<void*>(executeHook),originalExecute)&&ok;
    ok=hook(device.Get(),10,reinterpret_cast<void*>(pipelineHook),originalPipeline)&&ok;
    initDiagnostics::mark("ok=hook(device.Get(),12,reinterpret_cast<void*>(createCommandListHook),originalCreateCommandList)&&ok;");
    ok=hook(device.Get(),12,reinterpret_cast<void*>(createCommandListHook),originalCreateCommandList)&&ok;
    ok=hook(device.Get(),16,reinterpret_cast<void*>(rootHook),originalRoot)&&ok;
    ok=hook(list.Get(),12,reinterpret_cast<void*>(drawHook),originalDraw)&&ok;
    ok=hook(list.Get(),13,reinterpret_cast<void*>(indexedHook),originalIndexed)&&ok;
    ok=hook(list.Get(),38,reinterpret_cast<void*>(cbvHook),originalCBV)&&ok;
    ok=hook(list.Get(),30,reinterpret_cast<void*>(setRootSignatureHook),originalSetRootSignature)&&ok;
    ok=hook(list.Get(),32,reinterpret_cast<void*>(tableHook),originalTable)&&ok;
    ok=hook(upload.Get(),8,reinterpret_cast<void*>(mapHook),originalMap)&&ok;
    ok=hook(upload.Get(),9,reinterpret_cast<void*>(unmapHook),originalUnmap)&&ok;
    ok=hook(list.Get(),10,reinterpret_cast<void*>(resetHook),originalReset)&&ok;
    ok=hook(list.Get(),25,reinterpret_cast<void*>(setPipelineHook),originalSetPipeline)&&ok;
    ok=hook(list.Get(),21,reinterpret_cast<void*>(viewportsHook),originalViewports)&&ok;
    ok=hook(list.Get(),22,reinterpret_cast<void*>(scissorsHook),originalScissors)&&ok;
    ok=hook(list.Get(),46,reinterpret_cast<void*>(targetsHook),originalTargets)&&ok;
    ok=hook(device.Get(),20,reinterpret_cast<void*>(createRTVHook),originalCreateRTV)&&ok;
    ok=hook(device.Get(),18,reinterpret_cast<void*>(srvHook),originalCreateSRV)&&ok;
    ok=hook(device.Get(),23,reinterpret_cast<void*>(copyDescriptorsHook),originalCopyDescriptors)&&ok;
    ok=hook(device.Get(),24,reinterpret_cast<void*>(copySimpleHook),originalCopySimple)&&ok;
    ok=hook(list.Get(),28,reinterpret_cast<void*>(heapsHook),originalSetHeaps)&&ok;
    ok=hook(list.Get(),16,reinterpret_cast<void*>(textureCopyHook),originalCopyTexture)&&ok;
    ok=hook(list.Get(),26,reinterpret_cast<void*>(resourceBarriersHook),originalResourceBarriers)&&ok;
    ok=hook(list.Get(),17,reinterpret_cast<void*>(resourceCopyHook),originalCopyResource)&&ok;
    ok=hook(list.Get(),19,reinterpret_cast<void*>(resolveHook),originalResolve)&&ok;
    ok=hook(list.Get(),48,reinterpret_cast<void*>(clearRTVHook),originalClearRTV)&&ok;
    ok=hook(list.Get(),43,reinterpret_cast<void*>(indicesHook),originalIndexBuffer)&&ok;
    ok=hook(list.Get(),44,reinterpret_cast<void*>(verticesHook),originalVertexBuffers)&&ok;
    ok=hook(list.Get(),15,reinterpret_cast<void*>(bufferCopyHook),originalCopyBuffer)&&ok;
    initDiagnostics::mark("gamepadBridge::install");
    bool controllerHooks=gamepadBridge::install()>0;log("native_controller_hooks=%d",controllerHooks);ok=controllerHooks&&ok;
    initDiagnostics::mark("MH_EnableHook");
    if(!ok||MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) {MH_Uninitialize();log("hook installation failed");return 9;}
    chain.Reset(); DestroyWindow(window); UnregisterClassW(wc.lpszClassName,self);
    stereo::nativeUiGameplay=[](){return gamepadBridge::menuState()==0&&!pillInventory::open();};
    stereo::nativeUiReloadSuppressed=[](){static std::atomic<unsigned> n{};if(n.fetch_add(1)==0)log("native_ui exact_weapon_reloaded_suppressed=1 other_text_preserved=1");};
    stereo::hideNativeStatus=[](const float* bounds,unsigned width,unsigned height,unsigned count){
        // Loading can precede the first replacement status upload. Do not
        // flash the desktop HUD while the enabled VR HUD acquires its data.
        const bool gameplay=gamepadBridge::menuState()==0&&!pillInventory::open();
        if(!nativeMotion::enabled||!wristband::settings.enabled||wristband::failed)return false;
        const auto element=nativeHud::classify(bounds,width,height,count,gameplay);
        return element==nativeHud::Element::Health||element==nativeHud::Element::Ammo;
    };
    stereo::uiTrace=[](const stereo::ListState& s,UINT count,const float* b,const float* c){log("ui_sprite pipeline=%p count=%u bounds=%.4f,%.4f,%.4f,%.4f color=%.3f,%.3f,%.3f,%.3f",s.pipeline,count,b[0],b[1],b[2],b[3],c[0],c[1],c[2],c[3]);
        traceSpriteTexture(s);
    };
    stereo::uiFailureTrace=[](const stereo::ListState& s,const stereo::PipelineInfo& info,UINT count){
        unsigned char bytes[16]{};bool index=stereo::copyConstants(s.indices.BufferLocation,16,bytes);
        bool vertex=stereo::copyConstants(s.vertices[info.layout.position.slot].BufferLocation,16,bytes);
        log("ui_bounds_missing layout=%d count=%u index=%llx format=%u readable=%d vertex=%llx stride=%u readable=%d position=%u/%u/%u row=%u/%u/%u",info.layout.valid,count,s.indices.BufferLocation,s.indices.Format,index,s.vertices[info.layout.position.slot].BufferLocation,s.vertices[info.layout.position.slot].StrideInBytes,vertex,info.layout.position.slot,info.layout.position.offset,info.layout.position.format,info.layout.rows[0].slot,info.layout.rows[0].offset,info.layout.rows[0].format);
    };
    internal=false; log("hooks_ready=1"); return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI Initialize(void*) {
    if(!initDiagnostics::open())return ERROR_WRITE_FAULT;
    initDiagnostics::module(nullptr);initDiagnostics::module(self);initDiagnostics::modules();
    return initDiagnostics::boundary([]{return initializeBody();});
}
extern "C" __declspec(dllexport) DWORD WINAPI Recenter(void*) {stereo::recenterRequested=true;return 0;}
extern "C" __declspec(dllexport) DWORD WINAPI CalibratePosture(void* command){auto mode=reinterpret_cast<std::uintptr_t>(command);if(mode<1||mode>2)return 1;stereo::postureCalibration=static_cast<unsigned>(mode);return 0;}
extern "C" __declspec(dllexport) DWORD WINAPI CaptureEyes(void*) {stereo::captureRequested=true;desktopMirror::captureRequested=true;return 0;}
extern "C" __declspec(dllexport) DWORD WINAPI CaptureNextHuntingShot(void*) {selectedHuntingFlash::captureUntil=GetTickCount64()+10000;return 0;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {self=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}








extern "C" __declspec(dllexport) DWORD WINAPI TestInput(void* command) {auto code=static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(command));
    if(code==86){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;auto now=GetTickCount64();meleeReplayStarted=now;meleeReplayUntil=now+2500;log("melee_contact_sweep synthetic=1 duration_ms=2500 weapon=%d",sniperScope::visibleWeapon.load());return 0;}
    if(code==85){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;selectedPistolWell::diagnosticUntil=GetTickCount64()+15000;return 0;}
    if(code==83){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;magazineDrop::captureUntil=GetTickCount64()+15000;return 0;}
    if(code==84){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;chamberFeedCapture::until=GetTickCount64()+15000;return 0;}
    if(code==87){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;casingFlight::captureUntil=GetTickCount64()+15000;return 0;}
    if(code==88){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;ejectionSideViewUntil=GetTickCount64()+15000;return 0;}
    if(code==80){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;mountedLight::diagnosticUntil=GetTickCount64()+15000;return 0;}
    if(code==79){if(!stereo::xr||!stereo::xr->desktopDiagnostic||!handheldLight::held.load())return ERROR_NOT_SUPPORTED;handheldLight::captureContact=true;handheldLight::diagnosticUntil=GetTickCount64()+130;return 0;}
    if(code==78){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic||!pillBottle::installed||gamepadBridge::menuState()!=0)return ERROR_NOT_SUPPORTED;
        pillBottle::effectVerificationReport=[](unsigned before,unsigned after,unsigned restored,bool accepted){log("medicine_native_effect_check before_deficit=%u after_deficit=%u restored_deficit=%u accepted=%d inventory_untouched=1 save_untouched=1",before,after,restored,accepted);};
        pillBottle::verifyEffectRequested=true;return 0;
    }
    if((code>=75&&code<=77)||code==81||code==82){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;
        bodyHolsters::diagnosticMode=code>=81?code-77:code-74;bodyHolsters::diagnosticUntil=GetTickCount64()+3000;
        geometryIsolation::replayUntil=GetTickCount64()+120000;
        log("holster_diagnostic mode=%u duration_ms=3000",bodyHolsters::diagnosticMode.load());return 0;
    }
    if(code==72){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;
        const auto now=GetTickCount64();supportAim::diagnosticStarted=now;supportAim::diagnosticMode=4;supportAim::diagnosticUntil=now+10000;
        log("desktop_support_pump_cycle grip_held=1 duration_ms=10000 synthetic_tracking=1");return 0;
    }
    if(code==69){leftAlignmentRequested=true;return 0;}
    if(code==70){ammoPouchRequested=true;return 0;}
    if(code==73||code==74){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;
        opticScope::diagnosticUntil=code==73?GetTickCount64()+120000:0;return 0;
    }
    if((code>=60&&code<=68)||code==71){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic||!selectedReload::profile.ready)return ERROR_NOT_READY;
        if(code!=67&&!geometryIsolation::replay(GetTickCount64()))return ERROR_NOT_READY;
        selectedReload::diagnosticUntil=0;
        selectedReload::gestureMode=code==67?0:code==68?8:code==71?9:code-59;
        selectedReload::gestureUntil=code==67?0:GetTickCount64()+10000;
        {std::lock_guard<std::mutex> lock(manualReload::mutex);
         log("selected_reload_gesture mode=%u prior_stage=%u completed=%u synthetic_tracking=1",selectedReload::gestureMode.load(),unsigned(manualReload::gestures.transaction().stage()),manualReload::completed.load());
         if(manualReload::hands.weapon==0)log("selected_revolver_toggle prior_phase=%u profile=%u fire_calls=%u fire_blocked=%u fire_table_rva=%llx",unsigned(arsenal::reloads[arsenal::index(0)].state()),unsigned(selectedRevolver::profile.ready),arsenal::revolverFireCalls.load(),arsenal::revolverFireBlocked.load(),static_cast<unsigned long long>(arsenal::revolverFireTable.load()));}
        return 0;
    }
    if(code>=55&&code<=59){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic||!selectedReload::profile.ready)return ERROR_NOT_READY;
        selectedReload::diagnosticMode=code==59?0:code-54;
        selectedReload::diagnosticUntil=code==59?0:GetTickCount64()+30000;
        log("selected_reload_draw_inspection mode=%u duration_ms=30000 ammo_transaction_untouched=1",selectedReload::diagnosticMode.load());return 0;
    }
    if(code>=49&&code<=54){
        if(!stereo::xr||!pillInventory::open())return ERROR_NOT_READY;
        inventoryTestUntil=GetTickCount64()+30000;
        desktopInventoryAction=code;
        log("desktop_inventory_action=%u synthetic_input=1",code);return 0;
    }
    if(code==48){
        if(!stereo::xr||!stereo::haveWorldFrame)return ERROR_NOT_READY;
        const auto now=GetTickCount64();
        inventoryTestUntil=now+30000;
        desktopInventoryToggle=true;
        log("desktop_inventory_y_edge_requested synthetic_input=1");return 0;
    }
    if(code==45||code==46){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;stereo::xr->diagnosticView=code-41;log("desktop_stereo_synthetic_pickup_view=%u native_player_unchanged=1",stereo::xr->diagnosticView.load());return 0;}
    if(code==44){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;return gamepadBridge::queueTest(15);}
    if(code==43){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;return gamepadBridge::queueTest(14);}
    if(code>=39&&code<=42){
        if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;
        const auto now=GetTickCount64();supportAim::diagnosticStarted=now;supportAim::diagnosticMode=code==42?0:code-38;supportAim::diagnosticUntil=code==42?0:now+120000;
        log("desktop_support_grip_replay mode=%u duration_ms=120000 synthetic_input=1",supportAim::diagnosticMode.load());return 0;
    }
    if(code==37){pickupTransitions.arm(GetTickCount64(),45000);log("pickup_transition_capture enabled=1 duration_ms=45000 max_events=64 max_keys=1024");return 0;}
    if(code==38){const auto dump=pickupDumpSerial.fetch_add(1)+1;pickupTransitions.stop();auto snapshot=pickupTransitions.snapshot();log("pickup_transition_capture dump=%u enabled=0 records=%llu",dump,static_cast<unsigned long long>(snapshot.count));for(size_t i=0;i<snapshot.count;++i){const auto& e=snapshot.events[i];log("pickup_transition dump=%u record=%llu time=%llu thread=%u baseline=%d scene=%llx entity=%p id=%d visible=%d tested=%d native=%.9g effective=%.9g plane=%d margin=%.5f sector=%s revision=%u stamp=%u",dump,static_cast<unsigned long long>(i),e.time,e.thread,e.baseline,e.scene,reinterpret_cast<void*>(e.entity),e.id,e.visible,e.tested,e.nativeResult,e.effectiveResult,e.plane,e.margin,nativePickupSector::name(e.sector.status),e.sector.revision,e.sector.stamp);if(e.sphereKnown)log("pickup_transition_tested_sphere dump=%u record=%llu time=%llu entity=%p center=%.9g,%.9g,%.9g radius=%.9g",dump,static_cast<unsigned long long>(i),e.time,reinterpret_cast<void*>(e.entity),e.sphere[0],e.sphere[1],e.sphere[2],e.sphere[3]);if(e.planesKnown)for(unsigned p=0;p<6;++p)log("pickup_transition_tested_plane dump=%u record=%llu time=%llu entity=%p index=%u values=%.9g,%.9g,%.9g,%.9g",dump,static_cast<unsigned long long>(i),e.time,reinterpret_cast<void*>(e.entity),p,e.testedPlanes[p*4],e.testedPlanes[p*4+1],e.testedPlanes[p*4+2],e.testedPlanes[p*4+3]);}return 0;}
    if(code==35||code==36){zeroConstantRedirects=0;zeroConstantGuardUntil=code==35?GetTickCount64()+120000:0;log("native_zero_constant_guard enabled=%d duration_ms=120000",code==35);return 0;}
    if(code==34){auto current=sceneEvidenceRequest.load();std::uint64_t next{};do{auto ticket=static_cast<unsigned>(current>>32)+1;if(!ticket)ticket=1;next=(std::uint64_t(ticket)<<32)|presents.load();}while(!sceneEvidenceRequest.compare_exchange_weak(current,next));log("scene_evidence_requested ticket=%u frame=%u max_draws_per_thread=12 source_frames=1",static_cast<unsigned>(next>>32),static_cast<unsigned>(next));return 0;}
    if(code==33){diagnosticSectorCalls=0;geometryIsolation::set(geometryIsolation::NoSectorVisibility,GetTickCount64());log("geometry_diagnostic all_loaded_sectors duration_ms=45000");return 0;}
    if(code==32){diagnosticSphereRejections=0;geometryIsolation::set(geometryIsolation::NoFrustum,GetTickCount64());log("geometry_diagnostic no_frustum duration_ms=45000");return 0;}
    if(code>=28&&code<=31){if(!stereo::xr||!stereo::xr->desktopDiagnostic)return ERROR_NOT_SUPPORTED;stereo::xr->diagnosticView=code==31?0:code-27;log("desktop_stereo_view=%u",stereo::xr->diagnosticView.load());return 0;}
    if(code==26||code==27){headLightConstants::enabled=code==26;log("headlamp_diagnostic enabled=%d",code==26);return 0;}
    if(code==47){auto now=GetTickCount64();geometryIsolation::set(geometryIsolation::Normal,now);nativeFog::diagnosticUntil=now+45000;log("geometry_diagnostic native_fog_on duration_ms=45000 saved_enabled=%d",int(nativeFog::enabled.load()));return 0;}
    if(code==25){nativeFog::diagnosticUntil=0;geometryIsolation::set(geometryIsolation::NoFog,GetTickCount64());log("geometry_diagnostic no_fog duration_ms=45000");return 0;}
    if(code==23||code==24){geometryIsolation::replayUntil=code==23?GetTickCount64()+120000:0;log("geometry_diagnostic synthetic_controller_poses=%d duration_ms=120000",code==23);return 0;}
    if(code>=18&&code<=22){auto mode=code==22?geometryIsolation::Normal:static_cast<geometryIsolation::Mode>(code-17);geometryIsolation::set(mode,GetTickCount64());log("geometry_diagnostic mode=%u duration_ms=45000",mode);return 0;}
    if(code==16){headLamp::testToggleRequested=true;return 0;}
    if(code==17){nativeHandClip::diagnosticUntil=GetTickCount64()+5000;log("native_hand_clip_diagnostic_ms=5000");return 0;}
    if(code==14||code==15){headLamp::diagnosticBypass=code==14;log("headlamp_diagnostic_native=%d",code==14);return 0;}
    return gamepadBridge::queueTest(code);}













