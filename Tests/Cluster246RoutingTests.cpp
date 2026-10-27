// SPDX-License-Identifier: AGPL-3.0-only
#include "Cluster246Routing.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Cluster routing fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){return cluster246Wrap16(int(p[0])+(int(p[1])<<8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto input=read(dir+"/cluster246-routing-input.bin"),oracle=read(dir+"/cluster246-routing-output.bin"),table=read(dir+"/verified-sine.bin");
    check(input.size()==144+1024*1340&&oracle.size()==1024*1104&&table.size()>=16384,"Incomplete Cluster routing fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(table.data()+i*2);
    Cluster246Tables tables;
    for(size_t i=0;i<8;++i)tables.normalization[i]=s16(input.data()+i*2);
    for(size_t i=0;i<32;++i)tables.damping[i]=signedPhase(u32(input.data()+16+i*4));
    Cluster246Pool pool{};Cluster246Context context;Cluster246SoundControls sounds{};
    size_t accepted=0,rejected=0,mismatchedReleases=0,factories=0;
    std::array<bool,6> visited{};std::array<bool,8> slots{};
    for(size_t caseIndex=0;caseIndex<1024;++caseIndex){
        const auto* p=input.data()+144+caseIndex*1340;const auto* e=oracle.data()+caseIndex*1104;
        const auto action=u32(p);const int voice=int(u32(p+4)),loaded=int(u32(p+8)),selected=int(u32(p+12));
        visited[size_t(voice)]=true;slots[size_t(loaded)]=true;
        if(action&1){std::memcpy(pool.data(),p+16,648);std::memcpy(context.weights.data(),p+664,16);std::memcpy(context.display.data(),p+680,16);context.random=u32(p+696);}
        for(size_t sound=0;sound<8;++sound)for(size_t knob=0;knob<8;++knob)sounds[sound][knob]=s16(p+700+sound*16+knob*2);
        if(action&2){++factories;for(int slot=0;slot<6;++slot)check(cluster246ConstructSlot(pool,slot),"Factory rejected slot");}
        uint32_t result=0xffffffffu;
        if(action&4){result=cluster246CoreNoteOn(pool,context,tables,sounds,loaded,selected,voice)?1u:0u;result?++accepted:++rejected;}
        check(result==u32(e),"Core note-on return differs");
        if(action&8){check(cluster246CoreRelease(pool,voice),"Core release rejected slot");if(loaded!=selected)++mismatchedReleases;}
        const auto& sound=sounds[size_t(loaded)];
        const std::array<int16_t,4> controls{{sound[0],sound[1],sound[2],sound[3]}};
        check(bool(cluster246ObservedNormalization(controls[0]/2048)),"Routing selected an unresolved normalization");
        std::array<uint32_t,128> pitch{};for(size_t i=0;i<128;++i)pitch[i]=u32(p+828+i*4);
        std::array<int16_t,128> output{};
        check(cluster246Block(pool[size_t(voice)],context,controls,tables,pitch,sine,output),"Render rejected controls");
        if(action&32)cluster246DisplayDecay(context);
        for(size_t i=0;i<128;++i)check(output[i]==s16(e+4+i*2),"Interleaved Cluster sample differs");
        check(std::memcmp(pool.data(),e+260,648)==0,"Full six-slot pool/padding differs");
        for(size_t i=0;i<8;++i)check(context.weights[i]==s16(e+908+i*2)&&context.display[i]==s16(e+924+i*2),"Shared context differs");
        check(context.random==u32(e+940),"Shared RNG differs");
        for(size_t i=0;i<6;++i)check(u32(e+944+i*4)==0xffb00000u+uint32_t(i)*108,"Core factory pointer differs");
        check(u32(e+968)==uint32_t(loaded)&&u32(e+972)==uint32_t(selected),"Core sound IDs mutated");
        for(size_t soundIndex=0;soundIndex<8;++soundIndex)for(size_t knob=0;knob<8;++knob)
            check(sounds[soundIndex][knob]==s16(e+976+soundIndex*16+knob*2),"Source sound controls mutated");
    }
    check(accepted&&rejected&&mismatchedReleases&&factories,"Missing routing branches");
    for(bool used:visited)check(used,"Missing voice index");for(bool used:slots)check(used,"Missing sound slot");
    // Invalid C++ inputs are adapter guards, separate from original assert paths.
    const auto previous=pool;const auto oldContext=context;
    for(int index:{-1,6,1000}){
        check(!cluster246CoreNoteOn(pool,context,tables,sounds,0,0,index),"Invalid note voice accepted");
        check(!cluster246CoreRelease(pool,index),"Invalid release voice accepted");
    }
    for(int index:{-1,8,1000}){
        check(!cluster246CoreNoteOn(pool,context,tables,sounds,index,index,0),"Invalid sound accepted");
        check(!cluster246CoreNoteOn(pool,context,tables,sounds,0,index,0),"Invalid selected sound accepted");
    }
    check(std::memcmp(pool.data(),previous.data(),648)==0&&std::memcmp(&context,&oldContext,sizeof(context))==0,"Rejected routing mutated state");
    sounds[0][0]=0;
    check(!cluster246CoreNoteOn(pool,context,tables,sounds,0,0,0)&&std::memcmp(pool.data(),previous.data(),648)==0&&std::memcmp(&context,&oldContext,sizeof(context))==0,"Invalid controls mutated state");
    std::cout<<"PASS: 131072 interleaved samples, 1024 full six-slot pools; "<<accepted<<" accepted / "<<rejected<<" rejected note-ons, "<<mismatchedReleases<<" releases across sound mismatch, "<<factories<<" Core factories\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
