// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Activation.h"
#include "Voice246PolyRelease.h"
namespace pocket::reference {
enum class DrWave246PolyFirstFreeStatus { unsupported, gated, duplicate, appended };
struct DrWave246PolyFirstFreeResult {
    DrWave246PolyFirstFreeStatus status=DrWave246PolyFirstFreeStatus::unsupported;
    int voice=-1;bool coreAccepted=false;
};
// Bounded path through original 0x01a84604: outer/per-sound modes zero,
// initialized ADSR and absent auxiliary Core modules. No stealing/error paths.
// Status/guards are our adapter's policy, not the original void return.
inline DrWave246PolyFirstFreeResult drWave246PolyFirstFree(
        DrWave246Pool& waves,Voice246Registers& registers,
        std::array<Adsr246State,6>& envelopes,Voice246ReleaseList& list,
        const Voice246Note& note,uint8_t gate,int loaded,int selected,int32_t tuning) noexcept {
    using S=DrWave246PolyFirstFreeStatus;
    if(list.count>128||note.noteIndex>=128||note.explicitPitch!=0||loaded<0||loaded>=8||selected<0||selected>=8)return {};
    if(gate)return {S::gated,-1,false};
    for(size_t i=0;i<list.count;++i){const size_t p=i*20;
        const auto n=uint32_t(list.bytes[p])|(uint32_t(list.bytes[p+1])<<8)|(uint32_t(list.bytes[p+2])<<16)|(uint32_t(list.bytes[p+3])<<24);
        if(n==note.noteIndex)return {S::duplicate,-1,false};}
    size_t voice=0;while(voice<6&&registers.active[voice]!=0)++voice;
    if(voice==6||list.count==128)return {};
    auto& envelope=envelopes[voice];envelope.noteOn();
    const auto gain=note.noteIndex==0?32767u:32768u-note.noteIndex*128u;
    envelope.gain=(envelope.gain&0xffff0000u)|gain;
    // Original auxiliary dispatch returns without callbacks in this context.
    const bool accepted=drWave246ActivateNote(waves,registers,note,tuning,loaded,selected,int(voice));
    const size_t p=size_t(list.count)*20;
    const auto put=[&](size_t offset,uint32_t value){for(unsigned j=0;j<4;++j)list.bytes[p+offset+j]=uint8_t(value>>(j*8));};
    put(0,note.noteIndex);put(4,note.velocity);list.bytes[p+8]=uint8_t(note.metadata);
    put(12,uint32_t(note.explicitPitch));put(16,uint32_t(voice));++list.count;
    return {S::appended,int(voice),accepted};
}
}
