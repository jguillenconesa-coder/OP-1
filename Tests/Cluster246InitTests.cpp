// SPDX-License-Identifier: AGPL-3.0-only
#include "Cluster246.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing Cluster initialization fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){return cluster246Wrap16(int(p[0])+(int(p[1])<<8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected vector directory");const std::string dir=argv[1];
    const auto input=read(dir+"/cluster246-init-input.bin"),oracle=read(dir+"/cluster246-init-output.bin");
    check(input.size()==36+60*664&&oracle.size()==20+60*764,"Incomplete Cluster factory fixture");
    check(u32(input.data())==0x79&&u32(oracle.data())==0x79,"Invalid readable-entry mask");
    Cluster246Tables tables;
    for(int count=1;count<=8;++count){
        const auto known=cluster246ObservedNormalization(count);
        check(bool(known)==bool(0x79&(1<<(count-1))),"Unknown coefficient must remain unavailable");
        if(known){check(*known==s16(oracle.data()+4+(count-1)*2),"Observed normalization conversion differs");tables.normalization[size_t(count-1)]=*known;}
    }
    check(!cluster246ObservedNormalization(0)&&!cluster246ObservedNormalization(9),"Out-of-range coefficient available");
    std::array<Cluster246State,cluster246FactorySlots> pool{};
    static_assert(sizeof(pool)==648);
    for(size_t caseIndex=0;caseIndex<60;++caseIndex){
        const auto* p=input.data()+36+caseIndex*664;const auto* e=oracle.data()+20+caseIndex*764;
        std::memcpy(pool.data(),p,648);const auto index=int(u32(p+648));
        check(cluster246ConstructSlot(pool,index),"Valid factory index rejected");
        check(u32(e)==0xffb00000u+uint32_t(index)*108,"Original slot pointer differs");
        check(std::memcmp(pool.data(),e+4,648)==0,"Factory object/padding/neighbor state differs");
        Cluster246Context context;context.random=u32(p+660);
        std::array<int16_t,4> controls{};for(size_t i=0;i<4;++i)controls[i]=s16(p+652+i*2);
        check(bool(cluster246ObservedNormalization(controls[0]/2048)),"Factory reset used an unknown coefficient");
        check(cluster246Reset(pool[size_t(index)],context,controls,tables),"Factory reset rejected");
        check(std::memcmp(&pool[size_t(index)],e+652,108)==0&&context.random==u32(e+760),"Post-factory reset/RNG differs");
    }
    const auto previous=pool;
    for(int index:{-1,6,1000})check(!cluster246ConstructSlot(pool,index)&&std::memcmp(pool.data(),previous.data(),648)==0,"Invalid factory index changed pool");
    const auto in=read(dir+"/cluster246-known-input.bin"),expected=read(dir+"/cluster246-known-output.bin"),table=read(dir+"/verified-sine.bin");
    check(in.size()==640*812&&expected.size()==160+640*400&&table.size()>=16384,"Incomplete known-normalization fixture");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(table.data()+i*2);
    Cluster246State state;Cluster246Context context;std::array<int16_t,128> output{};
    for(size_t block=0;block<640;++block){
        const auto* p=in.data()+block*812;const auto* e=expected.data()+160+block*400;const auto action=u32(p+808);
        if(action&1){std::memcpy(&state,p,108);std::memcpy(context.weights.data(),p+776,16);std::memcpy(context.display.data(),p+792,16);context.random=u32(p+772);}
        std::array<int16_t,4> controls{};std::array<uint32_t,128> pitch{};
        for(size_t i=0;i<4;++i)controls[i]=s16(p+108+i*2);
        const auto count=controls[0]/2048;const auto normalization=cluster246ObservedNormalization(count);
        check(bool(normalization),"Known render used an unresolved count");
        for(size_t i=0;i<128;++i)pitch[i]=u32(p+116+i*4);
        for(size_t i=0;i<8;++i)tables.normalization[i]=s16(p+628+i*2);
        check(tables.normalization[size_t(count-1)]==*normalization,"Known render coefficient differs");
        for(size_t i=0;i<32;++i)tables.damping[i]=signedPhase(u32(p+644+i*4));
        if(action&2)check(cluster246Reset(state,context,controls,tables),"Known reset rejected");
        if(action&4)cluster246Release(state);
        check(cluster246Block(state,context,controls,tables,pitch,sine,output),"Known render rejected");
        if(action&8)cluster246DisplayDecay(context);
        for(size_t i=0;i<128;++i)check(output[i]==s16(e+i*2),"Known-normalization sample differs");
        check(std::memcmp(&state,e+256,108)==0,"Known-normalization state differs");
        for(size_t i=0;i<8;++i)check(context.weights[i]==s16(e+364+i*2)&&context.display[i]==s16(e+380+i*2),"Known-normalization context differs");
        check(context.random==u32(e+396),"Known-normalization RNG differs");
    }
    std::cout<<"PASS: 5 observed normalization conversions, 60 six-slot factory/reset states, 81920 original-kernel samples; counts 2/3/8 unresolved\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
