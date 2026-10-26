// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
namespace pocket {
// All storage is fixed. Timing is expressed in beats so a host seek is immediate.
struct SequenceTiming {int64_t tick;double phase;};
inline SequenceTiming sequenceTiming(double beat,double division,double swing) {
    const double units=beat*division, pair=std::floor(units/2),within=units-pair*2;
    const double first=1+std::clamp(swing,0.,.45);
    const bool second=within>=first;
    return {int64_t(pair)*2+(second?1:0),second?(within-first)/(2-first):within/first};
}
inline int sequenceIndex(int64_t tick,int length,int direction) {
    length=std::clamp(length,1,16);
    const auto mod=[](int64_t value,int n){return int((value%n+n)%n);};
    const int forward=mod(tick,length);
    if(direction==1)return length-1-forward;
    if(direction==2){if(length==1)return 0;const int pos=mod(tick,2*length-2);return pos<length?pos:2*length-2-pos;}
    if(direction==3){uint64_t x=uint64_t(tick)+0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return int((x^(x>>31))%uint64_t(length));}
    return forward;
}
inline bool euclideanPulse(int64_t tick,int length,int pulses,int rotation) {
    length=std::clamp(length,1,16);pulses=std::clamp(pulses,0,length);
    const int index=int(((tick-rotation)%length+length)%length);
    return (index*pulses)%length<pulses;
}
class HeldNotes {
    std::array<std::bitset<128>,16> channels{};
    int held=0;
public:
    bool empty() const{return held==0;}
    void clear(){for(auto& c:channels)c.reset();held=0;}
    void clear(int channel){if(channel>=1&&channel<=16){auto& c=channels[size_t(channel-1)];held-=int(c.count());c.reset();}}
    void set(int channel,int note,bool down){if(channel>=1&&channel<=16&&note>=0&&note<128){auto& c=channels[size_t(channel-1)];if(c[size_t(note)]!=down){held+=down?1:-1;c.set(size_t(note),down);}}}
    int note(int64_t tick,int direction,int octaves) const {
        std::array<int,384> notes{};int count=0;
        for(int octave=0;octave<std::clamp(octaves,1,3);++octave)for(int n=0;n<128;++n){bool down=false;for(const auto& c:channels)down=down||c[size_t(n)];if(down&&n+12*octave<128)notes[size_t(count++)]=n+12*octave;}
        if(!count)return -1;
        // Overlapping octave ranges are sorted and de-duplicated without a heap.
        std::sort(notes.begin(),notes.begin()+count);count=int(std::unique(notes.begin(),notes.begin()+count)-notes.begin());
        const auto mod=[](int64_t value,int n){return int((value%n+n)%n);};
        int index=mod(tick,count);
        if(direction==1)index=count-1-index;
        if(direction==2&&count>1){index=mod(tick,2*count-2);if(index>=count)index=2*count-2-index;}
        return notes[size_t(index)];
    }
};
}
