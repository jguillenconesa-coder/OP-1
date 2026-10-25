// SPDX-License-Identifier: AGPL-3.0-only
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <string>
#include "pluginterfaces/base/ipluginbase.h"
int main(int argc,char** argv) {
    const bool executable = argc==3 && std::string(argv[1])=="--manifest-exe";
    if(argc!=2 && !executable) {std::cerr<<"Expected VST3 module path or --manifest-exe executable\n";return 1;}
    const char* input = argv[executable ? 2 : 1];
    const int length=MultiByteToWideChar(CP_UTF8,0,input,-1,nullptr,0);
    std::wstring path(size_t(length),L'\0');MultiByteToWideChar(CP_UTF8,0,input,-1,path.data(),length);
    if(!path.empty()&&path.front()==L'/') {path=L"Z:"+path;std::replace(path.begin(),path.end(),L'/',L'\\');}
    // Wine may resolve Common Controls ordinals without the Windows activation
    // context. Inspect the actual PE resource before attempting a module load.
    HMODULE data=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE);
    if(!data) {std::cerr<<"Cannot inspect PE resources: "<<GetLastError()<<'\n';return 1;}
    auto resource=FindResourceW(data,MAKEINTRESOURCEW(executable?1:2),MAKEINTRESOURCEW(24));
    auto loaded=resource?LoadResource(data,resource):nullptr;
    auto bytes=loaded?static_cast<const char*>(LockResource(loaded)):nullptr;
    std::string manifest=bytes?std::string(bytes,SizeofResource(data,resource)):std::string();
    FreeLibrary(data);
    if(manifest.find("Microsoft.Windows.Common-Controls")==std::string::npos ||
       manifest.find("6.0.0.0")==std::string::npos) {
        std::cerr<<"Missing Common Controls v6 activation manifest (ordinal 345 would fail on Windows)\n";return 1;
    }
    std::cout<<"PASS: embedded Common Controls v6 activation manifest\n";
    if(executable) return 0;
    HMODULE module=LoadLibraryW(path.c_str());
    if(!module) {std::cerr<<"LoadLibrary failed: "<<GetLastError()<<'\n';return 1;}
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));
    auto finish=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
    auto getFactory=reinterpret_cast<Steinberg::IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));
    if(!getFactory||(init&&!init())) {std::cerr<<"VST3 entry point failed\n";FreeLibrary(module);return 1;}
    auto* factory=getFactory();
    const int classes=factory?factory->countClasses():0;
    if(factory) factory->release();
    if(finish) finish();
    FreeLibrary(module);
    if(classes<1) {std::cerr<<"VST3 factory exposes no classes\n";return 1;}
    std::cout<<"PASS: Windows VST3 module loaded; factory exposes "<<classes<<" classes\n";
}
