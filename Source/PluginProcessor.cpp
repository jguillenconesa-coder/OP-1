// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "FactoryData.h"
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
PocketProcessor::PocketProcessor(juce::File soundsDirectory) : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"PocketOne",layout()) {
    libraryDirectory=soundsDirectory==juce::File{}?juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Pocket Instruments/Pocket One/Banks"):soundsDirectory;
    const char* audioIds[]={"engine","tone","color","envelope","effect","attack","decay","sustain","release","lfoRate","lfoDepth","lfoDestination","lfoShape","delayTime","delayFeedback","drive","sampleLoop","sampleRoot","volume","sequencer","tempo","track1","track2","track3","track4","tapePlay","tapeRecord","tapeOverdub","tapeTrack","tapeSpeed","tapeLength","tapeLoop","tapeLoopIn","tapeLoopOut","metronome","inputMonitor","inputGain","tapeRecordGain","eqLow","eqMid","eqHigh","eqMix","masterDrive","masterEcho","masterDelayTime","masterFeedback","clickVolume","seqMode","seqLength","seqRate","seqGate","seqSwing","arpOctaves","seqVelocity","seqEuclidean","seqPulses","seqRotation","sampleClean"};
    for(size_t i=0;i<audioParameters.size();++i) audioParameters[i]=state.getRawParameterValue(audioIds[i]);
    const char* colours[]={"Blue","Green","White","Orange"};
    for(int i=0;i<4;++i){auto* knob=new ScreenKnobParameter("screenKnob"+juce::String(i+1),"Screen: Knob "+juce::String(i+1)+" "+colours[i]);addParameter(knob);screenKnobs[size_t(i)]=knob;}
    screenKnobs[0]->enableTapeCursor(tapePosition,pendingTapeSeek,*state.getRawParameterValue("tapeLength"));
    screenKnobs[1]->enableTapeTracks({state.getParameter("track1"),state.getParameter("track2"),state.getParameter("track3"),state.getParameter("track4")},*state.getRawParameterValue("tapeTrack"));
    juce::AudioProcessorValueTreeState::ParameterLayout phaseLayout;
    phaseLayout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"phase246Enabled",1},"Phase 246: Experimental Core",false));
    const char* phaseNames[]={"Phase 246: Blue Shape","Phase 246: Green Primary Gain","Phase 246: White Contour","Phase 246: Orange Cycles"};
    for(size_t i=0;i<4;++i)phaseLayout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{pocket::phase246Ids[i],1},phaseNames[i],0,pocket::phase246Max[i],i==1?29491:0));
    // Append only: never shift the five 0.8.0 Phase controls or old Screen proxies.
    phaseLayout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"phase246Envelope",1},"Phase 246: Original Envelope",false));
    const char* envelopeNames[]={"Phase 246: Envelope Attack","Phase 246: Envelope Decay","Phase 246: Envelope Sustain","Phase 246: Envelope Release"};
    const int envelopeDefaults[]={64,8064,32767,8192};
    for(size_t i=0;i<4;++i)phaseLayout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{pocket::phase246EnvelopeIds[i],1},envelopeNames[i],0,i==2?32767:16383,envelopeDefaults[i]));
    // Append Digital after every 0.9.0 parameter; keep the original engine choice.
    phaseLayout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"digital246Enabled",1},"Digital 246: Experimental Core",false));
    const char* digitalNames[]={"Digital 246: Blue Feedback","Digital 246: Green Ratio","Digital 246: White Detune and Sync","Digital 246: Orange XOR"};
    const int digitalDefaults[]={0,16384,0,0};
    for(size_t i=0;i<4;++i)phaseLayout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{pocket::digital246Ids[i],1},digitalNames[i],pocket::digital246Min[i],pocket::digital246Max[i],digitalDefaults[i]));
    phaseLayout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"digital246Envelope",1},"Digital 246: Original Envelope",false));
    const char* digitalEnvelopeNames[]={"Digital 246: Envelope Attack","Digital 246: Envelope Decay","Digital 246: Envelope Sustain","Digital 246: Envelope Release"};
    for(size_t i=0;i<4;++i)phaseLayout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{pocket::digital246EnvelopeIds[i],1},digitalEnvelopeNames[i],0,i==2?32767:16383,envelopeDefaults[i]));
    phaseState=std::make_unique<juce::AudioProcessorValueTreeState>(*this,nullptr,"Phase246",std::move(phaseLayout));
    phaseAudio[0]=phaseState->getRawParameterValue("phase246Enabled");for(size_t i=0;i<4;++i)phaseAudio[i+1]=phaseState->getRawParameterValue(pocket::phase246Ids[i]);
    phaseAudio[5]=phaseState->getRawParameterValue("phase246Envelope");for(size_t i=0;i<4;++i)phaseAudio[i+6]=phaseState->getRawParameterValue(pocket::phase246EnvelopeIds[i]);
    phaseAudio[10]=phaseState->getRawParameterValue("digital246Enabled");for(size_t i=0;i<4;++i)phaseAudio[i+11]=phaseState->getRawParameterValue(pocket::digital246Ids[i]);
    phaseAudio[15]=phaseState->getRawParameterValue("digital246Envelope");for(size_t i=0;i<4;++i)phaseAudio[i+16]=phaseState->getRawParameterValue(pocket::digital246EnvelopeIds[i]);
    setScreenPage(0);
    for(int pad=0;pad<pocket::kitCapacity;++pad){const char* ids[]={"start","end","gain","pitch","reverse","gated"};for(int i=0;i<6;++i)padParameters[size_t(pad)][size_t(i)]=state.getRawParameterValue(padParameterId(pad,ids[i]));}
    keyboard.addListener(this);
    initializeSoundSlots();
    refreshUserPresets();
    loadFactoryKit(0);
    const int notes[]={60,-1,67,-1,63,-1,70,-1,60,72,67,-1,63,-1,58,-1};
    for(int i=0;i<16;++i) steps[size_t(i)].store(notes[i]);
}
PocketProcessor::~PocketProcessor() {for(auto* knob:screenKnobs)knob->disconnect();keyboard.removeListener(this);}

juce::String PocketProcessor::screenParameterId(int index) const {
    if(index<0||index>=4)return {};
    static const char* ids[8][4]={{"tone","color","envelope","effect"},{"attack","decay","sustain","release"},{"lfoRate","lfoDepth","lfoDestination","lfoShape"},{"delayTime","delayFeedback","effect","drive"},{"track1","track2","track3","track4"},{"eqLow","eqMid","eqHigh","eqMix"},{"masterDrive","masterEcho","masterDelayTime","masterFeedback"},{"volume","inputGain","tapeRecordGain","clickVolume"}};
    if(visiblePage.load()==4&&visibleTape.load()){
        if(index==0)return {};
        if(index==1)return "track"+juce::String(1+int(state.getRawParameterValue("tapeTrack")->load()));
        return index==2?"tapeSpeed":"tapeRecordGain";
    }
    if(visiblePage.load()==0&&phase246Enabled())return pocket::phase246Ids[size_t(index)];
    if(visiblePage.load()==1&&phase246EnvelopeEnabled())return pocket::phase246EnvelopeIds[size_t(index)];
    if(visiblePage.load()==0&&digital246Enabled())return pocket::digital246Ids[size_t(index)];
    if(visiblePage.load()==1&&digital246EnvelopeEnabled())return pocket::digital246EnvelopeIds[size_t(index)];
    return ids[size_t(visiblePage.load())][size_t(index)];
}
void PocketProcessor::setScreenPage(int page,bool tapeMode) {
    visiblePage.store(std::clamp(page,0,7));visibleTape.store(tapeMode&&page==4);
    const char* normal[]={"tone","color","envelope","effect"};
    const char* envelope[]={"attack","decay","sustain","release"};
    for(int i=0;i<4;++i){if(visiblePage.load()==0)screenKnobs[size_t(i)]->followNative(state.getParameter(normal[i]),phaseState->getParameter(pocket::phase246Ids[size_t(i)]),phaseState->getParameter(pocket::digital246Ids[size_t(i)]),*phaseAudio[0],*phaseAudio[10],*audioParameters[size_t(Param::engine)]);
        else if(visiblePage.load()==1)screenKnobs[size_t(i)]->followNative(state.getParameter(envelope[i]),phaseState->getParameter(pocket::phase246EnvelopeIds[size_t(i)]),phaseState->getParameter(pocket::digital246EnvelopeIds[size_t(i)]),*phaseAudio[5],*phaseAudio[15],*audioParameters[size_t(Param::engine)],phaseAudio[0],phaseAudio[10]);
        else if(page==4&&tapeMode&&i==1)screenKnobs[1]->followTapeTrack();else screenKnobs[size_t(i)]->follow(state.getParameter(screenParameterId(i)));}
}
void PocketProcessor::handleNoteOn(juce::MidiKeyboardState*,int channel,int note,float velocity) {
    if(!reflectingHostNotes&&!uiToAudio.push({channel,note,velocity,true})) keyboardOverflow.store(true);
}
void PocketProcessor::handleNoteOff(juce::MidiKeyboardState*,int channel,int note,float) {
    if(!reflectingHostNotes&&!uiToAudio.push({channel,note,0,false})) keyboardOverflow.store(true);
}
void PocketProcessor::updateKeyboardDisplay() {
    reflectingHostNotes=true;pocket::NoteEvent event;
    if(keyboardDisplayReset.exchange(false)) {keyboard.reset();while(audioToUi.pop(event)) {}}
    while(audioToUi.pop(event)) keyboard.processNextMidiEvent(event.on?juce::MidiMessage::noteOn(event.channel,event.note,event.velocity):juce::MidiMessage::noteOff(event.channel,event.note));
    reflectingHostNotes=false;
}
juce::AudioProcessorValueTreeState::ParameterLayout PocketProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"engine",1},"Sound: Engine",juce::StringArray{"FM","PHASE","STRING","SAMPLER","DRUM","KIT"},0));
    const auto name=[](const char* id)->juce::String{
        static const std::pair<const char*,const char*> names[]={
            {"tone","Sound: Tone"},
            {"color","Sound: Color"},
            {"envelope","Sound: Envelope Macro"},
            {"effect","Sound FX: Echo Mix"},
            {"volume","Output: Volume"},
            {"attack","Envelope: Attack"},
            {"decay","Envelope: Decay"},
            {"sustain","Envelope: Sustain"},
            {"release","Envelope: Release"},
            {"lfoDepth","LFO: Depth"},
            {"delayFeedback","Sound FX: Echo Feedback"},
            {"drive","Sound FX: Drive"},
            {"sampleRoot","Sampler: Root Note"},{"sampleClean","Sampler: Clean Playback"},
            {"lfoRate","LFO: Rate"},
            {"delayTime","Sound FX: Echo Time"},
            {"sequencer","Sequencer: Run"},
            {"sampleLoop","Sampler: Loop"},
            {"tapePlay","Tape: Play"},
            {"tapeRecord","Tape: Record Arm"},
            {"tapeOverdub","Tape: Overdub"},
            {"eqLow","Master EQ: Low Gain"},
            {"eqMid","Master EQ: Mid Gain"},
            {"eqHigh","Master EQ: High Gain"},
            {"eqMix","Master EQ: Mix"},
            {"masterDrive","Master FX: Drive"},
            {"masterEcho","Master FX: Echo Mix"},
            {"masterDelayTime","Master FX: Echo Time"},
            {"masterFeedback","Master FX: Echo Feedback"},
            {"clickVolume","Metronome: Level"},
            {"inputMonitor","Input: Monitor"},
            {"inputGain","Input: Gain"},
            {"tapeRecordGain","Tape: Record Gain"},
            {"tapeLoop","Tape: Loop Enable"},
            {"metronome","Metronome: Enable"},
            {"tapeLoopIn","Tape: Loop In"},
            {"tapeLoopOut","Tape: Loop Out"},
            {"tempo","Sequencer: Internal Tempo"},
            {"tapeSpeed","Tape: Playback Speed"},
            {"seqEuclidean","Sequencer: Euclidean Enable"},
            {"seqGate","Sequencer: Gate"},
            {"seqSwing","Sequencer: Swing"},
            {"seqVelocity","Sequencer: Note Velocity"},
            {"track1","Tape: Track 1 Level"},
            {"track2","Tape: Track 2 Level"},
            {"track3","Tape: Track 3 Level"},
            {"track4","Tape: Track 4 Level"},
        };
        for(const auto& pair:names)if(juce::String(id)==pair.first)return pair.second;
        return id;
    };
    const auto unit=[](const char* id)->juce::String{const juce::String key(id);if(key=="tempo")return "BPM";if(key=="lfoRate")return "Hz";if(key=="delayTime"||key=="masterDelayTime"||key=="tapeLoopIn"||key=="tapeLoopOut")return "s";if(key=="eqLow"||key=="eqMid"||key=="eqHigh")return "dB";if(key=="sampleRoot")return "MIDI note";if(key=="tapeSpeed")return "x";return {};};
    auto number=[&](const char* id,float low,float high,float value){p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name(id),juce::NormalisableRange<float>(low,high,.01f),value,juce::AudioParameterFloatAttributes().withLabel(unit(id))));};
    auto flag=[&](const char* id){p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},name(id),false));};
    for(auto pair:{std::pair{"tone",.45f},{"color",.35f},{"envelope",.35f},{"effect",.2f},{"volume",.7f},{"attack",.02f},{"decay",.2f},{"sustain",.8f},{"release",.3f},{"lfoDepth",0.f},{"delayFeedback",.42f},{"drive",0.f},{"track1",.8f},{"track2",.8f},{"track3",.8f},{"track4",.8f}}) number(pair.first,0,1,pair.second);
    number("sampleRoot",0,127,60);
    number("lfoRate",.05f,20,1); number("delayTime",.05f,1,.281f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"lfoDestination",1},"LFO: Destination",juce::StringArray{"PITCH","FILTER"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"lfoShape",1},"LFO: Shape",juce::StringArray{"SINE","TRIANGLE","SQUARE"},0));
    flag("sequencer"); flag("sampleLoop"); flag("tapePlay"); flag("tapeRecord"); flag("tapeOverdub");
    number("eqLow",-12,12,0);number("eqMid",-12,12,0);number("eqHigh",-12,12,0);number("eqMix",0,1,1);
    number("masterDrive",0,1,0);number("masterEcho",0,1,0);number("masterDelayTime",.05f,1,.281f);number("masterFeedback",0,.85f,.42f);number("clickVolume",0,.3f,.12f);
    flag("inputMonitor");number("inputGain",0,2,1);number("tapeRecordGain",0,2,1);
    flag("tapeLoop");flag("metronome");number("tapeLoopIn",0,16,0);number("tapeLoopOut",0,16,16);
    number("tempo",40,240,110); number("tapeSpeed",-2,2,1);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tapeTrack",1},"Tape: Record Track",juce::StringArray{"1","2","3","4"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tapeLength",1},"Tape: Loop Length",juce::StringArray{"4 seconds","8 seconds","16 seconds"},1));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"seqMode",1},"Sequencer: Mode",juce::StringArray{"FORWARD","REVERSE","PENDULUM","RANDOM","ARP UP","ARP DOWN","ARP UP/DOWN"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"seqRate",1},"Sequencer: Rate",juce::StringArray{"1/4","1/8","1/16","1/32"},2));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"seqLength",1},"Sequencer: Pattern Length",1,16,16));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"arpOctaves",1},"Arpeggiator: Octaves",1,3,1));
    flag("seqEuclidean");
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"seqPulses",1},"Sequencer: Euclidean Pulses",0,16,8));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"seqRotation",1},"Sequencer: Euclidean Rotation",0,15,0));
    number("seqGate",.1f,1,1);number("seqSwing",0,.45f,0);number("seqVelocity",.1f,1,.8f);
    for(int pad=0;pad<pocket::kitCapacity;++pad){
        const auto prefix="Pad "+juce::String(pad+1).paddedLeft('0',2)+": ";
        for(const auto* field:{"start","end","gain","pitch"}){const float low=juce::String(field)=="pitch"?-24:0,high=juce::String(field)=="pitch"?24:juce::String(field)=="gain"?2:1,initial=juce::String(field)=="end"||juce::String(field)=="gain"?1:0;
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{padParameterId(pad,field),1},prefix+juce::String(field).substring(0,1).toUpperCase()+juce::String(field).substring(1),juce::NormalisableRange<float>(low,high,.01f),initial,juce::AudioParameterFloatAttributes().withLabel(juce::String(field)=="pitch"?"st":"")));}
        for(const auto* field:{"reverse","gated"})p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{padParameterId(pad,field),1},prefix+juce::String(field).substring(0,1).toUpperCase()+juce::String(field).substring(1),false));
    }
    flag("sampleClean");
    return p;
}
void PocketProcessor::prepareToPlay(double sr,int) {
    outputGain=state.getRawParameterValue("volume")->load(); outputSmoothing=float(1-std::exp(-1/(sr*.005)));
    eqLowCoefficient=float(1-std::exp(-2*pocket::pi*350/sr));eqHighCoefficient=float(1-std::exp(-2*pocket::pi*3500/sr));resetOutputHistory();
    masterDriveSmooth=state.getRawParameterValue("masterDrive")->load();masterEchoSmooth=state.getRawParameterValue("masterEcho")->load();masterTimeSmooth=state.getRawParameterValue("masterDelayTime")->load();eqMixSmooth=state.getRawParameterValue("eqMix")->load();
    clickDecay=float(std::exp(-1/(sr*.012)));inputCapturing.store(false);
    keyboardDisplayReset.store(true);rate=sr; engine.prepare(sr); tape.prepare(sr); clock=0; step=0; seqNote=-1; sequenceNote.store(-1);heldNotes.clear();previousSeqMode=-1;wasSequencing=false;lastHostStep=INT64_MIN;
}
double PocketProcessor::getTailLengthSeconds() const {
    const double feedback=std::clamp(double(state.getRawParameterValue("delayFeedback")->load()),.01,.85);
    const double echo=state.getRawParameterValue("delayTime")->load()*std::log(.00001)/std::log(feedback);
    const double release=state.getRawParameterValue("release")->load(),macro=state.getRawParameterValue("envelope")->load();
    const double masterFeedback=std::clamp(double(state.getRawParameterValue("masterFeedback")->load()),.001,.85);
    const double masterTail=state.getRawParameterValue("masterEcho")->load()>0?state.getRawParameterValue("masterDelayTime")->load()*std::log(.00001)/std::log(masterFeedback):0;
    double envelopeTail=(.008+release*release*5*(.5+macro))*12;
    if(phase246EnvelopeEnabled()||digital246EnvelopeEnabled()){
        const int index=127-std::clamp(int(phaseAudio[digital246EnvelopeEnabled()?19:9]->load())/128,0,127);
        const double coefficient=pocket::reference::adsr246Decay[size_t(index)]/2147483648.;
        envelopeTail=std::log(16./1073741824.)/std::log(coefficient)/88200.+128./88200.;
    }
    return envelopeTail+(state.getRawParameterValue("effect")->load()>0?echo:0)+masterTail;
}
bool PocketProcessor::isBusesLayoutSupported(const BusesLayout& b) const {return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()&&(b.getMainInputChannelSet().isDisabled()||b.getMainInputChannelSet()==juce::AudioChannelSet::mono()||b.getMainInputChannelSet()==juce::AudioChannelSet::stereo());}
void PocketProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    auto get=[this](Param id){return audioParameters[size_t(id)]->load(std::memory_order_relaxed);};
    const int seqMode=int(get(Param::seqMode));
    const bool arp=get(Param::sequencer)>.5f&&seqMode>=4;
    if(previousSeqMode!=seqMode){engine.allNotesOff();seqNote=-1;sequenceNote.store(-1);lastHostStep=INT64_MIN;previousSeqMode=seqMode;}
    if(panicRequested.exchange(false)) {engine.reset();if(panicOutputRequested.exchange(false))resetOutputHistory();heldNotes.clear();seqNote=-1;sequenceNote.store(-1);}
    pocket::NoteEvent key;
    if(keyboardOverflow.exchange(false)) {engine.allNotesOff();heldNotes.clear();seqNote=-1;sequenceNote.store(-1);for(int i=0;i<1024&&uiToAudio.pop(key);++i) {}keyboardDisplayReset.store(true);}
    for(int i=0;i<1024&&uiToAudio.pop(key);++i) {heldNotes.set(key.channel,key.note,key.on);if(key.on){if(!arp)engine.noteOn(key.note,key.velocity,key.channel);lastNote.store(key.note);}else if(!arp)engine.noteOff(key.note,key.channel);}
    pocket::Parameters p;
    p.phase246=phaseAudio[0]->load(std::memory_order_relaxed)>.5f;for(size_t i=0;i<4;++i)p.phase246Controls[i]=int16_t(phaseAudio[i+1]->load(std::memory_order_relaxed));
    p.phase246Envelope=phaseAudio[5]->load(std::memory_order_relaxed)>.5f;for(size_t i=0;i<4;++i)p.phase246Adsr[i]=int16_t(phaseAudio[i+6]->load(std::memory_order_relaxed));
    p.digital246=phaseAudio[10]->load(std::memory_order_relaxed)>.5f;for(size_t i=0;i<4;++i)p.digital246Controls[i]=int16_t(phaseAudio[i+11]->load(std::memory_order_relaxed));
    p.digital246Envelope=phaseAudio[15]->load(std::memory_order_relaxed)>.5f;for(size_t i=0;i<4;++i)p.digital246Adsr[i]=int16_t(phaseAudio[i+16]->load(std::memory_order_relaxed));
    p.engine=int(get(Param::engine)); p.tone=get(Param::tone); p.color=get(Param::color); p.envelope=get(Param::envelope); p.effect=get(Param::effect); p.volume=1;
    p.attack=get(Param::attack); p.decay=get(Param::decay); p.sustain=get(Param::sustain); p.release=get(Param::release);
    p.lfoRate=get(Param::lfoRate); p.lfoDepth=get(Param::lfoDepth); p.lfoDestination=int(get(Param::lfoDestination)); p.lfoShape=int(get(Param::lfoShape));
    p.delayTime=get(Param::delayTime); p.feedback=get(Param::delayFeedback); p.drive=get(Param::drive); p.sampleLoop=get(Param::sampleLoop)>.5f;p.sampleRoot=get(Param::sampleRoot);p.sampleClean=get(Param::sampleClean)>.5f;
    if(p.engine==5)for(size_t pad=0;pad<p.pads.size();++pad){const auto& source=padParameters[pad];auto& target=p.pads[pad];target.start=source[0]->load(std::memory_order_relaxed);target.end=source[1]->load(std::memory_order_relaxed);target.gain=source[2]->load(std::memory_order_relaxed);target.pitch=source[3]->load(std::memory_order_relaxed);target.reverse=source[4]->load(std::memory_order_relaxed)>.5f;target.gated=source[5]->load(std::memory_order_relaxed)>.5f;}
    const float outputTarget=get(Param::volume);
    bool running=get(Param::sequencer)>.5f;
    double bpm=get(Param::tempo),ppq=0; bool hostPosition=false,hostPlaying=true;
    if(auto* host=getPlayHead()) if(auto pos=host->getPosition()) {
        if(auto tempo=pos->getBpm()) bpm=*tempo;
        hostPlaying=pos->getIsPlaying(); running=running&&hostPlaying;
        if(auto beat=pos->getPpqPosition()) {ppq=*beat; hostPosition=true;}
    }
    bpm=std::isfinite(bpm)?std::clamp(bpm,40.0,300.0):110;effectiveTempo.store(float(bpm),std::memory_order_relaxed);
    if(running&&!wasSequencing) {clock=0;step=0;lastHostStep=INT64_MIN;if(arp)engine.allNotesOff();}
    if(!running&&wasSequencing) {if(seqNote>=0) engine.noteOff(seqNote,17); seqNote=-1;sequenceNote.store(-1);playhead.store(-1);lastHostStep=INT64_MIN;}
    wasSequencing=running;
    const int seqLength=std::clamp(int(get(Param::seqLength)),1,16),arpOctaves=int(get(Param::arpOctaves));
    const double seqDivision=double(1<<std::clamp(int(get(Param::seqRate)),0,3)),seqSwing=get(Param::seqSwing),seqGate=get(Param::seqGate);
    const float seqVelocity=get(Param::seqVelocity);
    const bool euclidean=get(Param::seqEuclidean)>.5f;const int seqPulses=int(get(Param::seqPulses)),seqRotation=int(get(Param::seqRotation));
    hostPosition=hostPosition&&std::isfinite(ppq)&&std::abs(ppq)<1e10;
    const std::array<float,4> gains{get(Param::track1),get(Param::track2),get(Param::track3),get(Param::track4)};
    const bool tapePlaying=get(Param::tapePlay)>.5f&&hostPlaying,tapeRecording=get(Param::tapeRecord)>.5f;
    const bool overdub=get(Param::tapeOverdub)>.5f; const int selectedTrack=int(get(Param::tapeTrack)); const float speed=get(Param::tapeSpeed);
    const float tapeSeconds=float(4<<int(get(Param::tapeLength)));
    const float seek=pendingTapeSeek.exchange(-1);if(seek>=0)tape.seek(seek);
    tape.setRegion(get(Param::tapeLoop)>.5f,get(Param::tapeLoopIn),get(Param::tapeLoopOut));tape.setChop(tapeChop.load(),bpm);
    const float spool=tapeSpool.load();const bool breaking=tapeBreak.load();const bool looping=get(Param::tapeLoop)>.5f;
    const std::array<float,4> playbackGains=breaking?std::array<float,4>{0,0,0,0}:gains;
    const bool clickOn=get(Param::metronome)>.5f;
    const bool capture=inputCapturing.load();const int inputChannels=std::min(getTotalNumInputChannels(),buffer.getNumChannels());
    const bool monitor=get(Param::inputMonitor)>.5f;const float inputGain=get(Param::inputGain),recordGain=get(Param::tapeRecordGain);
    const std::array<float,3> eqTargets{float(std::pow(10.,get(Param::eqLow)/20)),float(std::pow(10.,get(Param::eqMid)/20)),float(std::pow(10.,get(Param::eqHigh)/20))};
    const float eqMix=get(Param::eqMix),masterDrive=get(Param::masterDrive),masterEcho=get(Param::masterEcho),masterTime=get(Param::masterDelayTime),masterFeedback=get(Param::masterFeedback),clickVolume=get(Param::clickVolume);
    float blockPeak=0;
    auto it=midi.cbegin();
    for(int i=0;i<buffer.getNumSamples();++i) {
        while(it!=midi.cend()&&(*it).samplePosition<=i) {
            const auto metadata=*it;if(metadata.numBytes>3){++it;continue;}const auto m=metadata.getMessage(); const int ch=m.getChannel();
            if(m.isNoteOn()) {heldNotes.set(ch,m.getNoteNumber(),true);if(!arp)engine.noteOn(m.getNoteNumber(),m.getFloatVelocity(),ch); lastNote.store(m.getNoteNumber());if(!audioToUi.push({ch,m.getNoteNumber(),m.getFloatVelocity(),true}))keyboardDisplayReset.store(true);}
            else if(m.isNoteOff()) {heldNotes.set(ch,m.getNoteNumber(),false);if(!arp)engine.noteOff(m.getNoteNumber(),ch);if(!audioToUi.push({ch,m.getNoteNumber(),0,false}))keyboardDisplayReset.store(true);}
            else if(m.isPitchWheel()) engine.pitchBend(ch,float(m.getPitchWheelValue()-8192)/8192.f*2);
            else if(m.isController()&&m.getControllerNumber()==64) engine.sustainPedal(ch,m.getControllerValue()>=64);
            else if(m.isAllSoundOff()) {engine.reset();resetOutputHistory();heldNotes.clear();seqNote=-1;sequenceNote.store(-1);keyboardDisplayReset.store(true);}
            else if(m.isAllNotesOff()) {engine.allNotesOff(ch);heldNotes.clear(ch);if(arp&&seqNote>=0){engine.noteOff(seqNote,17);seqNote=-1;sequenceNote.store(-1);}keyboardDisplayReset.store(true);}
            ++it;
        }
        if(running) {
            const double beat=hostPosition?ppq+double(i)*bpm/(60*rate):clock;
            const auto timing=pocket::sequenceTiming(beat,seqDivision,seqSwing);
            if(timing.tick!=lastHostStep) {
                if(seqNote>=0)engine.noteOff(seqNote,17);
                const int index=pocket::sequenceIndex(timing.tick,seqLength,seqMode);
                seqNote=arp?heldNotes.note(timing.tick,seqMode-4,arpOctaves):steps[size_t(index)].load();
                if(euclidean&&!pocket::euclideanPulse(timing.tick,seqLength,seqPulses,seqRotation))seqNote=-1;
                if(seqNote>=0)engine.noteOn(seqNote,seqVelocity,17);
                sequenceNote.store(seqNote);playhead.store(index);lastHostStep=timing.tick;
            }
            if(seqNote>=0&&(timing.phase>=seqGate||(arp&&heldNotes.empty()))){engine.noteOff(seqNote,17);seqNote=-1;sequenceNote.store(-1);}
            if(!hostPosition)clock+=bpm/(60*rate);
        }
        float external=0;if((monitor||capture)&&inputChannels>0){external=buffer.getSample(0,i);if(inputChannels>1)external=(external+buffer.getSample(1,i))*.5f;external*=inputGain;}
        if(capture&&captureWrite<int(inputCapture.size()))inputCapture[size_t(captureWrite++)]=std::clamp(external,-1.f,1.f);
        const float live=engine.next(p)+(monitor?external:0);
        float mixed=tape.process(live,(tapePlaying&&(!breaking||looping))||spool!=0,tapeRecording&&spool==0,overdub,selectedTrack,spool!=0?spool:speed,tapeSeconds,playbackGains,recordGain);
        if(clickOn&&(!hostPosition||hostPlaying)) {const double beat=hostPosition?ppq+double(i)*bpm/(60*rate):clickBeat;const auto integer=int64_t(std::floor(beat));if(integer!=lastClickBeat){lastClickBeat=integer;clickEnvelope=clickVolume;clickPhase=0;}const double frequency=integer%4==0?1600:1100;clickPhase+=frequency/rate;mixed+=float(std::sin(2*pocket::pi*clickPhase))*clickEnvelope;clickEnvelope*=clickDecay;clickBeat+=bpm/(60*rate);}else {lastClickBeat=-1;clickEnvelope=0;}
        eqLowState+=eqLowCoefficient*(mixed-eqLowState);eqHighState+=eqHighCoefficient*(mixed-eqHighState);
        for(size_t band=0;band<3;++band)eqGains[band]+=outputSmoothing*(eqTargets[band]-eqGains[band]);eqMixSmooth+=outputSmoothing*(eqMix-eqMixSmooth);
        const float equalized=eqLowState*eqGains[0]+(eqHighState-eqLowState)*eqGains[1]+(mixed-eqHighState)*eqGains[2];mixed+=(equalized-mixed)*eqMixSmooth;
        masterDriveSmooth+=outputSmoothing*(masterDrive-masterDriveSmooth);masterEchoSmooth+=outputSmoothing*(masterEcho-masterEchoSmooth);masterTimeSmooth+=outputSmoothing*(masterTime-masterTimeSmooth);
        if(masterDriveSmooth>.00001f)mixed=std::tanh(mixed*(1+masterDriveSmooth*12))/(1+masterDriveSmooth*2);
        const size_t lag=std::clamp(size_t(rate*masterTimeSmooth),size_t(1),masterDelay.size()-1);const float echo=lag<=masterDelayValid?masterDelay[(masterDelayWrite+masterDelay.size()-lag)%masterDelay.size()]:0;
        masterDelay[masterDelayWrite]=std::tanh(mixed+echo*masterFeedback);masterDelayWrite=(masterDelayWrite+1)%masterDelay.size();if(masterDelayValid<masterDelay.size())++masterDelayValid;mixed+=echo*masterEchoSmooth;
        outputGain+=outputSmoothing*(outputTarget-outputGain);
        const float value=mixed*outputGain;blockPeak=std::max(blockPeak,std::abs(value));
        for(int ch=0;ch<buffer.getNumChannels();++ch) buffer.setSample(ch,i,value);
        if(++scopeCounter>=8) {scopeCounter=0; scope[size_t(scopeIndex)].store(value,std::memory_order_relaxed); scopeIndex=(scopeIndex+1)%256;}
    }
    if(capture)inputCaptureFrames.store(captureWrite);
    const auto visual=engine.visualState(p.engine);
    visualEnvelope.store(visual.envelope,std::memory_order_relaxed);visualSamplePosition.store(visual.progress,std::memory_order_relaxed);
    visualLfoPhase.store(visual.lfo,std::memory_order_relaxed);visualVoices.store(visual.active,std::memory_order_relaxed);visualEnvelopeStage.store(visual.stage,std::memory_order_relaxed);
    visualTrigger.store(visual.trigger,std::memory_order_relaxed);visualLevel.store(blockPeak,std::memory_order_relaxed);
    processedTime+=buffer.getNumSamples()/rate;visualTime.store(float(processedTime),std::memory_order_relaxed);
    tapePosition.store(float(tape.getPosition())); midi.clear();
}
void PocketProcessor::publishSamplePreview(const pocket::Sample& sample) {
    for(size_t bin=0;bin<samplePreview.size();++bin) {
        const size_t first=sample.frames.size()*bin/samplePreview.size(),last=sample.frames.size()*(bin+1)/samplePreview.size();float peak=0;
        for(size_t i=first;i<last;++i) peak=std::max(peak,std::abs(sample.frames[i]));
        samplePreview[bin].store(peak,std::memory_order_relaxed);
    }
}
pocket::Sample PocketProcessor::readSample(const juce::File& file,juce::String& error) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<1) {error="Cannot read this WAV/AIFF file (8-192 kHz required).";return {};}
    const int length=int(std::min(reader->lengthInSamples,juce::int64(reader->sampleRate*16)));
    juce::AudioBuffer<float> audio(2,length);
    if(!reader->read(&audio,0,length,0,true,true)) {error="Audio file could not be decoded.";return {};}
    pocket::Sample data; data.rate=reader->sampleRate; data.frames.resize(size_t(length));
    for(int i=0;i<length;++i) data.frames[size_t(i)]=reader->numChannels>1?(audio.getSample(0,i)+audio.getSample(1,i))*.5f:audio.getSample(0,i);
    return data;
}
bool PocketProcessor::loadSample(const juce::File& file,juce::String& error) {
    auto data=readSample(file,error);if(data.frames.empty()) return false;
    const int length=int(data.frames.size());publishSamplePreview(data);
    {const juce::ScopedLock lock(getCallbackLock()); engine.swapSample(data); sampleFrames.store(length);}
    for(auto pair:{std::pair{"tone",0.f},{"color",1.f},{"engine",3.f},{"sampleRoot",60.f}}) {auto* parameter=state.getParameter(pair.first); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(pair.second)); parameter->endChangeGesture();}
    setSelectedSound({});currentFactoryPreset.store(-1);currentUserPreset.store(-1);return true;
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
juce::StringArray PocketProcessor::factoryPresetNames() {
    juce::StringArray names;for(const auto& preset:pocket::factoryPresets) names.add(preset.name);return names;
}
pocket::Sample PocketProcessor::factorySample(const juce::String& name) const {
    int size=0;const char* bytes=FactoryData::getNamedResource(name.toRawUTF8(),size);
    if(!bytes||size<1) return {};
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::MemoryInputStream(bytes,size,false),true));
    if(!reader) return {};
    juce::AudioBuffer<float> audio(1,int(reader->lengthInSamples));
    if(!reader->read(&audio,0,audio.getNumSamples(),0,true,false)) return {};
    pocket::Sample result;result.rate=reader->sampleRate;
    result.frames.assign(audio.getReadPointer(0),audio.getReadPointer(0)+audio.getNumSamples());return result;
}
void PocketProcessor::loadFactoryKit(int index) {
    const char* kits[]={"Studio","Circuit","Dust"};
    const char* hits[]={"kick","snare","closed_hat","open_hat","tom","clap","crash","rim"};
    if(index<0||index>2) return;
    pocket::Kit kit;
    for(size_t i=0;i<kit.size();++i) kit[i]=factorySample(juce::String(kits[(index+int(i/8))%3])+"_"+hits[i%8]+"_wav");
    {const juce::ScopedLock lock(libraryStateLock);for(int pad=0;pad<pocket::kitCapacity;++pad)kitNames[size_t(pad)]=juce::String(kits[(index+pad/8)%3])+" / "+hits[pad%8];}
    for(int pad=0;pad<pocket::kitCapacity;++pad)publishPadPreview(pad,kit[size_t(pad)]);
    const juce::ScopedLock lock(getCallbackLock());engine.swapKit(kit,24);kitPads.store(24);
}
void PocketProcessor::applyPreset(int index) {
    if(index<0||index>=int(pocket::factoryPresets.size())) return;
    phaseState->getParameter("phase246Enabled")->setValueNotifyingHost(0);phaseState->getParameter("phase246Envelope")->setValueNotifyingHost(0);
    phaseState->getParameter("digital246Enabled")->setValueNotifyingHost(0);phaseState->getParameter("digital246Envelope")->setValueNotifyingHost(0);
    const auto& p=pocket::factoryPresets[size_t(index)];
    // Decode factory assets off the audio thread; only swap under the callback lock.
    if(p.sample>=0) {
        const char* names[]={"VelvetKeys_wav","GlassMallet_wav","AirChoir_wav","LowPluck_wav"};
        auto data=factorySample(names[p.sample]);const int frames=int(data.frames.size());publishSamplePreview(data);
        const juce::ScopedLock lock(getCallbackLock());engine.swapSample(data);sampleFrames.store(frames);
    }
    if(p.kit>=0){loadFactoryKit(p.kit);for(int pad=0;pad<pocket::kitCapacity;++pad)for(const auto* field:{"start","end","gain","pitch","reverse","gated"}){auto* parameter=state.getParameter(padParameterId(pad,field));parameter->setValueNotifyingHost(parameter->getDefaultValue());}}
    const std::pair<const char*,float> values[]={
        {"engine",float(p.engine)},{"tone",p.tone},{"color",p.color},{"envelope",p.envelope},{"effect",p.effect},
        {"attack",p.attack},{"release",p.release},{"decay",p.decay},{"sustain",p.sustain},
        {"lfoRate",p.lfoRate},{"lfoDepth",p.lfoDepth},{"lfoDestination",0},{"lfoShape",0},
        {"sampleRoot",60},{"drive",p.drive},{"delayTime",.281f},{"delayFeedback",.42f},{"sampleLoop",0},{"sampleClean",0}};
    for(const auto& value:values) {auto* parameter=state.getParameter(value.first);parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(value.second));parameter->endChangeGesture();}
    setSelectedSound("factory:"+juce::String(index));currentFactoryPreset.store(index);currentUserPreset.store(-1);requestPanic(false);
}

void PocketProcessor::applyPhase246Preset(int index) {
    if(index<0||index>=int(pocket::phase246Presets.size()))return;
    // Recover the first four stored envelope controls; FX/LFO/octave stay separate.
    applyPreset(2);
    for(auto item:{std::pair{"tone",.5f},{"color",.5f},{"envelope",.5f},{"effect",0.f},{"attack",.005f},{"decay",.2f},{"sustain",1.f},{"release",.2f},{"drive",0.f},{"lfoDepth",0.f}}){auto* p=state.getParameter(item.first);p->setValueNotifyingHost(p->convertTo0to1(item.second));}
    for(size_t i=0;i<4;++i){auto* p=phaseState->getParameter(pocket::phase246Ids[i]);p->setValueNotifyingHost(p->convertTo0to1(pocket::phase246Presets[size_t(index)].controls[i]));}
    for(size_t i=0;i<4;++i){auto* p=phaseState->getParameter(pocket::phase246EnvelopeIds[i]);p->setValueNotifyingHost(p->convertTo0to1(pocket::phase246Envelopes[size_t(index)][i]));}
    phaseState->getParameter("phase246Enabled")->setValueNotifyingHost(1);phaseState->getParameter("phase246Envelope")->setValueNotifyingHost(1);
    setSelectedSound("phase246:"+juce::String(index));currentFactoryPreset.store(-1);currentUserPreset.store(-1);requestPanic(false);
}
void PocketProcessor::applyDigital246Preset(int index) {
    if(index<0||index>=int(pocket::digital246Presets.size()))return;
    // Recover the first four stored envelope controls; FX/LFO/octave stay separate.
    applyPreset(0);
    for(auto item:{std::pair{"tone",.5f},{"color",.5f},{"envelope",.5f},{"effect",0.f},{"attack",.005f},{"decay",.2f},{"sustain",1.f},{"release",.2f},{"drive",0.f},{"lfoDepth",0.f}}){auto* p=state.getParameter(item.first);p->setValueNotifyingHost(p->convertTo0to1(item.second));}
    for(size_t i=0;i<4;++i){auto* p=phaseState->getParameter(pocket::digital246Ids[i]);p->setValueNotifyingHost(p->convertTo0to1(pocket::digital246Presets[size_t(index)].controls[i]));}
    for(size_t i=0;i<4;++i){auto* p=phaseState->getParameter(pocket::digital246EnvelopeIds[i]);p->setValueNotifyingHost(p->convertTo0to1(pocket::digital246Presets[size_t(index)].adsr[i]));}
    phaseState->getParameter("digital246Enabled")->setValueNotifyingHost(1);phaseState->getParameter("digital246Envelope")->setValueNotifyingHost(1);
    setSelectedSound("digital246:"+juce::String(index));currentFactoryPreset.store(-1);currentUserPreset.store(-1);requestPanic(false);
}
void PocketProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree=state.copyState();tree.removeChild(tree.getChildWithName("Phase246"),nullptr);tree.addChild(phaseState->copyState(),-1,nullptr);tree.setProperty("screenPage",screenPage(),nullptr);tree.setProperty("screenTape",screenTapeMode(),nullptr);tree.removeChild(tree.getChildWithName("TapeEdits"),nullptr);tree.addChild(tapeEditSnapshot(),-1,nullptr);tree.removeChild(tree.getChildWithName("SoundLibrary"),nullptr);tree.addChild(librarySnapshot(),-1,nullptr); for(int i=0;i<16;++i) tree.setProperty("step"+juce::String(i),steps[size_t(i)].load(),nullptr);
    {const juce::ScopedLock lock(libraryStateLock);for(int pad=0;pad<pocket::kitCapacity;++pad)tree.setProperty("kitName"+juce::String(pad),kitNames[size_t(pad)],nullptr);}
    pocket::Kit kitCopy;
    juce::AudioBuffer<float> sampleAudio,tapeAudio; double sampleRate=48000,tapeRate=48000;
    {const juce::ScopedLock lock(getCallbackLock()); kitCopy=engine.getKit();tree.setProperty("kitCount",engine.getKitCount(),nullptr);const auto& sample=engine.getSample(); sampleRate=sample.rate;
        if(!sample.frames.empty()) {sampleAudio.setSize(1,int(sample.frames.size())); sampleAudio.copyFrom(0,0,sample.frames.data(),sampleAudio.getNumSamples());}
        const auto& tracks=tape.getTracks(); tapeRate=tape.getRate();
        if(!tracks[0].empty()) {tapeAudio.setSize(4,int(tracks[0].size())); for(int c=0;c<4;++c) tapeAudio.copyFrom(c,0,tracks[size_t(c)].data(),tapeAudio.getNumSamples());}
    }
    if(sampleAudio.getNumSamples()>0) tree.setProperty("sampleAudio",encodeAudio(sampleAudio,sampleRate),nullptr);
    tree.setProperty("factoryPreset",currentFactoryPreset.load(),nullptr);
    for(size_t i=0;i<kitCopy.size();++i) if(!kitCopy[i].frames.empty()) {
        juce::AudioBuffer<float> hit(1,int(kitCopy[i].frames.size()));hit.copyFrom(0,0,kitCopy[i].frames.data(),hit.getNumSamples());
        tree.setProperty("kitAudio"+juce::String(int(i)),encodeAudio(hit,kitCopy[i].rate),nullptr);
    }
    if(tapeAudio.getNumSamples()>0) tree.setProperty("tapeAudio",encodeAudio(tapeAudio,tapeRate),nullptr);
    if(auto xml=tree.createXml()) copyXmlToBinary(*xml,dest);
}
juce::ValueTree PocketProcessor::normalizedPhaseState(juce::ValueTree tree) const {
    if(!tree.isValid())tree=juce::ValueTree("Phase246");
    const char* ids[]={"phase246Enabled","phase246Blue","phase246Green","phase246White","phase246Orange","phase246Envelope","phase246Attack","phase246Decay","phase246Sustain","phase246Release","digital246Enabled","digital246Blue","digital246Green","digital246White","digital246Orange","digital246Envelope","digital246Attack","digital246Decay","digital246Sustain","digital246Release"};
    for(const char* id:ids)if(!tree.getChildWithProperty("id",id).isValid()){
        juce::ValueTree child("PARAM");child.setProperty("id",id,nullptr);auto* p=phaseState->getParameter(id);
        child.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);tree.addChild(child,-1,nullptr);
    }
    return tree;
}
void PocketProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        currentUserPreset.store(-1);inputCapturing.store(false);tapeSpool.store(0);tapeBreak.store(false);tapeChop.store(false);
        auto tree=juce::ValueTree::fromXml(*xml);auto phase=tree.getChildWithName("Phase246").createCopy();tree.removeChild(tree.getChildWithName("Phase246"),nullptr);
        phase=normalizedPhaseState(phase);
        const auto edits=tree.getChildWithName("TapeEdits").createCopy();tree.removeChild(tree.getChildWithName("TapeEdits"),nullptr);restoreLibrary(tree.getChildWithName("SoundLibrary"));tree.removeChild(tree.getChildWithName("SoundLibrary"),nullptr);
        juce::AudioBuffer<float> sampleAudio,tapeAudio; double sampleRate=48000,tapeRate=48000;
        const bool hasSample=decodeAudio(tree.getProperty("sampleAudio").toString(),sampleAudio,sampleRate,1);
        const bool hasTape=decodeAudio(tree.getProperty("tapeAudio").toString(),tapeAudio,tapeRate,4)&&tapeAudio.getNumChannels()==4;
        if(auto record=tree.getChildWithProperty("id","tapeRecord");record.isValid()) record.setProperty("value",0.f,nullptr);
        // Encoded payloads belong to the state snapshot, not the live parameter tree.
        pocket::Kit restoredKit;
        bool hasKit=false;
        for(size_t i=0;i<restoredKit.size();++i) {
            const auto key="kitAudio"+juce::String(int(i));juce::AudioBuffer<float> hit;double sr=32000;
            if(decodeAudio(tree.getProperty(key).toString(),hit,sr,1)) {
                restoredKit[i].rate=sr;restoredKit[i].frames.assign(hit.getReadPointer(0),hit.getReadPointer(0)+hit.getNumSamples());hasKit=true;
            }
            tree.removeProperty(key,nullptr);
        }
        for(const char* id:{"seqMode","seqLength","seqRate","seqGate","seqSwing","arpOctaves","seqVelocity","seqEuclidean","seqPulses","seqRotation","sampleClean"})
            if(!tree.getChildWithProperty("id",id).isValid()){juce::ValueTree parameter("PARAM");parameter.setProperty("id",id,nullptr);auto* p=state.getParameter(id);parameter.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);tree.addChild(parameter,-1,nullptr);}
        {const juce::ScopedLock lock(libraryStateLock);for(int pad=0;pad<pocket::kitCapacity;++pad)kitNames[size_t(pad)]=tree.getProperty("kitName"+juce::String(pad),"Pad "+juce::String(pad+1)).toString().substring(0,160);}
        for(int pad=0;pad<pocket::kitCapacity;++pad)publishPadPreview(pad,restoredKit[size_t(pad)]);
        const int selected=int(tree.getProperty("factoryPreset",-1));
        currentFactoryPreset.store(selected>=0&&selected<int(pocket::factoryPresets.size())?selected:-1);
        tree.removeProperty("factoryPreset",nullptr);
        tree.removeProperty("sampleAudio",nullptr); tree.removeProperty("tapeAudio",nullptr);
        pocket::Sample sample; sample.rate=sampleRate;
        if(hasSample) sample.frames.assign(sampleAudio.getReadPointer(0),sampleAudio.getReadPointer(0)+sampleAudio.getNumSamples());
        publishSamplePreview(sample);
        std::array<std::vector<float>,4> tracks;
        if(hasTape) for(int c=0;c<4;++c) tracks[size_t(c)].assign(tapeAudio.getReadPointer(c),tapeAudio.getReadPointer(c)+tapeAudio.getNumSamples());
        {const juce::ScopedLock lock(getCallbackLock()); if(hasKit) engine.swapKit(restoredKit,int(tree.getProperty("kitCount",8)));kitPads.store(engine.getKitCount());engine.swapSample(sample); sampleFrames.store(hasSample?sampleAudio.getNumSamples():0);
            if(hasTape) {tape.restore(std::move(tracks),tapeRate); tape.prepare(rate);} else {tape.prepare(rate); for(int c=0;c<4;++c) tape.clear(c);}
            for(int i=0;i<16;++i) steps[size_t(i)].store(juce::jlimit(-1,127,int(tree.getProperty("step"+juce::String(i),-1))));
            for(int pad=0;pad<pocket::kitCapacity;++pad)for(const auto* field:{"start","end","gain","pitch","reverse","gated"}){const auto id=padParameterId(pad,field);if(!tree.getChildWithProperty("id",id).isValid()){juce::ValueTree parameter("PARAM");parameter.setProperty("id",id,nullptr);auto* p=state.getParameter(id);parameter.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);tree.addChild(parameter,-1,nullptr);}}
            phaseState->replaceState(phase);state.replaceState(tree);engine.reset();resetOutputHistory();heldNotes.clear();seqNote=-1;sequenceNote.store(-1);wasSequencing=false;lastHostStep=INT64_MIN;
        }
        restoreTapeEdits(edits);
        setScreenPage(int(tree.getProperty("screenPage",0)),bool(tree.getProperty("screenTape",false)));
    }
}
juce::AudioProcessorEditor* PocketProcessor::createEditor() {return new PocketEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new PocketProcessor();}

juce::String PocketProcessor::padParameterId(int pad,const char* field){return "pad"+juce::String(pad+1)+"_"+field;}
bool PocketProcessor::loadKitPad(int pad,const juce::File& file,juce::String& error){if(pad<0||pad>=pocket::kitCapacity)return false;auto sample=readSample(file,error);if(sample.frames.empty())return false;publishPadPreview(pad,sample);{const juce::ScopedLock lock(libraryStateLock);kitNames[size_t(pad)]=file.getFileName();}{const juce::ScopedLock lock(getCallbackLock());engine.swapKitPad(pad,sample);kitPads.store(engine.getKitCount());}setSelectedSound({});currentFactoryPreset.store(-1);currentUserPreset.store(-1);return true;}
void PocketProcessor::publishPadPreview(int pad,const pocket::Sample& sample){auto& preview=padPreviews[size_t(pad)];for(size_t bin=0;bin<preview.size();++bin){float peak=0;for(size_t i=sample.frames.size()*bin/preview.size();i<sample.frames.size()*(bin+1)/preview.size();++i)peak=std::max(peak,std::abs(sample.frames[i]));preview[bin].store(peak,std::memory_order_relaxed);}}
std::array<float,128> PocketProcessor::kitPadPreview(int pad){std::array<float,128> out{};if(pad>=0&&pad<pocket::kitCapacity)for(size_t i=0;i<out.size();++i)out[i]=padPreviews[size_t(pad)][i].load(std::memory_order_relaxed);return out;}

juce::String PocketProcessor::kitPadName(int pad) const{if(pad<0||pad>=pocket::kitCapacity)return {};const juce::ScopedLock lock(libraryStateLock);return kitNames[size_t(pad)];}
