// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace pocket::reference {
// Functional reconstructions. Pd is used by the optional experimental Phase 246 mode.
// Oracle: isolated OP-1 rev246 instruction sequences on GNU Blackfin simulator.
struct FmFields {
    std::array<int,3> tableIndices{};
    int selector=0;
    bool hasExplicitJumpCase() const {return selector>=0&&selector<9;}
};
inline FmFields decodeFmFields(uint16_t packed,uint16_t selectorWord) noexcept {
    const int signedWord=(selectorWord&0x8000)?int(selectorWord)-65536:int(selectorWord);
    const int selector=signedWord>=0?signedWord/2048:-((-signedWord+2047)/2048);
    return {{{int((packed>>12)&7),int((packed>>9)&7),int((packed>>6)&7)}},selector};
}
// Signed fractional multiply with nearest/even rounding and signed saturation.
// Simulator default rounding only; other ASTAT rounding modes are not covered.
inline int16_t multiplyQ15(int16_t a,int16_t b) noexcept {
    const int32_t product=int32_t(a)*int32_t(b);
    int32_t quotient=product/32768,remainder=product%32768;
    if(remainder<0){--quotient;remainder+=32768;}
    if(remainder>16384||(remainder==16384&&quotient%2!=0))++quotient;
    return int16_t(std::clamp(quotient,int32_t(-32768),int32_t(32767)));
}
inline uint16_t phaseIndex(uint32_t phase) noexcept {return uint16_t((phase>>19)^4096u);}
// Pd render subcase: secondary state inactive, positive increment, no cycle wrap.
// Table supplied by caller. Oracle uses a synthetic sine, not the firmware LUT.
// This does not implement the original Pd sync/transient/distortion branches.
inline int16_t pdSteadySample(uint32_t& phase,uint32_t increment,int16_t gain,
                             const std::array<int16_t,8192>& table) noexcept {
    const int value=multiplyQ15(table[phaseIndex(phase)],gain);
    phase+=increment;
    return int16_t(value>=0?value/2:-((-value+1)/2));
}

inline int64_t floorDivide(int64_t value,int64_t divisor) noexcept {
    const auto quotient=value/divisor;
    return quotient-(value%divisor<0?1:0);
}
inline int32_t signedPhase(uint32_t value) noexcept {
    return int32_t(value<=0x7fffffffu?int64_t(value):int64_t(value)-4294967296ll);
}
inline int32_t saturate32(int64_t value) noexcept {
    return int32_t(std::clamp(value,int64_t(-2147483647)-1,int64_t(2147483647)));
}
inline int16_t saturate16(int32_t value) noexcept {
    return int16_t(std::clamp(value,-32768,32767));
}
inline int32_t multiplyQ31(int32_t a,int32_t b) noexcept {
    return saturate32(floorDivide(int64_t(a)*b,2147483648ll));
}
struct PdState {
    uint32_t phase=0x80000000u,secondaryPhase=0x80000000u,counter=0;
    bool secondaryActive=true;
    void reset() noexcept {*this=PdState{};}
};
struct PdCoefficients {
    int32_t distortion=0,secondaryRate=0,cycleLimit=0;
    int16_t primaryGain=0,contourGain=0;
    explicit PdCoefficients(const std::array<int16_t,4>& controls) noexcept {
        distortion=int32_t(multiplyQ15(controls[0],controls[0]))*65536;
        primaryGain=controls[1];contourGain=controls[2];
        secondaryRate=saturate32(int64_t(controls[3])*controls[3]*2);
        cycleLimit=int32_t(floorDivide(controls[3],1024));
    }
};
// Reconstructs the audio/state branch of the isolated Pd kernel. Inputs are raw
// signed controls and caller-supplied phase increments, not mapped OP-1 presets.
// Lookup is explicitly supplied; later probes verify the source sine separately.
inline int16_t pdSample(PdState& state,uint32_t increment,const PdCoefficients& c,
                        const std::array<int16_t,8192>& table) noexcept {
    const auto primary=multiplyQ15(table[phaseIndex(state.phase)],c.primaryGain);
    const auto before=signedPhase(state.phase);
    state.phase+=increment;
    const auto phase=signedPhase(state.phase);
    int16_t output=int16_t(floorDivide(primary,2));
    if(phase<before){
        state.secondaryPhase=c.cycleLimit!=0?0x80000000u:state.phase;
        state.secondaryActive=true;state.counter=0;
    }
    if(!state.secondaryActive)return output;
    const auto ramp=saturate16(int32_t(floorDivide(phase,131072))+16384);
    const auto gain=saturate16(32767-int32_t(multiplyQ15(ramp,c.contourGain)));
    auto secondary=multiplyQ15(table[phaseIndex(state.secondaryPhase)],gain);
    secondary=multiplyQ15(secondary,gain);secondary=multiplyQ15(secondary,gain);
    output=multiplyQ15(secondary,saturate16(int32_t(primary)+16384));
    const auto delta=signedPhase(increment);
    auto step=saturate32(int64_t(delta)+multiplyQ31(saturate32(int64_t(delta)*4),c.distortion));
    const auto phaseRamp=saturate32(floorDivide(phase,2)+1073741824);
    const auto curved=multiplyQ31(saturate32(int64_t(delta)*256),phaseRamp);
    step=saturate32(int64_t(step)+multiplyQ31(curved,c.secondaryRate));
    const auto secondaryBefore=signedPhase(state.secondaryPhase);
    state.secondaryPhase+=uint32_t(step);
    if(signedPhase(state.secondaryPhase)<secondaryBefore){
        ++state.counter;
        if(c.cycleLimit<signedPhase(state.counter))state.secondaryActive=false;
    }
    return output;
}
}
