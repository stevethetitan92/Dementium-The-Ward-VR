#include "../src/save_slot_store.hpp"
#include <cassert>
#include <iostream>
namespace fs=std::filesystem;
static void write(const fs::path& p,char value,size_t n){std::ofstream out(p,std::ios::binary);std::string b(n,value);out.write(b.data(),b.size());assert(out.good());}
static char first(const fs::path& p){std::ifstream in(p,std::ios::binary);char c{};in.get(c);assert(in);return c;}
int main(){
 auto root=fs::absolute("../../work/save-slot-tests")/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));fs::create_directories(root/"native");
 auto native=root/"native";write(native/"InGame00.sav",'A',61440);write(native/"Global.sav",'G',8);write(native/"Pickups.sav",'P',128);write(native/"Options.sav",'O',4);write(native/"steam_autocloud.vdf",'S',12);
 saveSlots::Store store(root/"slots");assert(store.importExisting(native));auto preserved=store.selected(0);assert(store.hasProgress(0)&&!store.importExisting(native));
 assert(!fs::exists(preserved/"Options.sav")&&!fs::exists(preserved/"steam_autocloud.vdf"));
 assert(store.newGame(1)&&store.newGame(2));assert(!store.hasProgress(1)&&!store.hasProgress(2));
 assert(!store.newGame(0)&&store.selected(0)==preserved);assert(!store.newGame(3)&&store.selected(3).empty());
 auto session=root/"session";assert(store.makeSession(0,session));assert(!store.makeSession(0,session));write(session/"InGame00.sav",'B',61440);
 assert(store.save(1,session));assert(first(store.selected(1)/"InGame00.sav")=='B');assert(first(preserved/"InGame00.sav")=='A');assert(first(native/"InGame00.sav")=='A');
 auto second=store.selected(1);assert(!store.save(1,root/"missing")&&store.selected(1)==second);
 assert(store.newGame(1,true)&&!store.hasProgress(1)&&first(second/"InGame00.sav")=='B');
 saveSlots::Store restarted(root/"slots");assert(restarted.hasProgress(0)&&!restarted.hasProgress(1));assert(first(restarted.selected(0)/"InGame00.sav")=='A');
 assert(!saveSlots::gameplayFile(L"../InGame00.sav")&&!saveSlots::gameplayFile(L"Options.sav")&&saveSlots::gameplayFile(L"INGAME99.SAV"));
 {std::wofstream out(root/"slots"/"slot-3"/"current");out<<L"../../native\n";}assert(restarted.selected(2).empty());
 std::cout<<"PASS: three independent slots, preserved native saves/options, restart, overwrite refusal, retained generations, failed publication and invalid paths\n";
}
