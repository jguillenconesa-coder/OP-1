// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PluginProcessor.h"
class SoundBrowser : public juce::Component,private juce::ListBoxModel {
public:
    explicit SoundBrowser(PocketProcessor&);
    ~SoundBrowser() override;
    void refresh();
    void showKind(int kind);
    void syncSelection();
    std::function<void()> onImportBank,onImportSamples,onLoaded;
    std::function<void(const juce::String&)> onError;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    int getNumRows() override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void selectedRowsChanged(int) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    void returnKeyPressed(int) override;
    void rebuild();
    void loadSelected();
    void audition();
    void confirmPurge();
    PocketProcessor& processor;
    std::vector<SoundInfo> catalog,rows;
    juce::ComboBox bank,kind,view;
    juce::TextEditor search;
    juce::ListBox list{"Sound results",this};
    juce::TextButton load{"LOAD SOUND"},preview{"AUDITION"},favorite{"FAVORITE"},importBank{"IMPORT BANK"},importSamples{"ADD SAMPLES"};
    juce::TextButton purge{"PURGE BANK"};
    juce::Label details,count,activeSound;
    juce::String selectedId;
    int auditionNote=-1;uint64_t auditionGeneration=0;
    juce::Slider root;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootAttachment;
};
