#include "../src/init_diagnostics.hpp"
#include <stdexcept>
#include <fstream>
#include <string>
#include <cassert>
DWORD WINAPI normal(void*){return 7;}
int main(){
 assert(initDiagnostics::open(true));initDiagnostics::mark("test-stage");
 assert(initDiagnostics::boundary([]{return DWORD(7);})==7);
 bool caught=false;try{initDiagnostics::boundary([]()->DWORD{throw std::runtime_error("diagnostic-what-sentinel");});}catch(const std::runtime_error& e){caught=std::string(e.what())=="diagnostic-what-sentinel";}assert(caught);
 caught=false;try{initDiagnostics::boundary([]()->DWORD{throw 42;});}catch(int value){caught=value==42;}assert(caught);
 SetLastError(123);initDiagnostics::write("preserve-error");assert(GetLastError()==123);
 HMODULE missing=LoadLibraryExW(L"Z:\\nonexistent-dementium-test\\missing.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);assert(!missing);DWORD error=GetLastError();initDiagnostics::os("missing_dependency_fixture",error);assert(GetLastError()==error);
 initDiagnostics::module(nullptr);assert(initDiagnostics::exception(nullptr)==EXCEPTION_CONTINUE_SEARCH);
 HANDLE t=CreateThread(nullptr,0,normal,nullptr,0,nullptr);assert(t);assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);DWORD code{};assert(GetExitCodeThread(t,&code)&&code==7);CloseHandle(t);
 std::ifstream f(initDiagnostics::path);std::string log((std::istreambuf_iterator<char>(f)),{});
 for(auto text:{"pid=","tid=","stage=test-stage","Initialize_return=7","diagnostic-what-sentinel","unknown_cpp_exception","missing_dependency_fixture","machine=0x8664"})assert(log.find(text)!=std::string::npos);
 wprintf(L"PASS: returns, std/unknown rethrow, flush visibility, OS error preservation, missing dependency, PE identity, thread exit; log=%ls\n",initDiagnostics::path);
}
