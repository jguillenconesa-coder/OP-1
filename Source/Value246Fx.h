// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Value246.h"
namespace pocket {
inline constexpr std::array<const char*,4> value246Ids{{"value246Speed","value246Amount","value246Destination","value246Colour"}};
// Own bounded control-clock adapter. It runs even before a voice reaches Punch
// so FREE phase is independent of audio silence and host block boundaries.
struct Value246Fx {
    reference::Value246State state;
    std::array<uint32_t,8> display{};
    double position=1,step=1;uint64_t ticks=0;
    int16_t modulation=0;bool active=false,pendingNote=false;
    void prepare(double rate) noexcept {step=44100./(64*std::clamp(rate,8000.,192000.));reset();}
    void reset() noexcept {state={};display={};position=1;ticks=0;modulation=0;active=pendingNote=false;}
    void noteOn() noexcept {pendingNote=true;}
    std::array<int16_t,4> next(bool enabled,bool route,bool free,int speed,int amount,int colour,uint32_t tempo,
                               const std::array<int16_t,4>& base,const std::array<int16_t,8192>& sine) noexcept {
        if(!enabled){if(active)reset();return base;}
        active=true;colour=std::clamp(colour,0,3);
        std::array<int16_t,8> controls{{int16_t(std::clamp(speed,0,16384)),int16_t(std::clamp(amount,-32767,32767)),
            int16_t((route?8192:0)|(free?2048:0)),int16_t(colour*4096),0,0,0,0}};
        if(pendingNote){state.noteOn(controls);pendingNote=false;}
        if(position>=1){
            std::array<int16_t,8> targets{};std::copy(base.begin(),base.end(),targets.begin());
            reference::value246FxStep(state,display,controls,tempo,2,2,targets,sine);
            modulation=route?reference::saturate16(reference::signedPhase(display[6]<<16)/65536):0;
            position-=1;++ticks;
        }
        position+=step;
        auto result=base;if(route)result[size_t(colour)]=reference::value246Adjust(base[size_t(colour)],modulation,reference::value246FxRanges[2][size_t(colour)]);
        return result;
    }
    float phase() const noexcept {return float(double(state.phase^0x80000000u)/4294967296.);}
};
}
