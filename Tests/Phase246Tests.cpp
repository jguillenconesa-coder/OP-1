// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include "Phase246Presets.h"
#include "Phase246Envelopes.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    check(argc==2,"Expected reference vector directory");pocket::Phase246Config verified;verified.prepare(48000);
    std::ifstream table(std::string(argv[1])+"/verified-sine.bin",std::ios::binary);check(bool(table),"Missing verified sine table");
    for(size_t i=0;i<verified.table.size();++i){const auto low=table.get(),high=table.get();check(low>=0&&high>=0,"Truncated reference table");const int word=low+(high<<8);check(verified.table[i]==(word<32768?word:word-65536),"Runtime Phase LUT differs from verified firmware source table");}
    // Simple core settings provide a known fundamental. Clock adaptation must
    // retain tuning across both upsampling and noninteger downsampling ratios.
    const pocket::reference::PdCoefficients controls({0,0,0,0});
    for(double rate:{8000.,22050.,44100.,48000.,88200.,96000.,192000.}){
        pocket::Phase246Config config;config.prepare(rate);pocket::Phase246Voice voice;
        int crossings=0;float previous=0;double energy=0;
        for(int i=0;i<int(rate);++i){const float sample=voice.next(220,controls,config);check(std::isfinite(sample)&&std::abs(sample)<1.1f,"Native Phase unstable at host rate");if(i>rate*.1&&sample>0&&previous<=0)++crossings;previous=sample;energy+=sample*sample;}
        check(std::abs(crossings/.9-220)<2,"Native Phase changed tuning with host rate");check(energy>1,"Native Phase clock adapter is silent");
    }
    auto engine=std::make_unique<pocket::Engine>();
    for(double rate:{44100.,48000.,96000.})for(const auto& patch:pocket::phase246Presets){
        engine->prepare(rate);pocket::Parameters p;p.engine=1;p.phase246=true;p.phase246Controls=patch.controls;p.effect=0;p.attack=0;p.sustain=1;p.release=0;
        engine->noteOn(60,.8f);double energy=0;for(int i=0;i<int(rate/10);++i){const float sample=engine->next(p);check(std::isfinite(sample)&&std::abs(sample)<1.1f,"Native Phase preset is unstable");energy+=sample*sample;}check(energy>.001,"Native Phase preset is silent");
        engine->noteOff(60);for(int i=0;i<int(rate/2);++i)engine->next(p);check(engine->visualState(1).active==0,"Native Phase release stuck");
    }
    for(double rate:{44100.,48000.,96000.})for(size_t patch=0;patch<pocket::phase246Presets.size();++patch){
        engine->prepare(rate);pocket::Parameters p;p.engine=1;p.phase246=true;p.phase246Envelope=true;p.phase246Controls=pocket::phase246Presets[patch].controls;p.phase246Adsr=pocket::phase246Envelopes[patch];p.effect=0;
        engine->noteOn(60,.8f);double energy=0;for(int i=0;i<int(rate/4);++i){const float sample=engine->next(p);check(std::isfinite(sample)&&std::abs(sample)<1.1f,"Original-envelope Phase preset is unstable");energy+=sample*sample;}check(energy>.000001,"Original-envelope Phase preset is silent");
        p.phase246Adsr[3]=16383;engine->sustainPedal(1,true);engine->noteOff(60);for(int i=0;i<int(rate/50);++i)engine->next(p);
        // Some zero-sustain percussive presets naturally finish while pedal is held.
        engine->sustainPedal(1,false);for(int i=0;i<int(rate/5);++i)engine->next(p);check(engine->visualState(1).active==0,"Original-envelope Phase note stuck after pedal release");
    }
    engine->prepare(48000);pocket::Parameters p;p.engine=1;p.phase246=true;p.phase246Controls={0,0,0,0};p.effect=0;p.sustain=1;p.attack=0;
    engine->pitchBend(1,2);engine->noteOn(69,1);int crossings=0;float previous=0;
    for(int i=0;i<48000;++i){const float sample=engine->next(p);if(i>4800&&sample>0&&previous<=0)++crossings;previous=sample;}check(std::abs(crossings/.9-493.883)<2,"Native Phase pitch bend incorrect");
    std::cout<<"PASS: native Phase tuning at 7 host rates, all 16 CORE presets with both envelopes at 3 rates, finite output, release, pedal and pitch bend\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
