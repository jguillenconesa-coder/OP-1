// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246Spectral.h"
#include "DrWave246WaveTransform.h"
namespace pocket::reference {
struct DrWave246Runtime {
    DrWave246WaveSettings settings;
    std::array<int32_t,8> configuration{};
    DrWave246TransformState transform;
    std::array<uint32_t,5> increments{};
    uint32_t wavePointer=0;
};
static_assert(sizeof(DrWave246Runtime)==2148);
namespace drwave246_detail {
inline std::array<int32_t,8> prepare(const std::array<int16_t,4>& controls) noexcept {
    const int32_t blue=controls[0],section=blue/8192,x=blue%8192,cycle=x*24;
    const int32_t green=controls[1]*8,gf=green&65535,white=controls[2]*8,wf=white&65535;
    const int32_t greenCube=drwave246_spectral_detail::mul(gf,drwave246_spectral_detail::mul(gf,gf));
    const int32_t distance=wf>=32768?wf-32768:32768-wf;
    const int32_t warp=std::max(0,int32_t((int64_t(65536-distance*2)*66191)/65536)-655);
    return {{cycle>>16,((cycle>>16)+1)%3,cycle&65535,section==0?0:(section==1?x*8:65536-x*8),
              white>>16,warp,green>>16,greenCube}};
}
inline void update(DrWave246State& state,DrWave246Waves& waves,DrWave246Runtime& runtime,
                   const DrWave246WaveBank& bank,const std::array<int16_t,8192>& sine,int16_t orange) noexcept {
    auto& settings=runtime.settings;const auto& c=runtime.configuration;auto& wave=waves[state.slot];
    const int32_t argument=c[7]*64;
    settings.workingGain=drwave246_spectral_detail::mul(65536-c[2],settings.referenceGain);
    settings.modeAndReady=(settings.modeAndReady&0xffffff00u)|1;
    const unsigned variant=c[6]==1?0:1;
    drWave246MixSpectrum(wave,bank,sine,settings,static_cast<DrWave246Spectrum>(unsigned(c[0])*2+variant),argument);
    settings.workingGain=drwave246_spectral_detail::mul(c[2],settings.referenceGain);
    settings.modeAndReady&=0xffffff00u;
    drWave246MixSpectrum(wave,bank,sine,settings,static_cast<DrWave246Spectrum>(unsigned(c[1])*2+variant),argument);
    const uint32_t doubled=uint32_t(orange)*2;
    const uint32_t step=uint32_t(uint64_t(doubled)*doubled>>16)*4;
    state.field32=(state.field32+step)%0x02000000;
    const uint32_t position=state.field32>>16;
    if(position&256){for(size_t i=position&255;i<256;++i)wave[i]=0u-wave[i];}
    else{for(size_t i=0;i<position;++i)wave[i]=0u-wave[i];}
    if(c[4]==0)drWave246TransformWave(wave,runtime.transform,DrWave246Transform::clamp,c[5]);
    else if(c[4]==1)drWave246TransformWave(wave,runtime.transform,DrWave246Transform::repeat,c[5]);
    drWave246TransformWave(wave,runtime.transform,DrWave246Transform::scatterHold,c[3]);
}
}
// Entire original render route in a bounded isolated context. This adapter
// still does not implement the external scheduler, allocator, ADSR or output.
inline DrWave246ReadResult drWave246Block(DrWave246State& state,DrWave246Waves& waves,
        DrWave246Runtime& runtime,const DrWave246WaveBank& bank,const std::array<int16_t,8192>& sine,
        const std::array<int16_t,4>& controls,const std::array<uint32_t,128>& pitch,
        int updateCounter,std::array<int16_t,128>& output) noexcept {
    if(!(runtime.settings.modeAndReady&0xff00))return DrWave246ReadResult::inactive;
    if(state.slot>=6||updateCounter<0||updateCounter>32767||controls[3]<0)return DrWave246ReadResult::unsupported;
    for(auto phase:state.phase)if(phase>0xffffff)return DrWave246ReadResult::unsupported;
    if(runtime.transform.reciprocalInitialized&&runtime.transform.reciprocalSeed!=drWave246ReciprocalSeed)return DrWave246ReadResult::unsupported;
    const bool updating=int(state.slot)==updateCounter%6;
    if(updating&&(controls[0]<0||controls[0]>=24576||controls[1]<0||controls[2]<0||state.field32>=0x02000000||
        runtime.settings.referenceGain!=32768||runtime.settings.outputGain<0||runtime.settings.outputGain>262144))
        return DrWave246ReadResult::unsupported;
    runtime.wavePointer=0x01cea08c+state.slot*1024;
    if(updating){
        runtime.configuration=drwave246_detail::prepare(controls);
        drwave246_detail::update(state,waves,runtime,bank,sine,controls[3]);
    }
    DrWave246Context reader;
    reader.initialized=runtime.transform.reciprocalInitialized;reader.reciprocalSeed=runtime.transform.reciprocalSeed;
    drwave246_detail::readPrepared(state,reader,waves,controls[3],pitch[0],output);
    runtime.increments=reader.increments;runtime.wavePointer=reader.wavePointer;
    runtime.transform.reciprocalInitialized=reader.initialized;runtime.transform.reciprocalSeed=reader.reciprocalSeed;
    return DrWave246ReadResult::rendered;
}
}
