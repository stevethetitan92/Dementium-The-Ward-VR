#pragma once
#include <Windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <array>
#include <atomic>
#include <vector>
#include <mutex>
#pragma comment(lib,"winmm.lib")
namespace reloadAudio {
enum class Action : unsigned { MagazineDrop,MagazineInsert,MechanismPull,MechanismRelease,ShellLoad,CylinderClose,SpeedloaderLoad,CylinderOpen };
inline std::array<std::wstring,42> clips;
inline std::atomic<unsigned> played{},failed{};
struct Voice { HWAVEOUT device{}; WAVEHDR header{}; std::vector<char> samples; };
inline std::array<Voice,16> voices;
inline std::mutex voiceMutex;
inline void collect(){for(auto& v:voices)if(v.device&&(v.header.dwFlags&WHDR_DONE)){
    if(waveOutUnprepareHeader(v.device,&v.header,sizeof(v.header))==MMSYSERR_NOERROR){
        waveOutClose(v.device);v.device=nullptr;v.header={};v.samples.clear();
    }
}}
// Read the RIFF chunks, rather than assuming every WAV has a 44-byte header.
inline bool readPcm(const std::wstring& path,WAVEFORMATEX& format,std::vector<char>& samples){
    HMMIO file=mmioOpenW(const_cast<wchar_t*>(path.c_str()),nullptr,MMIO_READ|MMIO_ALLOCBUF);
    if(!file)return false;
    MMCKINFO riff{};riff.fccType=mmioFOURCC('W','A','V','E');
    MMCKINFO fmt{};fmt.ckid=mmioFOURCC('f','m','t',' ');
    MMCKINFO data{};data.ckid=mmioFOURCC('d','a','t','a');
    format={};samples.clear();
    bool ok=mmioDescend(file,&riff,nullptr,MMIO_FINDRIFF)==MMSYSERR_NOERROR &&
        mmioDescend(file,&fmt,&riff,MMIO_FINDCHUNK)==MMSYSERR_NOERROR && fmt.cksize>=16 &&
        mmioRead(file,reinterpret_cast<char*>(&format),16)==16;
    if(ok)ok=format.wFormatTag==WAVE_FORMAT_PCM && (format.nChannels==1||format.nChannels==2) &&
        (format.wBitsPerSample==8||format.wBitsPerSample==16) && format.nSamplesPerSec>=8000 && format.nSamplesPerSec<=192000 &&
        format.nBlockAlign==format.nChannels*format.wBitsPerSample/8 &&
        format.nAvgBytesPerSec==format.nSamplesPerSec*format.nBlockAlign;
    if(ok)ok=mmioAscend(file,&fmt,0)==MMSYSERR_NOERROR &&
        mmioDescend(file,&data,&riff,MMIO_FINDCHUNK)==MMSYSERR_NOERROR && data.cksize>0 &&
        data.cksize<=format.nAvgBytesPerSec*5u && data.cksize%format.nBlockAlign==0;
    if(ok){samples.resize(data.cksize);ok=mmioRead(file,samples.data(),LONG(samples.size()))==LONG(samples.size());}
    mmioClose(file,0);if(!ok)samples.clear();return ok;
}
inline void load(const std::filesystem::path& folder){
    for(unsigned i=0;i<2;++i){auto p=folder/(i?L"pill-use.wav":L"pill-pickup.wav");clips[40+i]=std::filesystem::is_regular_file(p)?p.wstring():std::wstring{};}
    constexpr const wchar_t* weapons[]={L"revolver",L"pistol",L"m16",L"shotgun",L"hunting"};
    constexpr const wchar_t* actions[]={L"drop",L"insert",L"pull",L"release",L"shell",L"close",L"loader",L"open"};
    for(unsigned w=0;w<5;++w)for(unsigned a=0;a<8;++a){auto& clip=clips[w*8+a];clip.clear();auto path=folder/(std::wstring(weapons[w])+L"-"+actions[a]+L".wav");if(std::filesystem::is_regular_file(path))clip=path.wstring();}
}
// Separate cosmetic playback; never invoke native reload/audio internals or
// advance ammo. Missing clips are silent, not a Windows notification beep.
inline void play(unsigned id){if(id>=clips.size()||clips[id].empty())return;
    std::lock_guard<std::mutex> lock(voiceMutex);collect();
    Voice* voice=nullptr;for(auto& v:voices)if(!v.device){voice=&v;break;}
    if(!voice){++failed;return;} // Never stop another stroke to play this one.
    WAVEFORMATEX format{};auto& v=*voice;
    if(!readPcm(clips[id],format,v.samples)||waveOutOpen(&v.device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){v.device=nullptr;++failed;return;}
    v.header={};v.header.lpData=v.samples.data();v.header.dwBufferLength=DWORD(v.samples.size());
    if(waveOutPrepareHeader(v.device,&v.header,sizeof(v.header))==MMSYSERR_NOERROR &&
        waveOutWrite(v.device,&v.header,sizeof(v.header))==MMSYSERR_NOERROR){++played;return;}
    waveOutUnprepareHeader(v.device,&v.header,sizeof(v.header));waveOutClose(v.device);v.device=nullptr;v.header={};++failed;
}
inline void play(unsigned weapon,Action action){if(weapon<5)play(weapon*8+unsigned(action));}
}
