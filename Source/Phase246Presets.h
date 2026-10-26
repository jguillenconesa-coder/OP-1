// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace pocket {
// Raw stored oscillator fields from rev246; envelope fields are separate.
// Original octave/FX/LFO/full mixing remain unvalidated. Experimental patches.
struct Phase246Preset {const char* name;std::array<int16_t,4> controls;};
inline constexpr std::array<Phase246Preset,16> phase246Presets{{
    {"bass phase",{{31733,29491,26598,19439}}},
    {"bubble",{{0,0,0,0}}},
    {"classic lfo",{{32767,29491,0,0}}},
    {"hold me",{{0,29491,0,32767}}},
    {"jangle",{{25600,29491,13311,11264}}},
    {"kitt",{{0,29491,0,0}}},
    {"lil critter",{{32767,24371,0,32767}}},
    {"pass band",{{32767,24371,0,32767}}},
    {"pure",{{0,3046,16368,8175}}},
    {"reflection",{{0,29491,0,17408}}},
    {"roar bass",{{0,8192,19456,14336}}},
    {"rownd bass",{{0,29491,0,0}}},
    {"schain bass",{{32767,29491,32767,0}}},
    {"spring pad",{{32767,29491,0,0}}},
    {"valentines",{{0,29491,13311,0}}},
    {"werkin it",{{5105,0,10214,0}}},
}};
inline constexpr std::array<const char*,4> phase246Ids{{"phase246Blue","phase246Green","phase246White","phase246Orange"}};
inline constexpr std::array<int,4> phase246Max{{32767,29491,32767,32767}};
}
