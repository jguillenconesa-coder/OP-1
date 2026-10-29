// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ControlEnvelope246.h"
#include "Envelope246.h"
namespace pocket::reference {
// Bounded ENV callback with current module type 2 (Dr. Wave) and initialized
// six-voice ADSR pool. Other module types/lazy initialization are not emulated.
inline ControlEnvelope246Result controlEnvelope246DrWaveStep(
        std::array<int16_t,8>& parameters,std::array<Adsr246State,6>& pool,
        const std::array<int16_t,8>& controls,const std::array<uint32_t,4>& sources,
        uint32_t kind) noexcept {
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        const auto colour=floorDivide(controls[i],4096);
        if(destination!=2&&(destination!=1||colour<0||colour>4))
            return ControlEnvelope246Result::unsupported;
    }
    if(kind!=1)return ControlEnvelope246Result::inactive;
    bool applied=false;
    for(size_t i=0;i<4;++i){
        if(floorDivide(controls[i+4],2048)!=1)continue;
        applied=true;
        const auto colour=size_t(floorDivide(controls[i],4096));
        const auto bits=uint16_t((sources[i]&65535u)-64u);
        const auto delta=bits<32768?int32_t(bits):int32_t(bits)-65536;
        const auto modulation=saturate16(delta*1024);
        const auto range=colour==4?std::array<int16_t,3>{{2048,14336,32767}}:valueVoice246EnvelopeRanges[colour];
        const auto updated=value246Adjust(parameters[colour],modulation,range);
        if(colour==4&&floorDivide(parameters[4],4096)!=floorDivide(updated,4096))
            for(auto& state:pool)state.fastRelease();
        parameters[colour]=updated;
    }
    return applied?ControlEnvelope246Result::applied:ControlEnvelope246Result::inactive;
}
}
