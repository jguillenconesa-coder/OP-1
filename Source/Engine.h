// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pocket {
constexpr double pi = 3.14159265358979323846;
struct Parameters {
    int engine=0;
    float tone=.45f,color=.35f,envelope=.35f,effect=.2f,volume=.7f;
    float attack=.02f,decay=.2f,sustain=.8f,release=.3f;
    float lfoRate=1,lfoDepth=0; int lfoDestination=0,lfoShape=0;
    float delayTime=.281f,feedback=.42f,drive=0;
    bool sampleLoop=false;
};
struct Sample { std::vector<float> frames; double rate=48000; };
class Engine {
public:
    static constexpr int polyphony=8;
    void prepare(double rate) { sampleRate=std::max(8000.0,rate); smoothing=float(1-std::exp(-1.0/(sampleRate*.012))); reset(); }
    void reset() { voices={}; delay.fill(0); write=0; filter=0; current={}; lfoPhase=0; bends.fill(0); pedals.fill(false); }
    void setSample(Sample data) { sample=std::move(data); voices={}; }
    const Sample& getSample() const { return sample; }
    void setKit(std::array<Sample,8> data) { kit=std::move(data); voices={}; }
    const std::array<Sample,8>& getKit() const { return kit; }
    static int kitSlot(int note) {
        if(note>=60) return (note-60)%8;
        switch(note) {case 35:case 36:return 0;case 38:case 40:return 1;case 42:case 44:return 2;case 46:return 3;case 41:case 43:case 45:case 47:case 48:case 50:return 4;case 39:return 5;case 49:case 57:return 6;case 37:case 51:return 7;default:return note%8;}
    }
    void pitchBend(int channel,float semitones) { bends[channelIndex(channel)]=std::clamp(semitones,-2.f,2.f); }
    void sustainPedal(int channel,bool down) {
        pedals[channelIndex(channel)]=down;
        if(!down) for(auto& v:voices) if(v.channel==channel&&!v.keyDown) v.released=true;
    }
    void noteOn(int note,float velocity,int channel=1) {
        Voice* v=nullptr;
        for(auto& candidate:voices) if(!candidate.active) { v=&candidate; break; }
        if(!v) v=&*std::min_element(voices.begin(),voices.end(),[](auto& a,auto& b){return a.age<b.age;});
        *v={}; v->active=true; v->keyDown=true; v->note=note; v->channel=channel;
        v->velocity=std::clamp(velocity,0.f,1.f); v->age=++age;
        v->frequency=440.0*std::pow(2.0,(note-69)/12.0);
        v->stringLength=std::clamp(int(sampleRate/v->frequency),2,int(v->stringBuffer.size()));
        for(int i=0;i<v->stringLength;++i) v->stringBuffer[size_t(i)]=noise()*.6f;
    }
    void noteOff(int note,int channel=1) {
        for(auto& v:voices) if(v.active&&v.note==note&&v.channel==channel) { v.keyDown=false; if(!pedals[channelIndex(channel)]) v.released=true; }
    }
    void allNotesOff(int channel=0) {
        for(auto& v:voices) if(channel==0||v.channel==channel) { v.keyDown=false; v.released=true; }
        if(channel==0) pedals.fill(false); else pedals[channelIndex(channel)]=false;
    }
    float next(const Parameters& target) {
        auto follow=[this](float& v,float t){v+=smoothing*(t-v);};
        follow(current.tone,target.tone); follow(current.color,target.color); follow(current.envelope,target.envelope);
        follow(current.effect,target.effect); follow(current.volume,target.volume); follow(current.drive,target.drive);
        follow(current.delayTime,target.delayTime); follow(current.feedback,target.feedback);
        const auto& p=current;
        lfoPhase+=std::clamp(double(target.lfoRate),.05,20.0)/sampleRate; lfoPhase-=std::floor(lfoPhase);
        const double lfo=target.lfoDepth*(target.lfoShape==1 ? 1-4*std::abs(lfoPhase-.5) : target.lfoShape==2 ? (lfoPhase<.5?1:-1) : std::sin(2*pi*lfoPhase));
        const double pitchMod=target.lfoDestination==0?lfo*2:0;
        float sum=0;
        for(auto& v:voices) {
            if(!v.active) continue;
            const float attack=.001f+target.attack*target.attack*4.f*(.5f+p.envelope);
            const float decay=.005f+target.decay*target.decay*3.f;
            const float release=.008f+target.release*target.release*5.f*(.5f+p.envelope);
            if(v.released) v.level*=float(std::exp(-1.0/(release*sampleRate)));
            else if(v.stage==0) { v.level=std::min(1.f,v.level+float(1.0/(attack*sampleRate))); if(v.level>=1) v.stage=1; }
            else v.level=target.sustain+(v.level-target.sustain)*float(std::exp(-1.0/(decay*sampleRate)));
            if(target.engine!=4&&target.engine!=5&&v.released&&v.level<.0001f) { v.active=false; continue; }
            const double bend=bends[channelIndex(v.channel)]+pitchMod;
            const double frequency=v.frequency*std::pow(2.0,bend/12.0);
            v.phase+=frequency/sampleRate; v.phase-=std::floor(v.phase);
            v.modPhase+=frequency*(1+std::round(p.color*7))/sampleRate; v.modPhase-=std::floor(v.modPhase);
            float s=0, amplitude=v.level;
            if(target.engine==0) s=float(std::sin(2*pi*v.phase+p.tone*7*(.25+.75*v.level)*std::sin(2*pi*v.modPhase)));
            else if(target.engine==1) {
                const double bendPoint=.08+p.tone*.84;
                const double phase=v.phase<bendPoint?.5*v.phase/bendPoint:.5+.5*(v.phase-bendPoint)/(1-bendPoint);
                s=float(std::cos(2*pi*phase)*(1-p.color*.45)+std::sin(4*pi*v.phase)*p.color*.45);
            } else if(target.engine==2) {
                auto a=size_t(v.stringIndex),b=size_t((v.stringIndex+1)%v.stringLength);
                s=v.stringBuffer[a]; v.stringBuffer[a]=(s+v.stringBuffer[b])*(.475f+p.tone*.0245f);
                v.stringIndex=(v.stringIndex+1)%v.stringLength;
                s=s*(1-p.color*.5f)+float(std::sin(2*pi*v.phase))*v.level*p.color*.25f;
            } else if(target.engine==3&&!sample.frames.empty()) {
                const auto start=size_t(p.tone*.9*double(sample.frames.size()-1));
                const auto length=std::max(size_t(1),size_t((.01+p.color*.99)*double(sample.frames.size()-start)));
                if(v.samplePosition>=double(length)) {
                    if(target.sampleLoop&&!v.released) v.samplePosition=std::fmod(v.samplePosition,double(length));
                    else { v.active=false; continue; }
                }
                const double position=double(start)+v.samplePosition;
                const auto a=std::min(size_t(position),sample.frames.size()-1),b=std::min(a+1,sample.frames.size()-1);
                const float fraction=float(position-std::floor(position));
                s=sample.frames[a]+fraction*(sample.frames[b]-sample.frames[a]);
                v.samplePosition+=sample.rate/sampleRate*std::pow(2.0,(v.note-60+bend)/12.0);
            } else if(target.engine==5) {
                const auto& hit=kit[size_t(kitSlot(v.note))];
                const double length=hit.frames.size()*std::clamp(double(p.color),.05,1.0);
                if(hit.frames.empty()||v.samplePosition>=length) {v.active=false;continue;}
                const auto a=std::min(size_t(v.samplePosition),hit.frames.size()-1),b=std::min(a+1,hit.frames.size()-1);
                const float fraction=float(v.samplePosition-std::floor(v.samplePosition));
                s=hit.frames[a]+fraction*(hit.frames[b]-hit.frames[a]);
                v.samplePosition+=hit.rate/sampleRate*std::pow(2.0,((p.tone-.5)*24+bend)/12.0);
                amplitude=1; // Kit samples are one-shots; note-off does not truncate hits.
            } else if(target.engine==4) {
                const int kind=(v.note%12==0||v.note%12==7)?0:(v.note%12==2||v.note%12==4)?1:2;
                const double duration=kind==0?.08+p.envelope*.7:kind==1?.04+p.envelope*.4:.015+p.envelope*.16;
                amplitude=float(std::exp(-v.time/duration));
                const float n=noise(),high=n-v.lastNoise; v.lastNoise=n;
                if(kind==0) { v.drumPhase+=(35+110*p.tone+150*std::exp(-v.time*35))/sampleRate; s=float(std::sin(2*pi*v.drumPhase))*(1-p.color*.35f)+high*p.color*.15f; }
                else if(kind==1) s=float(std::sin(2*pi*v.time*(150+p.tone*180)))*.35f+n*(.35f+p.color*.5f);
                else s=high*(.3f+p.color*.5f);
                if(amplitude<.0001f) v.active=false;
            }
            v.time+=1/sampleRate;
            sum+=s*amplitude*v.velocity*.16f;
        }
        const double cutoff=(900+p.tone*14500)*std::pow(2.0,target.lfoDestination==1?lfo*3:0);
        const float coefficient=float(1-std::exp(-2*pi*std::min(cutoff,sampleRate*.42)/sampleRate));
        filter+=coefficient*(sum-filter);
        const float driven=std::tanh(filter*(1+p.drive*12))/(1+p.drive*2);
        const auto lag=std::clamp(size_t(sampleRate*p.delayTime),size_t(1),delay.size()-1);
        const float echo=delay[(write+delay.size()-lag)%delay.size()];
        delay[write]=std::tanh(driven+echo*std::clamp(p.feedback,0.f,.85f)); write=(write+1)%delay.size();
        return std::tanh((driven+echo*p.effect*.7f)*p.volume);
    }
private:
    static size_t channelIndex(int ch) { return size_t(std::clamp(ch,1,17)-1); }
    struct Voice {
        bool active=false,released=false,keyDown=false; int note=0,channel=1,stage=0,stringLength=2,stringIndex=0;
        uint64_t age=0; float velocity=0,level=0,lastNoise=0;
        double phase=0,modPhase=0,frequency=440,samplePosition=0,time=0,drumPhase=0;
        std::array<float,8192> stringBuffer{};
    };
    float noise() { random^=random<<13; random^=random>>17; random^=random<<5; return float(double(random)/4294967295.0*2-1); }
    double sampleRate=48000,lfoPhase=0; float smoothing=.0017f; Parameters current;
    std::array<Voice,polyphony> voices{}; std::array<float,192000> delay{};
    std::array<float,17> bends{}; std::array<bool,17> pedals{};
    Sample sample; std::array<Sample,8> kit;
    size_t write=0; uint64_t age=0; uint32_t random=0x12345678; float filter=0;
};
}
