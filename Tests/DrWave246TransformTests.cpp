// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246WaveTransform.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing transform fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/drwave246-transform-input.bin"),oracle=read(dir+"/drwave246-transform-output.bin");
    check(input.size()==6144+512*3188&&oracle.size()==512*8300,"Incomplete transform fixtures");
    DrWave246Waves pool;DrWave246TransformState state;std::array<uint8_t,8> cacheGuards{};
    std::array<size_t,3> counts{};std::array<bool,6> used{};
    for(size_t test=0;test<512;++test){
        const auto* p=input.data()+6144+test*3188;const auto* e=oracle.data()+test*8300;
        const auto action=u32(p),mode=u32(p+4),slot=u32(p+8),word=u32(p+12);check(mode<3&&slot<6,"Invalid transform fixture selectors");
        if(action&1){
            std::memcpy(pool.data(),input.data(),6144);
            std::memcpy(&state,p+1044,2072);state.reciprocalInitialized=u32(p+3120);
            std::memcpy(cacheGuards.data(),p+1040,4);std::memcpy(cacheGuards.data()+4,p+3116,4);
        }
        if(action&4)state.scatterInitialized=state.repeatInitialized=state.reciprocalInitialized=0;
        if(action&2)for(size_t i=0;i<256;++i)pool[slot][i]=u32(p+16+i*4);
        ++counts[mode];used[slot]=true;
        check(drWave246TransformWave(pool[slot],state,static_cast<DrWave246Transform>(mode),signedPhase(word)),"Valid transform rejected");
        if(std::memcmp(pool.data(),e+16,6144)!=0){
            for(size_t i=0;i<256;++i)if(pool[slot][i]!=u32(e+16+slot*1024+i*4)){
                std::cerr<<"Transform "<<test<<", mode "<<mode<<", word "<<word<<", index "<<i<<", got "<<pool[slot][i]<<", expected "<<u32(e+16+slot*1024+i*4)<<'\n';break;
            }
            throw std::runtime_error("Original wave transform differs");
        }
        check(std::memcmp(&state,e+6180,2072)==0&&state.reciprocalInitialized==u32(e+8256),"Original transform cache/flags differ");
        check(std::memcmp(cacheGuards.data(),e+6176,4)==0&&std::memcmp(cacheGuards.data()+4,e+8252,4)==0,"Cache guard changed");
        check(std::memcmp(p+3156,e,16)==0&&std::memcmp(p+3172,e+6160,16)==0,"Wave pool guard changed");
        check(std::memcmp(p+3124,e+8260,32)==0,"Original transform mutated configuration");
        check(u32(e+8292)==0x02f00000&&u32(e+8296)==0x01cea08c+slot*1024,"Original transform mutated pointers");
    }
    for(auto count:counts)check(count>0,"Missing transform mode");for(bool slot:used)check(slot,"Missing transformed slot");
    const auto before=pool;const auto oldState=state;
    for(int32_t word:{-1,65537,2147483647})for(auto mode:{DrWave246Transform::scatterHold,DrWave246Transform::clamp,DrWave246Transform::repeat})
        check(!drWave246TransformWave(pool[0],state,mode,word),"Out-of-domain word accepted");
    check(!drWave246TransformWave(pool[0],state,static_cast<DrWave246Transform>(99),0),"Unknown mode accepted");
    check(std::memcmp(pool.data(),before.data(),6144)==0&&std::memcmp(&state,&oldState,sizeof(state))==0,"Rejected transform mutated state");
    state.reciprocalInitialized=1;state.reciprocalSeed.fill(0);const auto invalid=state;
    check(!drWave246TransformWave(pool[0],state,DrWave246Transform::scatterHold,0)&&std::memcmp(&state,&invalid,sizeof(state))==0&&pool==before,"Invalid reciprocal cache accepted/mutated");
    std::cout<<"PASS: 131072 original transformed wave words; "<<counts[0]<<" scatter/hold, "<<counts[1]<<" clamp, "<<counts[2]<<" repeat calls; full pools, caches and guards\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
