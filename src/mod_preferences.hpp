#pragma once
#include <Windows.h>
#include <atomic>
#include <filesystem>
namespace modPreferences {
inline std::atomic<bool> whiteHaze{true};
inline std::atomic<bool> blackFog{true};
inline std::filesystem::path file;
inline void load(const std::filesystem::path& path){file=path;whiteHaze=GetPrivateProfileIntW(L"Appearance",L"WhiteHaze",1,file.c_str())!=0;blackFog=GetPrivateProfileIntW(L"Appearance",L"BlackFog",1,file.c_str())!=0;}
inline bool toggleHaze(){bool next=!whiteHaze.load();if(file.empty()||!WritePrivateProfileStringW(L"Appearance",L"WhiteHaze",next?L"1":L"0",file.c_str()))return false;whiteHaze=next;return true;}
inline bool toggleBlackFog(){bool next=!blackFog.load();if(file.empty()||!WritePrivateProfileStringW(L"Appearance",L"BlackFog",next?L"1":L"0",file.c_str()))return false;blackFog=next;return true;}
// Removing the white component must not disable the fog curve or its black
// occlusion. Native ambient/flashlight values and fog lookup remain untouched.
inline void fogColor(float* color){if(!whiteHaze.load())color[0]=color[1]=color[2]=0;}
}
