#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <string>
#include <cwctype>

namespace saveSlots {
namespace fs=std::filesystem;
inline bool gameplayFile(std::wstring name){
    for(auto& c:name)c=static_cast<wchar_t>(std::towlower(c));
    if(name==L"global.sav"||name==L"pickups.sav"||name==L"scores.sav"||name==L"-saveindex")return true;
    return name.size()==12&&name.substr(0,6)==L"ingame"&&name[6]>=L'0'&&name[6]<=L'9'&&name[7]>=L'0'&&name[7]<=L'9'&&name.substr(8)==L".sav";
}
// Each publication preserves the previous generation. A failed or interrupted
// copy cannot replace the selected slot. Native saves are only ever read here.
class Store {
    fs::path root;
    inline static std::atomic<unsigned> serial{};
    fs::path slot(unsigned i)const {return root/(L"slot-"+std::to_wstring(i+1));}
    static bool generation(const std::wstring& name){
        return name.size()>5&&name.substr(0,5)==L"bank-"&&name.find_first_not_of(L"0123456789-",5)==std::wstring::npos;
    }
    static bool copyFiles(const fs::path& source,const fs::path& dest){
        std::uintmax_t total=0;unsigned count=0;
        if(source.empty())return true;
        for(const auto& e:fs::directory_iterator(source)){
            if(!gameplayFile(e.path().filename().wstring()))continue;
            if(e.is_symlink()||!e.is_regular_file()||++count>102)return false;
            auto bytes=e.file_size();total+=bytes;if(bytes>8*1024*1024||total>16*1024*1024)return false;
            if(!fs::copy_file(e.path(),dest/e.path().filename(),fs::copy_options::none))return false;
            if(fs::file_size(dest/e.path().filename())!=bytes)return false;
        }
        return true;
    }
    bool publish(unsigned i,const fs::path& source){
        if(i>=3)return false;
        try {
            auto folder=slot(i);fs::create_directories(folder);
            auto name=L"bank-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial++);
            auto pending=folder/(name+L".pending"),ready=folder/name;
            if(!fs::create_directory(pending)||!copyFiles(source,pending))return false;
            {std::ofstream marker(pending/L"complete",std::ios::binary);marker<<"Dementium save bank 1\n";if(!marker.flush())return false;}
            fs::rename(pending,ready);
            auto temp=folder/(name+L".pointer");
            {std::wofstream out(temp);out<<name<<L'\n';if(!out.flush())return false;}
            return MoveFileExW(temp.c_str(),(folder/L"current").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
        }catch(...){return false;}
    }
public:
    explicit Store(fs::path directory):root(fs::absolute(directory)){}
    fs::path selected(unsigned i)const {
        if(i>=3)return {};
        try{std::wifstream input(slot(i)/L"current");std::wstring name;std::getline(input,name);
            if(!input||!generation(name))return {};auto result=slot(i)/name;
            if(fs::is_symlink(result)||!fs::is_regular_file(result/L"complete"))return {};
            return result;
        }catch(...){return {};}
    }
    bool hasProgress(unsigned i)const {
        auto path=selected(i);if(path.empty())return false;
        try{for(auto& e:fs::directory_iterator(path)){auto name=e.path().filename().wstring();for(auto& c:name)c=static_cast<wchar_t>(std::towlower(c));
            if(name.substr(0,6)==L"ingame"&&gameplayFile(name)&&e.is_regular_file()&&!e.is_symlink()&&e.file_size()==61440)return true;}
        }catch(...){}return false;
    }
    bool save(unsigned i,const fs::path& source){return !source.empty()&&publish(i,source);}
    bool importExisting(const fs::path& source){return selected(0).empty()&&publish(0,source);}
    bool newGame(unsigned i,bool overwriteConfirmed=false){return i<3&&(!hasProgress(i)||overwriteConfirmed)&&publish(i,{});}
    bool makeSession(unsigned i,const fs::path& destination)const {
        auto source=selected(i);if(source.empty())return false;
        try{if(!fs::create_directory(destination))return false;return copyFiles(source,destination);}catch(...){return false;}
    }
};
}
