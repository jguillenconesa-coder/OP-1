// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReferenceDsp.h"
#include <cmath>
namespace pocket {
// Experimental native Pd voice. Only pdSample is matched against firmware.
// Fixed clock, NCO, smoothing, filtering and rate adaptation are our integration.
struct Phase246Config {
    static constexpr double internalRate=88200;
    struct Biquad {double b0=1,b1=0,b2=0,a1=0,a2=0;};
    std::array<Biquad,2> filters{};
    std::array<int16_t,8192> table{};
    double step=2;
    void prepare(double hostRate) noexcept {
        step=internalRate/hostRate;
        constexpr double pi=3.14159265358979323846;
        for(size_t i=0;i<table.size();++i)table[i]=int16_t(std::lround(std::sin(i*pi/4096)*32767));
        // Our causal 4-pole Butterworth; not the original output decimator.
        const double w=2*pi*std::min(20000.,hostRate*.4)/internalRate;
        const double cs=std::cos(w),sn=std::sin(w);
        const double q[]={.541196100146197,1.306562964876377};
        for(size_t i=0;i<2;++i){const double alpha=sn/(2*q[i]),a0=1+alpha;filters[i]={(1-cs)/(2*a0),(1-cs)/a0,(1-cs)/(2*a0),-2*cs/a0,(1-alpha)/a0};}
    }
};
struct Phase246Voice {
    reference::PdState state;
    std::array<std::array<double,2>,2> memory{};
    double position=0,cachedFrequency=-1;uint32_t increment=0;float first=0,second=0;bool started=false;
    float source(uint32_t increment,const reference::PdCoefficients& controls,const Phase246Config& config) noexcept {
        double sample=reference::pdSample(state,increment,controls,config.table)/32768.;
        for(size_t i=0;i<2;++i){const auto& c=config.filters[i];auto& m=memory[i];const double out=c.b0*sample+m[0];m[0]=c.b1*sample-c.a1*out+m[1];m[1]=c.b2*sample-c.a2*out;sample=out;}
        return float(sample);
    }
    float next(double frequency,const reference::PdCoefficients& controls,const Phase246Config& config) noexcept {
        if(frequency!=cachedFrequency){cachedFrequency=frequency;increment=uint32_t(std::clamp(frequency,0.,Phase246Config::internalRate*.49)/Phase246Config::internalRate*4294967296.+.5);}
        if(!started){first=source(increment,controls,config);second=source(increment,controls,config);started=true;}
        const float result=first+float(position)*(second-first);
        position+=config.step;
        while(position>=1){position-=1;first=second;second=source(increment,controls,config);}
        return result;
    }
};
}
