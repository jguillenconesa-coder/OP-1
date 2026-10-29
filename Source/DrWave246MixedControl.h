// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246ControlInstrument.h"
#include "ControlVolume246.h"
namespace pocket::reference {
// Bounded type-2 callback: instrument, volume, pitch and disabled channels.
// ENV and other module types are excluded. Both incoming buffers are explicit.
inline DrWave246ControlResult drWave246MixedControlStep(
        std::array<int16_t,8>& parameters,std::array<int16_t,128>& amplitude,
        std::array<uint32_t,128>& pitch,std::array<int16_t,4>& smoothing,
        const std::array<int16_t,8>& controls,const std::array<uint32_t,4>& sources,
        uint32_t kind) noexcept {
    // Validate the complete call before any earlier channel can mutate state.
    for(size_t c=0;c<4;++c)
        if(parameters[c]<0||parameters[c]>drWave246InstrumentRanges[c][1])
            return DrWave246ControlResult::unsupported;
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        const auto target=floorDivide(controls[i],4096);
        if(destination!=2 && !(destination==0&&target>=0) &&
                !(destination==3&&target>=0&&target<=1))
            return DrWave246ControlResult::unsupported;
    }
    if(kind!=0)return DrWave246ControlResult::inactive;
    bool applied=false;
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        if(destination==2)continue;
        applied=true;
        // Reuse the independently verified operations, in original channel order.
        std::array<int16_t,8> single{{0,0,0,0,4096,4096,4096,4096}};
        single[i]=controls[i];single[i+4]=controls[i+4];
        if(destination==0)drWave246ControlInstrumentStep(parameters,single,sources,0);
        else controlVolume246Step(amplitude,pitch,smoothing,single,sources,0);
    }
    return applied?DrWave246ControlResult::applied:DrWave246ControlResult::inactive;
}
}
