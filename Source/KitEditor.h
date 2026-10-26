// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PluginProcessor.h"
class KitEditor : public juce::Component,private juce::Timer {
public:
    explicit KitEditor(PocketProcessor&);
    ~KitEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
private:
    void select(int);
    void chooseSample();
    void timerCallback() override;
    PocketProcessor& processor;
    int selected=0,auditionNote=-1;
    std::array<float,128> waveform{};
    std::array<juce::TextButton,24> pads;
    std::array<juce::Slider,4> sliders;
    std::array<juce::Label,4> labels;
    juce::Label title;
    juce::TextButton reverse{"REVERSE"},gated{"GATED"},load{"LOAD PAD"},reset{"RESET PAD"};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttachment,gatedAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
};
