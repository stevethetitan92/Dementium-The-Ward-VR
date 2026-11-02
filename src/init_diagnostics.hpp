#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdarg>
#include <exception>
#include <winver.h>
#pragma comment(lib,"version.lib")
namespace initDiagnostics {
inline HANDLE file=INVALID_HANDLE_VALUE;
inline wchar_t path[32768]{};
inline thread_local const char* stage="entry";
inline thread_local bool observing=false;
inline void write(const char* format,...) noexcept {
 if(file==INVALID_HANDLE_VALUE)return;
 const DWORD saved=GetLastError();char buffer[4096]{};SYSTEMTIME t{};GetSystemTime(&t);
 int n=sprintf_s(buffer,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ pid=%lu tid=%lu stage=%s ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId(),GetCurrentThreadId(),stage);
 va_list a;va_start(a,format);_vsnprintf_s(buffer+n,sizeof(buffer)-n-2,_TRUNCATE,format,a);va_end(a);
 size_t len=strlen(buffer);buffer[len++]='\n';DWORD written{};WriteFile(file,buffer,DWORD(len),&written,nullptr);FlushFileBuffers(file);SetLastError(saved);
}
struct Observation {bool previous=observing;Observation(){observing=true;}~Observation(){observing=previous;}};
inline void mark(const char* name) noexcept {stage=name;write("begin");}
inline void os(const char* call,DWORD error) noexcept {char message[512]{};FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,0,message,sizeof message,nullptr);write("api=%s win32=%lu message=%s",call,error,message);}
inline void hr(const char* call,HRESULT value) noexcept {if(FAILED(value))os(call,DWORD(value));else write("api=%s result=0x%08lx",call,value);}
inline HRESULT result(const char* call,HRESULT value) noexcept {hr(call,value);return value;}
inline void module(HMODULE m) noexcept {
 wchar_t p[32768]{};if(!GetModuleFileNameW(m,p,32768)){os("GetModuleFileNameW",GetLastError());return;}
 auto base=reinterpret_cast<const unsigned char*>(m?m:GetModuleHandleW(nullptr));auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
 write("module=%ls machine=0x%x pe_timestamp=0x%08lx image_size=%lu",p,nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage);
 DWORD ignored{},size=GetFileVersionInfoSizeW(p,&ignored);if(size&&size<1048576){auto data=HeapAlloc(GetProcessHeap(),0,size);if(data){if(GetFileVersionInfoW(p,0,size,data)){VS_FIXEDFILEINFO* v{};UINT count{};if(VerQueryValueW(data,L"\\",reinterpret_cast<void**>(&v),&count)&&count>=sizeof(*v))write("file_version=%u.%u.%u.%u",HIWORD(v->dwFileVersionMS),LOWORD(v->dwFileVersionMS),HIWORD(v->dwFileVersionLS),LOWORD(v->dwFileVersionLS));}HeapFree(GetProcessHeap(),0,data);}}
}
inline void modules() noexcept {HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());if(s==INVALID_HANDLE_VALUE)return;MODULEENTRY32W m{};m.dwSize=sizeof m;for(BOOL ok=Module32FirstW(s,&m);ok;ok=Module32NextW(s,&m))module(m.hModule);CloseHandle(s);}
inline LONG CALLBACK exception(EXCEPTION_POINTERS* p) noexcept {
 if(observing&&p&&p->ExceptionRecord)write("first_chance_exception=0x%08lx address=%p disposition=CONTINUE_SEARCH",p->ExceptionRecord->ExceptionCode,p->ExceptionRecord->ExceptionAddress);
 return EXCEPTION_CONTINUE_SEARCH;
}
inline bool open(bool launcher=false) noexcept {
 if(file!=INVALID_HANDLE_VALUE)return true;
 if(launcher||!GetEnvironmentVariableW(L"DEMENTIUM_INIT_LOG",path,32768)){
 wchar_t temp[32768]{};if(!GetTempPathW(32768,temp))return false;
 swprintf_s(path,L"%lsDementiumVR-init-%lu-%llu.log",temp,GetCurrentProcessId(),GetTickCount64());
 if(launcher)SetEnvironmentVariableW(L"DEMENTIUM_INIT_LOG",path);
 }
 file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 write("diagnostic_build=%s %s pointer_bits=%u log=%ls",__DATE__,__TIME__,unsigned(sizeof(void*)*8),path);
 wchar_t runtime[32768]{};GetEnvironmentVariableW(L"XR_RUNTIME_JSON",runtime,32768);write("VR_API=OpenXR requested_XR_RUNTIME_JSON=%ls",runtime);
 AddVectoredExceptionHandler(1,exception);return true;
}
inline void printTail() noexcept {
 HANDLE input=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(input==INVALID_HANDLE_VALUE)return;LARGE_INTEGER size{};if(GetFileSizeEx(input,&size)){LARGE_INTEGER start{};start.QuadPart=size.QuadPart>4095?size.QuadPart-4095:0;SetFilePointerEx(input,start,nullptr,FILE_BEGIN);char text[4096]{};DWORD count{};if(ReadFile(input,text,4095,&count,nullptr)){text[count]=0;printf("initialization_diagnostic_tail:\n%s\n",text);}}CloseHandle(input);
}
template<class F> DWORD boundary(F f){observing=true;try{DWORD result=f();write("Initialize_return=%lu hex=0x%08lx",result,result);observing=false;return result;}
 catch(const std::exception& e){write("std_exception what=%s disposition=rethrow",e.what());observing=false;throw;}
 catch(...){write("unknown_cpp_exception disposition=rethrow");observing=false;throw;}}
}
