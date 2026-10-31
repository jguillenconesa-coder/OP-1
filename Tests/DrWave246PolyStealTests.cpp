// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246PolySteal.h"
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
 check(argc==2,"Expected fixture directory");const std::string d=argv[1];auto in=read(d+"/drwave246-poly-steal-input.bin"),out=read(d+"/drwave246-poly-steal-output.bin");
 check(in.size()==768*4880&&out.size()==768*9500+1024,"Wrong release dimensions");
 constexpr std::array<size_t,17> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476,4524,4556}};
 constexpr std::array<size_t,17> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,48,32,12}};
 std::array<uint8_t,4568> image{};std::array<Adsr246State,6> pool{};
 size_t appended=0,gated=0,duplicates=0,written=0,setterCalls=0;
 std::array<bool,128> acceptedNotes{};std::array<bool,6> voices{};
 for(size_t i=0;i<128;++i)check(u32(out.data()+i*4)==uint32_t(voice246NotePitch[i]),"Original note table differs");
 check(std::memcmp(out.data(),out.data()+out.size()-512,512)==0,"Note table changed");
 for(size_t c=0;c<768;++c){const auto* p=in.data()+c*4880;const auto* e=out.data()+512+c*9500;
  for(size_t j=0;j<17;++j)if(u32(p)||!(j==0||j==1||j==2||j==3||j==5||j==6||j==14))std::memcpy(image.data()+offsets[j],p+40+offsets[j],sizes[j]);
  for(size_t j=0;j<6;++j)image[3504+j]=p[40+3504+j];
  image[4]=p[44];for(size_t j=0;j<6;++j)put(image.data()+2584+j*24,u32(p+40+2584+j*24));
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
  ++setterCalls;
  for(size_t i=0;i<8;++i)put(image.data()+4484+i*4,adjustments[i]);
  check(std::memcmp(image.data()+4476,e+9412,48)==0,"Guarded table after setters differs");
  const auto discriminator=u32(image.data()+4468);
  check(voice246ConstructNoteArgument(argument,u32(p+16),u32(p+20),discriminator,selected,adjustments),"Constructor rejected linked context");
  check(u32(argument.data())<128,"Invalid constructed note");
  check(s16(image.data()+4240+selected*16+8)/4096==0,"Unsupported selector in fixture");
  Voice246Registers registers;for(size_t i=0;i<6;++i){registers.active[i]=image[3504+i];registers.velocity[i]=s16(image.data()+3512+i*2);registers.pitch[i]=signedPhase(u32(image.data()+4136+i*4));}
  const Voice246Note note{u32(argument.data()),u32(argument.data()+4),u32(argument.data()+8),signedPhase(u32(argument.data()+12))};
  const auto result=drWave246PolySteal(registers,pool,list,note,image[4],signedPhase(u32(image.data()+4444)));
  using S=DrWave246PolyStealStatus;check(result.status!=S::unsupported,"Unsupported fixture reached");
  if(result.status==S::gated)++gated;else if(result.status==S::duplicate)++duplicates;else{
   ++appended;check(result.voice>=0&&result.voice<6,"Invalid selected voice");voices[size_t(result.voice)]=true;
   acceptedNotes[note.noteIndex]=true;
  }
  for(size_t i=0;i<6;++i){image[3504+i]=registers.active[i];const auto v=uint16_t(registers.velocity[i]);image[3512+i*2]=uint8_t(v);image[3513+i*2]=uint8_t(v>>8);put(image.data()+4136+i*4,uint32_t(registers.pitch[i]));}
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  for(size_t j=0;j<6;++j)save(image.data()+2580+j*24,pool[j]);
  if(std::memcmp(image.data(),e,4568)!=0){for(size_t k=0;k<4568;++k)if(image[k]!=e[k]){std::cerr<<"offset "<<k<<" actual "<<unsigned(image[k])<<" oracle "<<unsigned(e[k])<<"\n";break;}}
  if(std::memcmp(image.data(),e,4568)!=0)throw std::runtime_error("Full callback image differs at case "+std::to_string(c));
  const auto v=u32(p+4);check(v<6,"Invalid explicit render voice");
  std::array<int16_t,128> audio{};for(size_t i=0;i<128;++i)audio[i]=s16(p+4616+i*2);
  Adsr246Noise noise{u32(image.data()+4368),signedPhase(u32(image.data()+4376))};auto scratch=noise;
  for(size_t i=0;i<128;++i)put(image.data()+2724+i*4,uint32_t(scratch.next()));
  std::array<int16_t,4> controls{};for(size_t j=0;j<4;++j)controls[j]=s16(image.data()+4240+selected*16+j*2);
  if(pool[v].stage>=1&&pool[v].stage<=8)written+=128;
  adsr246Block(pool[v],controls,s16(p+8),noise,audio);save(image.data()+2580+v*24,pool[v]);put(image.data()+4368,noise.seed);put(image.data()+4376,uint32_t(noise.previous));
  if(std::memcmp(image.data(),e+4568,4568)!=0)throw std::runtime_error("Full post-render image differs at case "+std::to_string(c));
  check(std::memcmp(p+4608,e+9136,8)==0&&std::memcmp(p+4872,e+9400,8)==0,"Output guard changed");
  for(size_t i=0;i<128;++i)check(audio[i]==s16(e+9144+i*2),"Post-release ADSR sample differs");
  check(std::memcmp(p,e+9460,16)==0&&std::memcmp(argument.data(),e+9476,16)==0&&std::memcmp(p+32,e+9492,8)==0,"Constructed argument/caller inputs differ");
  check(pool[v].finished==(u32(e+9408)!=0),"Completion flag differs");
 }
 for(bool b:acceptedNotes)check(b,"Missing accepted note");for(bool b:voices)check(b,"Missing stolen voice");
 check(appended==320&&gated==256&&duplicates==192&&setterCalls==4896,"Missing poly coverage");
 std::cout<<"PASS: "<<appended<<" busy-voice appends, "<<gated<<" gated and "<<duplicates<<" duplicates; "<<written<<" written / "<<98304-written<<" preserved ADSR positions; full pending fields/list/registers/pools/guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
