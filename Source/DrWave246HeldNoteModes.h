// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Envelope246Transition.h"
#include "Voice246Activation.h"
#include "Voice246NoteArgument.h"
#include "Voice246PolyRelease.h"

namespace pocket::reference {
enum class DrWave246HeldNoteModesStatus { unsupported, gated, duplicate, appended };
struct DrWave246HeldNoteModesResult {
    DrWave246HeldNoteModesStatus status=DrWave246HeldNoteModesStatus::unsupported;
    unsigned voices=0;
};
// Bounded 0x01a84604 caller: NONEMPTY list, selectors 1..3, outer mode zero,
// six initialized ADSR states, Dr. Wave Core and absent auxiliary modules.
// No empty-list branch, renderer or consumer of the pending ADSR fields.
// Guards and result labels are adapter policy, not original void returns/asserts.
inline DrWave246HeldNoteModesResult drWave246HeldNoteModes(
        Voice246Registers& registers,std::array<Adsr246State,6>& envelopes,
        Voice246ReleaseList& list,const Voice246NoteArgument& argument,
        uint8_t gate,int16_t selectorWord,int32_t tuning,uint32_t& random) noexcept {
    using S=DrWave246HeldNoteModesStatus;
    if(list.count==0||list.count>128)return {};
    if(gate)return {S::gated,0};
    const auto word=[](const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|
        (uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    const auto note=word(argument.data());
    for(size_t i=0;i<list.count;++i)
        if(word(list.bytes.data()+i*20)==note)return {S::duplicate,0};
    const auto selector=floorDivide(selectorWord,4096);
    if(selector<1||selector>3||list.count==128||note>=128)return {};
    const auto explicitWord=word(argument.data()+12);
    const auto source=explicitWord?signedPhase(explicitWord):voice246NotePitch[note];
    const auto base=saturate32(int64_t(multiplyQ31(source,tuning))*2);
    const auto low=word(argument.data()+4)&65535u;
    const auto velocity=saturate16((low<32768?int32_t(low):int32_t(low)-65536)*256);
    const unsigned voices=selector==3?6u:1u;
    for(unsigned v=0;v<voices;++v){
        random=random*1664525u+1013904223u;
        const auto high=floorDivide(signedPhase(random),65536);
        const auto step=int32_t(floorDivide(high,512));
        const auto deviation=signedPhase((uint32_t(step)*v)<<16);
        const auto pitch=saturate32(int64_t(base)+multiplyQ31(base,deviation));
        if(selector==2)registers.pitch[v]=pitch;
        else {
            adsr246RequestRestart(envelopes[v],uint32_t(pitch),uint16_t(velocity));
            const auto gain=note==0?32767u:32768u-note*128u;
            envelopes[v].gain=(envelopes[v].gain&0xffff0000u)|gain;
            // Original auxiliary dispatch returns without callbacks here.
        }
        registers.velocity[v]=velocity;
    }
    const size_t p=size_t(list.count)*20;
    const auto put=[&](size_t offset,uint32_t value){
        for(unsigned j=0;j<4;++j)list.bytes[p+offset+j]=uint8_t(value>>(j*8));};
    put(0,note);put(4,word(argument.data()+4));list.bytes[p+8]=argument[8];
    put(12,explicitWord);put(16,0xfffffffeu);++list.count;
    return {S::appended,voices};
}
}
