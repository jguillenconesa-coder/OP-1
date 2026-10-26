// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
namespace pocket::reference {
struct Punch246State {
    uint32_t tag=0;std::array<int32_t,24> a{},b{};int32_t pole=0;
    int16_t meter=0,padding=0;int32_t slot=0;
    void reset(int32_t position=0) noexcept {a.fill(0);b.fill(0);pole=0;slot=position;}
};
// Independent reconstruction of the sine-interpolated nonlinearity. Argument
// uses its signed high word; the low word is deliberately discarded.
inline int32_t punch246Curve(int32_t x,const std::array<int16_t,8192>& sine) noexcept {
    const auto high=int32_t(floorDivide(x,65536));
    const auto index=size_t(4096-floorDivide(high,16));
    const auto fraction=int16_t((uint32_t(high)&15u)*2048);
    return saturate32(int64_t(sine[index])*fraction*2+
                      int64_t(sine[index+1])*(32767-fraction)*2);
}
struct Punch246Block {
    int stages=1;int32_t feedback=0,wet=0;int16_t dry=32767;
    void begin(Punch246State& s,const std::array<int16_t,4>& c,const std::array<int32_t,129>& table) noexcept {
        stages=std::clamp(int(c[2])/1024,1,24);
        const auto index=size_t(std::clamp(int(c[0])/128+int(c[2])/4096,0,128));
        s.pole=saturate32(int64_t(multiplyQ31(s.pole,0x70000000))+multiplyQ31(table[index],0x10000000));
        int32_t total=0;
        for(int i=0;i<stages;++i){const auto x=s.b[size_t(i)],negative=signedPhase(0u-uint32_t(x));total=saturate32(int64_t(total)+std::max(x,negative));}
        feedback=std::max(int32_t(0),saturate32(int64_t(c[1])*65536-total));
        wet=int32_t(c[3])*65536;dry=saturate16(32767-int(c[3]));
    }
    int16_t sample(Punch246State& s,int16_t input,const std::array<int16_t,8192>& sine) const noexcept {
        auto x=saturate32(int64_t(multiplyQ31(int32_t(input)*65536,wet))-
                          multiplyQ31(punch246Curve(s.a[size_t(stages-1)],sine),feedback));
        auto curved=punch246Curve(multiplyQ31(x,s.pole),sine);
        for(int i=0;i<stages;++i){
            const auto index=size_t(i);
            x=saturate32(int64_t(saturate32(int64_t(curved)-s.b[index]))+s.a[index]);
            s.a[index]=x;curved=punch246Curve(multiplyQ31(x,s.pole),sine);s.b[index]=curved;
        }
        const auto processed=multiplyQ31(punch246Curve(x,sine),wet);
        return saturate16(int(multiplyQ15(input,dry))+int(floorDivide(processed,65536)));
    }
    void finish(Punch246State& s) const noexcept {
        for(int i=stages;i<24;++i){s.a[size_t(i)]=s.a[size_t(stages-1)];s.b[size_t(i)]=s.b[size_t(stages-1)];}
    }
};
inline void punch246Block(Punch246State& s,const std::array<int16_t,4>& controls,const std::array<int32_t,129>& table,
                          const std::array<int16_t,8192>& sine,std::array<int16_t,64>& audio) noexcept {
    Punch246Block block;block.begin(s,controls,table);
    auto meter=multiplyQ15(s.meter,32512);
    for(auto& value:audio){value=block.sample(s,value,sine);meter=std::max(meter,value);}
    block.finish(s);s.meter=meter;
}
}
