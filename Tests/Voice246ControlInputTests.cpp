// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246ControlInput.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing control-input fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string dir=argv[1];const auto input=read(dir+"/voice246-control-input.bin"),output=read(dir+"/voice246-control-output.bin");
 constexpr size_t cases=32768+256;check(input.size()==cases*16&&output.size()==cases*544,"Wrong decoder fixture dimensions");
 std::array<uint32_t,132> image{};std::array<uint32_t,128> values{};
 for(size_t c=0;c<cases;++c){const auto* p=input.data()+c*16;const auto* e=output.data()+c*544;
  if(u32(p)){auto seed=u32(p+8);for(auto& word:image){seed=seed*1664525u+1013904223u;word=seed;}for(size_t i=0;i<128;++i)values[i]=image[i+2];}
  check(voice246DecodeControl(values,p+4,3),"Valid message rejected");
  for(size_t i=0;i<128;++i)check(values[i]==u32(e+8+i*4),"Complete original control table differs");
  check(image[0]==u32(e)&&image[1]==u32(e+4)&&image[130]==u32(e+520)&&image[131]==u32(e+524),"Control table guards changed");
  check(u32(p+12)==u32(e+528)&&u32(p+4)==u32(e+532)&&u32(e+536)==4&&u32(e+540)==7,"Original packet/message changed");
 }
 const auto before=values;std::array<uint8_t,3> bad{{0xb0,128,127}};
 check(!voice246DecodeControl(values,bad.data(),3)&&values==before,"Invalid controller mutated state");
 bad[1]=255;check(!voice246DecodeControl(values,bad.data(),3)&&values==before,"High controller accepted");
 bad[1]=1;for(size_t n=0;n<3;++n)check(!voice246DecodeControl(values,bad.data(),n)&&values==before,"Short input mutated state");
 check(!voice246DecodeControl(values,nullptr,3)&&values==before,"Null input mutated state");
 std::cout<<"PASS: 32768 controller/value pairs + 256 continuous calls; 128-word table, guards and packet preserved\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
