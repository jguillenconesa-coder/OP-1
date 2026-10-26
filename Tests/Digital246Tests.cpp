// SPDX-License-Identifier: AGPL-3.0-only
#include "Digital246.h"
#include "Engine.h"
#include "Digital246Presets.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <memory>
using namespace pocket::reference;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Digital fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){return wrap16(int32_t(p[0])+(int32_t(p[1])<<8));}
Digital246State state(const uint8_t* p){return {u32(p),u32(p+4),s16(p+8),s16(p+10),u32(p+12),u32(p+16),u32(p+20),u32(p+24),s16(p+28),s16(p+30)};}
std::array<uint32_t,8> words(const Digital246State& s){return {s.tag,s.phase,uint16_t(s.feedback)|(uint32_t(uint16_t(s.pad10))<<16),s.secondary,s.field16,s.maskPhase,s.field24,uint16_t(s.previousSine)|(uint32_t(uint16_t(s.previousXor))<<16)};}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto in=read(dir+"/digital246-input.bin"),expected=read(dir+"/digital246-output.bin"),table=read(dir+"/verified-sine.bin");
    check(in.size()%556==0&&expected.size()==in.size()/556*288&&table.size()>=16384,"Incomplete Digital fixture");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<sine.size();++i)sine[i]=s16(table.data()+i*2);
    Digital246State s;std::array<int16_t,128> out{};size_t failures=0;
    for(size_t block=0;block<in.size()/556;++block){const auto* p=in.data()+block*556;const auto* e=expected.data()+block*288;
        const auto action=u32(p+552);if(action==1)s=state(p);else if(action==2)s.reset();else if(action==3)s.construct();
        std::array<int16_t,4> controls{};std::array<uint32_t,128> pitch{};
        for(size_t j=0;j<4;++j)controls[j]=s16(p+32+j*2);
        for(size_t j=0;j<128;++j)pitch[j]=u32(p+40+j*4);
        digital246Block(s,controls,pitch,sine,out);
        for(size_t j=0;j<128;++j)if(out[j]!=s16(e+j*2)){if(failures++<8)std::cerr<<"block "<<block<<" sample "<<j<<" got "<<out[j]<<" expected "<<s16(e+j*2)<<'\n';}
        const auto got=words(s);for(size_t j=0;j<8;++j)if(got[j]!=u32(e+256+j*4)){if(failures++<8)std::cerr<<"block "<<block<<" state "<<j<<" got "<<std::hex<<got[j]<<" expected "<<u32(e+256+j*4)<<std::dec<<'\n';}
        if(failures>100)break;
    }
    check(failures==0,"Digital kernel differs from original instruction oracle");
    const auto ranges=read(dir+"/voice-contract.bin");
    for(size_t i=0;i<4;++i){check(pocket::digital246Min[i]==s16(ranges.data()+28+(8+i)*8),"Digital minimum differs from original range initializer");check(pocket::digital246Max[i]==s16(ranges.data()+30+(8+i)*8),"Digital maximum differs from original range initializer");}
    for(double rate:{8000.,22050.,44100.,48000.,88200.,96000.,192000.}){
        pocket::Phase246Config config;config.prepare(rate);pocket::Digital246Voice voice;const Digital246Coefficients c({0,16384,0,0});double energy=0;
        for(int i=0;i<int(rate);++i){const auto sample=voice.next(220,c,config);check(std::isfinite(sample)&&std::abs(sample)<1.1,"Digital rate adapter unstable");energy+=sample*sample;}
        check(energy>.01,"Digital rate adapter silent");
        const auto count=int64_t(std::floor(rate*config.step))+2;
        const auto expectedPhase=uint32_t(0x80000000u+uint64_t(count)*voice.output.increment);
        check(std::abs(int64_t(signedPhase(voice.state.phase-expectedPhase)))<=voice.output.increment,"Digital clock drifts with host sample rate");
    }
    auto engine=std::make_unique<pocket::Engine>();
    for(double rate:{44100.,48000.,96000.})for(const auto& patch:pocket::digital246Presets)for(bool nativeEnvelope:{false,true}){
        engine->prepare(rate);pocket::Parameters p;p.engine=0;p.digital246=true;p.digital246Envelope=nativeEnvelope;p.digital246Controls=patch.controls;p.digital246Adsr=patch.adsr;p.effect=0;p.attack=0;p.sustain=1;p.release=0;
        engine->noteOn(60,.8f);double energy=0;
        for(int i=0;i<int(rate/3);++i){const auto sample=engine->next(p);check(std::isfinite(sample)&&std::abs(sample)<1.1,"Digital preset unstable");energy+=sample*sample;}
        check(energy>.0000001,"Digital preset silent");
        p.digital246Adsr[3]=16383;engine->sustainPedal(1,true);engine->noteOff(60);for(int i=0;i<int(rate/50);++i)engine->next(p);
        engine->sustainPedal(1,false);for(int i=0;i<int(rate/2);++i)engine->next(p);check(engine->visualState(0).active==0,"Digital note stuck after pedal release");
    }
    std::cout<<"PASS: 262144 Digital samples / 2048 complete states, original ranges, clock at 7 host rates, 16 presets with both envelopes at 3 rates, pedal/release\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
