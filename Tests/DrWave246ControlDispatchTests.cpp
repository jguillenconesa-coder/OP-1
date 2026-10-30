// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246ControlDispatch.h"
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
    const auto extra=read(dir+"/drwave246-control-dispatch-overrides.bin");
    const auto oracle=read(dir+"/drwave246-control-dispatch-output.bin");
    const auto sineData=read(dir+"/verified-sine.bin");
    check(input.size()==512*9316&&extra.size()==1506*1176&&oracle.size()==112+327680+1506*10728&&sineData.size()>=16384,"Incomplete mixed-control fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;
    DrWave246Context initial;drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data()+112,sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Base banks differ/changed");
    std::array<DrWave246State,6> pool{};std::array<int16_t,4> smoothing{};std::array<uint8_t,24> volumeImage{};std::array<bool,8> colours{};size_t fineChannels=0;std::array<std::array<bool,625>,2> combinations{};std::array<bool,5> envColours{};std::array<bool,9> envStages{};
    std::array<Adsr246State,6> envelopes{};Adsr246Noise noise;uint32_t poolGuard=0,rngGuard=0,previousGuard=0;size_t adsrCalls=0,adsrWrites=0;
    size_t applied=0,inactive=0,updates=0,reads=0,disabled=0;
    std::array<bool,16> masks{};std::array<bool,5> channelCounts{};
    for(size_t c=0;c<8;++c)for(size_t w=0;w<4;++w)check(drWave246InstrumentRanges[c][w]==s16(oracle.data()+c*8+w*2),"Original instrument range differs");
    for(size_t c=0;c<4;++c)for(size_t w=0;w<3;++w)check(valueVoice246EnvelopeRanges[c][w]==s16(oracle.data()+80+c*6+w*2),"Original ENV range differs");
    check(s16(oracle.data()+104)==2048&&s16(oracle.data()+106)==14336&&s16(oracle.data()+108)==32767,"Original ENV mode range differs");
    check(oracle[66]==0,"Unexpected original type-2 flag");
    for(size_t test=0;test<1506;++test){
        const auto* x=extra.data()+test*1176;const auto index=u32(x+1168);check(index<512,"Invalid source record");const auto* p=input.data()+index*9316;
        const auto* e=oracle.data()+112+163840+test*10728;
        const auto kind=u32(x);const auto action=u32(x+1172),slot=u32(p+4),counter=u32(p+8),enabled=u32(p+12);check(slot<6,"Invalid slot");
        if(action&1){
            poolGuard=u32(x+464);noise={u32(x+1128),signedPhase(u32(x+1136))};rngGuard=u32(x+1132);previousGuard=u32(x+1140);
            for(size_t v=0;v<6;++v){auto& st=envelopes[v];const auto* q=x+468+v*24;st={};st.stage=u32(q);st.blocks=u32(q+4);st.level=signedPhase(u32(q+8));st.field12=u32(q+12);st.field16=u32(q+16);st.gain=u32(q+20);}
            std::memcpy(pool.data(),p+536,216);std::memcpy(waves.data(),p+752,6144);
            std::memcpy(&runtime.settings,p+6896,16);std::memcpy(&runtime.transform,p+6912,2072);
            for(size_t i=0;i<5;++i)runtime.increments[i]=u32(p+8984+i*4);
            std::memcpy(runtime.configuration.data(),p+9004,32);runtime.wavePointer=u32(p+9036);runtime.transform.reciprocalInitialized=u32(p+9040);
        }
        const auto compareEnvelopes=[&](const uint8_t* expected){
            check(poolGuard==u32(expected),"ADSR pool guard changed");
            for(size_t v=0;v<6;++v){const auto& st=envelopes[v];const std::array<uint32_t,6> words{{st.stage,st.blocks,uint32_t(st.level),st.field12,st.field16,st.gain}};for(size_t w=0;w<6;++w)if(words[w]!=u32(expected+4+v*24+w*4)){std::cerr<<"Record "<<test<<", ADSR "<<v<<", word "<<w<<'\n';throw std::runtime_error("Complete ADSR pool differs");}}
        };
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
        size_t mask=0,count=0,combination=0,power=1;
        for(size_t i=0;i<4;++i){const auto destination=floorDivide(controls[i+4],2048);const auto target=floorDivide(controls[i],4096);
            const auto route=destination==2?0:destination==0?1:destination==1?4:2+target;check(route>=0&&route<5,"Invalid test route");combination+=size_t(route)*power;power*=5;
            if(destination==1){check(target>=0&&target<5,"Invalid ENV colour");envColours[size_t(target)]=true;}
            if((kind==0&&(destination==0||destination==3))||(kind==1&&destination==1)){mask|=size_t(1)<<i;++count;}
        }
        if(kind<2)combinations[kind][combination]=true;
        masks[mask]=true;channelCounts[count]=true;
        const auto result=drWave246ControlDispatchStep(parameters,amplitude,pitch,smoothing,envelopes,controls,sources,kind);
        compareEnvelopes(e+9840);
        check(result!=DrWave246ControlResult::unsupported,"Supported routing rejected");
        const bool changed=result==DrWave246ControlResult::applied;
        check(u32(e+8784)==uint32_t(changed),"Original callback return differs");
        if(changed)++applied;else{++inactive;check(pitch==oldPitch&&amplitude==oldAmplitude&&smoothing==oldSmoothing,"Inactive callback changed buffers/smoothing");}
        if(!changed)check(parameters==oldParameters,"Inactive mixed route changed parameters");
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
        std::array<int16_t,128> out{};for(size_t i=0;i<128;++i)out[i]=s16(p+9052+i*2);
        if(kind==1){
            ++adsrCalls;check(envelopes[slot].stage<9,"Invalid ADSR stage");envStages[envelopes[slot].stage]=true;adsrWrites+=envelopes[slot].stage>=1&&envelopes[slot].stage<=8;
            auto predictedNoise=noise;for(size_t i=0;i<128;++i)check(uint32_t(predictedNoise.next())==u32(e+10136+i*4),"Shared dither scratch differs");
            adsr246Block(envelopes[slot],{{parameters[0],parameters[1],parameters[2],parameters[3]}},s16(x+1152),noise,out);
            check(envelopes[slot].finished==(u32(e+10676)!=0),"ADSR completion differs");
            check(u32(x+1124)==u32(e+10648),"Dither trailing guard changed");
        }else{
            std::array<int16_t,4> synthControls{};for(size_t i=0;i<4;++i)synthControls[i]=parameters[i];
            const auto render=drWave246Block(state,waves,runtime,bank,sine,synthControls,pitch,int(counter&65535),out);
            check(render!=DrWave246ReadResult::unsupported,"Following Dr. Wave render rejected");
            if(render==DrWave246ReadResult::inactive)++disabled;else if(slot==(counter&65535)%6)++updates;else ++reads;
            check(u32(e+10676)==0&&std::memcmp(x+612,e+10136,516)==0,"Non-ADSR route changed scratch/return");
        }
        for(size_t i=0;i<128;++i)if(out[i]!=s16(e+8+i*2)){std::cerr<<"Record "<<test<<", kind "<<kind<<", sample "<<i<<'\n';throw std::runtime_error("Following renderer sample differs");}
        compareEnvelopes(e+9988);
        check(noise.seed==u32(e+10652)&&rngGuard==u32(e+10656)&&uint32_t(noise.previous)==u32(e+10660)&&previousGuard==u32(e+10664),"Shared RNG/previous/guards differ");
        check(std::memcmp(x+1144,e+10668,8)==0&&std::memcmp(x+1156,e+10680,12)==0,"ADSR readiness/current type context changed");
        check(std::memcmp(oracle.data()+80,e+10692,32)==0,"Shared ENV ranges changed");
        check(u32(x+1152)==u32(e+10724),"Explicit ADSR volume input changed");
        check(std::memcmp(p+9044,e,8)==0&&std::memcmp(p+9308,e+264,8)==0,"Original output guard changed");
        check(std::memcmp(pool.data(),e+272,216)==0&&std::memcmp(waves.data(),e+488,6144)==0,"Following voice/waves differ");
        check(std::memcmp(&runtime.settings,e+6632,16)==0&&std::memcmp(&runtime.transform,e+6648,2072)==0,"Following settings/cache differ");
        for(size_t i=0;i<5;++i)check(runtime.increments[i]==u32(e+8720+i*4),"Following increment differs");
        check(std::memcmp(runtime.configuration.data(),e+8740,32)==0,"Following configuration differs");
        check(runtime.wavePointer==u32(e+8772)&&runtime.transform.reciprocalInitialized==u32(e+8776)&&counter==u32(e+8780),"Following pointer/flags/counter differ");
    }
    for(bool used:masks)check(used,"Missing routing mask");for(bool used:channelCounts)check(used,"Missing channel count");
    for(const auto& matrix:combinations)for(bool used:matrix)check(used,"Missing kind-0/1 route combination");
    for(bool used:colours)check(used,"Missing instrument selector");for(bool used:envColours)check(used,"Missing ENV selector");for(bool used:envStages)check(used,"Missing ADSR render stage");
    check(applied+inactive==1506&&fineChannels>0&&adsrCalls==743,"Unexpected callback/render coverage");
    // Own guards validate the entire call before any route writes state.
    std::array<int16_t,8> parameters{{1000,1000,1000,1000,11,22,33,44}},controls{{0,0,4096,0,0,6144,6144,0}};const auto before=parameters;
    std::array<int16_t,128> amplitude{};amplitude.fill(1234);const auto oldAmplitude=amplitude;
    std::array<uint32_t,128> pitch{};pitch.fill(0x12345678u);const auto oldPitch=pitch;const auto oldSmoothing=smoothing;const auto oldEnvelopes=envelopes;
    const auto unchanged=[&](){if(parameters!=before||amplitude!=oldAmplitude||pitch!=oldPitch||smoothing!=oldSmoothing)return false;for(size_t v=0;v<6;++v){const auto& a=envelopes[v];const auto& b=oldEnvelopes[v];if(a.stage!=b.stage||a.blocks!=b.blocks||a.level!=b.level||a.field12!=b.field12||a.field16!=b.field16||a.gain!=b.gain)return false;}return true;};
    for(int16_t destination:{int16_t(-1),int16_t(8192)}){controls[7]=destination;check(drWave246ControlDispatchStep(parameters,amplitude,pitch,smoothing,envelopes,controls,{{127,127,127,127}},0)==DrWave246ControlResult::unsupported&&unchanged(),"Late invalid destination mutated state");}
    controls[7]=2048;controls[3]=20480;check(drWave246ControlDispatchStep(parameters,amplitude,pitch,smoothing,envelopes,controls,{{127,127,127,127}},1)==DrWave246ControlResult::unsupported&&unchanged(),"Invalid ENV selector mutated state");
    controls[7]=6144;controls[3]=8192;check(drWave246ControlDispatchStep(parameters,amplitude,pitch,smoothing,envelopes,controls,{{127,127,127,127}},0)==DrWave246ControlResult::unsupported&&unchanged(),"Invalid pitch target mutated state");
    controls[7]=0;controls[3]=0;check(drWave246ControlDispatchStep(parameters,amplitude,pitch,smoothing,envelopes,controls,{{127,127,127,127}},2)==DrWave246ControlResult::inactive&&unchanged(),"Inactive kind mutated state");
    std::cout<<"PASS: 625 route combinations at both kinds 0/1; 1506 callbacks, 6024 decodes; "<<applied<<" applied / "<<inactive<<" inactive, Dr Wave "<<(updates+reads)*128<<" active samples, ADSR "<<adsrCalls*128<<" sample positions ("<<adsrWrites*128<<" written); complete pools/scratch/shared state/guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
