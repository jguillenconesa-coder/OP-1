// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Routing.h"
#include "Voice246Activation.h"
namespace pocket::reference {
// Dr. Wave specialization of original VoiceCtl activation 0x01a84414.
// Core rejection preserves the pool and registers. The bool reports adapter
// acceptance, not a defined return value of the original void routine.
inline bool drWave246ActivateNote(DrWave246Pool& pool,Voice246Registers& registers,
                                  const Voice246Note& note,int32_t tuningWord,
                                  int loadedSound,int selectedSound,int voice) noexcept {
    if(note.explicitPitch==0&&note.noteIndex>=voice246NotePitch.size())return false;
    if(!drWave246CoreNoteOn(pool,loadedSound,selectedSound,voice))return false;
    return voice246PrepareRegisters(registers,note,tuningWord,voice);
}
}
