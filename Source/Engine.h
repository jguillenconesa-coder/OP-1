// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pocket {
constexpr double pi = 3.14159265358979323846;
constexpr int kitCapacity=24;
struct PadSettings {float start=0,end=1,gain=1,pitch=0;bool reverse=false,gated=false;};
struct Parameters {
    int engine=0;
    std::array<PadSettings,kitCapacity> pads{};
    float tone=.45f,color=.35f,envelope=.35f,effect=.2f,volume=.7f;
    float attack=.02f,decay=.2f,sustain=.8f,release=.3f;
    float lfoRate=1,lfoDepth=0; int lfoDestination=0,lfoShape=0;
    float delayTime=.281f,feedback=.42f,drive=0;
    bool sampleClean=false;bool sampleLoop=false; float sampleRoot=60;
};
struct Sample { std::vector<float> frames; double rate=48000; };
using Kit=std::array<Sample,kitCapacity>;
class Engine {
public:
    static constexpr int polyphony=8;
    void prepare(double rate) { sampleRate=std::max(8000.0,rate); smoothing=float(1-std::exp(-1.0/(sampleRate*.012))); cachedCutoff=-1;reset(); }
    void reset() { voices={}; delay.fill(0); write=0; filter=0; current={}; lfoPhase=0; bends.fill(1); pedals.fill(false); cachedAttack=cachedDecay=cachedRelease=cachedMacro=-1;cleanMix=0;cleanInitialized=false; }
    void setSample(Sample data) { sample=std::move(data); voices={}; }
    void swapSample(Sample& data) {std::swap(sample,data);voices={};}
    void swapKit(Kit& data,int count=8) {kit.swap(data);kitCount=count==24?24:8;voices={};}
    void swapKitPad(int pad,Sample& data){if(pad>=0&&pad<kitCapacity){std::swap(kit[size_t(pad)],data);if(pad>=kitCount)kitCount=24;voices={};}}
    int getKitCount() const{return kitCount;}
    const Sample& getSample() const { return sample; }
    void setKit(Kit data,int count=8) {kit=std::move(data);kitCount=count==24?24:8;voices={};}
    const Kit& getKit() const { return kit; }
    static int kitSlot(int note,int count=8) {
        if(note>=60) return (note-60)%(count==24?24:8);
        switch(note) {case 35:case 36:return 0;case 38:case 40:return 1;case 42:case 44:return 2;case 46:return 3;case 41:case 43:case 45:case 47:case 48:case 50:return 4;case 39:return 5;case 49:case 57:return 6;case 37:case 51:return 7;default:return note%8;}
    }
    void pitchBend(int channel,float semitones) { bends[channelIndex(channel)]=std::exp2(std::clamp(semitones,-2.f,2.f)/12.f); }
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
        v->stringLength=std::clamp(int(sampleRate/v->frequency),2,8192);
    }
    void noteOff(int note,int channel=1) {
        for(auto& v:voices) if(v.active&&v.note==note&&v.channel==channel) { v.keyDown=false; if(!pedals[channelIndex(channel)]) v.released=true; }
    }
    void allNotesOff(int channel=0) {
        for(auto& v:voices) if(channel==0||v.channel==channel) { v.keyDown=false; v.released=true; }
        if(channel==0) pedals.fill(false); else pedals[channelIndex(channel)]=false;
    }
    struct VisualState {float envelope=0,progress=0,lfo=0;int active=0,stage=0;uint64_t trigger=0;};
    VisualState visualState(int mode) const noexcept {
        VisualState out;out.trigger=age;out.lfo=float(lfoPhase);uint64_t latest=0;
        for(const auto& v:voices) if(v.active) {
            ++out.active;out.envelope=std::max(out.envelope,v.level);
            if(v.age>=latest) {latest=v.age;out.stage=v.released?4:v.stage==0?1:std::abs(v.level-currentSustain)>.005f?2:3;
                const size_t length=mode==5?kit[size_t(kitSlot(v.note,kitCount))].frames.size():sample.frames.size();
                if(length) out.progress=std::clamp(float(v.samplePosition/double(length))+(mode==3?current.tone*.9f:0.f),0.f,1.f);
            }
        }
        return out;
    }
    float next(const Parameters& target) {
        if(!cleanInitialized){const bool clean=target.sampleClean&&(target.engine==3||target.engine==5);cleanMix=clean?1.f:0.f;if(clean)current=target;cleanInitialized=true;}
        auto follow=[this,&target](float& v,float t){v+=smoothing*(t-v);if(target.sampleClean&&std::abs(v-t)<.00002f)v=t;};
        follow(current.tone,target.tone); follow(current.color,target.color); follow(current.envelope,target.envelope);
        follow(current.effect,target.effect); follow(current.volume,target.volume); follow(current.drive,target.drive);
        follow(current.delayTime,target.delayTime); follow(current.feedback,target.feedback);
        const auto& p=current;currentSustain=target.sustain;
        lfoPhase+=std::clamp(double(target.lfoRate),.05,20.0)/sampleRate; lfoPhase-=std::floor(lfoPhase);
        const double lfo=target.lfoDepth*(target.lfoShape==1 ? 1-4*std::abs(lfoPhase-.5) : target.lfoShape==2 ? (lfoPhase<.5?1:-1) : std::sin(2*pi*lfoPhase));
        const double pitchMod=target.lfoDestination==0?lfo*2:0;
        const double pitchFactor=pitchMod==0?1:std::exp2(pitchMod/12);
        const double kitPitch=target.engine==5?std::exp2((p.tone-.5)*2):1;
        if(cachedAttack!=target.attack||cachedMacro!=p.envelope) attackStep=float(1/(sampleRate*(.001+target.attack*target.attack*4*(.5+p.envelope))));
        if(cachedDecay!=target.decay) decayFactor=float(std::exp(-1/(sampleRate*(.005+target.decay*target.decay*3))));
        if(cachedRelease!=target.release||cachedMacro!=p.envelope) releaseFactor=float(std::exp(-1/(sampleRate*(.008+target.release*target.release*5*(.5+p.envelope)))));
        cachedAttack=target.attack;cachedDecay=target.decay;cachedRelease=target.release;cachedMacro=p.envelope;
        if(cachedRoot!=target.sampleRoot) {cachedRoot=target.sampleRoot;rootFrequency=440*std::exp2((cachedRoot-69)/12);}

        if(target.engine==5)for(size_t i=0;i<padPitch.size();++i)if(target.pads[i].pitch!=cachedPadPitch[i]){cachedPadPitch[i]=target.pads[i].pitch;padPitch[i]=std::exp2(std::clamp(double(target.pads[i].pitch),-24.,24.)/12);}
        float sum=0;
        for(auto& v:voices) {
            if(!v.active) continue;
            if(v.released) v.level*=releaseFactor;
            else if(v.stage==0) {v.level=std::min(1.f,v.level+attackStep);if(v.level>=1) v.stage=1;}
            else v.level=target.sustain+(v.level-target.sustain)*decayFactor;
            if(target.engine!=4&&target.engine!=5&&v.released&&v.level<.0001f) { v.active=false; continue; }
            const double bend=bends[channelIndex(v.channel)]*pitchFactor;
            const double frequency=v.frequency*bend;
            v.phase+=frequency/sampleRate; v.phase-=std::floor(v.phase);
            v.modPhase+=frequency*(1+std::round(p.color*7))/sampleRate; v.modPhase-=std::floor(v.modPhase);
            float s=0, amplitude=v.level;
            if(target.engine==0) s=float(std::sin(2*pi*v.phase+p.tone*7*(.25+.75*v.level)*std::sin(2*pi*v.modPhase)));
            else if(target.engine==1) {
                const double bendPoint=.08+p.tone*.84;
                const double phase=v.phase<bendPoint?.5*v.phase/bendPoint:.5+.5*(v.phase-bendPoint)/(1-bendPoint);
                s=float(std::cos(2*pi*phase)*(1-p.color*.45)+std::sin(4*pi*v.phase)*p.color*.45);
            } else if(target.engine==2) {
                auto& stringBuffer=stringBuffers[size_t(&v-voices.data())];
                if(!v.stringReady) {for(int i=0;i<v.stringLength;++i) stringBuffer[size_t(i)]=noise()*.6f;v.stringReady=true;}
                auto a=size_t(v.stringIndex),b=size_t((v.stringIndex+1)%v.stringLength);
                s=stringBuffer[a]; stringBuffer[a]=(s+stringBuffer[b])*(.475f+p.tone*.0245f);
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
                v.samplePosition+=sample.rate/sampleRate*v.frequency/rootFrequency*bend;
            } else if(target.engine==5) {
                const auto slot=size_t(kitSlot(v.note,kitCount));const auto& hit=kit[slot];const auto& pad=target.pads[slot];
                if(hit.frames.empty()){v.active=false;continue;}
                const double start=std::clamp(double(pad.start),0.,1.)*double(hit.frames.size());
                const double end=std::clamp(double(pad.end),0.,1.)*double(hit.frames.size());
                const double length=std::max(0.,end-start)*std::clamp(double(p.color),.05,1.0);
                if(length<1||v.samplePosition>=length||(pad.gated&&v.released&&v.level<.0001f)){v.active=false;continue;}
                const double position=std::clamp(pad.reverse?start+length-1-v.samplePosition:start+v.samplePosition,0.,double(hit.frames.size()-1));
                const auto a=size_t(position),b=std::min(a+1,hit.frames.size()-1);
                const float fraction=float(position-double(a));
                s=(hit.frames[a]+fraction*(hit.frames[b]-hit.frames[a]))*pad.gain;
                v.samplePosition+=hit.rate/sampleRate*kitPitch*bend*padPitch[slot];
                amplitude=pad.gated?v.level:1; // One-shots retain legacy note-off behavior.
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
        const double cutoff=(900+p.tone*14500)*(target.lfoDestination==1&&lfo!=0?std::exp2(lfo*3):1);
        if(cutoff!=cachedCutoff) {cachedCutoff=cutoff;filterCoefficient=float(1-std::exp(-2*pi*std::min(cutoff,sampleRate*.42)/sampleRate));}
        const float coefficient=filterCoefficient;
        filter+=coefficient*(sum-filter);
        follow(cleanMix,target.sampleClean&&(target.engine==3||target.engine==5)?1.f:0.f);
        const float legacyDriven=std::tanh(filter*(1+p.drive*12))/(1+p.drive*2);
        const float cleanDriven=p.drive<.000001f?sum:std::tanh(sum*(1+p.drive*12))/(1+p.drive*2);
        const float driven=legacyDriven+(cleanDriven-legacyDriven)*cleanMix;
        const auto lag=std::clamp(size_t(sampleRate*p.delayTime),size_t(1),delay.size()-1);
        const float echo=delay[(write+delay.size()-lag)%delay.size()];
        delay[write]=std::tanh(driven+echo*std::clamp(p.feedback,0.f,.85f)); write=(write+1)%delay.size();
        const float output=(driven+echo*p.effect*.7f)*p.volume,limited=std::tanh(output);
        return limited+(output-limited)*cleanMix;
    }
private:
    static size_t channelIndex(int ch) { return size_t(std::clamp(ch,1,17)-1); }
    struct Voice {
        bool active=false,released=false,keyDown=false,stringReady=false; int note=0,channel=1,stage=0,stringLength=2,stringIndex=0;
        uint64_t age=0; float velocity=0,level=0,lastNoise=0;
        double phase=0,modPhase=0,frequency=440,samplePosition=0,time=0,drumPhase=0;
    };
    float noise() { random^=random<<13; random^=random>>17; random^=random<<5; return float(double(random)/4294967295.0*2-1); }
    bool cleanInitialized=false;float cleanMix=0,currentSustain=.8f;
    float cachedAttack=-1,cachedDecay=-1,cachedRelease=-1,cachedMacro=-1,attackStep=0,decayFactor=0,releaseFactor=0;
    double cachedRoot=-1,rootFrequency=261.625565,cachedCutoff=-1;float filterCoefficient=0;
    double sampleRate=48000,lfoPhase=0; float smoothing=.0017f; Parameters current;
    std::array<std::array<float,8192>,polyphony> stringBuffers{};
    std::array<Voice,polyphony> voices{}; std::array<float,192000> delay{};
    std::array<float,17> bends{}; std::array<bool,17> pedals{};
    Sample sample;Kit kit;int kitCount=8;
    std::array<float,kitCapacity> cachedPadPitch{};
    std::array<double,kitCapacity> padPitch{{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};
    size_t write=0; uint64_t age=0; uint32_t random=0x12345678; float filter=0;
};
}
