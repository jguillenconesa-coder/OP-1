// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246WaveBank.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
constexpr size_t base=0x01cc1864,region=0x01ceb8b0-base,result=16+region+4;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Dr. Wave bank fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){const auto x=u16(p);return int16_t(x<32768?int(x):int(x)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put32(uint8_t* p,uint32_t x){for(size_t i=0;i<4;++i)p[i]=uint8_t(x>>(i*8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/drwave246-wavebank-input.bin"),oracle=read(dir+"/drwave246-wavebank-output.bin"),sineData=read(dir+"/verified-sine.bin");
    check(input.size()==18*16&&oracle.size()==18*result&&sineData.size()>=16384,"Incomplete bank fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;std::array<bool,6> scratch{};
    for(size_t test=0;test<18;++test){
        const auto* p=input.data()+test*16;const auto* e=oracle.data()+test*result;
        const auto slot=u32(p),patch=u32(p+4),pattern=u32(p+12);check(slot<6,"Invalid scratch slot");scratch[slot]=true;
        std::vector<uint8_t> memory(region);for(size_t i=0;i<region;i+=4)put32(memory.data()+i,pattern);
        DrWave246Waves waves;DrWave246Context context;
        for(auto& wave:waves)wave.fill(pattern);context.increments.fill(pattern);
        context.initialized=pattern;context.reciprocalSeed.fill(uint16_t(pattern));context.wavePointer=0x01cea08c+slot*1024;
        const auto oldContext=context;
        DrWave246WaveSettings settings{pattern,signedPhase(pattern),signedPhase(pattern),signedPhase(pattern)};
        drWave246InitializeShared(waves,context,settings);
        check(std::memcmp(&settings,e,16)==0,"Original shared settings defaults differ");
        check(context.initialized==oldContext.initialized&&context.reciprocalSeed==oldContext.reciprocalSeed&&context.wavePointer==oldContext.wavePointer,"Shared initializer changed preserved C++ fields");
        if(patch)settings.referenceGain=signedPhase(u32(p+8));
        drWave246BuildWaveBank(bank,settings,sine);
        std::memcpy(memory.data(),&settings,16);
        std::memcpy(memory.data()+0x01cc208c-base,&bank,sizeof(bank));
        std::memcpy(memory.data()+0x01cea08c-base,&waves,sizeof(waves));
        std::memcpy(memory.data()+0x01ceb88c-base,context.increments.data(),20);
        const auto* expected=e+16;
        if(std::memcmp(memory.data(),expected,region)!=0){
            for(size_t i=0;i<region;++i)if(memory[i]!=expected[i]){
                std::cerr<<"Startup case "<<test<<", address 0x"<<std::hex<<(base+i)<<std::dec<<", got "<<unsigned(memory[i])<<", expected "<<unsigned(expected[i])<<'\n';
                break;
            }
            throw std::runtime_error("Original wave-bank table/state differs");
        }
        check(u32(expected+region)==context.wavePointer,"Original scratch pointer changed");
    }
    for(bool used:scratch)check(used,"Missing scratch slot");
    std::cout<<"PASS: 18 original shared/bank initializations; 1474560 generated table samples, all six scratch slots and complete surrounding state\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
