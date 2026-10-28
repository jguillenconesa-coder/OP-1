// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
namespace pocket::reference {
// Isolated reader/mixer route. Wave construction and the original scheduler
// remain separate dependencies; all six 256-word waves are explicit inputs.
struct DrWave246State {
    uint32_t tag=0,field4=0;
    std::array<uint32_t,5> phase{};
    uint32_t slot=0,field32=0;
    void reset() noexcept {phase.fill(0);field32=0;}
    void release() noexcept {} // The common ADSR is external to this callback.
    void construct(uint32_t index) noexcept {tag=0x0190fb2c;field4=0;slot=index;reset();}
};
static_assert(sizeof(DrWave246State)==36);
using DrWave246Waves=std::array<std::array<uint32_t,256>,6>;
struct DrWave246Context {
    std::array<uint32_t,5> increments{};
    uint32_t initialized=0;
    std::array<uint16_t,8> reciprocalSeed{};
    uint32_t wavePointer=0;
};
inline constexpr std::array<uint16_t,8> drWave246ReciprocalSeed{{32768,29127,26214,23831,21845,20164,18724,17476}};
inline uint32_t drWave246Multiply16(uint32_t a,uint32_t b) noexcept {
    return uint32_t(floorDivide(int64_t(signedPhase(a))*signedPhase(b),65536));
}
// With the tested orange range [0,32767], the reciprocal's normalized
// lookup index is always zero. Preserve both integer refinement iterations.
inline std::array<uint32_t,5> drWave246Increments(uint32_t pitch,int16_t orange) noexcept {
    const auto doubled=int32_t(orange)*2;
    const uint32_t square=uint32_t(int64_t(doubled)*doubled/65536);
    const uint32_t spread=uint32_t(uint64_t(square)*655>>16);
    const uint32_t factor=65536+spread;
    uint32_t reciprocal=131072-factor;
    reciprocal=drWave246Multiply16(reciprocal,131072-drWave246Multiply16(factor,reciprocal));
    std::array<uint32_t,5> result{};
    result[0]=uint32_t(floorDivide(signedPhase(pitch),256));
    result[1]=drWave246Multiply16(result[0],factor);
    result[2]=drWave246Multiply16(result[1],factor);
    result[3]=drWave246Multiply16(result[0],reciprocal);
    result[4]=drWave246Multiply16(result[3],reciprocal);
    return result;
}
enum class DrWave246ReadResult { rendered,inactive,unsupported };
// The original signed word load is followed by unsigned low-half products.
// Its dispatch remainder therefore covers all 16 bits, including 0x8000..ffff.
inline constexpr unsigned drWave246UpdateSlot(uint16_t counter) noexcept {return counter%6u;}
namespace drwave246_detail {
// Preconditions are checked by the bounded reader or complete-kernel adapter.
inline void readPrepared(DrWave246State& state,DrWave246Context& context,const DrWave246Waves& waves,
                         int16_t orange,uint32_t pitch,std::array<int16_t,128>& output) noexcept {
    context.wavePointer=0x01cea08c+state.slot*1024;
    context.increments=drWave246Increments(pitch,orange);
    if(!context.initialized){context.initialized=1;context.reciprocalSeed=drWave246ReciprocalSeed;}
    const auto& wave=waves[state.slot];
    for(auto& sample:output){
        uint32_t sum=0;
        for(size_t i=0;i<5;++i){
            sum+=wave[state.phase[i]>>16]>>4;
            state.phase[i]=(state.phase[i]+context.increments[i])&0x00ffffff;
        }
        const uint32_t word=sum&65535;
        sample=int16_t(word<32768?int32_t(word):int32_t(word)-65536);
    }
}
}
inline DrWave246ReadResult drWave246ReadBlock(DrWave246State& state,DrWave246Context& context,
        const DrWave246Waves& waves,const std::array<int16_t,4>& controls,
        const std::array<uint32_t,128>& pitch,int updateCounter,bool enabled,
        std::array<int16_t,128>& output) noexcept {
    if(!enabled)return DrWave246ReadResult::inactive;
    // This reader-only adapter excludes wave updates. The complete render
    // adapter in DrWave246Kernel.h handles that separately verified route.
    if(state.slot>=6||updateCounter<0||updateCounter>65535||state.slot==drWave246UpdateSlot(uint16_t(updateCounter))||controls[3]<0)
        return DrWave246ReadResult::unsupported;
    for(auto phase:state.phase)if(phase>0x00ffffff)return DrWave246ReadResult::unsupported;
    if(context.initialized&&context.reciprocalSeed!=drWave246ReciprocalSeed)
        return DrWave246ReadResult::unsupported;
    drwave246_detail::readPrepared(state,context,waves,controls[3],pitch[0],output);
    return DrWave246ReadResult::rendered;
}
}
