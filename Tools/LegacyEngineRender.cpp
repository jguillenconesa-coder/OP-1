// SPDX-License-Identifier: AGPL-3.0-only
// Differential harness: compile this SAME file against baseline/current Source
// directories. Writes native float samples; compare on the same host/compiler.
#include "Engine.h"
#include "Phase246Presets.h"
#include "Phase246Envelopes.h"
#include "Digital246Presets.h"
#include <cstdio>
#include <memory>
int main(){
    auto engine=std::make_unique<pocket::Engine>();pocket::Sample wave;wave.rate=44100;
    for(int i=0;i<4096;++i)wave.frames.push_back(float(std::sin(i*.11)*.4));
    engine->setSample(wave);pocket::Kit kit;for(auto& hit:kit)hit=wave;engine->setKit(std::move(kit),24);
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<70;++mode){
        engine->prepare(rate);pocket::Parameters p;p.engine=mode<6?mode:1;
        p.sampleClean=mode==3||mode==5;p.effect=.4;p.drive=.2;p.sustain=.7;p.attack=.02;p.release=.02;
        if(mode>=6&&mode<38){const auto index=size_t((mode-6)/2);p.phase246=true;p.phase246Envelope=(mode-6)%2;
            p.phase246Controls=pocket::phase246Presets[index].controls;p.phase246Adsr=pocket::phase246Envelopes[index];}
        if(mode>=38){const auto index=size_t((mode-38)/2);p.engine=0;p.digital246=true;p.digital246Envelope=(mode-38)%2;
            p.digital246Controls=pocket::digital246Presets[index].controls;p.digital246Adsr=pocket::digital246Presets[index].adsr;}
        for(int note:{48,60,67})engine->noteOn(note,.7f);
        for(int i=0;i<8192;++i){
            if(i==1000)engine->pitchBend(1,1.2f);
            if(i==3000){p.lfoDestination=1;p.lfoDepth=.3f;}
            if(i==5000)engine->allNotesOff();
            const auto sample=engine->next(p);if(std::fwrite(&sample,sizeof(sample),1,stdout)!=1)return 2;
        }
    }
}
