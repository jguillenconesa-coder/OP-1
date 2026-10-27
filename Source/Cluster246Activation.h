// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Cluster246Routing.h"
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
// Cluster specialization of VoiceCtl's 0x01a84414. The tuning word is an
// explicit input, not a recovered original/default tuning table entry.
// The bool is adapter acceptance; the original function has no defined result.
inline bool cluster246ActivateNote(Cluster246Pool& pool,Cluster246Context& context,
                                   const Cluster246Tables& tables,
                                   const Cluster246SoundControls& sounds,
                                   Voice246Registers& registers,const Voice246Note& note,
                                   int32_t tuningWord,int loadedSound,int selectedSound,int voice) noexcept {
    if(note.explicitPitch==0&&note.noteIndex>=voice246NotePitch.size())return false;
    if(!cluster246CoreNoteOn(pool,context,tables,sounds,loadedSound,selectedSound,voice))return false;
    const auto source=note.explicitPitch!=0?note.explicitPitch:voice246NotePitch[note.noteIndex];
    registers.pitch[size_t(voice)]=saturate32(int64_t(multiplyQ31(source,tuningWord))*2);
    const auto velocity=cluster246Wrap16(int32_t(note.velocity&65535u));
    registers.velocity[size_t(voice)]=saturate16(int32_t(velocity)*256);
    registers.active[size_t(voice)]=1;
    return true;
}
}
