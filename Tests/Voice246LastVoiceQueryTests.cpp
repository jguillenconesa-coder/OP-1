// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246LastVoiceQuery.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){
    std::ifstream f(path,std::ios::binary);check(bool(f),"Missing last-voice query fixture");
    return {std::istreambuf_iterator<char>(f),{}};
}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void put(uint8_t* p,uint32_t value){for(unsigned i=0;i<4;++i)p[i]=uint8_t(value>>(i*8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/voice246-last-voice-query-input.bin");
    const auto oracle=read(dir+"/voice246-last-voice-query-output.bin");
    constexpr size_t cases=1024,imageBytes=3740,recordBytes=3756,resultBytes=3744;
    check(input.size()==cases*recordBytes&&oracle.size()==cases*resultBytes,"Wrong query dimensions");
    constexpr std::array<size_t,8> offsets{{0,64,2640,3416,3576,3584,3712,3728}};
    constexpr std::array<size_t,8> sizes{{64,2576,776,160,8,128,16,12}};
    constexpr std::array<bool,8> retain{{true,false,true,true,true,false,false,false}};
    std::array<uint8_t,imageBytes> image{};std::array<bool,129> lengths{};std::array<bool,6> returned{};
    size_t empty=0,fallback=0,withValid=0,repaired=0,changed=0,validReads=0,discardedReads=0,duplicates=0,zeroByte=0;
    std::set<uint32_t> discarded;
    for(size_t c=0;c<cases;++c){
        const auto* p=input.data()+c*recordBytes;const auto* e=oracle.data()+c*resultBytes;
        const bool reload=u32(p)!=0;check(reload==(c<512||(c-512)%64==0),"Wrong independent/stream action");
        for(size_t i=0;i<offsets.size();++i)
            if(reload||!retain[i])std::memcpy(image.data()+offsets[i],p+16+offsets[i],sizes[i]);
        const auto count=u32(image.data()+2632),prior=u32(image.data()+4);
        check(count<=128,"Invalid original list count");lengths[count]=true;if(!count)++empty;
        std::array<uint8_t,2560> entries{};std::memcpy(entries.data(),image.data()+72,2560);
        std::set<uint32_t> ids;size_t accepted=0;
        for(size_t i=0;i<count;++i){
            const auto word=u32(entries.data()+i*20+16);
            if(word<6){++accepted;++validReads;ids.insert(word);if(entries[i*20+8]==0)++zeroByte;}
            else{++discardedReads;discarded.insert(word);}
        }
        if(accepted){++withValid;if(accepted>ids.size())++duplicates;}else ++fallback;
        uint32_t persistent=prior;
        const auto result=voice246LastVoiceQuery(entries,count,persistent);
        check(result.has_value(),"Valid original query rejected");
        check(*result==u32(e)&&persistent==*result&&*result<6,"Original return/persistent differs");
        returned[*result]=true;if(prior>=6)++repaired;if(prior!=persistent)++changed;
        if(!accepted)check(persistent==prior,"Empty/no-valid list reset persistent");
        if(c<64){
            check(*result==(c%2?0u:5u),"Order query picked first/max instead of last valid identifier");
            if(c%2){
                const auto* previous=input.data()+(c-1)*recordBytes;
                check(u32(previous+16+2632)==count,"Reversed pair count changed");
                for(size_t i=0;i<count;++i)
                    check(std::memcmp(p+16+72+i*20,previous+16+72+(count-1-i)*20,20)==0,"Pair is not a full-record permutation");
            }
        }
        put(image.data()+4,persistent);
        if(std::memcmp(image.data(),e+4,imageBytes)!=0)
            throw std::runtime_error("Full list/persistent/neighbors/shared image differs at record "+std::to_string(c));
    }
    for(bool seen:lengths)check(seen,"Missing list length 0..128");for(bool seen:returned)check(seen,"Missing returned voice 0..5");
    check(empty>0&&fallback>100&&withValid>500&&repaired>100&&changed>100&&duplicates>0&&zeroByte>0&&discarded.size()==13,"Insufficient query coverage");
    // These are adapter-only exclusions, not original error-helper return values.
    std::array<uint8_t,2560> entries{};entries.fill(0xff);
    for(uint32_t bad:{6u,7u,0xffffffffu,0x80000000u,0x10000u,0x10000005u,0x7fffffffu}){
        auto persistent=bad;
        check(!voice246LastVoiceQuery(entries,0,persistent)&&persistent==bad,"Unsupported empty-list fallback invented a voice");
        check(!voice246LastVoiceQuery(entries,128,persistent)&&persistent==bad,"Unsupported no-valid-ID fallback changed memory");
    }
    uint32_t persistent=3;const auto before=entries;
    check(!voice246LastVoiceQuery(entries,129,persistent)&&persistent==3&&entries==before,"Own capacity rejection changed state");
    put(entries.data()+127*20+16,5);persistent=0xffffffffu;
    const auto repairedResult=voice246LastVoiceQuery(entries,128,persistent);
    check(repairedResult&&*repairedResult==5&&persistent==5,"Invalid previous word rejected before valid later entry could repair it");
    check(voice246LastVoiceQuery(entries,0,persistent)==std::optional<uint32_t>(5)&&persistent==5,"Following empty list lost retained value");
    std::cout<<"PASS: 1,024 original queries, all 129 list lengths; "<<withValid<<" lists with valid IDs / "<<fallback<<" fallbacks; "
             <<repaired<<" repaired prior words; "<<validReads<<" valid / "<<discardedReads<<" discarded reads; "
             <<"32 reversed pairs; full list, persistent, guards and 3,740 shared bytes\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
