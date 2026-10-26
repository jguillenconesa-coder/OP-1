// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
 try {
  auto e=std::make_unique<pocket::Engine>(); pocket::Parameters p; p.effect=0;
  for(double sr : {44100.,48000.,96000.}) for(int mode=0;mode<3;++mode) {
   e->prepare(sr); p.engine=mode;
   for(int i=0;i<1024;++i) check(e->next(p)==0,"Idle engine must be silent");
   e->noteOn(69,.8f); double energy=0;
   for(int i=0;i<int(sr);++i) { float s=e->next(p); check(std::isfinite(s)&&std::abs(s)<=1,"Nonfinite or unbounded output"); energy+=s*s; }
   check(energy>0.01,"Engine generated no audio"); e->noteOff(69);
   for(int i=0;i<int(sr*8);++i) e->next(p);
   check(std::abs(e->next(p))<.00001,"Released voice stuck");
  }
  e->prepare(48000); p.engine=0; p.tone=0; p.color=0; p.envelope=0; e->noteOn(69,1);
  int crossings=0; float last=0;
  for(int i=0;i<48000;++i) { auto s=e->next(p); if(i>=4800 && s>0&&last<=0) ++crossings; last=s; }
  check(std::abs(crossings/0.9-440)<2,"A4 pitch is incorrect");
  for(int mode=0;mode<3;++mode) for(int extreme=0;extreme<2;++extreme) {
   e->prepare(48000); p.engine=mode; p.tone=p.color=p.envelope=p.effect=p.volume=float(extreme);
   for(int n=0;n<128;++n) e->noteOn(n,1);
   for(int i=0;i<48000;++i) check(std::isfinite(e->next(p)),"Extreme parameter instability");
   e->allNotesOff();
  }
  // Sampler and drums exercise the newly added signal paths independently.
  pocket::Sample sample; sample.rate=48000; sample.frames.resize(4800);
  for(size_t i=0;i<sample.frames.size();++i) sample.frames[i]=float(std::sin(2*pocket::pi*440*double(i)/48000));
  e->prepare(48000); e->setSample(std::move(sample)); p={}; p.engine=3; p.tone=0; p.color=1; p.sampleLoop=true;
  for(int i=0;i<10000;++i) e->next(p);
  e->noteOn(60,1); double samplerEnergy=0;
  for(int i=0;i<24000;++i){const auto v=e->next(p); check(std::isfinite(v),"Sampler output invalid"); if(i>12000) samplerEnergy+=v*v;}
  check(samplerEnergy>1,"Sampler loop did not continue past sample end");
  e->noteOff(60); for(int i=0;i<480000;++i) e->next(p); check(std::abs(e->next(p))<.0001,"Sampler did not release");
  e->prepare(48000); p={}; p.engine=4; p.effect=0;
  for(int note:{60,62,66}) {e->noteOn(note,1); double energy=0; for(int i=0;i<24000;++i){const auto v=e->next(p); check(std::isfinite(v),"Drum invalid");energy+=v*v;} check(energy>.01,"Drum produced no sound");}
  e->prepare(48000); p={}; p.effect=0; e->noteOn(69,1); e->sustainPedal(1,true); e->noteOff(69);
  for(int i=0;i<48000;++i) e->next(p);
  double pedalEnergy=0; for(int i=0;i<4800;++i) {const auto v=e->next(p);pedalEnergy+=v*v;}
  check(pedalEnergy>1,"Sustain pedal did not hold note"); e->sustainPedal(1,false);
  for(int i=0;i<480000;++i) e->next(p); check(std::abs(e->next(p))<.0001,"Sustain release stuck");
  e->prepare(48000); p={}; p.tone=0;p.color=0;p.effect=0; e->pitchBend(1,2);e->noteOn(69,1);
  int bentCrossings=0;float previous=0;for(int i=0;i<48000;++i){const auto v=e->next(p);if(i>=4800&&v>0&&previous<=0)++bentCrossings;previous=v;}
  check(std::abs(bentCrossings/.9-493.883)<3,"Pitch bend did not shift by two semitones");
  // Twenty-four independent kit voices use trim, reverse, gain and pitch.
  {
   pocket::Kit kit;for(int pad=0;pad<24;++pad){kit[size_t(pad)].rate=48000;kit[size_t(pad)].frames.assign(4800,.5f);}
   kit[23].frames.assign(4800,0);for(int i=0;i<1200;++i)kit[23].frames[size_t(i)]=.5f;
   e->setKit(std::move(kit),24);p={};p.engine=5;p.tone=.5f;p.color=1;p.effect=0;
   check(pocket::Engine::kitSlot(83,24)==23&&pocket::Engine::kitSlot(68,8)==0,"Kit MIDI mapping compatibility failed");
   auto energy=[&](int note,int frames){e->reset();for(int i=0;i<20000;++i)e->next(p);e->noteOn(note,1);double sum=0;for(int i=0;i<frames;++i){const auto sample=e->next(p);sum+=sample*sample;}return sum;};
   check(energy(83,500)>0.01,"Pad 24 is not reachable");p.pads[23].reverse=true;check(energy(83,500)<.000001,"Reverse pad did not start from its tail");
   p.pads[23].reverse=false;p.pads[23].start=.5f;check(energy(83,500)<.000001,"Pad start trim ignored");p.pads[23].start=0;p.pads[23].gain=0;check(energy(83,500)<.000001,"Per-pad gain did not mute");
   p.pads[23].gain=1;p.pads[23].end=0;check(energy(83,500)<.000001,"Empty pad trim is not silent");
   p.pads[23].end=1;p.pads[23].pitch=12;const auto pitched=energy(83,2000);p.pads[23].pitch=0;check(energy(83,2000)>pitched*1.4,"Pad pitch does not change sample playback speed");
   p.pads[0].gated=true;p.release=0;e->reset();e->noteOn(60,1);for(int i=0;i<300;++i)e->next(p);e->noteOff(60);for(int i=0;i<4400;++i)e->next(p);check(e->visualState(5).active==0,"Gated pad voice stuck after release");
  }
  {pocket::Kit kit;kit[0].rate=48000;kit[0].frames.resize(2048);for(int i=0;i<2048;++i)kit[0].frames[size_t(i)]=float(std::sin(2*pocket::pi*8000*i/48000))*.7f;
   const auto reference=kit[0].frames;e->setKit(std::move(kit),24);e->reset();p={};p.engine=5;p.tone=.5f;p.color=1;p.effect=0;p.volume=1;p.sampleClean=true;
   e->noteOn(60,1);double error=0;for(int i=0;i<1024;++i){const double difference=e->next(p)-reference[size_t(i)]*.16;error+=difference*difference;}
   check(std::sqrt(error/1024)<.00001,"Clean sample path changes the dry PCM waveform");
  }
  if(argc>1) {
   e->prepare(48000); p={}; std::vector<int16_t> pcm;
   for(int section=0;section<3;++section) { p.engine=section; e->noteOn(60, .8f); e->noteOn(67,.7f); e->noteOn(75,.6f);
    for(int i=0;i<96000;++i) { if(i==60000) e->allNotesOff(); pcm.push_back(int16_t(e->next(p)*30000)); }
   }
   std::ofstream out(argv[1],std::ios::binary); auto put=[&](uint32_t n,int bytes) { for(int i=0;i<bytes;++i) out.put(char((n>>(8*i))&255)); };
   out.write("RIFF",4); put(36+uint32_t(pcm.size()*2),4); out.write("WAVEfmt ",8); put(16,4); put(1,2); put(1,2); put(48000,4); put(96000,4); put(2,2); put(16,2); out.write("data",4); put(uint32_t(pcm.size()*2),4); for(auto s:pcm) put(uint16_t(s),2);
   check(bool(out),"Could not write demo audio");
  }
  std::cout<<"PASS: 3 engines x 3 rates; silence, audio, release, pitch, voice stealing, parameter extremes, sampler loop/release, drums, sustain and pitch bend\n";
 } catch(const std::exception& ex) { std::cerr<<ex.what()<<'\n'; return 1; }
}
