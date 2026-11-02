#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <openxr/openxr.h>
#include <cstdio>
int wmain(int argc,wchar_t** argv){if(argc!=2)return 1;
 auto dll=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 if(!dll){printf("LoadLibrary error=%lu\n",GetLastError());return 2;}
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(dll);auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<char*>(dll)+dos->e_lfanew);if(nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return 3;
 auto get=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(dll,"xrGetInstanceProcAddr"));if(!get)return 4;
 PFN_xrEnumerateApiLayerProperties enumerate{};if(XR_FAILED(get(XR_NULL_HANDLE,"xrEnumerateApiLayerProperties",reinterpret_cast<PFN_xrVoidFunction*>(&enumerate)))||!enumerate)return 5;
 uint32_t count{};auto result=enumerate(0,&count,nullptr);printf("official_loader x64=1 enumerate_api_layers_result=%d layer_count=%u no_instance_or_session_created=1\n",result,count);FreeLibrary(dll);return XR_FAILED(result)?6:0;}
