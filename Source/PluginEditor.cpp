// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginEditor.h"
#include "ScreenAnimations.h"
namespace {
const std::array<juce::Colour,4> hardwareColours {juce::Colour(0xff519fce),juce::Colour(0xff69b692),juce::Colour(0xffe6e8eb),juce::Colour(0xffe07e2d)};
void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size,juce::Colour c,juce::Justification j=juce::Justification::centred) {
    g.setColour(c); g.setFont(juce::FontOptions(size)); g.drawText(s,r,j);
}
}
void PocketLook::drawButtonText(juce::Graphics& g,juce::TextButton& button,bool over,bool down) {
    if(!button.getProperties().contains("modeIcon")) {juce::LookAndFeel_V4::drawButtonText(g,button,over,down);return;}
    const int mode=int(button.getProperties()["modeIcon"]);const float x=button.getWidth()*.5f,y=button.getHeight()*.5f;
    g.setColour(mode==0?hardwareColours[0]:mode==1?hardwareColours[1]:mode==2?hardwareColours[3]:juce::Colour(0xff627185));
    if(mode==0) {juce::Path wave;for(int i=0;i<25;++i){const float px=x-12+i,py=y+std::sin(float(i)*float(pocket::pi)/6)*6;if(i==0)wave.startNewSubPath(px,py);else wave.lineTo(px,py);}g.strokePath(wave,juce::PathStrokeType(1.5f));}
    else if(mode==1) {g.drawEllipse(x-9,y-7,18,14,1.4f);g.drawLine(x-7,y-7,x+7,y+7,1);g.drawLine(x+7,y-7,x-7,y+7,1);}
    else if(mode==2) {g.drawEllipse(x-12,y-6,12,12,1.4f);g.drawEllipse(x,y-6,12,12,1.4f);g.drawLine(x-6,y+6,x+6,y+6,1);}
    else for(int i=0;i<4;++i)g.fillRect(x-10+i*6,y-8+(i%3)*3,2.f,16.f-(i%3)*3);
}
void PocketLook::drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour& color,bool over,bool down) {
    auto r=button.getLocalBounds().toFloat().reduced(1);
    if(bool(button.getProperties()["hardware"])) {
        g.setColour(juce::Colour(0xff343b45));g.fillRoundedRectangle(r,3);
        auto tile=r.reduced(1);g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe4e9ef),tile.getTopLeft(),juce::Colour(0xffc9d2de),tile.getBottomRight(),false));g.fillRoundedRectangle(tile,2);
        const float diameter=std::min(r.getWidth(),r.getHeight())*.67f;
        auto cap=r.withSizeKeepingCentre(diameter,diameter).translated(0,down?1.f:0.f);
        g.setColour(juce::Colour(0xff9da9b8).withAlpha(.7f));g.fillEllipse(cap.translated(2,3));
        const auto face=button.getToggleState()?juce::Colour(0xffc0d6e2):over?color.brighter(.08f):color;
        g.setGradientFill(juce::ColourGradient(face.brighter(.07f),cap.getTopLeft(),face.darker(.07f),cap.getBottomRight(),false));g.fillEllipse(cap);
        g.setColour(juce::Colours::white.withAlpha(.35f));g.drawEllipse(cap.reduced(1),1);return;
    }
    g.setColour(juce::Colour(0xff929ba8));g.fillRoundedRectangle(r.translated(0,1),5);
    const auto face=button.getToggleState()?color.darker(.08f):down?color.darker(.1f):over?color.brighter(.06f):color;
    g.setGradientFill(juce::ColourGradient(face.brighter(.06f),r.getTopLeft(),face.darker(.03f),r.getBottomLeft(),false));g.fillRoundedRectangle(r,5);
    g.setColour(juce::Colours::white.withAlpha(.6f));g.drawRoundedRectangle(r.reduced(.7f),4,1);
}
void PocketLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& slider) {
    auto r=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(8);
    const float d=std::min(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(d,d);
    g.setColour(juce::Colour(0xffa8b3c0));g.fillEllipse(r.translated(0,2));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff5f7f9),r.getTopLeft(),juce::Colour(0xffd0d8e1),r.getBottomRight(),false));g.fillEllipse(r);
    g.setColour(juce::Colour(0xffa7b4c2));g.drawEllipse(r.reduced(.5f),1);
    auto cap=r.reduced(d*.21f);const auto color=slider.findColour(juce::Slider::rotarySliderFillColourId);
    g.setGradientFill(juce::ColourGradient(color.brighter(.13f),cap.getTopLeft(),color.darker(.12f),cap.getBottomRight(),false));g.fillEllipse(cap);
    g.setColour(juce::Colours::white.withAlpha(.65f));g.drawEllipse(cap.reduced(1),1);
    const float angle=start+position*(end-start);
    juce::Path tick;tick.addRoundedRectangle(-2,-d*.18f,4,d*.36f,2);
    g.setColour(juce::Colour(0xfff4f7fa));g.fillPath(tick,juce::AffineTransform::rotation(angle).translated(r.getCentreX(),r.getCentreY()));
}
void HardwareKeyboard::drawWhiteNote(int,juce::Graphics& g,juce::Rectangle<float> r,bool down,bool over,juce::Colour,juce::Colour) {
    r=r.withTrimmedTop(r.getHeight()*.30f).reduced(1,2);
    g.setColour(juce::Colour(0xff343c48));g.fillRoundedRectangle(r,3);
    auto tile=r.reduced(1);g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe2e8ef),tile.getTopLeft(),juce::Colour(0xffc6d0dd),tile.getBottomRight(),false));g.fillRoundedRectangle(tile,2);
    auto cap=tile.reduced(8,10).translated(0,down?1.f:0.f);
    g.setColour(juce::Colour(0xffa3afbe));g.fillRoundedRectangle(cap.translated(1,3),cap.getWidth()*.45f);
    const auto face=down?juce::Colour(0xffa4cce2):over?juce::Colour(0xfff1f4f7):juce::Colour(0xffe0e6ed);
    g.setGradientFill(juce::ColourGradient(face.brighter(.06f),cap.getTopLeft(),face.darker(.04f),cap.getBottomRight(),false));g.fillRoundedRectangle(cap,cap.getWidth()*.45f);
    g.setColour(juce::Colours::white.withAlpha(.5f));g.drawRoundedRectangle(cap.reduced(.5f),cap.getWidth()*.44f,1);
}
void HardwareKeyboard::drawBlackNote(int,juce::Graphics& g,juce::Rectangle<float> r,bool down,bool over,juce::Colour) {
    g.setColour(juce::Colour(0xff343c48));g.fillRoundedRectangle(r.reduced(1),2);
    g.setColour(juce::Colour(0xffd6dee7));g.fillRoundedRectangle(r.reduced(2),1);
    const float diameter=std::min(r.getWidth()*.72f,r.getHeight()*.70f);
    auto circle=r.withSizeKeepingCentre(diameter,diameter);
    g.setColour(juce::Colour(0xfff4f6f9));g.fillEllipse(circle.expanded(1));
    g.setColour(down?juce::Colour(0xff5799bc):over?juce::Colour(0xff37424f):juce::Colour(0xff181d24));g.fillEllipse(circle);
}
bool HardwareKeyboard::mouseDownOnKey(int note,const juce::MouseEvent& event) {
    const int pitch=note%12; const bool accidental=pitch==1||pitch==3||pitch==6||pitch==8||pitch==10;
    return accidental||event.position.y>=getHeight()*.30f;
}
bool HardwareKeyboard::mouseDraggedToKey(int note,const juce::MouseEvent& event) {return mouseDownOnKey(note,event);}
PocketEditor::PocketEditor(PocketProcessor& p) : AudioProcessorEditor(p),processor(p),keyboard(p.keyboard) {
    setLookAndFeel(&look); setSize(1200,500);
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffdbe1e9));
    look.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff66717d));
    look.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff253947));
    for(int i=0;i<4;++i) {auto& button=modeButtons[size_t(i)];addAndMakeVisible(button);button.setButtonText(juce::StringArray{"SYNTH","DRUM","TAPE","MIX"}[i]);button.getProperties().set("modeIcon",i);button.setTooltip(juce::StringArray{"Synthesizer","Drums and sample kits","Four-track tape","Four-track mixer"}[i]);button.onClick=[this,i]{selectMode(i);};}
    for(int i=0;i<8;++i) {auto& button=slotButtons[size_t(i)];addAndMakeVisible(button);button.setButtonText(juce::String(i+1));button.onClick=[this,i]{applySoundSlot(i);};button.setTooltip("Sound slot "+juce::String(i+1)+". Shift-click to assign the selected sound.");}
    addAndMakeVisible(helpKey);helpKey.setTooltip("Show control guide");helpKey.onClick=[] {juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Pocket One controls","Modes: synth, drums, tape and mixer. T1 motor / T2 envelope / T3 FX / T4 LFO. In tape mode T1-T4 select the recording track. Shift shows sequence steps and assigns sound slots. BANK imports a user sound bank ZIP. Stop transport before loading audio or saving sessions.");};
    addAndMakeVisible(tapTempoKey);tapTempoKey.setTooltip("Tap tempo twice or more. The DAW tempo takes precedence when supplied.");tapTempoKey.onClick=[this] {const double now=juce::Time::getMillisecondCounterHiRes()/1000,interval=now-lastTempoTap;if(interval>=.25&&interval<=1.5){auto* p=processor.state.getParameter("tempo");p->setValueNotifyingHost(p->convertTo0to1(float(60/interval)));}lastTempoTap=now;};
    addAndMakeVisible(shiftKey);shiftKey.setClickingTogglesState(true);shiftKey.setTooltip("Latch Shift to assign a sound slot or write a sequence note.");
    for(size_t i=0;i<4;++i) {
        auto& s=knobs[i]; addAndMakeVisible(s); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); s.setColour(juce::Slider::rotarySliderFillColourId,hardwareColours[i]);
    }
    for(size_t i=0;i<5;++i) {
        auto& b=pageButtons[i]; addAndMakeVisible(b); b.setButtonText(i<4?juce::String(int(i+1)):"TAPE");
        b.onClick=[this,i]{if(page==4&&i<4) {auto* p=processor.state.getParameter("tapeTrack");p->setValueNotifyingHost(p->convertTo0to1(float(i)));}else {const int pages[]={0,1,3,2,4};selectPage(pages[i]);}};
    }
    addAndMakeVisible(presets); presets.setTextWhenNothingSelected("FACTORY SOUNDS");
    refreshPresetMenu();
    presets.setTooltip("25 factory sounds: synthesis, melodic samples and three 8-pad drum kits.");
    presets.onChange=[this]{const int index=presets.getSelectedItemIndex();if(index<25) processor.applyPreset(index);else {juce::String error;if(!processor.applyUserPreset(index-25,error)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"User preset",error);}};
    addAndMakeVisible(importBank);importBank.setTooltip("Import a Pocket One sound bank ZIP. Adds sounds to your local library.");importBank.onClick=[this]{chooseBank();};
    for(auto pair:{std::pair{&loopSample,"sampleLoop"},{&tapePlay,"tapePlay"},{&tapeRecord,"tapeRecord"},{&overdub,"tapeOverdub"}}) {
        addAndMakeVisible(*pair.first); pair.first->setClickingTogglesState(true);
        pair.first->setColour(juce::TextButton::buttonOnColourId,pair.first==&tapeRecord?hardwareColours[3]:hardwareColours[1].darker());
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,pair.second,*pair.first));
    }
    for(auto* b:{&loadSample,&rewind,&clear,&exportTape,&octaveDown,&octaveUp,&saveSession,&loadSession}) addAndMakeVisible(*b);
    saveSession.onClick=[this]{chooseSession(true);}; loadSession.onClick=[this]{chooseSession(false);};
    saveSession.setTooltip("Save complete session: sound, sample, pattern and four tape tracks."); loadSession.setTooltip("Open a .pocketone session.");
    loadSample.onClick=[this]{chooseSample();}; exportTape.onClick=[this]{chooseExport();};
    rewind.onClick=[this]{processor.rewindTape();}; clear.onClick=[this]{processor.clearTape();};
    clear.setTooltip("Erase the selected tape track. Export or save your project first.");
    octaveDown.onClick=[this]{if(page==4) processor.rewindTape();else setOctave(octave-1);};
    octaveUp.onClick=[this]{if(page==4) {auto* p=processor.state.getParameter("tapeSpeed");p->setValueNotifyingHost(p->convertTo0to1(p->getValue()<.99f?2.f:1.f));}else setOctave(octave+1);};
    tapeRecord.setButtonText("●");tapeRecord.setColour(juce::TextButton::textColourOffId,hardwareColours[3]);tapeRecord.setTooltip("Record to the selected tape track; starts tape playback.");
    tapeRecord.onClick=[this]{if(tapeRecord.getToggleState()){auto* p=processor.state.getParameter("tapePlay");p->setValueNotifyingHost(1);selectMode(2);}};
    sequence.setButtonText("▶");panic.setButtonText("■");octaveDown.setButtonText("‹");octaveUp.setButtonText("›");
    addAndMakeVisible(tapeTrack); tapeTrack.addItemList({"Track 1","Track 2","Track 3","Track 4"},1);
    addAndMakeVisible(tapeLength); tapeLength.addItemList({"4 sec","8 sec","16 sec"},1);
    trackAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"tapeTrack",tapeTrack);
    lengthAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"tapeLength",tapeLength);
    addAndMakeVisible(tapeSpeed); tapeSpeed.setSliderStyle(juce::Slider::LinearHorizontal); tapeSpeed.setTextBoxStyle(juce::Slider::TextBoxRight,false,38,20);
    tapeSpeed.setTooltip("Tape playback speed: reverse, stop, half or double speed. Recording always runs at x1.");
    speedAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"tapeSpeed",tapeSpeed);
    addAndMakeVisible(engine); engine.addItemList({"01 / FM","02 / PHASE","03 / STRING","04 / SAMPLER","05 / DRUM","06 / SAMPLE KIT"},1);
    engine.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff202b2e)); engine.setColour(juce::ComboBox::outlineColourId,juce::Colours::transparentBlack);
    engineAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"engine",engine);
    for(auto* s : {&volume,&tempo}) { addAndMakeVisible(*s); s->setSliderStyle(juce::Slider::LinearHorizontal); s->setTextBoxStyle(juce::Slider::TextBoxRight,false,45,20); s->setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xff343b3b)); s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack); }
    volume.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);volume.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);volume.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffe0e5eb));volume.setTooltip("Master output volume");
    volumeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"volume",volume);
    tempoAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"tempo",tempo);
    addAndMakeVisible(sequence); sequence.setClickingTogglesState(true);
    sequence.setColour(juce::TextButton::buttonOnColourId,hardwareColours[1].darker());
    sequenceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"sequencer",sequence);
    addAndMakeVisible(panic); panic.onClick=[this] {
        auto* param=processor.state.getParameter("sequencer"); param->beginChangeGesture(); param->setValueNotifyingHost(0); param->endChangeGesture();
        for(const char* id:{"tapePlay","tapeRecord"}) {auto* parameter=processor.state.getParameter(id); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();}
        processor.keyboard.allNotesOff(0); processor.requestPanic();
    };
    for(size_t i=0;i<16;++i) {
        auto& b=stepButtons[i]; addAndMakeVisible(b); b.setButtonText(juce::String(int(i+1))); b.setTooltip("Click: toggle. Shift-click: write the last played MIDI note.");
        b.onClick=[this,i] { static constexpr int melody[]={60,64,67,72,63,67,70,74,60,72,67,65,63,67,58,62}; const int n=processor.steps[i].load(); processor.steps[i].store((shiftKey.getToggleState()||juce::ModifierKeys::getCurrentModifiers().isShiftDown())?processor.lastNote.load():n<0?melody[i]:-1); repaint(); };
    }
    addAndMakeVisible(keyboard); keyboard.setAvailableRange(60,83); keyboard.setLowestVisibleKey(60); keyboard.setScrollButtonsVisible(false); keyboard.setKeyWidth(65); keyboard.setBlackNoteLengthProportion(.29f); keyboard.setBlackNoteWidthProportion(.84f);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xffcdd5df));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,juce::Colour(0xffcdd5df));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,juce::Colours::transparentBlack);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,hardwareColours[0].withAlpha(.6f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,hardwareColours[0].withAlpha(.15f));
    keyboard.setWantsKeyboardFocus(true);
    for(auto& button:modeButtons)button.getProperties().set("hardware",true);
    for(auto& button:slotButtons)button.getProperties().set("hardware",true);
    for(auto& button:pageButtons)button.getProperties().set("hardware",true);
    for(auto* button:{&tapeRecord,&sequence,&panic,&octaveDown,&octaveUp,&shiftKey,&helpKey,&tapTempoKey})button->getProperties().set("hardware",true);
    setOctave(0); selectPage(0); startTimerHz(30);
}
PocketEditor::~PocketEditor() { stopTimer(); setLookAndFeel(nullptr); }
void PocketEditor::resized() {
    volume.setBounds(158,36,66,66);helpKey.setBounds(154,108,55,58);tapTempoKey.setBounds(211,108,55,58);
    for(int i=0;i<4;++i) knobs[size_t(i)].setBounds(577+i*144,49,92,92);
    for(int i=0;i<4;++i) {modeButtons[size_t(i)].setBounds(35+i*56,177,54,49);pageButtons[size_t(i)].setBounds(282+i*65,177,63,49);}
    pageButtons[4].setVisible(false);
    for(int i=0;i<8;++i) slotButtons[size_t(i)].setBounds(550+i*72,177,70,49);
    tapeRecord.setBounds(35,240,54,69);sequence.setBounds(91,240,54,69);panic.setBounds(147,240,54,69);
    octaveDown.setBounds(35,321,54,84);octaveUp.setBounds(91,321,54,84);shiftKey.setBounds(147,321,54,84);
    keyboard.setBounds(220,233,910,175);
    for(int i=0;i<16;++i) stepButtons[size_t(i)].setBounds(282+i*53,233,49,20);
    presets.setBounds(35,452,322,27);importBank.setBounds(367,452,66,27);saveSession.setBounds(443,452,64,27);loadSession.setBounds(517,452,64,27);
    engine.setBounds(600,452,169,27);tempo.setBounds(785,452,178,27);loadSample.setBounds(980,452,77,27);loopSample.setBounds(1066,452,80,27);
    tapeTrack.setBounds(602,452,80,27);tapeLength.setBounds(686,452,75,27);tapeSpeed.setBounds(769,452,135,27);
    tapePlay.setBounds(912,452,45,27);overdub.setBounds(963,452,49,27);clear.setBounds(1018,452,55,27);exportTape.setBounds(1079,452,67,27);
    rewind.setVisible(false);
}
void PocketEditor::selectMode(int mode) {
    mainMode=mode;
    if(mode>=2) selectPage(4);
    else {const int engineIndex=int(processor.state.getRawParameterValue("engine")->load());
        if(mode==1&&engineIndex<4) {auto* p=processor.state.getParameter("engine");p->setValueNotifyingHost(p->convertTo0to1(5));}
        if(mode==0&&engineIndex>=4) {auto* p=processor.state.getParameter("engine");p->setValueNotifyingHost(0);}
        selectPage(0);
    }
}
void PocketEditor::applySoundSlot(int slot) {
    if(shiftKey.getToggleState()||juce::ModifierKeys::getCurrentModifiers().isShiftDown()) {
        const int selected=presets.getSelectedItemIndex();if(selected>=0)soundSlots[size_t(slot)]=selected;shiftKey.setToggleState(false,juce::dontSendNotification);return;
    }
    presets.setSelectedItemIndex(soundSlots[size_t(slot)],juce::sendNotificationSync);
}
void PocketEditor::setOctave(int value) {
    octave=juce::jlimit(-2,1,value); const int base=60+octave*12;
    keyboard.setAvailableRange(base,base+23); keyboard.setLowestVisibleKey(base); keyboard.setKeyPressBaseOctave(base/12);
}
void PocketEditor::selectPage(int selected) {
    page=selected;
    const std::array<std::array<juce::String,4>,5> ids{{{"tone","color","envelope","effect"},{"attack","decay","sustain","release"},{"lfoRate","lfoDepth","lfoDestination","lfoShape"},{"delayTime","delayFeedback","effect","drive"},{"track1","track2","track3","track4"}}};
    const std::array<std::array<juce::String,4>,5> labels{{{"TONE","COLOR","ENVELOPE","ECHO"},{"ATTACK","DECAY","SUSTAIN","RELEASE"},{"RATE","DEPTH","DESTINATION","SHAPE"},{"DELAY TIME","FEEDBACK","ECHO MIX","DRIVE"},{"TRACK 1","TRACK 2","TRACK 3","TRACK 4"}}};
    knobLabels=labels[size_t(page)];
    for(size_t i=0;i<4;++i) {attachments[i].reset(); attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,ids[size_t(page)][i],knobs[i]); knobs[i].setTooltip(knobLabels[i]+": drag to adjust; double-click to reset");}
    const int tab=page==0?0:page==1?1:page==3?2:page==2?3:-1;
    for(int i=0;i<4;++i) pageButtons[size_t(i)].setColour(juce::TextButton::buttonColourId,i==tab?juce::Colour(0xffc0d0dd):juce::Colour(0xffdbe1e9));
    sequenceAttachment.reset();sequenceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,page==4?"tapePlay":"sequencer",sequence);
    const bool tapePage=page==4; engine.setVisible(!tapePage);
    for(juce::Component* c:std::array<juce::Component*,7>{&tapePlay,&overdub,&clear,&exportTape,&tapeTrack,&tapeLength,&tapeSpeed}) c->setVisible(tapePage);
    tapeRecord.setVisible(true);tempo.setVisible(!tapePage);rewind.setVisible(false);
    octaveDown.setTooltip(tapePage?"Rewind tape":"Octave down");octaveUp.setTooltip(tapePage?"Toggle tape playback x1/x2":"Octave up");
    timerCallback();
}
void PocketEditor::timerCallback() {
    processor.updateKeyboardDisplay();
    const int e=int(processor.state.getRawParameterValue("engine")->load());
    presets.setSelectedItemIndex(processor.currentUserPreset.load()>=0?25+processor.currentUserPreset.load():processor.currentFactoryPreset.load(),juce::dontSendNotification);
    loadSample.setVisible(page!=4&&e==3); loopSample.setVisible(page!=4&&e==3);
    if(page==0) {knobLabels[0]=e==3?"START":"TONE"; knobLabels[1]=e==3?"LENGTH":"COLOR";}
    const int activeMode=page==4?(mainMode==3?3:2):e>=4?1:0;
    for(int i=0;i<4;++i) modeButtons[size_t(i)].setColour(juce::TextButton::buttonColourId,i==activeMode?juce::Colour(0xffbed0dc):juce::Colour(0xffdbe1e9));
    for(size_t i=0;i<16;++i) {
        stepButtons[i].setVisible(shiftKey.getToggleState());
        const int n=processor.steps[i].load(); const bool current=processor.playhead.load()==int(i);
        stepButtons[i].setColour(juce::TextButton::buttonColourId,current?hardwareColours[0]:n>=0?juce::Colour(0xffc3d5dd):juce::Colour(0xffdbe1e9));
        stepButtons[i].setTooltip("Step "+juce::String(int(i+1))+": "+(n<0?"rest":juce::MidiMessage::getMidiNoteName(n,true,true,4))+". Shift-click writes last played note.");
    }
    repaint(279,33,263,135);repaint(1142,100,16,72);
}
void PocketEditor::refreshPresetMenu() {
    presets.clear(juce::dontSendNotification);presets.addItemList(PocketProcessor::factoryPresetNames(),1);
    const auto user=processor.userPresetNames();for(int i=0;i<user.size();++i) presets.addItem("USER / "+user[i],26+i);
}
void PocketEditor::chooseBank() {
    chooser=std::make_unique<juce::FileChooser>("Import Pocket One sound bank ZIP",juce::File{},"*.zip");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<PocketEditor>(this)](const juce::FileChooser& fc){
        if(!safe||fc.getResult()==juce::File{}) return;juce::String error;
        if(!safe->processor.importUserBank(fc.getResult(),error)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Sound bank import",error);
        else safe->refreshPresetMenu();
    });
}
void PocketEditor::chooseSample() {
    chooser=std::make_unique<juce::FileChooser>("Load WAV / AIFF (first 16 seconds)",juce::File{},"*.wav;*.aif;*.aiff");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<PocketEditor>(this)](const juce::FileChooser& fc){
        if(!safe||fc.getResult()==juce::File{}) return; juce::String error;
        if(!safe->processor.loadSample(fc.getResult(),error)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Sample import",error);
    });
}
void PocketEditor::chooseExport() {
    chooser=std::make_unique<juce::FileChooser>("Export four-track WAV",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Pocket-One-tape.wav"),"*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe=juce::Component::SafePointer<PocketEditor>(this)](const juce::FileChooser& fc){
        if(!safe||fc.getResult()==juce::File{}) return; juce::String error;
        if(!safe->processor.exportTape(fc.getResult().withFileExtension("wav"),error)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Tape export",error);
    });
}
void PocketEditor::chooseSession(bool save) {
    chooser=std::make_unique<juce::FileChooser>(save?"Save complete session":"Open complete session",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Pocket-One.pocketone"),"*.pocketone");
    const int flags=juce::FileBrowserComponent::canSelectFiles|(save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode);
    chooser->launchAsync(flags,[safe=juce::Component::SafePointer<PocketEditor>(this),save](const juce::FileChooser& fc){
        if(!safe||fc.getResult()==juce::File{}) return;
        juce::MemoryBlock data; bool ok=false;
        if(save) {safe->processor.getStateInformation(data);ok=fc.getResult().withFileExtension("pocketone").replaceWithData(data.getData(),data.getSize());}
        else if(fc.getResult().getSize()<140000000&&fc.getResult().loadFileAsData(data)) {
            if(auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()))) if(xml->hasTagName("PocketOne")) {safe->processor.setStateInformation(data.getData(),int(data.getSize()));ok=true;}
        }
        if(!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Session",save?"Could not save session.":"This is not a valid Pocket One session.");
    });
}
void PocketEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xfff4f6f8));
    auto body=juce::Rectangle<float>(18,20,1164,404);
    g.setColour(juce::Colour(0xffaeb8c5));g.fillRoundedRectangle(body.translated(0,4),20);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe5eaf0),18,20,juce::Colour(0xffcbd4df),1180,425,false));g.fillRoundedRectangle(body,20);
    g.setColour(juce::Colours::white.withAlpha(.65f));g.drawRoundedRectangle(body.reduced(1),19,1);
    const auto ink=juce::Colour(0xff667482),line=juce::Colour(0xff596675);
    for(int y=40;y<155;y+=6) for(int x=39;x<143;x+=6) {g.setColour(juce::Colour(0xff616c7a));g.fillEllipse(float(x),float(y),2.2f,2.2f);}
    for(int i=0;i<4;++i) {g.setColour(line);g.drawRoundedRectangle(550.f+i*144,35,140,131,4,1);}
    g.setColour(line);g.drawRoundedRectangle(153,35,113,131,4,1);
    text(g,"VOL",{224,57,35,20},9,ink);
    g.setColour(juce::Colour(0xff12171d));g.fillRoundedRectangle(279,33,263,135,4);
    g.setColour(juce::Colour(0xff05080b));g.fillRoundedRectangle(285,39,251,123,2);
    pocket::ScreenFrame frame;frame.page=page;frame.mode=mainMode;
    const auto get=[this](const char* id){return processor.state.getRawParameterValue(id)->load();};
    frame.engine=int(get("engine"));frame.note=processor.lastNote.load();frame.voices=processor.visualVoices.load();frame.stage=processor.visualEnvelopeStage.load();frame.step=processor.playhead.load();
    frame.time=processor.visualTime.load();frame.level=processor.visualLevel.load();frame.envelope=processor.visualEnvelope.load();frame.samplePosition=processor.visualSamplePosition.load();frame.lfoPhase=processor.visualLfoPhase.load();
    frame.tapePosition=processor.tapePosition.load();frame.tapeLength=float(4<<int(get("tapeLength")));frame.track=int(get("tapeTrack"));
    frame.tone=get("tone");frame.color=get("color");frame.attack=get("attack");frame.decay=get("decay");frame.sustain=get("sustain");frame.release=get("release");frame.lfoRate=get("lfoRate");frame.lfoShape=int(get("lfoShape"));frame.delay=get("delayTime");frame.feedback=get("delayFeedback");
    for(size_t i=0;i<128;++i)frame.sample[i]=processor.samplePreview[i].load(std::memory_order_relaxed);
    for(size_t i=0;i<4;++i){frame.labels[i]=knobLabels[i];frame.values[i]=float(knobs[i].getValue());frame.trackGains[i]=get(("track"+juce::String(int(i+1))).toRawUTF8());}
    {juce::Graphics::ScopedSaveState save(g);g.setOrigin(285,39);g.reduceClipRegion(0,0,251,123);pocket::drawScreen(g,frame);}
    g.setColour(line);g.drawLine(34,230,1130,230,1);g.drawLine(212,233,212,407,1);
    text(g,"POCKET ONE",{1137,283,33,90},10,ink);
    g.setColour(juce::Colour(0xff596777));for(int y=46;y<61;y+=5)for(int x=1145;x<1158;x+=5)g.fillEllipse(float(x),float(y),1.5f,1.5f);
    const float level=std::min(1.f,std::abs(processor.scope[0].load())*5);
    for(int i=0;i<8;++i){g.setColour(i<int(level*8)?hardwareColours[1]:juce::Colour(0xffb4c0cb));g.fillRoundedRectangle(1146,104.f+i*8,7,4,1);}
    g.setColour(juce::Colour(0xffe2e7ed));g.fillRoundedRectangle(18,439,1164,49,10);
    for(auto point:{juce::Point<float>{28,29},{1172,29},{28,414},{1172,414}}) {g.setColour(line);g.fillEllipse(point.x-2,point.y-2,4,4);}
}
