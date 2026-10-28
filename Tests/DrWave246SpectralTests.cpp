// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246Spectral.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing spectral fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto x=u16(p);return int16_t(x<32768?int(x):int(x)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/drwave246-spectral-input.bin"),oracle=read(dir+"/drwave246-spectral-output.bin"),sineData=read(dir+"/verified-sine.bin");
    check(input.size()==6144+768*1072&&oracle.size()==327680+768*6196&&sineData.size()>=16384,"Incomplete spectral fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246WaveSettings settings;DrWave246Waves pool;DrWave246Context context;
    drWave246InitializeShared(pool,context,settings);drWave246BuildWaveBank(bank,settings,sine);
    check(std::memcmp(&bank,oracle.data(),sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Original generated banks differ or changed");
    std::array<size_t,6> counts{};std::array<bool,6> slots{};size_t replaced=0,added=0;
    for(size_t test=0;test<768;++test){
        const auto* p=input.data()+6144+test*1072;const auto* e=oracle.data()+163840+test*6196;
        const auto argument=u32(p),mode=u32(p+4),slot=u32(p+8),action=u32(p+12);check(mode<6&&slot<6,"Invalid fixture selectors");
        if(action&1)std::memcpy(pool.data(),input.data(),6144);
        if(action&2)for(size_t i=0;i<256;++i)pool[slot][i]=u32(p+32+i*4);
        std::memcpy(&settings,p+16,16);++counts[mode];slots[slot]=true;
        if(settings.modeAndReady&255)++replaced;else ++added;
        check(drWave246MixSpectrum(pool[slot],bank,sine,settings,static_cast<DrWave246Spectrum>(mode),signedPhase(argument)),"Valid spectral call rejected");
        if(std::memcmp(pool.data(),e+16,6144)!=0){
            for(size_t i=0;i<256;++i)if(pool[slot][i]!=u32(e+16+slot*1024+i*4)){
                std::cerr<<"Spectral case "<<test<<", mode "<<mode<<", argument "<<argument<<", index "<<i<<", got "<<signedPhase(pool[slot][i])<<", expected "<<signedPhase(u32(e+16+slot*1024+i*4))<<'\n';break;
            }
            throw std::runtime_error("Original spectral callback differs");
        }
        check(std::memcmp(e,oracle.data()+163840-16,16)==0&&std::memcmp(e+6160,p+1056,16)==0,"Spectral callback changed guard/bank tail");
        check(std::memcmp(&settings,e+6176,16)==0&&std::memcmp(p+16,e+6176,16)==0,"Spectral settings mutated");
        check(u32(e+6192)==0x01cea08c+slot*1024,"Spectral wave pointer changed");
    }
    for(auto count:counts)check(count==128,"Missing spectral callback cases");for(bool slot:slots)check(slot,"Missing slot");check(replaced&&added,"Missing replace/add route");
    const auto previous=pool;const auto oldSettings=settings;
    for(int32_t argument:{-1,64*65536,2147483647})for(unsigned mode=0;mode<6;++mode)
        check(!drWave246MixSpectrum(pool[0],bank,sine,settings,static_cast<DrWave246Spectrum>(mode),argument),"Invalid argument accepted");
    check(!drWave246MixSpectrum(pool[0],bank,sine,settings,static_cast<DrWave246Spectrum>(6),0),"Invalid callback accepted");
    for(unsigned field=0;field<3;++field){
        settings=oldSettings;
        if(field==0)settings.referenceGain=0;else if(field==1)settings.workingGain=65537;else settings.outputGain=-1;
        check(!drWave246MixSpectrum(pool[0],bank,sine,settings,DrWave246Spectrum::allUp,0),"Unsupported settings accepted");
    }
    check(pool==previous,"Rejected spectral callback mutated wave");
    std::cout<<"PASS: 196608 spectral wave words, 768 six-slot records; "<<replaced<<" replace / "<<added<<" add calls, six original callbacks and unchanged banks\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
