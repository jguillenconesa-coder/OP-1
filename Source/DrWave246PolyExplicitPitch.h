// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Activation.h"
#include "Envelope246Transition.h"
#include "Voice246NoteArgument.h"
#include "Voice246PolyRelease.h"
namespace pocket::reference {
enum class DrWave246PolyExplicitPitchStatus { unsupported, gated, duplicate, freeAppended, busyAppended };
struct DrWave246PolyExplicitPitchResult {
    DrWave246PolyExplicitPitchStatus status=DrWave246PolyExplicitPitchStatus::unsupported;
    int voice=-1;
    bool coreAccepted=false;
};
// Original COMPLETE caller 0x01a84604, selector0, nonzero explicit pitch.
// Six ADSR ready, outer mode0, Dr. Wave Core and absent auxiliary modules.
// Guards/statuses are adapter policy, not original assertions or R0 returns.
// No dispatcher, render, pending-field consumer, clock or plugin integration.
inline DrWave246PolyExplicitPitchResult drWave246PolyExplicitPitch(
        DrWave246Pool& waves,Voice246Registers& registers,
        std::array<Adsr246State,6>& envelopes,Voice246ReleaseList& list,
        const Voice246NoteArgument& argument,uint8_t gate,int loaded,int selected,
        int16_t selectorWord,int32_t tuning) noexcept {
    using S=DrWave246PolyExplicitPitchStatus;
    const auto word=[](const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|
        (uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    const auto note=word(argument.data()),explicitWord=word(argument.data()+12);
    if(list.count>128||note>=128||!explicitWord||loaded<0||loaded>=8||selected<0||selected>=8)return {};
    if(gate)return {S::gated,-1,false};
    for(size_t i=0;i<list.count;++i)
        if(word(list.bytes.data()+i*20)==note)return {S::duplicate,-1,false};
    if(floorDivide(selectorWord,4096)!=0||list.count==128)return {};
    int voice=0;
    while(voice<6&&registers.active[size_t(voice)]!=0)++voice;
    const bool busy=voice==6;
    if(busy){
        voice=-1;int32_t greatest=-1;
        for(size_t i=0;i<6;++i){const auto age=signedPhase(envelopes[i].blocks);
            if(age>greatest){greatest=age;voice=int(i);}}
        if(voice<0)return {}; // Excluded original assert, not an original fallback.
    }
    bool accepted=false;
    if(!busy){
        auto& env=envelopes[size_t(voice)];env.noteOn();
        const auto gain=note==0?32767u:32768u-note*128u;
        env.gain=(env.gain&0xffff0000u)|gain;
        const Voice246Note raw{note,word(argument.data()+4),argument[8],signedPhase(explicitWord)};
        accepted=drWave246ActivateNote(waves,registers,raw,tuning,loaded,selected,voice);
    }else{
        const auto pitch=saturate32(int64_t(multiplyQ31(signedPhase(explicitWord),tuning))*2);
        const auto low=word(argument.data()+4)&65535u;
        const auto velocity=saturate16((low<32768?int32_t(low):int32_t(low)-65536)*256);
        adsr246RequestRestart(envelopes[size_t(voice)],uint32_t(pitch),uint16_t(velocity));
        // Busy helper preserves gain, all VoiceCtl registers and Dr. Wave.
        for(size_t i=0;i<list.count;++i)if(word(list.bytes.data()+i*20+16)==uint32_t(voice)){
            for(unsigned j=0;j<4;++j)list.bytes[i*20+16+j]=255;
            break;
        }
    }
    const size_t p=size_t(list.count)*20;
    const auto put=[&](size_t offset,uint32_t value){for(unsigned j=0;j<4;++j)list.bytes[p+offset+j]=uint8_t(value>>(j*8));};
    put(0,note);put(4,word(argument.data()+4));list.bytes[p+8]=argument[8];
    put(12,explicitWord);put(16,uint32_t(voice));++list.count;
    return {busy?S::busyAppended:S::freeAppended,voice,accepted};
}
}
