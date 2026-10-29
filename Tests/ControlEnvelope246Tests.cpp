// SPDX-License-Identifier: AGPL-3.0-only
#include "ControlEnvelope246.h"
#include "Envelope246.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing ENV fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){const auto v=u16(p);return int16_t(v<32768?int(v):int(v)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/control-envelope246-input.bin"),oracle=read(dir+"/control-envelope246-output.bin");
    check(input.size()==1024*440&&oracle.size()==1024*432,"Wrong ENV dimensions");
    Adsr246State state;Adsr246Noise noise;uint32_t left=0,right=0;
    std::array<bool,16> masks{};std::array<bool,4> colours{};std::array<bool,9> stages{};
    size_t applied=0,inactive=0,serial=0,lowClamps=0,highClamps=0;
    for(size_t test=0;test<1024;++test){
        const auto* p=input.data()+test*440;const auto* e=oracle.data()+test*432;
        if(u32(p)&1){left=u32(p+4);right=u32(p+32);state={};state.stage=u32(p+8);state.blocks=u32(p+12);state.level=signedPhase(u32(p+16));state.field12=u32(p+20);state.field16=u32(p+24);state.gain=u32(p+28);noise={u32(p+36),signedPhase(u32(p+40))};}
        stages[state.stage]=true;
        std::array<int16_t,8> parameters{},controls{};std::array<uint32_t,4> sources{};
        for(size_t i=0;i<8;++i){parameters[i]=s16(p+48+2*i);controls[i]=s16(p+72+2*i);}
        for(size_t i=0;i<4;++i)sources[i]=u32(p+92+4*i);
        const auto before=parameters;const auto oldControls=controls;const auto oldSources=sources;
        size_t mask=0;std::array<unsigned,4> visits{};
        for(size_t i=0;i<4;++i)if(floorDivide(controls[i+4],2048)==1){mask|=size_t(1)<<i;const auto c=size_t(floorDivide(controls[i],4096));colours[c]=true;if(++visits[c]>1&&u32(p+68)==1)++serial;}
        masks[mask]=true;
        const auto result=controlEnvelope246Step(parameters,controls,sources,u32(p+68));
        check(result!=ControlEnvelope246Result::unsupported,"Valid ENV route rejected");
        const bool changed=result==ControlEnvelope246Result::applied;
        check(u32(e+272)==uint32_t(changed),"Callback return differs");
        if(changed)++applied;else{++inactive;check(parameters==before,"Inactive route changed parameters");}
        for(size_t i=0;i<8;++i)if(parameters[i]!=s16(e+280+i*2)){std::cerr<<"Record "<<test<<", parameter "<<i<<'\n';throw std::runtime_error("Original ENV parameter differs");}
        if(changed)for(size_t i=0;i<4;++i){lowClamps+=parameters[i]==valueVoice246EnvelopeRanges[i][0];highClamps+=parameters[i]==valueVoice246EnvelopeRanges[i][1];}
        check(controls==oldControls&&sources==oldSources,"C++ mutated source inputs");
        check(std::memcmp(p+44,e+276,4)==0&&std::memcmp(p+64,e+296,4)==0,"Parameter guards changed");
        check(std::memcmp(p+88,e+344,24)==0&&std::memcmp(p+112,e+368,16)==0&&std::memcmp(p+72,e+384,16)==0&&std::memcmp(p+408,e+400,32)==0,"Source/self/controls/ranges changed");
        std::array<int16_t,128> samples{};for(size_t i=0;i<128;++i)samples[i]=s16(p+144+i*2);
        adsr246Block(state,{{parameters[0],parameters[1],parameters[2],parameters[3]}},s16(p+128),noise,samples);
        for(size_t i=0;i<128;++i)if(samples[i]!=s16(e+8+i*2)){std::cerr<<"Record "<<test<<", sample "<<i<<'\n';throw std::runtime_error("Following ADSR sample differs");}
        check(std::memcmp(p+136,e,8)==0&&std::memcmp(p+400,e+264,8)==0,"Output guards changed");
        const std::array<uint32_t,8> image{{left,state.stage,state.blocks,uint32_t(state.level),state.field12,state.field16,state.gain,right}};
        for(size_t i=0;i<8;++i)check(image[i]==u32(e+304+4*i),"Complete ADSR state/guards differ");
        check(state.finished==(u32(e+300)!=0)&&noise.seed==u32(e+336)&&uint32_t(noise.previous)==u32(e+340),"Completion/shared RNG/dither differs");
    }
    for(bool v:masks)check(v,"Missing channel mask");for(bool v:colours)check(v,"Missing ENV colour");for(bool v:stages)check(v,"Missing ADSR stage");
    check(applied==872&&inactive==152&&serial==142&&lowClamps==663&&highClamps==707,"Unexpected serial/clamp coverage");
    std::array<int16_t,8> params{{1000,1000,1000,1000,11,22,33,44}},controls{{0,0,0,0,2048,2048,2048,2048}};const auto before=params;
    for(int16_t destination:{int16_t(-1),int16_t(0),int16_t(6144),int16_t(8192)}){controls[7]=destination;check(controlEnvelope246Step(params,controls,{{127,127,127,127}},1)==ControlEnvelope246Result::unsupported&&params==before,"Late invalid destination mutated parameters");}
    controls[7]=2048;for(int16_t colour:{int16_t(-1),int16_t(16384),int16_t(28672)}){controls[3]=colour;check(controlEnvelope246Step(params,controls,{{127,127,127,127}},1)==ControlEnvelope246Result::unsupported&&params==before,"Auxiliary colour accepted or mutated parameters");}
    std::cout<<"PASS: 1024 four-source ENV calls and 131072 ADSR samples; complete state/guards/shared dither; "<<applied<<" applied / "<<inactive<<" inactive, "<<serial<<" serial updates, "<<lowClamps<<" low / "<<highClamps<<" high clamps\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
