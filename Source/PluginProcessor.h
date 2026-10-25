// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Tape.h"
class PocketProcessor : public juce::AudioProcessor {
public:
    PocketProcessor();
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
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::array<std::atomic<float>,256> scope{};
    std::atomic<int> playhead{-1},lastNote{60};
    std::atomic<float> tapePosition{0};
    std::atomic<int> sampleFrames{0};
    std::array<std::atomic<int>,16> steps{};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
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
