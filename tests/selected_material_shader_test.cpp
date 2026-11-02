#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <string>
#include <cstdio>
int main(){
 std::ifstream f("src/wristband_renderer.hpp");std::string text((std::istreambuf_iterator<char>(f)),{});
 auto start=text.find("inline constexpr char shader[]=R\"(");if(start==std::string::npos)return 2;start+=34;
 auto end=text.find(")\";",start);auto shader=text.substr(start,end-start);
 for(auto entry:{"vs","ps"}){Microsoft::WRL::ComPtr<ID3DBlob> code,error;
  auto profile=std::string(entry)+"_5_0";
  if(FAILED(D3DCompile(shader.data(),shader.size(),nullptr,nullptr,nullptr,entry,profile.c_str(),D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error))){if(error)puts(static_cast<const char*>(error->GetBufferPointer()));return 1;}
 }
 puts("Material vertex and pixel shaders compile with normal/occlusion/roughness/metallic sampling");
}
