// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Value246.h"
#include "ValueVoice246Ranges.h"
namespace pocket::reference {
enum class ControlEnvelope246Result { applied, inactive, unsupported };
// Four-input callback 0x019cce68, ENV colours 0..3 only. The initial
// parameter image and source values are explicit, with no original cadence.
inline ControlEnvelope246Result controlEnvelope246Step(std::array<int16_t,8>& parameters,
        const std::array<int16_t,8>& controls,const std::array<uint32_t,4>& sources,
        uint32_t kind) noexcept {
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        const auto colour=floorDivide(controls[i],4096);
        if(destination!=2 && (destination!=1 || colour<0 || colour>=4))
            return ControlEnvelope246Result::unsupported;
    }
    if(kind!=1)return ControlEnvelope246Result::inactive;
    bool applied=false;
    for(size_t i=0;i<4;++i){
        if(floorDivide(controls[i+4],2048)!=1)continue;
        const auto colour=size_t(floorDivide(controls[i],4096));
        // Original halfword subtraction wraps before the saturating shift.
        const auto bits=uint16_t((sources[i]&65535u)-64u);
        const auto delta=bits<32768?int32_t(bits):int32_t(bits)-65536;
        const auto modulation=saturate16(delta*1024);
        parameters[colour]=value246Adjust(parameters[colour],modulation,valueVoice246EnvelopeRanges[colour]);
        applied=true;
    }
    return applied?ControlEnvelope246Result::applied:ControlEnvelope246Result::inactive;
}
}
