// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246Kernel.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing kernel fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto x=u16(p);return int16_t(x<32768?int(x):int(x)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto sourceInput=read(dir+"/drwave246-kernel-input.bin"),sineData=read(dir+"/verified-sine.bin");
    check(sourceInput.size()==512*9316,"Incomplete source kernel input");
    const auto dispatch=read(dir+"/drwave246-counter-output.bin"),overrides=read(dir+"/drwave246-counter-overrides.bin");
    check(dispatch.size()==131072*8&&overrides.size()==512*12,"Incomplete counter fixtures");
    for(size_t i=0;i<131072;++i){
        check(drWave246UpdateSlot(uint16_t(i))==u32(dispatch.data()+i*8),"Original low16 dispatch differs");
        check(u32(dispatch.data()+i*8+4)==(uint32_t(i&65535)|(i>=65536?0xffff0000u:0)),"Original counter/neighbor changed");
    }
    for(bool extended:{false,true}){
    auto input=sourceInput;
    const auto oracle=read(dir+(extended?"/drwave246-counter-render-output.bin":"/drwave246-kernel-output.bin"));
    if(extended)for(size_t i=0;i<512;++i)std::memcpy(input.data()+i*9316,overrides.data()+i*12,12);
    check(input.size()==512*9316&&oracle.size()==327680+512*8784&&sineData.size()>=16384,"Incomplete kernel fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;
    DrWave246Context initial;drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data(),sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Kernel base banks differ/changed");
    std::array<DrWave246State,6> pool{};size_t updated=0,readOnly=0,inactive=0,inversionWraps=0;
    std::array<bool,6> spectral{},updatedSlots{};std::array<bool,3> transforms{},blueSections{};
    std::array<bool,4> greenModes{},whiteModes{};std::array<bool,2> inversionHalves{};
    for(size_t test=0;test<512;++test){
        const auto* p=input.data()+test*9316;const auto* e=oracle.data()+163840+test*8784;
        const auto action=u32(p),slot=u32(p+4),counter=u32(p+8),enabled=u32(p+12);check(slot<6,"Invalid fixture slot");
        if(action&1){
            std::memcpy(pool.data(),p+536,216);std::memcpy(waves.data(),p+752,6144);
            std::memcpy(&runtime.settings,p+6896,16);std::memcpy(&runtime.transform,p+6912,2072);
            for(size_t i=0;i<5;++i)runtime.increments[i]=u32(p+8984+i*4);
            std::memcpy(runtime.configuration.data(),p+9004,32);runtime.wavePointer=u32(p+9036);runtime.transform.reciprocalInitialized=u32(p+9040);
        }
        runtime.settings.modeAndReady=(runtime.settings.modeAndReady&0xffff00ffu)|((enabled&255)<<8);
        auto& state=pool[slot];if(action&2)state.construct(slot);if(action&4)state.reset();if(action&8)state.release();
        std::array<int16_t,4> controls{};for(size_t i=0;i<4;++i)controls[i]=s16(p+16+i*2);
        std::array<uint32_t,128> pitch{};for(size_t i=0;i<128;++i)pitch[i]=u32(p+24+i*4);
        std::array<int16_t,128> out{};for(size_t i=0;i<128;++i)out[i]=s16(p+9052+i*2);
        const auto beforeInversion=state.field32;
        const auto result=drWave246Block(state,waves,runtime,bank,sine,controls,pitch,int(counter&65535),out);
        check(result!=(DrWave246ReadResult::unsupported),"Valid kernel fixture rejected");
        if(result==DrWave246ReadResult::inactive)++inactive;
        else if(slot==(counter&65535)%6){
            ++updated;updatedSlots[slot]=true;const auto& c=runtime.configuration;
            spectral[size_t(c[0])*2+(c[6]==1?0:1)]=true;spectral[size_t(c[1])*2+(c[6]==1?0:1)]=true;
            transforms[0]=true;if(c[4]<2)transforms[size_t(c[4])+1]=true;
            blueSections[size_t(controls[0]/8192)]=true;greenModes[size_t(c[6])]=true;whiteModes[size_t(c[4])]=true;
            inversionHalves[(state.field32>>24)&1]=true;if(state.field32<beforeInversion)++inversionWraps;
        }
        else ++readOnly;
        for(size_t i=0;i<128;++i)if(out[i]!=s16(e+8+i*2)){
            std::cerr<<"Kernel "<<test<<", sample "<<i<<", got "<<out[i]<<", expected "<<s16(e+8+i*2)<<'\n';throw std::runtime_error("Complete render sample differs");
        }
        check(std::memcmp(p+9044,e,8)==0&&std::memcmp(p+9308,e+264,8)==0,"Kernel output guard changed");
        check(std::memcmp(pool.data(),e+272,216)==0,"Kernel voice pool differs");
        if(std::memcmp(waves.data(),e+488,6144)!=0)throw std::runtime_error("Kernel runtime waves differ");
        check(std::memcmp(&runtime.settings,e+6632,16)==0,"Kernel shared settings differ");
        if(std::memcmp(&runtime.transform,e+6648,2072)!=0||runtime.transform.reciprocalInitialized!=u32(e+8776)){
            const auto* bytes=reinterpret_cast<const uint8_t*>(&runtime.transform);
            for(size_t i=0;i<2072;++i)if(bytes[i]!=e[6648+i]){std::cerr<<"Kernel "<<test<<", cache byte "<<i<<", got "<<unsigned(bytes[i])<<", expected "<<unsigned(e[6648+i])<<'\n';break;}
            std::cerr<<"Reciprocal flag got "<<runtime.transform.reciprocalInitialized<<", expected "<<u32(e+8776)<<'\n';
            throw std::runtime_error("Kernel cache/flags differ");
        }
        for(size_t i=0;i<5;++i)check(runtime.increments[i]==u32(e+8720+i*4),"Kernel increment differs");
        check(std::memcmp(runtime.configuration.data(),e+8740,32)==0,"Kernel control configuration differs");
        check(runtime.wavePointer==u32(e+8772)&&counter==u32(e+8780),"Kernel pointer/counter differs");
    }
    for(bool used:spectral)check(used,"Missing spectral family");for(bool used:transforms)check(used,"Missing wave transform");
    for(bool used:updatedSlots)check(used,"Missing updated voice slot");for(bool used:blueSections)check(used,"Missing blue section");
    for(bool used:greenModes)check(used,"Missing green mode");for(bool used:whiteModes)check(used,"Missing white mode");
    for(bool used:inversionHalves)check(used,"Missing inversion half");check(inversionWraps>0,"Missing inversion wrap");
    const auto previousPool=pool;const auto previousWaves=waves;const auto previousRuntime=runtime;
    auto rejected=[&](std::array<int16_t,4> controls,int counter){
        std::array<int16_t,128> out{};out.fill(123);const auto before=out;
        const auto oldPool=pool;const auto oldWaves=waves;const auto oldRuntime=runtime;
        check(drWave246Block(pool[0],waves,runtime,bank,sine,controls,{},counter,out)==DrWave246ReadResult::unsupported,"Unsupported kernel input accepted");
        check(std::memcmp(pool.data(),oldPool.data(),216)==0&&waves==oldWaves&&std::memcmp(&runtime,&oldRuntime,sizeof(runtime))==0&&out==before,"Rejected kernel call mutated state/output");
    };
    runtime.settings.modeAndReady|=0x100;
    for(int counter:{-1,65536})rejected({{0,0,0,0}},counter);
    for(const auto controls:{std::array<int16_t,4>{{-1,0,0,0}},{{24576,0,0,0}},{{0,-1,0,0}},{{0,0,-1,0}},{{0,0,0,-1}}})rejected(controls,0);
    pool[0].phase[0]=0x1000000;rejected({{0,0,0,0}},0);pool=previousPool;
    pool[0].slot=6;rejected({{0,0,0,0}},0);pool=previousPool;
    pool[0].field32=0x02000000;rejected({{0,0,0,0}},0);pool=previousPool;
    runtime.settings.referenceGain=0;rejected({{0,0,0,0}},0);runtime=previousRuntime;runtime.settings.modeAndReady|=0x100;
    runtime.transform.reciprocalInitialized=1;runtime.transform.reciprocalSeed.fill(0);rejected({{0,0,0,0}},0);
    runtime.settings.modeAndReady&=0xffff00ffu;
    std::array<int16_t,128> untouched{};untouched.fill(234);const auto oldOutput=untouched;const auto disabledRuntime=runtime;
    check(drWave246Block(pool[0],waves,runtime,bank,sine,{{-1,-1,-1,-1}},{},-1,untouched)==DrWave246ReadResult::inactive&&untouched==oldOutput&&waves==previousWaves&&std::memcmp(&runtime,&disabledRuntime,sizeof(runtime))==0,"Inactive early return mutated state/output");
    std::cout<<(extended?"Counter boundary streams: ":"Original render regression: ")<<"PASS: "<<(updated+readOnly)*128<<" complete original Dr. Wave samples; "<<updated<<" updates, "<<readOnly<<" reads, "<<inactive<<" inactive; six families, three transforms and "<<inversionWraps<<" inversion wraps\n";
    }
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
