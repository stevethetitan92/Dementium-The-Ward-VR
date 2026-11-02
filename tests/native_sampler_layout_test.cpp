#include <d3d12.h>
#include <cstddef>
#include <cstdio>
static_assert(sizeof(D3D12_SAMPLER_DESC)==52,"native52-byte descriptor copy");
static_assert(offsetof(D3D12_SAMPLER_DESC,AddressU)==4&&offsetof(D3D12_SAMPLER_DESC,AddressV)==8,"native U/V fields");
static_assert(offsetof(D3D12_SAMPLER_DESC,AddressW)==12,"native W field");
static_assert(D3D12_TEXTURE_ADDRESS_MODE_CLAMP==3,"native spotlight assignments");
static_assert(offsetof(D3D12_SAMPLER_DESC,MaxAnisotropy)==20,"native filter override checks offset20");
static_assert(D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT==0x14&&D3D12_FILTER_ANISOTROPIC==0x55,"native filter bit/override values");
int main(){std::puts("PASS: SDK sampler descriptor52-byte layout, U/V offsets4/8 and CLAMP3 match pinned native copy path; no device/game/XR.");}
