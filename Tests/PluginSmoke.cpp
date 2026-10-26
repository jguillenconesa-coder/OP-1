// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include "ScreenAnimations.h"
#include "SoundBrowser.h"
#include "KitEditor.h"
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
  // New sequencers retain sample-accurate host sync and release held arpeggios.
  {
   auto seq=std::make_unique<PocketProcessor>();seq->prepareToPlay(48000,256);
   auto set=[&](const char* id,float value){auto* parameter=seq->state.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
   set("sequencer",1);set("seqMode",4);set("tempo",120);set("seqRate",2);set("seqGate",.5f);set("seqSwing",.2f);set("arpOctaves",2);
   juce::AudioBuffer<float> sample(2,1);juce::MidiBuffer events;
   events.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);events.addEvent(juce::MidiMessage::noteOn(1,64,.8f),0);events.addEvent(juce::MidiMessage::noteOn(1,67,.8f),0);
   seq->processBlock(sample,events);events.clear();check(seq->sequenceNote.load()==60,"Arpeggio did not start on lowest held key");
   for(int i=1;i<3601;++i)seq->processBlock(sample,events);
   check(seq->sequenceNote.load()==-1,"Gate did not release halfway through swung step");
   for(int i=3601;i<7201;++i)seq->processBlock(sample,events);
   check(seq->sequenceNote.load()==64,"Swung arpeggio step did not advance at exact beat");
   events.addEvent(juce::MidiMessage::allNotesOff(1),0);seq->processBlock(sample,events);events.clear();check(seq->sequenceNote.load()==-1,"Arpeggio notes stuck after all notes off");
   for(int i=0;i<15000;++i)seq->processBlock(sample,events);check(seq->sequenceNote.load()==-1,"Empty arpeggio retriggered");
   juce::MemoryBlock sequenceState;seq->getStateInformation(sequenceState);auto roundtrip=std::make_unique<PocketProcessor>();roundtrip->setStateInformation(sequenceState.getData(),int(sequenceState.getSize()));
   for(const char* id:{"seqMode","seqLength","seqRate","seqGate","seqSwing","arpOctaves","seqVelocity","seqEuclidean","seqPulses","seqRotation"})check(std::abs(seq->state.getRawParameterValue(id)->load()-roundtrip->state.getRawParameterValue(id)->load())<.0001,"Sequence settings not portable");
   // Loading an older project must not inherit new controls from the last project.
   auto oldTree=juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(sequenceState.getData(),int(sequenceState.getSize())));
   for(const char* id:{"seqMode","seqLength","seqRate","seqGate","seqSwing","arpOctaves","seqVelocity","seqEuclidean","seqPulses","seqRotation"})oldTree.removeChild(oldTree.getChildWithProperty("id",id),nullptr);
   juce::MemoryBlock oldState;juce::AudioProcessor::copyXmlToBinary(*oldTree.createXml(),oldState);roundtrip->setStateInformation(oldState.getData(),int(oldState.getSize()));
   check(roundtrip->state.getRawParameterValue("seqMode")->load()==0&&roundtrip->state.getRawParameterValue("seqGate")->load()==1&&roundtrip->state.getRawParameterValue("seqSwing")->load()==0,"Older project inherited sequencer settings");
   struct Host:juce::AudioPlayHead {double beat=0;bool playing=true;juce::Optional<PositionInfo> getPosition()const override {PositionInfo p;p.setBpm(120);p.setPpqPosition(beat);p.setIsPlaying(playing);return p;}} host;
   seq->setPlayHead(&host);set("seqMode",1);set("seqLength",4);set("seqSwing",0);set("seqGate",1);
   seq->steps[0].store(60);seq->steps[1].store(62);seq->steps[2].store(64);seq->steps[3].store(65);
   host.beat=0;seq->processBlock(sample,events);check(seq->sequenceNote.load()==65,"Reverse sequence does not start at final active step");
   host.beat=.75;seq->processBlock(sample,events);check(seq->sequenceNote.load()==60,"Host seek did not relocate sequence immediately");
   host.beat=-.25;seq->processBlock(sample,events);check(seq->sequenceNote.load()==60,"Negative host beat fails");
   host.playing=false;seq->processBlock(sample,events);check(seq->sequenceNote.load()==-1,"Host stop left sequence voice active");seq->setPlayHead(nullptr);
  }
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
  auto stereoFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pocket-stereo",".wav",false);
  check(portable->exportMix(stereoFile,error),"COM stereo render failed");std::unique_ptr<juce::AudioFormatReader> mixReader(wav.createReaderFor(stereoFile.createInputStream().release(),true));
  check(mixReader&&mixReader->numChannels==2&&mixReader->lengthInSamples==384000,"COM render has wrong channel count or length");juce::AudioBuffer<float> mixProbe(2,4096);check(mixReader->read(&mixProbe,0,4096,0,true,true)&&mixProbe.getMagnitude(0,0,4096)>.001,"COM render is silent");mixReader.reset();stereoFile.deleteFile();
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
  // Every embedded sound must be playable without files on the user's disk.
  p->state.getParameter("tapePlay")->setValueNotifyingHost(0);p->state.getParameter("tapeRecord")->setValueNotifyingHost(0);
  p->state.getParameter("sequencer")->setValueNotifyingHost(0);
  check(PocketProcessor::factoryPresetNames().size()==25,"Factory sound catalog is incomplete");
  for(int preset=0;preset<25;++preset) {
   p->applyPreset(preset);p->prepareToPlay(48000,256);midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
   double total=0;
   for(int b=0;b<60;++b) {p->processBlock(buffer,midi);midi.clear();for(int i=0;i<256;++i){const auto v=buffer.getSample(0,i);check(std::isfinite(v),"Factory sound produced nonfinite output");total+=v*v;}}
   check(total>.001,"Factory preset is silent");
   if(preset>=18&&preset<=21) check(p->sampleFrames.load()==64000,"Embedded melodic WAV missing");
  }
  // All 24 slots in every kit, including the GM kick alias, must have audio.
  for(int kit=22;kit<25;++kit) for(int pad=0;pad<25;++pad) {
   p->applyPreset(kit);p->prepareToPlay(48000,256);midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,pad==24?36:60+pad,.8f),0);
   double total=0;for(int b=0;b<24;++b){p->processBlock(buffer,midi);midi.clear();for(int i=0;i<256;++i)total+=buffer.getSample(0,i)*buffer.getSample(0,i);}
   check(total>.001,"Embedded kit pad is silent");
  }
  p->getStateInformation(saved);auto kitRestored=std::make_unique<PocketProcessor>();kitRestored->setStateInformation(saved.getData(),int(saved.getSize()));kitRestored->prepareToPlay(48000,256);
  check(kitRestored->kitPads.load()==24,"24-pad kit count did not restore");
  check(kitRestored->currentFactoryPreset.load()==24,"Kit preset selection failed to restore");
  midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,38,.8f),0);kitRestored->processBlock(buffer,midi);
  check(buffer.getMagnitude(0,0,256)>.001,"Embedded kit state failed to restore");
  p->state.getParameter("tapePlay")->setValueNotifyingHost(0);p->state.getParameter("tapeRecord")->setValueNotifyingHost(0);p->applyPreset(0);
  // Import a ZIP bank into an isolated library, then reopen it and restore its sound state.
  auto library=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pocket-bank-test","",false);
  library.createDirectory();
  struct RemoveLibrary {juce::File file;~RemoveLibrary(){file.deleteRecursively();}} removeLibrary{library};
  auto bankProcessor=std::make_unique<PocketProcessor>(library.getChildFile("Library"));
  if(argc==3&&juce::String::fromUTF8(argv[1])=="--bank") {
   check(bankProcessor->importUserBank(juce::File(juce::String::fromUTF8(argv[2])),error),error.toRawUTF8());
   const auto names=bankProcessor->userPresetNames();check(!names.isEmpty(),"Imported bank is empty");
   for(int preset=0;preset<names.size();++preset) {
    check(bankProcessor->applyUserPreset(preset,error),error.toRawUTF8());bankProcessor->prepareToPlay(48000,256);
    midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);double total=0;
    for(int b=0;b<1100;++b){bankProcessor->processBlock(buffer,midi);midi.clear();for(int i=0;i<256;++i){auto value=buffer.getSample(0,i);check(std::isfinite(value),"User preset output is not finite");total+=value*value;}}
    if(total<=.000001) throw std::runtime_error("Silent user preset: "+names[preset].toStdString());
   }
   bankProcessor->getStateInformation(saved);auto independent=std::make_unique<PocketProcessor>(library.getChildFile("No bank installed"));independent->setStateInformation(saved.getData(),int(saved.getSize()));independent->prepareToPlay(48000,256);
   midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);independent->processBlock(buffer,midi);
   check(buffer.getMagnitude(0,0,256)>.00001,"User kit state requires external files");
   std::cout<<"PASS: all "<<names.size()<<" user presets decoded, played finite audio, and restored without their bank"<<std::endl;
  } else {
   juce::MemoryBlock wavData;juce::WavAudioFormat format;
   std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::MemoryOutputStream(wavData,false),48000,1,16,{},0));
   juce::AudioBuffer<float> fixture(1,4800);for(int i=0;i<4800;++i)fixture.setSample(0,i,float(std::sin(2*pocket::pi*220*i/48000))*.7f);
   check(writer&&writer->writeFromAudioSampleBuffer(fixture,0,4800),"Bank test WAV generation failed");writer.reset();
   auto makeZip=[&](const juce::String& manifest,const juce::String& audioPath){
    juce::ZipFile::Builder builder;builder.addEntry(new juce::MemoryInputStream(wavData.getData(),wavData.getSize(),true),9,audioPath,juce::Time::getCurrentTime());
    builder.addEntry(new juce::MemoryInputStream(manifest.toRawUTF8(),manifest.getNumBytesAsUTF8(),true),9,"bank.json",juce::Time::getCurrentTime());
    auto target=library.getNonexistentChildFile("test",".zip",false);juce::FileOutputStream output(target);check(builder.writeToStream(output,nullptr),"Bank ZIP generation failed");return target;
   };
   const auto valid=makeZip(R"({"formatVersion":1,"presets":[{"name":"Test bass","sample":"samples/test.wav","parameters":{"sampleRoot":45,"attack":0,"effect":0}}]})","samples/test.wav");
   bankProcessor->state.getParameter("tempo")->setValueNotifyingHost(.65f);const auto tempoBefore=bankProcessor->state.getRawParameterValue("tempo")->load();bankProcessor->steps[0].store(77);
   check(bankProcessor->importUserBank(valid,error),"Valid bank import failed");check(bankProcessor->userPresetNames().size()==1,"Bank catalog missing");check(bankProcessor->importUserBank(valid,error),"Repeat bank import failed");check(bankProcessor->userPresetNames().size()==1,"Reimport duplicated the bank catalog");
   check(bankProcessor->applyUserPreset(0,error),"User preset selection failed");check(bankProcessor->state.getRawParameterValue("sampleRoot")->load()==45,"Sample root note was not applied");
   check(bankProcessor->state.getRawParameterValue("tempo")->load()==tempoBefore&&bankProcessor->steps[0].load()==77,"Sound preset changed tempo or pattern");
   juce::String paths;for(int pad=0;pad<24;++pad){if(pad)paths+=",";paths+="\"samples/test.wav\"";}
   const auto kit24=makeZip("{\"formatVersion\":1,\"presets\":[{\"name\":\"24 pad fixture\",\"kit\":["+paths+"],\"pads\":[{\"gain\":0.25,\"pitch\":7,\"reverse\":true}],\"parameters\":{}}]}","samples/test.wav");
   auto extended=std::make_unique<PocketProcessor>(library.getChildFile("24 pad library"));check(extended->importUserBank(kit24,error),"24-pad bank import failed");check(extended->applyUserPreset(0,error),"24-pad bank load failed");check(extended->kitPads.load()==24,"Imported kit count wrong");
   check(std::abs(extended->state.getRawParameterValue("pad1_gain")->load()-.25f)<.0001f&&std::abs(extended->state.getRawParameterValue("pad1_pitch")->load()-7)<.0001f&&extended->state.getRawParameterValue("pad1_reverse")->load()==1,"Per-pad bank settings not applied");
   extended->getStateInformation(saved);auto embedded=std::make_unique<PocketProcessor>(library.getChildFile("Missing kit bank"));embedded->setStateInformation(saved.getData(),int(saved.getSize()));embedded->prepareToPlay(48000,256);midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,83,.8f),0);embedded->processBlock(buffer,midi);check(buffer.getMagnitude(0,0,256)>.00001,"Embedded pad 24 needs external bank");
   auto oldTree=juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize())));oldTree.removeProperty("kitCount",nullptr);for(int child=oldTree.getNumChildren();--child>=0;)if(oldTree.getChild(child).getProperty("id").toString().startsWith("pad"))oldTree.removeChild(child,nullptr);for(int pad=8;pad<24;++pad)oldTree.removeProperty("kitAudio"+juce::String(pad),nullptr);juce::MemoryBlock oldKit;juce::AudioProcessor::copyXmlToBinary(*oldTree.createXml(),oldKit);embedded->setStateInformation(oldKit.getData(),int(oldKit.getSize()));check(embedded->kitPads.load()==8,"Legacy embedded kit lost eight-pad mapping");check(embedded->state.getRawParameterValue("pad1_gain")->load()==1&&embedded->state.getRawParameterValue("pad1_reverse")->load()==0,"Legacy kit inherited per-pad edits");
   {std::unique_ptr<juce::AudioProcessorEditor> editor(extended->createEditor());for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="KIT EDIT")button->onClick();bool edited=false;for(auto* child:editor->getChildren())if(auto* kit=dynamic_cast<KitEditor*>(child)){check(kit->isVisible(),"Kit editor did not open");for(auto* control:kit->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(control))if(button->getButtonText().startsWith("24 /"))button->onClick();for(auto* control:kit->getChildren())if(control->getComponentID()=="pad-control-2")if(auto* slider=dynamic_cast<juce::Slider*>(control)){slider->setValue(.3,juce::sendNotificationSync);edited=true;}}check(edited&&std::abs(extended->state.getRawParameterValue("pad24_gain")->load()-.3f)<.001,"Kit editor gain changed the wrong pad");}
   auto reopened=std::make_unique<PocketProcessor>(library.getChildFile("Library"));check(reopened->userPresetNames().size()==1,"Bank did not persist across instances");
   const auto invalid=makeZip(R"({"formatVersion":1,"presets":[{"name":"Invalid","sample":"../escape.wav","parameters":{}}]})","../escape.wav");
   check(!bankProcessor->importUserBank(invalid,error),"Unsafe bank path was accepted");check(!library.getChildFile("Library/escape.wav").exists(),"ZIP escaped its bank folder");
   check(bankProcessor->userPresetNames().size()==1,"Failed import damaged existing bank");
  }
  // Browser metadata and independent edited sound slots survive project state.
  auto libraryProcessor=std::make_unique<PocketProcessor>(library.getChildFile("Catalog"));
  check(libraryProcessor->soundCatalog().size()==25,"Factory catalog incomplete");
  libraryProcessor->applyPreset(0);libraryProcessor->state.getParameter("tone")->setValueNotifyingHost(.83f);
  libraryProcessor->state.getParameter("seqMode")->setValueNotifyingHost(1);
  libraryProcessor->toggleFavorite("factory:0");check(libraryProcessor->assignSoundSlot(false,0),"Could not store edited synth slot");
  check(libraryProcessor->soundSlot(true,0)=="factory:22","Synth assignment changed drum slots");
  libraryProcessor->applyPreset(22);libraryProcessor->getStateInformation(saved);
  auto slotsRestored=std::make_unique<PocketProcessor>(library.getChildFile("Catalog"));slotsRestored->setStateInformation(saved.getData(),int(saved.getSize()));
  slotsRestored->state.getParameter("seqMode")->setValueNotifyingHost(0);
  check(slotsRestored->isFavorite("factory:0"),"Favorite was not restored");check(slotsRestored->recallSoundSlot(false,0,error),"Stored synth slot did not recall");
  check(std::abs(slotsRestored->state.getRawParameterValue("tone")->load()-.83f)<.001,"Slot lost edited sound parameters");
  check(slotsRestored->state.getRawParameterValue("seqMode")->load()==0,"Sound slot changed sequence mode");
  check(slotsRestored->copySound(error),"Sound lift failed");slotsRestored->state.getParameter("tone")->setValueNotifyingHost(.12f);slotsRestored->state.getParameter("masterDrive")->setValueNotifyingHost(.2f);
  check(slotsRestored->pasteSound(error),"Sound drop failed");check(std::abs(slotsRestored->state.getRawParameterValue("tone")->load()-.83f)<.001,"Sound clipboard lost parameters");check(std::abs(slotsRestored->state.getRawParameterValue("masterDrive")->load()-.2f)<.001,"Pasting sound overwrote master settings");
  // Capture actual input into a sampler without leaking monitored audio by default.
  auto capture=std::make_unique<PocketProcessor>(library.getChildFile("Capture"));capture->prepareToPlay(48000,256);
  check(capture->startInputCapture(error),"Input capture did not start");
  for(int block=0;block<32;++block){for(int i=0;i<256;++i){float value=float(std::sin(2*pocket::pi*220*(block*256+i)/48000))*.3f;buffer.setSample(0,i,value);buffer.setSample(1,i,value);}midi.clear();capture->processBlock(buffer,midi);check(buffer.getMagnitude(0,0,256)==0,"Input leaked while monitoring was disabled");}
  check(capture->finishInputCapture(error),"Input capture did not finish");check(capture->sampleFrames.load()==8192,"Captured input had wrong length");
  buffer.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);capture->processBlock(buffer,midi);check(buffer.getMagnitude(0,0,256)>.001,"Captured sample is silent");
  // MIDI metronome and master processing exercise the newly connected mixer pages.
  capture->requestPanic();capture->state.getParameter("inputMonitor")->setValueNotifyingHost(1);capture->state.getParameter("volume")->setValueNotifyingHost(1);
  auto inputEnergy=[&](float boost){auto* eq=capture->state.getParameter("eqLow");eq->setValueNotifyingHost(eq->convertTo0to1(boost));double energy=0;for(int block=0;block<100;++block){for(int i=0;i<256;++i){float value=float(std::sin(2*pocket::pi*100*(block*256+i)/48000))*.1f;buffer.setSample(0,i,value);buffer.setSample(1,i,value);}midi.clear();capture->processBlock(buffer,midi);if(block>30)for(int i=0;i<256;++i)energy+=buffer.getSample(0,i)*buffer.getSample(0,i);}return energy;};
  const double flat=inputEnergy(0),boosted=inputEnergy(12);check(boosted>flat*4,"Master low EQ has no audible effect");
  capture->state.getParameter("inputMonitor")->setValueNotifyingHost(0);capture->state.getParameter("metronome")->setValueNotifyingHost(1);capture->requestPanic();buffer.clear();midi.clear();capture->processBlock(buffer,midi);check(buffer.getMagnitude(0,0,256)>.01,"Metronome did not produce a click");
  // Browser search narrows the list independently of the playing sound.
  {auto editor=std::unique_ptr<juce::AudioProcessorEditor>(libraryProcessor->createEditor());SoundBrowser* browser=nullptr;for(auto* child:editor->getChildren())if(auto* found=dynamic_cast<SoundBrowser*>(child))browser=found;check(browser!=nullptr,"Sound browser is missing");
   juce::TextEditor* search=nullptr;juce::ListBox* list=nullptr;for(auto* child:browser->getChildren()){if(child->getComponentID()=="sound-search")search=dynamic_cast<juce::TextEditor*>(child);if(child->getComponentID()=="sound-results")list=dynamic_cast<juce::ListBox*>(child);}check(search&&list,"Browser controls are missing");
   search->setText("Soft keys",false);search->onTextChange();check(list->getListBoxModel()->getNumRows()==1,"Browser search did not narrow results");
   search->setText("a sound that does not exist",false);search->onTextChange();check(list->getListBoxModel()->getNumRows()==0,"Browser empty state failed");}
  // Every OLED page renders deterministic frames and reacts to its live telemetry.
  for(int screen=0;screen<14;++screen) {
   pocket::ScreenFrame frame;frame.engine=screen<6?screen:0;frame.page=screen<6?0:screen==10?4:screen>10?screen-6:screen-5;frame.mode=screen>=10?3:2;frame.voices=4;frame.level=.02f;frame.envelope=.4f;frame.stage=1;frame.samplePosition=.2f;frame.lfoPhase=.2f;frame.tapePosition=.5f;frame.trackGains={.8f,.6f,.7f,.5f};frame.sample.fill(.5f);
   juce::Image first(juce::Image::ARGB,251,123,true),second(juce::Image::ARGB,251,123,true);
   {juce::Graphics graphics(first);pocket::drawScreen(graphics,frame);}
   frame.time=2.1f;frame.level=.7f;frame.envelope=.8f;frame.samplePosition=.8f;frame.lfoPhase=.8f;frame.tapePosition=1.7f;frame.values={.7f,.4f,.5f,.8f};
   {juce::Graphics graphics(second);pocket::drawScreen(graphics,frame);}
   int changed=0;for(int y=0;y<123;++y)for(int x=0;x<251;++x)if(first.getPixelAt(x,y)!=second.getPixelAt(x,y))++changed;
   check(changed>3,"OLED page did not respond to animation telemetry");
  }
  if(argc>1&&juce::String::fromUTF8(argv[1])!="--bank") {
   if(argc>2){p->getStateInformation(saved);auto screenProcessor=std::make_unique<PocketProcessor>(library.getChildFile("Screen bank"));check(screenProcessor->importUserBank(juce::File(juce::String::fromUTF8(argv[2])),error),"Screenshot bank import failed");screenProcessor->setStateInformation(saved.getData(),int(saved.getSize()));p=std::move(screenProcessor);}

   p->prepareToPlay(48000,256);midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,69,.8f),0);p->processBlock(buffer,midi);midi.clear();
   std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
   auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);
   juce::FileOutputStream output{juce::File(juce::String::fromUTF8(argv[1]))}; output.setPosition(0);output.truncate();juce::PNGImageFormat png;
   check(png.writeImageToStream(image,output),"Screenshot failed");
   auto frames=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pocket-one-animation-preview");frames.createDirectory();
   for(int frame=0;frame<24;++frame) {for(int block=0;block<10;++block)p->processBlock(buffer,midi);auto preview=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);juce::FileOutputStream file(frames.getChildFile("frame-"+juce::String(frame).paddedLeft('0',2)+".png"));file.setPosition(0);file.truncate();check(png.writeImageToStream(preview,file),"Animation preview failed");}
   if(argc>2){for(const auto& entry:p->soundCatalog())if(entry.name.equalsIgnoreCase("crystal piano")&&entry.variant=="Dry"){check(p->loadSound(entry.id,error),"Screenshot sound load failed");break;}
    for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="SYNTH")button->onClick();
    for(auto* child:editor->getChildren())if(auto* browser=dynamic_cast<SoundBrowser*>(child)){for(auto* field:browser->getChildren())if(field->getComponentID()=="sound-search")if(auto* search=dynamic_cast<juce::TextEditor*>(field)){search->setText("crystal",false);search->onTextChange();}browser->syncSelection();}
    auto browserImage=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);juce::FileOutputStream browserOutput{juce::File(juce::String::fromUTF8(argv[1])).getSiblingFile("sound-browser.png")};browserOutput.setPosition(0);browserOutput.truncate();check(png.writeImageToStream(browserImage,browserOutput),"Browser screenshot failed");}
   {p->applyPreset(22);for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="DRUM")button->onClick();for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="KIT EDIT")button->onClick();auto shot=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);juce::FileOutputStream file(juce::File(juce::String::fromUTF8(argv[1])).getSiblingFile("kit-editor.png"));file.setPosition(0);file.truncate();check(png.writeImageToStream(shot,file),"Kit editor screenshot failed");}
   for(auto* child:editor->getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child)) if(button->getButtonText()=="TAPE") button->onClick();
   auto tapeImage=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.5f);
   juce::FileOutputStream tapeOutput{juce::File(juce::String::fromUTF8(argv[1])).getSiblingFile("tape.png")};
   tapeOutput.setPosition(0);tapeOutput.truncate();
   check(png.writeImageToStream(tapeImage,tapeOutput),"Tape screenshot failed");
  }
  std::cout<<"PASS: plugin MIDI timing, stereo audio, state roundtrip, sequencer/host sync, sampler/tape state, export and record disarm"<<std::endl;
 } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
