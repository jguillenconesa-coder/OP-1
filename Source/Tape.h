// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace pocket {
// Four mono tracks. Allocation, clearing and state copies happen outside process().
class Tape {
public:
    void prepare(double sr) {
        if(rate!=sr||tracks[0].empty()) {
            for(auto& t:tracks) {
                std::vector<float> next(size_t(sr*16),0);
                if(!t.empty()) for(size_t i=0;i<next.size();++i) {
                    const double pos=double(i)*rate/sr; const auto a=std::min(size_t(pos),t.size()-1),b=std::min(a+1,t.size()-1);
                    next[i]=t[a]+float(pos-std::floor(pos))*(t[b]-t[a]);
                }
                t=std::move(next);
            }
            rate=sr;
        }
        position=0;
    }
    void clear(int track) { if(track>=0&&track<4) std::fill(tracks[size_t(track)].begin(),tracks[size_t(track)].end(),0); }
    void rewind() { position=0; }
    float process(float input,bool play,bool record,bool overdub,int selected,float speed,float seconds,const std::array<float,4>& gains) {
        if(!play||tracks[0].empty()) return input;
        const auto length=std::clamp(size_t(rate*seconds),size_t(2),tracks[0].size());
        position=std::fmod(position+double(length),double(length));
        const auto a=size_t(position),b=(a+1)%length; const float fraction=float(position-double(a));
        float mixed=input;
        for(size_t i=0;i<4;++i) {
            const float old=tracks[i][a]+fraction*(tracks[i][b]-tracks[i][a]);
            if(!(record&&!overdub&&int(i)==selected)) mixed+=old*gains[i];
            if(record&&int(i)==selected) tracks[i][a]=std::clamp(input+(overdub?tracks[i][a]:0),-1.f,1.f);
        }
        position+=record?1.0:double(speed);
        return std::clamp(mixed,-1.f,1.f);
    }
    double getPosition() const { return position/rate; }
    double getRate() const { return rate; }
    const std::array<std::vector<float>,4>& getTracks() const { return tracks; }
    void restore(std::array<std::vector<float>,4> data,double sr) { tracks=std::move(data); rate=sr; position=0; }
private:
    std::array<std::vector<float>,4> tracks;
    double rate=48000,position=0;
};
}
