// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246NoteAdjustment.h"
#include "Voice246NoteArgument.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){
    std::ifstream f(path,std::ios::binary);check(bool(f),"Missing adjustment fixture");
    return {std::istreambuf_iterator<char>(f),{}};
}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int32_t s32(uint32_t value){return int32_t(value<=0x7fffffffu?int64_t(value):int64_t(value)-4294967296ll);}
void put(uint8_t* p,uint32_t value){for(unsigned i=0;i<4;++i)p[i]=uint8_t(value>>(i*8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/voice246-note-adjustment-input.bin");
    const auto oracle=read(dir+"/voice246-note-adjustment-output.bin");
    constexpr size_t cases=904,imageBytes=3592,recordBytes=3720,snapshotBytes=3656,resultBytes=7316;
    check(input.size()==cases*recordBytes&&oracle.size()==cases*resultBytes,"Wrong adjustment fixture dimensions");
    constexpr std::array<size_t,7> offsets{{0,12,24,72,848,1008,3584}};
    constexpr std::array<size_t,7> sizes{{12,12,48,776,160,2576,8}};
    std::array<uint8_t,64> area{};std::array<uint8_t,imageBytes> shared{};
    std::array<bool,72> standalonePairs{};
    std::array<std::array<bool,72>,2> chainPairs{};
    std::array<bool,4> alignments{};
    size_t standalone=0,constructed=0,initializers=0,bypass=0,lookup=0,sameSlot=0,otherSlot=0,invalidIgnored=0,repeated=0;
    for(size_t c=0;c<cases;++c){
        const auto* p=input.data()+c*recordBytes;const auto* e=oracle.data()+c*resultBytes;
        const auto* after=e+4+snapshotBytes;
        const bool reload=u32(p)!=0,chain=u32(p+56)!=0;
        check(chain==(c>=72),"Wrong standalone/chain phase");
        check(reload==(c<648||(c-648)%64==0),"Wrong independent/stream action");
        if(reload)std::memcpy(area.data(),p+64,64);
        for(size_t i=0;i<offsets.size();++i)
            if(reload||i<2)std::memcpy(shared.data()+offsets[i],p+128+offsets[i],sizes[i]);
        std::array<uint32_t,8> table{};
        for(size_t i=0;i<8;++i)table[i]=u32(shared.data()+32+i*4);
        if(reload&&chain){
            for(size_t i=0;i<8;++i){
                check(voice246SetNoteAdjustment(table,int32_t(i),s32(u32(p+24+i*4))),"Valid initializer rejected");
                ++initializers;
            }
        }
        const auto slot=s32(u32(p+4)),adjustment=s32(u32(p+8));
        check(slot>=0&&slot<8&&adjustment>=-4&&adjustment<=4,"Invalid original setter fixture");
        const auto pair=size_t(slot)*9+size_t(adjustment+4);
        if(table[size_t(slot)]==uint32_t(adjustment))++repeated;
        const auto before=table;
        check(voice246SetNoteAdjustment(table,slot,adjustment),"Valid setter rejected");
        for(size_t i=0;i<8;++i){
            if(i!=size_t(slot))check(table[i]==before[i],"Setter changed another slot");
            put(shared.data()+32+i*4,table[i]);
        }
        check(u32(e)==4,"Original valid R0 observation differs");
        check(std::memcmp(area.data(),e+4,64)==0,"Setter changed argument/guards");
        if(std::memcmp(shared.data(),e+68,imageBytes)!=0)
            throw std::runtime_error("Original setter table/shared image differs at record "+std::to_string(c));
        if(chain){
            ++constructed;
            const auto discriminator=u32(shared.data()+4),selected=u32(shared.data()+16),shift=u32(p+20);
            check(shift<=12&&shift%4==0,"Wrong argument destination shift");alignments[shift/4]=true;
            chainPairs[discriminator==1?0:1][pair]=true;
            for(auto word:table)check(s32(word)>=-4&&s32(word)<=4,"Chain table did not come from valid setters");
            if(discriminator==1){++bypass;if(selected>=8)++invalidIgnored;}
            else{++lookup;check(selected<8,"Invalid constructor selected slot");if(selected==uint32_t(slot))++sameSlot;else ++otherSlot;}
            const size_t destination=16+shift;
            Voice246NoteArgument argument{};std::memcpy(argument.data(),area.data()+destination,16);
            const auto oldArgument=argument;
            check(voice246ConstructNoteArgument(argument,u32(p+12),u32(p+16),discriminator,selected,table),"Existing constructor rejected setter output");
            check(std::memcmp(argument.data()+9,oldArgument.data()+9,3)==0,"Constructor padding changed");
            std::memcpy(area.data()+destination,argument.data(),16);
        }else{++standalone;standalonePairs[pair]=true;}
        if(std::memcmp(area.data(),after,64)!=0)
            throw std::runtime_error("Setter/constructor complete argument and guards differ at record "+std::to_string(c));
        check(std::memcmp(shared.data(),after+64,imageBytes)==0,"Constructor changed table/globals/shared state");
    }
    for(bool seen:standalonePairs)check(seen,"Missing standalone slot/value pair");
    for(const auto& mode:chainPairs)for(bool seen:mode)check(seen,"Missing slot/value pair in one constructor branch");
    for(bool seen:alignments)check(seen,"Missing argument alignment");
    check(standalone==72&&constructed==832&&initializers==4640&&bypass>300&&lookup>400&&sameSlot>150&&otherSlot>150&&invalidIgnored>40&&repeated>100,"Insufficient setter/chain coverage");
    // Adapter policy only. Do not execute original assertions or continuations.
    const std::array<int32_t,12> badSlots{{std::numeric_limits<int32_t>::min(),-65536,-256,-1,8,9,255,256,65536,0x10000000,0x7fff0000,std::numeric_limits<int32_t>::max()}};
    const std::array<int32_t,12> badValues{{std::numeric_limits<int32_t>::min(),-65540,-65536,-32768,-5,5,255,256,65532,65536,0x7fff0004,std::numeric_limits<int32_t>::max()}};
    std::array<uint32_t,8> table{{0x89abcdef,0x76543210,0xffffffff,0,0x80000000,0x7fffffff,0x12345678,0xdeadbeef}};
    const auto before=table;
    for(auto slot:badSlots)for(int32_t value:{-4,0,4})
        check(!voice246SetNoteAdjustment(table,slot,value)&&table==before,"Invalid signed32 slot accepted/mutated");
    for(int32_t slot:{0,3,7})for(auto value:badValues)
        check(!voice246SetNoteAdjustment(table,slot,value)&&table==before,"Invalid signed32 value truncated/accepted/mutated");
    std::cout<<"PASS: 72 isolated slot/value pairs; 832 setter -> constructor calls; 4,640 original initializing setters; 3,592 shared bytes, full arguments/padding/guards; "
             <<bypass<<" bypass / "<<lookup<<" lookup, "<<sameSlot<<" updated / "<<otherSlot<<" other selected slots, "<<repeated<<" repeated target writes\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
