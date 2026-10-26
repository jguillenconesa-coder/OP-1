// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace pocket {
// Our Karplus-Strong model, not a reconstruction of the OP-1 String kernel.
// Fractional loop delay compensates the averaging filter's half-sample delay.
struct StringVoice {
    static constexpr size_t capacity=8192;
    bool ready=false;size_t write=0;double delay=2;float previous=0;
    template<class Noise> void excite(std::array<float,capacity>& buffer,double frequency,double rate,Noise&& noise) noexcept {
        delay=std::clamp(rate/frequency-.5,1.5,double(capacity-2));
        const auto count=std::min(capacity,size_t(std::ceil(delay))+2);
        // Only the initial delay segment can be read before it is overwritten.
        for(size_t i=capacity-count;i<capacity;++i)buffer[i]=noise()*.6f;
        write=0;previous=0;ready=true;
    }
    float next(std::array<float,capacity>& buffer,double frequency,double rate,float tone,double retune) noexcept {
        const auto target=std::clamp(rate/frequency-.5,1.5,double(capacity-2));
        // Keep the first read head advancing through initialized excitation only.
        delay+=std::clamp(retune*(target-delay),-.5,.5);
        double position=double(write)+capacity-delay;if(position>=capacity)position-=capacity;
        const auto first=size_t(position),second=(first+1)%capacity;const float fraction=float(position-first);
        const float sample=buffer[first]+fraction*(buffer[second]-buffer[first]);
        buffer[write]=(sample+previous)*(.475f+tone*.0245f);
        previous=sample;write=(write+1)%capacity;return sample;
    }
};
}
