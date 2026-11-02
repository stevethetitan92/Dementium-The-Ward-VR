#include "../src/save_slot_session.hpp"
#include <cassert>
#include <iostream>
static void write(const std::filesystem::path& p,char c){std::ofstream o(p,std::ios::binary);std::string b(61440,c);o.write(b.data(),b.size());assert(o.good());}
static char first(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);char c{};assert(in.get(c));return c;}
int main(){namespace fs=std::filesystem;auto root=fs::absolute("../../work/save-session-tests")/std::to_string(GetTickCount64());auto native=root/"native";fs::create_directories(native);write(native/"InGame00.sav",'A');{std::ofstream o(native/"Scores.sav",std::ios::binary);o<<std::string(12,'S');}{std::ofstream o(native/"Pickups.sav",std::ios::binary);o<<std::string(128,'P');}
 saveSlots::Sessions s(root/"slots",native);assert(s.initialize()&&s.selected()==0&&s.hasProgress(0)&&!s.hasProgress(1));assert(first(s.directory()/"Scores.sav")=='S');
 assert(s.route(native/"InGame00.sav")==s.directory()/"InGame00.sav");assert(s.route(native/"*")==s.directory()/"*");assert(s.route(native)==s.directory());
 assert(!s.route(native/"Options.sav")&&!s.route(native/"steam_autocloud.vdf")&&!s.route(native/"sub"/"InGame00.sav")&&!s.route(native/".."/"InGame00.sav"));
 auto a=s.directory();assert(!s.newGame(0));assert(s.newGame(1)&&s.selected()==1&&!s.hasProgress(1));assert(first(s.directory()/"Scores.sav")=='S'&&first(s.directory()/"Pickups.sav")=='P');write(s.directory()/"InGame00.sav",'B');assert(s.hasProgress(1));auto b=s.directory();
 // Recover writes even if the process dies before publishing its next snapshot.
 saveSlots::Sessions restart(root/"slots",native);assert(restart.initialize()&&restart.selected()==1&&restart.directory()==b&&first(b/"InGame00.sav")=='B');
 assert(restart.select(0)&&first(restart.directory()/"InGame00.sav")=='A');assert(restart.select(1)&&restart.snapshot());
 assert(!restart.newGame(1)&&restart.directory()==b);assert(restart.newGame(1,true)&&!restart.hasProgress(1)&&restart.directory()!=b);
 assert(first(b/"InGame00.sav")=='B'&&first(a/"InGame00.sav")=='A'&&first(native/"InGame00.sav")=='A');assert(!restart.select(3)&&!restart.newGame(3));
 std::cout<<"PASS: isolated route/temporary files, protected options, durable crash recovery, three slots, overwrite refusal and retained old session\n";
}
