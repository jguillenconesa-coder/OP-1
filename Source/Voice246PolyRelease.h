// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Envelope246.h"
namespace pocket::reference {
// Mode-zero branch of 0x01a85058 with an initialized Dr. Wave/ADSR pool.
// Bytes 9..11 of each 20-byte list entry are padding and survive compaction.
struct Voice246ReleaseList { std::array<uint8_t,2560> bytes{}; uint32_t count=0; };
enum class Voice246ReleaseResult { rejected, absent, removed };
inline Voice246ReleaseResult voice246PolyRelease(Voice246ReleaseList& list,
        uint32_t note,std::array<Adsr246State,6>& envelopes,int16_t sustain) noexcept {
    const auto word=[&](size_t p){return uint32_t(list.bytes[p])|(uint32_t(list.bytes[p+1])<<8)|
        (uint32_t(list.bytes[p+2])<<16)|(uint32_t(list.bytes[p+3])<<24);};
    // Adapter guards, not the original assertion/error paths.
    if(list.count>128||sustain<0)return Voice246ReleaseResult::rejected;
    for(size_t i=0;i<list.count;++i){const auto voice=signedPhase(word(i*20+16));
        if(voice>=6)return Voice246ReleaseResult::rejected;}
    size_t index=0;while(index<list.count&&word(index*20)!=note)++index;
    if(index==list.count)return Voice246ReleaseResult::absent;
    const auto voice=signedPhase(word(index*20+16));
    // Core's Dr. Wave release callback preserves its complete 36-byte object.
    if(voice>=0)envelopes[size_t(voice)].noteOff(sustain);
    for(size_t i=index;i+1<list.count;++i)
        for(size_t j=0;j<20;++j)if(j<9||j>=12)list.bytes[i*20+j]=list.bytes[(i+1)*20+j];
    --list.count; // The old final slot is not cleared.
    return Voice246ReleaseResult::removed;
}
}
