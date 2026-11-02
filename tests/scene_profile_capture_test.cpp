#define NOMINMAX
#include "../src/bridge.cpp"
#include <stdexcept>
#include <string>
#include <thread>
#include <fstream>
static void require(bool value){if(!value)throw std::runtime_error("scene profile capture boundary failed");}
int main(int argc,char** argv){try{
    FILE* capture{};require(tmpfile_s(&capture)==0&&capture);logFile=capture;
    auto list=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(0x1000));auto pipeline=reinterpret_cast<ID3D12PipelineState*>(std::uintptr_t(0x2000));stereo::lists[list].pipeline=pipeline;
    stereo::PipelineInfo info{};info.constants=1200;info.headLightLayout=true;stereo::pipelineInfo[pipeline]=info;presents=10;
    traceSceneDraw(list,3,1,false); // Default off.
    require(TestInput(reinterpret_cast<void*>(34))==0);for(unsigned i=0;i<20;++i)traceSceneDraw(list,3,1,false);
    presents=11;traceSceneDraw(list,3,1,false); // Expired source frame.
    std::thread late([&]{traceSceneDraw(list,3,1,false);});late.join(); // New thread cannot revive an expired request.
    require(TestInput(reinterpret_cast<void*>(34))==0);traceSceneDraw(list,3,1,false);
    std::rewind(capture);std::string text;char bytes[1024];while(auto count=std::fread(bytes,1,sizeof bytes,capture))text.append(bytes,count);logFile=nullptr;std::fclose(capture);
    if(argc==2){std::ofstream output(argv[1],std::ios::binary);require(bool(output));output<<"offline_fixture=1 inert_list=1 game_access=0 gpu_access=0\n"<<text;require(bool(output));}
    size_t position=0,count=0;while((position=text.find("scene_evidence_cpu_profile ticket=",position))!=std::string::npos){++count;++position;}require(count==2);
    position=count=0;while((position=text.find("scene_evidence ticket=",position))!=std::string::npos){++count;++position;}require(count==13); //12 capped first-frame draws plus one new-ticket draw.
    require(text.find("known=0 finite=0")!=std::string::npos&&text.find("scene_evidence_cpu_profile_values")==std::string::npos);
    require(text.find("atomic_snapshot=0 gpu_contents_verified=0")!=std::string::npos);
    auto layout=text.find("scene_evidence_texture_layout ticket=1 ");require(layout!=std::string::npos);auto end=text.find('\n',layout);auto record=text.substr(layout,end-layout);require(record.find("frame=10 ")!=std::string::npos&&record.find("thread=")!=std::string::npos);
    std::puts("PASS: actual scene trace/profile capture is explicit-ticket-only, once per source frame/thread, signature mismatch fails closed in test executable; no native calls/game/GPU/XR.");return 0;
}catch(const std::exception& e){logFile=nullptr;std::puts(e.what());return 1;}}
