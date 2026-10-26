// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Digital246.h"
#include "Phase246.h"
namespace pocket {
// Our per-voice envelope, gain and causal rate adapter around the verified core.
struct Digital246Voice {
    reference::Digital246State state;
    Phase246Voice output;
    float source(uint32_t increment,const reference::Digital246Coefficients& controls,const Phase246Config& config,
                 const std::array<int16_t,4>* adsr,bool released) noexcept {
        return output.shapeSample(reference::digital246Sample(state,controls,increment,config.table)/32768.,config,adsr,released);
    }
    float next(double frequency,const reference::Digital246Coefficients& controls,const Phase246Config& config,
               const std::array<int16_t,4>* adsr=nullptr,bool released=false) noexcept {
        if(frequency!=output.cachedFrequency){output.cachedFrequency=frequency;output.increment=uint32_t(std::clamp(frequency,0.,Phase246Config::internalRate*.49)/Phase246Config::internalRate*4294967296.+.5);}
        if(!output.started){output.first=source(output.increment,controls,config,adsr,released);output.second=source(output.increment,controls,config,adsr,released);output.started=true;}
        const float result=output.first+float(output.position)*(output.second-output.first);
        output.position+=config.step;
        while(output.position>=1){output.position-=1;output.first=output.second;output.second=source(output.increment,controls,config,adsr,released);}
        return result;
    }
};
}
