// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246PolyRelease.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::vector<uint8_t> read(const std::string& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Missing release fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){return int16_t(u16(p)<32768?int(u16(p)):int(u16(p))-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put(uint8_t* p,uint32_t v){for(unsigned j=0;j<4;++j)p[j]=uint8_t(v>>(j*8));}
Adsr246State state(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void save(uint8_t* p,const Adsr246State& s){const std::array<uint32_t,6> w{{s.stage,s.blocks,uint32_t(s.level),s.field12,s.field16,s.gain}};for(size_t j=0;j<6;++j)put(p+j*4,w[j]);}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string d=argv[1];auto in=read(d+"/voice246-poly-release-input.bin"),out=read(d+"/voice246-poly-release-output.bin");
 check(in.size()==768*4744&&out.size()==768*9188,"Wrong release dimensions");
 constexpr std::array<size_t,11> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408}};
 constexpr std::array<size_t,11> sizes{{2576,664,224,776,128,8,8,8,8,8,32}};
 std::array<uint8_t,4440> image{};std::array<Adsr246State,6> pool{};
 size_t removed=0,absent=0,released=0,written=0;std::array<bool,129> counts{};std::array<bool,6> voices{};size_t mismatched=0;
 for(size_t c=0;c<768;++c){const auto* p=in.data()+c*4744;const auto* e=out.data()+c*9188;
  for(size_t j=0;j<11;++j)if(u32(p)||!(j==0||j==1||j==2||j==5||j==6))std::memcpy(image.data()+offsets[j],p+32+offsets[j],sizes[j]);
  for(size_t j=0;j<6;++j)pool[j]=state(image.data()+2580+j*24);
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=u32(image.data()+2568);counts[list.count]=true;
  const auto selected=u32(image.data()+4392);check(selected<8,"Invalid selected sound");
  size_t match=0;while(match<list.count&&u32(list.bytes.data()+match*20)!=u32(p+16))++match;
  if(match<list.count){const auto v=signedPhase(u32(list.bytes.data()+match*20+16));if(v>=0){++released;voices[size_t(v)]=true;if(selected!=u32(image.data()+4400))++mismatched;}}
  const auto result=voice246PolyRelease(list,u32(p+16),pool,s16(image.data()+4240+selected*16+4));
  check(result!=Voice246ReleaseResult::rejected,"Valid fixture rejected");if(result==Voice246ReleaseResult::removed)++removed;else ++absent;
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  for(size_t j=0;j<6;++j)save(image.data()+2580+j*24,pool[j]);
  if(std::memcmp(image.data(),e,4440)!=0){for(size_t k=0;k<4440;++k)if(image[k]!=e[k]){std::cerr<<"offset "<<k<<" actual "<<unsigned(image[k])<<" oracle "<<unsigned(e[k])<<"\n";break;}}
  if(std::memcmp(image.data(),e,4440)!=0)throw std::runtime_error("Full callback image differs at case "+std::to_string(c));
  const auto v=u32(p+4);check(v<6,"Invalid explicit render voice");
  std::array<int16_t,128> audio{};for(size_t i=0;i<128;++i)audio[i]=s16(p+4480+i*2);
  Adsr246Noise noise{u32(image.data()+4368),signedPhase(u32(image.data()+4376))};auto scratch=noise;
  for(size_t i=0;i<128;++i)put(image.data()+2724+i*4,uint32_t(scratch.next()));
  std::array<int16_t,4> controls{};for(size_t j=0;j<4;++j)controls[j]=s16(image.data()+4240+selected*16+j*2);
  if(pool[v].stage>=1&&pool[v].stage<=8)written+=128;
  adsr246Block(pool[v],controls,s16(p+8),noise,audio);save(image.data()+2580+v*24,pool[v]);put(image.data()+4368,noise.seed);put(image.data()+4376,uint32_t(noise.previous));
  if(std::memcmp(image.data(),e+4440,4440)!=0)throw std::runtime_error("Full post-render image differs at case "+std::to_string(c));
  check(std::memcmp(p+4472,e+8880,8)==0&&std::memcmp(p+4736,e+9144,8)==0,"Output guard changed");
  for(size_t i=0;i<128;++i)check(audio[i]==s16(e+8888+i*2),"Post-release ADSR sample differs");
  check(std::memcmp(p,e+9156,32)==0,"Source argument/caller inputs changed");
  check(pool[v].finished==(u32(e+9152)!=0),"Completion flag differs");
 }
 check(removed==504&&absent==264&&released==297&&written==84352,"Release coverage counts differ");
 for(bool b:counts)check(b,"Missing list length");for(bool b:voices)check(b,"Missing released voice");check(mismatched>20,"Missing mismatched loaded/selected release");
 // Adapter rejects invalid inputs before mutation; these are not firmware asserts.
 Voice246ReleaseList bad;bad.count=129;auto old=pool;check(voice246PolyRelease(bad,0,pool,0)==Voice246ReleaseResult::rejected&&bad.count==129,"Invalid count accepted");
 bad.count=1;put(bad.bytes.data()+16,6);check(voice246PolyRelease(bad,0,pool,0)==Voice246ReleaseResult::rejected,"Invalid voice accepted");
 for(size_t i=0;i<6;++i)check(pool[i].stage==old[i].stage&&pool[i].blocks==old[i].blocks,"Rejected event changed pool");
 std::cout<<"PASS: "<<removed<<" removals, "<<absent<<" absent, "<<released<<" ADSR releases; "<<written<<" written / "<<98304-written<<" preserved sample positions; complete list/pools/registers/shared state/guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
