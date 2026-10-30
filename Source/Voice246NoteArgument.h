// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace pocket::reference {
using Voice246NoteArgument = std::array<uint8_t,16>;

// Bounded reconstruction of the complete SDRAM constructor 0x01a85014.
// Inputs are raw words, not a MIDI message. The caller supplies the discriminator
// at 0x01de4c40, selected slot at 0xff8016d4 and eight words at 0x01dfe4e0.
// The bool and invalid-index rejection are adapter behavior, not original returns.
inline bool voice246ConstructNoteArgument(Voice246NoteArgument& argument,
        uint32_t receivedNote, uint32_t receivedVelocity, uint32_t discriminator,
        uint32_t selectedSlot, const std::array<uint32_t,8>& adjustmentWords) noexcept {
    if (discriminator != 1 && selectedSlot >= adjustmentWords.size()) return false;
    // Unsigned arithmetic deliberately preserves the original modulo-2^32 wrap.
    uint32_t note = receivedNote + 53u;
    if (discriminator != 1) note += adjustmentWords[selectedSlot] * 12u;
    const auto put = [&](size_t offset, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) argument[offset+i] = uint8_t(value >> (i*8));
    };
    put(0, note);
    put(4, receivedVelocity);
    argument[8] = 1; // Bytes 9..11 retain the caller's existing contents.
    put(12, 0);
    return true;
}
}
