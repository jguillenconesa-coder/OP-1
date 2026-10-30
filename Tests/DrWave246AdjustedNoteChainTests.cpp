// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246ReleaseSelectors.h"
#include "Voice246NoteArgument.h"
#include "Voice246NoteAdjustment.h"
#include "DrWave246Activation.h"
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
 check(argc==2,"Expected fixture directory");const std::string d=argv[1];auto in=read(d+"/drwave246-adjusted-note-chain-input.bin"),out=read(d+"/drwave246-adjusted-note-chain-output.bin");
 check(in.size()==768*4836&&out.size()==768*9412+1024,"Wrong release dimensions");
 constexpr std::array<size_t,15> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476}};
 constexpr std::array<size_t,15> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,48}};
 std::array<uint8_t,4524> image{};std::array<Adsr246State,6> pool{};
 size_t activations=0,accepted=0,blocked=0,releases=0,removed=0,written=0,bypass=0,lookup=0;
 std::array<std::array<bool,128>,2> notes{};std::array<bool,5> selectors{};std::array<std::array<bool,9>,8> setterPairs{};size_t setterCalls=0;
 for(size_t i=0;i<128;++i)check(u32(out.data()+i*4)==uint32_t(voice246NotePitch[i]),"Original note table differs");
 check(std::memcmp(out.data(),out.data()+out.size()-512,512)==0,"Note table changed");
 for(size_t c=0;c<768;++c){const auto* p=in.data()+c*4836;const auto* e=out.data()+512+c*9412;
  for(size_t j=0;j<15;++j)if(u32(p)||!(j==0||j==1||j==2||j==3||j==5||j==6||j==14))std::memcpy(image.data()+offsets[j],p+40+offsets[j],sizes[j]);
  for(size_t j=0;j<6;++j)pool[j]=state(image.data()+2580+j*24);
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=u32(image.data()+2568);
  const auto selected=u32(image.data()+4392);check(selected<8,"Invalid selected sound");
  Voice246NoteArgument argument{};std::memcpy(argument.data(),p+16,16);
  std::array<uint32_t,8> adjustments{};for(size_t i=0;i<8;++i)adjustments[i]=u32(image.data()+4484+i*4);
  if(u32(p)){
   const auto initial=adjustments;adjustments.fill(0xa5a5a5a5u);
   for(size_t i=0;i<8;++i){check(voice246SetNoteAdjustment(adjustments,int32_t(i),signedPhase(initial[i])),"Invalid initial setter");++setterCalls;}
  }
  check(voice246SetNoteAdjustment(adjustments,signedPhase(u32(p+32)),signedPhase(u32(p+36))),"Invalid setter change");
  ++setterCalls;setterPairs[u32(p+32)][size_t(signedPhase(u32(p+36))+4)]=true;
  for(size_t i=0;i<8;++i)put(image.data()+4484+i*4,adjustments[i]);
  check(std::memcmp(image.data()+4476,e+9324,48)==0,"Guarded table after setters differs");
  const auto discriminator=u32(image.data()+4468);if(discriminator==1)++bypass;else ++lookup;
  check(voice246ConstructNoteArgument(argument,u32(p+16),u32(p+20),discriminator,selected,adjustments),"Constructor rejected linked context");
  const auto kind=u32(p+12);check(kind<2&&u32(argument.data())<128,"Invalid linked note/action");notes[kind][u32(argument.data())]=true;
  const auto mode=s16(image.data()+4240+selected*16+8);const auto selector=int(floorDivide(mode,4096));check(selector>=0&&selector<=4,"Invalid selector");selectors[size_t(selector)]=true;
  auto random=u32(image.data()+4368);std::array<int32_t,6> pitch{};for(size_t i=0;i<6;++i)pitch[i]=signedPhase(u32(image.data()+4136+i*4));
  if(!kind){
   ++activations;DrWave246Pool waves{};std::memcpy(waves.data(),image.data()+3240,216);
   Voice246Registers registers;registers.pitch=pitch;
   for(size_t i=0;i<6;++i){registers.active[i]=image[3504+i];registers.velocity[i]=s16(image.data()+3512+i*2);}
   const Voice246Note note{u32(argument.data()),u32(argument.data()+4),u32(argument.data()+8),signedPhase(u32(argument.data()+12))};
   const bool result=drWave246ActivateNote(waves,registers,note,signedPhase(u32(image.data()+4444)),int(u32(image.data()+4400)),int(selected),int(u32(p+4)));
   check(result==(u32(image.data()+4400)==selected),"Core acceptance differs");if(result)++accepted;else ++blocked;
   std::memcpy(image.data()+3240,waves.data(),216);pitch=registers.pitch;
   for(size_t i=0;i<6;++i){image[3504+i]=registers.active[i];const auto velocity=uint16_t(registers.velocity[i]);image[3512+i*2]=uint8_t(velocity);image[3513+i*2]=uint8_t(velocity>>8);}
  }else{
   ++releases;
   const auto sustain=s16(image.data()+4240+selected*16+4);
   const auto result=selector==0?voice246PolyRelease(list,u32(argument.data()),pool,sustain):voice246ReleaseSelectors(list,u32(argument.data()),mode,pool,sustain,signedPhase(u32(image.data()+4444)),pitch,random);
   check(result!=Voice246ReleaseResult::rejected,"Valid release rejected");removed+=result==Voice246ReleaseResult::removed;
  }
  for(size_t i=0;i<6;++i)put(image.data()+4136+i*4,uint32_t(pitch[i]));put(image.data()+4368,random);
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  for(size_t j=0;j<6;++j)save(image.data()+2580+j*24,pool[j]);
  if(std::memcmp(image.data(),e,4524)!=0){for(size_t k=0;k<4524;++k)if(image[k]!=e[k]){std::cerr<<"offset "<<k<<" actual "<<unsigned(image[k])<<" oracle "<<unsigned(e[k])<<"\n";break;}}
  if(std::memcmp(image.data(),e,4524)!=0)throw std::runtime_error("Full callback image differs at case "+std::to_string(c));
  const auto v=u32(p+4);check(v<6,"Invalid explicit render voice");
  std::array<int16_t,128> audio{};for(size_t i=0;i<128;++i)audio[i]=s16(p+4572+i*2);
  Adsr246Noise noise{u32(image.data()+4368),signedPhase(u32(image.data()+4376))};auto scratch=noise;
  for(size_t i=0;i<128;++i)put(image.data()+2724+i*4,uint32_t(scratch.next()));
  std::array<int16_t,4> controls{};for(size_t j=0;j<4;++j)controls[j]=s16(image.data()+4240+selected*16+j*2);
  if(pool[v].stage>=1&&pool[v].stage<=8)written+=128;
  adsr246Block(pool[v],controls,s16(p+8),noise,audio);save(image.data()+2580+v*24,pool[v]);put(image.data()+4368,noise.seed);put(image.data()+4376,uint32_t(noise.previous));
  if(std::memcmp(image.data(),e+4524,4524)!=0)throw std::runtime_error("Full post-render image differs at case "+std::to_string(c));
  check(std::memcmp(p+4564,e+9048,8)==0&&std::memcmp(p+4828,e+9312,8)==0,"Output guard changed");
  for(size_t i=0;i<128;++i)check(audio[i]==s16(e+9056+i*2),"Post-release ADSR sample differs");
  check(std::memcmp(p,e+9372,16)==0&&std::memcmp(argument.data(),e+9388,16)==0&&std::memcmp(p+32,e+9404,8)==0,"Constructed argument/caller inputs differ");
  check(pool[v].finished==(u32(e+9320)!=0),"Completion flag differs");
 }
 for(const auto& action:notes)for(bool b:action)check(b,"Missing constructed note/action combination");for(bool b:selectors)check(b,"Missing release selector");
 for(const auto& slot:setterPairs)for(bool b:slot)check(b,"Missing slot/adjustment pair");check(setterCalls==4896,"Wrong original setter schedule");
 check(accepted>=128&&blocked>20&&removed>40&&bypass>100&&lookup>100,"Insufficient linked coverage");
 std::cout<<"PASS: "<<setterCalls<<" setters; "<<activations<<" activations after original adjustment setters ("<<accepted<<" accepted / "<<blocked<<" blocked), "<<releases<<" release calls, "<<removed<<" removals; "<<written<<" written / "<<98304-written<<" preserved ADSR positions; complete images, arguments and guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
