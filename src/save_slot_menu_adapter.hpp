#pragma once
#include "save_slot_menu.hpp"
#include "save_slot_file_hooks.hpp"
#include "gamepad_bridge.hpp"
namespace saveSlotMenu {
inline std::atomic<int> allowNative{-1};
inline saveSlots::Menu model;inline std::mutex mutex;inline bool stickHeld{};
// Observed native Steam v1.1.10 main menu. No writes to UI selection/pointers.
inline int nativeSelection(){
 if(!gamepadBridge::engineBound)return -1;
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));auto root=*reinterpret_cast<unsigned char**>(base+0x3fc468);
 if(!manualReload::readable(root,0x2e74)||*reinterpret_cast<void**>(root+8)!=root+0x2d18||*reinterpret_cast<void**>(root+0x2d18)!=base+0x312258)return -1;
 if(*reinterpret_cast<unsigned*>(root+0x2e6c)!=1)return -1; // normal menu, not native Yes/No or difficulty
 auto selected=*reinterpret_cast<unsigned*>(root+0x2e70);return selected<4?int(selected):-1;
}
// Native settings/save loader used at startup. Reload after changing the routed
// directory so Continue cannot consume the previous slot's in-memory save cache.
inline bool refreshNativeSave(bool fresh){
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
 const unsigned char guard[]={0x41,0xb8,0x00,0xf0,0x00,0x00,0x89,0xbb,0x50,0x01,0x00,0x00,0x48,0x8d,0x15,0x29,0x94,0x2b,0x00};
 if(nativeSelection()<0||memcmp(base+0xcde40,guard,sizeof guard))return false;
 if(fresh)return true; // Native New Game resets the run after difficulty selection.
 saveSlotFiles::Internal bypass;
 auto directory=saveSlotFiles::sessions->directory();
 struct Block {const wchar_t* name;size_t size;unsigned destination;std::vector<char> bytes;};
 Block blocks[]={{L"InGame00.sav",61440,0x38727c,{}},{L"Global.sav",8,0x39630c,{}},{L"Pickups.sav",128,0x39628c,{}},{L"Scores.sav",12,0x396280,{}}};
 // Validate every file before changing any cached native state. The native VFS
 // keeps files in memory; rerunning its loader alone reuses the old slot.
 for(auto& b:blocks){std::ifstream in(directory/b.name,std::ios::binary|std::ios::ate);if(!in||in.tellg()!=std::streamoff(b.size))return false;in.seekg(0);b.bytes.resize(b.size);if(!in.read(b.bytes.data(),b.size))return false;}
 for(auto& b:blocks)std::memcpy(base+b.destination,b.bytes.data(),b.size);
 std::memcpy(base+0x396318,blocks[0].bytes.data(),blocks[0].size);
 return true;
}
struct View{saveSlots::MenuPage page;unsigned selection;bool fresh,overwrite;std::array<bool,3> occupied;};
inline View view(){std::lock_guard<std::mutex> lock(mutex);View v{model.page(),model.selection(),model.newGame(),model.confirmOverwrite(),{}};saveSlotFiles::Internal bypass;std::lock_guard<std::mutex> files(saveSlotFiles::mutex);if(saveSlotFiles::sessions)for(unsigned i=0;i<3;i++)v.occupied[i]=saveSlotFiles::sessions->hasProgress(i);return v;}
inline bool visible(){std::lock_guard<std::mutex> lock(mutex);return model.page()!=saveSlots::MenuPage::closed;}
inline bool input(const vrControls::Input& raw,const vrControls::Output& out,unsigned test){
 if(!saveSlotFiles::enabled)return false;std::lock_guard<std::mutex> lock(mutex);
 auto selected=nativeSelection();if(selected<0){if(model.page()!=saveSlots::MenuPage::closed)model.completed(true);return false;}
 const bool confirm=out.confirm||test==1,back=out.back||out.weaponAction||test==2;
 if(model.page()==saveSlots::MenuPage::closed){if(!confirm||selected>1)return false;model.open(selected==0,saveSlotFiles::sessions->selected());stickHeld=true;return true;}
 if(back){model.back();return true;}
 bool stick=raw.active&&(raw.leftY>.5f||raw.leftY<-.5f||raw.leftX>.5f||raw.leftX<-.5f);
 if((stick&&!stickHeld)||test==5||test==6||test==7||test==8)model.move(test==5||test==7||raw.leftY>.5f||raw.leftX<-.5f?-1:1);stickHeld=stick;
 if(confirm){std::array<bool,3> occupied{};{saveSlotFiles::Internal bypass;std::lock_guard<std::mutex> files(saveSlotFiles::mutex);for(unsigned i=0;i<3;i++)occupied[i]=saveSlotFiles::sessions->hasProgress(i);}
 auto action=model.confirm(occupied);if(action.selected){if(selected!=(action.fresh?0:1)){model.completed(false);return true;}bool success=saveSlotFiles::choose(action.slot,action.fresh,action.overwrite);if(success)success=refreshNativeSave(action.fresh);model.completed(success);if(success){allowNative=action.fresh?0:1;gamepadBridge::keyPulse(true,gamepadBridge::ConfirmKey,GetTickCount64());}}}
 return true;
}
using Action=bool(*)(void*,int);inline Action originalAction{};
inline bool action(void* screen,int choice){
 if(choice>=0&&choice<=1&&nativeSelection()>=0){
  int expected=choice;if(allowNative.compare_exchange_strong(expected,-1)){
   if(choice==0){ // The slot selector already obtained overwrite consent. Native difficulty is next.
    auto flag=reinterpret_cast<unsigned*>(reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x39631c);auto saved=*flag;*flag&=~1u;bool result=originalAction(screen,choice);*flag=saved;return result;
   }
   return originalAction(screen,choice);
  }
  std::lock_guard<std::mutex> lock(mutex);if(model.page()==saveSlots::MenuPage::closed)model.open(choice==0,saveSlotFiles::sessions->selected());return true;
 }
 return originalAction(screen,choice);
}
using Update=void(*)(void*,void*);inline Update originalUpdate{};
inline void update(void* screen,void* state){
 if(visible()){
  if(manualReload::readable(state,12)){unsigned flags=*reinterpret_cast<unsigned*>(static_cast<char*>(state)+4);unsigned command=flags&0x200?2:flags&0x60?1:flags&1?7:flags&2?8:0;
   if(command){std::lock_guard<std::mutex> lock(gamepadBridge::mutex);input({}, {}, command);}}
  return;
 }
 originalUpdate(screen,state);
}
inline bool install(){
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
 const unsigned char a[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x85,0xd2};
 const unsigned char u[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x83,0xb9,0x4c,0x01,0x00,0x00,0x00};
 if(memcmp(base+0xce5d0,a,sizeof a)||memcmp(base+0xce330,u,sizeof u))return false;
 if(MH_CreateHook(base+0xce5d0,(void*)action,(void**)&originalAction)!=MH_OK)return false;
 if(MH_CreateHook(base+0xce330,(void*)update,(void**)&originalUpdate)!=MH_OK){MH_RemoveHook(base+0xce5d0);return false;}
 return true;
}

}
