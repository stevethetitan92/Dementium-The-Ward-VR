#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <openxr/openxr.h>
#include <atomic>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include "../src/optic_scope.hpp"
int main(){
 Microsoft::WRL::ComPtr<IDXGIFactory4> factory;Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;Microsoft::WRL::ComPtr<ID3D12Device> device;
 if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))||FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))||FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))))return 2;
 try{if(!opticScope::initialize(device.Get(),4)){puts("Optic GPU resource initialization failed");return 1;}}
 catch(const std::exception& e){puts(e.what());return 1;}
 selectedBallistics::ready[4]=true;selectedBallistics::muzzle[4]={.02f,.04f,-.75f};
 opticScope::enabled=true;XrPosef gun{{0,0,0,1},{0,0,0}};XrView eye{XR_TYPE_VIEW};eye.pose=gun;eye.pose.position={opticScope::ocular.x,opticScope::ocular.y,opticScope::ocular.z+.1f};
 opticScope::publish(gun,1000,true);opticScope::update(1000,eye,eye);assert(opticScope::active);
 assert(std::fabs(opticScope::camera.pose.position.x-.02f)<1e-6f&&std::fabs(opticScope::camera.pose.position.y-.04f)<1e-6f&&std::fabs(opticScope::camera.pose.position.z+.75f)<1e-6f);
 puts("Optic HLSL, root signature, multisampled targets, depth-aware pipeline and shared muzzle-ray camera passed on WARP");
}
