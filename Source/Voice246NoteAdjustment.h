// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace pocket::reference {
// Valid, non-error route of SDRAM 0x01a8633c. Slot and value are full signed32
// inputs. The original route stores exactly one word at 0x01dfe4e0 + slot*4.
// This bool and rejection before mutation are adapter policy: original error
// calls/continuations are not emulated. Observed original R0=4 is not a bool.
inline bool voice246SetNoteAdjustment(std::array<uint32_t,8>& words,
        int32_t slot, int32_t adjustment) noexcept {
    if (slot < 0 || slot >= 8 || adjustment < -4 || adjustment > 4) return false;
    words[std::size_t(slot)] = uint32_t(adjustment);
    return true;
}
}
