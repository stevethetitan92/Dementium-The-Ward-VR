#include "../src/arsenal_props.hpp"
#include "../src/native_hand_clip.hpp"
#include <d3d12shader.h>
#include <cstdio>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("arsenal props validation failed");}
int main(){try{
    auto geometry=arsenalProps::mesh();check(geometry.size()==192);
    for(auto& v:geometry){check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));check(std::fabs(v.x)<=.501f&&std::fabs(v.y)<=.501f&&std::fabs(v.z)<=.501f);}
    Microsoft::WRL::ComPtr<ID3DBlob> vs,ps,error;
    check(SUCCEEDED(D3DCompile(arsenalProps::shader,sizeof(arsenalProps::shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error)));
    check(SUCCEEDED(D3DCompile(arsenalProps::shader,sizeof(arsenalProps::shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error)));
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflect;check(SUCCEEDED(D3DReflect(vs->GetBufferPointer(),vs->GetBufferSize(),IID_PPV_ARGS(&reflect))));
    Microsoft::WRL::ComPtr<ID3DBlob> clipVS;
    check(SUCCEEDED(D3DCompile(nativeHandClip::shader,sizeof(nativeHandClip::shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&clipVS,&error)));
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> clipReflection;check(SUCCEEDED(D3DReflect(clipVS->GetBufferPointer(),clipVS->GetBufferSize(),IID_PPV_ARGS(&clipReflection))));
    D3D12_SHADER_DESC clipDescription{};check(SUCCEEDED(clipReflection->GetDesc(&clipDescription)));bool uv1=false;
    for(UINT index=0;index<clipDescription.OutputParameters;index++){D3D12_SIGNATURE_PARAMETER_DESC output{};check(SUCCEEDED(clipReflection->GetOutputParameterDesc(index,&output)));if(!_stricmp(output.SemanticName,"TEXCOORD")&&output.SemanticIndex==1){unsigned components=0;for(unsigned mask=output.Mask;mask;mask>>=1)components+=mask&1;uv1=components==2;}}
    check(uv1); // Native scene PS requires TEXCOORD1 independently of TEXCOORD0.
    D3D12_SHADER_BUFFER_DESC constants{};check(SUCCEEDED(reflect->GetConstantBufferByIndex(0)->GetDesc(&constants))&&constants.Size==80);
    XrFovf f{-.8f,.9f,.8f,-.7f};auto p=arsenalProps::projection(f);
    DirectX::XMFLOAT4 nearPoint,farPoint;
    DirectX::XMStoreFloat4(&nearPoint,DirectX::XMVector4Transform(DirectX::XMVectorSet(0,0,-.02f,1),p));
    DirectX::XMStoreFloat4(&farPoint,DirectX::XMVector4Transform(DirectX::XMVectorSet(0,0,-10,1),p));
    check(nearPoint.w>0&&std::fabs(nearPoint.z/nearPoint.w)<.0001f&&farPoint.z/farPoint.w>.99f);
    using namespace arsenal;using namespace manualReload;
    arsenal::installed=true;arsenal::enabled=true;manualReload::enabled=false;
    hands={};hands.session=hands.gameplay=hands.tracked=hands.gunTracked=true;hands.weapon=0;hands.time=GetTickCount64();
    arsenal::reloads[0]=arsenal::Reload{};Event e;e.valid=true;arsenal::reloads[0].update(Kind::Revolver,e);
    e.release=true;arsenal::reloads[0].update(Kind::Revolver,e);e.release=false;e.grip=e.atPouch=true;arsenal::reloads[0].update(Kind::Revolver,e);
    check(arsenalProps::props(hands.time).size()==1);hands.tracked=false;check(arsenalProps::props(hands.time).empty());
    hands.tracked=true;hands.gameplay=false;check(arsenalProps::props(hands.time).empty());
    hands.gameplay=true;hands.tracked=true;hands.left.orientation.w=0;check(arsenalProps::props(hands.time).empty());
    hands.left.orientation.w=1;hands.gun.position.x=std::numeric_limits<float>::quiet_NaN();check(arsenalProps::props(hands.time).empty());
    hands.gun.position={};hands.weapon=static_cast<int>(Kind::Sniper);hands.scale=1;hands.time=GetTickCount64();
    arsenal::reloads[3]=arsenal::Reload{};Event bolt;bolt.valid=true;auto& sniper=arsenal::reloads[3];sniper.update(Kind::Sniper,bolt);
    auto closed=arsenalProps::props(hands.time);check(closed.size()==1);
    bolt.release=true;sniper.update(Kind::Sniper,bolt);bolt.release=false;bolt.grip=bolt.atHandle=true;sniper.update(Kind::Sniper,bolt);
    bolt.travel=.08f;sniper.update(Kind::Sniper,bolt);bolt.grip=false;sniper.update(Kind::Sniper,bolt);
    auto open=arsenalProps::props(hands.time);check(open.size()==1&&std::fabs(open[0].pose.position.z-closed[0].pose.position.z-.08f)<.00001f);
    puts("PASS: procedural ammo geometry, shaders/root constants, eye depth projection and modal/tracking visibility gates; no GPU/game launched.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
