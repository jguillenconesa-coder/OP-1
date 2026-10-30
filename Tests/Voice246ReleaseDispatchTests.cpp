// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246ReleaseDispatch.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing release-dispatch fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){return int16_t(u16(p)<32768?int(u16(p)):int(u16(p))-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put(uint8_t* p,uint32_t value){for(unsigned i=0;i<4;++i)p[i]=uint8_t(value>>(i*8));}
Adsr246State loadState(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void saveState(uint8_t* p,const Adsr246State& s){const std::array<uint32_t,6> words{{s.stage,s.blocks,uint32_t(s.level),s.field12,s.field16,s.gain}};for(size_t i=0;i<6;++i)put(p+i*4,words[i]);}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
 const auto in=read(dir+"/voice246-release-dispatch-input.bin"),out=read(dir+"/voice246-release-dispatch-output.bin");
 constexpr size_t cases=1680,imageBytes=4656,recordBytes=4720,resultBytes=4704;
 check(in.size()==cases*recordBytes&&out.size()==1028+cases*resultBytes,"Wrong dispatch dimensions");
 check(u32(out.data())==0x02ffffe8u,"Own LINK-frame calibration differs");
 for(size_t i=0;i<128;++i)check(u32(out.data()+4+i*4)==uint32_t(voice246NotePitch[i]),"Original copied note table differs");
 check(std::memcmp(out.data()+4,out.data()+out.size()-512,512)==0,"Note table changed");
 constexpr std::array<size_t,17> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476,4612,4652}};
 constexpr std::array<size_t,17> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,136,40,4}};
 constexpr std::array<bool,17> retained{{true,true,true,false,false,true,true,false,false,false,false,false,false,false,true,true,true}};
 std::array<uint8_t,imageBytes> image{};std::array<Adsr246State,6> envelopes{};
 std::array<std::array<std::array<bool,128>,2>,5> notes{};std::array<std::array<bool,129>,5> lengths{};
 std::array<bool,256> metadata{};size_t filtered=0,removed=0,absent=0,nonzero=0,zero=0,retunes=0,envelopeChanges=0;
 for(size_t c=0;c<cases;++c){
  const auto* p=in.data()+c*recordBytes;const auto* e=out.data()+516+c*resultBytes;
  check((u32(p)!=0)==(c<1360||(c-1360)%64==0),"Wrong independent/stream action");
  for(size_t i=0;i<offsets.size();++i)if(u32(p)||!retained[i])std::memcpy(image.data()+offsets[i],p+64+offsets[i],sizes[i]);
  const auto selected=u32(image.data()+4392);check(selected<8,"Invalid selected sound");
  const auto modeWord=s16(image.data()+4240+selected*16+8);const auto mode=size_t(floorDivide(modeWord,4096));check(mode<5,"Invalid selector");
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=u32(image.data()+2568);check(list.count<=128,"Invalid list count");lengths[mode][list.count]=true;
  for(size_t i=0;i<6;++i)envelopes[i]=loadState(image.data()+2580+i*24);
  std::array<int32_t,6> pitch{};for(size_t i=0;i<6;++i)pitch[i]=signedPhase(u32(image.data()+4136+i*4));
  auto random=u32(image.data()+4368);const auto oldPitch=pitch;const auto oldEnvelopes=envelopes;
  Voice246NoteArgument argument{},local{};std::memcpy(argument.data(),p+16,16);std::memcpy(local.data(),p+40,16);
  std::array<uint8_t,128> map{};std::memcpy(map.data(),image.data()+4480,128);
  for(auto byte:map)check(byte==0,"Map ceased to be explicit empty context");
  const auto note=u32(argument.data());metadata[argument[8]]=true;
  const auto result=voice246ReleaseDispatch(argument,u32(image.data()+4468),map,local,list,modeWord,envelopes,
      s16(image.data()+4240+selected*16+4),signedPhase(u32(image.data()+4444)),pitch,random);
  check(result!=Voice246ReleaseDispatchResult::rejected,"Valid bounded dispatch rejected");
  if(note>=128){++filtered;check(result==Voice246ReleaseDispatchResult::filtered,"Signed32 note filter differs");}
  else{
   check(result!=Voice246ReleaseDispatchResult::filtered,"Valid note filtered");notes[mode][argument[8]?1:0][note]=true;
   if(argument[8])++nonzero;else ++zero;
   if(result==Voice246ReleaseDispatchResult::removed)++removed;else ++absent;
  }
  retunes+=oldPitch!=pitch;bool changed=false;
  for(size_t i=0;i<6;++i){changed|=envelopes[i].stage!=oldEnvelopes[i].stage||envelopes[i].blocks!=oldEnvelopes[i].blocks;saveState(image.data()+2580+i*24,envelopes[i]);put(image.data()+4136+i*4,uint32_t(pitch[i]));}
  envelopeChanges+=changed;put(image.data()+4368,random);std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  if(std::memcmp(image.data(),e,imageBytes)!=0){
   for(size_t i=0;i<imageBytes;++i)if(image[i]!=e[i]){std::cerr<<"record "<<c<<", image byte "<<i<<", actual "<<unsigned(image[i])<<", oracle "<<unsigned(e[i])<<'\n';break;}
   throw std::runtime_error("Full dispatcher list/pools/registers/map/transport image differs");
  }
  check(std::memcmp(p+8,e+imageBytes,32)==0,"Source argument or its guards changed");
  check(std::memcmp(argument.data(),e+imageBytes+8,16)==0,"Source metadata was erased/changed");
  if(std::memcmp(local.data(),e+imageBytes+32,16)!=0)throw std::runtime_error("Original local field copy/padding differs at record "+std::to_string(c));
 }
 for(const auto& mode:notes)for(const auto& path:mode)for(bool seen:path)check(seen,"Missing note/selector/metadata path");
 for(const auto& mode:lengths)for(bool seen:mode)check(seen,"Missing selector/list length");
 for(size_t value:{size_t(0),size_t(1),size_t(2),size_t(127),size_t(128),size_t(255)})check(metadata[value],"Missing raw metadata byte");
 check(filtered==80&&nonzero>600&&zero>600&&removed>200&&retunes>0&&envelopeChanges>0,"Insufficient release/retune/filter coverage");
 // Rejections are adapter policy; no original assert/transport is emulated.
 Voice246NoteArgument argument{},local{};local.fill(0xa5);const auto oldLocal=local;
 Voice246ReleaseList list;std::array<uint8_t,128> map{};std::array<int32_t,6> pitch{};uint32_t random=123;
 map[7]=1;
 check(voice246ReleaseDispatch(argument,0,map,local,list,0,envelopes,0,0,pitch,random)==Voice246ReleaseDispatchResult::rejected&&local==oldLocal&&random==123,"Active-map guard changed state");
 map.fill(0);
 check(voice246ReleaseDispatch(argument,1,map,local,list,0,envelopes,0,0,pitch,random)==Voice246ReleaseDispatchResult::rejected&&local==oldLocal,"Other discriminator accepted");
 put(argument.data(),0xffffffffu);map[0]=1;
 check(voice246ReleaseDispatch(argument,0,map,local,list,0,envelopes,0,0,pitch,random)==Voice246ReleaseDispatchResult::filtered&&local==oldLocal,"Filter consulted later map context");
 std::cout<<"PASS: 1,680 original release dispatches, "<<filtered<<" signed32 filtered, "<<removed<<" removals / "<<absent<<" absent/disabled; "
          <<zero<<" zero / "<<nonzero<<" nonzero metadata; full local/source arguments, 4,656 shared bytes, empty map and PC-guarded transport/error/L1\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
