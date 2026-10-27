// SPDX-License-Identifier: AGPL-3.0-only
#include "Cluster246Activation.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing activation fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){return cluster246Wrap16(u16(p));}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put16(uint8_t* p,uint16_t x){p[0]=uint8_t(x);p[1]=uint8_t(x>>8);}
void put32(uint8_t* p,uint32_t x){put16(p,uint16_t(x));put16(p+2,uint16_t(x>>16));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto input=read(dir+"/cluster246-activation-input.bin"),oracle=read(dir+"/cluster246-activation-output.bin"),sineData=read(dir+"/verified-sine.bin");
    check(input.size()==144+1024*912&&oracle.size()==512+1024*1020&&sineData.size()>=16384,"Incomplete activation fixtures");
    for(size_t i=0;i<128;++i)check(uint32_t(voice246NotePitch[i])==u32(oracle.data()+i*4),"Original 128-note copy differs");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    Cluster246Tables tables;for(size_t i=0;i<8;++i)tables.normalization[i]=s16(input.data()+i*2);
    for(size_t i=0;i<32;++i)tables.damping[i]=signedPhase(u32(input.data()+16+i*4));
    Cluster246Pool pool{};Cluster246Context context;Cluster246SoundControls sounds{};Voice246Registers registers;
    std::array<uint8_t,64> registerImages{};std::array<bool,128> lookupUsed{};std::array<bool,6> voices{};
    size_t accepted=0,rejected=0,overrides=0;
    for(size_t block=0;block<1024;++block){
        const auto* p=input.data()+144+block*912;const auto* e=oracle.data()+512+block*1020;
        const auto action=u32(p);const int voice=int(u32(p+4)),loaded=int(u32(p+8)),selected=int(u32(p+12));
        const Voice246Note note{u32(p+16),u32(p+20),u32(p+24),signedPhase(u32(p+28))};
        if(action&1){
            std::memcpy(pool.data(),p+36,648);std::memcpy(context.weights.data(),p+684,16);std::memcpy(context.display.data(),p+700,16);context.random=u32(p+716);
            std::memcpy(registerImages.data(),p+720,64);
            for(size_t i=0;i<6;++i){registers.active[i]=p[720+i];registers.velocity[i]=s16(p+728+i*2);registers.pitch[i]=signedPhase(u32(p+752+i*4));}
        }
        for(size_t sound=0;sound<8;++sound)for(size_t knob=0;knob<8;++knob)sounds[sound][knob]=s16(p+784+sound*16+knob*2);
        if(action&2)for(int i=0;i<6;++i)check(cluster246ConstructSlot(pool,i),"Factory rejected slot");
        const bool activated=cluster246ActivateNote(pool,context,tables,sounds,registers,note,signedPhase(u32(p+32)),loaded,selected,voice);
        check(activated==(loaded==selected),"Adapter acceptance differs from tested Core gate");
        if(activated){++accepted;voices[size_t(voice)]=true;if(note.explicitPitch)++overrides;else lookupUsed[note.noteIndex]=true;}else ++rejected;
        for(size_t i=0;i<6;++i){registerImages[i]=registers.active[i];put16(registerImages.data()+8+i*2,uint16_t(registers.velocity[i]));put32(registerImages.data()+32+i*4,uint32_t(registers.pitch[i]));}
        check(std::memcmp(registerImages.data(),e+940,64)==0,"Activation pitch/velocity/flags or neighboring bytes differ");
        const auto& raw=sounds[size_t(loaded)];const std::array<int16_t,4> controls{{raw[0],raw[1],raw[2],raw[3]}};
        check(bool(cluster246ObservedNormalization(controls[0]/2048)),"Activation used an unknown normalization");
        std::array<uint32_t,128> held{};held.fill(uint32_t(registers.pitch[size_t(voice)]));std::array<int16_t,128> output{};
        check(cluster246Block(pool[size_t(voice)],context,controls,tables,held,sine,output),"Held-pitch render rejected controls");
        for(size_t i=0;i<128;++i)check(output[i]==s16(e+i*2),"Activation/render sample differs");
        check(std::memcmp(pool.data(),e+256,648)==0,"Activation/render pool state differs");
        for(size_t i=0;i<8;++i)check(context.weights[i]==s16(e+904+i*2)&&context.display[i]==s16(e+920+i*2),"Activation/render shared context differs");
        check(context.random==u32(e+936),"Activation/render RNG differs");
        check(std::memcmp(p+16,e+1004,16)==0,"Original activation mutated source note");
    }
    check(accepted&&rejected&&overrides,"Missing activation branches");for(bool used:lookupUsed)check(used,"Missing accepted note index");for(bool used:voices)check(used,"Missing accepted voice");
    const auto previousPool=pool;const auto previousContext=context;const auto previousRegisters=registers;const Voice246Note valid{};
    for(int voice:{-1,6,1000})check(!cluster246ActivateNote(pool,context,tables,sounds,registers,valid,0x40000000,0,0,voice),"Invalid voice accepted");
    for(uint32_t note:{128u,0xffffffffu})check(!cluster246ActivateNote(pool,context,tables,sounds,registers,Voice246Note{note,0,0,0},0x40000000,0,0,0),"Invalid lookup note accepted");
    check(!cluster246ActivateNote(pool,context,tables,sounds,registers,valid,0x40000000,-1,-1,0),"Invalid sound accepted");
    sounds[0][0]=0;check(!cluster246ActivateNote(pool,context,tables,sounds,registers,valid,0x40000000,0,0,0),"Invalid controls accepted");
    check(std::memcmp(pool.data(),previousPool.data(),648)==0&&std::memcmp(&context,&previousContext,sizeof(context))==0&&registers.active==previousRegisters.active&&registers.velocity==previousRegisters.velocity&&registers.pitch==previousRegisters.pitch,"Rejected activation mutated state");
    std::cout<<"PASS: 128 note-table words, 131072 scheduled Cluster samples and 1024 activation/pool/register records; "<<accepted<<" accepted, "<<rejected<<" gated, "<<overrides<<" explicit-pitch notes\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
