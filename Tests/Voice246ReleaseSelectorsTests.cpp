// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246ReleaseSelectors.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
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
 check(argc==2,"Expected fixture directory");const std::string d=argv[1];auto in=read(d+"/voice246-release-selectors-input.bin"),out=read(d+"/voice246-release-selectors-output.bin");
 check(in.size()==772*4768&&out.size()==772*9236+1024,"Wrong release dimensions");
 constexpr std::array<size_t,13> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452}};
 constexpr std::array<size_t,13> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12}};
 std::array<uint8_t,4464> image{};std::array<Adsr246State,6> pool{};
 size_t removed=0,absent=0,written=0,retuned=0,rngUpdates=0,envelopeChanges=0;
 std::array<std::array<bool,129>,4> lengths{};std::array<size_t,4> modeCount{};
 for(size_t i=0;i<128;++i)check(u32(out.data()+i*4)==uint32_t(voice246NotePitch[i]),"Original note table differs");
 check(std::memcmp(out.data(),out.data()+out.size()-512,512)==0,"Note table changed");
 for(size_t c=0;c<772;++c){const auto* p=in.data()+c*4768;const auto* e=out.data()+512+c*9236;
  for(size_t j=0;j<13;++j)if(u32(p)||!(j==0||j==1||j==2||j==5||j==6))std::memcpy(image.data()+offsets[j],p+32+offsets[j],sizes[j]);
  for(size_t j=0;j<6;++j)pool[j]=state(image.data()+2580+j*24);
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=u32(image.data()+2568);
  const auto selected=u32(image.data()+4392);check(selected<8,"Invalid selected sound");
  const auto mode=s16(image.data()+4240+selected*16+8);const auto selector=int(floorDivide(mode,4096));check(selector>=1&&selector<=4,"Invalid selector");
  if(c<516)lengths[size_t(selector-1)][list.count]=true;++modeCount[size_t(selector-1)];
  std::array<int32_t,6> pitch{};for(size_t i=0;i<6;++i)pitch[i]=signedPhase(u32(image.data()+4136+i*4));
  auto random=u32(image.data()+4368);const auto oldRandom=random;const auto oldPitch=pitch;const auto oldPool=pool;
  const auto result=voice246ReleaseSelectors(list,u32(p+16),mode,pool,s16(image.data()+4240+selected*16+4),signedPhase(u32(image.data()+4444)),pitch,random);
  check(result!=Voice246ReleaseResult::rejected,"Valid fixture rejected");if(result==Voice246ReleaseResult::removed)++removed;else ++absent;
  retuned+=oldPitch!=pitch;rngUpdates+=oldRandom!=random;
  bool changed=false;for(size_t i=0;i<6;++i)changed|=oldPool[i].stage!=pool[i].stage||oldPool[i].blocks!=pool[i].blocks;envelopeChanges+=changed;
  for(size_t i=0;i<6;++i)put(image.data()+4136+i*4,uint32_t(pitch[i]));put(image.data()+4368,random);
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  for(size_t j=0;j<6;++j)save(image.data()+2580+j*24,pool[j]);
  if(std::memcmp(image.data(),e,4464)!=0){for(size_t k=0;k<4464;++k)if(image[k]!=e[k]){std::cerr<<"offset "<<k<<" actual "<<unsigned(image[k])<<" oracle "<<unsigned(e[k])<<"\n";break;}}
  if(std::memcmp(image.data(),e,4464)!=0)throw std::runtime_error("Full callback image differs at case "+std::to_string(c));
  const auto v=u32(p+4);check(v<6,"Invalid explicit render voice");
  std::array<int16_t,128> audio{};for(size_t i=0;i<128;++i)audio[i]=s16(p+4504+i*2);
  Adsr246Noise noise{u32(image.data()+4368),signedPhase(u32(image.data()+4376))};auto scratch=noise;
  for(size_t i=0;i<128;++i)put(image.data()+2724+i*4,uint32_t(scratch.next()));
  std::array<int16_t,4> controls{};for(size_t j=0;j<4;++j)controls[j]=s16(image.data()+4240+selected*16+j*2);
  if(pool[v].stage>=1&&pool[v].stage<=8)written+=128;
  adsr246Block(pool[v],controls,s16(p+8),noise,audio);save(image.data()+2580+v*24,pool[v]);put(image.data()+4368,noise.seed);put(image.data()+4376,uint32_t(noise.previous));
  if(std::memcmp(image.data(),e+4464,4464)!=0)throw std::runtime_error("Full post-render image differs at case "+std::to_string(c));
  check(std::memcmp(p+4496,e+8928,8)==0&&std::memcmp(p+4760,e+9192,8)==0,"Output guard changed");
  for(size_t i=0;i<128;++i)check(audio[i]==s16(e+8936+i*2),"Post-release ADSR sample differs");
  check(std::memcmp(p,e+9204,32)==0,"Source argument/caller inputs changed");
  check(pool[v].finished==(u32(e+9200)!=0),"Completion flag differs");
 }
 for(const auto& mode:lengths)for(bool b:mode)check(b,"Missing selector/list length combination");
 for(auto n:modeCount)check(n>=129,"Missing selector coverage");
 check(removed>200&&retuned>40&&rngUpdates>40&&envelopeChanges>=3,"Missing release/retune branch coverage");
 // Guards are our adapter's validation, outside the original assertion paths.
 Voice246ReleaseList bad;bad.count=129;std::array<int32_t,6> pitch{};uint32_t random=1;
 check(voice246ReleaseSelectors(bad,0,4096,pool,0,0,pitch,random)==Voice246ReleaseResult::rejected&&bad.count==129&&random==1,"Invalid list accepted");
 std::cout<<"PASS: "<<removed<<" removals, "<<absent<<" absent/disabled, "<<retuned<<" pitch changes, "<<rngUpdates<<" RNG changes, "<<envelopeChanges<<" envelope changes; "<<written<<" written / "<<98816-written<<" preserved positions; complete images and guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
