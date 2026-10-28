// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Kernel.h"
namespace pocket::reference {
using DrWave246Pool=std::array<DrWave246State,6>;
using DrWave246SoundControls=std::array<std::array<int16_t,8>,8>;
static_assert(sizeof(DrWave246Pool)==216);
static_assert(sizeof(DrWave246SoundControls)==128);

// Dr. Wave specialization of Core factory 0x01967960, internal engine type 2.
// Does not reset shared waves, banks, configuration or transform caches.
inline void drWave246CoreConstruct(DrWave246Pool& pool) noexcept {
    for(size_t voice=0;voice<pool.size();++voice)pool[voice].construct(uint32_t(voice));
}
// Core note-on 0x01967854: dispatches +0x1c only for matching sound slots.
// Dr. Wave's callback ignores the supplied sound controls and resets only its
// five read phases and inversion phase. Shared waves/caches remain external.
inline bool drWave246CoreNoteOn(DrWave246Pool& pool,int loadedSound,int selectedSound,int voice) noexcept {
    if(voice<0||voice>=6||loadedSound<0||loadedSound>=8||selectedSound<0||selectedSound>=8||loadedSound!=selectedSound)
        return false;
    pool[size_t(voice)].reset();return true;
}
// Core release 0x019678f0 dispatches +0x2c regardless of selected sound.
// Dr. Wave's engine callback is a no-op; this is not common ADSR release.
// The bool is our adapter's index validation, not an original return value.
inline bool drWave246CoreRelease(DrWave246Pool& pool,int voice) noexcept {
    if(voice<0||voice>=6)return false;
    pool[size_t(voice)].release();return true;
}
}
