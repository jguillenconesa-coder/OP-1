// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace pocket {
struct Punch246Preset {const char* name;int engine,preset;std::array<int16_t,4> controls;};
// Additional variants; existing Phase/Digital patches keep Punch disabled.
inline constexpr std::array<Punch246Preset,3> punch246Presets{{
    {"didg + Punch",0,3,{{6000,28315,20000,28000}}},
    {"bass phase + Punch",1,0,{{9186,0,25088,32767}}},
    {"roar bass + Punch",1,10,{{7233,28175,16992,32767}}},
}};
inline constexpr std::array<const char*,4> punch246Ids{{"punch246Blue","punch246Green","punch246White","punch246Orange"}};
inline constexpr std::array<int,4> punch246Min{{1344,0,1536,0}},punch246Max{{12480,32767,25088,32767}};
}
