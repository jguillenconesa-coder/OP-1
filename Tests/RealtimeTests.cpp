// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>
namespace {thread_local bool tracking=false;thread_local size_t allocations=0,frees=0;}
void* operator new(std::size_t size) {if(tracking)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* p) noexcept {if(tracking&&p)++frees;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
#if POCKET_TRACK_C_HEAP
extern "C" {
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void* __real_realloc(void*,size_t);void __real_free(void*);
void* __wrap_malloc(size_t size){if(tracking)++allocations;return __real_malloc(size);}
void* __wrap_calloc(size_t n,size_t size){if(tracking)++allocations;return __real_calloc(n,size);}
void* __wrap_realloc(void* p,size_t size){if(tracking)++allocations;return __real_realloc(p,size);}
void __wrap_free(void* p){if(tracking&&p)++frees;__real_free(p);}
}
#endif
int main(int argc,char** argv) {
 try {
  juce::ScopedJuceInitialiser_GUI gui;
  auto p=std::make_unique<PocketProcessor>();
  if(p->getLatencySamples()!=0)throw std::runtime_error("Plugin reports unexpected processing latency");
  juce::Array<juce::var> measurements;
  const int presets[]={0,2,3,18,4,22};
  for(double rate:{48000.,96000.}) for(int block:{64,128,256}) for(int preset:presets) {
   p->applyPreset(preset);p->prepareToPlay(rate,block);
   for(auto item:{std::pair{"tapePlay",1.f},{"tapeRecord",1.f},{"tapeOverdub",1.f},{"lfoDepth",.35f},{"drive",.7f},{"effect",.7f},{"sampleLoop",1.f},{"metronome",1.f},{"masterDrive",.6f},{"masterEcho",.3f},{"eqLow",.25f}}) p->state.getParameter(item.first)->setValueNotifyingHost(item.second);
   juce::String captureError;if(!p->startInputCapture(captureError))throw std::runtime_error(captureError.toStdString());
   juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(16384);
   for(int note:{48,52,55,59,60,64,67,72})midi.addEvent(juce::MidiMessage::noteOn(1,note,.8f),0);
   p->processBlock(audio,midi);midi.clear();for(int i=0;i<64;++i)p->processBlock(audio,midi);
   std::vector<double> times;times.reserve(512);allocations=frees=0;double peak=0;
   for(int iteration=0;iteration<512;++iteration) {
    if(iteration%64==0) {midi.addEvent(juce::MidiMessage::pitchWheel(1,8192+(iteration%128?1000:0)),0);for(int note:{48,52,55,59,60,64,67,72})midi.addEvent(juce::MidiMessage::noteOn(1,note,.8f),0);}
    if(iteration%128==0){const std::array<uint8_t,128> sysex{};midi.addEvent(juce::MidiMessage::createSysExMessage(sysex.data(),int(sysex.size())),0);}
    if(iteration%100==0)p->keyboard.noteOn(2,65,.6f);
    if(iteration%100==50)p->keyboard.noteOff(2,65,0);
    const auto start=std::chrono::steady_clock::now();tracking=true;p->processBlock(audio,midi);tracking=false;
    const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();times.push_back(elapsed);midi.clear();
    for(int channel=0;channel<2;++channel)for(int i=0;i<block;++i){const auto sample=audio.getSample(channel,i);if(!std::isfinite(sample))throw std::runtime_error("Nonfinite realtime output");peak=std::max(peak,double(std::abs(sample)));}
   }
   if(allocations||frees)throw std::runtime_error("Heap activity in processBlock: allocations="+std::to_string(allocations)+", frees="+std::to_string(frees));
   if(peak==0)throw std::runtime_error("Realtime workload is silent");
   std::sort(times.begin(),times.end());const double deadline=block/rate*1000000;
   auto result=std::make_unique<juce::DynamicObject>();result->setProperty("preset",preset);result->setProperty("rate",rate);result->setProperty("block",block);result->setProperty("deadlineUs",deadline);result->setProperty("medianUs",times[times.size()/2]);result->setProperty("p99Us",times[size_t(times.size()*.99)]);result->setProperty("maxUs",times.back());result->setProperty("deadlineOverruns",int(std::count_if(times.begin(),times.end(),[&](double value){return value>deadline;})));result->setProperty("heapCalls",0);measurements.add(juce::var(result.release()));
  }
  if(argc>1) {juce::File output(juce::String::fromUTF8(argv[1]));if(!output.replaceWithText(juce::JSON::toString(juce::var(measurements))))throw std::runtime_error("Cannot write realtime metrics");}
  std::cout<<"PASS: 36 realtime workloads, 8 voices + MIDI/GUI/SysEx + tape/master FX/EQ/LFO + input capture; no tracked callback heap calls; reported latency 0 samples\n";
  // Timing remains diagnostic: OS scheduling on shared CI cannot certify a DAW deadline.
 }catch(const std::exception& error){tracking=false;std::cerr<<error.what()<<'\n';return 1;}
}
