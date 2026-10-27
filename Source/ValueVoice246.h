// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Value246.h"
#include "ValueVoice246Ranges.h"
namespace pocket::reference {
// Verified scope: Digital/Phase instrument controls and common ADSR, colours
// 0..3. Other instruments, auxiliary words, drum and play-mode routes are pending.
inline bool valueVoice246Step(Value246State& state,std::array<uint32_t,8>& display,
                              const std::array<int16_t,8>& controls,uint32_t tempo,int kind,int type,
                              std::array<int16_t,8>& targets,const std::array<int16_t,8192>& sine) noexcept {
    const auto destination=uint16_t(controls[2])>>12,colour=uint16_t(controls[3])>>12;
    if(!((destination==0&&kind==0)||(destination==1&&kind==1)))return false;
    if((type!=1&&type!=7)||colour>=4||controls[0]<0||controls[0]>16384)return false;
    const auto modulation=value246Advance(state,display,controls,tempo,sine);
    const auto& descriptor=valueVoice246Ranges[type==1?0:1][colour];
    const auto& range=kind==1?valueVoice246EnvelopeRanges[colour]:std::array<int16_t,3>{{descriptor[0],descriptor[1],descriptor[2]}};
    targets[colour]=value246Adjust(targets[colour],modulation,range);
    return true;
}
}
