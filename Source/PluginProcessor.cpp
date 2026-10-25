// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"
namespace {
juce::String encodeAudio(const juce::AudioBuffer<float>& audio,double rate) {
    juce::MemoryBlock data;
    auto stream=std::make_unique<juce::MemoryOutputStream>(data,false);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),rate,unsigned(audio.getNumChannels()),24,{},0));
    if(!writer) return {};
    stream.release();
    if(!writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples())) return {};
    writer.reset();
    juce::MemoryOutputStream compressed;
    {juce::GZIPCompressorOutputStream gzip(&compressed,6,false); gzip.write(data.getData(),data.getSize());}
    return compressed.getMemoryBlock().toBase64Encoding();
}
bool decodeAudio(const juce::String& encoded,juce::AudioBuffer<float>& audio,double& rate,int maxChannels) {
    if(encoded.isEmpty()||encoded.length()>140000000) return false;
    juce::MemoryBlock block; if(!block.fromBase64Encoding(encoded)) return false;
    juce::MemoryInputStream compressed(block,false); juce::GZIPDecompressorInputStream unzip(compressed);
    juce::MemoryBlock unpacked; unzip.readIntoMemoryBlock(unpacked,64000000);
    if(unpacked.getSize()>=64000000) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::MemoryInputStream(unpacked,false),true));
    if(!reader||reader->sampleRate<8000||reader->sampleRate>192000||reader->numChannels<1||reader->numChannels>unsigned(maxChannels)||reader->lengthInSamples<1||reader->lengthInSamples>reader->sampleRate*16) return false;
    audio.setSize(int(reader->numChannels),int(reader->lengthInSamples)); rate=reader->sampleRate;
    return reader->read(&audio,0,audio.getNumSamples(),0,true,true);
}
}
PocketProcessor::PocketProcessor() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"PocketOne",layout()) {
    const int notes[]={60,-1,67,-1,63,-1,70,-1,60,72,67,-1,63,-1,58,-1};
    for(int i=0;i<16;++i) steps[size_t(i)].store(notes[i]);
}
juce::AudioProcessorValueTreeState::ParameterLayout PocketProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"engine",1},"Engine",juce::StringArray{"FM","PHASE","STRING","SAMPLER","DRUM"},0));
    auto number=[&](const char* id,float low,float high,float value){p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},id,low,high,value));};
    auto flag=[&](const char* id){p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},id,false));};
    for(auto pair:{std::pair{"tone",.45f},{"color",.35f},{"envelope",.35f},{"effect",.2f},{"volume",.7f},{"attack",.02f},{"decay",.2f},{"sustain",.8f},{"release",.3f},{"lfoDepth",0.f},{"delayFeedback",.42f},{"drive",0.f},{"track1",.8f},{"track2",.8f},{"track3",.8f},{"track4",.8f}}) number(pair.first,0,1,pair.second);
    number("lfoRate",.05f,20,1); number("delayTime",.05f,1,.281f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"lfoDestination",1},"LFO destination",juce::StringArray{"PITCH","FILTER"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"lfoShape",1},"LFO shape",juce::StringArray{"SINE","TRIANGLE","SQUARE"},0));
    flag("sequencer"); flag("sampleLoop"); flag("tapePlay"); flag("tapeRecord"); flag("tapeOverdub");
    number("tempo",40,240,110); number("tapeSpeed",-2,2,1);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tapeTrack",1},"Tape track",juce::StringArray{"1","2","3","4"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tapeLength",1},"Tape loop",juce::StringArray{"4 seconds","8 seconds","16 seconds"},1));
    return p;
}
void PocketProcessor::prepareToPlay(double sr,int) {
    outputGain=state.getRawParameterValue("volume")->load(); outputSmoothing=float(1-std::exp(-1/(sr*.005)));
    rate=sr; engine.prepare(sr); tape.prepare(sr); clock=0; step=0; seqNote=-1; wasSequencing=false; lastHostStep=-1; keyboard.reset();
}
double PocketProcessor::getTailLengthSeconds() const {
    const double feedback=std::clamp(double(state.getRawParameterValue("delayFeedback")->load()),.01,.85);
    const double echo=state.getRawParameterValue("delayTime")->load()*std::log(.00001)/std::log(feedback);
    const double release=state.getRawParameterValue("release")->load(),macro=state.getRawParameterValue("envelope")->load();
    return std::max(echo,(.008+release*release*5*(.5+macro))*12);
}
bool PocketProcessor::isBusesLayoutSupported(const BusesLayout& b) const {return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
void PocketProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear(); keyboard.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
    if(panicRequested.exchange(false)) { engine.reset(); seqNote=-1; }
    auto get=[this](const char* id){return state.getRawParameterValue(id)->load();};
    pocket::Parameters p;
    p.engine=int(get("engine")); p.tone=get("tone"); p.color=get("color"); p.envelope=get("envelope"); p.effect=get("effect"); p.volume=1;
    p.attack=get("attack"); p.decay=get("decay"); p.sustain=get("sustain"); p.release=get("release");
    p.lfoRate=get("lfoRate"); p.lfoDepth=get("lfoDepth"); p.lfoDestination=int(get("lfoDestination")); p.lfoShape=int(get("lfoShape"));
    p.delayTime=get("delayTime"); p.feedback=get("delayFeedback"); p.drive=get("drive"); p.sampleLoop=get("sampleLoop")>.5f;
    const float outputTarget=get("volume");
    bool running=get("sequencer")>.5f;
    double bpm=get("tempo"),ppq=0; bool hostPosition=false,hostPlaying=true;
    if(auto* host=getPlayHead()) if(auto pos=host->getPosition()) {
        if(auto tempo=pos->getBpm()) bpm=*tempo;
        hostPlaying=pos->getIsPlaying(); running=running&&hostPlaying;
        if(auto beat=pos->getPpqPosition()) {ppq=*beat; hostPosition=true;}
    }
    if(running&&!wasSequencing) {clock=0; step=0; lastHostStep=-1;}
    if(!running&&wasSequencing) {if(seqNote>=0) engine.noteOff(seqNote,17); seqNote=-1; playhead.store(-1); lastHostStep=-1;}
    wasSequencing=running;
    const std::array<float,4> gains{get("track1"),get("track2"),get("track3"),get("track4")};
    const bool tapePlaying=get("tapePlay")>.5f&&hostPlaying,tapeRecording=get("tapeRecord")>.5f;
    const bool overdub=get("tapeOverdub")>.5f; const int selectedTrack=int(get("tapeTrack")); const float speed=get("tapeSpeed");
    const float tapeSeconds=float(4<<int(get("tapeLength")));
    auto it=midi.cbegin();
    for(int i=0;i<buffer.getNumSamples();++i) {
        while(it!=midi.cend()&&(*it).samplePosition<=i) {
            const auto m=(*it).getMessage(); const int ch=m.getChannel();
            if(m.isNoteOn()) {engine.noteOn(m.getNoteNumber(),m.getFloatVelocity(),ch); lastNote.store(m.getNoteNumber());}
            else if(m.isNoteOff()) engine.noteOff(m.getNoteNumber(),ch);
            else if(m.isPitchWheel()) engine.pitchBend(ch,float(m.getPitchWheelValue()-8192)/8192.f*2);
            else if(m.isController()&&m.getControllerNumber()==64) engine.sustainPedal(ch,m.getControllerValue()>=64);
            else if(m.isAllSoundOff()) engine.reset();
            else if(m.isAllNotesOff()) engine.allNotesOff(ch);
            ++it;
        }
        if(running) {
            int hostStep=-1;
            if(hostPosition) { const double beat=ppq+double(i)*bpm/(60*rate); hostStep=int(std::floor(beat*4)); }
            const bool trigger=hostPosition?hostStep!=lastHostStep:clock<=0;
            if(trigger) {
                if(seqNote>=0) engine.noteOff(seqNote,17);
                const int index=hostPosition?((hostStep%16)+16)%16:step;
                seqNote=steps[size_t(index)].load(); if(seqNote>=0) engine.noteOn(seqNote,.8f,17);
                playhead.store(index); step=(index+1)%16; lastHostStep=hostStep;
                if(!hostPosition) clock+=rate*60.0/std::clamp(bpm,40.0,300.0)/4;
            }
            if(!hostPosition) --clock;
        }
        const float mixed=tape.process(engine.next(p),tapePlaying,tapeRecording,overdub,selectedTrack,speed,tapeSeconds,gains);
        outputGain+=outputSmoothing*(outputTarget-outputGain);
        const float value=mixed*outputGain;
        for(int ch=0;ch<buffer.getNumChannels();++ch) buffer.setSample(ch,i,value);
        if(++scopeCounter>=8) {scopeCounter=0; scope[size_t(scopeIndex)].store(value,std::memory_order_relaxed); scopeIndex=(scopeIndex+1)%256;}
    }
    tapePosition.store(float(tape.getPosition())); midi.clear();
}
bool PocketProcessor::loadSample(const juce::File& file,juce::String& error) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<1) {error="Cannot read this WAV/AIFF file (8-192 kHz required).";return false;}
    const int length=int(std::min(reader->lengthInSamples,juce::int64(reader->sampleRate*16)));
    juce::AudioBuffer<float> audio(2,length);
    if(!reader->read(&audio,0,length,0,true,true)) {error="Audio file could not be decoded.";return false;}
    pocket::Sample data; data.rate=reader->sampleRate; data.frames.resize(size_t(length));
    for(int i=0;i<length;++i) data.frames[size_t(i)]=reader->numChannels>1?(audio.getSample(0,i)+audio.getSample(1,i))*.5f:audio.getSample(0,i);
    {const juce::ScopedLock lock(getCallbackLock()); engine.setSample(std::move(data)); sampleFrames.store(length);}
    for(auto pair:{std::pair{"tone",0.f},{"color",1.f},{"engine",3.f}}) {auto* parameter=state.getParameter(pair.first); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(pair.second)); parameter->endChangeGesture();}
    return true;
}
void PocketProcessor::clearTape() {const juce::ScopedLock lock(getCallbackLock()); tape.clear(int(state.getRawParameterValue("tapeTrack")->load()));}
void PocketProcessor::rewindTape() {const juce::ScopedLock lock(getCallbackLock()); tape.rewind(); tapePosition.store(0);}
bool PocketProcessor::exportTape(const juce::File& file,juce::String& error) {
    juce::AudioBuffer<float> audio; double sr;
    {const juce::ScopedLock lock(getCallbackLock()); sr=tape.getRate(); const int n=int(sr*float(4<<int(state.getRawParameterValue("tapeLength")->load()))); audio.setSize(4,n); const auto& tracks=tape.getTracks(); for(int c=0;c<4;++c) {audio.clear(c,0,n); const int size=std::min(n,int(tracks[size_t(c)].size())); if(size>0) audio.copyFrom(c,0,tracks[size_t(c)].data(),size);}}
    auto stream=file.createOutputStream(); if(!stream) {error="Cannot create output file.";return false;}
    stream->setPosition(0); stream->truncate(); juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),sr,4,24,{},0));
    if(!writer) {error="Cannot create WAV writer.";return false;}
    stream.release(); if(!writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples())) {error="Could not write all tape frames.";return false;} return true;
}
void PocketProcessor::applyPreset(int index) {
    const std::array<std::array<float,7>,6> presets{{{0,.18f,.15f,.2f,.15f,.01f,.18f},{0,.7f,.62f,.6f,.35f,.08f,.5f},{1,.58f,.3f,.4f,.28f,.04f,.4f},{2,.7f,.2f,.25f,.25f,.01f,.3f},{4,.28f,.2f,.3f,.05f,0,.1f},{4,.7f,.8f,.1f,.12f,0,.1f}}};
    if(index<0||index>=int(presets.size())) return;
    const auto& values=presets[size_t(index)]; const char* ids[]={"engine","tone","color","envelope","effect","attack","release"};
    for(int i=0;i<7;++i) {auto* parameter=state.getParameter(ids[i]); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(values[size_t(i)])); parameter->endChangeGesture();}
    for(auto pair:{std::pair{"decay",.2f},{"sustain",.8f},{"lfoDepth",0.f},{"drive",0.f},{"delayTime",.281f},{"delayFeedback",.42f}}) {auto* parameter=state.getParameter(pair.first); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(pair.second)); parameter->endChangeGesture();}
    requestPanic();
}
void PocketProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree=state.copyState(); for(int i=0;i<16;++i) tree.setProperty("step"+juce::String(i),steps[size_t(i)].load(),nullptr);
    juce::AudioBuffer<float> sampleAudio,tapeAudio; double sampleRate=48000,tapeRate=48000;
    {const juce::ScopedLock lock(getCallbackLock()); const auto& sample=engine.getSample(); sampleRate=sample.rate;
        if(!sample.frames.empty()) {sampleAudio.setSize(1,int(sample.frames.size())); sampleAudio.copyFrom(0,0,sample.frames.data(),sampleAudio.getNumSamples());}
        const auto& tracks=tape.getTracks(); tapeRate=tape.getRate();
        if(!tracks[0].empty()) {tapeAudio.setSize(4,int(tracks[0].size())); for(int c=0;c<4;++c) tapeAudio.copyFrom(c,0,tracks[size_t(c)].data(),tapeAudio.getNumSamples());}
    }
    if(sampleAudio.getNumSamples()>0) tree.setProperty("sampleAudio",encodeAudio(sampleAudio,sampleRate),nullptr);
    if(tapeAudio.getNumSamples()>0) tree.setProperty("tapeAudio",encodeAudio(tapeAudio,tapeRate),nullptr);
    if(auto xml=tree.createXml()) copyXmlToBinary(*xml,dest);
}
void PocketProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto tree=juce::ValueTree::fromXml(*xml);
        juce::AudioBuffer<float> sampleAudio,tapeAudio; double sampleRate=48000,tapeRate=48000;
        const bool hasSample=decodeAudio(tree.getProperty("sampleAudio").toString(),sampleAudio,sampleRate,1);
        const bool hasTape=decodeAudio(tree.getProperty("tapeAudio").toString(),tapeAudio,tapeRate,4)&&tapeAudio.getNumChannels()==4;
        if(auto record=tree.getChildWithProperty("id","tapeRecord");record.isValid()) record.setProperty("value",0.f,nullptr);
        // Encoded payloads belong to the state snapshot, not the live parameter tree.
        tree.removeProperty("sampleAudio",nullptr); tree.removeProperty("tapeAudio",nullptr);
        pocket::Sample sample; sample.rate=sampleRate;
        if(hasSample) sample.frames.assign(sampleAudio.getReadPointer(0),sampleAudio.getReadPointer(0)+sampleAudio.getNumSamples());
        std::array<std::vector<float>,4> tracks;
        if(hasTape) for(int c=0;c<4;++c) tracks[size_t(c)].assign(tapeAudio.getReadPointer(c),tapeAudio.getReadPointer(c)+tapeAudio.getNumSamples());
        {const juce::ScopedLock lock(getCallbackLock()); engine.setSample(std::move(sample)); sampleFrames.store(hasSample?sampleAudio.getNumSamples():0);
            if(hasTape) {tape.restore(std::move(tracks),tapeRate); tape.prepare(rate);} else {tape.prepare(rate); for(int c=0;c<4;++c) tape.clear(c);}
            for(int i=0;i<16;++i) steps[size_t(i)].store(juce::jlimit(-1,127,int(tree.getProperty("step"+juce::String(i),-1))));
            state.replaceState(tree);
        }
    }
}
juce::AudioProcessorEditor* PocketProcessor::createEditor() {return new PocketEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new PocketProcessor();}
