// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <limits>

namespace pocket::reference {
// Independent reconstructions of rev246's IEEE-binary32 bit conversions.
// Preserve the original saturation for signed infinities/NaNs, flush tiny
// values to zero, and truncate magnitudes before applying their sign.
inline int16_t fractional246Q15FromBits(uint32_t bits) noexcept {
    const auto exponent=(bits>>23)&255u;
    const bool negative=(bits>>31)!=0;
    if(exponent>=127)return negative?int16_t(-32768):int16_t(32767);
    if(exponent<112)return 0;
    const auto significand=(bits&0x7fffffu)|0x800000u;
    const auto magnitude=significand>>(135-exponent);
    return int16_t(negative?-int32_t(magnitude):int32_t(magnitude));
}
inline int32_t fractional246Q31FromBits(uint32_t bits) noexcept {
    const auto exponent=(bits>>23)&255u;
    const bool negative=(bits>>31)!=0;
    if(exponent>=127)return negative?std::numeric_limits<int32_t>::min():std::numeric_limits<int32_t>::max();
    if(exponent<96)return 0;
    const auto significand=(bits&0x7fffffu)|0x800000u;
    const auto magnitude=exponent>=119?significand<<(exponent-119):significand>>(119-exponent);
    return negative?-int32_t(magnitude):int32_t(magnitude);
}
}
