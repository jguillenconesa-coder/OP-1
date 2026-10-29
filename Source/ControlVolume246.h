// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ControlPitch246.h"
namespace pocket::reference {
// Bounded volume/pitch routes of 0x019cce68. Both buffers must already be
// filled by the caller. Four signed smoothing words are shared persistent state.
inline ControlPitch246Result controlVolume246Step(std::array<int16_t,128>& amplitude,
        std::array<uint32_t,128>& pitch,std::array<int16_t,4>& smoothing,
        const std::array<int16_t,8>& controls,const std::array<uint32_t,4>& sources,
        uint32_t kind) noexcept {
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        const auto target=floorDivide(controls[i],4096);
        if(destination!=2&&(destination!=3||target<0||target>1))
            return ControlPitch246Result::unsupported;
    }
    if(kind!=0)return ControlPitch246Result::inactive;
    bool applied=false;
    for(size_t i=0;i<4;++i){
        if(floorDivide(controls[i+4],2048)!=3)continue;
        applied=true;
        if(floorDivide(controls[i],4096)==1){
            const auto coefficient=controlPitch246Coefficient(sources[i]);
            for(auto& word:pitch)word=uint32_t(saturate32(int64_t(multiplyQ31(signedPhase(word),coefficient))*8));
        }else{
            const auto low=sources[i]&65535u;
            const auto raw=low<32768?int32_t(low):int32_t(low)-65536;
            const auto scaled=saturate16(raw*256);
            smoothing[i]=saturate16(int32_t(multiplyQ15(smoothing[i],28672))+multiplyQ15(scaled,4096));
            for(auto& sample:amplitude)sample=saturate16(int32_t(multiplyQ15(sample,smoothing[i]))*2);
        }
    }
    return applied?ControlPitch246Result::applied:ControlPitch246Result::inactive;
}
}
