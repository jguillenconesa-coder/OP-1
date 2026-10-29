// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
namespace pocket::reference {
enum class ControlPitch246Result { applied, inactive, unsupported };
// Bounded pitch-only route of original four-input callback 0x019cce68.
// Source low16 is shifted/saturated, then squared/cubed with Q15 rounding.
inline int32_t controlPitch246Coefficient(uint32_t source) noexcept {
    const auto low = source & 65535u;
    const int32_t raw = low < 32768 ? int32_t(low) : int32_t(low) - 65536;
    const auto scaled = saturate16(raw * 256);
    const auto cube = multiplyQ15(multiplyQ15(scaled, scaled), scaled);
    return signedPhase(uint32_t(uint16_t(cube)) << 16);
}
// The complete original callback also routes instrument/ENV/volume targets.
// This adapter accepts only disabled routing (destination 2) or pitch routing
// (destination 3 and target nibble 1). It modifies an ALREADY FILLED buffer;
// it is not the original VoiceCtl-register -> buffer producer or its scheduler.
inline ControlPitch246Result controlPitch246Step(std::array<uint32_t,128>& pitch,
        const std::array<int16_t,8>& controls, const std::array<uint32_t,4>& sources,
        uint32_t kind) noexcept {
    // Validate every route before writing any pitch word.
    for (size_t i = 0; i < 4; ++i) {
        const auto destination = floorDivide(controls[i+4], 2048);
        if ((destination != 2 && destination != 3) ||
            (destination == 3 && floorDivide(controls[i], 4096) != 1))
            return ControlPitch246Result::unsupported;
    }
    if (kind != 0) return ControlPitch246Result::inactive;
    bool applied = false;
    for (size_t i = 0; i < 4; ++i) {
        if (floorDivide(controls[i+4], 2048) != 3) continue;
        applied = true;
        const auto coefficient = controlPitch246Coefficient(sources[i]);
        for (auto& word : pitch)
            word = uint32_t(saturate32(int64_t(multiplyQ31(signedPhase(word), coefficient)) * 8));
    }
    return applied ? ControlPitch246Result::applied : ControlPitch246Result::inactive;
}
}
