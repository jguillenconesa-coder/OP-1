// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246PitchBend.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing bend fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void put32(uint8_t* p,uint32_t x){for(size_t i=0;i<4;++i)p[i]=uint8_t(x>>(i*8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/voice246-pitch-bend-input.bin"),oracle=read(dir+"/voice246-pitch-bend-output.bin");
    constexpr size_t cases=65536+256;
    check(input.size()==cases*32&&oracle.size()==cases*72,"Incomplete bend fixtures");
    std::array<uint8_t,16> control{};std::array<uint32_t,6> registers{};
    std::array<bool,256> statuses{};std::array<bool,65536> bytePairs{};
    size_t legal=0,negative=0,center=0,positive=0;
    for(size_t test=0;test<cases;++test){
        const auto* p=input.data()+test*32;const auto* e=oracle.data()+test*72;
        check(u32(p)==uint32_t(test<=65536),"Unexpected reload schedule");
        if(u32(p)){
            std::copy_n(p+8,16,control.begin());
            for(size_t i=0;i<6;++i)registers[i]=u32(p+24)+uint32_t(i);
        }
        const auto before=control;
        check(voice246DecodePitchBend(control,p+4,3),"Valid original decoder input rejected");
        for(size_t i=0;i<16;++i){
            if(control[i]!=e[i]){std::cerr<<"Bend case "<<test<<", byte "<<i<<'\n';throw std::runtime_error("Original bend control image differs");}
            if(i!=4&&i!=5&&i!=6&&i!=10&&i!=11)check(control[i]==before[i],"Decoder changed neighbor byte");
        }
        std::array<uint8_t,32> source{};
        for(size_t i=0;i<8;++i)put32(source.data()+i*4,u32(p+24));
        std::copy_n(p+4,4,source.begin()+4);
        put32(source.data()+8,u32(p+28));put32(source.data()+12,0x02f02004);
        put32(source.data()+16,0x02f02007);put32(source.data()+20,0x02f02008);
        check(std::equal(source.begin(),source.end(),e+16),"Original decoder changed source/vector/guards");
        for(size_t i=0;i<6;++i)check(registers[i]==u32(e+48+i*4),"Original decoder changed per-voice pitch");
        if(test<65536){
            const auto pair=unsigned(p[5])|(unsigned(p[6])<<8);
            check(!bytePairs[pair],"Duplicate independent pair");bytePairs[pair]=true;statuses[p[4]]=true;
            legal+=p[5]<128&&p[6]<128;
            const auto raw=unsigned(p[5])+(unsigned(p[6])<<7);
            negative+=raw<8192;center+=raw==8192;positive+=raw>8192;
        }
    }
    for(bool used:bytePairs)check(used,"Missing independent byte pair");
    for(bool used:statuses)check(used,"Missing status stress byte");
    check(legal==16384&&negative&&center&&positive,"Missing bend branch");
    const auto unchanged=control;
    const uint8_t message[3]={0xe0,0,64};
    for(size_t n:{size_t(0),size_t(1),size_t(2)})check(!voice246DecodePitchBend(control,message,n),"Truncated message accepted");
    check(!voice246DecodePitchBend(control,nullptr,3)&&control==unchanged,"Rejected decoder input mutated state");
    // Status, channel and a trailing byte are outside this decoder's contract.
    std::array<uint8_t,16> a{},b{};const uint8_t extended[4]={0,0,64,255};
    check(voice246DecodePitchBend(a,message,3)&&voice246DecodePitchBend(b,extended,4)&&a==b,"Decoder used status or trailing byte");
    std::cout<<"PASS: 65536 independent bend byte pairs (16384 MIDI-valid), 256 continuous calls, control/source guards and six preserved pitch registers; no audio rendered\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
