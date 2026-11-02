#pragma once
#include <cstddef>
#include <cstring>
namespace particleFog {
// Retail particle PS: float3 fogColor, bool fogEnabled, float4 LUT[64].
// Follow the world-fog switch without changing particle tint, alpha or LUT.
inline bool prepare(void* destination,const void* source,std::size_t bytes,bool worldFog){
 if(!destination||!source||bytes!=1040||worldFog)return false;
 std::memcpy(destination,source,bytes);
 const unsigned disabled=0;
 std::memcpy(static_cast<unsigned char*>(destination)+12,&disabled,4);
 return true;
}
}
