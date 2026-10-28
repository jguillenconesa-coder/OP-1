// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
#include "Voice246NoteTable.h"
namespace pocket::reference {
struct Voice246Note {
    uint32_t noteIndex=0,velocity=0,metadata=0;
    int32_t explicitPitch=0;
};
static_assert(sizeof(Voice246Note)==16);
struct Voice246Registers {
    std::array<uint8_t,6> active{};
    std::array<int16_t,6> velocity{};
    std::array<int32_t,6> pitch{};
};
// Register writes of VoiceCtl 0x01a84414, AFTER an accepted Core note-on.
// The explicit tuning word is not a recovered default calibration.
inline bool voice246PrepareRegisters(Voice246Registers& registers,const Voice246Note& note,
                                     int32_t tuningWord,int voice) noexcept {
    if(voice<0||voice>=6||(note.explicitPitch==0&&note.noteIndex>=voice246NotePitch.size()))return false;
    const auto source=note.explicitPitch!=0?note.explicitPitch:voice246NotePitch[note.noteIndex];
    registers.pitch[size_t(voice)]=saturate32(int64_t(multiplyQ31(source,tuningWord))*2);
    const auto word=note.velocity&65535u;
    const auto velocity=word<32768?int32_t(word):int32_t(word)-65536;
    registers.velocity[size_t(voice)]=saturate16(velocity*256);
    registers.active[size_t(voice)]=1;
    return true;
}
}
