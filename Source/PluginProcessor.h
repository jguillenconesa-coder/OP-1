// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Tape.h"
#include "NoteQueue.h"
#include "SoundLibrary.h"
#include "Sequencer.h"
class PocketProcessor : public juce::AudioProcessor,private juce::MidiKeyboardStateListener {
public:
    explicit PocketProcessor(juce::File libraryDirectory={});
    ~PocketProcessor() override;
    void updateKeyboardDisplay();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Pocket One"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool loadSample(const juce::File&,juce::String& error);
    bool exportTape(const juce::File&,juce::String& error);
    void clearTape();
    void rewindTape();
    void requestPanic() { panicRequested.store(true); }
    void applyPreset(int index);
    static juce::StringArray factoryPresetNames();
    std::atomic<int> currentFactoryPreset{-1},currentUserPreset{-1};
    juce::StringArray userPresetNames() const;
    std::vector<SoundInfo> soundCatalog() const;
    bool loadSound(const juce::String& id,juce::String& error);
    juce::String selectedSound() const;
    juce::String lastSound(bool drums) const;
    bool isFavorite(const juce::String&) const;
    void toggleFavorite(const juce::String&);
    bool assignSoundSlot(bool drums,int slot);
    bool recallSoundSlot(bool drums,int slot,juce::String& error);
    juce::String soundSlot(bool drums,int slot) const;
    bool importSamples(const juce::Array<juce::File>&,juce::String& error);
    std::atomic<int> clipboardKind{0};
    bool copySound(juce::String& error);
    bool pasteSound(juce::String& error);
    bool tapeEdit(const juce::String& action,bool shifted,juce::String& error);
    std::pair<float,float> currentTapeTake();
    bool slideTape(double delta,juce::String& error);
    void seekTape(double seconds) {pendingTapeSeek.store(float(seconds));}
    std::atomic<float> tapeSpool{0};
    std::atomic<bool> tapeBreak{false},tapeChop{false};
    void tapeMemo(int slot,bool store);
    bool startInputCapture(juce::String& error);
    bool finishInputCapture(juce::String& error);
    std::atomic<bool> inputCapturing{false};
    std::atomic<int> inputCaptureFrames{0};
    bool exportMix(const juce::File&,juce::String& error);
    bool importUserBank(const juce::File&,juce::String& error);
    bool applyUserPreset(int index,juce::String& error);
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::array<std::atomic<float>,256> scope{};
    std::atomic<float> effectiveTempo{110};
    std::atomic<int> playhead{-1},lastNote{60},sequenceNote{-1};
    std::atomic<float> tapePosition{0};
    std::atomic<int> sampleFrames{0};
    std::array<std::atomic<float>,128> samplePreview{};
    std::atomic<float> visualEnvelope{0},visualSamplePosition{0},visualLfoPhase{0},visualTime{0},visualLevel{0};
    std::atomic<int> visualVoices{0},visualEnvelopeStage{0};
    std::atomic<uint64_t> visualTrigger{0};
    std::array<std::atomic<int>,16> steps{};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    enum class Param {engine,tone,color,envelope,effect,attack,decay,sustain,release,lfoRate,lfoDepth,lfoDestination,lfoShape,delayTime,delayFeedback,drive,sampleLoop,sampleRoot,volume,sequencer,tempo,track1,track2,track3,track4,tapePlay,tapeRecord,tapeOverdub,tapeTrack,tapeSpeed,tapeLength,tapeLoop,tapeLoopIn,tapeLoopOut,metronome,inputMonitor,inputGain,tapeRecordGain,eqLow,eqMid,eqHigh,eqMix,masterDrive,masterEcho,masterDelayTime,masterFeedback,clickVolume,seqMode,seqLength,seqRate,seqGate,seqSwing,arpOctaves,seqVelocity,seqEuclidean,seqPulses,seqRotation,Count};
    std::array<std::atomic<float>*,size_t(Param::Count)> audioParameters{};
    void handleNoteOn(juce::MidiKeyboardState*,int,int,float) override;
    void handleNoteOff(juce::MidiKeyboardState*,int,int,float) override;
    pocket::NoteQueue uiToAudio,audioToUi;
    std::atomic<bool> keyboardOverflow{false},keyboardDisplayReset{false};
    bool reflectingHostNotes=false;
    struct UserPreset {juce::String name,id,bank,source;juce::File sample;std::array<juce::File,8> kit;juce::var parameters;bool isKit=false;};
    mutable juce::CriticalSection libraryStateLock;
    juce::String selectedSoundId,lastSynthSound="factory:0",lastDrumSound="factory:22";
    juce::StringArray favorites;
    std::array<std::array<juce::String,8>,2> soundSlotIds;
    std::array<std::array<juce::ValueTree,8>,2> slotSettings;
    juce::ValueTree soundClipboard;juce::String soundClipboardId;
    juce::ValueTree librarySnapshot() const;
    void restoreLibrary(const juce::ValueTree&);
    void setSelectedSound(const juce::String&);
    void initializeSoundSlots();
    juce::ValueTree tapeEditSnapshot();
    void restoreTapeEdits(const juce::ValueTree&);
    std::atomic<float> pendingTapeSeek{-1};
    std::array<std::array<float,21>,2> tapeMemos{};
    std::array<bool,2> memoSaved{};
    juce::File libraryDirectory;
    std::vector<UserPreset> userPresets;
    void refreshUserPresets();
    void publishSamplePreview(const pocket::Sample&);
    double processedTime=0;
    static pocket::Sample readSample(const juce::File&,juce::String& error);
    pocket::Sample factorySample(const juce::String& name) const;
    void loadFactoryKit(int index);
    pocket::Engine engine;
    pocket::Tape tape;
    std::atomic<bool> panicRequested{false};
    pocket::HeldNotes heldNotes;
    double rate=48000, clock=0;
    int previousSeqMode=-1;
    int step=0, seqNote=-1, scopeCounter=0, scopeIndex=0;
    bool wasSequencing=false;
    int64_t lastHostStep=INT64_MIN;
    float outputGain=.7f,outputSmoothing=.002f;
    std::vector<float> inputCapture;
    int captureWrite=0;
    double captureRate=48000;
    float clickDecay=.99f;
    float eqLowState=0,eqHighState=0,eqLowCoefficient=0,eqHighCoefficient=0;
    std::array<float,3> eqGains{{1,1,1}};
    std::array<float,192000> masterDelay{};size_t masterDelayWrite=0;
    float masterDriveSmooth=0,masterEchoSmooth=0,masterTimeSmooth=.281f,eqMixSmooth=0;
    double clickBeat=0,clickPhase=0;float clickEnvelope=0;int64_t lastClickBeat=-1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PocketProcessor)
};
