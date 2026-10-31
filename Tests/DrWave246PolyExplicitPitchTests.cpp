// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246PolyExplicitPitch.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
constexpr size_t cases=4552,probes=497,imageBytes=4568,tableOffset=4608,recordBytes=5120,resultBytes=5112;
constexpr std::array<size_t,17> offsets{{0,2576,3240,3464,4240,4368,4376,4384,4392,4400,4408,4440,4452,4464,4476,4524,4556}};
constexpr std::array<size_t,17> sizes{{2576,664,224,776,128,8,8,8,8,8,32,12,12,12,48,32,12}};
constexpr std::array<bool,17> retained{{true,true,true,true,false,true,true,false,false,false,false,false,false,false,true,false,false}};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto v=u16(p);return int16_t(v<32768?int(v):int(v)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put(uint8_t* p,uint32_t v){for(unsigned j=0;j<4;++j)p[j]=uint8_t(v>>(j*8));}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing poly explicit fixture");return {std::istreambuf_iterator<char>(f),{}};}
Adsr246State state(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void save(uint8_t* p,const Adsr246State& s){const std::array<uint32_t,6> words{{s.stage,s.blocks,uint32_t(s.level),s.field12,s.field16,s.gain}};for(size_t j=0;j<6;++j)put(p+j*4,words[j]);}
DrWave246State wave(const uint8_t* p){DrWave246State w;w.tag=u32(p);w.field4=u32(p+4);for(size_t j=0;j<5;++j)w.phase[j]=u32(p+8+j*4);w.slot=u32(p+28);w.field32=u32(p+32);return w;}
void save(uint8_t* p,const DrWave246State& w){put(p,w.tag);put(p+4,w.field4);for(size_t j=0;j<5;++j)put(p+8+j*4,w.phase[j]);put(p+28,w.slot);put(p+32,w.field32);}
void guards(){
 DrWave246Pool waves{};Voice246Registers registers;std::array<Adsr246State,6> pool{};Voice246ReleaseList list;Voice246NoteArgument argument{};
 using S=DrWave246PolyExplicitPitchStatus;
 const auto call=[&](uint8_t gate=0,int16_t mode=0){return drWave246PolyExplicitPitch(waves,registers,pool,list,argument,gate,0,0,mode,0x7fffffff).status;};
 check(call()==S::unsupported,"Zero pitch outside new scope");put(argument.data()+12,1);put(argument.data(),60);
 list.count=129;check(call()==S::unsupported,"Capacity bound");list.count=128;check(call()==S::unsupported,"New note at capacity");list.count=0;
 registers.active.fill(1);for(auto& env:pool)env.blocks=0xffffffffu;
 check(call()==S::unsupported,"All-negative busy ages outside original non-error oracle");
 check(list.count==0&&pool[0].stage==0&&registers.pitch[0]==0,"Own guard mutated state");
 check(call(255)==S::gated,"Gate must precede invalid age selection");
 list.count=1;put(list.bytes.data(),60);check(call()==S::duplicate,"Duplicate must precede invalid age selection");
 list.count=0;registers.active.fill(0);check(call(0,4096)==S::unsupported&&call(0,-1)==S::unsupported,"Nonzero selector outside new scope");
}
}
int main(int argc,char** argv){try{
 check(argc==2,"Expected fixture directory");const std::string directory=argv[1];
 const auto input=read(directory+"/drwave246-poly-explicit-pitch-input.bin"),output=read(directory+"/drwave246-poly-explicit-pitch-output.bin");
 check(input.size()==cases*recordBytes&&output.size()==cases*resultBytes,"Wrong poly explicit dimensions");
 for(size_t p=0;p<probes;++p){
  for(size_t profile=0;profile<4;++profile){const auto c=p*8+profile*2;const auto* a=input.data()+c*recordBytes;const auto* b=a+recordBytes;
   check(std::memcmp(a,b,tableOffset)==0&&std::memcmp(a+tableOffset,b+tableOffset,512)!=0,"Uncontrolled lookup pair");
   const auto* x=output.data()+c*resultBytes;const auto* y=x+resultBytes;
   check(std::memcmp(x+4,y+4,imageBytes+28)==0,"Explicit result depends on lookup table");}
  for(const auto first:{0u,4u}){const auto* a=input.data()+(p*8+first)*recordBytes;const auto* b=a+2*recordBytes;
   for(size_t k=0;k<recordBytes;++k)if(k<40+4400||k>=40+4404)check(a[k]==b[k],"Core input pair differs beyond loaded slot");}
  const auto* busy=output.data()+(p*8+4)*resultBytes;const auto* mismatch=busy+2*resultBytes;
  for(size_t k=0;k<resultBytes;++k)if(k<4400||k>=4404)check(busy[k]==mismatch[k],"Busy path depends on loaded/selected acceptance");
  const auto* free=output.data()+p*8*resultBytes;const auto* rejected=free+2*resultBytes;
  check(std::memcmp(free+2576,rejected+2576,664)==0&&std::memcmp(free,rejected,2576)==0,"Core rejection rolled back free ADSR/list");
 }
 std::array<uint8_t,imageBytes> image{};DrWave246Pool waves{};std::array<Adsr246State,6> pool{};
 std::array<bool,6> freeVoices{},victims{};std::array<bool,129> lengths{};std::array<std::array<bool,128>,2> notes{};
 size_t freeCount=0,busyCount=0,gated=0,duplicates=0,accepted=0,blocked=0,detached=0,duplicateOwners=0,tied=0;
 std::set<uint32_t> pitches,tunings;
 for(size_t c=0;c<cases;++c){
  const auto* p=input.data()+c*recordBytes;const auto* e=output.data()+c*resultBytes;
  for(size_t j=0;j<17;++j)if(u32(p)||!retained[j])std::memcpy(image.data()+offsets[j],p+40+offsets[j],sizes[j]);
  image[4]=p[44];std::memcpy(image.data()+3504,p+3544,6);std::memcpy(image.data(),p+tableOffset+508,4);
  for(size_t j=0;j<6;++j)put(image.data()+2584+j*24,u32(p+40+2584+j*24));
  const auto n=u32(image.data()+2568),selected=u32(image.data()+4392),loaded=u32(image.data()+4400);
  check(n<=128&&selected<8&&loaded<8,"Unbounded context");lengths[n]=true;
  const auto mode=s16(image.data()+4240+selected*16+8);
  check(floorDivide(mode,4096)==0&&u32(image.data()+3468)==0&&u32(image.data()+2576)==1,"Unbounded mode/ready context");
  Voice246Registers registers;
  for(size_t j=0;j<6;++j){pool[j]=state(image.data()+2580+j*24);waves[j]=wave(image.data()+3240+j*36);
   registers.active[j]=image[3504+j];registers.velocity[j]=s16(image.data()+3512+j*2);registers.pitch[j]=signedPhase(u32(image.data()+4136+j*4));
   check(u32(image.data()+4528+j*4)==0,"Auxiliary module present");}
  check(u32(image.data()+4560)==0,"Auxiliary module present");const auto oldRegisters=registers;
  Voice246ReleaseList list;std::memcpy(list.bytes.data(),image.data()+8,2560);list.count=n;
  Voice246NoteArgument argument;std::memcpy(argument.data(),p+16,16);const auto before=argument;
  check(u32(argument.data()+12)!=0,"Zero explicit pitch");pitches.insert(u32(argument.data()+12));tunings.insert(u32(image.data()+4444));
  const auto result=drWave246PolyExplicitPitch(waves,registers,pool,list,argument,image[4],int(loaded),int(selected),mode,signedPhase(u32(image.data()+4444)));
  using S=DrWave246PolyExplicitPitchStatus;check(result.status!=S::unsupported,"Unsupported fixture");const auto kind=u32(p+4);
  if(result.status==S::gated){++gated;check(kind==2,"Wrong gate classification");}
  else if(result.status==S::duplicate){++duplicates;check(kind==3,"Wrong duplicate classification");}
  else {
   check(result.voice>=0&&result.voice<6&&n<128&&list.count==n+1,"Invalid append/voice");
   const auto v=size_t(result.voice);check(u32(list.bytes.data()+n*20+16)==v,"Appended owner differs");
   const bool busy=result.status==S::busyAppended;notes[busy?1:0][u32(argument.data())]=true;
   if(busy){++busyCount;victims[v]=true;check(kind==1,"Wrong busy classification");
    check(registers.active==oldRegisters.active&&registers.velocity==oldRegisters.velocity&&registers.pitch==oldRegisters.pitch,"Busy helper wrote VoiceCtl registers");
    check(pool[v].gain==u32(image.data()+2600+v*24),"Busy helper changed gain");
    size_t owners=0;for(size_t i=0;i<n;++i)owners+=u32(image.data()+8+i*20+16)==v;
    detached+=owners!=0;duplicateOwners+=owners>1;
    int32_t greatest=-1;size_t ties=0;for(size_t j=0;j<6;++j){const auto age=signedPhase(u32(image.data()+2584+j*24));
     if(age>greatest){greatest=age;ties=1;}else if(age==greatest)++ties;}
    tied+=ties>1;
   }else{++freeCount;freeVoices[v]=true;check(kind==0,"Wrong free classification");accepted+=result.coreAccepted;blocked+=!result.coreAccepted;check(result.coreAccepted==(loaded==selected),"Wrong Core acceptance");}
   for(size_t j=9;j<12;++j)check(list.bytes[n*20+j]==image[8+n*20+j],"Destination padding changed");
  }
  check(argument==before&&std::memcmp(p+8,e+imageBytes,32)==0,"Argument or guards changed");
  check(std::memcmp(p+tableOffset,e+imageBytes+32,512)==0,"Synthetic lookup table changed");
  check(std::memcmp(image.data()+4368,e+4368,8)==0,"Poly explicit path changed RNG/neighbor");
  for(size_t j=0;j<6;++j){image[3504+j]=registers.active[j];const auto v=uint16_t(registers.velocity[j]);image[3512+j*2]=uint8_t(v);image[3513+j*2]=uint8_t(v>>8);put(image.data()+4136+j*4,uint32_t(registers.pitch[j]));save(image.data()+2580+j*24,pool[j]);save(image.data()+3240+j*36,waves[j]);}
  std::memcpy(image.data()+8,list.bytes.data(),2560);put(image.data()+2568,list.count);
  if(std::memcmp(image.data(),e,imageBytes)!=0){for(size_t k=0;k<imageBytes;++k)if(image[k]!=e[k]){std::cerr<<"case "<<c<<" offset "<<k<<" adapter "<<unsigned(image[k])<<" original "<<unsigned(e[k])<<'\n';break;}throw std::runtime_error("Full poly explicit image differs");}
 }
 for(bool b:freeVoices)check(b,"Missing free voice");
 for(bool b:victims)check(b,"Missing victim");
 for(bool b:lengths)check(b,"Missing list length");
 for(const auto& branch:notes)for(bool b:branch)check(b,"Missing note per poly path");
 for(unsigned b=0;b<32;++b){check(pitches.count(1u<<b)==1,"Missing walking bit");check(pitches.count(0xffffffffu^(1u<<b))==1,"Missing complement");}
 check(pitches.size()==71&&tunings.size()==7,"Missing signed numerical patterns");
 check(freeCount==2132&&busyCount==2132&&gated==144&&duplicates==144&&accepted==1066&&blocked==1066&&detached==1605&&duplicateOwners==541&&tied==850,"Coverage changed");
 guards();std::cout<<"PASS: poly explicit "<<cases<<" calls; free "<<freeCount<<" ("<<accepted<<" accepted, "<<blocked<<" blocked), busy "<<busyCount<<", gated/duplicate "<<gated<<'/'<<duplicates<<"; six free/victims, "<<tied<<" ties; full states/args/table/guards, RNG preserved\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
