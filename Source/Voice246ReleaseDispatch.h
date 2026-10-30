// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Voice246NoteArgument.h"
#include "Voice246ReleaseSelectors.h"

namespace pocket::reference {
enum class Voice246ReleaseDispatchResult { rejected, filtered, absent, removed };

// Instrument discriminator 0 route of 0x01a85840, with an explicitly EMPTY
// 128-byte notification map. The enum/guards are adapter policy, not original
// returns or asserts. Pools, mode word, sustain and tuning are explicit context.
inline Voice246ReleaseDispatchResult voice246ReleaseDispatch(
        const Voice246NoteArgument& argument, uint32_t discriminator,
        const std::array<uint8_t,128>& notificationMap,
        Voice246NoteArgument& localArgument, Voice246ReleaseList& list,
        int16_t modeWord, std::array<Adsr246State,6>& envelopes, int16_t sustain,
        int32_t tuning, std::array<int32_t,6>& pitch, uint32_t& random) noexcept {
    if (discriminator != 0) return Voice246ReleaseDispatchResult::rejected;
    const uint32_t note = uint32_t(argument[0]) | (uint32_t(argument[1])<<8) |
                          (uint32_t(argument[2])<<16) | (uint32_t(argument[3])<<24);
    // Exactly the original signed32 0..127 filter for raw 32-bit words.
    if (note >= 128) return Voice246ReleaseDispatchResult::filtered;
    for (auto byte : notificationMap)
        if (byte != 0) return Voice246ReleaseDispatchResult::rejected;
    const int mode = int(floorDivide(modeWord,4096));
    if (mode < 0 || mode > 4) return Voice246ReleaseDispatchResult::rejected;
    auto copied = localArgument;
    for (std::size_t i = 0; i < 16; ++i)
        if (i < 9 || i >= 12) copied[i] = argument[i];
    const auto result = mode == 0
        ? voice246PolyRelease(list,note,envelopes,sustain)
        : voice246ReleaseSelectors(list,note,modeWord,envelopes,sustain,tuning,pitch,random);
    if (result == Voice246ReleaseResult::rejected) return Voice246ReleaseDispatchResult::rejected;
    localArgument = copied;
    // With byte 8 nonzero the original notification helper reads a zero map
    // entry and returns before any pending-output/transport work. No output is
    // synthesized here and the source metadata byte remains unchanged.
    return result == Voice246ReleaseResult::removed
        ? Voice246ReleaseDispatchResult::removed : Voice246ReleaseDispatchResult::absent;
}
}
