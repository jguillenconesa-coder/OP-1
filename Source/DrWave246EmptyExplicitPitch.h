// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Activation.h"
#include "Envelope246Transition.h"
#include "Voice246NoteArgument.h"
#include "Voice246PolyRelease.h"
namespace pocket::reference {
enum class DrWave246EmptyExplicitPitchStatus { unsupported, gated, appended };
struct DrWave246EmptyExplicitPitchResult {
    DrWave246EmptyExplicitPitchStatus status=DrWave246EmptyExplicitPitchStatus::unsupported;
    unsigned started=0,restarted=0,blocked=0;
};
// Original complete caller 0x01a84604, EMPTY list, selectors 1..3, nonzero
// explicit pitch. Six ADSR ready, outer mode zero and auxiliary modules absent.
// No constructor, lifecycle, renderer, pending-field consumer or scheduler.
// Guards/results are adapter policy, not original asserts/semantic R0 returns.
inline DrWave246EmptyExplicitPitchResult drWave246EmptyExplicitPitch(
        DrWave246Pool& waves,Voice246Registers& registers,
        std::array<Adsr246State,6>& envelopes,Voice246ReleaseList& list,
        const Voice246NoteArgument& argument,uint8_t gate,int loaded,int selected,
        int16_t selectorWord,int32_t tuning,uint32_t& random) noexcept {
    using S=DrWave246EmptyExplicitPitchStatus;
    const auto word=[](const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|
        (uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    const auto note=word(argument.data()),explicitWord=word(argument.data()+12);
    if(list.count!=0||!explicitWord||note>=128||loaded<0||loaded>=8||selected<0||selected>=8)return {};
    if(gate)return {S::gated,0,0,0};
    const auto selector=floorDivide(selectorWord,4096);
    if(selector<1||selector>3)return {};
    // The nonzero explicit word replaces lookup BEFORE Q31 and explicit tuning.
    const auto base=saturate32(int64_t(multiplyQ31(signedPhase(explicitWord),tuning))*2);
    const auto low=word(argument.data()+4)&65535u;
    const auto velocity=saturate16((low<32768?int32_t(low):int32_t(low)-65536)*256);
    const auto gain=note==0?32767u:32768u-note*128u;
    DrWave246EmptyExplicitPitchResult result{S::appended,0,0,0};
    const unsigned voices=selector==3?6u:1u;
    for(unsigned v=0;v<voices;++v){
        auto& env=envelopes[v];const bool active=registers.active[v]!=0;
        if(!active){
            env.noteOn();env.gain=(env.gain&0xffff0000u)|gain;
            if(!drWave246CoreNoteOn(waves,loaded,selected,int(v))){++result.blocked;continue;}
        }
        random=random*1664525u+1013904223u;
        const auto high=floorDivide(signedPhase(random),65536);
        const auto step=int32_t(floorDivide(high,active?512:256));
        const auto deviation=signedPhase((uint32_t(step)*v)<<16);
        const auto pitch=saturate32(int64_t(base)+multiplyQ31(base,deviation));
        registers.velocity[v]=velocity;
        if(active){
            adsr246RequestRestart(env,uint32_t(pitch),uint16_t(velocity));
            env.gain=(env.gain&0xffff0000u)|gain;++result.restarted;
        }else{registers.pitch[v]=pitch;registers.active[v]=1;++result.started;}
    }
    const auto put=[&](size_t p,uint32_t value){for(unsigned j=0;j<4;++j)list.bytes[p+j]=uint8_t(value>>(j*8));};
    put(0,note);put(4,word(argument.data()+4));list.bytes[8]=argument[8];
    put(12,explicitWord);put(16,0xfffffffeu);list.count=1;
    return result;
}
}
