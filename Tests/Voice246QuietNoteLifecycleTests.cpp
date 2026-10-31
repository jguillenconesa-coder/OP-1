// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246QuietNoteDispatch.h"
#include "DrWave246HeldNoteModes.h"
#include "Voice246ReleaseDispatch.h"
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
std::vector<uint8_t> read(const std::string& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Missing lifecycle fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){return int16_t(u16(p)<32768?int(u16(p)):int(u16(p))-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put(uint8_t* p,uint32_t v){for(unsigned j=0;j<4;++j)p[j]=uint8_t(v>>(j*8));}
Adsr246State state(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void save(uint8_t* p,const Adsr246State& s){const std::array<uint32_t,6> w{{s.stage,s.blocks,uint32_t(s.level),s.field12,s.field16,s.gain}};for(size_t j=0;j<6;++j)put(p+j*4,w[j]);}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string d=argv[1];auto in=read(d+"/voice246-quiet-note-lifecycle-input.bin"),out=read(d+"/voice246-quiet-note-lifecycle-output.bin");
 check(in.size()==1536*5072&&out.size()==1536*9900+1024,"Wrong lifecycle dimensions");
 constexpr std::array<size_t,21> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476,4524,4556,4568,4704,4744,4748}};
 constexpr std::array<size_t,21> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,48,32,12,136,40,4,12}};
 std::array<uint8_t,4760> image{};std::array<Adsr246State,6> pool{};
 size_t appended=0,gated=0,duplicates=0,removed=0,absent=0,emptyOn=0,heldOn=0,mapChanges=0,written=0,setterCalls=0;
 std::array<std::array<bool,128>,3> notes{};std::array<bool,6> voices{};
 for(size_t i=0;i<128;++i)check(u32(out.data()+i*4)==uint32_t(voice246NotePitch[i]),"Original note table differs");
 check(std::memcmp(out.data(),out.data()+out.size()-512,512)==0,"Note table changed");
 for(size_t c=0;c<1536;++c){const auto* p=in.data()+c*5072;const auto* e=out.data()+512+c*9900;
  for(size_t j=0;j<21;++j)if(u32(p)||!(j==0||j==1||j==2||j==3||j==5||j==6||j==14||j>=17))std::memcpy(image.data()+offsets[j],p+40+offsets[j],sizes[j]);
  image[4]=p[44]; // Explicit event gate; release requires zero.
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
  check(std::memcmp(image.data()+4476,e+9796,48)==0,"Guarded table after setters differs");
  const auto discriminator=u32(image.data()+4468);
  check(voice246ConstructNoteArgument(argument,u32(p+16),u32(p+20),discriminator,selected,adjustments),"Constructor rejected linked context");
  check(u32(argument.data())<128,"Invalid constructed note");
  const int mode=int(floorDivide(s16(image.data()+4240+selected*16+8),4096));
  check(mode>=1&&mode<=3,"Invalid lifecycle selector");
  DrWave246Pool waves{};std::memcpy(waves.data(),image.data()+3240,216);
  Voice246Registers registers;for(size_t i=0;i<6;++i){registers.active[i]=image[3504+i];registers.velocity[i]=s16(image.data()+3512+i*2);registers.pitch[i]=signedPhase(u32(image.data()+4136+i*4));}
  const Voice246Note note{u32(argument.data()),u32(argument.data()+4),u32(argument.data()+8),signedPhase(u32(argument.data()+12))};
  auto random=u32(image.data()+4368);
  std::array<uint8_t,128> map{};std::memcpy(map.data(),image.data()+4572,128);const auto oldMap=map;
  Voice246NoteArgument local{};put(local.data(),0x11223344);put(local.data()+4,0xdeadbeef);put(local.data()+8,0x55667788);put(local.data()+12,0xa5a5a5a5);
  check(argument[8]==1&&discriminator==0&&u32(image.data()+4752)==0,"Unbounded notification context");
  if(u32(p+12)==0){
   notes[size_t(mode-1)][note.noteIndex]=true;
   if(list.count==0)++emptyOn;else ++heldOn;
   const auto result=voice246QuietNoteOn(argument,local,image[4481],map,u32(image.data()+4752),waves,registers,pool,list,image[4],int(u32(image.data()+4400)),int(selected),s16(image.data()+4240+selected*16+8),signedPhase(u32(image.data()+4444)),random);
   using S=Voice246NoteDispatchResult;check(result!=S::rejected,"Quiet on rejected");
   if(result==S::appended)++appended;else if(result==S::gated)++gated;else ++duplicates;
   for(unsigned v=0;v<(mode==3?6u:1u);++v)voices[v]=true;
  }else{
   const auto result=voice246QuietNoteOff(argument,local,map,u32(image.data()+4752),list,s16(image.data()+4240+selected*16+8),pool,s16(image.data()+4240+selected*16+4),signedPhase(u32(image.data()+4444)),registers.pitch,random);
   check(result!=Voice246ReleaseResult::rejected,"Quiet off rejected");
   if(result==Voice246ReleaseResult::removed)++removed;else ++absent;
  }
  for(size_t j=0;j<128;++j)mapChanges+=map[j]!=oldMap[j];
  std::memcpy(image.data()+4572,map.data(),128);put(image.data()+4368,random);
  std::memcpy(image.data()+3240,waves.data(),216);
  for(size_t i=0;i<6;++i){image[3504+i]=registers.active[i];const auto v=uint16_t(registers.velocity[i]);image[3512+i*2]=uint8_t(v);image[3513+i*2]=uint8_t(v>>8);put(image.data()+4136+i*4,uint32_t(registers.pitch[i]));}
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  for(size_t j=0;j<6;++j)save(image.data()+2580+j*24,pool[j]);
  if(std::memcmp(image.data(),e,4760)!=0){for(size_t k=0;k<4760;++k)if(image[k]!=e[k]){std::cerr<<"offset "<<k<<" actual "<<unsigned(image[k])<<" oracle "<<unsigned(e[k])<<"\n";break;}}
  if(std::memcmp(image.data(),e,4760)!=0)throw std::runtime_error("Full callback image differs at case "+std::to_string(c));
  if(c%8==7)check(list.count==0,"Cycle did not return to empty list");
  const auto v=u32(p+4);check(v<6,"Invalid explicit render voice");
  std::array<int16_t,128> audio{};for(size_t i=0;i<128;++i)audio[i]=s16(p+4808+i*2);
  Adsr246Noise noise{u32(image.data()+4368),signedPhase(u32(image.data()+4376))};auto scratch=noise;
  for(size_t i=0;i<128;++i)put(image.data()+2724+i*4,uint32_t(scratch.next()));
  std::array<int16_t,4> controls{};for(size_t j=0;j<4;++j)controls[j]=s16(image.data()+4240+selected*16+j*2);
  if(pool[v].stage>=1&&pool[v].stage<=8)written+=128;
  adsr246Block(pool[v],controls,s16(p+8),noise,audio);save(image.data()+2580+v*24,pool[v]);put(image.data()+4368,noise.seed);put(image.data()+4376,uint32_t(noise.previous));
  if(std::memcmp(image.data(),e+4760,4760)!=0)throw std::runtime_error("Full post-render image differs at case "+std::to_string(c));
  check(std::memcmp(p+4800,e+9520,8)==0&&std::memcmp(p+5064,e+9784,8)==0,"Output guard changed");
  for(size_t i=0;i<128;++i)check(audio[i]==s16(e+9528+i*2),"Post-event ADSR sample differs");
  check(std::memcmp(p,e+9844,16)==0&&std::memcmp(argument.data(),e+9860,16)==0&&std::memcmp(p+32,e+9876,8)==0,"Constructed argument/caller inputs differ");
  check(std::memcmp(local.data(),e+9884,16)==0,"Complete event-local argument copy differs");
  check(pool[v].finished==(u32(e+9792)!=0),"Completion flag differs");
 }
 for(const auto& row:notes)for(bool b:row)check(b,"Missing note/selector coverage");for(bool b:voices)check(b,"Missing voice");
 check(appended==432&&gated==192&&duplicates==144&&removed==432&&absent==336&&emptyOn==336&&heldOn==432&&setterCalls==1632&&mapChanges>500,"Missing quiet lifecycle coverage");
 std::cout<<"PASS: "<<appended<<" appends / "<<gated<<" gated / "<<duplicates<<" duplicates, "<<removed<<" removals / "<<absent<<" absent; "<<mapChanges<<" map byte changes; "<<written<<" written ADSR positions; metadata 1, retained state, full local copies and guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
