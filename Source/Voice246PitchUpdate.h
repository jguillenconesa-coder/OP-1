// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Voice246Activation.h"
namespace pocket::reference {
struct Voice246PitchEntry {
    Voice246Note note;
    int32_t voice=-1;
};
static_assert(sizeof(Voice246PitchEntry)==20);
// Isolated VoiceCtl 0x01a85cbe. Tuning remains explicit. This updates registers,
// not the render's pitch buffer, and does not activate/reset/release a voice.
// The bool and fail-before-mutation guards belong to this bounded adapter.
inline bool voice246UpdatePitch(Voice246Registers& registers,uint32_t noteIndex,
        int32_t& pitch,int32_t tuningWord,const Voice246PitchEntry* entries,size_t count) noexcept {
    if(count>128||(!entries&&count))return false;
    for(size_t i=0;i<count;++i)if(entries[i].voice<-2||entries[i].voice>=6)return false;
    pitch=saturate32(int64_t(multiplyQ31(pitch,tuningWord))*2);
    for(size_t i=0;i<count;++i){
        const auto& entry=entries[i];
        if(entry.voice==-2)registers.pitch.fill(pitch);
        else if(entry.voice!=-1&&entry.note.noteIndex==noteIndex)registers.pitch[size_t(entry.voice)]=pitch;
    }
    return true;
}
}
