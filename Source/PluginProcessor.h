// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Tape.h"
#include "NoteQueue.h"
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
    bool importUserBank(const juce::File&,juce::String& error);
    bool applyUserPreset(int index,juce::String& error);
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::array<std::atomic<float>,256> scope{};
    std::atomic<int> playhead{-1},lastNote{60};
    std::atomic<float> tapePosition{0};
    std::atomic<int> sampleFrames{0};
    std::array<std::atomic<float>,128> samplePreview{};
    std::atomic<float> visualEnvelope{0},visualSamplePosition{0},visualLfoPhase{0},visualTime{0},visualLevel{0};
    std::atomic<int> visualVoices{0},visualEnvelopeStage{0};
    std::atomic<uint64_t> visualTrigger{0};
    std::array<std::atomic<int>,16> steps{};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    enum class Param {engine,tone,color,envelope,effect,attack,decay,sustain,release,lfoRate,lfoDepth,lfoDestination,lfoShape,delayTime,delayFeedback,drive,sampleLoop,sampleRoot,volume,sequencer,tempo,track1,track2,track3,track4,tapePlay,tapeRecord,tapeOverdub,tapeTrack,tapeSpeed,tapeLength,Count};
    std::array<std::atomic<float>*,size_t(Param::Count)> audioParameters{};
    void handleNoteOn(juce::MidiKeyboardState*,int,int,float) override;
    void handleNoteOff(juce::MidiKeyboardState*,int,int,float) override;
    pocket::NoteQueue uiToAudio,audioToUi;
    std::atomic<bool> keyboardOverflow{false},keyboardDisplayReset{false};
    bool reflectingHostNotes=false;
    struct UserPreset {juce::String name;juce::File sample;std::array<juce::File,8> kit;juce::var parameters;bool isKit=false;};
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
    double rate=48000, clock=0;
    int step=0, seqNote=-1, scopeCounter=0, scopeIndex=0;
    bool wasSequencing=false;
    int lastHostStep=-1;
    float outputGain=.7f,outputSmoothing=.002f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PocketProcessor)
};
