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
    if(button.getProperties().contains("transportIcon")) {
        const int role=int(button.getProperties()["transportIcon"]);const float x=button.getWidth()*.5f,y=button.getHeight()*.5f;
        g.setColour(role==0?hardwareColours[3]:juce::Colour(0xff59677a));
        if(role==0)g.fillEllipse(x-7,y-7,14,14);
        else if(role==2)g.fillRect(x-6,y-6,12.f,12.f);
        else {juce::Path shape;if(role==1){shape.addTriangle(x-5,y-8,x-5,y+8,x+8,y);g.fillPath(shape);}else {const float direction=role==3?-1.f:1.f;shape.startNewSubPath(x-direction*4,y-7);shape.lineTo(x+direction*4,y);shape.lineTo(x-direction*4,y+7);g.strokePath(shape,juce::PathStrokeType(1.5f));}}return;
    }
    const auto legend=button.getProperties()["legend"].toString();if(legend.isNotEmpty()){g.setColour(juce::Colour(0xff4f687a));g.setFont(juce::FontOptions(13));g.drawText(button.getButtonText(),0,8,button.getWidth(),18,juce::Justification::centred);g.setFont(juce::FontOptions(8));g.drawText(legend,0,27,button.getWidth(),13,juce::Justification::centred);return;}
    if(button.getWidth()<40){g.setColour(juce::Colour(0xff536b7d));g.setFont(juce::FontOptions(9));g.drawFittedText(button.getButtonText(),button.getLocalBounds().reduced(2),juce::Justification::centred,2);return;}
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
PocketEditor::PocketEditor(PocketProcessor& p) : AudioProcessorEditor(p),processor(p),browser(p),kitEditor(p),keyboard(p.keyboard) {
    setLookAndFeel(&look); setSize(1200,790);
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffdbe1e9));
    look.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff66717d));
    look.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff253947));
    for(int i=0;i<4;++i) {auto& button=modeButtons[size_t(i)];addAndMakeVisible(button);button.setButtonText(juce::StringArray{"SYNTH","DRUM","TAPE","MIX"}[i]);button.getProperties().set("modeIcon",i);button.setTooltip(juce::StringArray{"Synthesizer","Drums and sample kits","Four-track tape","Four-track mixer"}[i]);button.onClick=[this,i]{selectMode(i);};}
    for(int i=0;i<8;++i) {auto& button=slotButtons[size_t(i)];addAndMakeVisible(button);button.setButtonText(juce::String(i+1));button.onClick=[this,i]{applySoundSlot(i);};button.setTooltip("Sound slot "+juce::String(i+1)+". Shift-click to assign the selected sound.");}
    addAndMakeVisible(helpKey);helpKey.setTooltip("Show control guide");helpKey.onClick=[] {juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Pocket One controls","Modes: synth, drums, tape and mixer. T1 motor / T2 envelope / T3 FX / T4 LFO. In tape mode T1-T4 select the recording track. Shift shows sequence steps and assigns sound slots. BROWSE > IMPORT BANK imports a user sound bank ZIP. Shift + SEQ or SEQ SETTINGS opens pattern/arpeggio controls. Shift + SYNTH/DRUM opens the filtered browser. Tape keys 1-8: IN / OUT / LOOP / BREAK / REVERSE / CHOP / MEM1 / MEM2. Shift stores a memory. SPLIT/LIFT/DROP edit stopped tape; Shift+SPLIT joins, Shift+LIFT lifts four tracks. Shift+IN records an input sample. Stop transport before saving/loading or tape edits.");};
    addAndMakeVisible(tapTempoKey);tapTempoKey.setTooltip("Tap tempo twice or more. Shift-click toggles metronome. The DAW tempo takes precedence when supplied.");tapTempoKey.onClick=[this] {if(shifted()){setParameter("metronome",processor.state.getRawParameterValue("metronome")->load()>.5f?0:1);return;}const double now=juce::Time::getMillisecondCounterHiRes()/1000,interval=now-lastTempoTap;if(interval>=.25&&interval<=1.5){auto* p=processor.state.getParameter("tempo");p->setValueNotifyingHost(p->convertTo0to1(float(60/interval)));}lastTempoTap=now;};
    addAndMakeVisible(shiftKey);shiftKey.setClickingTogglesState(true);shiftKey.setTooltip("Latch Shift to assign a sound slot or write a sequence note.");
    for(size_t i=0;i<4;++i) {
        auto& s=knobs[i]; addAndMakeVisible(s);s.setComponentID("screen-knob-"+juce::String(int(i+1)));s.setName(p.screenKnob(int(i)).getName(100)); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); s.setColour(juce::Slider::rotarySliderFillColourId,hardwareColours[i]);
    }
    for(size_t i=0;i<5;++i) {
        auto& b=pageButtons[i]; addAndMakeVisible(b); b.setButtonText(i<4?juce::String(int(i+1)):"TAPE");
        b.onClick=[this,i]{if(mainMode==3&&i<4){const int pages[]={4,5,6,7};selectPage(pages[i]);}else if(mainMode==2&&i<4){setParameter("tapeTrack",float(i));selectPage(4);}else {const int pages[]={0,1,3,2,4};selectPage(pages[i]);}};
    }
    addAndMakeVisible(kitEditor);kitEditor.setVisible(false);addAndMakeVisible(kitEditKey);kitEditKey.onClick=[this]{kitOpen=!kitOpen;browserOpen=true;setSize(1200,790);kitEditor.refresh();timerCallback();};
    addAndMakeVisible(browser);browser.onImportBank=[this]{chooseBank();};browser.onImportSamples=[this]{chooseSamples();};browser.onError=[this](const juce::String& error){showError(error);};browser.onLoaded=[this]{const bool drum=processor.state.getRawParameterValue("engine")->load()>=4;mainMode=drum?1:0;selectPage(0);lastSelectedSound=processor.selectedSound();repaint();};
    addAndMakeVisible(browseKey);browseKey.onClick=[this]{kitOpen=false;kitEditor.setVisible(false);browserOpen=!browserOpen;browser.setVisible(browserOpen);setSize(1200,browserOpen?790:540);};
    for(auto* button:{&splitKey,&liftKey,&dropKey,&undoKey,&sequencerKey,&comKey,&micKey,&metroKey})addAndMakeVisible(*button);
    splitKey.onClick=[this]{doTapeEdit("split");};liftKey.onClick=[this]{doTapeEdit(mainMode>=2?"lift":"copySound");};dropKey.onClick=[this]{doTapeEdit(mainMode>=2?"drop":processor.clipboardKind.load()==2?"pasteSound":"sampler");};undoKey.onClick=[this]{doTapeEdit("undo");};
    sequencerKey.onClick=[this]{if(shifted()){showSequenceMenu();shiftKey.setToggleState(false,juce::dontSendNotification);}else {showSequence=!showSequence;timerCallback();}};sequencerKey.setTooltip("Show sequence steps. Shift + SEQ opens mode, timing and arpeggio settings.");
    addAndMakeVisible(sequenceSettings);sequenceSettings.onClick=[this]{showSequenceMenu();};sequenceSettings.setTooltip("Pattern direction, held-note arpeggiator, rate, length, gate, swing and velocity");
    comKey.onClick=[this]{showComMenu();};comKey.setTooltip("Album / COM: export stereo mix, tracks and sessions.");
    micKey.onClick=[this]{if(shifted()){juce::String error;if(processor.inputCapturing.load()){if(!processor.finishInputCapture(error))showError(error);else {mainMode=0;selectPage(0);status.setText("Captured input loaded into sampler",juce::dontSendNotification);}}else if(!processor.startInputCapture(error))showError(error);else status.setText("Capturing input (up to 16 seconds). Shift + IN again to finish.",juce::dontSendNotification);}else setParameter("inputMonitor",processor.state.getRawParameterValue("inputMonitor")->load()>.5f?0:1);};micKey.setTooltip("Input monitor / record source. Shift + IN starts or finishes a 16-second sampler recording. Configure input in your host.");
    metroAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"metronome",metroKey);metroKey.setClickingTogglesState(true);metroKey.setTooltip("Toggle metronome at internal/host tempo.");
    addAndMakeVisible(phase246);phase246.setClickingTogglesState(true);phase246.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffb5dce8));phase246.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff263e53));phase246.setTooltip("Experimental native rev246 Pd core at 88.2 kHz. Raw colored controls on set 1; our envelope, modulation and output filter. Existing Phase sounds remain available with this switched off.");phaseAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.phaseParameters(),"phase246Enabled",phase246);
    addAndMakeVisible(cleanSample);cleanSample.setClickingTogglesState(true);cleanSample.setTooltip("Neutral sample/kit playback: bypass the implicit tone filter and dry saturation. Explicit effects remain active.");cleanAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"sampleClean",cleanSample);
    addAndMakeVisible(status);status.setColour(juce::Label::textColourId,juce::Colour(0xff536c80));status.setFont(juce::FontOptions(11));
    for(auto pair:{std::pair{&loopSample,"sampleLoop"},{&tapePlay,"tapePlay"},{&tapeRecord,"tapeRecord"},{&overdub,"tapeOverdub"}}) {
        addAndMakeVisible(*pair.first); pair.first->setClickingTogglesState(true);
        pair.first->setColour(juce::TextButton::buttonOnColourId,pair.first==&tapeRecord?hardwareColours[3]:hardwareColours[1].darker());
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,pair.second,*pair.first));
    }
    for(auto* b:{&loadSample,&rewind,&clear,&exportTape,&octaveDown,&octaveUp,&saveSession,&loadSession}) addAndMakeVisible(*b);
    saveSession.onClick=[this]{chooseSession(true);}; loadSession.onClick=[this]{chooseSession(false);};
    saveSession.setTooltip("Save complete session: sound, sample, pattern and four tape tracks."); loadSession.setTooltip("Open a .pocketone session.");
    loadSample.onClick=[this]{chooseSample();}; exportTape.onClick=[this]{chooseExport();};
    rewind.onClick=[this]{processor.rewindTape();}; clear.onClick=[this]{doTapeEdit("clear");};
    clear.setTooltip("Erase the selected tape track. Export or save your project first.");
    octaveDown.onClick=[this]{if(mainMode>=2){if(shifted())processor.seekTape(std::max(0.f,processor.tapePosition.load()-240/std::clamp(processor.effectiveTempo.load(),40.f,300.f)));}else setOctave(octave-1);};
    octaveUp.onClick=[this]{if(mainMode>=2){if(shifted())processor.seekTape(processor.tapePosition.load()+240/std::clamp(processor.effectiveTempo.load(),40.f,300.f));}else setOctave(octave+1);};
    octaveDown.onStateChange=[this]{if(!octaveDown.isDown())processor.tapeSpool.store(0);else if(mainMode>=2&&!shifted())processor.tapeSpool.store(-5);};octaveUp.onStateChange=[this]{if(!octaveUp.isDown())processor.tapeSpool.store(0);else if(mainMode>=2&&!shifted())processor.tapeSpool.store(5);};
    slotButtons[3].onStateChange=[this]{processor.tapeBreak.store(mainMode>=2&&slotButtons[3].isDown());};slotButtons[5].onStateChange=[this]{processor.tapeChop.store(mainMode>=2&&slotButtons[5].isDown());};
    sequence.onClick=[this]{if(mainMode>=2&&shifted()){const float speed=processor.state.getRawParameterValue("tapeSpeed")->load();setParameter("tapeSpeed",-std::max(.1f,std::abs(speed)));setParameter("tapePlay",1);}};
    tapeRecord.setButtonText("REC");tapeRecord.setColour(juce::TextButton::textColourOffId,hardwareColours[3]);tapeRecord.setTooltip("Record to the selected tape track; starts tape playback.");
    tapeRecord.onClick=[this]{if(tapeRecord.getToggleState()){const bool armed=shifted();setParameter("tapePlay",armed?0:1);selectMode(2);if(armed){status.setText("Tape armed. Press PLAY to record.",juce::dontSendNotification);shiftKey.setToggleState(false,juce::dontSendNotification);}}};
    sequence.setButtonText("PLAY");panic.setButtonText("STOP");octaveDown.setButtonText("<");octaveUp.setButtonText(">");
    int role=0;for(auto* button:{&tapeRecord,&sequence,&panic,&octaveDown,&octaveUp})button->getProperties().set("transportIcon",role++);
    addAndMakeVisible(tapeTrack); tapeTrack.addItemList({"Track 1","Track 2","Track 3","Track 4"},1);
    addAndMakeVisible(tapeLength); tapeLength.addItemList({"4 sec","8 sec","16 sec"},1);
    trackAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"tapeTrack",tapeTrack);
    lengthAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"tapeLength",tapeLength);
    tapeTrack.onChange=[this]{if(mainMode==2&&page==4)selectPage(4);};tapeLength.onChange=[this]{if(mainMode==2&&page==4)selectPage(4);};
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
        if(processor.inputCapturing.load()){juce::String error;if(!processor.finishInputCapture(error))showError(error);else {mainMode=0;selectPage(0);}}
        processor.tapeSpool.store(0);processor.tapeBreak.store(false);processor.tapeChop.store(false);if(shifted())processor.seekTape(0);
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
    for(auto* button:{&tapeRecord,&sequence,&panic,&octaveDown,&octaveUp,&shiftKey,&helpKey,&tapTempoKey,&splitKey,&liftKey,&dropKey,&sequencerKey,&comKey,&micKey})button->getProperties().set("hardware",true);
    setOctave(0);mainMode=processor.screenTapeMode()?2:processor.screenPage()>=4?3:processor.state.getRawParameterValue("engine")->load()>=4?1:0;selectPage(processor.screenPage()); startTimerHz(30);
}
PocketEditor::~PocketEditor() { processor.tapeSpool.store(0);processor.tapeBreak.store(false);processor.tapeChop.store(false);stopTimer(); setLookAndFeel(nullptr); }
void PocketEditor::resized() {
    volume.setBounds(158,36,66,66);helpKey.setBounds(154,108,55,58);tapTempoKey.setBounds(211,108,55,58);
    for(int i=0;i<4;++i) knobs[size_t(i)].setBounds(577+i*144,49,92,92);
    for(int i=0;i<4;++i) {modeButtons[size_t(i)].setBounds(35+i*56,177,54,49);pageButtons[size_t(i)].setBounds(282+i*65,177,63,49);}
    pageButtons[4].setVisible(false);
    for(int i=0;i<8;++i) slotButtons[size_t(i)].setBounds(550+i*72,177,70,49);
    splitKey.setBounds(35,234,54,48);liftKey.setBounds(91,234,54,48);dropKey.setBounds(147,234,54,48);
    tapeRecord.setBounds(35,287,54,50);sequence.setBounds(91,287,54,50);panic.setBounds(147,287,54,50);
    octaveDown.setBounds(35,343,54,62);octaveUp.setBounds(91,343,54,62);shiftKey.setBounds(147,343,54,62);
    micKey.setBounds(1138,36,34,60);comKey.setBounds(1138,103,34,63);sequencerKey.setBounds(1138,177,34,49);
    keyboard.setBounds(220,233,910,175);
    for(int i=0;i<16;++i) stepButtons[size_t(i)].setBounds(282+i*53,233,49,20);
    presets.setVisible(false);browseKey.setBounds(35,452,108,27);importBank.setBounds(367,452,66,27);importBank.setVisible(false);metroKey.setBounds(150,452,76,27);undoKey.setBounds(236,452,76,27);saveSession.setBounds(443,452,64,27);loadSession.setBounds(517,452,64,27);
    engine.setBounds(600,452,169,27);tempo.setBounds(785,452,178,27);loadSample.setBounds(980,452,77,27);loopSample.setBounds(1066,452,80,27);
    tapeTrack.setBounds(602,452,80,27);tapeLength.setBounds(686,452,75,27);tapeSpeed.setBounds(769,452,135,27);
    tapePlay.setBounds(912,452,45,27);overdub.setBounds(963,452,49,27);clear.setBounds(1018,452,55,27);exportTape.setBounds(1079,452,67,27);
    rewind.setVisible(false);browser.setBounds(18,516,1164,262);status.setBounds(35,490,620,23);cleanSample.setBounds(690,487,150,26);phase246.setBounds(690,487,160,26);kitEditKey.setBounds(850,487,140,26);kitEditor.setBounds(18,516,1164,262);sequenceSettings.setBounds(1000,487,146,26);
}
void PocketEditor::selectMode(int mode) {
    if(shifted()&&mode<2){showBrowser(mode==0?2:4);shiftKey.setToggleState(false,juce::dontSendNotification);return;}
    mainMode=mode;
    if(mode>=2) selectPage(4);
    else {const int engineIndex=int(processor.state.getRawParameterValue("engine")->load());
        if((mode==1)!=(engineIndex>=4)){juce::String error;if(!processor.loadSound(processor.lastSound(mode==1),error))processor.applyPreset(mode==1?22:0);browser.syncSelection();}
        selectPage(0);
    }
}
bool PocketEditor::shifted() const {return shiftKey.getToggleState()||juce::ModifierKeys::getCurrentModifiers().isShiftDown();}
int PocketEditor::getControlParameterIndex(juce::Component& control){for(int i=0;i<4;++i)if(&control==&knobs[size_t(i)])return processor.screenKnob(i).getParameterIndex();return -1;}
void PocketEditor::setParameter(const char* id,float value){auto* p=processor.state.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}
void PocketEditor::showError(const juce::String& message){status.setText(message,juce::dontSendNotification);}
void PocketEditor::showBrowser(int kind){kitOpen=false;kitEditor.setVisible(false);browserOpen=true;browser.setVisible(true);setSize(1200,790);browser.showKind(kind);browser.syncSelection();}
void PocketEditor::applySoundSlot(int slot) {
    if(mainMode>=2){applyTapeTrick(slot);return;}
    const bool drums=mainMode==1||processor.state.getRawParameterValue("engine")->load()>=4;
    if(shifted()){if(processor.assignSoundSlot(drums,slot))status.setText("Stored edited sound in "+juce::String(drums?"drum":"synth")+" slot "+juce::String(slot+1),juce::dontSendNotification);else showError("Load a sound before assigning a slot.");shiftKey.setToggleState(false,juce::dontSendNotification);return;}
    juce::String error;if(!processor.recallSoundSlot(drums,slot,error))showError(error);browser.syncSelection();
}
void PocketEditor::doTapeEdit(const juce::String& action){juce::String error;if(!(action=="copySound"?processor.copySound(error):action=="pasteSound"?processor.pasteSound(error):processor.tapeEdit(action,shifted(),error))){showError(error);return;}status.setText(action=="split"?(shifted()?"Joined adjacent takes":"Split take at tape head"):action=="lift"?(shifted()?"Lifted all four tracks":"Lifted take to clipboard"):action=="drop"?"Dropped take; clipboard can be reused":action=="sampler"?"Tape clipboard loaded into sampler":action=="copySound"?"Copied current sound and audio to temporary clipboard":action=="pasteSound"?"Restored copied sound; tape and mixer kept":action=="clear"?"Cleared selected track (undo available)":"Undo / redo tape edit",juce::dontSendNotification);if(action=="sampler"){mainMode=0;selectPage(0);}repaint();}
void PocketEditor::applyTapeTrick(int slot){const bool alternate=shifted();const float position=processor.tapePosition.load(),duration=float(4<<int(processor.state.getRawParameterValue("tapeLength")->load()));
    if(slot==0){if(shifted())processor.seekTape(processor.state.getRawParameterValue("tapeLoopIn")->load());else {setParameter("tapeLoopIn",std::min(position,duration-.01f));if(processor.state.getRawParameterValue("tapeLoopOut")->load()<=position)setParameter("tapeLoopOut",duration);}}
    else if(slot==1){if(shifted())processor.seekTape(processor.state.getRawParameterValue("tapeLoopOut")->load()-.001f);else if(position>.001f){setParameter("tapeLoopOut",position);if(processor.state.getRawParameterValue("tapeLoopIn")->load()>=position)setParameter("tapeLoopIn",0);}}
    else if(slot==2){if(shifted()){const auto range=processor.currentTapeTake();setParameter("tapeLoopIn",range.first);setParameter("tapeLoopOut",range.second);setParameter("tapeLoop",1);}else setParameter("tapeLoop",processor.state.getRawParameterValue("tapeLoop")->load()>.5f?0:1);}
    else if(slot==4){float speed=processor.state.getRawParameterValue("tapeSpeed")->load();setParameter("tapeSpeed",speed==0?-1:-speed);}
    else if(slot>=6){processor.tapeMemo(slot-6,shifted());status.setText(shifted()?"Tape/mixer memory stored":"Tape/mixer memory recalled",juce::dontSendNotification);}
    if(alternate)shiftKey.setToggleState(false,juce::dontSendNotification);
}
void PocketEditor::showComMenu(){juce::PopupMenu menu;menu.addItem(1,"Export stereo tape mix (WAV)");menu.addItem(2,"Export four raw tracks (WAV)");menu.addSeparator();menu.addItem(3,"Save complete session");menu.addItem(4,"Open complete session");juce::Component::SafePointer<PocketEditor> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(comKey),[safe](int choice){if(!safe)return;if(choice==1){safe->chooser=std::make_unique<juce::FileChooser>("Export stereo tape mix",juce::File{},"*.wav");safe->chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe](const juce::FileChooser& fc){if(!safe||fc.getResult()==juce::File{})return;juce::String error;if(!safe->processor.exportMix(fc.getResult().withFileExtension("wav"),error))safe->showError(error);});}else if(choice==2)safe->chooseExport();else if(choice==3)safe->chooseSession(true);else if(choice==4)safe->chooseSession(false);});}
bool PocketEditor::keyPressed(const juce::KeyPress& key){if(dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))return false;const int code=key.getKeyCode();if(code==juce::KeyPress::spaceKey){sequence.triggerClick();return true;}if(code==juce::KeyPress::leftKey||code==juce::KeyPress::rightKey){const int direction=code==juce::KeyPress::leftKey?-1:1;if(mainMode>=2)processor.seekTape(std::max(0.f,processor.tapePosition.load()+direction*(shifted()?240/std::clamp(processor.effectiveTempo.load(),40.f,300.f):.5f)));else setOctave(octave+direction);return true;}return false;}
void PocketEditor::setOctave(int value) {
    octave=juce::jlimit(-2,1,value); const int base=60+octave*12;
    keyboard.setAvailableRange(base,base+23); keyboard.setLowestVisibleKey(base); keyboard.setKeyPressBaseOctave(base/12);
}
void PocketEditor::selectPage(int selected) {
    page=selected;if(page<4&&mainMode>=2)mainMode=processor.state.getRawParameterValue("engine")->load()>=4?1:0;
    for(auto& attachment:attachments)attachment.reset();
    processor.setScreenPage(page,mainMode==2);
    const std::array<std::array<juce::String,4>,8> labels{{{"TONE","COLOR","ENVELOPE","ECHO"},{"ATTACK","DECAY","SUSTAIN","RELEASE"},{"RATE","DEPTH","DESTINATION","SHAPE"},{"DELAY TIME","FEEDBACK","ECHO MIX","DRIVE"},{"TRACK 1","TRACK 2","TRACK 3","TRACK 4"},{"LOW dB","MID dB","HIGH dB","EQ MIX"},{"DRIVE","ECHO MIX","DELAY TIME","FEEDBACK"},{"MASTER","INPUT","REC GAIN","CLICK"}}};
    lastPhase246=processor.phase246Enabled();
    knobLabels=labels[size_t(page)];if(page==0&&lastPhase246)knobLabels={"SHAPE","PRIMARY GAIN","CONTOUR","CYCLES"};
    for(size_t i=0;i<4;++i){knobs[i].onValueChange=nullptr;knobs[i].onDragStart=nullptr;knobs[i].onDragEnd=nullptr;auto id=processor.screenParameterId(int(i));
        if(page==4&&mainMode==2)knobLabels={"POSITION","TRACK LEVEL","SPEED","REC GAIN"};
        if(id.isEmpty()){knobs[i].setRange(0,4<<int(processor.state.getRawParameterValue("tapeLength")->load()),.001);knobs[i].setValue(processor.tapePosition.load(),juce::dontSendNotification);knobs[i].onDragStart=[this]{scrubStart=processor.tapePosition.load();slidingTake=shifted();};knobs[i].onValueChange=[this]{if(knobs[0].isMouseButtonDown()&&!slidingTake)processor.seekTape(knobs[0].getValue());};knobs[i].onDragEnd=[this]{if(slidingTake){juce::String error;if(!processor.slideTape(knobs[0].getValue()-scrubStart,error))showError(error);}slidingTake=false;};}
        else attachments[i]=std::make_unique<juce::SliderParameterAttachment>(processor.screenKnob(int(i)),knobs[i]);knobs[i].setTooltip(processor.screenKnob(int(i)).getName(100)+" follows this page: "+knobLabels[i]+". Map once in your host; sets 1-4 reuse the mapping.");}
    const int tab=mainMode==3?(page==4?0:page-4):page==0?0:page==1?1:page==3?2:page==2?3:-1;
    for(int i=0;i<4;++i) pageButtons[size_t(i)].setColour(juce::TextButton::buttonColourId,i==tab?juce::Colour(0xffc0d0dd):juce::Colour(0xffdbe1e9));
    sequenceAttachment.reset();sequenceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,mainMode>=2?"tapePlay":"sequencer",sequence);
    const bool tapePage=mainMode>=2; engine.setVisible(!tapePage);
    for(juce::Component* c:std::array<juce::Component*,7>{&tapePlay,&overdub,&clear,&exportTape,&tapeTrack,&tapeLength,&tapeSpeed}) c->setVisible(tapePage);
    tapeRecord.setVisible(true);tempo.setVisible(!tapePage);rewind.setVisible(false);
    octaveDown.setTooltip(tapePage?"Rewind tape":"Octave down");octaveUp.setTooltip(tapePage?"Toggle tape playback x1/x2":"Octave up");
    timerCallback();
}
void PocketEditor::timerCallback() {
    if(page!=processor.screenPage()||(page==4&&(mainMode==2)!=processor.screenTapeMode())){mainMode=processor.screenTapeMode()?2:processor.screenPage()>=4?3:processor.state.getRawParameterValue("engine")->load()>=4?1:0;selectPage(processor.screenPage());return;}
    if(lastPhase246!=processor.phase246Enabled()){lastPhase246=processor.phase246Enabled();if(page==0){selectPage(page);return;}}
    processor.updateKeyboardDisplay();
    const int e=int(processor.state.getRawParameterValue("engine")->load());
    cleanSample.setVisible(mainMode<2&&(e==3||e==5));phase246.setVisible(mainMode<2&&e==1);
    const bool showKit=kitOpen&&e==5&&mainMode<2;kitEditor.setVisible(showKit);if(showKit)kitEditor.refresh();browser.setVisible(browserOpen&&!showKit);kitEditKey.setVisible(e==5&&mainMode<2);
    micKey.setButtonText(processor.inputCapturing.load()?"REC":processor.state.getRawParameterValue("inputMonitor")->load()>.5f?"IN ON":"IN");
    const auto selected=processor.selectedSound();if(selected!=lastSelectedSound){lastSelectedSound=selected;browser.syncSelection();}
    const bool tapeMode=mainMode>=2;splitKey.setEnabled(tapeMode);liftKey.setEnabled(true);splitKey.setButtonText(shifted()?"JOIN":"SPLIT");dropKey.setTooltip(tapeMode?"Drop the lifted take at the head":"Load the tape clipboard into the sampler");
    const char* legends[]={"IN","OUT","LOOP","BREAK","REV","CHOP","MEM1","MEM2"};for(int i=0;i<8;++i){slotButtons[size_t(i)].getProperties().set("legend",tapeMode?legends[i]:"");slotButtons[size_t(i)].setTooltip(tapeMode?juce::String(legends[i])+" / Shift: alternate action":"Sound slot "+juce::String(i+1)+". Shift-click stores the edited sound.");}
    loadSample.setVisible(mainMode<2&&e==3); loopSample.setVisible(mainMode<2&&e==3);
    if(page==4&&mainMode==2&&!knobs[0].isMouseButtonDown())knobs[0].setValue(processor.tapePosition.load(),juce::dontSendNotification);
    if(page==0) {knobLabels[0]=lastPhase246?"SHAPE":e==3?"START":"TONE"; knobLabels[1]=lastPhase246?"GAIN":e==3?"LENGTH":"COLOR";}
    const int activeMode=mainMode>=2?mainMode:e>=4?1:0;
    for(int i=0;i<4;++i) modeButtons[size_t(i)].setColour(juce::TextButton::buttonColourId,i==activeMode?juce::Colour(0xffbed0dc):juce::Colour(0xffdbe1e9));
    for(size_t i=0;i<16;++i) {
        stepButtons[i].setVisible(showSequence||shifted());
        stepButtons[i].setEnabled(processor.state.getRawParameterValue("seqMode")->load()<4&&int(i)<int(processor.state.getRawParameterValue("seqLength")->load()));
        const int n=processor.steps[i].load(); const bool current=processor.playhead.load()==int(i);
        stepButtons[i].setColour(juce::TextButton::buttonColourId,current?hardwareColours[0]:n>=0?juce::Colour(0xffc3d5dd):juce::Colour(0xffdbe1e9));
        stepButtons[i].setTooltip("Step "+juce::String(int(i+1))+": "+(n<0?"rest":juce::MidiMessage::getMidiNoteName(n,true,true,4))+". Shift-click writes last played note.");
    }
    sequenceSettings.setButtonText("SEQ: "+processor.state.getParameter("seqMode")->getCurrentValueAsText());
    repaint(279,33,263,135);repaint(1142,100,16,72);
}
void PocketEditor::refreshPresetMenu() {
    browser.refresh();
}
void PocketEditor::chooseBank() {
    chooser=std::make_unique<juce::FileChooser>("Import Pocket One sound bank ZIP",juce::File{},"*.zip");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<PocketEditor>(this)](const juce::FileChooser& fc){
        if(!safe||fc.getResult()==juce::File{}) return;juce::String error;
        if(!safe->processor.importUserBank(fc.getResult(),error)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Sound bank import",error);
        else safe->refreshPresetMenu();
    });
}
void PocketEditor::chooseSamples(){chooser=std::make_unique<juce::FileChooser>("Add WAV / AIFF samples (first 16 seconds per file)",juce::File{},"*.wav;*.aif;*.aiff");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectMultipleItems,[safe=juce::Component::SafePointer<PocketEditor>(this)](const juce::FileChooser& fc){if(!safe||fc.getResults().isEmpty())return;juce::String error;if(!safe->processor.importSamples(fc.getResults(),error))safe->showError(error);else {safe->browser.refresh();safe->showBrowser(3);}});}
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
    frame.engine=int(get("engine"));frame.kitPads=processor.kitPads.load();frame.note=processor.sequenceNote.load()>=0?processor.sequenceNote.load():processor.lastNote.load();frame.voices=processor.visualVoices.load();frame.stage=processor.visualEnvelopeStage.load();frame.step=processor.playhead.load();
    frame.time=processor.visualTime.load();frame.level=processor.visualLevel.load();frame.envelope=processor.visualEnvelope.load();frame.samplePosition=processor.visualSamplePosition.load();frame.lfoPhase=processor.visualLfoPhase.load();
    frame.tapePosition=processor.tapePosition.load();frame.tapeLength=float(4<<int(get("tapeLength")));frame.track=int(get("tapeTrack"));
    frame.tone=get("tone");frame.color=get("color");frame.attack=get("attack");frame.decay=get("decay");frame.sustain=get("sustain");frame.release=get("release");frame.lfoRate=get("lfoRate");frame.lfoShape=int(get("lfoShape"));frame.delay=get("delayTime");frame.feedback=get("delayFeedback");
    frame.phase246=processor.phase246Enabled();if(frame.phase246){frame.tone=processor.phaseParameters().getParameter(pocket::phase246Ids[0])->getValue();frame.color=processor.phaseParameters().getParameter(pocket::phase246Ids[1])->getValue();}
    for(size_t i=0;i<128;++i)frame.sample[i]=processor.samplePreview[i].load(std::memory_order_relaxed);
    for(size_t i=0;i<4;++i){frame.labels[i]=knobLabels[i];frame.values[i]=float(knobs[i].getValue());frame.trackGains[i]=get(("track"+juce::String(int(i+1))).toRawUTF8());}
    {juce::Graphics::ScopedSaveState save(g);g.setOrigin(285,39);g.reduceClipRegion(0,0,251,123);pocket::drawScreen(g,frame);}
    g.setColour(line);g.drawLine(34,230,1130,230,1);g.drawLine(212,233,212,407,1);
    {juce::Graphics::ScopedSaveState save(g);g.addTransform(juce::AffineTransform::rotation(-float(pocket::pi)*.5f).translated(1157,351));text(g,"pocket one",{-50,-7,100,15},10,ink);}
    g.setColour(juce::Colour(0xff596777));for(int y=46;y<61;y+=5)for(int x=1145;x<1158;x+=5)g.fillEllipse(float(x),float(y),1.5f,1.5f);
    const float level=std::min(1.f,std::abs(processor.scope[0].load())*5);
    for(int i=0;i<8;++i){g.setColour(i<int(level*8)?hardwareColours[1]:juce::Colour(0xffb4c0cb));g.fillRoundedRectangle(1164,241.f+i*8,5,4,1);}
    g.setColour(juce::Colour(0xffe2e7ed));g.fillRoundedRectangle(18,439,1164,49,10);
    for(auto point:{juce::Point<float>{28,29},{1172,29},{28,414},{1172,414}}) {g.setColour(line);g.fillEllipse(point.x-2,point.y-2,4,4);}
}

void PocketEditor::showSequenceMenu() {
    juce::PopupMenu menu;
    const auto value=[this](const char* id){return processor.state.getRawParameterValue(id)->load();};
    juce::PopupMenu modes,rates,lengths,gates,swings,octaves,velocities,pulses,rotations;
    const juce::StringArray names{"Forward steps","Reverse steps","Pendulum steps","Random steps","Arpeggio UP","Arpeggio DOWN","Arpeggio UP/DOWN"};
    for(int i=0;i<names.size();++i)modes.addItem(100+i,names[i],true,int(value("seqMode"))==i);
    for(int i=0;i<4;++i)rates.addItem(200+i,juce::StringArray{"1/4","1/8","1/16","1/32"}[i],true,int(value("seqRate"))==i);
    for(int i=1;i<=16;++i)lengths.addItem(300+i,juce::String(i)+" steps",true,int(value("seqLength"))==i);
    for(int i=1;i<=10;++i)gates.addItem(400+i,juce::String(i*10)+"%",true,std::abs(value("seqGate")-i*.1f)<.025f);
    for(int i=0;i<=9;++i)swings.addItem(500+i,juce::String(i*5)+"%",true,std::abs(value("seqSwing")-i*.05f)<.012f);
    for(int i=1;i<=3;++i)octaves.addItem(600+i,juce::String(i),true,int(value("arpOctaves"))==i);
    for(int i=1;i<=10;++i)velocities.addItem(700+i,juce::String(i*10)+"%",true,std::abs(value("seqVelocity")-i*.1f)<.025f);
    for(int i=0;i<=16;++i)pulses.addItem(800+i,juce::String(i)+" pulses",true,int(value("seqPulses"))==i);
    for(int i=0;i<16;++i)rotations.addItem(900+i,juce::String(i)+" steps",true,int(value("seqRotation"))==i);
    menu.addSectionHeader("Sequence / held-note arpeggio");
    menu.addSubMenu("Mode",modes);menu.addSubMenu("Rate",rates);menu.addSubMenu("Pattern length",lengths);menu.addSubMenu("Gate",gates);menu.addSubMenu("Swing",swings);menu.addSubMenu("Arpeggio octaves",octaves);menu.addSubMenu("Velocity",velocities);menu.addSeparator();menu.addItem(1000,"Euclidean rhythm mask",true,value("seqEuclidean")>.5f);menu.addSubMenu("Euclidean pulses",pulses);menu.addSubMenu("Euclidean rotation",rotations);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sequenceSettings),[safe=juce::Component::SafePointer<PocketEditor>(this)](int result){
        if(!safe||result==0)return;
        const char* id=nullptr;float value=0;
        if(result>=100&&result<=106){id="seqMode";value=float(result-100);}
        else if(result>=200&&result<=203){id="seqRate";value=float(result-200);}
        else if(result>=301&&result<=316){id="seqLength";value=float(result-300);}
        else if(result>=401&&result<=410){id="seqGate";value=(result-400)*.1f;}
        else if(result>=500&&result<=509){id="seqSwing";value=(result-500)*.05f;}
        else if(result>=601&&result<=603){id="arpOctaves";value=float(result-600);}
        else if(result>=701&&result<=710){id="seqVelocity";value=(result-700)*.1f;}
        else if(result>=800&&result<=816){id="seqPulses";value=float(result-800);}
        else if(result>=900&&result<=915){id="seqRotation";value=float(result-900);}
        else if(result==1000){id="seqEuclidean";value=safe->processor.state.getRawParameterValue(id)->load()>.5f?0.f:1.f;}
        if(id){safe->setParameter(id,value);safe->showSequence=true;safe->timerCallback();safe->status.setText("Sequence: "+safe->processor.state.getParameter("seqMode")->getCurrentValueAsText()+" | "+safe->processor.state.getParameter("seqRate")->getCurrentValueAsText()+" | hold keys for ARP; PLAY starts",juce::dontSendNotification);}
    });
}
