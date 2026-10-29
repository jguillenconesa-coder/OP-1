// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246ControlInstrument.h"
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
    const auto extra=read(dir+"/drwave246-control-instrument-overrides.bin");
    const auto oracle=read(dir+"/drwave246-control-instrument-output.bin");
    const auto sineData=read(dir+"/verified-sine.bin");
    check(input.size()==512*9316&&extra.size()==512*464&&oracle.size()==80+327680+512*9840&&sineData.size()>=16384,"Incomplete instrument-control fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;
    DrWave246Context initial;drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data()+80,sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Base banks differ/changed");
    std::array<DrWave246State,6> pool{};std::array<int16_t,4> smoothing{};std::array<uint8_t,24> volumeImage{};std::array<bool,8> colours{};size_t fineChannels=0;std::array<bool,3> lowerWrap{},upperWrap{};bool lowerClamp=false,upperClamp=false;
    size_t applied=0,inactive=0,updates=0,reads=0,disabled=0;
    std::array<bool,16> masks{};std::array<bool,5> channelCounts{};
    for(size_t c=0;c<8;++c)for(size_t w=0;w<4;++w)check(drWave246InstrumentRanges[c][w]==s16(oracle.data()+c*8+w*2),"Original instrument range differs");
    check(oracle[66]==0,"Unexpected original type-2 flag");
    for(size_t test=0;test<512;++test){
        const auto* p=input.data()+test*9316;const auto* x=extra.data()+test*464;
        const auto* e=oracle.data()+80+163840+test*9840;
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
        std::array<int16_t,8> parameters{};for(size_t i=0;i<8;++i)parameters[i]=s16(x+400+i*2);
        for(size_t i=0;i<4;++i)if(floorDivide(controls[i+4],2048)==0){const auto colour=size_t(floorDivide(controls[i],4096));check(colour<8,"Invalid test colour");colours[colour]=true;fineChannels+=colour>=4&&u32(x)==0;}
        const auto selected=u32(x+420);check(selected<8&&u32(x+428+selected*4)==2,"Invalid explicit type context");
        const auto oldParameters=parameters;
        const auto oldPitch=pitch;const auto oldControls=controls;const auto oldSources=sources;
        size_t mask=0,count=0;for(size_t i=0;i<4;++i)if(floorDivide(controls[i+4],2048)==0){mask|=size_t(1)<<i;++count;}
        masks[mask]=true;channelCounts[count]=true;
        const auto result=drWave246ControlInstrumentStep(parameters,controls,sources,u32(x));
        check(result!=DrWave246ControlResult::unsupported,"Supported routing rejected");
        const bool changed=result==DrWave246ControlResult::applied;
        check(u32(e+8784)==uint32_t(changed),"Original callback return differs");
        if(changed)++applied;else{++inactive;check(pitch==oldPitch&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Inactive callback changed buffers/smoothing");}
        check(pitch==oldPitch&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Instrument callback changed explicit buffers/smoothing");
        if(!changed)check(parameters==oldParameters,"Inactive instrument route changed parameters");
        if(changed&&count==1){
            size_t channel=0;while(floorDivide(controls[channel+4],2048)!=0)++channel;
            const auto colour=size_t(floorDivide(controls[channel],4096)),target=colour%4;
            const auto value=sources[channel]&65535u;const auto before=oldParameters[target],after=parameters[target];
            if(target<3){lowerWrap[target]|=value<64&&before==0&&after>before;upperWrap[target]|=value>64&&before==drWave246InstrumentRanges[target][1]&&after<before;}
            else{lowerClamp|=value<64&&before==0&&after==0;upperClamp|=value>64&&before==32767&&after==32767;}
        }
        for(size_t i=0;i<8;++i)if(parameters[i]!=s16(e+9696+i*2)){std::cerr<<"Record "<<test<<", parameter "<<i<<'\n';throw std::runtime_error("Original instrument parameter differs");}
        check(std::memcmp(x+396,e+9692,4)==0&&std::memcmp(x+416,e+9712,4)==0,"Instrument parameter guards changed");
        check(std::memcmp(x+420,e+9716,44)==0&&std::memcmp(oracle.data()+64,e+9760,16)==0,"Type selection/guards/flags changed");
        check(std::memcmp(oracle.data(),e+9776,64)==0,"Shared instrument ranges changed");
        check(controls==oldControls&&sources==oldSources,"C++ callback changed inputs");
        for(size_t i=0;i<128;++i){
            if(pitch[i]!=u32(e+8796+i*4)){
                std::cerr<<"Case "<<test<<", pitch word "<<i<<": "<<pitch[i]<<" expected "<<u32(e+8796+i*4)<<'\n';
                throw std::runtime_error("Original pitch buffer differs");
            }

        }
        check(std::memcmp(x+92,e+8788,8)==0&&std::memcmp(x+100,e+9308,8)==0,"Original pitch guard changed");
        check(std::memcmp(x+20,e+9316,8)==0&&std::memcmp(x+44,e+9340,8)==0,"Original source guards changed");
        for(size_t i=0;i<4;++i)check(sources[i]==u32(e+9324+i*4),"Original decoded source differs");
        check(std::memcmp(x+380,e+9676,16)==0,"Original control messages changed");
        for(size_t i=0;i<4;++i){const auto bits=uint16_t(smoothing[i]);volumeImage[8+i*2]=uint8_t(bits);volumeImage[9+i*2]=uint8_t(bits>>8);}
        for(size_t i=0;i<24;++i)if(volumeImage[i]!=e[9348+i]){std::cerr<<"Record "<<test<<", smoothing byte "<<i<<": "<<unsigned(volumeImage[i])<<" expected "<<unsigned(e[9348+i])<<'\n';throw std::runtime_error("Complete smoothing image/guards differ");}
        for(size_t i=0;i<128;++i)if(amplitude[i]!=s16(e+9412+i*2)){std::cerr<<"Record "<<test<<", amplitude "<<i<<'\n';throw std::runtime_error("Original amplitude buffer differs");}

        check(std::memcmp(x+108,e+9404,8)==0&&std::memcmp(x+372,e+9668,8)==0,"Amplitude guards changed");
        check(std::memcmp(x+76,e+9372,16)==0&&std::memcmp(x+4,e+9388,16)==0,"Original self/controls changed");
        std::array<int16_t,4> synthControls{};for(size_t i=0;i<4;++i)synthControls[i]=parameters[i];
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
    for(bool used:lowerWrap)check(used,"Missing lower wrap coverage");for(bool used:upperWrap)check(used,"Missing upper wrap coverage");check(lowerClamp&&upperClamp,"Missing orange clamp coverage");
    for(bool used:colours)check(used,"Missing original selector colour");
    check(applied==436&&inactive==76&&fineChannels>0&&updates==240&&reads==241&&disabled==31,"Unexpected instrument routing coverage");
    // Own guards must reject a late invalid route before earlier channels mutate.
    std::array<int16_t,8> parameters{{1000,1000,1000,1000,11,22,33,44}},controls{{0,0,0,0,0,0,0,0}};const auto before=parameters;
    for(int16_t destination:{int16_t(-1),int16_t(2048),int16_t(6144),int16_t(8192)}){controls[7]=destination;check(drWave246ControlInstrumentStep(parameters,controls,{{127,127,127,127}},0)==DrWave246ControlResult::unsupported&&parameters==before,"Invalid destination mutated parameters");}
    controls[7]=0;controls[3]=-1;check(drWave246ControlInstrumentStep(parameters,controls,{{127,127,127,127}},0)==DrWave246ControlResult::unsupported&&parameters==before,"Negative selector mutated parameters");
    controls[3]=0;check(drWave246ControlInstrumentStep(parameters,controls,{{127,127,127,127}},2)==DrWave246ControlResult::inactive&&parameters==before,"Inactive kind mutated parameters");
    std::cout<<"PASS: initialized Dr. Wave ranges; 2048 original decodes, 512 instrument callbacks, all eight selectors/16 masks and 61568 following Dr. Wave samples; complete state/guards/context\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
