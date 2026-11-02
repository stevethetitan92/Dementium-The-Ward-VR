#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "ppm_capture.hpp"

namespace asyncPpmCapture {
struct Image {
    std::wstring path;
    std::vector<unsigned char> rgba;
    std::size_t pitch{};
    unsigned width{},height{},step{1};
};
inline std::atomic<bool> busy{false};
inline std::atomic<unsigned> completed{},failed{};
struct Batch {std::vector<Image> images;HMODULE module{};};
inline DWORD WINAPI run(void* argument) {
    HMODULE module{};bool ok=true;
    {
        std::unique_ptr<Batch> batch(static_cast<Batch*>(argument));module=batch->module;
        try {
            for(const auto& image:batch->images) {
                const auto temporary=image.path+L".pending";
                FILE* file{};
                if(_wfopen_s(&file,temporary.c_str(),L"wb")){ok=false;continue;}
                bool written=false;
                try {written=ppmCapture::write(file,image.rgba.data(),image.pitch,image.width,image.height,image.step);}catch(...){written=false;}
                if(fclose(file))written=false;
                if(!written||!MoveFileExW(temporary.c_str(),image.path.c_str(),MOVEFILE_REPLACE_EXISTING)){
                    DeleteFileW(temporary.c_str());ok=false;
                }
            }
        }catch(...){ok=false;}
    } // Only CPU-owned pixels; no D3D objects or mapped GPU memory on this thread.
    if(ok)++completed;else ++failed;
    busy.store(false,std::memory_order_release);
    // Pin the DLL until execution leaves it; never join a worker under loader lock.
    FreeLibraryAndExitThread(module,ok?0:1);
    return 0;
}
inline bool submit(std::vector<Image>& images) {
    if(images.empty())return true;
    bool expected=false;if(!busy.compare_exchange_strong(expected,true))return false;
    std::unique_ptr<Batch> batch;
    try {batch=std::make_unique<Batch>();}catch(...){busy=false;return false;}
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&run),&batch->module)){busy=false;return false;}
    batch->images=std::move(images);
    HANDLE thread=CreateThread(nullptr,0,run,batch.get(),0,nullptr);
    if(!thread){images=std::move(batch->images);FreeLibrary(batch->module);busy=false;return false;}
    batch.release();CloseHandle(thread);return true;
}
}
