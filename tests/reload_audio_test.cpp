#include "../src/reload_audio.hpp"
#include <cassert>
#include <cstdio>
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 reloadAudio::load(argv[1]);
 unsigned count=0;
 for(const auto& path:reloadAudio::clips)if(!path.empty()){
  WAVEFORMATEX format{};std::vector<char> samples;
  assert(reloadAudio::readPcm(path,format,samples));assert(!samples.empty());++count;
 }
 assert(count>=16);
 WAVEFORMATEX format{};std::vector<char> samples;
 assert(!reloadAudio::readPcm(L"missing-audio.wav",format,samples));
 reloadAudio::play(3,reloadAudio::Action::MechanismPull);
 reloadAudio::play(3,reloadAudio::Action::MechanismRelease);
 assert(reloadAudio::played==2&&reloadAudio::failed==0);
 unsigned active=0;for(auto& v:reloadAudio::voices)if(v.device)++active;
 assert(active==2);
 Sleep(500);reloadAudio::collect();
 for(auto& v:reloadAudio::voices)assert(!v.device);
 printf("%u PCM clips decoded; overlapping pump strokes completed and released their audio devices\n",count);
}
