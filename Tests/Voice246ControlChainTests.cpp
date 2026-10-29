// SPDX-License-Identifier: AGPL-3.0-only
#include "ControlVolume246.h"
#include "Voice246ControlInput.h"
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
    const auto extra=read(dir+"/voice246-control-chain-overrides.bin");
    const auto oracle=read(dir+"/voice246-control-chain-output.bin");
    const auto sineData=read(dir+"/verified-sine.bin");
    check(input.size()==512*9316&&extra.size()==512*396&&oracle.size()==327680+512*9692&&sineData.size()>=16384,"Incomplete control-pitch fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;
    DrWave246Context initial;drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data(),sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Base banks differ/changed");
    std::array<DrWave246State,6> pool{};std::array<int16_t,4> smoothing{};std::array<uint8_t,24> volumeImage{};std::array<bool,81> combinations{};size_t volumeChannels=0,pitchChannels=0;
    size_t applied=0,inactive=0,updates=0,reads=0,disabled=0;
    std::array<bool,16> masks{};std::array<bool,5> channelCounts{};
    bool negative=false,positiveSaturation=false,negativeSaturation=false,amplitudePositiveSaturation=false,amplitudeNegativeSaturation=false;
    for(size_t test=0;test<512;++test){
        const auto* p=input.data()+test*9316;const auto* x=extra.data()+test*396;
        const auto* e=oracle.data()+163840+test*9692;
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
        std::array<uint32_t,128> controlValues{};for(size_t i=0;i<4;++i)controlValues[i+1]=u32(x+28+i*4);
        for(size_t i=0;i<4;++i)check(voice246DecodeControl(controlValues,x+380+i*4,3),"Decoder rejected chained message");
        std::array<uint32_t,4> sources{};for(size_t i=0;i<4;++i)sources[i]=controlValues[i+1];
        std::array<uint32_t,128> pitch{};for(size_t i=0;i<128;++i)pitch[i]=u32(p+24+i*4);
        if(action&1){std::memcpy(volumeImage.data(),x+52,24);for(size_t i=0;i<4;++i)smoothing[i]=s16(x+60+i*2);}
        std::array<int16_t,128> amplitude{};for(size_t i=0;i<128;++i)amplitude[i]=s16(x+116+i*2);
        const auto oldAmplitude=amplitude;const auto oldSmoothing=smoothing;
        size_t combination=0,power=1;for(size_t i=0;i<4;++i){const auto route=floorDivide(controls[i+4],2048)==2?0:floorDivide(controls[i],4096)==0?1:2;combination+=size_t(route)*power;power*=3;if(u32(x)==0){volumeChannels+=route==1;pitchChannels+=route==2;}}combinations[combination]=true;
        const auto oldPitch=pitch;const auto oldControls=controls;const auto oldSources=sources;
        size_t mask=0,count=0;for(size_t i=0;i<4;++i)if(floorDivide(controls[i+4],2048)==3){mask|=size_t(1)<<i;++count;}
        masks[mask]=true;channelCounts[count]=true;
        const auto result=controlVolume246Step(amplitude,pitch,smoothing,controls,sources,u32(x));
        check(result!=ControlPitch246Result::unsupported,"Supported routing rejected");
        const bool changed=result==ControlPitch246Result::applied;
        check(u32(e+8784)==uint32_t(changed),"Original callback return differs");
        if(changed)++applied;else{++inactive;check(pitch==oldPitch&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Inactive callback changed buffers/smoothing");}
        check(controls==oldControls&&sources==oldSources,"C++ callback changed inputs");
        for(size_t i=0;i<128;++i){
            if(pitch[i]!=u32(e+8796+i*4)){
                std::cerr<<"Case "<<test<<", pitch word "<<i<<": "<<pitch[i]<<" expected "<<u32(e+8796+i*4)<<'\n';
                throw std::runtime_error("Original pitch buffer differs");
            }
            if(changed){negative|=signedPhase(pitch[i])<0;positiveSaturation|=pitch[i]==0x7fffffffu;negativeSaturation|=pitch[i]==0x80000000u;}
        }
        check(std::memcmp(x+92,e+8788,8)==0&&std::memcmp(x+100,e+9308,8)==0,"Original pitch guard changed");
        check(std::memcmp(x+20,e+9316,8)==0&&std::memcmp(x+44,e+9340,8)==0,"Original source guards changed");
        for(size_t i=0;i<4;++i)check(sources[i]==u32(e+9324+i*4),"Original decoded source differs");
        check(std::memcmp(x+380,e+9676,16)==0,"Original control messages changed");
        for(size_t i=0;i<4;++i){const auto bits=uint16_t(smoothing[i]);volumeImage[8+i*2]=uint8_t(bits);volumeImage[9+i*2]=uint8_t(bits>>8);}
        for(size_t i=0;i<24;++i)if(volumeImage[i]!=e[9348+i]){std::cerr<<"Record "<<test<<", smoothing byte "<<i<<": "<<unsigned(volumeImage[i])<<" expected "<<unsigned(e[9348+i])<<'\n';throw std::runtime_error("Complete smoothing image/guards differ");}
        for(size_t i=0;i<128;++i)if(amplitude[i]!=s16(e+9412+i*2)){std::cerr<<"Record "<<test<<", amplitude "<<i<<'\n';throw std::runtime_error("Original amplitude buffer differs");}
        for(auto sample:amplitude){amplitudePositiveSaturation|=sample==32767;amplitudeNegativeSaturation|=sample==-32768;}
        check(std::memcmp(x+108,e+9404,8)==0&&std::memcmp(x+372,e+9668,8)==0,"Amplitude guards changed");
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
    for(bool used:combinations)check(used,"Missing volume/pitch/disabled combination");
    check(amplitudePositiveSaturation&&amplitudeNegativeSaturation,"Missing amplitude saturation coverage");
    check(applied==459&&inactive==53&&volumeChannels>0&&pitchChannels>0&&updates==240&&reads==241&&disabled==31,"Unexpected branch coverage");
    // Reject late unsupported routes before touching either buffer or shared state.
    std::array<uint32_t,128> pitch{};pitch.fill(0x12345678u);const auto before=pitch;
    std::array<int16_t,128> amplitude{};amplitude.fill(1234);const auto oldAmplitude=amplitude;
    const auto oldSmoothing=smoothing;
    std::array<int16_t,8> controls{{0,0,4096,0,6144,6144,6144,6144}};
    for(int16_t destination:{int16_t(-1),int16_t(0),int16_t(2048),int16_t(8192)}){
        controls[7]=destination;
        check(controlVolume246Step(amplitude,pitch,smoothing,controls,{{127,127,127,127}},0)==ControlPitch246Result::unsupported&&pitch==before&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Invalid destination mutated buffers/state");
    }
    controls[7]=6144;controls[3]=8192;
    check(controlVolume246Step(amplitude,pitch,smoothing,controls,{{127,127,127,127}},0)==ControlPitch246Result::unsupported&&pitch==before&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Invalid target mutated buffers/state");
    controls[3]=0;
    check(controlVolume246Step(amplitude,pitch,smoothing,controls,{{127,127,127,127}},2)==ControlPitch246Result::inactive&&pitch==before&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Inactive kind mutated buffers/state");
    std::cout<<"PASS: 2048 original decoder calls; 65,536 amplitude samples + 65,536 pitch words and 61,568 following Dr. Wave samples; all 81 route combinations; shared smoothing streams\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
