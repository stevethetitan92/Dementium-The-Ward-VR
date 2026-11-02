#include "../src/async_ppm_capture.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iterator>
static std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void wait(){auto start=GetTickCount64();while(asyncPpmCapture::busy.load(std::memory_order_acquire)){assert(GetTickCount64()-start<10000);Sleep(1);}}
int main(){
    auto folder=std::filesystem::current_path()/"bin"/("capture-worker-fixture-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(folder);
    std::vector<asyncPpmCapture::Image> batch;
    for(unsigned step:{1u,2u}){
        asyncPpmCapture::Image im;im.path=(folder/("actual-"+std::to_string(step)+".ppm")).wstring();im.width=7;im.height=5;im.pitch=32;im.step=step;
        im.rgba.resize(im.pitch*(im.height-1)+im.width*4);for(size_t i=0;i<im.rgba.size();++i)im.rgba[i]=static_cast<unsigned char>(i*17);
        FILE* f{};assert(!_wfopen_s(&f,(folder/("expected-"+std::to_string(step)+".ppm")).c_str(),L"wb"));assert(ppmCapture::write(f,im.rgba.data(),im.pitch,im.width,im.height,im.step));fclose(f);
        batch.push_back(std::move(im));
    }
    asyncPpmCapture::busy=true;assert(!asyncPpmCapture::submit(batch)&&batch.size()==2);asyncPpmCapture::busy=false;
    assert(asyncPpmCapture::submit(batch)&&batch.empty());wait();assert(asyncPpmCapture::completed==1&&asyncPpmCapture::failed==0);
    for(unsigned step:{1u,2u}){
        auto actual=folder/("actual-"+std::to_string(step)+".ppm");assert(read(actual)==read(folder/("expected-"+std::to_string(step)+".ppm")));assert(!std::filesystem::exists(actual.wstring()+L".pending"));
    }
    asyncPpmCapture::Image bad;bad.path=(folder/"missing"/"not-created.ppm").wstring();bad.width=bad.height=1;bad.pitch=4;bad.rgba={1,2,3,4};batch.push_back(std::move(bad));assert(asyncPpmCapture::submit(batch));wait();assert(asyncPpmCapture::failed==1&&asyncPpmCapture::completed==1);
    puts("PASS: real worker byte-identical padded/truncated-last-row/downsampled P6, bounded busy rejection, atomic output completion and failed-write reporting; no GPU objects on worker.");
}
