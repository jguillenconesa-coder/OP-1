// SPDX-License-Identifier: AGPL-3.0-only
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <string>
#include "pluginterfaces/base/ipluginbase.h"
int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"Expected VST3 module path\n";return 1;}
    const int length=MultiByteToWideChar(CP_UTF8,0,argv[1],-1,nullptr,0);
    std::wstring path(size_t(length),L'\0');MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path.data(),length);
    if(!path.empty()&&path.front()==L'/') {path=L"Z:"+path;std::replace(path.begin(),path.end(),L'/',L'\\');}
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
