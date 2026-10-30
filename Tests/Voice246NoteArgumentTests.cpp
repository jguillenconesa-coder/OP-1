// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246NoteArgument.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){
    std::ifstream f(path,std::ios::binary);check(bool(f),"Missing note-argument fixture");
    return {std::istreambuf_iterator<char>(f),{}};
}
uint32_t u32(const uint8_t* p){
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/voice246-note-argument-input.bin");
    const auto oracle=read(dir+"/voice246-note-argument-output.bin");
    constexpr size_t cases=1280,imageBytes=3592,recordBytes=3672,resultBytes=3656;
    check(input.size()==cases*recordBytes&&oracle.size()==cases*resultBytes,"Wrong constructor fixture dimensions");
    constexpr std::array<size_t,7> offsets{{0,12,24,72,848,1008,3584}};
    constexpr std::array<size_t,7> sizes{{12,12,48,776,160,2576,8}};
    std::array<uint8_t,64> area{};
    std::array<uint8_t,imageBytes> shared{};
    std::array<bool,8> slots{};
    std::array<bool,4> alignments{};
    std::array<bool,32> noteBits{},velocityBits{},noteComplements{},velocityComplements{};
    size_t bypass=0,lookup=0,invalidIgnored=0,addWraps=0,multiplyWraps=0,nonzeroPadding=0;
    for(size_t c=0;c<cases;++c){
        const auto* p=input.data()+c*recordBytes;const auto* e=oracle.data()+c*resultBytes;
        const bool reload=u32(p)!=0;
        check(reload==(c<1024||(c-1024)%64==0),"Wrong independent/stream action");
        if(reload)std::memcpy(area.data(),p+16,area.size());
        for(size_t i=0;i<offsets.size();++i)
            if(reload||i<3)std::memcpy(shared.data()+offsets[i],p+80+offsets[i],sizes[i]);
        const auto before=area;const auto oldShared=shared;
        const auto shift=u32(p+12);check(shift<=12&&shift%4==0,"Wrong destination alignment");
        alignments[shift/4]=true;const size_t destination=16+shift;
        Voice246NoteArgument argument{};
        std::memcpy(argument.data(),area.data()+destination,argument.size());
        const auto oldArgument=argument;
        const uint32_t note=u32(p+4),velocity=u32(p+8);
        const uint32_t discriminator=u32(shared.data()+4),slot=u32(shared.data()+16);
        std::array<uint32_t,8> table{};for(size_t i=0;i<8;++i)table[i]=u32(shared.data()+32+i*4);
        if(discriminator==1){++bypass;if(slot>=8)++invalidIgnored;}
        else{
            ++lookup;check(slot<8,"Invalid lookup fixture index");slots[slot]=true;
            if(uint64_t(table[slot])*12>0xffffffffu)++multiplyWraps;
        }
        if(uint64_t(note)+53>0xffffffffu)++addWraps;
        for(unsigned bit=0;bit<32;++bit){
            noteBits[bit]=noteBits[bit]||note==(uint32_t(1)<<bit);
            velocityBits[bit]=velocityBits[bit]||velocity==(uint32_t(1)<<bit);
            noteComplements[bit]=noteComplements[bit]||note==(0xffffffffu^(uint32_t(1)<<bit));
            velocityComplements[bit]=velocityComplements[bit]||velocity==(0xffffffffu^(uint32_t(1)<<bit));
        }
        check(voice246ConstructNoteArgument(argument,note,velocity,discriminator,slot,table),"Valid original context rejected");
        check(u32(argument.data()+4)==velocity&&argument[8]==1&&u32(argument.data()+12)==0,"Field width/value contract differs");
        check(std::memcmp(argument.data()+9,oldArgument.data()+9,3)==0,"Argument padding was overwritten");
        if(oldArgument[9]||oldArgument[10]||oldArgument[11])++nonzeroPadding;
        std::memcpy(area.data()+destination,argument.data(),argument.size());
        if(std::memcmp(area.data(),e,64)!=0){
            std::cerr<<"Constructor case "<<c<<", note "<<note<<", velocity "<<velocity<<", discriminator "<<discriminator<<", slot "<<slot<<'\n';
            throw std::runtime_error("Complete argument/destination/guards differ");
        }
        check(std::memcmp(shared.data(),e+64,imageBytes)==0&&shared==oldShared,"Original or C++ changed shared context");
        for(size_t i=0;i<64;++i)
            if(i<destination||i>=destination+16)check(area[i]==before[i],"Destination neighbor changed");
    }
    for(bool seen:slots)check(seen,"Missing selected table slot");
    for(bool seen:alignments)check(seen,"Missing destination alignment");
    for(size_t i=0;i<32;++i)check(noteBits[i]&&velocityBits[i]&&noteComplements[i]&&velocityComplements[i],"Missing full-width walking bit/complement");
    check(bypass==160&&lookup==1120&&invalidIgnored==106&&addWraps==80&&multiplyWraps==847&&nonzeroPadding==1088,"Unexpected constructor branch/boundary coverage");
    // The original constructor has no index guard. Rejection here is a bounded
    // adapter policy; bypass must not require a valid index or consult the table.
    Voice246NoteArgument rejected{};rejected.fill(0xa5);const auto before=rejected;
    for(uint32_t slot:{8u,0xffffffffu,0x10000000u}){
        check(!voice246ConstructNoteArgument(rejected,0,127,0,slot,{})&&rejected==before,"Invalid index mutated argument");
        check(!voice246ConstructNoteArgument(rejected,0,127,0x10001,slot,{})&&rejected==before,"Discriminator was truncated");
    }
    check(voice246ConstructNoteArgument(rejected,0,0xabcdef01,1,0xffffffffu,{})&&u32(rejected.data())==53&&u32(rejected.data()+4)==0xabcdef01,"Bypass consulted invalid index or truncated velocity");
    std::cout<<"PASS: 1,280 complete original note arguments; 160 bypass / 1,120 table lookups; "
             <<invalidIgnored<<" ignored invalid indices; 3,592 preserved shared bytes per call; guards, padding, walking bits and wrap\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
