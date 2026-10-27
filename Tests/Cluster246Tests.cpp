// SPDX-License-Identifier: AGPL-3.0-only
#include "Cluster246.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Cluster fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){return cluster246Wrap16(int(p[0])+(int(p[1])<<8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto in=read(dir+"/cluster246-input.bin"),expected=read(dir+"/cluster246-output.bin"),table=read(dir+"/verified-sine.bin");
    check(in.size()==2048*812&&expected.size()==160+2048*400&&table.size()>=16384,"Incomplete Cluster fixture");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(table.data()+i*2);
    for(size_t i=0;i<4;++i){check(cluster246ControlMin[i]==s16(expected.data()+i*8),"Cluster original minimum differs");check(cluster246ControlMax[i]==s16(expected.data()+i*8+2),"Cluster original maximum differs");}
    for(size_t i=0;i<24;++i)check(cluster246Damping[i]==signedPhase(u32(expected.data()+64+i*4)),"Cluster original damping coefficient differs");
    Cluster246State state;Cluster246Context context;Cluster246Tables coefficients;
    std::array<int16_t,128> output{};size_t failures=0;
    for(size_t block=0;block<2048;++block){
        const auto* p=in.data()+block*812;const auto* e=expected.data()+160+block*400;
        const auto action=u32(p+808);
        if(action&1){std::memcpy(&state,p,108);std::memcpy(context.weights.data(),p+776,16);std::memcpy(context.display.data(),p+792,16);context.random=u32(p+772);}
        std::array<int16_t,4> controls{};std::array<uint32_t,128> pitch{};
        for(size_t i=0;i<4;++i)controls[i]=s16(p+108+i*2);
        for(size_t i=0;i<128;++i)pitch[i]=u32(p+116+i*4);
        for(size_t i=0;i<8;++i)coefficients.normalization[i]=s16(p+628+i*2);
        for(size_t i=0;i<32;++i)coefficients.damping[i]=signedPhase(u32(p+644+i*4));
        if(action&2)check(cluster246Reset(state,context,controls,coefficients),"Valid Cluster reset rejected");
        if(action&4)cluster246Release(state);
        check(cluster246Block(state,context,controls,coefficients,pitch,sine,output),"Valid Cluster block rejected");
        if(action&8)cluster246DisplayDecay(context);
        for(size_t i=0;i<128;++i)if(output[i]!=s16(e+i*2)){
            if(failures++<12)std::cerr<<"block "<<block<<" sample "<<i<<" got "<<output[i]<<" expected "<<s16(e+i*2)<<'\n';
        }
        const auto* bytes=reinterpret_cast<const uint8_t*>(&state);
        for(size_t i=0;i<108;++i)if(bytes[i]!=e[256+i]){
            if(failures++<12)std::cerr<<"block "<<block<<" state byte "<<i<<" got "<<unsigned(bytes[i])<<" expected "<<unsigned(e[256+i])<<'\n';
        }
        for(size_t i=0;i<8;++i){
            if(context.weights[i]!=s16(e+364+i*2)){if(failures++<12)std::cerr<<"block "<<block<<" weight "<<i<<" got "<<context.weights[i]<<" expected "<<s16(e+364+i*2)<<'\n';}
            if(context.display[i]!=s16(e+380+i*2)){if(failures++<12)std::cerr<<"block "<<block<<" display "<<i<<" got "<<context.display[i]<<" expected "<<s16(e+380+i*2)<<'\n';}
        }
        if(context.random!=u32(e+396)){if(failures++<12)std::cerr<<"block "<<block<<" seed differs\n";}
        if(failures>100)break;
    }
    check(failures==0,"Cluster differs from original isolated instruction outputs");
    const auto previous=state;const auto oldContext=context;const auto oldOutput=output;
    for(int blue:{-32768,0,2047,18432,32767}){
        const std::array<int16_t,4> invalid{{int16_t(blue),0,0,0}};
        std::array<uint32_t,128> pitch{};
        check(!cluster246Reset(state,context,invalid,coefficients),"Invalid Cluster reset accepted");
        check(!cluster246Block(state,context,invalid,coefficients,pitch,sine,output),"Invalid Cluster render accepted");
        check(std::memcmp(&state,&previous,108)==0&&context.random==oldContext.random&&context.weights==oldContext.weights&&context.display==oldContext.display&&output==oldOutput,"Rejected input changed Cluster data");
    }
    std::cout<<"PASS: 262144 Cluster samples / 2048 complete states, context, RNG; synthetic caller coefficients, no original L1 table or integration claimed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
