// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
namespace pocket::reference {
// Numeric observations of the original common range initializer, type 2.
inline constexpr std::array<std::array<int16_t,4>,8> drWave246InstrumentRanges{{
    {{0,24575,4915,1}},{{0,16383,3276,1}},{{0,16383,3276,1}},{{0,32767,6553,0}},
    {{0,32767,0,1}},{{0,32767,0,1}},{{0,32767,0,1}},{{0,32767,0,0}}
}};
enum class DrWave246ControlResult { applied,inactive,unsupported };
// Type-2 instrument routes only. Type flag is the observed zero from SDRAM;
// caller supplies valid initial first-four parameters. Not a general dispatcher.
inline DrWave246ControlResult drWave246ControlInstrumentStep(
        std::array<int16_t,8>& parameters,const std::array<int16_t,8>& controls,
        const std::array<uint32_t,4>& sources,uint32_t kind) noexcept {
    for(size_t c=0;c<4;++c)
        if(parameters[c]<0||parameters[c]>drWave246InstrumentRanges[c][1])
            return DrWave246ControlResult::unsupported;
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        if(destination!=2&&(destination!=0||controls[i]<0))
            return DrWave246ControlResult::unsupported;
    }
    if(kind!=0)return DrWave246ControlResult::inactive;
    bool applied=false;
    for(size_t i=0;i<4;++i){
        if(floorDivide(controls[i+4],2048)!=0)continue;
        applied=true;
        const auto colour=size_t(floorDivide(controls[i],4096));
        const auto low=uint16_t((sources[i]&65535u)-64u);
        const auto delta=low<32768?int32_t(low):int32_t(low)-65536;
        auto modulation=saturate16(delta*1024);
        if(colour>=4)modulation=int16_t(floorDivide(modulation,16));
        const auto target=colour%4;const auto& range=drWave246InstrumentRanges[target];
        auto combined=saturate16(int32_t(parameters[target])+multiplyQ15(modulation,range[2]));
        if(drWave246InstrumentRanges[colour][3]==1){
            if(combined<range[0])combined=saturate16(int32_t(combined)+saturate16(int32_t(range[1])-range[0]));
            else if(combined>range[1])combined=saturate16(int32_t(combined)+saturate16(int32_t(range[0])-range[1]));
        }else combined=std::clamp(combined,range[0],range[1]);
        parameters[target]=combined;
    }
    return applied?DrWave246ControlResult::applied:DrWave246ControlResult::inactive;
}
}
