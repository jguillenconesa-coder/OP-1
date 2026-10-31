// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246EmptyExplicitPitch.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
constexpr size_t cases=3462,probes=497,imageBytes=4568,tableOffset=4608,recordBytes=5120,resultBytes=5112;
constexpr std::array<size_t,17> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476,4524,4556}};
constexpr std::array<size_t,17> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,48,32,12}};
constexpr std::array<bool,17> retained{{true,true,true,true,false,true,true,false,false,false,false,false,false,false,true,false,false}};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto v=u16(p);return int16_t(v<32768?int(v):int(v)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put(uint8_t* p,uint32_t v){for(unsigned j=0;j<4;++j)p[j]=uint8_t(v>>(j*8));}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing empty explicit-pitch fixture");return {std::istreambuf_iterator<char>(f),{}};}
Adsr246State state(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void save(uint8_t* p,const Adsr246State& s){const std::array<uint32_t,6> words{{s.stage,s.blocks,uint32_t(s.level),s.field12,s.field16,s.gain}};for(size_t j=0;j<6;++j)put(p+j*4,words[j]);}
DrWave246State wave(const uint8_t* p){DrWave246State w;w.tag=u32(p);w.field4=u32(p+4);for(size_t j=0;j<5;++j)w.phase[j]=u32(p+8+j*4);w.slot=u32(p+28);w.field32=u32(p+32);return w;}
void save(uint8_t* p,const DrWave246State& w){put(p,w.tag);put(p+4,w.field4);for(size_t j=0;j<5;++j)put(p+8+j*4,w.phase[j]);put(p+28,w.slot);put(p+32,w.field32);}
void guards(){
 DrWave246Pool waves{};Voice246Registers registers;std::array<Adsr246State,6> pool{};
 Voice246ReleaseList list;Voice246NoteArgument argument{};uint32_t random=123;
 using S=DrWave246EmptyExplicitPitchStatus;
 const auto call=[&](int16_t mode=4096,int loaded=0,int selected=0){return drWave246EmptyExplicitPitch(waves,registers,pool,list,argument,0,loaded,selected,mode,0x7fffffff,random).status;};
 check(call()==S::unsupported,"Zero explicit pitch is outside this adapter");
 put(argument.data()+12,1);list.count=1;check(call()==S::unsupported,"Held context is excluded");list.count=0;
 put(argument.data(),128);check(call()==S::unsupported,"Unverified gain input accepted");put(argument.data(),0);
 check(call(0)==S::unsupported&&call(16384)==S::unsupported&&call(-1)==S::unsupported,"Invalid selector accepted");
 check(call(4096,-1)==S::unsupported&&call(4096,0,8)==S::unsupported,"Invalid slots accepted");
 check(random==123&&list.count==0&&pool[0].stage==0&&registers.active[0]==0,"Own guard mutated state");
}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string directory=argv[1];
 const auto input=read(directory+"/drwave246-empty-explicit-pitch-input.bin"),output=read(directory+"/drwave246-empty-explicit-pitch-output.bin");
 check(input.size()==cases*recordBytes&&output.size()==cases*resultBytes,"Wrong explicit-pitch dimensions");
 for(size_t p=0;p<probes;++p){
  const auto* first=input.data()+p*6*recordBytes;const auto selected=u32(first+40+4392);check(selected<8,"Invalid paired slot");
  const size_t control=40+4240+selected*16+8;
  for(size_t j=1;j<6;++j){const auto* other=first+j*recordBytes;
   for(size_t k=0;k<tableOffset;++k)if(k!=control&&k!=control+1)check(first[k]==other[k],"Controlled matrix changes other inputs");}
  for(size_t mode=0;mode<3;++mode){
   const auto c=p*6+mode*2;const auto* a=output.data()+c*resultBytes;const auto* b=a+resultBytes;
   check(std::memcmp(input.data()+c*recordBytes+tableOffset,input.data()+(c+1)*recordBytes+tableOffset,512)!=0,"Lookup pair identical");
   check(std::memcmp(a+4,b+4,imageBytes-4)==0&&std::memcmp(a+imageBytes,b+imageBytes,32)==0,"Explicit result depends on lookup table");
  }
  const auto* a=output.data()+p*6*resultBytes;const auto* b=a+2*resultBytes;
  const size_t modeOffset=control-40;
  for(size_t k=0;k<resultBytes;++k)if(k!=modeOffset&&k!=modeOffset+1)check(a[k]==b[k],"Empty selectors1/2 differ beyond control word");
 }
 std::array<uint8_t,imageBytes> image{};DrWave246Pool waves{};std::array<Adsr246State,6> pool{};
 size_t appended=0,gated=0,started=0,restarted=0,blocked=0,positive=0,negative=0;
 std::set<uint32_t> pitches,tunings;std::array<bool,64> masks{};
 for(size_t c=0;c<cases;++c){
  const auto* p=input.data()+c*recordBytes;const auto* e=output.data()+c*resultBytes;
  for(size_t j=0;j<17;++j)if(u32(p)||!retained[j])std::memcpy(image.data()+offsets[j],p+40+offsets[j],sizes[j]);
  image[4]=p[44];std::memcpy(image.data()+3504,p+3544,6);put(image.data()+2568,0);
  std::memcpy(image.data(),p+tableOffset+508,4);
  const auto selected=u32(image.data()+4392),loaded=u32(image.data()+4400);check(selected<8&&loaded<8,"Unbounded slots");
  const auto mode=s16(image.data()+4240+selected*16+8);const auto selector=floorDivide(mode,4096);
  check(selector>=1&&selector<=3&&u32(image.data()+3468)==0&&u32(image.data()+2576)==1,"Unbounded context");
  Voice246Registers registers;
  unsigned mask=0;
  for(size_t j=0;j<6;++j){
   pool[j]=state(image.data()+2580+j*24);waves[j]=wave(image.data()+3240+j*36);
   registers.active[j]=image[3504+j];registers.velocity[j]=s16(image.data()+3512+j*2);registers.pitch[j]=signedPhase(u32(image.data()+4136+j*4));
   if(registers.active[j])mask|=1u<<j;
   check(u32(image.data()+4528+j*4)==0,"Auxiliary module present");
  }
  check(u32(image.data()+4560)==0,"Auxiliary module present");masks[mask]=true;
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=0;
  Voice246NoteArgument argument;std::memcpy(argument.data(),p+16,16);const auto before=argument;
  check(u32(argument.data()+12)!=0,"Zero pitch leaked into new oracle");pitches.insert(u32(argument.data()+12));tunings.insert(u32(image.data()+4444));
  auto random=u32(image.data()+4368);
  const auto result=drWave246EmptyExplicitPitch(waves,registers,pool,list,argument,image[4],int(loaded),int(selected),mode,signedPhase(u32(image.data()+4444)),random);
  using S=DrWave246EmptyExplicitPitchStatus;check(result.status!=S::unsupported,"Unsupported fixture");
  if(result.status==S::gated){++gated;check(u32(p+4)==1,"Wrong gated class");}
  else {++appended;check(u32(p+4)==0&&list.count==1,"Wrong appended class");started+=result.started;restarted+=result.restarted;blocked+=result.blocked;
   check(u32(list.bytes.data()+12)==u32(argument.data()+12)&&u32(list.bytes.data()+16)==0xfffffffeu,"Explicit word/list marker differs");
   for(size_t j=9;j<12;++j)check(list.bytes[j]==image[8+j],"Destination padding changed");}
  check(argument==before&&std::memcmp(p+8,e+imageBytes,32)==0,"Argument or guards changed");
  check(std::memcmp(p+tableOffset,e+imageBytes+32,512)==0,"Synthetic lookup table changed");
  for(size_t j=0;j<6;++j){
   image[3504+j]=registers.active[j];const auto v=uint16_t(registers.velocity[j]);image[3512+j*2]=uint8_t(v);image[3513+j*2]=uint8_t(v>>8);put(image.data()+4136+j*4,uint32_t(registers.pitch[j]));
   save(image.data()+2580+j*24,pool[j]);save(image.data()+3240+j*36,waves[j]);
   if(result.status==S::appended&&j<(selector==3?6u:1u)&&((mask&(1u<<j))||loaded==selected)){
    const auto pitch=(mask&(1u<<j))?pool[j].field12:uint32_t(registers.pitch[j]);
    positive+=pitch==0x7fffffffu;negative+=pitch==0x80000000u;
   }
  }
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);put(image.data()+4368,random);
  if(std::memcmp(image.data(),e,imageBytes)!=0){for(size_t k=0;k<imageBytes;++k)if(image[k]!=e[k]){std::cerr<<"case "<<c<<" offset "<<k<<" adapter "<<unsigned(image[k])<<" original "<<unsigned(e[k])<<'\n';break;}throw std::runtime_error("Full explicit-pitch state differs");}
 }
 for(unsigned b=0;b<32;++b){check(pitches.count(1u<<b)==1,"Missing walking bit");check(pitches.count(0xffffffffu^(1u<<b))==1,"Missing walking complement");}
 check(pitches.size()==71&&tunings.size()==7&&positive&&negative,"Missing numerical extrema");
 for(bool mask:masks)check(mask,"Missing activity mask");
 check(appended==3318&&gated==144&&started==2317&&restarted==4351&&blocked==2185,"Coverage changed");
 guards();std::cout<<"PASS: empty explicit pitch "<<cases<<" records; started "<<started<<", restarted "<<restarted<<", blocked "<<blocked<<"; 1491 lookup-invariant pairs; full shared state/args/table/guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
