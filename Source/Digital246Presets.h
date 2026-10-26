// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace pocket {
// Stored numeric controls/envelope fields; original full patches remain unverified.
struct Digital246Preset {const char* name;std::array<int16_t,4> controls,adsr;};
inline constexpr std::array<Digital246Preset,16> digital246Presets{{
    {"auto rptr",{{17408,24320,1535,10240}},{{64,12352,10239,3008}}},
    {"booooh",{{31744,2816,1023,4095}},{{1600,1984,12800,64}}},
    {"daylight",{{5120,9728,17944,4096}},{{11328,3648,16383,2496}}},
    {"didg",{{0,14336,-9192,30690}},{{1535,11258,29183,14784}}},
    {"forgotten",{{12288,18176,-3713,18240}},{{11840,12352,10239,1984}}},
    {"glenn",{{16359,24320,-7169,0}},{{64,8640,32767,16320}}},
    {"low end",{{17408,8840,-1000,0}},{{64,16320,18943,11200}}},
    {"lullaby time",{{0,19712,4095,0}},{{64,6080,0,5568}}},
    {"organbratto",{{8184,23432,-3073,0}},{{3648,10746,27647,14272}}},
    {"princess",{{8184,17288,-1025,0}},{{64,16320,0,7616}}},
    {"queff bass",{{0,5120,-32768,9216}},{{64,4090,0,13760}}},
    {"recognizer",{{13536,6656,-32768,0}},{{64,15808,0,10688}}},
    {"sugar",{{17408,24320,1535,10240}},{{64,12352,10239,3008}}},
    {"theremono",{{0,15104,32767,0}},{{2624,64,32767,15808}}},
    {"tomorrow",{{0,23552,-3072,19455}},{{1088,4602,10751,2496}}},
    {"violinist",{{9206,4352,32767,0}},{{9280,10746,27647,6592}}},
}};
inline constexpr std::array<const char*,4> digital246Ids{{"digital246Blue","digital246Green","digital246White","digital246Orange"}};
inline constexpr std::array<const char*,4> digital246EnvelopeIds{{"digital246Attack","digital246Decay","digital246Sustain","digital246Release"}};
inline constexpr std::array<int,4> digital246Min{{0,2048,-32768,0}},digital246Max{{32767,26624,32767,32767}};
}
