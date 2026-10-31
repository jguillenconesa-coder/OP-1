// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Voice246Activation.h"
#include "Voice246PolyRelease.h"
#include "Envelope246Transition.h"
namespace pocket::reference {
enum class DrWave246PolyStealStatus { unsupported, gated, duplicate, appended };
struct DrWave246PolyStealResult { DrWave246PolyStealStatus status=DrWave246PolyStealStatus::unsupported;int voice=-1; };
// 0x01a84604 busy-voice poly path -> 0x01a844d8, with initialized ADSR,
// modes zero and absent auxiliary Core modules. No pending-field consumer.
// Guards/status are adapter policy, not original asserts or void returns.
inline DrWave246PolyStealResult drWave246PolySteal(
        const Voice246Registers& registers,std::array<Adsr246State,6>& envelopes,
        Voice246ReleaseList& list,const Voice246Note& note,uint8_t gate,int32_t tuning) noexcept {
    using S=DrWave246PolyStealStatus;
    if(list.count>128||note.noteIndex>=128||note.explicitPitch!=0)return {};
    if(gate)return {S::gated,-1};
    const auto word=[&](size_t p){return uint32_t(list.bytes[p])|(uint32_t(list.bytes[p+1])<<8)|
        (uint32_t(list.bytes[p+2])<<16)|(uint32_t(list.bytes[p+3])<<24);};
    for(size_t i=0;i<list.count;++i)if(word(i*20)==note.noteIndex)return {S::duplicate,-1};
    if(list.count==128)return {};
    for(auto active:registers.active)if(!active)return {};
    int voice=-1;int32_t greatest=-1;
    for(size_t i=0;i<6;++i){const auto age=signedPhase(envelopes[i].blocks);
        if(age>greatest){greatest=age;voice=int(i);}}
    if(voice<0)return {}; // Original assert path excluded when all ages are negative.
    const auto pitch=saturate32(int64_t(multiplyQ31(voice246NotePitch[note.noteIndex],tuning))*2);
    const auto low=note.velocity&65535u;
    const auto velocity=saturate16((low<32768?int32_t(low):int32_t(low)-65536)*256);
    adsr246RequestRestart(envelopes[size_t(voice)],uint32_t(pitch),uint16_t(velocity));
    const auto put=[&](size_t p,uint32_t value){for(unsigned j=0;j<4;++j)list.bytes[p+j]=uint8_t(value>>(j*8));};
    for(size_t i=0;i<list.count;++i)if(word(i*20+16)==uint32_t(voice)){put(i*20+16,0xffffffffu);break;}
    const size_t p=size_t(list.count)*20;
    put(p,note.noteIndex);put(p+4,note.velocity);list.bytes[p+8]=uint8_t(note.metadata);
    put(p+12,uint32_t(note.explicitPitch));put(p+16,uint32_t(voice));++list.count;
    return {S::appended,voice};
}
}
