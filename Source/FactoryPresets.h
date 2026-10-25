// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
namespace pocket {
struct FactoryPreset {
    const char* name;
    int engine;
    float tone,color,envelope,effect,attack,release;
    int sample=-1,kit=-1;
    float decay=.25f,sustain=.75f,lfoRate=1,lfoDepth=0,drive=0;
};
// Original recipes; descriptive names do not claim to reproduce factory firmware.
inline constexpr std::array<FactoryPreset,25> factoryPresets{{
 {"FM / Soft keys",0,.18f,.15f,.2f,.15f,.01f,.18f},
 {"FM / Glass bells",0,.7f,.62f,.6f,.35f,.08f,.5f},
 {"PHASE / Slow pad",1,.58f,.3f,.4f,.28f,.24f,.4f},
 {"STRING / Plucked wire",2,.7f,.2f,.25f,.25f,.01f,.3f},
 {"DRUM / Synth machine",4,.28f,.2f,.3f,.05f,0,.1f},
 {"DRUM / Bright hats",4,.7f,.8f,.1f,.12f,0,.1f},
 {"FM / Digital tine",0,.32f,.28f,.3f,.2f,.005f,.25f},
 {"FM / Hollow bass",0,.16f,.02f,.15f,.03f,.002f,.08f},
 {"FM / Metallic lead",0,.76f,.4f,.2f,.18f,.005f,.16f},
 {"FM / Orbit chimes",0,.88f,.73f,.5f,.42f,.015f,.55f},
 {"PHASE / Round bass",1,.22f,.18f,.2f,.04f,.003f,.1f},
 {"PHASE / Reed keys",1,.7f,.45f,.3f,.12f,.025f,.22f},
 {"PHASE / Warm organ",1,.48f,.78f,.3f,.12f,.015f,.16f},
 {"PHASE / Glass sweep",1,.83f,.52f,.5f,.38f,.3f,.6f},
 {"STRING / Nylon cloud",2,.4f,.12f,.35f,.32f,.002f,.42f},
 {"STRING / Muted pluck",2,.22f,.08f,.15f,.04f,.001f,.08f},
 {"STRING / Steel shimmer",2,.91f,.04f,.4f,.38f,.001f,.5f},
 {"STRING / Bowed hybrid",2,.68f,.8f,.5f,.32f,.18f,.5f},
 {"SAMPLE / Velvet keys",3,0,1,.3f,.14f,.005f,.3f,0},
 {"SAMPLE / Glass mallet",3,0,1,.3f,.28f,.001f,.42f,1},
 {"SAMPLE / Air choir",3,0,1,.4f,.35f,.12f,.5f,2},
 {"SAMPLE / Low pluck",3,0,1,.2f,.08f,.002f,.12f,3},
 {"KIT / Studio",5,.5f,1,.2f,.05f,.001f,.1f,-1,0},
 {"KIT / Circuit",5,.5f,1,.2f,.08f,.001f,.1f,-1,1},
 {"KIT / Dust",5,.5f,1,.2f,.03f,.001f,.1f,-1,2}
}};
}
