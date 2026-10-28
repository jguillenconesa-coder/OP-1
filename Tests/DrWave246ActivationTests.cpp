// SPDX-License-Identifier: AGPL-3.0-only
#include "DrWave246Activation.h"
#include "Voice246PitchUpdate.h"
#include "Voice246TuningIndex.h"
#include "Voice246TuningTable.h"
#include "DrWave246PitchRefresh.h"
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
int16_t s16(const uint8_t* p){auto x=u16(p);return int16_t(x<32768?int(x):int(x)-65536);}
uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
void put16(uint8_t* p,uint16_t x){p[0]=uint8_t(x);p[1]=uint8_t(x>>8);}
void put32(uint8_t* p,uint32_t x){put16(p,uint16_t(x));put16(p+2,uint16_t(x>>16));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    for(int profile=0;profile<5;++profile){
    const bool refresh=profile==4,pitchUpdate=profile==1,indexedTuning=profile>=2,generatedTuning=profile>=3;
    const std::string prefix=refresh?"drwave246-pitch-refresh":generatedTuning?"drwave246-tuning-table":indexedTuning?"drwave246-tuning-index":pitchUpdate?"drwave246-pitch-update":"drwave246-activation";
    const size_t eventBytes=refresh?2980:indexedTuning?240:pitchUpdate?2824:232,resultBytes=refresh?12168:indexedTuning?9272:pitchUpdate?11828:9244;
    const auto base=read(dir+"/drwave246-kernel-input.bin"),events=read(dir+"/"+prefix+"-overrides.bin"),oracle=read(dir+"/"+prefix+"-output.bin"),sineData=read(dir+"/verified-sine.bin");
    check(base.size()==512*9316&&events.size()==512*eventBytes&&oracle.size()==512+327680+512*resultBytes+(indexedTuning?9600:0)&&sineData.size()>=16384,"Incomplete activation fixtures");
    std::array<int16_t,8192> sine{};for(size_t i=0;i<8192;++i)sine[i]=s16(sineData.data()+i*2);
    for(size_t i=0;i<128;++i)check(uint32_t(voice246NotePitch[i])==u32(oracle.data()+i*4),"Original note-table copy differs");
    Voice246Registers registers;std::array<uint8_t,64> registerImages{};std::array<bool,128> lookupUsed{};
    size_t explicitNotes=0,largeIgnoredIndices=0;
    std::array<bool,129> listCounts{};size_t broadcasts=0,selective=0,ignored=0;
    std::array<int32_t,2400> tuningTable{};
    if(generatedTuning)voice246BuildTuningTable(tuningTable);
    std::array<Adsr246State,6> envelopes{};std::array<uint8_t,160> envelopeImage{};uint32_t random=0;
    std::array<bool,16> refreshModes{};size_t refreshEmpty=0,refreshDisabled=0;
    DrWave246WaveBank bank;DrWave246Waves waves;DrWave246Runtime runtime;DrWave246Context initial;
    drWave246InitializeShared(waves,initial,runtime.settings);drWave246BuildWaveBank(bank,runtime.settings,sine);
    check(std::memcmp(&bank,oracle.data()+512,sizeof(bank))==0&&std::memcmp(&bank,oracle.data()+oracle.size()-sizeof(bank),sizeof(bank))==0,"Routing base banks differ/changed");
    DrWave246Pool pool;drWave246CoreConstruct(pool);
    size_t factories=0,accepted=0,rejected=0,updated=0,readOnly=0,inactive=0;
    std::array<bool,6> voices{},updateVoices{},resetVoices{};std::array<bool,8> soundsUsed{};
    std::array<bool,6> spectra{};std::array<bool,3> transforms{};
    for(size_t test=0;test<512;++test){
        const auto* p=base.data()+test*9316;const auto* a=events.data()+test*eventBytes;const auto* e=oracle.data()+512+163840+test*resultBytes;
        const auto action=u32(a),slot=u32(a+4),counter=u32(a+8),loaded=u32(a+12),selected=u32(a+16),enabled=u32(p+12);
        check(slot<6&&loaded<8&&selected<8,"Invalid activation fixture index");voices[slot]=true;soundsUsed[loaded]=true;
        if(action&1){
            std::memcpy(pool.data(),p+536,216);for(auto& state:pool)state.tag=0x0190fb2c;
            std::memcpy(waves.data(),p+752,6144);std::memcpy(&runtime.settings,p+6896,16);std::memcpy(&runtime.transform,p+6912,2072);
            for(size_t i=0;i<5;++i)runtime.increments[i]=u32(p+8984+i*4);
            std::memcpy(runtime.configuration.data(),p+9004,32);runtime.wavePointer=u32(p+9036);runtime.transform.reciprocalInitialized=u32(p+9040);
            std::memcpy(registerImages.data(),a+168,64);
            for(size_t i=0;i<6;++i){registers.active[i]=a[168+i];registers.velocity[i]=s16(a+176+i*2);registers.pitch[i]=signedPhase(u32(a+200+i*4));}
            if(refresh){
                random=u32(a+244);std::memcpy(envelopeImage.data(),a+248,160);
                for(size_t i=0;i<6;++i){const auto* state=a+256+i*24;auto& env=envelopes[i];
                    env.stage=u32(state);env.blocks=u32(state+4);env.level=signedPhase(u32(state+8));
                    env.field12=u32(state+12);env.field16=u32(state+16);env.gain=u32(state+20);
                }
            }
        }
        runtime.settings.modeAndReady=(runtime.settings.modeAndReady&0xffff00ffu)|((enabled&255)<<8);
        DrWave246SoundControls sounds;for(size_t sound=0;sound<8;++sound)for(size_t i=0;i<8;++i)sounds[sound][i]=s16(a+20+sound*16+i*2);
        if(action&2){drWave246CoreConstruct(pool);++factories;}
        const Voice246Note note{u32(a+148),u32(a+152),u32(a+156),signedPhase(u32(a+160))};
        auto tuningWord=signedPhase(u32(a+164));
        if(indexedTuning){
            const auto index=voice246TuningIndex(signedPhase(u32(a+232)),signedPhase(u32(a+236)));
            check(index>=1&&index<2400,"Indexed activation outside explicit table");
            if(!generatedTuning)tuningTable[index]=tuningWord;
            else check(u32(a+164)==0xdeadbeef,"Generated-table fixture lost poisoned explicit override");
            tuningWord=tuningTable[index];
            check(u32(e+9244)==0x11112222&&u32(e+9248)==0x33334444&&u32(e+9252)==index&&u32(e+9256)==0x77778888,"Original tuning index/neighbors differ");
            check(u32(e+9260)==uint32_t(tuningTable[index-1])&&u32(e+9264)==uint32_t(tuningWord)&&u32(e+9268)==uint32_t(index+1<2400?tuningTable[index+1]:0),"Original indexed coefficient/adjacent words differ");
        }
        auto countNote=[&](const Voice246Note& n,size_t voice){
            if(loaded==selected){++accepted;resetVoices[voice]=true;
                if(n.explicitPitch){++explicitNotes;if(n.noteIndex>=128)++largeIgnoredIndices;}
                else {check(n.noteIndex<128,"Unexpected lookup index");lookupUsed[n.noteIndex]=true;}
            }else ++rejected;
        };
        if(refresh){
            const auto count=u32(a+2972);check(count<=128,"Invalid refresh count");listCounts[count]=true;
            std::array<Voice246PitchEntry,128> entries{};
            for(size_t i=0;i<128;++i){const auto* row=a+412+i*20;
                entries[i]={{u32(row),u32(row+4),u32(row+8),signedPhase(u32(row+12))},signedPhase(u32(row+16))};
            }
            const auto modeWord=s16(a+240);check(modeWord!=sounds[selected][4],"Refresh fixture lost independent common/Core modes");
            const auto mode=int(floorDivide(modeWord,4096));refreshModes[size_t(mode+8)]=true;
            if(count==0)++refreshEmpty;
            if(mode==4)++refreshDisabled;
            if(count&&mode!=4){
                if(mode==0){for(size_t i=0;i<count;++i)if(entries[i].voice>=0)countNote(entries[i].note,size_t(entries[i].voice));}
                else for(int voice=0;voice<(mode==3?6:1);++voice)countNote(entries[count-1].note,size_t(voice));
            }
            const auto index=voice246TuningIndex(signedPhase(u32(a+232)),signedPhase(u32(a+236)));
            check(drWave246RefreshPitch(pool,registers,envelopes,random,entries.data(),count,modeWord,tuningTable,index,int(loaded),int(selected)),"Valid refresh rejected");
            for(size_t i=0;i<6;++i){auto* state=envelopeImage.data()+8+i*24;const auto& env=envelopes[i];
                put32(state,env.stage);put32(state+4,env.blocks);put32(state+8,uint32_t(env.level));
                put32(state+12,env.field12);put32(state+16,env.field16);put32(state+20,env.gain);
            }
            for(size_t i=0;i<160;++i)if(envelopeImage[i]!=e[9272+i]){
                std::cerr<<"Refresh case "<<test<<", mode "<<mode<<", count "<<count<<", ADSR byte "<<i<<", got "<<unsigned(envelopeImage[i])<<", expected "<<unsigned(e[9272+i])<<'\n';
                throw std::runtime_error("Original refresh ADSR states/guards differ");
            }
            check(random==u32(e+9432)&&u32(e+9436)==0x31415926,"Original refresh RNG/neighbor differs");
            check(std::memcmp(a+408,e+9440,2572)==0,"Original refresh changed list/count/guards");
            for(size_t i=0;i<6;++i)check(u32(e+12012+i*4)==0,"Auxiliary Core module became present");
            check(u32(e+12036)==1,"Original refresh changed ADSR initialization flag");
            std::array<uint8_t,128> commonParameters{};std::memcpy(commonParameters.data(),a+20,128);
            put16(commonParameters.data()+selected*16+8,uint16_t(modeWord));
            check(std::memcmp(commonParameters.data(),e+12040,128)==0,"Original refresh changed common sound parameters");
        }else{
            const bool activated=drWave246ActivateNote(pool,registers,note,tuningWord,int(loaded),int(selected),int(slot));
            check(activated==(loaded==selected),"VoiceCtl acceptance differs");countNote(note,size_t(slot));
        }
        if(pitchUpdate){
            const auto* u=a+232;const auto count=u32(u+2584);check(count<=128,"Invalid update list count");listCounts[count]=true;
            std::array<Voice246PitchEntry,128> entries{};
            for(size_t i=0;i<128;++i){
                const auto* row=u+24+i*20;
                entries[i]={{u32(row),u32(row+4),u32(row+8),signedPhase(u32(row+12))},signedPhase(u32(row+16))};
                if(i<count){if(entries[i].voice==-2)++broadcasts;else if(entries[i].voice>=0&&entries[i].note.noteIndex==u32(u))++selective;else ++ignored;}
            }
            auto pitchWord=signedPhase(u32(u+12));const auto previousActive=registers.active;const auto previousVelocity=registers.velocity;
            check(voice246UpdatePitch(registers,u32(u),pitchWord,signedPhase(u32(u+4)),entries.data(),count),"Valid pitch update rejected");
            check(uint32_t(pitchWord)==u32(e+9248),"Original tuned update scalar differs");
            check(u32(u+8)==u32(e+9244)&&u32(u+16)==u32(e+9252),"Pitch scalar guards changed");
            check(std::memcmp(u+20,e+9256,2572)==0,"Original pitch update changed list/count/guards");
            check(registers.active==previousActive&&registers.velocity==previousVelocity,"Pitch update activated voice or changed velocity");
        }
        check(u32(e+8784)==0xffffffff,"Harness reserved return field changed");
        for(size_t i=0;i<6;++i){registerImages[i]=registers.active[i];put16(registerImages.data()+8+i*2,uint16_t(registers.velocity[i]));put32(registerImages.data()+32+i*4,uint32_t(registers.pitch[i]));}
        check(std::memcmp(registerImages.data(),e+9164,64)==0,"VoiceCtl registers/neighbor bytes differ");
        check(std::memcmp(a+148,e+9228,16)==0,"Original activation changed source note");
        check(std::memcmp(pool.data(),e+8948,216)==0,"Pre-render Core voice pool differs");
        std::array<int16_t,4> controls{};for(size_t i=0;i<4;++i)controls[i]=sounds[loaded][i];
        std::array<uint32_t,128> pitch{};pitch.fill(uint32_t(registers.pitch[slot]));
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
            std::cerr<<"Activation case "<<test<<", sample "<<i<<", got "<<out[i]<<", expected "<<s16(e+8+i*2)<<'\n';throw std::runtime_error("Routed Dr. Wave sample differs");
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
    if(indexedTuning)check(std::memcmp(tuningTable.data(),oracle.data()+512+163840+512*resultBytes,9600)==0,"Original explicit tuning table differs");
    check(accepted&&rejected&&explicitNotes&&largeIgnoredIndices&&factories,"Missing activation route");
    for(bool used:lookupUsed)check(used,"Missing accepted table note");
    if(refresh){
        for(bool used:refreshModes)check(used,"Missing signed mode nibble");
        for(bool used:listCounts)check(used,"Missing refresh list count");
        check(refreshEmpty&&refreshDisabled,"Missing refresh early return");
        const auto savedPool=pool;const auto savedRegisters=registers;const auto savedEnvelopes=envelopes;const auto savedRandom=random;
        Voice246PitchEntry invalid;invalid.voice=0;
        check(!drWave246RefreshPitch(pool,registers,envelopes,random,nullptr,1,0,tuningTable,0,0,0),"Null refresh list accepted");
        check(!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,129,0,tuningTable,0,0,0),"Oversized refresh list accepted");
        check(!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,0,tuningTable,2400,0,0),"Invalid refresh tuning index accepted");
        for(int sound:{-1,8})check(!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,0,tuningTable,0,sound,0)&&!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,0,tuningTable,0,0,sound),"Invalid refresh sound accepted");
        invalid.voice=6;check(!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,0,tuningTable,0,0,0),"Invalid poly refresh voice accepted");
        invalid.voice=0;invalid.note.noteIndex=128;
        check(!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,0,tuningTable,0,0,0)&&!drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,4096,tuningTable,0,0,0),"Invalid refresh lookup accepted");
        // Original early-return routes do not inspect a note or tuning entry.
        check(drWave246RefreshPitch(pool,registers,envelopes,random,nullptr,0,0,tuningTable,2400,0,0)&&drWave246RefreshPitch(pool,registers,envelopes,random,&invalid,1,16384,tuningTable,2400,0,0),"Refresh early-return precedence changed");
        check(std::memcmp(pool.data(),savedPool.data(),sizeof(pool))==0&&registers.active==savedRegisters.active&&registers.velocity==savedRegisters.velocity&&registers.pitch==savedRegisters.pitch&&random==savedRandom,"Rejected/no-op refresh mutated pool/registers/RNG");
        for(size_t i=0;i<6;++i){const auto& a=envelopes[i];const auto& b=savedEnvelopes[i];
            check(a.stage==b.stage&&a.blocks==b.blocks&&a.level==b.level&&a.field12==b.field12&&a.field16==b.field16&&a.gain==b.gain&&a.frame==b.frame&&a.retriggerStep==b.retriggerStep&&a.finished==b.finished,"Rejected/no-op refresh mutated ADSR");
        }
    }
    const auto old=pool;const auto previous=registers;
    for(int voice:{-1,6})check(!drWave246ActivateNote(pool,registers,{},0x40000000,0,0,voice),"Invalid voice accepted");
    for(int sound:{-1,8})check(!drWave246ActivateNote(pool,registers,{},0x40000000,sound,0,0)&&!drWave246ActivateNote(pool,registers,{},0x40000000,0,sound,0),"Invalid sound accepted");
    for(uint32_t index:{128u,0xffffffffu})check(!drWave246ActivateNote(pool,registers,{index,0,0,0},0x40000000,0,0,0),"Invalid lookup accepted");
    check(std::memcmp(pool.data(),old.data(),sizeof(pool))==0&&registers.active==previous.active&&registers.velocity==previous.velocity&&registers.pitch==previous.pitch,"Invalid activation mutated pool/registers");
    if(pitchUpdate){
        for(bool used:listCounts)check(used,"Missing pitch update list length");
        check(broadcasts&&selective&&ignored,"Missing pitch update selection branch");
        int32_t value=123;Voice246PitchEntry invalid;invalid.voice=6;
        check(!voice246UpdatePitch(registers,0,value,0,nullptr,1)&&!voice246UpdatePitch(registers,0,value,0,&invalid,129),"Invalid update list accepted");
        for(int index:{-3,6}){invalid.voice=index;check(!voice246UpdatePitch(registers,0,value,0,&invalid,1),"Invalid update voice accepted");}
        check(value==123&&registers.pitch==previous.pitch&&registers.active==previous.active&&registers.velocity==previous.velocity,"Invalid update mutated input/registers");
    }
    std::cout<<"PASS: "<<prefix<<", 128 note-table words, "<<(updated+readOnly)*128<<" VoiceCtl/Dr. Wave samples; "<<accepted<<" accepted / "<<rejected<<" gated, "<<explicitNotes<<" explicit pitches ("<<largeIgnoredIndices<<" ignored large indices); "<<updated<<" updates / "<<readOnly<<" reads / "<<inactive<<" inactive\n";
    }
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
