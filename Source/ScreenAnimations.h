// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
namespace pocket {
// Immutable, bounded UI snapshot. No audio buffers or processor locks are accessed by drawing.
struct ScreenFrame {
    bool phase246=false,phaseEnvelope=false;
    int kitPads=8;
    int page=0,engine=0,note=60,voices=0,stage=0,step=-1,mode=0,track=0;
    float time=0,level=0,envelope=0,samplePosition=0,lfoPhase=0,tapePosition=0,tapeLength=8;
    float tone=.5f,color=.5f,attack=.02f,decay=.2f,sustain=.8f,release=.3f,lfoRate=1;
    float feedback=.42f,delay=.281f;int lfoShape=0;
    std::array<float,4> values{},trackGains{};
    std::array<juce::String,4> labels{};
    std::array<float,128> sample{};
};
void drawScreen(juce::Graphics&,const ScreenFrame&);
}
