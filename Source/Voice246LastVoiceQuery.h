// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace pocket::reference {
// Non-error contract of SDRAM 0x01a863a8. Entries are 20-byte raw records;
// their signed32 voice word is at offset 16. Only values 0..5 are retained.
// nullopt is adapter rejection, not a return observed from the original error
// helper. Count validation bounds the supplied array; the firmware has no such
// capacity check here. No events, voice allocation or renderer are invoked.
inline std::optional<uint32_t> voice246LastVoiceQuery(
        const std::array<uint8_t,2560>& entries, uint32_t count,
        uint32_t& persistent) noexcept {
    if (count > 128) return std::nullopt;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* p = entries.data() + i*20 + 16;
        const uint32_t voice = uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
                               (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        // For raw words, unsigned < 6 is exactly signed32 >= 0 && < 6.
        if (voice < 6) persistent = voice;
    }
    if (persistent >= 6) return std::nullopt;
    return persistent;
}
}
