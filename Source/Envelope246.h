// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
#include "Envelope246Tables.h"
namespace pocket::reference {
struct Adsr246Parameters {
    int32_t attack=0,decay=0,release=0,sustain=0;
    explicit Adsr246Parameters(const std::array<int16_t,4>& raw) noexcept {
        attack=adsr246Attack[size_t(std::clamp(int(raw[0])/128,0,127))];
        decay=adsr246Decay[size_t(std::clamp(int(raw[1])/128,0,127))];
        release=adsr246Decay[size_t(127-std::clamp(int(raw[3])/128,0,127))];
        sustain=int32_t(std::max(int(raw[2]),0))*32768;
    }
};
// Independently reconstructed 128-sample envelope kernel. Only the listed
// fields belong to the original 24-byte state; scheduler fields are our adapter.
struct Adsr246State {
    uint32_t stage=0,blocks=0;int32_t level=0;
    uint32_t field12=0,field16=0,gain=32767;
    unsigned frame=0;int32_t retriggerStep=0;bool finished=false;
    void noteOn() noexcept {stage=1;blocks=0;level=0;frame=0;finished=false;}
    void noteOff(int16_t sustainWord) noexcept {
        stage=level>int32_t(uint16_t(sustainWord))*65536?4:5;
        blocks=0x10000000u;frame=0;finished=false;
    }
    void fastRelease() noexcept {stage=8;blocks=0;frame=0;finished=false;}
    int32_t advance(const Adsr246Parameters& p) noexcept {
        if(frame==0){finished=false;if(stage==7){stage=1;level=0;}if(stage==6)retriggerStep=int32_t(floorDivide(level,128));}
        const auto activeStage=stage;
        switch(stage){
            case 1:level=std::min(saturate32(int64_t(level)+p.attack),int32_t(1073741824));break;
            case 2:level=std::max(p.sustain,multiplyQ31(level,p.decay));break;
            case 3:level=p.sustain;break;
            case 4:level=std::max(p.sustain,multiplyQ31(level,std::min(p.decay,p.release)));break;
            case 5:level=multiplyQ31(level,p.release);break;
            case 6:level=signedPhase(uint32_t(level)-uint32_t(retriggerStep));break;
            case 8:level=multiplyQ31(level,int32_t(0x7e147ae1));break;
            default:break;
        }
        // Return the final sample BEFORE block-end state transitions.
        const auto sample=level;
        if(++frame==128){frame=0;
            switch(activeStage){
                case 1:if(level>=1073741824)stage=2;++blocks;break;
                case 2:if(level<=p.sustain){level=p.sustain;stage=p.sustain?3:0;finished=!p.sustain;}++blocks;break;
                case 3:++blocks;break;
                case 4:if(level<=p.sustain){stage=5;level=p.sustain;}if(level<=16){stage=0;level=0;finished=true;}++blocks;break;
                case 5:if(level<=16){stage=0;level=0;finished=true;}++blocks;break;
                case 6:stage=7;break;
                case 8:if(level<16){stage=0;finished=true;}break;
                default:break;
            }
        }
        return sample;
    }
};
struct Adsr246Noise {
    uint32_t seed=0x504f434bu;int32_t previous=0;
    int32_t next() noexcept {
        seed=seed*1664525u+1013904223u;
        const auto random=int32_t(floorDivide(signedPhase(seed),65536));
        const auto out=saturate32(int64_t(random-previous)*2);previous=random;return out;
    }
};
// Inactive/unknown stages leave the caller's output buffer untouched, as the
// original does. Dither generation still advances for all 128 sample positions.
inline void adsr246Block(Adsr246State& state,const std::array<int16_t,4>& controls,
                         int16_t volume,Adsr246Noise& noise,std::array<int16_t,128>& output) noexcept {
    const Adsr246Parameters p(controls);const bool writes=(state.stage>=1&&state.stage<=8);
    const int32_t gain=int32_t(multiplyQ15(volume,int16_t(state.gain&65535)))*65536;
    for(size_t i=0;i<128;++i){const auto dither=noise.next();const auto level=state.advance(p);
        if(writes)output[i]=int16_t(floorDivide(saturate32(int64_t(multiplyQ31(level,gain))+dither),65536));}
}
}
