// SPDX-License-Identifier: AGPL-3.0-only
#include "Core246ControlWorkspace.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
std::vector<uint8_t> read(const std::string& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Missing workspace fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string dir=argv[1];const auto input=read(dir+"/core246-control-workspace-input.bin"),output=read(dir+"/core246-control-workspace-output.bin");
 check(input.size()==4352*436&&output.size()==4352*424,"Wrong workspace dimensions");
 Core246ControlWorkspace image{};std::array<uint8_t,196> neighbors{};
 for(size_t c=0;c<4352;++c){const auto* p=input.data()+c*436;const auto* e=output.data()+c*424;
  if(u32(p)){std::memcpy(image.data(),p+12,228);std::memcpy(neighbors.data(),p+240,196);}
  const auto offset=u32(p+4);check(offset<image.size(),"Invalid explicit byte poke");image[offset]=uint8_t(u32(p+8));
  core246InitializeControlWorkspaces(image);
  check(std::memcmp(image.data(),e,228)==0,"Complete initialized image/guards differ");
  check(std::memcmp(neighbors.data(),e+228,196)==0,"Pitch registers, ADSR pool or RNG changed");
 }
 std::cout<<"PASS: 4096 independent + 256 continuous Core workspace initializations; complete 228-byte footprint and shared pitch/ADSR/RNG preservation\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
