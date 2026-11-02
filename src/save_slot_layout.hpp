#pragma once
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>
namespace saveSlotLayout {
struct Layout {float x=330,y=178,scale=.62f;};
inline bool parse(const std::string& text,Layout& result){
 if(text.size()>2048)return false;Layout next;
 auto number=[&](const char* key,float& out){auto p=text.find(std::string("\"")+key+"\"");if(p==std::string::npos)return false;p=text.find(':',p);if(p==std::string::npos)return false;char* end{};const char* start=text.c_str()+p+1;out=std::strtof(start,&end);return end!=start&&std::isfinite(out);};
 if(!number("x",next.x)||!number("y",next.y)||!number("scale",next.scale))return false;
 if(next.scale<.35f||next.scale>1.2f||next.x<0||next.y<0||next.x+465*next.scale>640.01f||next.y+209*next.scale>360.01f)return false;
 result=next;return true;
}
inline std::filesystem::path path;
inline Layout current;
inline unsigned long long checked{};
inline Layout get(unsigned long long now){
 if(path.empty()||now<checked+1000)return current;checked=now;
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file||file.tellg()>2048)return current;file.seekg(0);
 std::string text((std::istreambuf_iterator<char>(file)),{});parse(text,current);return current;
}
}
