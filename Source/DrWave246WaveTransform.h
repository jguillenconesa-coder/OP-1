// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246.h"
namespace pocket::reference {
enum class DrWave246Transform { scatterHold,clamp,repeat };
struct DrWave246TransformState {
    uint32_t scatterInitialized=0,repeatInitialized=0;
    std::array<uint16_t,8> reciprocalSeed{};
    std::array<uint32_t,256> scatterCache{},repeatCache{};
    uint32_t reciprocalInitialized=0;
};
static_assert(sizeof(DrWave246TransformState)==2076);
// Explicit internal Q16 word in [0,65536], not a host/knob mapping.
inline uint32_t drWave246TransformRate(uint32_t word) noexcept {
    const uint32_t scaled=word*7;
    return (65536+(scaled&65535))<<(scaled>>16);
}
inline uint32_t drWave246TransformReciprocal(uint32_t rate) noexcept {
    unsigned bit=0;for(uint32_t n=rate;n>>=1;)++bit;
    const unsigned index=(rate>>(bit-3))&7;
    uint32_t estimate=(uint32_t(drWave246ReciprocalSeed[index])*4)>>(bit-15);
    for(int i=0;i<2;++i)estimate=drWave246Multiply16(estimate,131072-drWave246Multiply16(rate,estimate));
    return estimate;
}
inline bool drWave246TransformWave(std::array<uint32_t,256>& wave,DrWave246TransformState& state,
                                    DrWave246Transform mode,int32_t word) noexcept {
    if(word<0||word>65536)return false;
    if(mode!=DrWave246Transform::scatterHold&&mode!=DrWave246Transform::clamp&&mode!=DrWave246Transform::repeat)return false;
    if(mode==DrWave246Transform::scatterHold&&state.reciprocalInitialized&&state.reciprocalSeed!=drWave246ReciprocalSeed)return false;
    const uint32_t rate=drWave246TransformRate(uint32_t(word));
    if(mode==DrWave246Transform::clamp){
        for(size_t i=0;i<256;++i)wave[i]=wave[std::min(size_t((uint64_t(i)*rate)>>16),size_t(255))];
    }else if(mode==DrWave246Transform::repeat){
        if(!state.repeatInitialized)state.repeatInitialized=1;
        state.repeatCache=wave;
        for(size_t i=0;i<256;++i)wave[i]=state.repeatCache[size_t((uint64_t(i)*rate)>>16)&255];
    }else{
        if(!state.scatterInitialized){state.scatterInitialized=1;state.scatterCache.fill(0);}
        if(!state.reciprocalInitialized){state.reciprocalInitialized=1;state.reciprocalSeed=drWave246ReciprocalSeed;}
        const uint32_t reciprocal=drWave246TransformReciprocal(rate);
        for(size_t i=0;i<256;++i)state.scatterCache[size_t((uint64_t(i)*reciprocal)>>16)]=wave[i];
        for(size_t i=0;i<256;++i)wave[i]=state.scatterCache[size_t((uint64_t(i)*reciprocal)>>16)];
    }
    return true;
}
}
