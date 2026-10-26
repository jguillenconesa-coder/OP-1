// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace pocket {
inline constexpr std::array<std::array<int16_t,4>,16> phase246Envelopes{{
    {{3136,10746,22527,8768}}, // bass phase
    {{64,1600,0,16320}}, // bubble
    {{64,15808,19456,3648}}, // classic lfo
    {{9280,8640,12800,3648}}, // hold me
    {{1600,2624,4096,7616}}, // jangle
    {{64,3136,0,3648}}, // kitt
    {{64,16320,32767,8256}}, // lil critter
    {{64,16320,32767,2112}}, // pass band
    {{64,10746,22527,15808}}, // pure
    {{64,3136,0,3648}}, // reflection
    {{3136,5696,30720,12736}}, // roar bass
    {{64,3136,0,3648}}, // rownd bass
    {{64,16320,19456,7744}}, // schain bass
    {{64,15808,19456,3648}}, // spring pad
    {{64,10304,0,5568}}, // valentines
    {{64,3136,0,3648}}, // werkin it
}};
inline constexpr std::array<const char*,4> phase246EnvelopeIds{{"phase246Attack","phase246Decay","phase246Sustain","phase246Release"}};
}
