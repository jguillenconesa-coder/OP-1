// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Activation.h"
#include "Voice246PitchUpdate.h"
#include "Voice246TuningTable.h"
#include "Envelope246.h"
namespace pocket::reference {
// Bounded Dr. Wave specialization of VoiceCtl 0x01a854cc. Auxiliary Core
// modules dispatched by 0x019673dc are explicitly absent in this contract.
// This refreshes registers/events, not the original per-sample pitch buffer.
// Return value and fail-before-mutation guards belong to this adapter.
inline bool drWave246RefreshPitch(DrWave246Pool& pool,Voice246Registers& registers,
        std::array<Adsr246State,6>& envelopes,uint32_t& random,
        const Voice246PitchEntry* entries,std::size_t count,int16_t modeWord,
        const Voice246TuningTable& tuning,uint32_t tuningIndex,int loadedSound,int selectedSound) noexcept {
    if(count>128||(!entries&&count)||loadedSound<0||loadedSound>=8||selectedSound<0||selectedSound>=8)return false;
    const auto mode=int(floorDivide(modeWord,4096));
    if(mode==4||count==0)return true;
    if(tuningIndex>=tuning.size())return false;
    if(mode==0){
        for(std::size_t i=0;i<count;++i)if(entries[i].voice>=0&&
            (entries[i].voice>=6||entries[i].note.noteIndex>=128))return false;
        for(std::size_t i=0;i<count;++i){
            const auto& entry=entries[i];if(entry.voice<0)continue;
            auto& envelope=envelopes[std::size_t(entry.voice)];envelope.noteOn();
            // The original binary32 expression 1 - noteIndex/256 is exact
            // in this bounded note domain, followed by saturated Q15.
            const auto gain=entry.note.noteIndex==0?32767:32768-int(entry.note.noteIndex)*128;
            envelope.gain=(envelope.gain&0xffff0000u)|uint32_t(gain);
            drWave246ActivateNote(pool,registers,entry.note,tuning[tuningIndex],loadedSound,selectedSound,entry.voice);
        }
        return true;
    }
    const auto& note=entries[count-1].note;
    if(note.explicitPitch==0&&note.noteIndex>=128)return false;
    const auto source=note.explicitPitch!=0?note.explicitPitch:voice246NotePitch[note.noteIndex];
    const auto base=saturate32(int64_t(multiplyQ31(source,tuning[tuningIndex]))*2);
    const auto rawVelocity=note.velocity&65535u;
    const auto velocity=saturate16((rawVelocity<32768?int32_t(rawVelocity):int32_t(rawVelocity)-65536)*256);
    const int voices=mode==3?6:1;
    for(int voice=0;voice<voices;++voice){
        envelopes[std::size_t(voice)].noteOn();
        if(!drWave246CoreNoteOn(pool,loadedSound,selectedSound,voice))continue;
        random=random*1664525u+1013904223u;
        const auto high=random>>16;
        const auto signedHigh=high<32768?int32_t(high):int32_t(high)-65536;
        const auto step=int32_t(floorDivide(signedHigh,512));
        const auto deviation=signedPhase((uint32_t(step)*uint32_t(voice))<<16);
        registers.pitch[std::size_t(voice)]=saturate32(int64_t(base)+multiplyQ31(base,deviation));
        registers.velocity[std::size_t(voice)]=velocity;
        registers.active[std::size_t(voice)]=1;
    }
    return true;
}
}
