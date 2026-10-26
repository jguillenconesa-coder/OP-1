// SPDX-License-Identifier: AGPL-3.0-only
#include "Value246.h"
#include "Value246Fx.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Value fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){const auto x=uint32_t(p[0])|(uint32_t(p[1])<<8);return int16_t(x<32768?int(x):int(x)-65536);}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto in=read(dir+"/value246-input.bin"),expected=read(dir+"/value246-output.bin"),table=read(dir+"/verified-sine.bin");
    check(in.size()==4096*96&&expected.size()==408+4096*68,"Incomplete Value oracle");
    for(size_t fx=0;fx<8;++fx)for(size_t c=0;c<8;++c)for(size_t j=0;j<3;++j)
        check(value246FxRanges[fx][c][j]==s16(expected.data()+fx*48+c*6+j*2),"Value FX range differs from original initializer");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<sine.size();++i)sine[i]=s16(table.data()+i*2);
    Value246State state;std::array<uint32_t,8> display{};size_t failures=0;
    for(size_t i=0;i<4096;++i){const auto* p=in.data()+i*96;const auto* e=expected.data()+408+i*68;
        const auto action=u32(p+88);
        if(action){state={u32(p),u32(p+4),uint16_t(s16(p+8)),uint16_t(s16(p+10))};for(size_t j=0;j<8;++j)display[j]=u32(p+12+j*4);}
        std::array<int16_t,8> controls{},targets{};for(size_t j=0;j<8;++j){controls[j]=s16(p+44+j*2);targets[j]=s16(p+60+j*2);}
        if(action==2)state.noteOn(controls);
        const auto result=value246FxStep(state,display,controls,u32(p+76),int(u32(p+80)),int(u32(p+84)),targets,sine);
        std::array<uint32_t,17> actual{};actual[0]=state.tag;actual[1]=state.phase;actual[2]=state.word|(uint32_t(state.padding)<<16);
        for(size_t j=0;j<8;++j)actual[j+3]=display[j];
        for(size_t j=0;j<4;++j)actual[j+11]=uint16_t(targets[j*2])|(uint32_t(uint16_t(targets[j*2+1]))<<16);
        actual[15]=result?1:0;float bpm=0;std::memcpy(&bpm,p+92,4);actual[16]=value246Tempo(bpm);
        for(size_t j=0;j<actual.size();++j)if(actual[j]!=u32(e+j*4)){
            if(failures++<12)std::cerr<<"case "<<i<<" word "<<j<<" got "<<std::hex<<actual[j]<<" expected "<<u32(e+j*4)<<std::dec<<'\n';
        }
        if(failures>100)break;
    }
    check(failures==0,"Value FX reconstruction differs from original instruction oracle");
    const std::array<int16_t,4> base{{6000,15000,20000,28000}};
    for(double rate:{8000.,22050.,44100.,48000.,88200.,96000.,192000.}){
        pocket::Value246Fx control;control.prepare(rate);
        for(int i=0;i<int(rate);++i)control.next(true,true,true,12288,24000,0,value246Tempo(120),base,sine);
        check(std::abs(double(control.ticks)-44100./64)<=1,"Value clock changes with host sample rate");
        const auto phase=control.state.phase;control.noteOn();control.next(true,true,true,12288,24000,0,value246Tempo(120),base,sine);
        check(control.state.phase==phase||control.state.phase==phase+value246Increment(12288,value246Tempo(120)),"FREE phase restarted on a note");
        control.noteOn();control.next(true,true,false,12288,24000,0,value246Tempo(120),base,sine);
        check(control.state.phase==0x80000000u||control.state.phase==0x80000000u+value246Increment(12288,value246Tempo(120)),"Value retrigger did not restart main phase");
        const auto paused=control.state.phase;for(int i=0;i<1024;++i)check(control.next(true,false,true,12288,24000,0,value246Tempo(120),base,sine)==base,"OFF destination changes Punch controls");
        check(control.state.phase==paused,"OFF destination advances original Value phase");
        for(int i=0;i<1024;++i)check(control.next(false,true,true,12288,24000,0,value246Tempo(120),base,sine)==base,"Disabled Value changes Punch controls");
    }
    std::cout<<"PASS: 4096 Value FX complete states, displays, target buffers and returns; 8 FX ranges, sync/free reset and tempo arithmetic; own clock at 7 rates\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
