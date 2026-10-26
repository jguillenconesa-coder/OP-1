// SPDX-License-Identifier: AGPL-3.0-only
#include "Punch246.h"
#include "Punch246Effect.h"
#include "Punch246Presets.h"
#include "Engine.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <memory>
using namespace pocket::reference;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Punch fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){const auto u=uint32_t(p[0])+(uint32_t(p[1])<<8);return int16_t(u<32768?int(u):int(u)-65536);}
Punch246State state(const uint8_t* p){Punch246State s;s.tag=u32(p);for(size_t i=0;i<24;++i){s.a[i]=signedPhase(u32(p+4+i*4));s.b[i]=signedPhase(u32(p+100+i*4));}s.pole=signedPhase(u32(p+196));s.meter=s16(p+200);s.padding=s16(p+202);s.slot=signedPhase(u32(p+204));return s;}
std::array<uint32_t,52> words(const Punch246State& s){std::array<uint32_t,52> out{};out[0]=s.tag;for(size_t i=0;i<24;++i){out[i+1]=uint32_t(s.a[i]);out[i+25]=uint32_t(s.b[i]);}out[49]=uint32_t(s.pole);out[50]=uint16_t(s.meter)|(uint32_t(uint16_t(s.padding))<<16);out[51]=uint32_t(s.slot);return out;}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto in=read(dir+"/punch246-input.bin"),expected=read(dir+"/punch246-output.bin"),table=read(dir+"/verified-sine.bin");
    check(in.size()%348==0&&expected.size()==540+in.size()/348*340,"Incomplete Punch fixture");
    std::array<int32_t,129> coefficients{};for(size_t i=0;i<129;++i)coefficients[i]=signedPhase(u32(expected.data()+i*4));
    check(coefficients==punch246Poles,"Runtime Punch table differs from original converted coefficients");
    for(size_t i=0;i<4;++i){check(pocket::punch246Min[i]==s16(expected.data()+516+i*6),"Punch minimum differs from original range initializer");check(pocket::punch246Max[i]==s16(expected.data()+518+i*6),"Punch maximum differs from original range initializer");}
    std::array<int16_t,8192> sine{};for(size_t i=0;i<sine.size();++i)sine[i]=s16(table.data()+i*2);
    Punch246State s;std::array<int16_t,64> audio{};size_t failures=0;pocket::Punch246Effect streaming;streaming.prepare(44100);streaming.removeDC=false;streaming.awake=true;
    for(size_t block=0;block<in.size()/348;++block){const auto* p=in.data()+block*348;const auto* e=expected.data()+540+block*340;
        const auto action=u32(p+344);if(action==1)s=state(p);else if(action==2)s.reset();
        if(action==1)streaming.state=state(p);else if(action==2)streaming.state.reset();
        std::array<int16_t,4> controls{};for(size_t j=0;j<4;++j)controls[j]=s16(p+208+j*2);for(size_t j=0;j<64;++j)audio[j]=s16(p+216+j*2);
        punch246Block(s,controls,coefficients,sine,audio);
        for(size_t j=0;j<64;++j)check(streaming.next(s16(p+216+j*2)/32768.f,controls,sine)*32768.f==s16(e+j*2),"Streaming 44.1 kHz Punch differs from original block output");
        check(words(streaming.state)==words(s),"Streaming Punch block state differs from block kernel");
        for(size_t j=0;j<64;++j)if(audio[j]!=s16(e+j*2)){if(failures++<8)std::cerr<<"block "<<block<<" sample "<<j<<" got "<<audio[j]<<" expected "<<s16(e+j*2)<<'\n';}
        const auto got=words(s);for(size_t j=0;j<52;++j)if(got[j]!=u32(e+128+j*4)){if(failures++<8)std::cerr<<"block "<<block<<" state "<<j<<" got "<<std::hex<<got[j]<<" expected "<<u32(e+128+j*4)<<std::dec<<'\n';}
        if(uint16_t(s.meter)!=u32(e+336)){if(failures++<8)std::cerr<<"Display meter mismatch\n";}
        if(failures>100)break;
    }
    check(failures==0,"Punch kernel differs from original instruction oracle");
    for(double rate:{8000.,22050.,44100.,48000.,88200.,96000.,192000.}){
        pocket::Punch246Effect effect;effect.prepare(rate);const std::array<int16_t,4> controls{6000,15000,20000,28000};
        for(int i=0;i<int(rate/2);++i){const auto v=effect.next(.1f,controls,sine);check(std::isfinite(v)&&std::abs(v)<=1,"Punch rate adapter unstable");}
        const auto clock=rate==44100?int64_t(rate/2):1+int64_t(std::floor((int(rate/2)-1)*44100.0/rate));
        check(std::abs(int64_t(effect.internalSamples)-clock)<=1,"Punch clock changes with host rate");
        effect.reset();for(int i=0;i<512;++i)check(effect.next(0,controls,sine)==0,"Reset Punch emits self-generated sound before a new input");
    }
    auto engine=std::make_unique<pocket::Engine>();
    for(double rate:{44100.,48000.,96000.})for(const auto& patch:pocket::punch246Presets){
        pocket::Parameters p;p.engine=1;p.phase246=true;p.phase246Envelope=true;p.phase246Controls={0,29491,0,0};p.phase246Adsr={64,8064,32767,16383};p.punch246=true;p.punch246Controls=patch.controls;p.effect=0;
        engine->prepare(rate);engine->noteOn(60,.8f);double energy=0;
        for(int i=0;i<int(rate/2);++i){const auto v=engine->next(p);check(std::isfinite(v)&&std::abs(v)<1.1,"Integrated Punch is unstable");energy+=v*v;}
        check(energy>.0001,"Integrated Punch is silent");engine->reset();for(int i=0;i<1024;++i)check(engine->next(p)==0,"Panic restarts Punch feedback");
    }
    std::cout<<"PASS: 131072 Punch samples / 2048 complete states and display words; streaming parity, original tables/ranges, adapter at 7 host rates and integrated panic\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
