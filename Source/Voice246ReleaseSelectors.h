// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Voice246PolyRelease.h"
#include "Voice246NoteTable.h"
namespace pocket::reference {
// Remaining bounded selectors of VoiceCtl 0x01a85058; initialized Dr. Wave
// objects have a preserving Core release callback. Tuning and schedule explicit.
inline Voice246ReleaseResult voice246ReleaseSelectors(Voice246ReleaseList& list,
        uint32_t note,int16_t modeWord,std::array<Adsr246State,6>& envelopes,
        int16_t sustain,int32_t tuning,std::array<int32_t,6>& pitch,uint32_t& random) noexcept {
    const int mode=int(floorDivide(modeWord,4096));
    const auto word=[&](size_t p){return uint32_t(list.bytes[p])|(uint32_t(list.bytes[p+1])<<8)|
        (uint32_t(list.bytes[p+2])<<16)|(uint32_t(list.bytes[p+3])<<24);};
    if(mode<1||mode>4||list.count>128||sustain<0)return Voice246ReleaseResult::rejected;
    if(mode==4)return Voice246ReleaseResult::absent;
    size_t match=0;while(match<list.count&&word(match*20)!=note)++match;
    if(match==list.count)return Voice246ReleaseResult::absent;
    const bool lastMatches=word((list.count-1)*20)==note;
    if(lastMatches&&list.count>1){const size_t p=(list.count-2)*20;
        if(word(p+12)==0&&word(p)>=128)return Voice246ReleaseResult::rejected;}
    const size_t remove=lastMatches?list.count-1:match;
    for(size_t i=remove;i+1<list.count;++i)
        for(size_t j=0;j<20;++j)if(j<9||j>=12)list.bytes[i*20+j]=list.bytes[(i+1)*20+j];
    --list.count;
    if(!lastMatches)return Voice246ReleaseResult::removed;
    const size_t voices=mode==3?6:1;
    if(!list.count){for(size_t v=0;v<voices;++v)envelopes[v].noteOff(sustain);}
    else {
        const size_t p=(list.count-1)*20;
        const auto source=word(p+12)?signedPhase(word(p+12)):voice246NotePitch[word(p)];
        const auto base=saturate32(int64_t(multiplyQ31(source,tuning))*2);
        for(size_t v=0;v<voices;++v){
            random=random*1664525u+1013904223u;
            const auto high=random>>16;
            const auto signedHigh=high<32768?int32_t(high):int32_t(high)-65536;
            const auto step=int32_t(floorDivide(signedHigh,512));
            const auto deviation=signedPhase((uint32_t(step)*uint32_t(v))<<16);
            pitch[v]=saturate32(int64_t(base)+multiplyQ31(base,deviation));
        }
    }
    return Voice246ReleaseResult::removed;
}
}
