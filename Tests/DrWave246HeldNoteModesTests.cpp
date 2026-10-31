// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246HeldNoteModes.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
constexpr size_t imageBytes=4568,recordBytes=4608,resultBytes=4600,cases=1536;
constexpr std::array<size_t,17> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476,4524,4556}};
constexpr std::array<size_t,17> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,48,32,12}};
constexpr std::array<bool,17> retained{{true,true,true,true,false,true,true,false,false,false,false,false,false,false,true,false,false}};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){const auto v=u16(p);return int16_t(v<32768?int(v):int(v)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put(uint8_t* p,uint32_t v){for(unsigned j=0;j<4;++j)p[j]=uint8_t(v>>(j*8));}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing held-note fixture");return {std::istreambuf_iterator<char>(f),{}};}
Adsr246State state(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void save(uint8_t* p,const Adsr246State& s){const std::array<uint32_t,6> words{{s.stage,s.blocks,uint32_t(s.level),s.field12,s.field16,s.gain}};for(size_t j=0;j<6;++j)put(p+j*4,words[j]);}
void adapterGuards(){
 Voice246Registers registers;std::array<Adsr246State,6> pool{};Voice246ReleaseList list;Voice246NoteArgument argument{};
 uint32_t random=0x12345678;using S=DrWave246HeldNoteModesStatus;
 const auto call=[&](int16_t selector,uint8_t gate=0){return drWave246HeldNoteModes(registers,pool,list,argument,gate,selector,0x7fffffff,random).status;};
 check(call(4096)==S::unsupported,"Empty list must stay outside reserved context");
 list.count=129;check(call(4096)==S::unsupported,"Capacity guard failed");
 list.count=1;put(list.bytes.data(),77);put(argument.data(),60);
 check(call(0)==S::unsupported&&call(16384)==S::unsupported&&call(-1)==S::unsupported,"Mode guard failed");
 list.count=128;check(call(4096)==S::unsupported,"New event at capacity must be rejected before mutation");
 list.count=1;put(argument.data(),128);put(argument.data()+12,1);
 check(call(4096)==S::unsupported,"Unverified gain/note range accepted");
 check(random==0x12345678&&registers.pitch[0]==0&&pool[0].stage==0&&list.count==1,"Own guard mutated state");
 check(call(4096,255)==S::gated,"Gate must precede note lookup");
 put(list.bytes.data(),128);check(call(4096)==S::duplicate,"Duplicate must precede note lookup");
}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string directory=argv[1];
 const auto input=read(directory+"/drwave246-held-note-modes-input.bin");
 const auto output=read(directory+"/drwave246-held-note-modes-output.bin");
 check(input.size()==cases*recordBytes&&output.size()==cases*resultBytes+1024,"Wrong held-note fixture dimensions");
 for(size_t i=0;i<128;++i)check(u32(output.data()+i*4)==uint32_t(voice246NotePitch[i]),"Note table differs");
 check(std::memcmp(output.data(),output.data()+output.size()-512,512)==0,"Note table changed");
 for(size_t c=0;c<256;++c){
  const auto* a=input.data()+c*recordBytes;const auto selected=u32(a+40+4392);check(selected<8,"Invalid paired selector");
  const size_t control=40+4240+selected*16+8;
  for(size_t s=1;s<3;++s){const auto* b=input.data()+(c+s*256)*recordBytes;
   for(size_t k=0;k<recordBytes;++k)if(k!=control&&k!=control+1)check(a[k]==b[k],"Selector triple inputs differ outside selected word");}
  const auto* first=output.data()+512+c*resultBytes;
  const auto* second=first+256*resultBytes;const auto* third=second+256*resultBytes;
  check(std::memcmp(first,second,2576)==0&&std::memcmp(first,third,2576)==0,"Paired append/list/padding differ");
  check(u32(first+4368)==u32(second+4368)&&u32(first+4368)!=u32(third+4368),"One-versus-six RNG transitions differ");
  check(std::memcmp(first+2580,third+2580,24)==0,"Paired voice-zero restart differs");
  check(std::memcmp(second+2580,input.data()+(c+256)*recordBytes+40+2580,144)==0,"Selector 2 changed ADSR pool");
 }
 std::array<uint8_t,imageBytes> image{};std::array<Adsr246State,6> pool{};
 std::array<std::array<bool,128>,3> notes{};std::array<bool,129> lengths{};
 std::array<size_t,3> appends{};size_t gated=0,duplicates=0,rngSteps=0;
 for(size_t c=0;c<cases;++c){
  const auto* p=input.data()+c*recordBytes;const auto* e=output.data()+512+c*resultBytes;
  for(size_t j=0;j<17;++j)if(u32(p)||!retained[j])std::memcpy(image.data()+offsets[j],p+40+offsets[j],sizes[j]);
  image[4]=p[44];std::memcpy(image.data()+3504,p+3544,6);
  const auto n=u32(image.data()+2568);check(n>=1&&n<=128,"Entry list is empty or oversized");lengths[n]=true;
  const auto selected=u32(image.data()+4392);check(selected<8,"Invalid explicit selected sound");
  const auto mode=s16(image.data()+4240+selected*16+8);const auto selector=int(floorDivide(mode,4096));
  check(selector>=1&&selector<=3&&u32(image.data()+3468)==0&&u32(image.data()+2576)==1,"Unbounded fixture context");
  for(size_t j=0;j<6;++j){pool[j]=state(image.data()+2580+j*24);check(u32(image.data()+4528+j*4)==0,"Auxiliary module present");}
  check(u32(image.data()+4560)==0,"Auxiliary module present");
  Voice246Registers registers;
  for(size_t j=0;j<6;++j){registers.active[j]=image[3504+j];registers.velocity[j]=s16(image.data()+3512+j*2);registers.pitch[j]=signedPhase(u32(image.data()+4136+j*4));}
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=n;
  Voice246NoteArgument argument;std::memcpy(argument.data(),p+16,16);const auto beforeArgument=argument;
  auto random=u32(image.data()+4368);const auto oldRandom=random;
  const auto result=drWave246HeldNoteModes(registers,pool,list,argument,image[4],mode,signedPhase(u32(image.data()+4444)),random);
  using S=DrWave246HeldNoteModesStatus;check(result.status!=S::unsupported,"Unsupported fixture reached");
  const auto kind=u32(p+4);
  if(result.status==S::gated){++gated;check(kind==1&&random==oldRandom,"Gate ordering/RNG differs");}
  else if(result.status==S::duplicate){++duplicates;check(kind==2&&random==oldRandom,"Duplicate ordering/RNG differs");}
  else {
   check(kind==0&&u32(argument.data())<128,"Invalid accepted fixture");++appends[size_t(selector-1)];
   notes[size_t(selector-1)][u32(argument.data())]=true;rngSteps+=result.voices;
   check(result.voices==(selector==3?6u:1u),"Incorrect loop bound");
   check(list.count==n+1&&u32(list.bytes.data()+n*20+16)==0xfffffffeu,"List marker/count differs");
   for(size_t j=9;j<12;++j)check(list.bytes[n*20+j]==image[8+n*20+j],"Destination padding changed");
  }
  check(argument==beforeArgument&&std::memcmp(p+8,e+imageBytes,32)==0,"Argument/padding/guards changed");
  for(size_t j=0;j<6;++j){image[3504+j]=registers.active[j];const auto v=uint16_t(registers.velocity[j]);image[3512+j*2]=uint8_t(v);image[3513+j*2]=uint8_t(v>>8);put(image.data()+4136+j*4,uint32_t(registers.pitch[j]));save(image.data()+2580+j*24,pool[j]);}
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);put(image.data()+4368,random);
  if(std::memcmp(image.data(),e,imageBytes)!=0){
   for(size_t k=0;k<imageBytes;++k)if(image[k]!=e[k]){std::cerr<<"case "<<c<<" offset "<<k<<" adapter "<<unsigned(image[k])<<" original "<<unsigned(e[k])<<'\n';break;}
   throw std::runtime_error("Complete shared image differs");
  }
 }
 for(const auto& mode:notes)for(bool accepted:mode)check(accepted,"Missing note per selector");
 for(size_t i=1;i<=128;++i)check(lengths[i],"Missing entry-list length");
 check(appends[0]+appends[1]+appends[2]==1056&&gated==240&&duplicates==240,"Coverage totals differ");
 check(rngSteps==appends[0]+appends[1]+appends[2]*6,"RNG coverage differs");
 adapterGuards();
 std::cout<<"PASS: held-note appends "<<appends[0]<<'/'<<appends[1]<<'/'<<appends[2]<<", gated "<<gated<<", duplicate "<<duplicates<<"; "<<rngSteps<<" RNG steps; full list/registers/ADSR/pending/gain/objects/guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
