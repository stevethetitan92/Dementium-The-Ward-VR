#include <string>
#include "../src/save_slot_legacy_metadata.hpp"
#include <windows.h>
#include <fstream>
#include <cassert>
#include <iostream>
int main(){namespace fs=std::filesystem;auto root=fs::temp_directory_path()/std::to_string(GetCurrentProcessId())/std::to_string(GetTickCount64());auto src=root/"medicine-saves",dest=root/"metadata";fs::create_directories(dest);
 bool reproduced=false;try{fs::directory_iterator old(src);}catch(const fs::filesystem_error&){reproduced=true;}assert(reproduced);
 saveSlots::importLegacyMedicine(src,dest);assert(fs::is_empty(dest)&&!fs::exists(src));
 fs::create_directories(src);saveSlots::importLegacyMedicine(src,dest);
 {std::ofstream(src/"old.vrmeds")<<"legacy";std::ofstream(src/"keep.vrmeds")<<"old";std::ofstream(dest/"keep.vrmeds")<<"current";std::ofstream(src/"other.txt")<<"ignore";}
 saveSlots::importLegacyMedicine(src,dest);std::string s;{std::ifstream f(dest/"keep.vrmeds");f>>s;}assert(s=="current");{std::ifstream f(dest/"old.vrmeds");f>>s;}assert(s=="legacy");assert(!fs::exists(dest/"other.txt"));
 saveSlots::importLegacyMedicine(src,dest);
 {std::ofstream(root/"not-directory")<<"file";}
 bool refused=false;try{saveSlots::importLegacyMedicine(root/"not-directory",dest);}catch(const fs::filesystem_error&){refused=true;}assert(refused);
 std::cout<<"PASS: original failure reproduced; absent/empty legacy directory; migration; existing metadata preserved; repeat migration; non-directory error retained\n";
}

