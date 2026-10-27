// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
#include "Value246Ranges.h"
namespace pocket::reference {
struct Value246State {
    uint32_t tag=0,phase=0x80000000u;
    uint16_t word=0,padding=0;
    void noteOn(const std::array<int16_t,8>& controls) noexcept {
        if((uint16_t(controls[2])&2048u)==0){phase=0x80000000u;word=0;}
    }
};
// Original float32 multiplication followed by truncation. The integrated tempo
// is bounded to 40..300 BPM, including the host range beyond the original 220.
inline uint32_t value246Tempo(float bpm) noexcept {return uint32_t(1662149.125f*bpm);}
inline uint32_t value246Shift(uint32_t value,unsigned bits) noexcept {return uint32_t(floorDivide(signedPhase(value),int64_t(1)<<bits));}
inline uint32_t value246Increment(int16_t speed,uint32_t tempo) noexcept {
    if(speed<8192)return value246Shift(tempo,uint16_t(speed)>>10);
    const auto x=saturate16(int(speed)-8192);
    return uint32_t(uint16_t(multiplyQ15(x,x)))<<16;
}
inline int16_t value246Adjust(int16_t base,int16_t modulation,const std::array<int16_t,3>& range) noexcept {
    const auto combined=saturate16(int(base)+multiplyQ15(modulation,range[2]));
    return std::clamp(combined,range[0],range[1]);
}
inline int16_t value246Advance(Value246State& state,std::array<uint32_t,8>& display,
                               const std::array<int16_t,8>& controls,uint32_t tempo,
                               const std::array<int16_t,8192>& sine) noexcept {
    const auto increment=value246Increment(controls[0],tempo);
    const auto modulation=multiplyQ15(sine[phaseIndex(state.phase)],controls[1]);
    state.phase+=increment;
    std::array<int16_t,4> values{{modulation,0,0,0}};
    for(size_t i=0;i<3;++i){values[i+1]=multiplyQ15(sine[phaseIndex(display[i*2])],controls[1]);display[i*2]+=value246Shift(increment,unsigned(i+1));}
    display[6]=uint16_t(values[0])|(uint32_t(uint16_t(values[1]))<<16);
    display[7]=uint16_t(values[2])|(uint32_t(uint16_t(values[3]))<<16);
    return modulation;
}
// Isolated FX dispatch only. Caller supplies an original-range speed, one of
// the eight FX IDs and a nonnegative parameter selector (0..7). Invalid dispatch
// leaves every state/display/target word untouched. No voice/ENV reconstruction.
inline bool value246FxStep(Value246State& state,std::array<uint32_t,8>& display,
                          const std::array<int16_t,8>& controls,uint32_t tempo,int kind,int fx,
                          std::array<int16_t,8>& targets,const std::array<int16_t,8192>& sine) noexcept {
    if((uint16_t(controls[2])>>12)!=2||(kind!=2&&kind!=5))return false;
    const auto colour=size_t(uint16_t(controls[3])>>12);
    if(fx<0||fx>=8||colour>=8||controls[0]<0||controls[0]>16384)return false;
    const auto modulation=value246Advance(state,display,controls,tempo,sine);
    targets[colour]=value246Adjust(targets[colour],modulation,value246FxRanges[size_t(fx)][colour]);
    return true;
}
}
