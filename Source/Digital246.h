// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
namespace pocket::reference {
// Independent functional reconstruction of the isolated rev246 Digital voice.
// The caller supplies the verified sine and 128 phase increments at 88.2 kHz.
struct Digital246State {
    uint32_t tag=0,phase=0x80000000u;int16_t feedback=0,pad10=0;
    uint32_t secondary=0x80000000u,field16=0,maskPhase=0x80000000u,field24=0;
    int16_t previousSine=0,previousXor=0;
    void reset() noexcept {phase=secondary=0x80000000u;feedback=previousSine=previousXor=0;field16&=0xffff0000u;}
    void construct() noexcept {reset();tag=0x01911a34u;maskPhase=0x80000000u;field24&=0xffff0000u;}
};
inline int16_t wrap16(int32_t x) noexcept {const auto u=uint32_t(x)&65535u;return int16_t(u<32768?int(u):int(u)-65536);}
inline int32_t shiftSaturated(int32_t value,int shift) noexcept {
    if(shift>=0)return saturate32(int64_t(value)*(int64_t(1)<<shift));
    return int32_t(floorDivide(value,int64_t(1)<<(-shift)));
}
struct Digital246Coefficients {
    int shift;int32_t fine;int16_t blue,damp,maskGain;bool sync;
    explicit Digital246Coefficients(const std::array<int16_t,4>& c) noexcept
        :shift(std::clamp(int(floorDivide(c[1],4096))-3,-32,31)),fine(int32_t(floorDivide(c[2],16))*65536),
         blue(c[0]),damp(saturate16(32767-int(c[1]))),maskGain(multiplyQ15(c[3],c[3])),sync(c[2]>0){}
};
inline int16_t digital246Sample(Digital246State& s,const Digital246Coefficients& c,uint32_t increment,
                                const std::array<int16_t,8192>& sine) noexcept {
        const auto before=signedPhase(s.phase);
        const auto offset=wrap16(int(multiplyQ15(c.blue,s.feedback))+int(int16_t(floorDivide(before,65536))));
        const auto first=sine[size_t(int(floorDivide(offset,8))+4096)];
        s.phase+=increment;s.feedback=first;
        const auto second=sine[phaseIndex(s.secondary)];
        const auto intermediate=saturate16(int(floorDivide(first,2))+int(multiplyQ15(saturate16(int(s.previousSine)+second),c.damp)));
        s.previousSine=second;
        const auto delta=signedPhase(increment);
        const auto secondaryIncrement=uint32_t(saturate32(int64_t(shiftSaturated(delta,c.shift))+multiplyQ31(delta,c.fine)));
        s.secondary+=secondaryIncrement;
        if(signedPhase(s.phase)<before&&c.sync)s.secondary=s.phase;
        const auto mask=std::abs(int(multiplyQ15(sine[phaseIndex(s.maskPhase)],c.maskGain)));
        s.maskPhase+=secondaryIncrement;
        const auto word=int32_t(intermediate)^mask;
        const auto current=wrap16(int32_t(floorDivide(word,2)));
        const auto output=int16_t(floorDivide(saturate16(int(s.previousXor)+current),2));
        s.previousXor=current;
        return output;
}
inline void digital246Block(Digital246State& s,const std::array<int16_t,4>& controls,
                            const std::array<uint32_t,128>& increments,
                            const std::array<int16_t,8192>& sine,std::array<int16_t,128>& out) noexcept {
    const Digital246Coefficients c(controls);
    for(size_t i=0;i<128;++i)out[i]=digital246Sample(s,c,increments[i],sine);
}
}
