// SPDX-License-Identifier: AGPL-3.0-only
#include "StringVoice.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
int main(){try{
    double worstCents=0;
    for(double rate:{44100.,48000.,96000.})for(double note:{36.,60.,69.,84.})for(double bend:{-2.,0.,2.}){
        const double frequency=440*std::exp2((note-69+bend)/12);pocket::StringVoice voice;std::array<float,8192> buffer;buffer.fill(1000000);
        uint32_t state=0x73574731;auto noise=[&]{state^=state<<13;state^=state>>17;state^=state<<5;return float(state/2147483648.-1);};
        voice.excite(buffer,440*std::exp2((note-69)/12),rate,noise);const double retune=1-std::exp(-1/(rate*.003));
        for(int i=0;i<int(rate*.2);++i)check(std::abs(voice.next(buffer,frequency,rate,1,retune))<1,"Retuning reads stale string storage");
        std::vector<float> signal(size_t(rate/2));for(auto& sample:signal)sample=voice.next(buffer,frequency,rate,1,retune);
        const int center=int(rate/frequency),low=std::max(2,center-5),high=center+5;
        auto error=[&](int lag){double sum=0;for(size_t i=0;i<signal.size()-size_t(high);++i){const auto d=double(signal[i])-signal[i+size_t(lag)];sum+=d*d;}return sum;};
        int best=low;for(int lag=low;lag<=high;++lag)if(error(lag)<error(best))best=lag;
        const auto a=error(best-1),mid=error(best),c=error(best+1);const double delta=.5*(a-c)/(a-2*mid+c),detected=rate/(best+delta);
        const double cents=1200*std::log2(detected/frequency);check(std::isfinite(cents)&&std::abs(cents)<3,"String tuning or +/-2 semitone bend is incorrect");worstCents=std::max(worstCents,std::abs(cents));
    }
    // Immediate downward bends on a low note must stay inside initialized excitation.
    pocket::StringVoice voice;std::array<float,8192> buffer;buffer.fill(1000000);voice.excite(buffer,32.7,96000,[]{return .5f;});
    for(int i=0;i<4096;++i)check(std::isfinite(voice.next(buffer,29.13,96000,1,.05))&&std::abs(voice.previous)<1,"Low-note bend leaks the stolen voice buffer");
    std::cout<<"PASS: fractional String loop, 36 tuning/bend configurations, worst autocorrelation estimate "<<worstCents<<" cents; poisoned-buffer reuse and early downward bend\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
