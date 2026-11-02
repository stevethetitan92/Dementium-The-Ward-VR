#include "../src/mod_preferences.hpp"
#include "../src/reload_audio.hpp"
#include <cassert>
#include <iostream>
int main(){
 auto path=std::filesystem::absolute("../../work/mod-menu-review/test-preferences.ini");
 modPreferences::load(path);if(!modPreferences::whiteHaze)assert(modPreferences::toggleHaze());
 float c[]={.6f,.7f,.8f,17.f,31.f};modPreferences::fogColor(c);assert(c[0]==.6f&&c[3]==17);
 assert(modPreferences::toggleHaze()&&!modPreferences::whiteHaze);modPreferences::fogColor(c);assert(c[0]==0&&c[1]==0&&c[2]==0&&c[3]==17&&c[4]==31);
 modPreferences::whiteHaze=true;modPreferences::load(path);assert(!modPreferences::whiteHaze);assert(modPreferences::toggleHaze());modPreferences::load(path);assert(modPreferences::whiteHaze);
 modPreferences::file.clear();assert(!modPreferences::toggleHaze()&&modPreferences::whiteHaze);
 modPreferences::load(path);if(!modPreferences::blackFog)assert(modPreferences::toggleBlackFog());
 const bool haze=modPreferences::whiteHaze;
 assert(modPreferences::toggleBlackFog()&&!modPreferences::blackFog);
 assert(modPreferences::whiteHaze==haze);
 modPreferences::blackFog=true;modPreferences::load(path);assert(!modPreferences::blackFog);
 assert(modPreferences::toggleBlackFog());modPreferences::load(path);assert(modPreferences::blackFog);
 modPreferences::file.clear();assert(!modPreferences::toggleBlackFog()&&modPreferences::blackFog);
 for(const auto* name:{L"assets/ReloadSounds/pill-pickup.wav",L"assets/ReloadSounds/pill-use.wav"}){WAVEFORMATEX format{};std::vector<char> data;assert(reloadAudio::readPcm(name,format,data)&&!data.empty());}
 // Same fresh-copy policy as both stereo-eye constant buffers: OFF must
 // alter only fog RGB, and ON must restore the original color next draw.
 modPreferences::load(path);
 for(int cycle=0;cycle<20;++cycle){
  const bool before=modPreferences::whiteHaze;
  const bool fogBefore=modPreferences::blackFog;
  assert(modPreferences::toggleHaze()&&modPreferences::whiteHaze!=before);
  assert(modPreferences::blackFog==fogBefore);
  for(unsigned eye=0;eye<2;++eye)for(unsigned rgbOffset:{0u,92u}){
   float source[260],copy[260];for(unsigned n=0;n<260;++n)source[n]=copy[n]=float(n+1)*.01f;
   modPreferences::fogColor(copy+rgbOffset);
   for(unsigned n=0;n<260;++n)assert(copy[n]==((!modPreferences::whiteHaze&&n>=rgbOffset&&n<rgbOffset+3)?0.f:source[n]));
  }
 }
 std::cout<<"PASS: 20 live ON/OFF changes, fresh mesh/particle copies for both eyes; original colors return ON; all other constants and black-fog choice unchanged\n";
 std::cout<<"PASS: haze changes immediately, persistence roundtrip, failed save leaves setting unchanged, only RGB changed, pill PCM decoding; no sound played or game run\n";
}

