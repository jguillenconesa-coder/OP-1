// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
#include <cstddef>

namespace pocket::reference {
// Isolated rev246 Cluster kernel. Initialisation coefficients MUST be supplied
// by the caller: the original L1 coefficient image has not been validated.
// This is not a complete/natively integrated original instrument.
// Observed first-four hardware control ranges, separate from the broader safe
// isolated-kernel input domain used by the instruction probes below.
inline constexpr std::array<int16_t,4> cluster246ControlMin{{3072,0,512,3}};
inline constexpr std::array<int16_t,4> cluster246ControlMax{{17408,32767,24064,1638}};
// Scalar coefficients reproduced by the original float-to-Q31 helper. These
// do not provide the still-unvalidated count-normalization coefficients.
inline constexpr std::array<int32_t,24> cluster246Damping{{
    117867096,503108000,816113920,1201772928,
    1494041600,1606483072,1717771776,1791209088,
    1857389568,1912090240,1949449344,1976580480,
    1997176448,2026371712,2046071936,2070966912,
    2086048896,2106329344,2116543360,2126806912,
    2135053696,2141259648,2143332352,2145406976,
}};
struct Cluster246Oscillator {
    uint32_t phase=0x80000000u;
    int16_t feedback=0;
    uint16_t padding=0;
};
struct Cluster246State {
    uint32_t tag=0;
    std::array<Cluster246Oscillator,8> oscillators{};
    std::array<int16_t,8> detune{};
    int16_t field84=0,field86=0,normalization=0;
    std::array<int16_t,8> gains{};
    uint8_t released=0,padding107=0;
};
static_assert(sizeof(Cluster246State)==108);
static_assert(offsetof(Cluster246State,detune)==68);
static_assert(offsetof(Cluster246State,gains)==90);
static_assert(offsetof(Cluster246State,released)==106);
struct Cluster246Context {
    std::array<int16_t,8> weights{},display{};
    uint32_t random=0;
};
struct Cluster246Tables {
    std::array<int16_t,8> normalization{};
    std::array<int32_t,32> damping{};
};
inline int16_t cluster246Wrap16(int32_t value) noexcept {
    const auto word=uint32_t(value)&65535u;
    return int16_t(word<32768?int(word):int(word)-65536);
}
inline int16_t cluster246High(uint32_t word) noexcept {
    return cluster246Wrap16(int32_t(word>>16));
}
inline int16_t cluster246Random(Cluster246Context& context) noexcept {
    context.random=context.random*1664525u+1013904223u;
    return cluster246High(context.random);
}
inline bool cluster246ControlsValid(const std::array<int16_t,4>& controls) noexcept {
    return controls[0]>=2048&&controls[0]<=18431&&controls[1]>=0&&controls[2]>=0&&controls[3]>=0;
}
inline bool cluster246Reset(Cluster246State& state,Cluster246Context& context,
                            const std::array<int16_t,4>& controls,
                            const Cluster246Tables& tables) noexcept {
    if(!cluster246ControlsValid(controls))return false;
    for(auto& detune:state.detune)
        detune=multiplyQ15(saturate16(int(cluster246Random(context))-16384),655);
    for(auto& oscillator:state.oscillators){oscillator.phase=0x80000000u;oscillator.feedback=0;}
    state.field84=0;state.field86=32767;
    const int count=controls[0]/2048;
    for(size_t i=0;i<8;++i)state.gains[i]=int(i)<count?32767:0;
    state.normalization=tables.normalization[size_t(count-1)];
    state.released=0;
    return true;
}
inline void cluster246Release(Cluster246State& state) noexcept {
    state.field86=256;state.released=1;
}
inline void cluster246DisplayDecay(Cluster246Context& context) noexcept {
    for(auto& sample:context.display)sample=multiplyQ15(sample,32512);
}
inline bool cluster246Block(Cluster246State& state,Cluster246Context& context,
                            const std::array<int16_t,4>& controls,
                            const Cluster246Tables& tables,
                            const std::array<uint32_t,128>& increments,
                            const std::array<int16_t,8192>& sine,
                            std::array<int16_t,128>& output) noexcept {
    if(!cluster246ControlsValid(controls))return false;
    const int count=controls[0]/2048;
    state.normalization=saturate16(int(multiplyQ15(state.normalization,32604))+
                                    multiplyQ15(tables.normalization[size_t(count-1)],163));
    for(size_t i=0;i<8;++i){
        state.gains[i]=saturate16(int(multiplyQ15(state.gains[i],32604))+
                                  multiplyQ15(int(i)<count?32767:0,163));
        context.weights[i]=multiplyQ15(state.gains[i],state.normalization);
    }
    const int morph=multiplyQ15(controls[1],20480);
    int16_t low=0,high=32767;
    if(morph<4096){low=saturate16(morph*8);high=-32768;}
    else if(morph<12288){low=32767;high=saturate16((morph-8192)*8);}
    else if(morph<16384)low=saturate16(32767-int(saturate16((morph-12288)*8)));
    else high=saturate16(32767-int(saturate16((morph-16384)*8)));
    const auto cube=multiplyQ15(multiplyQ15(high,high),high);
    state.field84=saturate16(int(state.field84)+multiplyQ15(cube,419));
    const auto feedback=multiplyQ15(saturate16(int(low)+state.field84),state.released?12288:16384);
    auto randomAmount=multiplyQ15(controls[3],16384);
    if(state.released)randomAmount=multiplyQ15(randomAmount,4096);
    for(size_t oscillator=0;oscillator<8;++oscillator){
        auto& voice=state.oscillators[oscillator];
        const auto detune=int32_t(state.detune[oscillator])*65536;
        const auto offset=multiplyQ31(signedPhase(increments[oscillator]),detune);
        for(size_t sample=0;sample<128;++sample){
            const auto phase=cluster246High(voice.phase);
            const auto displaced=cluster246Wrap16(int(multiplyQ15(feedback,voice.feedback))+phase);
            const auto index=size_t(floorDivide(displaced,8)+4096);
            voice.feedback=sine[index];
            voice.phase+=uint32_t(saturate32(int64_t(signedPhase(increments[sample]))+offset));
            const auto value=multiplyQ15(voice.feedback,context.weights[oscillator]);
            output[sample]=oscillator==0?value:saturate16(int(output[sample])+value);
        }
    }
    const auto damping=cluster246High(uint32_t(tables.damping[size_t(controls[2]/1024)]));
    state.field86=multiplyQ15(state.field86,damping);
    const auto displayGain=saturate16(int(feedback)+8192);
    for(size_t i=0;i<8;++i){
        state.detune[i]=saturate16(int(multiplyQ15(state.detune[i],damping))+
                                  multiplyQ15(cluster246Random(context),randomAmount));
        const auto phase=multiplyQ15(cluster246High(state.oscillators[i].phase),displayGain);
        context.display[i]=multiplyQ15(saturate16(int(state.detune[i])+phase),state.gains[i]);
    }
    return true;
}
}
