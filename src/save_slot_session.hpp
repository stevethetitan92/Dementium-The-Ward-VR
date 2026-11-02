#pragma once
#include "save_slot_store.hpp"
#include <mutex>
#include <optional>
namespace saveSlots {
// A selected slot owns a durable working directory. Crashes leave that directory
// recoverable; bank snapshots remain independent and the original save is read-only.
class Sessions {
 inline static std::atomic<unsigned> serial{};
 fs::path root,native;Store bank;unsigned active=0;fs::path working;
 static std::wstring folded(fs::path p){auto s=fs::absolute(p).lexically_normal().wstring();if(s.rfind(L"\\\\?\\",0)==0)s.erase(0,4);for(auto& c:s){if(c==L'/')c=L'\\';c=static_cast<wchar_t>(std::towlower(c));}while(s.size()>3&&s.back()==L'\\')s.pop_back();return s;}
 fs::path slot(unsigned i)const{return root/(L"slot-"+std::to_wstring(i+1));}
 static bool sessionName(const std::wstring& n){return n.size()>8&&n.substr(0,8)==L"session-"&&n.find_first_not_of(L"0123456789-",8)==std::wstring::npos;}
 fs::path recovered(unsigned i)const{
  std::wifstream in(slot(i)/L"working");std::wstring name;std::getline(in,name);if(!in||!sessionName(name))return {};
  auto base=slot(i)/name;auto p=base/L"Dementium - The Ward"/L"saves";
  if(fs::is_symlink(base)||!fs::is_regular_file(base/L"complete")||!fs::is_directory(p))return {};return p;
 }
 bool createWorking(unsigned i){
  auto name=L"session-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial++);
  auto parent=slot(i)/name/L"Dementium - The Ward";fs::create_directories(parent);
  auto dest=parent/L"saves";if(!bank.makeSession(i,dest))return false;
  // Native New Game retains these profile files and its manifest continues to
  // list them. Seed the same baseline in a fresh slot, without copying a run.
  for(auto name:{L"Pickups.sav",L"Scores.sav"}){
   auto source=native/name,target=dest/name;
   if(!fs::exists(target)&&fs::is_regular_file(source)&&!fs::is_symlink(source)){
    auto bytes=fs::file_size(source);if(bytes>1024)return false;
    if(!fs::copy_file(source,target))return false;
   }
  }
  {std::ofstream out(parent.parent_path()/L"complete");out<<"1\n";if(!out.flush())return false;}
  auto temp=slot(i)/(name+L".pointer");{std::wofstream out(temp);out<<name<<L'\n';if(!out.flush())return false;}
  return MoveFileExW(temp.c_str(),(slot(i)/L"working").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 }
public:
 Sessions(fs::path directory,fs::path nativeDirectory):root(fs::absolute(directory)),native(fs::absolute(nativeDirectory)),bank(root/L"banks"){}
 bool initialize(){try{
  fs::create_directories(root);if(bank.selected(0).empty()&&!bank.importExisting(native))return false;
  for(unsigned i=1;i<3;i++)if(bank.selected(i).empty()&&!bank.newGame(i))return false;
  unsigned last=0;std::ifstream in(root/L"active");if(!(in>>last)||last>=3)last=0;in.close();if(!hasProgress(last)){for(unsigned i=0;i<3;i++)if(hasProgress(i)){last=i;break;}}return select(last);
 }catch(...){return false;}}
 bool select(unsigned i){if(i>=3)return false;try{auto p=recovered(i);if(p.empty()){if(!createWorking(i))return false;p=recovered(i);}if(p.empty())return false;
  auto temp=root/L"active.tmp";{std::ofstream out(temp);out<<i<<'\n';if(!out.flush())return false;}
  if(!MoveFileExW(temp.c_str(),(root/L"active").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
  active=i;working=p;return true;
 }catch(...){return false;}}
 bool hasProgress(unsigned i)const{if(i>=3)return false;try{auto p=recovered(i);if(p.empty())return bank.hasProgress(i);for(auto& e:fs::directory_iterator(p)){auto name=e.path().filename().wstring();for(auto& c:name)c=static_cast<wchar_t>(std::towlower(c));if(name.substr(0,6)==L"ingame"&&gameplayFile(name)&&e.is_regular_file()&&!e.is_symlink()&&e.file_size()==61440)return true;}return false;}catch(...){return false;}}
 bool snapshot(){return !working.empty()&&bank.save(active,working);}
 bool newGame(unsigned i,bool confirmed=false){if(i>=3||(!confirmed&&hasProgress(i)))return false;try{
  // Publish an empty generation without destroying either old bank or session.
  if(!bank.newGame(i,true)||!createWorking(i))return false;return select(i);
 }catch(...){return false;}}
 unsigned selected()const{return active;}
 const fs::path& directory()const{return working;}
 fs::path metadata()const{return working.parent_path().parent_path()/L"vr-state";}
 // Restrict redirection to direct children of the exact native save directory.
 // Options/cloud metadata remain global, while temporary save files and wildcard
 // enumeration use the same selected session as the actual game-save files.
 std::optional<fs::path> route(const fs::path& input)const{
  if(working.empty()||input.empty())return {};try{auto abs=fs::absolute(input).lexically_normal();
   if(folded(abs)==folded(native))return working;
   if(folded(abs.parent_path())!=folded(native))return {};
   auto name=abs.filename().wstring(),lower=name;for(auto& c:lower)c=static_cast<wchar_t>(std::towlower(c));
   if(lower==L"options.sav"||lower==L"steam_autocloud.vdf")return {};
   if(name.empty()||name.find(L':')!=std::wstring::npos)return {};return working/name;
  }catch(...){return {};}
 }
};
}

