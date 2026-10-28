// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Cluster246Routing.h"
#include "Voice246Activation.h"

namespace pocket::reference {
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
    return voice246PrepareRegisters(registers,note,tuningWord,voice);
}
}
