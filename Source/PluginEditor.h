// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PluginProcessor.h"
#include "SoundBrowser.h"
#include "KitEditor.h"
class PocketLook : public juce::LookAndFeel_V4 {
public:
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
};
class HardwareKeyboard : public juce::MidiKeyboardComponent {
public:
    explicit HardwareKeyboard(juce::MidiKeyboardState& s) : MidiKeyboardComponent(s,horizontalKeyboard) {}
    void drawWhiteNote(int,juce::Graphics&,juce::Rectangle<float>,bool,bool,juce::Colour,juce::Colour) override;
    void drawBlackNote(int,juce::Graphics&,juce::Rectangle<float>,bool,bool,juce::Colour) override;
    bool mouseDownOnKey(int,const juce::MouseEvent&) override;
    bool mouseDraggedToKey(int,const juce::MouseEvent&) override;
};
class PocketEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PocketEditor(PocketProcessor&);
    ~PocketEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    int getControlParameterIndex(juce::Component&) override;
private:
    void timerCallback() override;
    void selectPage(int);
    void chooseSample();
    void chooseSamples();
    void showBrowser(int kind=1);
    void showError(const juce::String&);
    void doTapeEdit(const juce::String&);
    void showComMenu();
    void showSequenceMenu();
    void applyTapeTrick(int);
    bool shifted() const;
    void setParameter(const char*,float);
    void chooseBank();
    void refreshPresetMenu();
    void chooseExport();
    void chooseSession(bool save);
    void setOctave(int);
    void selectMode(int);
    void applySoundSlot(int);
    PocketProcessor& processor;
    PocketLook look;
    SoundBrowser browser;
    KitEditor kitEditor;
    juce::TextButton kitEditKey{"KIT EDIT"};
    bool kitOpen=false;
    juce::String lastSelectedSound;
    bool browserOpen=true,showSequence=false;
    juce::TooltipWindow tooltip{this,650};
    int page=0,octave=0,mainMode=0;

    std::array<juce::TextButton,4> modeButtons;
    std::array<juce::TextButton,8> slotButtons;
    juce::TextButton browseKey{"BROWSE"},splitKey{"SPLIT"},liftKey{"LIFT"},dropKey{"DROP"},undoKey{"UNDO"},sequencerKey{"SEQ"},comKey{"COM"},micKey{"IN"},metroKey{"CLICK"};
    juce::TextButton phase246{"PHASE 246 CORE"};bool lastPhase246=false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAttachment;
    juce::TextButton cleanSample{"CLEAN SAMPLE"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cleanAttachment;
    juce::Label status;
    juce::TextButton sequenceSettings{"SEQ SETTINGS"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> metroAttachment;
    juce::TextButton shiftKey{"shift"},helpKey{"?"},tapTempoKey{"TAP"};
    double lastTempoTap=0,scrubStart=0;bool slidingTake=false;
    std::array<juce::String,4> knobLabels;
    std::array<juce::TextButton,5> pageButtons;
    juce::ComboBox presets,tapeTrack,tapeLength;
    juce::TextButton importBank{"BANK"};
    juce::TextButton saveSession{"SAVE"},loadSession{"OPEN"};
    juce::TextButton loadSample{"LOAD"},loopSample{"LOOP"},tapePlay{"PLAY"},tapeRecord{"REC"},overdub{"OVER"},rewind{"|<"},clear{"CLEAR"},exportTape{"EXPORT"},octaveDown{"OCT -"},octaveUp{"OCT +"};
    juce::Slider tapeSpeed;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> trackAttachment,lengthAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    std::array<juce::Slider,4> knobs;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>,4> attachments;
    juce::Slider volume, tempo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment, tempoAttachment;
    juce::ComboBox engine;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineAttachment;
    juce::TextButton sequence{"PLAY"}, panic{"STOP"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sequenceAttachment;
    std::array<juce::TextButton,16> stepButtons;
    HardwareKeyboard keyboard;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PocketEditor)
};
