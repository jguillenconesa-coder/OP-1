// SPDX-License-Identifier: AGPL-3.0-only
#include "Envelope246Transition.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing ADSR transition fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){const auto v=u16(p);return int16_t(v<32768?int(v):int(v)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
Adsr246State stateAt(const uint8_t* p){Adsr246State s;s.stage=u32(p);s.blocks=u32(p+4);s.level=signedPhase(u32(p+8));s.field12=u32(p+12);s.field16=u32(p+16);s.gain=u32(p+20);return s;}
void compareState(const Adsr246State& state,const uint8_t* p){
    const std::array<uint32_t,6> words{{state.stage,state.blocks,uint32_t(state.level),state.field12,state.field16,state.gain}};
    for(size_t i=0;i<6;++i)check(words[i]==u32(p+i*4),"Original transition state differs");
}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto input=read(dir+"/adsr246-transition-input.bin"),oracle=read(dir+"/adsr246-transition-output.bin");
    check(input.size()==2560*56&&oracle.size()==2560*316,"Incomplete transition fixtures");
    Adsr246State state;Adsr246Noise noise;std::array<size_t,6> actions{};
    std::array<std::array<bool,9>,6> priorStages{};size_t chainedRestarts=0;
    for(size_t block=0;block<2560;++block){
        const auto* p=input.data()+block*56;const auto* e=oracle.data()+block*316;
        const auto action=u32(p+52);const auto kind=action&255;
        check(kind<6,"Invalid transition action");++actions[kind];
        if(action&256){state=stateAt(p);noise={u32(p+44),signedPhase(u32(p+48))};}
        if(block<512){check(state.stage<9,"Invalid initial stage");priorStages[kind][state.stage]=true;}
        if(kind==5&&state.stage==7)++chainedRestarts;
        switch(kind){
            case 1:adsr246Initialize(state);break;
            case 2:state.noteOn();break;
            case 3:state.noteOff(s16(p+34));break;
            case 4:state.fastRelease();break;
            case 5:adsr246RequestRestart(state,u32(p+36),u16(p+40));break;
            default:break;
        }
        compareState(state,e);
        const std::array<int16_t,4> controls{{s16(p+24),s16(p+26),s16(p+28),s16(p+30)}};
        std::array<int16_t,128> output{};
        adsr246Block(state,controls,s16(p+32),noise,output);
        for(size_t i=0;i<128;++i)check(output[i]==s16(e+24+i*2),"Post-transition ADSR sample differs");
        compareState(state,e+280);
        check(state.finished==(u32(e+304)!=0)&&noise.seed==u32(e+308)&&uint32_t(noise.previous)==u32(e+312),"Transition completion/shared dither differs");
    }
    for(const auto& stages:priorStages)for(bool covered:stages)check(covered,"Callback missing a prior stage");
    for(auto count:actions)check(count>80,"Insufficient callback coverage");
    check(chainedRestarts>=16,"Back-to-back restart not covered");
    std::cout<<"PASS: 327680 ADSR samples, 2560 complete pre/post-callback/render states and dither; all six actions from all nine prior stages; "<<chainedRestarts<<" restarts from stage 7\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
