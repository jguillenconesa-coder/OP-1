// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246Routing.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing routing fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint16_t u16(const uint8_t* p){return uint16_t(unsigned(p[0])|(unsigned(p[1])<<8));}
int16_t s16(const uint8_t* p){auto x=u16(p);return int16_t(x<32768?int(x):int(x)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto base=read(dir+"/drwave246-kernel-input.bin"),events=read(dir+"/drwave246-routing-overrides.bin"),oracle=read(dir+"/drwave246-routing-output.bin"),sineData=read(dir+"/verified-sine.bin");
    check(base.size()==512*9316&&events.size()==512*148&&oracle.size()==327680+512*9164&&sineData.size()>=16384,"Incomplete routing fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;DrWave246Context initial;
    drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data(),sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Routing base banks differ/changed");
    DrWave246Pool pool;drWave246CoreConstruct(pool);
    size_t factories=0,accepted=0,rejected=0,releases=0,mismatchReleases=0,updated=0,readOnly=0,inactive=0;
    std::array<bool,6> voices{},updateVoices{},resetVoices{};std::array<bool,8> soundsUsed{};
    std::array<bool,6> spectra{};std::array<bool,3> transforms{};
    for(size_t test=0;test<512;++test){
        const auto* p=base.data()+test*9316;const auto* a=events.data()+test*148;const auto* e=oracle.data()+163840+test*9164;
        const auto action=u32(a),slot=u32(a+4),counter=u32(a+8),loaded=u32(a+12),selected=u32(a+16),enabled=u32(p+12);
        check(slot<6&&loaded<8&&selected<8,"Invalid routing fixture index");voices[slot]=true;soundsUsed[loaded]=true;
        if(action&1){
            std::memcpy(pool.data(),p+536,216);for(auto& state:pool)state.tag=0x0190fb2c;
            std::memcpy(waves.data(),p+752,6144);std::memcpy(&runtime.settings,p+6896,16);std::memcpy(&runtime.transform,p+6912,2072);
            for(size_t i=0;i<5;++i)runtime.increments[i]=u32(p+8984+i*4);
            std::memcpy(runtime.configuration.data(),p+9004,32);runtime.wavePointer=u32(p+9036);runtime.transform.reciprocalInitialized=u32(p+9040);
        }
        runtime.settings.modeAndReady=(runtime.settings.modeAndReady&0xffff00ffu)|((enabled&255)<<8);
        DrWave246SoundControls sounds;for(size_t sound=0;sound<8;++sound)for(size_t i=0;i<8;++i)sounds[sound][i]=s16(a+20+sound*16+i*2);
        if(action&2){drWave246CoreConstruct(pool);++factories;}
        uint32_t noteResult=0xffffffff;
        if(action&4){
            noteResult=uint32_t(drWave246CoreNoteOn(pool,int(loaded),int(selected),int(slot)));
            if(noteResult){++accepted;resetVoices[slot]=true;}else ++rejected;
        }
        check(noteResult==u32(e+8784),"Original Core note-on result differs");
        if(action&8){check(drWave246CoreRelease(pool,int(slot)),"Valid release rejected");++releases;if(loaded!=selected)++mismatchReleases;}
        check(std::memcmp(pool.data(),e+8948,216)==0,"Pre-render Core voice pool differs");
        std::array<int16_t,4> controls{};for(size_t i=0;i<4;++i)controls[i]=sounds[loaded][i];
        std::array<uint32_t,128> pitch{};for(size_t i=0;i<128;++i)pitch[i]=u32(p+24+i*4);
        std::array<int16_t,128> out{};for(size_t i=0;i<128;++i)out[i]=s16(p+9052+i*2);
        const auto result=drWave246Block(pool[slot],waves,runtime,bank,sine,controls,pitch,int(counter&65535),out);
        check(result!=DrWave246ReadResult::unsupported,"Valid routed render rejected");
        if(result==DrWave246ReadResult::inactive)++inactive;
        else if(slot==(counter&65535)%6){
            ++updated;updateVoices[slot]=true;const auto& c=runtime.configuration;
            spectra[size_t(c[0])*2+(c[6]==1?0:1)]=true;spectra[size_t(c[1])*2+(c[6]==1?0:1)]=true;
            transforms[0]=true;if(c[4]<2)transforms[size_t(c[4])+1]=true;
        }else ++readOnly;
        for(size_t i=0;i<128;++i)if(out[i]!=s16(e+8+i*2)){
            std::cerr<<"Routing case "<<test<<", sample "<<i<<", got "<<out[i]<<", expected "<<s16(e+8+i*2)<<'\n';throw std::runtime_error("Routed Dr. Wave sample differs");
        }
        check(std::memcmp(p+9044,e,8)==0&&std::memcmp(p+9308,e+264,8)==0,"Routed output guard changed");
        check(std::memcmp(pool.data(),e+272,216)==0,"Post-render routed pool differs");
        check(std::memcmp(waves.data(),e+488,6144)==0,"Routed runtime waves differ");
        check(std::memcmp(&runtime.settings,e+6632,16)==0,"Routed shared settings differ");
        check(std::memcmp(&runtime.transform,e+6648,2072)==0&&runtime.transform.reciprocalInitialized==u32(e+8776),"Routed caches differ");
        for(size_t i=0;i<5;++i)check(runtime.increments[i]==u32(e+8720+i*4),"Routed increments differ");
        check(std::memcmp(runtime.configuration.data(),e+8740,32)==0&&runtime.wavePointer==u32(e+8772)&&counter==u32(e+8780),"Routed configuration/pointer/counter differs");
        for(size_t i=0;i<6;++i)check(u32(e+8788+i*4)==0xffb00000+i*36,"Original Core factory pointer differs");
        check(loaded==u32(e+8812)&&selected==u32(e+8816)&&std::memcmp(a+20,e+8820,128)==0,"Original routing IDs/controls changed");
    }
    for(bool used:voices)check(used,"Missing rendered voice");for(bool used:updateVoices)check(used,"Missing updated voice");
    for(bool used:resetVoices)check(used,"Missing accepted note-on voice");for(bool used:soundsUsed)check(used,"Missing loaded sound");
    for(bool used:spectra)check(used,"Missing routed spectrum");for(bool used:transforms)check(used,"Missing routed transform");
    check(accepted&&rejected&&mismatchReleases&&factories,"Missing Core event route");
    const auto old=pool;
    for(int voice:{-1,6}){
        check(!drWave246CoreNoteOn(pool,0,0,voice)&&!drWave246CoreRelease(pool,voice),"Invalid voice index accepted");
    }
    for(int sound:{-1,8})check(!drWave246CoreNoteOn(pool,sound,0,0)&&!drWave246CoreNoteOn(pool,0,sound,0),"Invalid sound index accepted");
    check(std::memcmp(pool.data(),old.data(),sizeof(pool))==0,"Invalid Core event mutated pool");
    std::cout<<"PASS: "<<(updated+readOnly)*128<<" routed Dr. Wave samples; "<<accepted<<" accepted / "<<rejected<<" rejected notes, "<<releases<<" releases ("<<mismatchReleases<<" mismatched sounds), "<<factories<<" factories; "<<updated<<" updates / "<<readOnly<<" reads / "<<inactive<<" inactive\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
