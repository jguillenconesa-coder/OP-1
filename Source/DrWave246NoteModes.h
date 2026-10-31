// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Activation.h"
#include "Voice246PolyRelease.h"
#include "Envelope246Transition.h"
namespace pocket::reference {
enum class DrWave246NoteModesStatus { unsupported, gated, disabled, appended };
struct DrWave246NoteModesResult {
    DrWave246NoteModesStatus status=DrWave246NoteModesStatus::unsupported;
    unsigned started=0,restarted=0,blocked=0;
};
// Original 0x01a84604: selectors 1-3 with an EMPTY list, and selector 4.
// Explicit lookup tuning, initialized ADSR, outer mode zero, null auxiliaries.
// Status/guards are adapter policy, not original return values/assert behavior.
inline DrWave246NoteModesResult drWave246NoteModes(
        DrWave246Pool& waves,Voice246Registers& registers,
        std::array<Adsr246State,6>& envelopes,Voice246ReleaseList& list,
        const Voice246Note& note,uint8_t gate,int loaded,int selected,
        int selector,int32_t tuning,uint32_t& random) noexcept {
    using S=DrWave246NoteModesStatus;
    if(note.noteIndex>=128||note.explicitPitch!=0||loaded<0||loaded>=8||selected<0||selected>=8)return {};
    if(gate)return {S::gated,0,0,0};
    if(list.count!=0||selector<1||selector>4)return {};
    if(selector==4)return {S::disabled,0,0,0};
    DrWave246NoteModesResult result{S::appended,0,0,0};
    const auto base=saturate32(int64_t(multiplyQ31(voice246NotePitch[note.noteIndex],tuning))*2);
    const auto low=note.velocity&65535u;
    const auto velocity=saturate16((low<32768?int32_t(low):int32_t(low)-65536)*256);
    const auto gain=note.noteIndex==0?32767u:32768u-note.noteIndex*128u;
    const unsigned count=selector==3?6u:1u;
    for(unsigned i=0;i<count;++i){
        auto& env=envelopes[i];const bool active=registers.active[i]!=0;
        if(!active){
            env.noteOn();env.gain=(env.gain&0xffff0000u)|gain;
            if(!drWave246CoreNoteOn(waves,loaded,selected,int(i))){++result.blocked;continue;}
        }
        random=random*1664525u+1013904223u;
        const auto high=random>>16;
        const auto signedHigh=high<32768u?int32_t(high):int32_t(high)-65536;
        const auto step=int32_t(floorDivide(signedHigh,active?512:256));
        const auto deviation=signedPhase((uint32_t(step)*i)<<16);
        const auto pitch=saturate32(int64_t(base)+multiplyQ31(base,deviation));
        registers.velocity[i]=velocity;
        if(active){
            adsr246RequestRestart(env,uint32_t(pitch),uint16_t(velocity));
            env.gain=(env.gain&0xffff0000u)|gain;++result.restarted;
        }else{
            registers.pitch[i]=pitch;registers.active[i]=1;++result.started;
        }
    }
    const auto put=[&](size_t p,uint32_t value){for(unsigned j=0;j<4;++j)list.bytes[p+j]=uint8_t(value>>(j*8));};
    put(0,note.noteIndex);put(4,note.velocity);list.bytes[8]=uint8_t(note.metadata);
    put(12,uint32_t(note.explicitPitch));put(16,0xfffffffeu);list.count=1;
    return result;
}
}
