// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246.h"
namespace pocket::reference {
// Six contiguous banks, in ascending original runtime address order. These are
// generated numerical tables, not patches or embedded factory sample assets.
struct DrWave246WaveBank {
    std::array<std::array<int16_t,256>,320> wave{};
    static constexpr std::array<size_t,6> first{{0,64,128,192,256,288}};
    static constexpr std::array<size_t,6> count{{64,64,64,64,32,32}};
};
static_assert(sizeof(DrWave246WaveBank)==163840);
struct DrWave246WaveSettings {
    uint32_t modeAndReady=0;
    int32_t workingGain=0,referenceGain=0,outputGain=0;
};
static_assert(sizeof(DrWave246WaveSettings)==16);
inline void drWave246InitializeShared(DrWave246Waves& waves,DrWave246Context& context,
                                      DrWave246WaveSettings& settings) noexcept {
    for(auto& wave:waves)wave.fill(0);
    context.increments.fill(0);
    settings.workingGain=0;settings.referenceGain=32768;settings.outputGain=262144;
}
// This is startup work, not a real-time callback. Match the sequence consisting
// of the shared initializer followed by the original bank builder with a clean
// scratch wave, optionally overriding referenceGain between those calls.
inline void drWave246BuildWaveBank(DrWave246WaveBank& bank,DrWave246WaveSettings& settings,
                                  const std::array<int16_t,8192>& sine) noexcept {
    settings.workingGain=settings.referenceGain;
    for(size_t family=0;family<6;++family){
        const bool descending=family%2==0;
        const bool oddOnly=family>=2;
        const bool triangle=family>=4;
        const int top=triangle?31:63;
        for(size_t index=0;index<DrWave246WaveBank::count[family];++index){
            std::array<uint32_t,256> sum{};
            for(int harmonic=1;harmonic<=top;++harmonic){
                if(oddOnly&&harmonic%2==0)continue;
                if(descending?harmonic<=int(index):harmonic>int(index))continue;
                const int sign=triangle&&harmonic%4==3?-1:1;
                const int divisor=triangle?harmonic*harmonic:harmonic;
                // Signed division truncates toward zero, followed by two
                // arithmetic shifts. Folding these steps changes negative bins.
                const int32_t coefficient=sign*65536/divisor;
                const auto scaled=signedPhase(drWave246Multiply16(uint32_t(coefficient),uint32_t(settings.workingGain)));
                const uint32_t gainWord=uint32_t(floorDivide(scaled,8))&65535;
                const int16_t gain=int16_t(gainWord<32768?int32_t(gainWord):int32_t(gainWord)-65536);
                for(size_t sample=0;sample<256;++sample){
                    const auto value=sine[(sample*size_t(harmonic)*32)&8191];
                    sum[sample]+=uint32_t(floorDivide(int32_t(gain)*int32_t(value),8192));
                }
            }
            auto& wave=bank.wave[DrWave246WaveBank::first[family]+index];
            for(size_t sample=0;sample<256;++sample){
                const auto word=sum[sample]&65535;
                wave[sample]=int16_t(word<32768?int32_t(word):int32_t(word)-65536);
            }
        }
    }
    settings.modeAndReady=(settings.modeAndReady&0xffff00ffu)|0x100;
}
}
