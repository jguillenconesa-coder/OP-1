// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
namespace pocket::reference {
// VoiceCtl 0x01a842f0: raw 32-bit index arithmetic, NOT table calibration.
// The original setter does not clamp/check the index before storing it.
// Unsigned operations preserve wrap without C++ signed-overflow UB.
inline uint32_t voice246TuningIndex(int32_t coarse,int32_t fine) noexcept {
    return (uint32_t(coarse)+12u)*100u+uint32_t(fine);
}
}
