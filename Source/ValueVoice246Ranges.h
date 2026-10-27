// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace pocket::reference {
// Numeric range-initializer observations for Digital (1), Phase (7) and ADSR.
inline constexpr std::array<std::array<std::array<int16_t,4>,8>,2> valueVoice246Ranges{{
    {{{{0,32767,32767,0}},{{2048,26624,24576,0}},{{-32768,32767,16384,0}},{{0,32767,32767,0}},{{0,32767,0,0}},{{0,32767,0,0}},{{0,32767,0,0}},{{0,32767,0,0}}}},
    {{{{0,32767,32767,0}},{{0,29491,32767,0}},{{0,32767,32767,0}},{{0,32767,32767,0}},{{0,32767,32767,0}},{{0,32767,0,0}},{{0,32767,0,0}},{{0,32767,0,0}}}},
}};
inline constexpr std::array<std::array<int16_t,3>,4> valueVoice246EnvelopeRanges{{{{64,16320,16384}},{{64,16320,16384}},{{0,32767,16384}},{{64,16320,16384}}}};
}
