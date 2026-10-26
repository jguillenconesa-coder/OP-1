// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Punch246.h"
#include "Punch246Tables.h"
#include <cmath>
namespace pocket {
// Independent causal integration. 44.1 kHz FX clock is an explicit assumption;
// original coefficient/control blocks and DSP core are tested separately.
struct Punch246Effect {
    static constexpr double internalRate=44100;
    struct Lowpass {
        struct Section {double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;};
        std::array<Section,2> sections{};bool enabled=false;
        void prepare(double rate,double cutoff,bool on) noexcept {
            enabled=on;constexpr double pi=3.14159265358979323846;
            const auto w=2*pi*cutoff/rate,cs=std::cos(w),sn=std::sin(w);const double q[]={.541196100146197,1.306562964876377};
            for(size_t i=0;i<2;++i){const auto alpha=sn/(2*q[i]),a0=1+alpha;sections[i]={(1-cs)/(2*a0),(1-cs)/a0,(1-cs)/(2*a0),-2*cs/a0,(1-alpha)/a0,0,0};}
        }
        void reset() noexcept {for(auto& s:sections)s.z1=s.z2=0;}
        double next(double x) noexcept {if(enabled)for(auto& s:sections){const auto y=s.b0*x+s.z1;s.z1=s.b1*x-s.a1*y+s.z2;s.z2=s.b2*x-s.a2*y;x=y;}return x;}
    };
    reference::Punch246State state;
    reference::Punch246Block block;
    Lowpass inputHost,inputInternal,outputInternal,outputHost;
    double step=1,position=0,previousInput=0,first=0,second=0,dcCoefficient=0,dcInput=0,dcOutput=0;
    int frame=0;int16_t meter=0;bool awake=false,started=false,exactRate=true,removeDC=true;
    uint64_t internalSamples=0;
    void prepare(double rate) noexcept {
        rate=std::clamp(rate,8000.,192000.);step=internalRate/rate;exactRate=rate==internalRate;
        inputHost.prepare(rate,std::min(rate*.4,internalRate*.4),!exactRate);
        inputInternal.prepare(internalRate,std::min(rate*.4,internalRate*.4),rate<internalRate);
        outputInternal.prepare(internalRate,std::min(rate*.4,internalRate*.4),rate<internalRate);
        outputHost.prepare(rate,internalRate*.4,rate>internalRate);
        dcCoefficient=std::exp(-2*3.14159265358979323846*5/rate);reset();
    }
    void reset() noexcept {
        state={};block={};inputHost.reset();inputInternal.reset();outputInternal.reset();outputHost.reset();
        position=previousInput=first=second=dcInput=dcOutput=0;frame=0;meter=0;awake=started=false;internalSamples=0;
    }
    double source(double input,const std::array<int16_t,4>& controls,const std::array<int16_t,8192>& sine) noexcept {
        input=inputInternal.next(input);
        const auto quantized=reference::saturate16(int32_t(std::lround(std::clamp(input,-1.,1.)*32768)));
        if(frame==0){block.begin(state,controls,reference::punch246Poles);meter=reference::multiplyQ15(state.meter,32512);}
        const auto value=block.sample(state,quantized,sine);meter=std::max(meter,value);
        if(++frame==64){frame=0;block.finish(state);state.meter=meter;}
        ++internalSamples;return outputInternal.next(value/32768.);
    }
    float next(float input,const std::array<int16_t,4>& controls,const std::array<int16_t,8192>& sine) noexcept {
        // A freshly reset effect stays exactly silent until audio arrives. The
        // original curve's DC offset would otherwise restart sound after panic.
        if(!awake&&input==0)return 0;
        awake=true;
        const auto filtered=inputHost.next(input);double value=0;
        if(exactRate)value=source(filtered,controls,sine);
        else {
            if(!started){previousInput=filtered;second=source(filtered,controls,sine);started=true;}
            else {
                const auto total=position+step;const int count=int(std::floor(total));
                for(int i=1;i<=count;++i){const auto fraction=(i-position)/step;first=second;second=source(previousInput+(filtered-previousInput)*fraction,controls,sine);}
                position=total-count;previousInput=filtered;
            }
            value=outputHost.next(first+position*(second-first));
        }
        if(removeDC){const auto dc=value-dcInput+dcCoefficient*dcOutput;dcInput=value;dcOutput=dc;value=dc;}
        return float(std::clamp(value,-1.,1.));
    }
};
}
