#pragma once
#include "support_aim.hpp"
#include "support_transition.hpp"
#include "selected_support_restore.hpp"
#include "wristband_mesh.hpp"
#include "forearm_mesh.hpp"
#include "wristband_texture.hpp"
#include <wincodec.h>
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")
#include "arsenal_props.hpp"
#include "weapon_grip_hands.hpp"
#include "hand_grip_state.hpp"
#include "replacement_hands.hpp"
#include "hunting_rifle.hpp"
#include "native_hand_clip.hpp"
#include "hand_fitting.hpp"
#include "selected_wrist_fade.hpp"
#include "native_wrist_pose.hpp"
#include "geometry_isolation.hpp"
#include "pill_bottle.hpp"
#include "body_holsters.hpp"
#include "holster_motion.hpp"
#include "psx_asset_pack.hpp"
#include "saved_wrist.hpp"
#include "selected_frames.hpp"
#include "left_melee_wrist.hpp"
#include "saw_motion.hpp"
#include "custom_saw_profile.hpp"
#include "selected_reload_profile.hpp"
#include "selected_pistol_well.hpp"
#include "selected_pistol_feed_channel.hpp"
#include "selected_pistol_passage.hpp"
#include "selected_rifle_profile.hpp"
#include "selected_shotgun_contact.hpp"
#include "selected_shotgun_props.hpp"
#include "selected_hand_ownership.hpp"
#include "selected_holding_hand.hpp"
#include "hand_curl_transition.hpp"
#include "mechanism_return.hpp"
#include "chamber_feed_capture.hpp"
#include "cylinder_spin.hpp"
#include "selected_hunting_contact.hpp"
#include "pistol_rounds.hpp"
#include "rifle_rounds.hpp"
#include "hunting_rounds.hpp"
#include "selected_hunting_flash.hpp"
#include "magazine_drop.hpp"
#include "reload_replay.hpp"
#include "selected_revolver_profile.hpp"
#include "selected_speedloader.hpp"
#include "selected_cases.hpp"
#include "selected_surface_maps.hpp"
#include "selected_flashlight.hpp"
#include "handheld_light.hpp"
#include "mounted_light.hpp"
#include "selected_hunting_texture.hpp"
#include "selected_wrist_finish.hpp"
#include "optic_scope.hpp"
namespace wristband {
// Published once per VR frame for replacement-rig rendering. This selector is
// observational: ammo transfer and combat remain owned by their native adapters.
inline handGrip::Selection replacementGrip;
using Microsoft::WRL::ComPtr;using namespace DirectX;
inline constexpr char shader[]=R"(
Texture2D Albedo:register(t0);Texture2D Skin:register(t1);Texture2D NormalMap:register(t2);Texture2D SurfaceMap:register(t3);SamplerState LinearWrap:register(s0);
cbuffer Transform:register(b0){row_major float4x4 mvp;float4 tint;row_major float4x4 modelView;}
struct V{float3 p:POSITION;float3 n:NORMAL;float3 c:COLOR;float2 uv:TEXCOORD0;float printed:TEXCOORD1;float3 nextP:POSITION1;float3 nextN:NORMAL1;};
struct O{float4 p:SV_POSITION;float3 n:NORMAL;float3 c:COLOR;float2 uv:TEXCOORD0;float printed:TEXCOORD1;float3 local:TEXCOORD2;float3 viewP:TEXCOORD3;float3 viewN:TEXCOORD4;float wristCoverage:TEXCOORD5;};
O vs(V v){O o;o.local=lerp(v.p,v.nextP,tint.w);o.p=mul(float4(o.local,1),mvp);o.n=normalize(lerp(v.n,v.nextN,tint.w));o.viewP=mul(float4(o.local,1),modelView).xyz;o.viewN=mul(float4(o.n,0),modelView).xyz;o.c=v.c;o.uv=v.uv;bool wrist=v.printed>=32&&v.printed<41;o.printed=wrist?floor(v.printed)-32:v.printed;o.wristCoverage=wrist?frac(v.printed)*4:1;return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float4 ps(O v):SV_TARGET{
 // Object-space coverage is identical for both eyes and does not advance
 // with time. Discarded fragments write no depth; no transparent cap remains.
 if(v.wristCoverage<.999){clip(v.wristCoverage-.0001);float3 cell=floor(v.local*3000);clip(v.wristCoverage-hash(cell.xy+cell.z*float2(37,19)));}
 float3 n=normalize(v.n);float shade=.68+.32*abs(dot(n,normalize(float3(.3,.8,.6))));
 float3 c=v.c;float3 sourceBase=1;
 float4 material=SurfaceMap.SampleLevel(LinearWrap,v.uv,0);
 bool mappedSurface=(v.printed>6.5||(v.printed>4.5&&v.printed<5.5))&&material.a>.5;
 if(mappedSurface){
  n=normalize(v.viewN);
  float3 dx=ddx(v.viewP),dy=ddy(v.viewP);float2 du=ddx(v.uv),dv=ddy(v.uv);float det=du.x*dv.y-du.y*dv.x;
  if(abs(det)>1e-12){
   float3 tangent=(dx*dv.y-dy*du.y)/det;tangent-=n*dot(n,tangent);
   float3 bitangent=(-dx*dv.x+dy*du.x)/det;
   if(dot(tangent,tangent)>1e-12&&dot(bitangent,bitangent)>1e-12){
    tangent=normalize(tangent);float handed=dot(cross(n,tangent),bitangent)<0?-1:1;
    bitangent=cross(n,tangent)*handed;float3 mapped=NormalMap.SampleLevel(LinearWrap,v.uv,0).rgb*2-1;
    n=normalize(tangent*mapped.x+bitangent*mapped.y+n*mapped.z);
   }
  }
  shade=(.68+.32*abs(dot(n,normalize(float3(.3,.8,.6)))))*material.r;
 }
   if(v.printed>7.5){c=v.c;}
   else if(v.printed>6.5){sourceBase=Skin.SampleLevel(LinearWrap,v.uv,0).rgb;c=sourceBase*v.c;}
   else if(v.printed>5.5){uint w,h;Skin.GetDimensions(w,h);float2 size=float2(w,h);return float4(Skin.SampleLevel(LinearWrap,(floor(v.uv*size)+.5)/size,0).rgb,1);}
   if(v.printed>=0&&v.printed<.5)c=Albedo.Sample(LinearWrap,v.uv).rgb;
 else if(v.printed>4.5&&v.printed<6.5){sourceBase=Skin.Sample(LinearWrap,v.uv).rgb;c=sourceBase*v.c;}
 else if(v.printed<0){float noise=hash(floor(v.uv*float2(900,400)));c*=.93+.07*noise;}
 // Authored unlit surfaces use a distinct binding marker; unbound atlas pixels
 // retain the existing hand and prop shading.
 if((v.printed>6.5||(v.printed>4.5&&v.printed<5.5))&&material.a>.1&&material.a<.4)return float4(c,1);
 float spec=pow(saturate(abs(dot(n,normalize(float3(.25,.9,.4))))),32)*.022;
 if(mappedSurface){
  // glTF base color is sRGB; metallic/roughness and normal maps are linear.
  // Decode the image before applying glTF's linear baseColorFactor. Applying
  // that factor to encoded texels incorrectly darkens non-white materials.
  float3 base=lerp(sourceBase/12.92,pow((sourceBase+.055)/1.055,2.4),step(.04045,sourceBase))*v.c;
  if(v.printed>7.5)base=c; // glTF constant baseColorFactor is already linear.
  float variance=max(dot(ddx(n),ddx(n)),dot(ddy(n),ddy(n)));
  float rough=sqrt(saturate(material.g*material.g+min(.18,variance*.25)));rough=max(.045,rough);float metal=saturate(material.b);
  float3 V=normalize(-v.viewP);if(dot(n,V)<0)n=-n;
  float3 L=normalize(float3(-.35,.65,1)),H=normalize(V+L);
  float nv=max(.001,dot(n,V)),nl=saturate(dot(n,L)),nh=saturate(dot(n,H)),vh=saturate(dot(V,H));
  float a=rough*rough,a2=a*a,den=nh*nh*(a2-1)+1;
  float D=a2/max(3.14159265*den*den,1e-7);
  float k=(rough+1)*(rough+1)/8;
  float G=(nv/(nv*(1-k)+k))*(nl/max(nl*(1-k)+k,1e-5));
  float specularFactor=NormalMap.SampleLevel(LinearWrap,v.uv,0).a;
  float3 f0=lerp(float3(.04,.04,.04)*specularFactor,base,metal);
  float3 F=f0+(1-f0)*pow(1-vh,5);
  float3 diffuse=(1-F)*(1-metal)*base/3.14159265;
  float3 direct=(diffuse+D*G*F/max(4*nv*nl,1e-5))*nl*2.2;
  // Neutral hemispherical fallback illumination. Uniform specular fill made
  // polished source metals look like flat gray paint outside the point light.
  // Blur its directional variation with roughness and retain grazing Fresnel.
  // This is not a captured reflection of the room.
  float3 reflected=reflect(-V,n);
  // Convolve two broad area-light lobes with the authored roughness.
  // Preserve their energy as they broaden instead of blending every direction
  // toward a constant gray reflection (which erased dark-metal contrast).
  float spread=rough*rough;
  float keyPower=18/(1+18*spread),fillPower=7/(1+7*spread);
  float environment=.012+.018*saturate(reflected.y*.5+.5);
  environment+=1.4*((keyPower+1)/19)*pow(saturate(dot(reflected,normalize(float3(-.6,.8,.4)))),keyPower);
  environment+=.38*((fillPower+1)/8)*pow(saturate(dot(reflected,normalize(float3(.85,.3,-.4)))),fillPower);
  float3 environmentF=f0+(max(1-rough,f0)-f0)*pow(1-nv,5);
  float diffuseFill=.14+.12*saturate(n.y*.5+.5);
  float3 ambient=((1-environmentF)*base*(1-metal)*diffuseFill+environmentF*environment)*material.r;
  // Retained source clearcoats use roughness .25; surface alpha encodes amount.
  float coat=saturate((1-material.a)*255/127);
  float coatF=.04+.96*pow(1-vh,5),coatA2=.00390625;
  float coatDen=nh*nh*(coatA2-1)+1;
  float coatD=coatA2/max(3.14159265*coatDen*coatDen,1e-7);
  // The coat is a separate dielectric layer, not the underlying metal/rubber
  // lobe. Its visibility therefore uses its own authored roughness (.25).
  float coatK=1.25*1.25/8;
  float coatG=(nv/(nv*(1-coatK)+coatK))*(nl/max(nl*(1-coatK)+coatK,1e-5));
  direct=direct*(1-coat*coatF)+coat*coatD*coatG*coatF/max(4*nv,1e-5)*2.2;
  float coatViewF=.04+.96*pow(1-nv,5);
  ambient*=1-coat*coatViewF;
  float3 radiance=max(0,direct+ambient);
  float3 encoded=lerp(radiance*12.92,1.055*pow(radiance,1/2.4)-.055,step(.0031308,radiance));
  float opacity=1;
  if(v.printed>7.05&&v.printed<7.4){
   opacity=Skin.SampleLevel(LinearWrap,v.uv,0).a;
   if(v.printed>7.2)opacity=lerp(.04+.96*pow(1-nv,5),1,opacity);
  }
  return float4(saturate(encoded),opacity);
 }
 return float4(c*shade+spec,1);
})";
inline ComPtr<ID3D12RootSignature> root;inline ComPtr<ID3D12PipelineState> pipeline,glassPipeline;
inline ComPtr<ID3D12Resource> vertices,dynamicVertices;inline D3D12_VERTEX_BUFFER_VIEW vb{},dynamicVB{};
inline ComPtr<ID3D12Resource> albedo,albedoUpload;inline ComPtr<ID3D12DescriptorHeap> albedoHeap;
inline ComPtr<ID3D12Resource> skinTexture,skinUpload;
inline unsigned count{},samples{};inline ID3D12Device* owner{};inline bool failed{},uploaded{};
inline std::vector<Vertex> dynamicMesh;inline Snapshot displayed;inline bool haveDisplay{},prepared{},visible{};
inline XrPosef framePose;inline unsigned lastWrist=2;
inline XrPosef forearmPose[2];inline bool forearmVisible[2]{};inline unsigned forearmFirst[2]{},forearmCount[2]{};
inline unsigned bottleFirst{};
inline unsigned huntingFirst{},flashFirst{},boltGripStep{};inline bool boltGripActive{},bottleVisible{};
struct GripTransition {unsigned from{},to{};std::uint64_t started{};bool set{};float mix{};};
inline GripTransition gripTransition[2];
inline unsigned replacementFirst{};inline bool replacementVisible[2]{};inline XrPosef replacementPose[2];
inline XMMATRIX gripMatrix[3];inline bool gripVisible[3]{};inline unsigned gripFirst[3]{},gripCount[3]{};
// The selected pack has its own draw list and material bank. Its caller must
// explicitly provide editor-to-world transforms; loading never guesses them.
inline psxAssets::Pack selectedPack;
struct SelectedDraw { unsigned section; XMMATRIX editorToWorld; unsigned morph=~0u;float mix{}; };
inline handCurl::Transition holdingCurl,flashlightCurl;
inline mechanismReturn::Spring pistolReturn,rifleReturn;
inline mechanismReturn::LockedBolt rifleBoltReturn;
inline cylinderSpin::Closing closingCylinderSpin;
inline std::vector<SelectedDraw> selectedDraws;
inline unsigned selectedFirst{};
inline bool selectedMode{};
inline bool customSawEnabled{};
inline unsigned selectedRotorSection=~0u;
inline std::array<float,3> selectedRotorPivot{},selectedRotorAxis{};
inline std::atomic<int> selectedRenderedWeapon{-1};
inline std::atomic<ULONGLONG> selectedRenderedAt{};
inline int selectedFrameWeapon=-1;
inline unsigned selectedWristFirst{};
inline bool selectedWristVisible{};
inline XMMATRIX selectedWristWorld=XMMatrixIdentity();
inline bool loadSelected(const std::filesystem::path& folder){
    if(pipeline)return false; // GPU resources must not retain a different atlas.
    psxAssets::Pack candidate;if(!psxAssets::load(folder,candidate)||!savedWrist::load(folder)||!selectedFrames::load(folder))return false;
    // Saved geometry already contains the user's size/placement. Live values
    // start in the original band frame and receive that same saved transform.
    settings.radiusX=fittedRadiusX;settings.radiusY=fittedRadiusY;settings.width=.028f;
    selectedRotorSection=candidate.version==3?candidate.rotorSection:
        (candidate.version==2?customSawProfile::rotorSection:~0u);
    customSawEnabled=selectedRotorSection!=~0u;
    for(unsigned i=0;i<3;i++){
        selectedRotorPivot[i]=candidate.version==3?candidate.rotorPivot[i]:customSawProfile::pivot[i];
        selectedRotorAxis[i]=candidate.version==3?candidate.rotorAxis[i]:customSawProfile::axis[i];
    }
    if(customSawEnabled){
        if(selectedRotorSection>=candidate.sections.size())return false;
        const auto& rotor=candidate.sections[selectedRotorSection];
        if(rotor.weapon!=5||rotor.kind!=psxAssets::Weapon)return false;
    }
    // Optional isolated-test profile is bound to exact geometry/frame bytes.
    // Missing/rejected profiles leave the established rest-pose path intact.
    if(selectedReload::loadProfile(folder,candidate)){
        std::lock_guard<std::mutex> lock(manualReload::mutex);
        const auto& p=selectedReload::profile;
        if(!manualReload::gestures.setReplacementGeometry(p.well,p.handle,p.geometry.pullAxisGrip,p.geometry.heldContactGrip))selectedReload::profile={};
    }
    selectedRevolver::load(folder,candidate);
    selectedRevolver::alignBarrel(candidate);
    if(selectedRevolver::profile.ready){
        auto hinge=XMLoadFloat4x4(&selectedRevolver::profile.parts[0].open);hinge.r[3]=XMVectorSet(0,0,0,1);
        auto q=XMQuaternionNormalize(XMQuaternionRotationMatrix(hinge));if(XMVectorGetW(q)<0)q=-q;
        auto axis=XMVector3Normalize(XMVectorSetW(q,0));
        XMFLOAT3 closing;XMStoreFloat3(&closing,-XMVector3Normalize(XMVector3TransformNormal(axis,XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim))));
        std::lock_guard<std::mutex> lock(manualReload::mutex);arsenal::cylinderClosingAxis={closing.x,closing.y,closing.z};
    }
    selectedRifle::load(folder,candidate);
    selectedRifle::fitMagazine(folder,candidate);
    selectedShotgun::loadContact(folder);
    arsenalProps::sourceShellReady=selectedShotgun::loadProps(folder,candidate);
    selectedShotgun::loadMotion(folder,candidate);
    if(!selectedShotgun::PumpCursor::validCycle(selectedShotgun::motion))selectedShotgun::motion={};
    {std::lock_guard<std::mutex> lock(manualReload::mutex);arsenal::replacementShotgunContact=selectedShotgun::contact.insertion;arsenal::replacementShotgunHandle=selectedShotgun::contact.pump;}
    {std::lock_guard<std::mutex> lock(manualReload::mutex);arsenal::replacementRifleContact={};
        if(selectedRifle::profile.ready){const auto& p=selectedRifle::profile;
            arsenal::replacementRifleContact.set(p.well,p.geometry.heldContactGrip);arsenal::replacementRifleHandle=p.handle;arsenal::replacementRiflePullAxis=p.geometry.pullAxisGrip;}}
    {std::lock_guard<std::mutex> lock(manualReload::mutex);arsenal::replacementRevolverContact=selectedRevolver::profile.contact;}
    {std::lock_guard<std::mutex> lock(manualReload::mutex);arsenal::replacementHuntingContact=selectedHunting::load(folder,candidate);}
    selectedHoldingHand::load(folder,candidate);
    selectedSupportRestore::load(folder,candidate);
    arsenalProps::sourceLoaderReady=selectedSpeedloader::load(folder,candidate);
    pistolRounds::load(folder);pistolRounds::append(candidate);
    rifleRounds::append(candidate);
    huntingRounds::append(folder,candidate);
    selectedCases::load(folder,candidate);
    selectedSurface::load(folder,candidate);
    if(selectedSurface::bound)selectedHuntingTexture::apply(folder,candidate);
    if(selectedSurface::bound)for(const auto& s:candidate.sections)if(s.weapon==5&&s.kind==psxAssets::Weapon)
        for(unsigned i=0;i<s.count;++i){auto& v=candidate.vertices[s.first+i];if(v.value[11]==4){v.value[9]=4090.5f/8192;v.value[10]=8190.5f/8192;v.value[11]=8;}}
    leftMeleeWrist::load(candidate);
    selectedMeleeContact::load(candidate);
    selectedWristFinish::apply(candidate);
    selectedFlashlight::load(folder,candidate);
    mountedLight::append(candidate);
    selectedPistolWell::load(folder,candidate);
    selectedPistolFeedChannel::load(folder,candidate);
    magazineExit::bind(candidate,1,selectedReload::profile.sections[0]);
    magazineExit::bind(candidate,2,selectedRifle::profile.sections[0]);
    magazineExit::bind(candidate,4,selectedHunting::section);
    selectedPistolPassage::load(folder,candidate);
    for(unsigned w=1;w<5;++w)arsenalProps::texturedCases[w]=selectedCases::sections[w]!=~0u;
    // Native held-weapon rendering retains the game's original flash effect.
    // Do not append an additional untextured replacement flash.
    selectedPack=std::move(candidate);selectedMode=true;return true;
}
inline void prepareSelectedStatus(const Snapshot& status,const XMMATRIX& previewToWorld,bool show){
    selectedWristVisible=show&&savedWrist::ready&&status.world;
    selectedWristWorld=previewToWorld;
    if(!selectedWristVisible)return;
    if(!haveDisplay||!same(status,displayed)){
        displayed=status;dynamicMesh=statusMesh(status);savedWrist::transformStatus(dynamicMesh);
        haveDisplay=true;uploaded=false;
    }
}
inline bool replaceSelectedNative(unsigned weapon){
    // loadSelected validates a weapon and right-hand section for every slot.
    // The native render hook supplies the VR gate. Do not resurrect native
    // meshes when a door fade temporarily stops selected-frame submission.
    return selectedMode&&weapon<7;
}
inline void(*selectedVisibilityObserved)(int,bool,bool){};
inline void(*selectedLoadoutDrawn)(){};
inline bool selectedFrameComplete{};
inline void prepareSelectedFrame(){
    selectedFrameComplete=false;selectedDraws.clear();selectedFrameWeapon=-1;selectedWristVisible=false;bottleVisible=false;
    manualReload::Hands h;manualReload::Visual reloadVisual,rifleVisual,huntingVisual;bool revolverOpen=false;XrVector3f pouchAnchor{};
    arsenal::Phase riflePhase=arsenal::Phase::Ready;float shotgunSeconds=0,cylinderAmount=0;bool reloadOwnsLeft=false,lampReloadBusy=false;std::uint64_t lastShot=0;arsenal::LoaderVisual loader;
    {std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;
        pouchAnchor=manualReload::ammoPouch;
        loader=arsenal::loader;
        if(h.weapon>=0&&h.weapon<5)lastShot=weaponShots::observers[h.weapon].fired;
        reloadVisual=manualReload::gestures.visual();reloadVisual.stage=manualReload::gestures.transaction().stage();
        reloadVisual.valid=reloadVisual.valid&&manualReload::ownsLocked()&&h.pistol&&h.gameplay;
        reloadOwnsLeft=reloadVisual.valid&&reloadVisual.stage!=manualReload::Stage::Ready;
        lampReloadBusy=reloadVisual.valid&&manualReload::usesReloadHand(reloadVisual.stage);
        const int activeReload=arsenal::index(h.weapon);
        if(activeReload>=0&&activeReload<4&&arsenal::enabled&&arsenal::ownedLocked(activeReload)&&h.gameplay)
        {
            reloadOwnsLeft=reloadOwnsLeft||!arsenal::allowsSupport(static_cast<arsenal::Kind>(h.weapon),arsenal::reloads[activeReload].state());
            lampReloadBusy=lampReloadBusy||arsenal::usesReloadHand(arsenal::reloads[activeReload].state());
        }
        const int rifle=arsenal::index(2);
        if(rifle>=0){rifleVisual=arsenal::visuals[rifle];riflePhase=arsenal::reloads[rifle].state();
            rifleVisual.valid=rifleVisual.valid&&h.weapon==2&&arsenal::enabled&&arsenal::ownedLocked(rifle)&&h.gameplay;}
        const int si=arsenal::index(3);
        if(h.weapon==4&&arsenal::enabled&&arsenal::ownedLocked(3)){
            huntingVisual=arsenal::visuals[3];auto phase=arsenal::reloads[3].state();
            huntingVisual.stage=phase==arsenal::Phase::Open?manualReload::Stage::EmptyWell:phase==arsenal::Phase::Held?manualReload::Stage::MagazineHeld:manualReload::Stage::Ready;
        }
        if(h.weapon==3&&si>=0&&arsenal::enabled&&arsenal::ownedLocked(si)&&arsenal::visuals[si].valid)shotgunSeconds=arsenal::shotgunCursor.time();
        const int ri=arsenal::index(0);
        cylinderAmount=arsenal::cylinderTransition.value();
        revolverOpen=h.weapon==0&&ri>=0&&arsenal::enabled&&arsenal::ownedLocked(ri)&&arsenal::reloads[ri].state()!=arsenal::Phase::Ready;}
    // Keep the free hands visible through the same last-valid tracking continuity
    // used by the other hand renderer; interactions still require fresh poses.
    h.leftPoseTracked=h.leftPoseTracked||h.leftDisplayAvailable;
    h.rightPoseTracked=h.rightPoseTracked||h.rightDisplayAvailable;
    // Rendering follows the same valid-pose continuity as the free hands.
    // The local copy must not withhold the whole loadout while the world
    // origin settles; interaction validity in manualReload::hands is unchanged.
    h.gunTracked=h.gunTracked||(h.primaryHand==0?h.leftDisplayAvailable:h.rightDisplayAvailable);
    const auto now=GetTickCount64();const auto status=model.read(now);
    if(!h.session||!h.gameplay||!status.world||now<h.time||now-h.time>=100){handheldLight::reloadSuppressed=false;handheldLight::publish({},now,false);sawMotion::blade.tick(now,false);supportTransition::left.reset();holsterMotion::reset();holdingCurl.reset();flashlightCurl.reset();selectedRenderedWeapon=-1;return;}
    const bool leftPrimary=bodyHolsters::heldHand.load()==0&&(h.weapon==5||h.weapon==6)&&!bodyHolsters::hidden.load();
    const bool right=h.gunTracked&&(leftPrimary?h.leftPoseTracked:h.rightPoseTracked)&&motionTracking::nearHead(h.gun,h.head)&&!bodyHolsters::hidden.load();
    const bool left=h.leftPoseTracked&&motionTracking::nearHead(h.left,h.head);
    const bool validWeapon=h.weapon>=0&&h.weapon<7;
    if(selectedVisibilityObserved)selectedVisibilityObserved(h.weapon,right,left);
    selectedFrameComplete=left&&h.rightPoseTracked&&(right||bodyHolsters::hidden.load()||!validWeapon)&&weaponGroups::nativeEquipmentValid&&h.weapon==weaponGroups::nativeEquipped&&(!validWeapon||h.weapon>4||(status.ammoValid&&status.weapon==h.weapon));
    float pistolPull=pistolReturn.update(reloadVisual.slidePull,reloadVisual.stage==manualReload::Stage::SlideHeld,now,right&&h.weapon==1&&reloadVisual.valid);
    const float cylinderSpinAngle=closingCylinderSpin.update(cylinderAmount,now,right&&h.weapon==0);
    float riflePull=rifleReturn.update(rifleVisual.slidePull,riflePhase==arsenal::Phase::Charging,now,right&&h.weapon==2&&rifleVisual.valid);
    const float rifleBoltPull=rifleBoltReturn.update(rifleVisual.slidePull,riflePhase==arsenal::Phase::Charging,
        status.ammoValid&&status.weapon==2&&status.loaded==0,now,right&&h.weapon==2&&rifleVisual.valid);
    if(selectedReload::profile.ready&&h.weapon==1&&right&&left&&now<selectedReload::diagnosticUntil.load()){
        const auto mode=selectedReload::diagnosticMode.load();
        reloadVisual.valid=true;reloadVisual.leftInGun=motionTracking::relativePose(h.gun,h.left);
        reloadVisual.stage=mode==1?manualReload::Stage::EmptyWell:mode==2?manualReload::Stage::MagazineHeld:
            mode==3?manualReload::Stage::Inserted:manualReload::Stage::SlideHeld;
        reloadVisual.slidePull=mode==4?.05f:0.f;
    }
    if(customSawEnabled)sawMotion::blade.tick(now,right&&h.weapon==5&&std::isfinite(h.trigger)&&h.trigger>.15f);
    // Cosmetic kick applies to the complete fitted weapon/right-hand assembly
    // and attached support hand. Published controller poses/aim stay untouched.
    const auto kick=weaponShots::kick(unsigned(h.weapon),lastShot,now,right&&(!reloadOwnsLeft||h.weapon==3));
    if(kick.pitch||kick.back){auto visualPose=XMMatrixRotationX(kick.pitch)*XMMatrixTranslation(0,0,kick.back)*selectedFrames::pose(h.gun);
        XMFLOAT4 q;XMFLOAT3 p;XMStoreFloat4(&q,XMQuaternionRotationMatrix(visualPose));XMStoreFloat3(&p,visualPose.r[3]);
        h.gun.orientation={q.x,q.y,q.z,q.w};h.gun.position={p.x,p.y,p.z};}
    // The ocular must use the very same display pose as the rifle body,
    // including cosmetic recoil. Its scene camera still follows the aim ray.
    if(right&&h.weapon==4)opticScope::alignDisplay(h.gun);
    XMMATRIX weaponWorld=XMMatrixIdentity();
    if(right&&h.weapon==6){
        arsenal::Segment visible;selectedMeleeContact::segment(1,h.gun,visible.base,visible.tip);
        const auto stop=meleeImpactStop::correction(visible,now);
        h.gun.position.x+=stop.x;h.gun.position.y+=stop.y;h.gun.position.z+=stop.z;
    }
    if(right&&h.weapon==1&&now<selectedPistolWell::diagnosticUntil.load()){
        XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(XMMatrixRotationX(-1.25f)*motionTracking::rotation(h.gun.orientation)));h.gun.orientation={q.x,q.y,q.z,q.w};}
    if(right&&mountedLight::eligible(h.weapon)&&now<mountedLight::diagnosticUntil.load()){
        XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(XMMatrixRotationY(-.75f)*motionTracking::rotation(h.gun.orientation)));h.gun.orientation={q.x,q.y,q.z,q.w};}
    if(right&&validWeapon){
        if((h.weapon==2||h.weapon==4)&&selectedHuntingFlash::visible(lastShot,now)&&now<selectedHuntingFlash::captureUntil.load()&&selectedHuntingFlash::captureUntil.exchange(0)&&selectedHuntingFlash::capture)selectedHuntingFlash::capture();
        if(h.weapon<2){
            auto pose=holsterMotion::hip[h.weapon].update(selectedFrames::pose(h.gun),true,now);
            XMFLOAT4 q;XMFLOAT3 p;XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(pose)));XMStoreFloat3(&p,pose.r[3]);
            h.gun.orientation={q.x,q.y,q.z,q.w};h.gun.position={p.x,p.y,p.z};
        }
        selectedFrameWeapon=h.weapon;weaponWorld=selectedFrames::weapon(h.weapon,h.gun);
        for(unsigned i=0;i<selectedPack.sections.size();i++){const auto& s=selectedPack.sections[i];
            if(pistolRounds::contains(i)||rifleRounds::contains(i)||huntingRounds::contains(i)||i==selectedSpeedloader::section||selectedCases::contains(i))continue;
            if(rifleRounds::first!=~0u&&i==selectedRifle::profile.sections[1])continue;
            if(s.weapon==unsigned(h.weapon)&&s.kind!=psxAssets::LeftSupport){
                if(h.weapon==0&&!selectedRevolver::visible(i,status.ammoValid&&status.weapon==0,status.loaded))continue;
                if(h.weapon==3&&!selectedShotgun::propVisible(i))continue;
                if(h.weapon==2&&selectedRifle::profile.ready&&i==selectedRifle::profile.sections[1]&&
                    !selectedRifle::roundsVisible(riflePhase,status.ammoValid&&status.weapon==2,status.loaded,status.reserve))continue;
                auto partWorld=weaponWorld;
                // Mirror only the fitted gripping hand in controller space.
                // The weapon itself retains its authored handedness and scale.
                if(leftPrimary&&s.kind==psxAssets::RightHand)
                    partWorld=XMLoadFloat4x4(&leftMeleeWrist::handToAim[h.weapon-5])*selectedFrames::pose(h.gun);
                if(h.weapon==4&&i==selectedHunting::section&&!selectedReload::transform(selectedReload::Role::Magazine,selectedHunting::geometry,huntingVisual,
                    XMLoadFloat4x4(&selectedFrames::frames[4].editorToAim),h.gun,partWorld))continue;
                if(h.weapon==3&&selectedShotgun::motion.ready)partWorld=selectedShotgun::sample(selectedShotgun::motion,i,shotgunSeconds)*weaponWorld;
                if(h.weapon==2&&selectedRifle::profile.ready){
                    auto v=rifleVisual;auto phase=riflePhase;
                    if(selectedRifle::profile.role(i)==selectedReload::Role::Slide&&riflePull>0){v.slidePull=riflePull;phase=arsenal::Phase::Charging;}
                    if(i==selectedRifle::boltSection){v.valid=true;v.slidePull=selectedRifle::boltPull(rifleBoltPull,status.ammoValid&&status.weapon==2,status.loaded,lastShot,now);phase=arsenal::Phase::Charging;}
                    if(!selectedRifle::transform(i,phase,v,XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim),h.gun,partWorld))continue;
                }
                if(h.weapon==0)partWorld=selectedRevolver::spinTransform(i,cylinderSpinAngle)*selectedRevolver::animatedTransform(i,cylinderAmount,weaponWorld);
                if(h.weapon==1&&selectedReload::profile.ready){
                    const auto& p=selectedReload::profile;
                    auto v=reloadVisual;if(p.role(i)==selectedReload::Role::Slide&&pistolPull>0){v.stage=manualReload::Stage::SlideHeld;v.slidePull=pistolPull;}
                    if(!selectedReload::transform(p.role(i),p.geometry,v,
                        XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim),h.gun,partWorld))continue;
                }
                if(customSawEnabled&&i==selectedRotorSection){
                    const auto* p=selectedRotorPivot.data();const auto* a=selectedRotorAxis.data();
                    partWorld=XMMatrixTranslation(-p[0],-p[1],-p[2])*
                        XMMatrixRotationAxis(XMVectorSet(a[0],a[1],a[2],0),static_cast<float>(sawMotion::blade.angle))*
                        XMMatrixTranslation(p[0],p[1],p[2])*weaponWorld;
                }
                selectedDraws.push_back({i,partWorld});
            }}
        if(h.weapon==1&&pistolRounds::first!=~0u){XMMATRIX world;
            if(selectedReload::transform(selectedReload::Role::Magazine,selectedReload::profile.geometry,reloadVisual,XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim),h.gun,world)){
                unsigned n=pistolRounds::count(status.ammoValid&&status.weapon==1,status.loaded,status.reserve,reloadVisual.stage!=manualReload::Stage::Ready);
                for(unsigned j=0;j<n;++j){
                    auto roundWorld=world;
                    if(j==0&&reloadVisual.stage==manualReload::Stage::Ready)
                        roundWorld=pistolRounds::fedWorld(world,pistolReturn.seatedFraction());
                    if(j==0&&reloadVisual.stage==manualReload::Stage::Ready)chamberFeedCapture::sample(pistolReturn.seatedFraction(),now);
                    selectedDraws.push_back({pistolRounds::first+j,roundWorld});}}}
        if(h.weapon==1&&pistolRounds::first!=~0u&&reloadVisual.stage!=manualReload::Stage::Ready&&status.ammoValid&&status.weapon==1&&status.loaded>0)
            selectedDraws.push_back({pistolRounds::first,pistolRounds::fedWorld(weaponWorld,1)});
        if(h.weapon==2&&rifleRounds::first!=~0u){XMMATRIX world;
            if(selectedRifle::transform(selectedRifle::profile.sections[1],riflePhase,rifleVisual,XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim),h.gun,world)){
                unsigned n=rifleRounds::count(status.ammoValid&&status.weapon==2,status.loaded,status.reserve,riflePhase!=arsenal::Phase::Ready);
                for(unsigned j=0;j<n;++j)selectedDraws.push_back({rifleRounds::first+j,j==0&&riflePhase==arsenal::Phase::Ready?rifleRounds::fedWorld(world,rifleBoltReturn.seatedFraction()):world});
                if(n&&riflePhase==arsenal::Phase::Ready)chamberFeedCapture::sample(rifleBoltReturn.seatedFraction(),now);}}
        if(h.weapon==2&&rifleRounds::first!=~0u&&riflePhase!=arsenal::Phase::Ready&&status.ammoValid&&status.weapon==2&&status.loaded>0)
            selectedDraws.push_back({rifleRounds::first,rifleRounds::fedWorld(weaponWorld,1)});
        if(h.weapon==4&&huntingRounds::first!=~0u){XMMATRIX world;
            if(selectedReload::transform(selectedReload::Role::Magazine,selectedHunting::geometry,huntingVisual,XMLoadFloat4x4(&selectedFrames::frames[4].editorToAim),h.gun,world)){
                unsigned n=huntingRounds::count(status.ammoValid&&status.weapon==4,status.loaded,status.reserve,huntingVisual.stage==manualReload::Stage::MagazineHeld);
                for(unsigned j=0;j<n;++j)selectedDraws.push_back({huntingRounds::first+j,world});}}
    }else selectedRenderedWeapon=-1;
    if(right&&h.weapon==0&&loader.active&&selectedRevolver::profile.roundsBound&&h.tracked){
        const auto& p=selectedRevolver::profile;auto frame=XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim);
        XMFLOAT3 center;XMStoreFloat3(&center,XMVector3TransformCoord(XMVectorSet(p.contact.port.x,p.contact.port.y,p.contact.port.z,1),XMMatrixInverse(nullptr,frame)));
        selectedReload::Geometry geometry;geometry.magazineContactEditor={center.x,center.y,center.z};geometry.heldContactGrip=p.contact.held;
        manualReload::Visual v;v.valid=true;v.stage=manualReload::Stage::MagazineHeld;v.leftInGun=motionTracking::relativePose(h.gun,h.left);
        XMMATRIX carrier;if(selectedReload::transform(selectedReload::Role::Magazine,geometry,v,frame,h.gun,carrier)){
            if(selectedSpeedloader::section!=~0u)selectedDraws.push_back({selectedSpeedloader::section,selectedSpeedloader::transform(carrier)});
            for(unsigned j=loader.first;j<(std::min)(6u,loader.first+loader.count);++j){auto id=p.rounds[j];selectedDraws.push_back({id,selectedRevolver::transform(id,true,carrier)});}
        }
    }
    if(selectedReload::profile.ready&&selectedReload::showPouchMagazine(h.weapon,reloadVisual.stage,
            status.ammoValid&&status.weapon==1,status.reserve,status.loaded)){
        const auto& p=selectedReload::profile;
        selectedDraws.push_back({p.sections[0],selectedReload::pouchMagazine(p.geometry,
            XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim),pouchAnchor,h.head)});
        if(pistolRounds::first!=~0u){auto world=selectedReload::pouchMagazine(p.geometry,XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim),pouchAnchor,h.head);
            for(unsigned j=0;j<pistolRounds::count(true,status.loaded,status.reserve,true);++j)selectedDraws.push_back({pistolRounds::first+j,world});}
    }
    if(h.weapon==2&&selectedRifle::profile.ready&&rifleVisual.valid&&riflePhase==arsenal::Phase::Open&&status.ammoValid&&status.weapon==2&&(status.reserve>0||status.loaded>0)){
        const auto& p=selectedRifle::profile;
        auto world=selectedReload::pouchMagazine(p.geometry,XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim),pouchAnchor,h.head);
        selectedDraws.push_back({p.sections[0],world});
        if(rifleRounds::first!=~0u){for(unsigned j=0;j<rifleRounds::count(true,status.loaded,status.reserve,true);++j)selectedDraws.push_back({rifleRounds::first+j,world});}
        else selectedDraws.push_back({p.sections[1],world});
    }
    if(h.weapon==4&&selectedHunting::section!=~0u&&huntingVisual.stage!=manualReload::Stage::MagazineHeld&&status.ammoValid&&status.weapon==4&&(status.reserve>0||status.loaded>0)){
        auto world=selectedReload::pouchMagazine(selectedHunting::geometry,XMLoadFloat4x4(&selectedFrames::frames[4].editorToAim),pouchAnchor,h.head);
        selectedDraws.push_back({selectedHunting::section,world});
        if(huntingRounds::first!=~0u)for(unsigned j=0;j<huntingRounds::count(true,status.loaded,status.reserve,true);++j)selectedDraws.push_back({huntingRounds::first+j,world});
    }
    if(h.weapon==3&&selectedShotgun::props.ready&&status.ammoValid&&status.weapon==3&&status.reserve>0){
        unsigned id=selectedShotgun::props.cartridges[0];const auto& s=selectedPack.sections[id];XrVector3f center{};
        for(unsigned j=0;j<s.count;++j){const auto* v=selectedPack.vertices[s.first+j].value;center.x+=v[0];center.y+=v[1];center.z+=v[2];}
        if(s.count){center.x/=s.count;center.y/=s.count;center.z/=s.count;
            selectedReload::Geometry g;g.magazineContactEditor=center;g.heldContactGrip=arsenal::replacementShotgunContact.held;
            bool held=false;{std::lock_guard<std::mutex> lock(manualReload::mutex);held=arsenal::reloads[2].state()==arsenal::Phase::Held;}
            auto frame=XMLoadFloat4x4(&selectedFrames::frames[3].editorToAim);
            if(held&&h.tracked){manualReload::Visual v;v.valid=true;v.stage=manualReload::Stage::MagazineHeld;v.leftInGun=motionTracking::relativePose(h.gun,h.left);XMMATRIX world;
                if(selectedReload::transform(selectedReload::Role::Magazine,g,v,frame,h.gun,world))selectedDraws.push_back({id,world});}
            else selectedDraws.push_back({id,selectedReload::pouchMagazine(g,frame,pouchAnchor,h.head)});}
    }
    if(h.weapon==0&&selectedRevolver::profile.roundsBound&&!loader.active&&status.ammoValid&&status.weapon==0&&status.reserve>0){
        const auto& p=selectedRevolver::profile;auto frame=XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim);XMFLOAT3 center;
        XMStoreFloat3(&center,XMVector3TransformCoord(XMVectorSet(p.contact.port.x,p.contact.port.y,p.contact.port.z,1),XMMatrixInverse(nullptr,frame)));
        selectedReload::Geometry g;g.magazineContactEditor={center.x,center.y,center.z};
        auto carrier=selectedReload::pouchMagazine(g,frame,pouchAnchor,h.head);
        if(selectedSpeedloader::section!=~0u)selectedDraws.push_back({selectedSpeedloader::section,selectedSpeedloader::transform(carrier)});
        for(unsigned j=0;j<unsigned((std::min)(6,status.reserve));++j){auto id=p.rounds[j];selectedDraws.push_back({id,selectedRevolver::transform(id,true,carrier)});}
    }
    if(right&&(h.weapon==1||h.weapon==2||h.weapon==4)){
        auto stage=h.weapon==1?reloadVisual.stage:h.weapon==4?huntingVisual.stage:riflePhase==arsenal::Phase::Open?manualReload::Stage::EmptyWell:riflePhase==arsenal::Phase::Held?manualReload::Stage::MagazineHeld:manualReload::Stage::Ready;
        const auto& geometry=h.weapon==1?selectedReload::profile.geometry:h.weapon==2?selectedRifle::profile.geometry:selectedHunting::geometry;
        unsigned id=h.weapon==1?selectedReload::profile.sections[0]:h.weapon==2?selectedRifle::profile.sections[0]:selectedHunting::section;
        // The same magazine section is also drawn at the hip and in the left
        // hand. Its draw-list transform is NOT the magazine-well transform.
        // Ejection always uses the fitted gun frame, even after the inserted
        // magazine becomes hidden. Keep the source mesh independent of visibility.
        std::vector<unsigned> parts;
        if(id<selectedPack.sections.size())parts.push_back(id);
        const XMMATRIX current=weaponWorld;
        if(h.weapon==2&&status.loaded>0){
            if(rifleRounds::first!=~0u){for(unsigned n=1;n<rifleRounds::count(status.ammoValid,status.loaded,status.reserve,false);++n)parts.push_back(rifleRounds::first+n);}
            else parts.push_back(selectedRifle::profile.sections[1]);}
        if(h.weapon==1&&pistolRounds::first!=~0u)for(unsigned n=1;n<pistolRounds::count(status.ammoValid,status.loaded,status.reserve,false);++n)parts.push_back(pistolRounds::first+n);
        if(h.weapon==4&&huntingRounds::first!=~0u)for(unsigned n=0;n<huntingRounds::count(status.ammoValid,status.loaded,status.reserve,false);++n)parts.push_back(huntingRounds::first+n);
        magazineDrop::update(h.weapon,stage==manualReload::Stage::EmptyWell,current,geometry.magazineContactEditor,parts,now,h.scale,true);
    }
    magazineDrop::draw(now,[&](unsigned section,const XMMATRIX& matrix){selectedDraws.push_back({section,matrix});});
    const auto holster=bodyHolsters::read();
    if(bodyHolsters::showHip(holster)){
        const auto a=bodyHolsters::anchor(0);
        auto pose=XMMatrixRotationX(-1.57079632679f)*XMMatrixTranslation(a.x,a.y,a.z)*selectedFrames::pose(holster.body);
        pose=holsterMotion::hip[holster.hip].update(pose,false,now);
        auto world=XMLoadFloat4x4(&selectedFrames::frames[holster.hip].editorToAim)*pose;
        for(unsigned i=0;i<selectedPack.sections.size();++i){const auto& s=selectedPack.sections[i];
            if(s.weapon==unsigned(holster.hip)&&s.kind==psxAssets::Weapon&&!pistolRounds::contains(i)&&!selectedCases::contains(i)&&i!=selectedSpeedloader::section)selectedDraws.push_back({i,world});
        }
    }
    // Left-shoulder melee storage is intentionally invisible.
    const int bottleSide=pillBottle::heldSide.load();
    const bool flashlightOnly=!validWeapon||bodyHolsters::hidden.load();
    // An empty/stowed right hand has no reload receiver. Do not use its
    // weapon visibility as the validity gate for the independent left lamp.
    if(flashlightOnly)handheldLight::reloadHandoff.blocked(false,false,0,false);
    const bool lampHandoffBlocked=!flashlightOnly&&handheldLight::reloadHandoff.blocked(lampReloadBusy,h.grip,manualReload::distance(h.left.position,h.gun.position),right&&left);
    handheldLight::reloadSuppressed=lampHandoffBlocked;
    const bool lampHeld=selectedFlashlight::ready&&weaponGroups::lampOwned.load()&&handheldLight::eligible(h.weapon,right,left,lampHandoffBlocked||pillBottle::holding.load()||leftPrimary,bodyHolsters::hidden.load());
    const bool lampMounted=selectedFlashlight::ready&&right&&mountedLight::eligible(h.weapon);
    const auto lampPose=lampMounted?mountedLight::pose(h.weapon,h.gun):handheldLight::modelPose(h.left);
    handheldLight::publish(lampPose,now,lampHeld||lampMounted,lampHeld);
    if(lampHeld&&handheldLight::thumbPress(now)>.999f&&handheldLight::captureContact.exchange(false)&&handheldLight::capture)handheldLight::capture();
    if(lampHeld||lampMounted){selectedDraws.push_back({selectedFlashlight::section,selectedFrames::pose(lampPose)});selectedDraws.push_back({selectedFlashlight::glassSection,selectedFrames::pose(lampPose)});}
    if(lampMounted)selectedDraws.push_back({mountedLight::tape[h.weapon],selectedFrames::pose(h.gun)});
    bottleVisible=pillBottle::holding.load()&&!pillBottle::geometry.empty()&&
        (bottleSide==1?h.rightPoseTracked&&motionTracking::nearHead(h.rightGrip,h.head):left);
    if(bottleVisible)replacementPose[bottleSide==1?1:0]=replacementHands::bind(bottleSide==1?h.rightGrip:h.left);
    if(((bottleVisible&&bottleSide==1)||leftPrimary||holster.stowed||!validWeapon)&&h.rightPoseTracked&&motionTracking::nearHead(h.rightGrip,h.head)){
        auto local=XMLoadFloat4x4(&selectedFrames::frames[2].editorToFree)*XMMatrixScaling(-1,1,1);
        // The source relaxed pose is authored upright. Correct only the empty
        // right hand around its wrist; never rotate weapon or bottle fits.
        if(!(bottleVisible&&bottleSide==1)){
            XMFLOAT3 pivot;XMStoreFloat3(&pivot,(XMLoadFloat4x4(&selectedFrames::frames[2].wristToEditor)*local).r[3]);
            local=local*XMMatrixTranslation(-pivot.x,-pivot.y,-pivot.z)*XMMatrixRotationX(-XM_PIDIV2)*XMMatrixTranslation(pivot.x,pivot.y,pivot.z);
        }
        auto world=local*selectedFrames::pose(h.rightGrip);
        for(unsigned i=0;i<selectedPack.sections.size();++i){const auto& s=selectedPack.sections[i];
            if(s.weapon==2&&s.kind==psxAssets::LeftSupport&&!selectedHoldingHand::auxiliary(i)){
                const auto held=bottleVisible&&bottleSide==1?selectedHoldingHand::section:selectedHoldingHand::relaxedSection;
                selectedDraws.push_back({held<selectedPack.sections.size()&&selectedPack.sections[held].count==s.count?held:i,world});
            }
        }
    }
    if(leftPrimary&&left){
        auto wristWorld=leftMeleeWrist::ready[h.weapon-5]?XMLoadFloat4x4(&leftMeleeWrist::local[h.weapon-5])*selectedFrames::pose(h.gun):selectedFrames::wrist(2,selectedFrames::freeHand(2,h.left));
        prepareSelectedStatus(status,wristWorld,settings.enabled&&settings.wrist==0);
    }
    if(left&&!leftPrimary){
        // A released hand retains the saved relaxed support curl, but follows
        // its own controller. Acquired support shares exactly the weapon frame.
        const bool attached=!(bottleVisible&&bottleSide==0)&&selectedHands::attachSupport(right,validWeapon&&selectedFrames::frames[h.weapon].support,supportAim::activeFor(h.weapon),h.grip,reloadOwnsLeft);
        const int leftProfile=(attached||(reloadOwnsLeft&&validWeapon&&selectedFrames::frames[h.weapon].support))?h.weapon:2;
        auto leftWorld=attached?weaponWorld:selectedFrames::freeHand(leftProfile,h.left);
        if(attached&&h.weapon==3){
            const int si=arsenal::index(3);float pull=0;
            {std::lock_guard<std::mutex> lock(manualReload::mutex);if(si>=0&&arsenal::visuals[si].valid)pull=arsenal::visuals[si].slidePull;}
            auto frame=XMLoadFloat4x4(&selectedFrames::frames[3].editorToAim);
            leftWorld=frame*XMMatrixTranslation(0,0,pull)*XMMatrixInverse(nullptr,frame)*weaponWorld;
        }
        if(h.weapon==1&&reloadVisual.valid&&(reloadVisual.stage==manualReload::Stage::MagazineHeld||reloadVisual.stage==manualReload::Stage::SlideHeld)&&reloadVisual.alignedHand)
            leftWorld=XMLoadFloat4x4(&selectedFrames::frames[leftProfile].editorToFree)*selectedReload::pose(reloadVisual.leftInGun)*selectedReload::pose(h.gun);
        if(h.weapon==2&&rifleVisual.valid&&(riflePhase==arsenal::Phase::Held||riflePhase==arsenal::Phase::Charging)&&rifleVisual.alignedHand)
            leftWorld=XMLoadFloat4x4(&selectedFrames::frames[leftProfile].editorToFree)*selectedReload::pose(rifleVisual.leftInGun)*selectedReload::pose(h.gun);
        if(h.weapon==4&&huntingVisual.valid&&huntingVisual.stage==manualReload::Stage::MagazineHeld&&huntingVisual.alignedHand)
            leftWorld=XMLoadFloat4x4(&selectedFrames::frames[leftProfile].editorToFree)*selectedReload::pose(huntingVisual.leftInGun)*selectedReload::pose(h.gun);
        leftWorld=supportTransition::left.update(leftWorld,attached,leftProfile,now,true,reloadOwnsLeft);
        const bool holding=lampHeld||(bottleVisible&&bottleSide==0)||selectedHoldingHand::use(h.weapon,reloadVisual.valid,reloadVisual.stage);
        const float curl=holdingCurl.update(holding,now,lampHeld||h.weapon==1||(bottleVisible&&bottleSide==0));
        const float lampCurl=flashlightCurl.update(lampHeld,now,!attached&&!reloadOwnsLeft&&!bottleVisible&&leftProfile==2);
        // This transform belongs exclusively to flashlight ownership. Free,
        // reload, bottle and support hands retain their accepted transforms.
        if(lampHeld||lampCurl>0)leftWorld=handheldLight::forwardHand(XMLoadFloat4x4(&selectedFrames::frames[leftProfile].editorToFree),leftWorld,lampCurl);
        for(unsigned i=0;i<selectedPack.sections.size();i++){const auto& s=selectedPack.sections[i];
            if(s.weapon==unsigned(leftProfile)&&s.kind==psxAssets::LeftSupport&&!selectedHoldingHand::auxiliary(i)){
                bool morph=(lampHeld||h.weapon==1||(bottleVisible&&bottleSide==0))&&selectedHoldingHand::section<selectedPack.sections.size()&&selectedPack.sections[selectedHoldingHand::section].count==s.count;
                const auto relaxed=selectedHoldingHand::relaxedSection;
                const bool empty=!attached&&!reloadOwnsLeft&&!bottleVisible&&!lampHeld;
                const auto base=(empty||morph)&&relaxed<selectedPack.sections.size()&&selectedPack.sections[relaxed].count==s.count?relaxed:i;
                if((lampHeld||lampCurl>0)&&selectedHoldingHand::flashlightSection<selectedPack.sections.size()&&relaxed<selectedPack.sections.size()){
                    if(lampCurl<.999f)selectedDraws.push_back({relaxed,leftWorld,selectedHoldingHand::flashlightSection,lampCurl});
                    else selectedDraws.push_back({selectedHoldingHand::flashlightSection,leftWorld,selectedHoldingHand::flashlightPressSection,handheldLight::thumbPress(now)});}
                else selectedDraws.push_back({base,leftWorld,morph?selectedHoldingHand::section:~0u,morph?curl:0.f});}}
        prepareSelectedStatus(status,selectedFrames::wrist(leftProfile,leftWorld),settings.enabled&&settings.wrist==0);
    }else {supportTransition::left.reset();holdingCurl.reset();flashlightCurl.reset();}
}
inline void prepareFrame(){
    if(selectedMode){prepareSelectedFrame();return;}
    replacementVisible[0]=replacementVisible[1]=false;prepared=false;forearmVisible[0]=forearmVisible[1]=false;for(auto& v:gripVisible)v=false;auto now=GetTickCount64();auto status=model.read(now);manualReload::Hands h;
    {std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;if(bodyHolsters::hidden.load()){h.weapon=-1;h.gunTracked=false;}huntingRifle::prepare(h,now);boltGripActive=h.weapon==4;bottleVisible=pillBottle::holding.load();
        handGrip::Input input;input.gameplay=h.gameplay;input.tracking=h.session;input.time=h.time;input.now=now;
        input.weapon=h.weapon;input.leftTracked=h.tracked&&h.leftPoseTracked;input.rightTracked=h.gunTracked&&h.rightPoseTracked;
        input.leftGrip=h.grip;input.trigger=h.trigger>.15f;
        if(bottleVisible)input.leftObject=handGrip::Object::Bottle;
        int profile=arsenal::index(h.weapon);
        input.magazineReload=h.weapon==4&&arsenal::reloads[3].usesDetachableMagazine();
        if(profile>=0&&profile<4)input.reload=static_cast<handGrip::Reload>(arsenal::reloads[profile].state());
        if(h.weapon==4){float travel=arsenal::reloads[3].state()==arsenal::Phase::Charging?arsenal::visuals[3].slidePull:arsenal::reloads[3].boltOffset();boltGripStep=static_cast<unsigned>(std::lround(std::clamp(std::fabs(travel)/.08f,0.f,1.f)*4));}
        if(h.weapon==1){
            auto stage=manualReload::gestures.transaction().stage();
            if(stage==manualReload::Stage::MagazineHeld)input.leftObject=handGrip::Object::Magazine;
            else if(stage==manualReload::Stage::SlideHeld)input.leftObject=handGrip::Object::Mechanism;
            else if(stage!=manualReload::Stage::Ready)input.reload=handGrip::Reload::Open;
        }
        if(h.tracked&&h.gunTracked&&motionTracking::validPose(h.gun)&&motionTracking::validPose(h.left))
            input.supportDistance=manualReload::distance(motionTracking::relativePose(h.gun,h.left).position,handFitting::support(h.weapon,replacementHands::measuredSupport(h.weapon)));
        if(replacementHands::measuredSupport(h.weapon)&&supportAim::activeFor(h.weapon))input.supportDistance=0;
        replacementGrip=handGrip::select(input);
    }
    // Visual continuity is independent of fresh tracking used by interactions.
    h.leftPoseTracked=h.leftPoseTracked||h.leftDisplayAvailable;
    h.rightPoseTracked=h.rightPoseTracked||h.rightDisplayAvailable;
    h.gunTracked=h.gunTracked||h.rightDisplayAvailable;
    h.tracked=h.tracked||(h.leftDisplayAvailable&&h.rightDisplayAvailable);
    if(h.session&&h.gameplay&&status.world&&now>=h.time&&now-h.time<100){
        for(unsigned i=0;i<2;i++){auto grip=i?h.rightGrip:h.left;bool tracked=i?h.rightPoseTracked:h.leftPoseTracked;
            if(tracked&&motionTracking::validPose(grip)&&motionTracking::nearHead(grip,h.head)){forearmPose[i]=bind(grip,i);forearmVisible[i]=true;}}
        for(unsigned i=0;i<3;i++)if(weaponGripHands::frame(i,h,now,gripMatrix[i])){gripVisible[i]=true;forearmVisible[1]=false;}
    }
    if((replacementHands::ready&&!nativeHandClip::originalHands)){
        for(unsigned side=0;side<2;side++){
            const bool tracked=side?h.rightPoseTracked:h.leftPoseTracked;auto grip=side?h.rightGrip:h.left;
            if(h.session&&h.gameplay&&status.world&&now>=h.time&&now-h.time<100&&tracked&&motionTracking::nearHead(grip,h.head)&&!(side&&h.weapon==4&&sniperScope::factor()>1.01f)){
                replacementVisible[side]=true;replacementPose[side]=side&&h.gunTracked?handFitting::weapon(h.gun,h.weapon):(!side&&replacementGrip.left==handGrip::Pose::Support?handFitting::supportHand(h.gun,grip):replacementHands::bind(grip));
                if(!side&&replacementGrip.left==handGrip::Pose::Support&&replacementHands::measuredSupport(h.weapon))
                    replacementVisible[side]=handFitting::measuredSupportPose(h.gun,h.weapon,replacementPose[side]);
                if(side&&replacementHands::nativeSource&&h.gunTracked&&h.weapon==1)
                    replacementVisible[side]=nativeWristPose::frame(h.gun,now,replacementPose[side]);
                if(!side&&replacementHands::nativeSource&&replacementGrip.left!=handGrip::Pose::Support&&replacementGrip.left!=handGrip::Pose::Mechanism&&!bottleVisible){
                    XrPosef fitted;if(nativeWristPose::leftFrame(h.gun,h.rightGrip,grip,now,h.gunTracked&&h.weapon==1,fitted))replacementPose[side]=fitted;
                }
                // Saved editor geometry is baked once into the tracked aim frame.
                // Do not apply handFitting's wrist correction a second time.
                if(replacementHands::savedFits&&h.gunTracked){
                    const auto p=side?replacementGrip.right:replacementGrip.left;
                    if((side&&(h.weapon==0||(h.weapon>=2&&h.weapon<=5))&&(p==handGrip::Pose::Weapon||p==handGrip::Pose::Trigger||p==handGrip::Pose::Buzzsaw))||
                       (!side&&(h.weapon==4||h.weapon==5)&&p==handGrip::Pose::Support))replacementPose[side]=h.gun;
                }
            }
            forearmVisible[side]=false;
        }
        for(auto& v:gripVisible)v=false;
        for(unsigned side=0;side<2;side++){
            unsigned target=replacementHands::section(side?replacementGrip.right:replacementGrip.left,side);
            auto pose=side?replacementGrip.right:replacementGrip.left;
            if(side&&replacementHands::sections>=36&&(h.weapon==3||h.weapon==4)&&(pose==handGrip::Pose::Weapon||pose==handGrip::Pose::Trigger))target=28+(h.weapon==4?4:0)+(pose==handGrip::Pose::Trigger?2:0)+side;
            if(!side&&pose==handGrip::Pose::Support&&replacementHands::measuredSupport(h.weapon))target=replacementHands::supportSection(h.weapon);
            if(side&&replacementHands::savedFits&&(h.weapon==0||(h.weapon>=2&&h.weapon<=5))&&(pose==handGrip::Pose::Weapon||pose==handGrip::Pose::Trigger||pose==handGrip::Pose::Buzzsaw))target=37+static_cast<unsigned>(h.weapon)*4+(pose==handGrip::Pose::Trigger?2:0);
            if(!side&&replacementGrip.left==handGrip::Pose::Mechanism&&boltGripActive&&replacementHands::sections>=28)target=18+boltGripStep*2;
            auto& t=gripTransition[side];
            if(!t.set||!replacementVisible[side]){t.from=t.to=target;t.started=now;t.set=true;}
            else if(t.to!=target){t.from=replacementHands::canMorph(t.to,target)?t.to:target;t.to=target;t.started=now;}
            t.mix=now>=t.started?std::clamp(float(now-t.started)/80.f,0.f,1.f):1.f;
        }
    }
    unsigned hand=settings.wrist;if(hand>1||!settings.enabled||!h.session||!h.gameplay||!status.world||now<h.time||now-h.time>=100){visible=false;return;}
    if((replacementHands::ready&&!nativeHandClip::originalHands)&&!replacementVisible[hand]){visible=false;return;}
    auto grip=hand?h.rightGrip:h.left;bool tracked=hand?h.rightPoseTracked:h.leftPoseTracked;
    if(!tracked||!motionTracking::nearHead(grip,h.head)){visible=false;return;}
    if(lastWrist!=hand){visible=false;lastWrist=hand;haveDisplay=false;}framePose=(replacementHands::ready&&!nativeHandClip::originalHands)?replacementPose[hand]:bind(grip,hand);visible=inspect(framePose,h.head,visible);if(!visible)return;
    if(!haveDisplay||!same(status,displayed)){displayed=status;dynamicMesh=statusMesh(status);if((replacementHands::ready&&!nativeHandClip::originalHands))replacementHands::statusFit(dynamicMesh,hand);haveDisplay=true;uploaded=false;}
    prepared=true;
}

inline bool createAlbedo(ID3D12Device* device,ID3D12GraphicsCommandList* list){
    auto* imageBytes=selectedMode?savedWrist::texture.data():const_cast<BYTE*>(albedoPNG);
    const auto imageSize=selectedMode?savedWrist::texture.size():sizeof(albedoPNG);
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct COMScope{bool own;~COMScope(){if(own)CoUninitialize();}} scope{SUCCEEDED(com)};
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
       FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromMemory(imageBytes,static_cast<DWORD>(imageSize)))||
       FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||
       FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    UINT w{},h{};if(FAILED(converter->GetSize(&w,&h))||!w||!h||w>4096||h>4096)return false;
    D3D12_RESOURCE_DESC tex{};tex.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;tex.Width=w;tex.Height=h;tex.DepthOrArraySize=1;tex.MipLevels=1;tex.Format=DXGI_FORMAT_R8G8B8A8_UNORM;tex.SampleDesc.Count=1;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&tex,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&albedo))))return false;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 size{};device->GetCopyableFootprints(&tex,0,1,0,&footprint,nullptr,nullptr,&size);
    D3D12_RESOURCE_DESC buf{};buf.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buf.Width=size;buf.Height=1;buf.DepthOrArraySize=1;buf.MipLevels=1;buf.SampleDesc.Count=1;buf.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buf,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&albedoUpload))))return false;
    void* bytes{};D3D12_RANGE none{};if(FAILED(albedoUpload->Map(0,&none,&bytes)))return false;
    HRESULT copied=converter->CopyPixels(nullptr,footprint.Footprint.RowPitch,static_cast<UINT>(size),static_cast<BYTE*>(bytes));albedoUpload->Unmap(0,nullptr);if(FAILED(copied))return false;
    D3D12_TEXTURE_COPY_LOCATION dest{},source{};dest.pResource=albedo.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.pResource=albedoUpload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprint;
    list->CopyTextureRegion(&dest,0,0,0,&source,nullptr);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={albedo.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&barrier);
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=4;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&albedoHeap))))return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=tex.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(albedo.Get(),&srv,albedoHeap->GetCPUDescriptorHandleForHeapStart());return true;
}
inline bool createSkin(ID3D12Device* device,ID3D12GraphicsCommandList* list){
    UINT width=128,height=128;std::vector<unsigned char> pixels;
    auto& skinBytes=selectedMode?selectedPack.texture:replacementHands::skinPNG;
    if(selectedMode||((replacementHands::ready&&!nativeHandClip::originalHands)&&!skinBytes.empty())){
        HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);struct COMScope{bool own;~COMScope(){if(own)CoUninitialize();}} scope{SUCCEEDED(com)};
        ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
        if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromMemory(skinBytes.data(),static_cast<DWORD>(skinBytes.size())))||FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||FAILED(converter->GetSize(&width,&height))||!width||!height||width>(selectedMode?selectedPack.textureSize:2048u)||height>(selectedMode?selectedPack.textureSize:2048u))return false;
        pixels.resize(static_cast<size_t>(width)*height*4);if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return false;
    }else if(nativeTexture::skinPixels.size()==65536)pixels=nativeTexture::skinPixels;else pixels.resize(65536,128);
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC tex{};tex.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;tex.Width=width;tex.Height=height;tex.DepthOrArraySize=tex.MipLevels=1;tex.Format=DXGI_FORMAT_R8G8B8A8_UNORM;tex.SampleDesc.Count=1;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&tex,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&skinTexture))))return false;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};device->GetCopyableFootprints(&tex,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&skinUpload))))return false;
    void* mapped{};D3D12_RANGE none{};if(FAILED(skinUpload->Map(0,&none,&mapped)))return false;
    for(unsigned y=0;y<height;y++)std::memcpy(static_cast<unsigned char*>(mapped)+footprint.Offset+y*footprint.Footprint.RowPitch,pixels.data()+static_cast<size_t>(y)*width*4,width*4);skinUpload->Unmap(0,nullptr);
    D3D12_TEXTURE_COPY_LOCATION source{},dest{};source.pResource=skinUpload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprint;dest.pResource=skinTexture.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dest,0,0,0,&source,nullptr);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={skinTexture.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&barrier);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=tex.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;auto handle=albedoHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(skinTexture.Get(),&srv,handle);return true;
}
inline bool initialize(ID3D12Device* device,ID3D12GraphicsCommandList* list,unsigned sampleCount){
    if(failed)return false;
    if(owner==device&&pipeline&&albedo&&albedoHeap&&vertices&&dynamicVertices&&samples==sampleCount)return true;
    ComPtr<ID3DBlob> vs,ps,error;
    if(FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error))||
       FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error))){failed=true;return false;}
    D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.Num32BitValues=36;parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,4,0,0,0};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[1].DescriptorTable={1,&range};parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=2;desc.pParameters=parameters;desc.NumStaticSamplers=1;desc.pStaticSamplers=&sampler;desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> serialized;
    if(FAILED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error))||
       FAILED(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)))){failed=true;return false;}
    D3D12_INPUT_ELEMENT_DESC layout[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,36,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,44,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"POSITION",1,DXGI_FORMAT_R32G32B32_FLOAT,1,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",1,DXGI_FORMAT_R32G32B32_FLOAT,1,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    p.InputLayout={layout,7};p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;p.SampleDesc.Count=sampleCount;p.SampleMask=UINT_MAX;
    p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.RasterizerState.MultisampleEnable=sampleCount>1;
    p.DepthStencilState.DepthEnable=TRUE;p.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    p.DepthStencilState.StencilReadMask=p.DepthStencilState.StencilWriteMask=255;
    p.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
    auto& blend=p.BlendState.RenderTarget[0];blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipeline)))){failed=true;return false;}
    blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_SRC_ALPHA;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
    p.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    if(FAILED(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&glassPipeline)))){failed=true;return false;}
    auto geometry=bandMesh();auto printing=printedMesh();geometry.insert(geometry.end(),printing.begin(),printing.end());count=static_cast<unsigned>(geometry.size());
    for(unsigned i=0;i<2;i++){auto arm=forearmMesh(i);forearmFirst[i]=static_cast<unsigned>(geometry.size());forearmCount[i]=static_cast<unsigned>(arm.size());geometry.insert(geometry.end(),arm.begin(),arm.end());}
    for(unsigned i=0;i<3;i++){const auto& hand=weaponGripHands::meshes[i];gripFirst[i]=static_cast<unsigned>(geometry.size());gripCount[i]=static_cast<unsigned>(hand.size());geometry.insert(geometry.end(),hand.begin(),hand.end());}
    bottleFirst=static_cast<unsigned>(geometry.size());geometry.insert(geometry.end(),pillBottle::geometry.begin(),pillBottle::geometry.end());
    huntingFirst=static_cast<unsigned>(geometry.size());geometry.insert(geometry.end(),huntingRifle::geometry.begin(),huntingRifle::geometry.end());
    flashFirst=static_cast<unsigned>(geometry.size());
    for(unsigned axis=0;axis<2;axis++){
        Vertex a{.548f,-.035f,0,1,0,0,1,.8f,.28f,0,0,4},b{.548f,.035f,0,1,0,0,1,.8f,.28f,0,0,4},c{.63f,0,0,1,0,0,1,.95f,.65f,0,0,4};
        if(axis){std::swap(a.y,a.z);std::swap(b.y,b.z);}geometry.insert(geometry.end(),{a,b,c});
    }
    replacementFirst=static_cast<unsigned>(geometry.size());geometry.insert(geometry.end(),replacementHands::geometry.begin(),replacementHands::geometry.end());
    selectedFirst=static_cast<unsigned>(geometry.size());
    if(selectedMode){
        static_assert(sizeof(psxAssets::Vertex)==sizeof(Vertex),"PSX vertex layout mismatch");
        const auto previous=geometry.size();geometry.resize(previous+selectedPack.vertices.size());
        std::memcpy(geometry.data()+previous,selectedPack.vertices.data(),selectedPack.vertices.size()*sizeof(Vertex));
        // Keep the flat skin-colored wrist caps opaque, including their rim.
        selectedWristFirst=static_cast<unsigned>(geometry.size());
        geometry.insert(geometry.end(),savedWrist::geometry.begin(),savedWrist::geometry.end());
    }
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=geometry.size()*sizeof(Vertex);buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&vertices)))){failed=true;return false;}
    void* data{};D3D12_RANGE empty{0,0};if(FAILED(vertices->Map(0,&empty,&data))){failed=true;return false;}
    std::memcpy(data,geometry.data(),geometry.size()*sizeof(Vertex));vertices->Unmap(0,nullptr);
    vb={vertices->GetGPUVirtualAddress(),static_cast<UINT>(buffer.Width),sizeof(Vertex)};owner=device;samples=sampleCount;
    buffer.Width=60000*sizeof(Vertex);
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&dynamicVertices)))){failed=true;return false;}
    dynamicVB={dynamicVertices->GetGPUVirtualAddress(),0,sizeof(Vertex)};uploaded=false;if(!createAlbedo(device,list)||!createSkin(device,list)||!selectedSurface::create(device,list,albedoHeap.Get())||!selectedFlashlight::initialize(device,list,albedo.Get())){failed=true;return false;}return true;
}

inline bool uploadStatus(){
    if(uploaded)return true;if(dynamicMesh.size()>60000)return false;
    void* mapped{};D3D12_RANGE none{0,0};if(FAILED(dynamicVertices->Map(0,&none,&mapped)))return false;
    std::memcpy(mapped,dynamicMesh.data(),dynamicMesh.size()*sizeof(Vertex));dynamicVertices->Unmap(0,nullptr);
    dynamicVB.SizeInBytes=static_cast<UINT>(dynamicMesh.size()*sizeof(Vertex));uploaded=true;return true;
}
inline void draw(ID3D12Device* device,ID3D12GraphicsCommandList* list,unsigned sampleCount,D3D12_CPU_DESCRIPTOR_HANDLE rtv,D3D12_CPU_DESCRIPTOR_HANDLE dsv,const XrView& view,unsigned width,unsigned height){
    if(geometryIsolation::mode(GetTickCount64())==geometryIsolation::NoSceneExtras)return;
    if((selectedMode?(selectedDraws.empty()&&!selectedWristVisible):(!prepared&&!forearmVisible[0]&&!forearmVisible[1]&&!gripVisible[0]&&!gripVisible[1]&&!gripVisible[2]&&!replacementVisible[0]&&!replacementVisible[1]&&!huntingRifle::visible))||!initialize(device,list,sampleCount)||((selectedMode?selectedWristVisible:prepared)&&!uploadStatus()))return;
    list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
    ID3D12DescriptorHeap* heaps[]={albedoHeap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootDescriptorTable(1,albedoHeap->GetGPUDescriptorHandleForHeapStart());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};D3D12_RECT rect{0,0,LONG(width),LONG(height)};
    list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);list->OMSetRenderTargets(1,&rtv,FALSE,&dsv);
    auto eye=XMMatrixInverse(nullptr,motionTracking::rotation(view.pose.orientation)*XMMatrixTranslation(view.pose.position.x,view.pose.position.y,view.pose.position.z));
    auto transform=[&](const XrPosef& pose){auto matrix=motionTracking::rotation(pose.orientation)*XMMatrixTranslation(pose.position.x,pose.position.y,pose.position.z);
        float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),matrix*eye*arsenalProps::projection(view.fov));XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values+20),matrix*eye);list->SetGraphicsRoot32BitConstants(0,36,values,0);};
    D3D12_VERTEX_BUFFER_VIEW both[2]={vb,vb};list->IASetVertexBuffers(0,2,both);
    if(selectedMode){
        // Draw glass after all opaque hands/props so the hand can occlude the lens.
        for(unsigned pass=0;pass<2;++pass)for(const auto& item:selectedDraws){
            if(item.section>=selectedPack.sections.size())continue;
            const bool glass=item.section==selectedFlashlight::glassSection;
            if(glass!=(pass==1))continue;
            list->SetPipelineState(glass?glassPipeline.Get():pipeline.Get());
            const auto& section=selectedPack.sections[item.section];
            auto drawHeap=selectedFlashlight::contains(item.section)?selectedFlashlight::heap.Get():albedoHeap.Get();
            if(!drawHeap)continue;
            list->SetDescriptorHeaps(1,&drawHeap);list->SetGraphicsRootDescriptorTable(1,drawHeap->GetGPUDescriptorHandleForHeapStart());
            float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),item.editorToWorld*eye*arsenalProps::projection(view.fov));XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values+20),item.editorToWorld*eye);
            const bool morph=item.morph<selectedPack.sections.size()&&selectedPack.sections[item.morph].count==section.count;
            D3D12_VERTEX_BUFFER_VIEW pair[2]={vb,vb};
            if(morph){for(unsigned slot=0;slot<2;++slot){const auto& s=selectedPack.sections[slot?item.morph:item.section];pair[slot].BufferLocation+=(selectedFirst+s.first)*sizeof(Vertex);pair[slot].SizeInBytes=s.count*sizeof(Vertex);}values[19]=item.mix;}
            list->IASetVertexBuffers(0,2,morph?pair:both);
            list->SetGraphicsRoot32BitConstants(0,36,values,0);
            list->DrawInstanced(section.count,1,morph?0:selectedFirst+section.first,0);
            if(section.kind==psxAssets::RightHand)nativeHandClip::drawn[1]=GetTickCount64();
            if(section.kind==psxAssets::LeftSupport)nativeHandClip::drawn[0]=GetTickCount64();
        }
        list->SetPipelineState(pipeline.Get());list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootDescriptorTable(1,albedoHeap->GetGPUDescriptorHandleForHeapStart());
        list->IASetVertexBuffers(0,2,both);
        for(const auto& object:arsenalProps::displayedProps){
            if(!object.casing||object.weapon>=5||selectedCases::sections[object.weapon]>=selectedPack.sections.size())continue;
            const auto& section=selectedPack.sections[selectedCases::sections[object.weapon]];
            auto matrix=XMMatrixScaling(object.scale.x,object.scale.y,object.scale.z)*selectedFrames::pose(object.pose);
            float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),matrix*eye*arsenalProps::projection(view.fov));XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values+20),matrix*eye);
            list->SetGraphicsRoot32BitConstants(0,36,values,0);list->DrawInstanced(section.count,1,selectedFirst+section.first,0);
        }
        if(selectedWristVisible){
            float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),selectedWristWorld*eye*arsenalProps::projection(view.fov));
            list->SetGraphicsRoot32BitConstants(0,36,values,0);
            list->DrawInstanced(static_cast<UINT>(savedWrist::geometry.size()),1,selectedWristFirst,0);
            D3D12_VERTEX_BUFFER_VIEW pair[2]={dynamicVB,dynamicVB};list->IASetVertexBuffers(0,2,pair);
            list->DrawInstanced(static_cast<UINT>(dynamicMesh.size()),1,0,0);
        }
        if(selectedFrameWeapon>=0){selectedRenderedAt=GetTickCount64();selectedRenderedWeapon=selectedFrameWeapon;}
        if(bottleVisible){list->IASetVertexBuffers(0,2,both);transform(replacementPose[pillBottle::heldSide.load()==1?1:0]);list->DrawInstanced(static_cast<UINT>(pillBottle::geometry.size()),1,bottleFirst,0);}
        const auto chest=bodyHolsters::read();
        const unsigned carried=pillBottle::carried.load(),pocketCount=carried-(bottleVisible&&carried?1:0);
        if(chest.valid&&!pillBottle::geometry.empty())for(unsigned i=0;i<(std::min)(3u,pocketCount);++i){
            const auto pocket=pillBottle::pocketAnchor;
            auto matrix=XMMatrixTranslation(0,.032f,.072f)*XMMatrixRotationZ(1.57079632679f)*
                XMMatrixTranslation(pocket.x+.036f*i,pocket.y,pocket.z)*selectedFrames::pose(chest.body);
            float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),matrix*eye*arsenalProps::projection(view.fov));XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values+20),matrix*eye);
            list->IASetVertexBuffers(0,2,both);list->SetGraphicsRoot32BitConstants(0,36,values,0);list->DrawInstanced(static_cast<UINT>(pillBottle::geometry.size()),1,bottleFirst,0);
        }
        if(selectedFrameComplete&&selectedLoadoutDrawn)selectedLoadoutDrawn();
        return; // Never draw the old bank with the new atlas.
    }
    if(huntingRifle::visible){
        for(unsigned i=0;i<8;i++)if(huntingRifle::partVisible[i]){float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),huntingRifle::part[i]*huntingRifle::pose*eye*arsenalProps::projection(view.fov));list->SetGraphicsRoot32BitConstants(0,36,values,0);list->DrawInstanced(huntingRifle::count[i],1,huntingFirst+huntingRifle::first[i],0);}
        auto now=GetTickCount64();
        if(huntingRifle::flash){float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),huntingRifle::pose*eye*arsenalProps::projection(view.fov));list->SetGraphicsRoot32BitConstants(0,36,values,0);list->DrawInstanced(6,1,flashFirst,0);}
        huntingRifle::gpuReady=true;huntingRifle::renderedAt=now;
    }
    for(unsigned i=0;i<3;i++)if(gripVisible[i]&&gripCount[i]){float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),gripMatrix[i]*eye*arsenalProps::projection(view.fov));list->SetGraphicsRoot32BitConstants(0,36,values,0);list->DrawInstanced(gripCount[i],1,gripFirst[i],0);}
    for(unsigned i=0;i<2;i++)if(forearmVisible[i]){transform(forearmPose[i]);list->DrawInstanced(forearmCount[i],1,forearmFirst[i],0);if(nativeHandClip::originalHands)nativeHandClip::drawn[i]=GetTickCount64();}
    for(unsigned side=0;side<2;side++)if(replacementVisible[side]){
        const auto& t=gripTransition[side];D3D12_VERTEX_BUFFER_VIEW pair[2]={vb,vb};
        for(unsigned slot=0;slot<2;slot++){unsigned section=slot?t.to:t.from;pair[slot].BufferLocation+=(replacementFirst+replacementHands::first[section])*sizeof(Vertex);pair[slot].SizeInBytes=replacementHands::count[section]*sizeof(Vertex);}
        list->IASetVertexBuffers(0,2,pair);
        auto matrix=motionTracking::rotation(replacementPose[side].orientation)*XMMatrixTranslation(replacementPose[side].position.x,replacementPose[side].position.y,replacementPose[side].position.z);
        float values[36]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),matrix*eye*arsenalProps::projection(view.fov));XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values+20),matrix*eye);values[19]=t.mix;list->SetGraphicsRoot32BitConstants(0,36,values,0);
        list->DrawInstanced(replacementHands::count[t.to],1,0,0);nativeHandClip::drawn[side]=GetTickCount64();
    }
    list->IASetVertexBuffers(0,2,both);
    if(bottleVisible&&replacementVisible[0]&&!pillBottle::geometry.empty()){transform(replacementPose[0]);list->DrawInstanced(static_cast<UINT>(pillBottle::geometry.size()),1,bottleFirst,0);}
    if(!prepared)return;transform(framePose);
    if((replacementHands::ready&&!nativeHandClip::originalHands)){unsigned section=16+settings.wrist;list->DrawInstanced(replacementHands::count[section],1,replacementFirst+replacementHands::first[section],0);}
    else list->DrawInstanced(count,1,0,0);
    D3D12_VERTEX_BUFFER_VIEW dynamicPair[2]={dynamicVB,dynamicVB};list->IASetVertexBuffers(0,2,dynamicPair);list->DrawInstanced(static_cast<UINT>(dynamicMesh.size()),1,0,0);
}
inline void drawExtras(ID3D12Device* d,ID3D12GraphicsCommandList* l,unsigned sampleCount,D3D12_CPU_DESCRIPTOR_HANDLE rtv,D3D12_CPU_DESCRIPTOR_HANDLE dsv,const XrView& v,unsigned w,unsigned h){arsenalProps::draw(d,l,sampleCount,rtv,dsv,v,w,h);draw(d,l,sampleCount,rtv,dsv,v,w,h);}
}


