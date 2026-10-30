// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246MixedControl.h"
#include "ControlEnvelope246Mode.h"
namespace pocket::reference {
// Successful type-2 routes with an explicitly initialized six-envelope pool.
// The caller owns the working parameter image and the input buffers/schedule.
inline DrWave246ControlResult drWave246ControlDispatchStep(
        std::array<int16_t,8>& parameters,std::array<int16_t,128>& amplitude,
        std::array<uint32_t,128>& pitch,std::array<int16_t,4>& smoothing,
        std::array<Adsr246State,6>& envelopes,const std::array<int16_t,8>& controls,
        const std::array<uint32_t,4>& sources,uint32_t kind) noexcept {
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        const auto target=floorDivide(controls[i],4096);
        if(destination==2)continue;
        if(!((destination==0&&target>=0)||(destination==1&&target>=0&&target<=4)||
                (destination==3&&target>=0&&target<=1)))
            return DrWave246ControlResult::unsupported;
    }
    if(kind!=0&&kind!=1)return DrWave246ControlResult::inactive;
    auto routed=controls;
    for(size_t i=0;i<4;++i){
        const auto destination=floorDivide(controls[i+4],2048);
        if((kind==0&&destination==1)||(kind==1&&destination!=1))routed[i+4]=4096;
    }
    if(kind==0)return drWave246MixedControlStep(parameters,amplitude,pitch,smoothing,routed,sources,0);
    const auto result=controlEnvelope246DrWaveStep(parameters,envelopes,routed,sources,1);
    return result==ControlEnvelope246Result::applied?DrWave246ControlResult::applied:
        result==ControlEnvelope246Result::inactive?DrWave246ControlResult::inactive:
        DrWave246ControlResult::unsupported;
}
}
