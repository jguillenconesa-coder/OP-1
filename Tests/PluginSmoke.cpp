// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include <iostream>
#include <memory>
#include <stdexcept>
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
 try {
  juce::ScopedJuceInitialiser_GUI init;
  auto p=std::make_unique<PocketProcessor>(); p->prepareToPlay(48000,256);
  juce::AudioBuffer<float> buffer(2,256); juce::MidiBuffer midi;
  // A timestamped note must not sound before its sample offset.
  midi.addEvent(juce::MidiMessage::noteOn(1,69,.8f),128); p->processBlock(buffer,midi);
  for(int i=0;i<128;++i) check(buffer.getSample(0,i)==0,"MIDI was not sample accurate");
  double energy=0;
  for(int b=0;b<100;++b) { midi.clear(); p->processBlock(buffer,midi); for(int i=0;i<256;++i) { auto v=buffer.getSample(0,i); check(std::isfinite(v),"Invalid plugin output"); check(v==buffer.getSample(1,i),"Stereo channel mismatch"); energy+=v*v; } }
  check(energy>1,"Plugin MIDI input produced no sound");
  midi.addEvent(juce::MidiMessage::allNotesOff(1),0); p->processBlock(buffer,midi);
  p->state.getParameter("tone")->setValueNotifyingHost(.82f); p->steps[0].store(75);
  juce::MemoryBlock saved; p->getStateInformation(saved);
  auto restored=std::make_unique<PocketProcessor>(); restored->setStateInformation(saved.getData(),int(saved.getSize()));
  check(std::abs(restored->state.getRawParameterValue("tone")->load()-.82f)<.001,"Parameter state restoration failed");
  check(restored->steps[0].load()==75,"Sequence state restoration failed");
  restored->prepareToPlay(48000,256); restored->state.getParameter("sequencer")->setValueNotifyingHost(1);
  midi.clear(); restored->processBlock(buffer,midi); check(restored->playhead.load()==0,"Sequencer failed to start");
  check(buffer.getMagnitude(0,0,256)>0,"Sequencer produced no audio");
  restored->state.getParameter("sequencer")->setValueNotifyingHost(0); restored->processBlock(buffer,midi);
  check(restored->playhead.load()==-1,"Sequencer failed to stop");
  // Generate a short sample, import it, then remove its file. The saved state
  // must remain portable without referring to the original disk location.
  auto sampleFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pocket-sample",".wav",false);
  {
   juce::AudioBuffer<float> source(1,4800); for(int i=0;i<4800;++i) source.setSample(0,i,float(std::sin(2*pocket::pi*220*i/48000))*.7f);
   auto stream=sampleFile.createOutputStream(); juce::WavAudioFormat wav;
   std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),48000,1,24,{},0));
   check(bool(writer),"Sample fixture writer failed");stream.release();check(writer->writeFromAudioSampleBuffer(source,0,4800),"Sample fixture write failed");
  }
  juce::String error; check(p->loadSample(sampleFile,error),"Sample import failed"); sampleFile.deleteFile();
  p->state.getParameter("sampleLoop")->setValueNotifyingHost(1);
  p->state.getParameter("tapePlay")->setValueNotifyingHost(1);p->state.getParameter("tapeRecord")->setValueNotifyingHost(1);
  midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
  for(int b=0;b<100;++b){p->processBlock(buffer,midi);midi.clear();}
  p->state.getParameter("tapeRecord")->setValueNotifyingHost(0);p->requestPanic();p->rewindTape();
  p->processBlock(buffer,midi);check(buffer.getMagnitude(0,0,256)>.001,"Tape playback produced no recorded audio");
  p->getStateInformation(saved);
  auto portable=std::make_unique<PocketProcessor>(); portable->setStateInformation(saved.getData(),int(saved.getSize()));portable->prepareToPlay(48000,256);
  check(portable->sampleFrames.load()==4800,"Embedded sample failed to restore");
  portable->processBlock(buffer,midi);check(buffer.getMagnitude(0,0,256)>.001,"Embedded tape failed to restore");
  portable->state.getParameter("volume")->setValueNotifyingHost(0);
  for(int b=0;b<40;++b) portable->processBlock(buffer,midi);
  check(buffer.getMagnitude(0,0,256)<.000001,"Master output did not mute tape");
  portable->state.getParameter("volume")->setValueNotifyingHost(.7f);
  auto exported=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pocket-tape",".wav",false);
  check(portable->exportTape(exported,error),"Tape export failed");
  juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(exported.createInputStream().release(),true));
  check(reader&&reader->numChannels==4&&reader->lengthInSamples==384000,"Exported WAV has wrong channels/length");
  juce::AudioBuffer<float> exportedAudio(4,4096);check(reader->read(&exportedAudio,0,4096,0,true,true),"Exported WAV could not be read");
  check(exportedAudio.getMagnitude(0,0,4096)>.001,"Exported track is silent");
  for(int c=1;c<4;++c)check(exportedAudio.getMagnitude(c,0,4096)==0,"Recording leaked into other tracks");reader.reset();exported.deleteFile();
  // Restoring a session must disarm recording to protect its existing audio.
  p->state.getParameter("tapeRecord")->setValueNotifyingHost(1);p->getStateInformation(saved);portable->setStateInformation(saved.getData(),int(saved.getSize()));
  check(portable->state.getRawParameterValue("tapeRecord")->load()==0,"State restore left recording armed");
  struct Host : juce::AudioPlayHead {
   double beat=0;bool running=true;
   juce::Optional<PositionInfo> getPosition() const override {PositionInfo info;info.setBpm(120);info.setIsPlaying(running);info.setPpqPosition(beat);return info;}
  } host;
  portable->setPlayHead(&host);portable->state.getParameter("tapePlay")->setValueNotifyingHost(0);portable->state.getParameter("sequencer")->setValueNotifyingHost(1);
  host.beat=2;portable->processBlock(buffer,midi);check(portable->playhead.load()==8,"Host beat position not followed");
  host.beat=.75;portable->processBlock(buffer,midi);check(portable->playhead.load()==3,"Host seek not followed");
  host.running=false;portable->processBlock(buffer,midi);check(portable->playhead.load()==-1,"Host stop ignored");portable->setPlayHead(nullptr);
  p->state.getParameter("tapePlay")->setValueNotifyingHost(0);p->state.getParameter("tapeRecord")->setValueNotifyingHost(0);p->applyPreset(0);
  if(argc>1) {
   std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
   auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);
   juce::FileOutputStream output{juce::File(juce::String::fromUTF8(argv[1]))}; output.setPosition(0);output.truncate();juce::PNGImageFormat png;
   check(png.writeImageToStream(image,output),"Screenshot failed");
   for(auto* child:editor->getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child)) if(button->getButtonText()=="TAPE") button->onClick();
   auto tapeImage=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);
   juce::FileOutputStream tapeOutput{juce::File(juce::String::fromUTF8(argv[1])).getSiblingFile("tape.png")};
   tapeOutput.setPosition(0);tapeOutput.truncate();
   check(png.writeImageToStream(tapeImage,tapeOutput),"Tape screenshot failed");
  }
  std::cout<<"PASS: plugin MIDI timing, stereo audio, state roundtrip, sequencer/host sync, sampler/tape state, export and record disarm"<<std::endl;
 } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
