// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246WaveBank.h"
namespace pocket::reference {
// Descriptive callback names, not instrument/host parameter IDs.
enum class DrWave246Spectrum { allUp,allDown,oddUp,oddDown,triangleUp,triangleDown };
namespace drwave246_spectral_detail {
inline int32_t mul(int32_t a,int32_t b) noexcept {
    return signedPhase(drWave246Multiply16(uint32_t(a),uint32_t(b)));
}
inline int32_t triangleCoefficient(int harmonic) noexcept {
    return (harmonic&2?-65536:65536)/(harmonic*harmonic);
}
inline void add(std::array<uint32_t,256>& wave,const std::array<int16_t,8192>& sine,
                int harmonic,int32_t coefficient) noexcept {
    if(harmonic<=0||harmonic>=64)return;
    const uint32_t low=uint32_t(floorDivide(coefficient,8))&65535;
    const int16_t gain=int16_t(low<32768?int32_t(low):int32_t(low)-65536);
    for(size_t i=0;i<256;++i)
        wave[i]+=uint32_t(floorDivide(int32_t(gain)*sine[(i*size_t(harmonic)*32)&8191],8192));
}
}
// Bounded isolated spectral callbacks. Argument and all settings are explicit;
// this does not perform the original knob conversion or wave-update dispatch.
inline bool drWave246MixSpectrum(std::array<uint32_t,256>& wave,const DrWave246WaveBank& bank,
        const std::array<int16_t,8192>& sine,const DrWave246WaveSettings& settings,
        DrWave246Spectrum mode,int32_t argument) noexcept {
    if(argument<0||argument>=64*65536||settings.referenceGain!=32768||settings.workingGain<0||
       settings.workingGain>65536||settings.outputGain<0||settings.outputGain>262144)return false;
    const auto kind=static_cast<unsigned>(mode);if(kind>=6)return false;
    const bool down=kind%2!=0,odd=kind>=2,triangle=kind>=4;
    int32_t value=argument;
    if(mode==DrWave246Spectrum::allDown||mode==DrWave246Spectrum::oddDown||mode==DrWave246Spectrum::triangleUp)value/=2;
    else if(mode==DrWave246Spectrum::triangleDown)value/=4;
    if(odd&&value<65536)value=(value-settings.referenceGain)*2;
    const int whole=int(floorDivide(value,65536));
    const size_t table=size_t(std::max(whole,0));
    const size_t family=kind^1u;
    if(table>=DrWave246WaveBank::count[family])return false;
    const auto& prefix=bank.wave[DrWave246WaveBank::first[family]+table];
    for(size_t i=0;i<256;++i){
        const uint32_t product=uint32_t(int64_t(prefix[i])*settings.workingGain);
        const uint32_t scaled=uint32_t(floorDivide(signedPhase(product),16384));
        wave[i]=(settings.modeAndReady&255)?scaled:wave[i]+scaled;
    }
    using namespace drwave246_spectral_detail;
    const auto gain=settings.workingGain,boost=settings.outputGain;
    if(!odd){
        if(!down){
            const int harmonic=whole+1;const int32_t fraction=value-whole*65536;
            const int32_t quotient=fraction/harmonic;
            add(wave,sine,harmonic,mul(quotient,gain));
            if(whole>0)add(wave,sine,whole,mul(mul(boost,65536-fraction)/whole,gain));
            add(wave,sine,harmonic,mul(mul(boost,quotient),gain));
        }else{
            const int32_t fraction=65536-(value-whole*65536);
            if(whole>0){
                const int32_t quotient=fraction/whole;
                add(wave,sine,whole,mul(quotient,gain));
                add(wave,sine,whole,mul(mul(boost,quotient),gain));
            }
            add(wave,sine,whole+1,mul(mul(boost,65536-fraction)/(whole+1),gain));
        }
    }else{
        const int harmonic=down?(whole>= (triangle?31:63)?(triangle?31:63):
                    (triangle?29:61)-2*(( (triangle?30:62)-whole)/2)):
                    (whole<1?1:((whole-1)&~1)+3);
        const int32_t difference=down?harmonic*65536-value:value-harmonic*65536;
        const int32_t fraction=difference/2+65536; // signed division toward zero
        const int adjacent=harmonic+(down?2:-2);
        if(!triangle){
            const int32_t quotient=fraction/harmonic;
            add(wave,sine,harmonic,mul(quotient,gain));
            add(wave,sine,adjacent,mul(mul(boost,65536-fraction)/adjacent,gain));
            add(wave,sine,harmonic,mul(mul(boost,quotient),gain));
        }else{
            const int32_t coefficient=triangleCoefficient(harmonic);
            add(wave,sine,harmonic,mul(mul(coefficient,fraction),gain));
            const int32_t other=triangleCoefficient(adjacent);
            if(down){
                add(wave,sine,adjacent,mul(mul(mul(boost,65536-fraction),other),gain));
                add(wave,sine,harmonic,mul(mul(mul(boost,fraction),coefficient),gain));
            }else{
                add(wave,sine,adjacent,mul(mul(mul(boost,other),65536-fraction),gain));
                add(wave,sine,harmonic,mul(mul(mul(boost,coefficient),fraction),gain));
            }
        }
    }
    return true;
}
}
