// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Dr. Wave fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto v=u16(p);return int16_t(v<32768?int(v):int(v)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    auto input=read(dir+"/drwave246-reader-input.bin"),oracle=read(dir+"/drwave246-reader-output.bin");
    check(input.size()==6144+2048*1072&&oracle.size()==2048*536+6144,"Incomplete Dr. Wave fixtures");
    DrWave246Waves waves{};for(size_t slot=0;slot<6;++slot)for(size_t i=0;i<256;++i)waves[slot][i]=u32(input.data()+slot*1024+i*4);
    check(std::memcmp(input.data(),oracle.data()+2048*536,6144)==0,"Original wave buffers changed");
    std::array<DrWave246State,6> pool{};DrWave246Context context;
    size_t rendered=0,inactive=0,factories=0,resets=0,releases=0;
    std::array<bool,6> used{};std::array<bool,256> indices{};
    for(size_t block=0;block<2048;++block){
        const auto* p=input.data()+6144+block*1072;const auto* e=oracle.data()+block*536;
        const auto action=u32(p),slot=u32(p+4);check(slot<6,"Invalid fixture slot");
        if(action&1){
            std::memcpy(pool.data(),p+540,216);
            for(size_t i=0;i<5;++i)context.increments[i]=u32(p+1028+i*4);
            context.initialized=u32(p+1048);
            for(size_t i=0;i<8;++i)context.reciprocalSeed[i]=u16(p+1052+i*2);
            context.wavePointer=u32(p+1068);
        }
        auto& state=pool[slot];
        if(action&2){state.construct(slot);++factories;}
        check(u32(e+532)==((action&2)?0xffb00000+slot*36:0),"Original factory pointer differs");
        if(action&4){state.reset();++resets;}
        // The original engine release callback is a no-op; common ADSR is external.
        if(action&8){state.release();++releases;}
        std::array<int16_t,4> controls{};for(size_t i=0;i<4;++i)controls[i]=s16(p+20+i*2);
        std::array<uint32_t,128> pitch{};for(size_t i=0;i<128;++i)pitch[i]=u32(p+28+i*4);
        std::array<int16_t,128> output{};for(size_t i=0;i<128;++i)output[i]=s16(p+764+i*2);
        if(u32(p+12))for(auto phase:state.phase)indices[phase>>16]=true;
        const auto result=drWave246ReadBlock(state,context,waves,controls,pitch,int(u32(p+8)),u32(p+12)!=0,output);
        if(result==DrWave246ReadResult::rendered){++rendered;used[slot]=true;}else{check(result==DrWave246ReadResult::inactive,"Unexpected unsupported route");++inactive;}
        for(size_t i=0;i<128;++i)if(output[i]!=s16(e+8+i*2)){
            std::cerr<<"Dr. Wave block "<<block<<", sample "<<i<<", got "<<output[i]<<", expected "<<s16(e+8+i*2)<<'\n';
            throw std::runtime_error("Original reader sample differs");
        }
        check(std::memcmp(p+756,e,8)==0&&std::memcmp(p+1020,e+264,8)==0,"Output guard changed");
        check(std::memcmp(pool.data(),e+272,216)==0,"Original pool state differs");
        for(size_t i=0;i<5;++i)if(context.increments[i]!=u32(e+488+i*4)){
            std::cerr<<"Dr. Wave block "<<block<<", increment "<<i<<", got "<<context.increments[i]<<", expected "<<u32(e+488+i*4)<<'\n';
            throw std::runtime_error("Original increment differs");
        }
        check(context.initialized==u32(e+508),"Original lazy-init flag differs");
        for(size_t i=0;i<8;++i)check(context.reciprocalSeed[i]==u16(e+512+i*2),"Original reciprocal seed copy differs");
        check(context.wavePointer==u32(e+528),"Original shared wave pointer differs");
    }
    for(bool slot:used)check(slot,"Missing rendered slot");for(bool index:indices)check(index,"Missing initial wave index");
    const auto previous=pool[0];const auto previousContext=context;
    std::array<uint32_t,128> pitch{};std::array<int16_t,128> output{};output.fill(123);const auto originalOutput=output;
    for(int counter:{-1,0,6,32768})check(drWave246ReadBlock(pool[0],context,waves,{{0,0,0,0}},pitch,counter,true,output)==DrWave246ReadResult::unsupported,"Unsupported update counter accepted");
    check(drWave246ReadBlock(pool[0],context,waves,{{0,0,0,-1}},pitch,1,true,output)==DrWave246ReadResult::unsupported,"Negative orange accepted");
    check(std::memcmp(&pool[0],&previous,36)==0&&context.increments==previousContext.increments&&context.initialized==previousContext.initialized&&context.reciprocalSeed==previousContext.reciprocalSeed&&context.wavePointer==previousContext.wavePointer&&output==originalOutput,"Rejected route mutated data");
    pool[0].phase[0]=0x01000000;const auto invalid=pool[0];
    check(drWave246ReadBlock(pool[0],context,waves,{{0,0,0,0}},pitch,1,true,output)==DrWave246ReadResult::unsupported&&std::memcmp(&pool[0],&invalid,36)==0&&output==originalOutput,"Invalid initial phase accepted/mutated");
    pool[0]=previous;pool[0].slot=6;const auto invalidSlot=pool[0];
    check(drWave246ReadBlock(pool[0],context,waves,{{0,0,0,0}},pitch,1,true,output)==DrWave246ReadResult::unsupported&&std::memcmp(&pool[0],&invalidSlot,36)==0&&output==originalOutput,"Invalid wave slot accepted/mutated");
    pool[0]=previous;context.initialized=1;context.reciprocalSeed.fill(0);const auto invalidContext=context;
    check(drWave246ReadBlock(pool[0],context,waves,{{0,0,0,0}},pitch,1,true,output)==DrWave246ReadResult::unsupported&&context.reciprocalSeed==invalidContext.reciprocalSeed&&context.increments==invalidContext.increments&&context.wavePointer==invalidContext.wavePointer&&output==originalOutput,"Invalid reciprocal cache accepted/mutated");
    std::cout<<"PASS: "<<rendered*128<<" Dr. Wave samples, "<<rendered<<" rendered / "<<inactive<<" inactive blocks, "<<factories<<" factories, "<<resets<<" resets, "<<releases<<" releases; six pools and shared state\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
