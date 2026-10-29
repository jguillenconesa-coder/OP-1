// SPDX-License-Identifier: AGPL-3.0-only
#include "ControlPitch246.h"
#include "DrWave246Kernel.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing control-pitch fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto x=u16(p);return int16_t(x<32768?int(x):int(x)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/drwave246-kernel-input.bin");
    const auto extra=read(dir+"/control-pitch246-overrides.bin");
    const auto oracle=read(dir+"/control-pitch246-output.bin");
    const auto sineData=read(dir+"/verified-sine.bin");
    check(input.size()==512*9316&&extra.size()==512*108&&oracle.size()==327680+512*9404&&sineData.size()>=16384,"Incomplete control-pitch fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;
    DrWave246Context initial;drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data(),sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Base banks differ/changed");
    std::array<DrWave246State,6> pool{};
    size_t applied=0,inactive=0,updates=0,reads=0,disabled=0;
    std::array<bool,16> masks{};std::array<bool,5> channelCounts{};
    bool negative=false,positiveSaturation=false,negativeSaturation=false;
    for(size_t test=0;test<512;++test){
        const auto* p=input.data()+test*9316;const auto* x=extra.data()+test*108;
        const auto* e=oracle.data()+163840+test*9404;
        const auto action=u32(p),slot=u32(p+4),counter=u32(p+8),enabled=u32(p+12);check(slot<6,"Invalid slot");
        if(action&1){
            std::memcpy(pool.data(),p+536,216);std::memcpy(waves.data(),p+752,6144);
            std::memcpy(&runtime.settings,p+6896,16);std::memcpy(&runtime.transform,p+6912,2072);
            for(size_t i=0;i<5;++i)runtime.increments[i]=u32(p+8984+i*4);
            std::memcpy(runtime.configuration.data(),p+9004,32);runtime.wavePointer=u32(p+9036);runtime.transform.reciprocalInitialized=u32(p+9040);
        }
        runtime.settings.modeAndReady=(runtime.settings.modeAndReady&0xffff00ffu)|((enabled&255)<<8);
        auto& state=pool[slot];if(action&2)state.construct(slot);if(action&4)state.reset();if(action&8)state.release();
        std::array<int16_t,8> controls{};for(size_t i=0;i<8;++i)controls[i]=s16(x+4+i*2);
        std::array<uint32_t,4> sources{};for(size_t i=0;i<4;++i)sources[i]=u32(x+28+i*4);
        std::array<uint32_t,128> pitch{};for(size_t i=0;i<128;++i)pitch[i]=u32(p+24+i*4);
        const auto oldPitch=pitch;const auto oldControls=controls;const auto oldSources=sources;
        size_t mask=0,count=0;for(size_t i=0;i<4;++i)if(floorDivide(controls[i+4],2048)==3){mask|=size_t(1)<<i;++count;}
        masks[mask]=true;channelCounts[count]=true;
        const auto result=controlPitch246Step(pitch,controls,sources,u32(x));
        check(result!=ControlPitch246Result::unsupported,"Supported routing rejected");
        const bool changed=result==ControlPitch246Result::applied;
        check(u32(e+8784)==uint32_t(changed),"Original callback return differs");
        if(changed)++applied;else{++inactive;check(pitch==oldPitch,"Inactive callback changed pitch");}
        check(controls==oldControls&&sources==oldSources,"C++ callback changed inputs");
        for(size_t i=0;i<128;++i){
            if(pitch[i]!=u32(e+8796+i*4)){
                std::cerr<<"Case "<<test<<", pitch word "<<i<<": "<<pitch[i]<<" expected "<<u32(e+8796+i*4)<<'\n';
                throw std::runtime_error("Original pitch buffer differs");
            }
            if(changed){negative|=signedPhase(pitch[i])<0;positiveSaturation|=pitch[i]==0x7fffffffu;negativeSaturation|=pitch[i]==0x80000000u;}
        }
        check(std::memcmp(x+92,e+8788,8)==0&&std::memcmp(x+100,e+9308,8)==0,"Original pitch guard changed");
        check(std::memcmp(x+20,e+9316,32)==0&&std::memcmp(x+52,e+9348,24)==0,"Original source/volume image changed");
        check(std::memcmp(x+76,e+9372,16)==0&&std::memcmp(x+4,e+9388,16)==0,"Original self/controls changed");
        std::array<int16_t,4> synthControls{};for(size_t i=0;i<4;++i)synthControls[i]=s16(p+16+i*2);
        std::array<int16_t,128> out{};for(size_t i=0;i<128;++i)out[i]=s16(p+9052+i*2);
        const auto render=drWave246Block(state,waves,runtime,bank,sine,synthControls,pitch,int(counter&65535),out);
        check(render!=DrWave246ReadResult::unsupported,"Following render rejected");
        if(render==DrWave246ReadResult::inactive)++disabled;else if(slot==(counter&65535)%6)++updates;else ++reads;
        for(size_t i=0;i<128;++i)check(out[i]==s16(e+8+i*2),"Following audio sample differs");
        check(std::memcmp(p+9044,e,8)==0&&std::memcmp(p+9308,e+264,8)==0,"Original output guard changed");
        check(std::memcmp(pool.data(),e+272,216)==0&&std::memcmp(waves.data(),e+488,6144)==0,"Following voice/waves differ");
        check(std::memcmp(&runtime.settings,e+6632,16)==0&&std::memcmp(&runtime.transform,e+6648,2072)==0,"Following settings/cache differ");
        for(size_t i=0;i<5;++i)check(runtime.increments[i]==u32(e+8720+i*4),"Following increment differs");
        check(std::memcmp(runtime.configuration.data(),e+8740,32)==0,"Following configuration differs");
        check(runtime.wavePointer==u32(e+8772)&&runtime.transform.reciprocalInitialized==u32(e+8776)&&counter==u32(e+8780),"Following pointer/flags/counter differ");
    }
    for(bool used:masks)check(used,"Missing routing mask");for(bool used:channelCounts)check(used,"Missing channel count");
    check(negative&&positiveSaturation&&negativeSaturation,"Missing signed/saturated pitch coverage");
    check(applied==436&&inactive==76&&updates==240&&reads==241&&disabled==31,"Unexpected branch coverage");
    // A late unsupported channel must reject before any earlier pitch channel mutates.
    std::array<uint32_t,128> pitch{};pitch.fill(0x12345678u);const auto before=pitch;
    std::array<int16_t,8> controls{{4096,4096,4096,4096,6144,6144,6144,6144}};
    for(int16_t destination:{int16_t(-1),int16_t(0),int16_t(2048),int16_t(8192)}){
        controls[7]=destination;
        check(controlPitch246Step(pitch,controls,{{127,127,127,127}},0)==ControlPitch246Result::unsupported&&pitch==before,"Invalid destination mutated pitch");
    }
    controls[7]=6144;controls[3]=0;
    check(controlPitch246Step(pitch,controls,{{127,127,127,127}},0)==ControlPitch246Result::unsupported&&pitch==before,"Volume target accepted/mutated pitch");
    controls[3]=4096;
    check(controlPitch246Step(pitch,controls,{{127,127,127,127}},2)==ControlPitch246Result::inactive&&pitch==before,"Inactive kind mutated pitch");
    std::cout<<"PASS: 65,536 original pitch words and 61,568 following Dr. Wave samples; all 16 routing masks; 436 applied / 76 inactive callbacks\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
