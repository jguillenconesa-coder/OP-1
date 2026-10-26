// SPDX-License-Identifier: AGPL-3.0-only
#include "Envelope246.h"
#include "Phase246.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing envelope oracle");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const std::vector<uint8_t>& b,size_t o){return uint16_t(b.at(o))|uint16_t(unsigned(b.at(o+1))<<8);}
int16_t s16(const std::vector<uint8_t>& b,size_t o){const auto v=u16(b,o);return int16_t(v<32768?v:int(v)-65536);}
uint32_t u32(const std::vector<uint8_t>& b,size_t o){return uint32_t(u16(b,o))|(uint32_t(u16(b,o+2))<<16);}
pocket::reference::Adsr246State stateAt(const std::vector<uint8_t>& b,size_t o){pocket::reference::Adsr246State s;s.stage=u32(b,o);s.blocks=u32(b,o+4);s.level=pocket::reference::signedPhase(u32(b,o+8));s.field12=u32(b,o+12);s.field16=u32(b,o+16);s.gain=u32(b,o+20);return s;}
void compare(const pocket::reference::Adsr246State& s,const pocket::reference::Adsr246Noise& n,const std::array<int16_t,128>& samples,const std::vector<uint8_t>& output,size_t o){
    for(size_t i=0;i<128;++i)check(samples[i]==s16(output,o+i*2),"ADSR sample differs from original kernel with deferred D-register model");
    check(s.stage==u32(output,o+256)&&s.blocks==u32(output,o+260)&&uint32_t(s.level)==u32(output,o+264)&&s.field12==u32(output,o+268)&&s.field16==u32(output,o+272)&&s.gain==u32(output,o+276),"ADSR final state differs from original kernel");
    check(s.finished==(u32(output,o+280)!=0)&&n.seed==u32(output,o+284)&&uint32_t(n.previous)==u32(output,o+288),"ADSR completion or shared dither state differs");
}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected oracle directory");const std::string root=std::string(argv[1])+"/";
    const auto input=read(root+"adsr246-input.bin"),output=read(root+"adsr246-output.bin"),stock=read(root+"adsr246-stock-output.bin");
    check(input.size()==4096*48&&output.size()==1024+4096*292&&stock.size()==output.size(),"ADSR oracle dimensions changed");check(stock!=output,"Expected stock simulator packet hazard is no longer present; investigate model provenance");
    for(size_t i=0;i<128;++i){check(uint32_t(pocket::reference::adsr246Attack[i])==u32(output,i*4),"Original attack coefficient differs");check(uint32_t(pocket::reference::adsr246Decay[i])==u32(output,512+i*4),"Original decay/release coefficient differs");}
    std::array<size_t,9> stages{};
    for(size_t block=0;block<4096;++block){const auto in=block*48,out=1024+block*292;auto state=stateAt(input,in);++stages[state.stage];pocket::reference::Adsr246Noise noise{u32(input,in+36),pocket::reference::signedPhase(u32(input,in+40))};std::array<int16_t,128> actual{};
        pocket::reference::adsr246Block(state,{s16(input,in+24),s16(input,in+26),s16(input,in+28),s16(input,in+30)},s16(input,in+32),noise,actual);compare(state,noise,actual,output,out);
    }
    for(auto count:stages)check(count>400,"Insufficient envelope branch coverage");
    const auto stream=read(root+"adsr246-stream-input.bin"),continuous=read(root+"adsr246-stream-output.bin");check(stream.size()==16384*48&&continuous.size()==16384*292,"Wrong continuous envelope oracle dimensions");
    pocket::reference::Adsr246State carried;pocket::reference::Adsr246Noise noise;size_t triggers=0,releases=0,stops=0;
    for(size_t block=0;block<16384;++block){const auto in=block*48;const std::array<int16_t,4> raw{{s16(stream,in+24),s16(stream,in+26),s16(stream,in+28),s16(stream,in+30)}};
        const auto action=u32(stream,in+44);
        if(action==1){carried=stateAt(stream,in);noise={u32(stream,in+36),pocket::reference::signedPhase(u32(stream,in+40))};carried.noteOn();++triggers;}
        else if(action==2){carried.noteOff(raw[2]);++releases;}
        else if(action==3){carried.fastRelease();++stops;}
        std::array<int16_t,128> samples{};pocket::reference::adsr246Block(carried,raw,s16(stream,in+32),noise,samples);compare(carried,noise,samples,continuous,block*292);
    }
    check(triggers==32&&releases==32&&stops==16,"Original note/release/stop callbacks not exercised");
    // Runtime source clock keeps the original envelope progression independent of host rate.
    double firstLevel=-1;
    const std::array<int16_t,4> raw{{8192,16383,32767,16383}};const pocket::reference::PdCoefficients pd({0,0,0,0});
    for(double rate:{8000.,22050.,44100.,48000.,88200.,96000.,192000.}){
        pocket::Phase246Config config;config.prepare(rate);pocket::Phase246Voice voice;
        for(int i=0;i<int(rate/10);++i){const auto v=voice.next(220,pd,config,&raw);check(std::isfinite(v),"Native envelope produces invalid audio");}
        const auto level=voice.envelopeLevel();if(firstLevel<0)firstLevel=level;check(std::abs(level-firstLevel)<.001,"Native envelope duration changes with host rate");
        for(int i=0;i<int(rate/10);++i)voice.next(220,pd,config,&raw,true);check(voice.envelopeDone(),"Native original-envelope release sticks");
    }
    std::cout<<"PASS: 256 original converted coefficients, 524288 random/boundary envelope samples + 2097152 continuous samples, 20480 complete states, all nine stages, original note/release/stop callbacks; native timing at 7 host rates\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
