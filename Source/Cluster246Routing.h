// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Cluster246.h"

namespace pocket::reference {
using Cluster246Pool=std::array<Cluster246State,cluster246FactorySlots>;
using Cluster246SoundControls=std::array<std::array<int16_t,8>,8>;
static_assert(sizeof(Cluster246Pool)==648);
static_assert(sizeof(Cluster246SoundControls)==128);

// Cluster specialization of Core::note-on at 0x01967854. The two sound IDs
// are sound-slot indices, not engine IDs or factory-database row IDs.
// Invalid caller input is rejected before mutation by our adapter guards.
inline bool cluster246CoreNoteOn(Cluster246Pool& pool,Cluster246Context& context,
                                const Cluster246Tables& tables,
                                const Cluster246SoundControls& sounds,
                                int loadedSound,int selectedSound,int voice) noexcept {
    if(voice<0||voice>=int(pool.size())||loadedSound<0||loadedSound>=8||
       selectedSound<0||selectedSound>=8||loadedSound!=selectedSound)return false;
    const auto& sound=sounds[size_t(loadedSound)];
    const std::array<int16_t,4> controls{{sound[0],sound[1],sound[2],sound[3]}};
    return cluster246Reset(pool[size_t(voice)],context,controls,tables);
}

// Cluster specialization of Core::release at 0x019678f0. It does NOT gate on
// selectedSound==loadedSound. The firmware callback is void; this bool only
// reports whether our adapter accepted the caller's voice index.
inline bool cluster246CoreRelease(Cluster246Pool& pool,int voice) noexcept {
    if(voice<0||voice>=int(pool.size()))return false;
    cluster246Release(pool[size_t(voice)]);
    return true;
}
}
