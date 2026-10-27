// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ValueVoice246.h"
namespace pocket {
inline constexpr std::array<const char*,4> valueVoice246Ids{{"valueVoice246Speed","valueVoice246Amount","valueVoice246Destination","valueVoice246Colour"}};
// Own per-voice host-rate clock. Inactive voices keep their phase for FREE reuse;
// the instrument's voice allocator and complete control cadence remain unverified.
struct ValueVoice246Control {
    reference::Value246State state;
    std::array<uint32_t,8> display{};
    double position=1;uint64_t ticks=0;int16_t modulation=0;
    bool active=false,pendingNote=false;
    void reset() noexcept {*this={};}
    void noteOn() noexcept {pendingNote=true;}
    std::array<int16_t,4> next(bool enabled,bool envelope,bool free,int speed,int amount,int colour,int type,
                               uint32_t tempo,double clockStep,const std::array<int16_t,4>& base,
                               const std::array<int16_t,8192>& sine) noexcept {
        if(!enabled){if(active)reset();return base;}
        active=true;colour=std::clamp(colour,0,3);
        const std::array<int16_t,8> controls{{int16_t(std::clamp(speed,0,16384)),int16_t(std::clamp(amount,-32767,32767)),
            int16_t((envelope?4096:0)|(free?2048:0)),int16_t(colour*4096),0,0,0,0}};
        if(pendingNote){state.noteOn(controls);pendingNote=false;}
        if(position>=1){
            std::array<int16_t,8> targets{};std::copy(base.begin(),base.end(),targets.begin());
            reference::valueVoice246Step(state,display,controls,tempo,envelope?1:0,type,targets,sine);
            modulation=reference::saturate16(reference::signedPhase(display[6]<<16)/65536);
            position-=1;++ticks;
        }
        position+=clockStep;
        auto result=base;
        const auto& r=reference::valueVoice246Ranges[type==1?0:1][size_t(colour)];
        const auto range=envelope?reference::valueVoice246EnvelopeRanges[size_t(colour)]:std::array<int16_t,3>{{r[0],r[1],r[2]}};
        result[size_t(colour)]=reference::value246Adjust(base[size_t(colour)],modulation,range);
        return result;
    }
    float phase() const noexcept {return float(double(state.phase^0x80000000u)/4294967296.);}
};
}
