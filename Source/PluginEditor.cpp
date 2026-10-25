// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginEditor.h"
namespace {
const std::array<juce::Colour,4> colours {juce::Colour(0xff508fcd),juce::Colour(0xff64a76b),juce::Colour(0xffdeded3),juce::Colour(0xffce754d)};
void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size,juce::Colour c,juce::Justification j=juce::Justification::centred) {
    g.setColour(c); g.setFont(juce::FontOptions(size)); g.drawText(s,r,j);
}
}
void PocketLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& slider) {
    auto r=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(7);
    const float d=std::min(r.getWidth(),r.getHeight()); r=r.withSizeKeepingCentre(d,d);
    g.setColour(juce::Colours::black.withAlpha(.12f)); g.fillEllipse(r.translated(0,4));
    g.setColour(juce::Colour(0xffaaaead)); g.fillEllipse(r.expanded(2));
    auto color=slider.findColour(juce::Slider::rotarySliderFillColourId);
    g.setGradientFill(juce::ColourGradient(color.brighter(.2f),r.getTopLeft(),color.darker(.18f),r.getBottomRight(),false)); g.fillEllipse(r);
    g.setColour(juce::Colours::white.withAlpha(.16f)); g.drawEllipse(r.reduced(2),1);
    const float angle=start+position*(end-start);
    juce::Path tick; tick.addRoundedRectangle(-1.7f,-d*.38f,3.4f,d*.2f,1.5f);
    g.setColour(juce::Colour(0xffe9ece7)); g.fillPath(tick,juce::AffineTransform::rotation(angle).translated(r.getCentreX(),r.getCentreY()));
}
namespace {
void drawHardwareKey(juce::Graphics& g,juce::Rectangle<float> r,bool down,bool over,bool accidental,int note) {
    r=r.reduced(3,4);
    g.setColour(juce::Colour(0xff8b958d)); g.fillRoundedRectangle(r.translated(0,3),5);
    auto fill=down?juce::Colour(0xff93b4cb):over?juce::Colour(0xfff7f8f1):juce::Colour(0xffe8ebe3);
    g.setGradientFill(juce::ColourGradient(fill.brighter(.12f),r.getTopLeft(),fill.darker(.06f),r.getBottomLeft(),false));
    g.fillRoundedRectangle(r,5);
    g.setColour(juce::Colour(0xfff5f6ef)); g.drawRoundedRectangle(r.reduced(.5f),5,1);
    if(accidental) { g.setColour(juce::Colour(0xff4f5853)); g.fillRoundedRectangle(r.getCentreX()-11,r.getY()+10,22,5,2); }
    else { g.setColour(juce::Colour(0xff859189)); g.setFont(juce::FontOptions(10)); g.drawText(juce::MidiMessage::getMidiNoteName(note,true,true,4),r.reduced(4),juce::Justification::centredBottom); }
}
}
void HardwareKeyboard::drawWhiteNote(int n,juce::Graphics& g,juce::Rectangle<float> r,bool d,bool o,juce::Colour,juce::Colour) {
    drawHardwareKey(g,r.withTrimmedTop(r.getHeight()*.51f),d,o,false,n);
}
void HardwareKeyboard::drawBlackNote(int n,juce::Graphics& g,juce::Rectangle<float> r,bool d,bool o,juce::Colour) {
    drawHardwareKey(g,r,d,o,true,n);
}
bool HardwareKeyboard::mouseDownOnKey(int note,const juce::MouseEvent& event) {
    const int pitch=note%12; const bool accidental=pitch==1||pitch==3||pitch==6||pitch==8||pitch==10;
    return accidental||event.position.y>=getHeight()*.51f;
}
bool HardwareKeyboard::mouseDraggedToKey(int note,const juce::MouseEvent& event) {return mouseDownOnKey(note,event);}
PocketEditor::PocketEditor(PocketProcessor& p) : AudioProcessorEditor(p),processor(p),keyboard(p.keyboard) {
    setLookAndFeel(&look); setSize(1100,450);
    for(size_t i=0;i<4;++i) {
        auto& s=knobs[i]; addAndMakeVisible(s); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); s.setColour(juce::Slider::rotarySliderFillColourId,colours[i]);
    }
    for(size_t i=0;i<5;++i) {
        auto& b=pageButtons[i]; addAndMakeVisible(b); b.setButtonText(juce::StringArray{"SYNTH","ENV","LFO","FX","TAPE"}[int(i)]);
        b.onClick=[this,i]{selectPage(int(i));};
    }
    addAndMakeVisible(presets); presets.setTextWhenNothingSelected("FACTORY SOUNDS");
    presets.addItemList(PocketProcessor::factoryPresetNames(),1);
    presets.setTooltip("25 factory sounds: synthesis, melodic samples and three 8-pad drum kits.");
    presets.onChange=[this]{processor.applyPreset(presets.getSelectedItemIndex());};
    for(auto pair:{std::pair{&loopSample,"sampleLoop"},{&tapePlay,"tapePlay"},{&tapeRecord,"tapeRecord"},{&overdub,"tapeOverdub"}}) {
        addAndMakeVisible(*pair.first); pair.first->setClickingTogglesState(true);
        pair.first->setColour(juce::TextButton::buttonOnColourId,pair.first==&tapeRecord?colours[3]:colours[1].darker());
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,pair.second,*pair.first));
    }
    for(auto* b:{&loadSample,&rewind,&clear,&exportTape,&octaveDown,&octaveUp,&saveSession,&loadSession}) addAndMakeVisible(*b);
    saveSession.onClick=[this]{chooseSession(true);}; loadSession.onClick=[this]{chooseSession(false);};
    saveSession.setTooltip("Save complete session: sound, sample, pattern and four tape tracks."); loadSession.setTooltip("Open a .pocketone session.");
    loadSample.onClick=[this]{chooseSample();}; exportTape.onClick=[this]{chooseExport();};
    rewind.onClick=[this]{processor.rewindTape();}; clear.onClick=[this]{processor.clearTape();};
    clear.setTooltip("Erase the selected tape track. Export or save your project first.");
    octaveDown.onClick=[this]{setOctave(octave-1);}; octaveUp.onClick=[this]{setOctave(octave+1);};
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
    volumeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"volume",volume);
    tempoAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"tempo",tempo);
    addAndMakeVisible(sequence); sequence.setClickingTogglesState(true);
    sequence.setColour(juce::TextButton::buttonOnColourId,colours[1].darker());
    sequenceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"sequencer",sequence);
    addAndMakeVisible(panic); panic.onClick=[this] {
        auto* param=processor.state.getParameter("sequencer"); param->beginChangeGesture(); param->setValueNotifyingHost(0); param->endChangeGesture();
        for(const char* id:{"tapePlay","tapeRecord"}) {auto* parameter=processor.state.getParameter(id); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();}
        processor.keyboard.allNotesOff(0); processor.requestPanic();
    };
    for(size_t i=0;i<16;++i) {
        auto& b=stepButtons[i]; addAndMakeVisible(b); b.setButtonText(juce::String(int(i+1))); b.setTooltip("Click: toggle. Shift-click: write the last played MIDI note.");
        b.onClick=[this,i] { static constexpr int melody[]={60,64,67,72,63,67,70,74,60,72,67,65,63,67,58,62}; const int n=processor.steps[i].load(); processor.steps[i].store(juce::ModifierKeys::getCurrentModifiers().isShiftDown()?processor.lastNote.load():n<0?melody[i]:-1); repaint(); };
    }
    addAndMakeVisible(keyboard); keyboard.setAvailableRange(60,83); keyboard.setLowestVisibleKey(60); keyboard.setScrollButtonsVisible(false); keyboard.setKeyWidth(57); keyboard.setBlackNoteLengthProportion(.47f); keyboard.setBlackNoteWidthProportion(.84f);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xffbec4bf));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,juce::Colour(0xff565b59));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,juce::Colour(0xffadb2af));
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,colours[0].withAlpha(.6f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,colours[0].withAlpha(.15f));
    keyboard.setWantsKeyboardFocus(true);
    setOctave(0); selectPage(0); startTimerHz(30);
}
PocketEditor::~PocketEditor() { stopTimer(); setLookAndFeel(nullptr); }
void PocketEditor::resized() {
    for(int i=0;i<5;++i) pageButtons[size_t(i)].setBounds(220+i*77,18,70,23);
    engine.setBounds(236,65,187,27); loadSample.setBounds(431,65,66,27); loopSample.setBounds(505,65,75,27);
    for(int i=0;i<4;++i) knobs[size_t(i)].setBounds(636+i*100,62,82,82);
    volume.setBounds(40,97,139,28); tempo.setBounds(40,162,139,28);
    presets.setBounds(42,302,136,27); saveSession.setBounds(42,334,62,25); loadSession.setBounds(112,334,62,25);
    octaveDown.setBounds(42,366,62,25); octaveUp.setBounds(112,366,62,25);
    sequence.setBounds(42,252,62,36); panic.setBounds(112,252,62,36);
    keyboard.setBounds(225,264,798,153);
    for(int i=0;i<16;++i) stepButtons[size_t(i)].setBounds(227+i*50,216,43,26);
    tapePlay.setBounds(236,65,48,25); tapeRecord.setBounds(289,65,44,25); overdub.setBounds(338,65,52,25);
    rewind.setBounds(395,65,40,25); clear.setBounds(440,65,62,25); exportTape.setBounds(507,65,72,25);
    tapeTrack.setBounds(236,164,83,25); tapeLength.setBounds(325,164,73,25); tapeSpeed.setBounds(405,164,177,25);
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
    for(int i=0;i<5;++i) pageButtons[size_t(i)].setColour(juce::TextButton::buttonColourId,i==page?juce::Colour(0xff6e8278):juce::Colour(0xffaeb8af));
    const bool tapePage=page==4; engine.setVisible(!tapePage);
    for(juce::Component* c:std::array<juce::Component*,9>{&tapePlay,&tapeRecord,&overdub,&rewind,&clear,&exportTape,&tapeTrack,&tapeLength,&tapeSpeed}) c->setVisible(tapePage);
    timerCallback();
}
void PocketEditor::timerCallback() {
    const int e=int(processor.state.getRawParameterValue("engine")->load());
    presets.setSelectedItemIndex(processor.currentFactoryPreset.load(),juce::dontSendNotification);
    loadSample.setVisible(page!=4&&e==3); loopSample.setVisible(page!=4&&e==3);
    if(page==0) {knobLabels[0]=e==3?"START":"TONE"; knobLabels[1]=e==3?"LENGTH":"COLOR";}
    for(size_t i=0;i<16;++i) {
        const int n=processor.steps[i].load(); const bool current=processor.playhead.load()==int(i);
        stepButtons[i].setColour(juce::TextButton::buttonColourId,current?colours[2]:n>=0?juce::Colour(0xff798780):juce::Colour(0xffb5bdb7));
        stepButtons[i].setTooltip("Step "+juce::String(int(i+1))+": "+(n<0?"rest":juce::MidiMessage::getMidiNoteName(n,true,true,4))+". Shift-click writes last played note.");
    }
    repaint();
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
    g.fillAll(juce::Colour(0xff272d2c));
    auto body=getLocalBounds().toFloat().reduced(10);
    g.setColour(juce::Colours::black.withAlpha(.3f)); g.fillRoundedRectangle(body.translated(0,4),20);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe0e3de),0,10,juce::Colour(0xffbec4bf),0,450,false)); g.fillRoundedRectangle(body,18);
    g.setColour(juce::Colour(0xffeef0e9)); g.drawRoundedRectangle(body.reduced(1),17,1);
    const auto ink=juce::Colour(0xff424b48);
    text(g,"pocket one",{40,30,158,28},24,ink,juce::Justification::centredLeft);
    text(g,"POLYPHONIC SYNTHESIZER",{42,59,150,16},9,ink,juce::Justification::centredLeft);
    text(g,"OUTPUT",{43,80,130,16},10,ink,juce::Justification::centredLeft);
    text(g,"TEMPO / HOST SYNC",{43,145,130,16},10,ink,juce::Justification::centredLeft);
    for(int i=0;i<4;++i) {
        text(g,juce::String(i+1),{659+i*100,36,34,20},13,ink);
        text(g,knobLabels[size_t(i)],{627+i*100,154,100,17},11,ink);
        juce::String readout=juce::String(knobs[size_t(i)].getValue(),page==2&&i>=2?0:2);
        if(page==2&&i==2) readout=knobs[2].getValue()<.5?"PITCH":"FILTER";
        if(page==2&&i==3) readout=juce::StringArray{"SINE","TRI","SQUARE"}[juce::jlimit(0,2,int(knobs[3].getValue()))];
        text(g,readout,{627+i*100,174,100,17},11,colours[size_t(i)].darker(.35f));
    }
    g.setColour(juce::Colour(0xff84908b)); g.fillRoundedRectangle(218,48,384,149,9);
    g.setColour(juce::Colour(0xff101a1d)); g.fillRoundedRectangle(222,52,376,141,6);
    if(page!=4&&int(processor.state.getRawParameterValue("engine")->load())!=3) text(g,"SOUND / 8 VOICES",{432,68,149,18},10,juce::Colour(0xff9eaaa6));
    if(page==4) {
        const float seconds=float(4<<int(processor.state.getRawParameterValue("tapeLength")->load()));
        for(int t=0;t<4;++t) {
            const int y=96+t*13; g.setColour(colours[size_t(t)].withAlpha(.6f)); g.fillRoundedRectangle(238,float(y),342,9,2);
            g.setColour(juce::Colour(0xff101a1d)); g.fillRect(240,y+2,juce::jlimit(0,338,int(338*processor.tapePosition.load()/seconds)),5);
        }
        text(g,juce::String(processor.tapePosition.load(),2)+" / "+juce::String(seconds,0)+" s",{438,145,140,16},10,juce::Colours::white);
    } else {
    g.setColour(juce::Colour(0xff263639));
    for(int x=238;x<586;x+=22) g.drawVerticalLine(x,102,167);
    for(int y=107;y<170;y+=16) g.drawHorizontalLine(y,237,584);
    juce::Path wave;
    const int e=int(processor.state.getRawParameterValue("engine")->load());
    juce::String caption;
    if(page==1) {
        const float attack=processor.state.getRawParameterValue("attack")->load(),decay=processor.state.getRawParameterValue("decay")->load(),sustain=processor.state.getRawParameterValue("sustain")->load(),release=processor.state.getRawParameterValue("release")->load();
        const float x1=247+attack*80,x2=x1+25+decay*60,x3=514-release*45;
        wave.startNewSubPath(238,163);wave.lineTo(x1,108);wave.lineTo(x2,163-sustain*55);wave.lineTo(x3,163-sustain*55);wave.lineTo(582,163);
        caption="AMPLITUDE ENVELOPE / ADSR";
    } else if(page==2) {
        const int shape=int(processor.state.getRawParameterValue("lfoShape")->load());
        for(int i=0;i<256;++i) {
            const double phase=double(i)/255*2,wrapped=phase-std::floor(phase);
            const double value=shape==1?1-4*std::abs(wrapped-.5):shape==2?(wrapped<.5?1:-1):std::sin(2*pocket::pi*phase);
            const float x=238+float(i)*346/255,y=137-float(value)*27;
            if(i==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);
        }
        caption="LFO / "+juce::String(processor.state.getRawParameterValue("lfoRate")->load(),2)+" Hz";
    } else if(page==3) {
        text(g,"IN  >  DRIVE  >  ECHO  >  OUT",{238,108,346,20},14,juce::Colour(0xffd9aa34));
        const float feedback=processor.state.getRawParameterValue("delayFeedback")->load();
        for(int i=0;i<12;++i){g.setColour(colours[3].withAlpha(std::pow(feedback,float(i))));g.fillRect(242+i*28,143,3,21);}
        caption="DELAY / "+juce::String(processor.state.getRawParameterValue("delayTime")->load()*1000,0)+" ms";
    } else {
        for(size_t i=0;i<256;++i) {
            const float x=238+float(i)*346/255,y=137-processor.scope[i].load(std::memory_order_relaxed)*79;
            if(i==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);
        }
        caption=juce::StringArray{"FREQUENCY MODULATION","PHASE DISTORTION","PLUCKED STRING",processor.sampleFrames.load()>0?"SAMPLE / ROOT C4":"LOAD A WAV / AIFF TO PLAY","C/G: KICK  D/E: SNARE  OTHER: HAT","8 PADS / C4-G4 + GM DRUM NOTES"}[e];
    }
    g.setColour(colours[size_t(page==0?e%4:(page-1)%4)]);g.strokePath(wave,juce::PathStrokeType(1.7f));
    text(g,caption,{238,170,346,16},9,juce::Colour(0xff95a4a3),juce::Justification::centredLeft);
    }
    text(g,"16 STEP",{41,216,137,20},11,ink,juce::Justification::centredLeft);
    text(g,"MIDI / OCT "+juce::String(octave)+" / BEND +/-2",{40,398,148,16},10,ink,juce::Justification::centredLeft);
    text(g,"POCKET INSTRUMENTS   /   01",{817,422,206,14},8,ink,juce::Justification::centredRight);
    for(auto point : {juce::Point<float>{27,28},{1073,28},{27,422},{1073,422}}) {
        g.setColour(juce::Colour(0xff9ba59d)); g.fillEllipse(point.x-3,point.y-3,6,6);
        g.setColour(juce::Colour(0xff6f7a72)); g.drawLine(point.x-1.5f,point.y,point.x+1.5f,point.y,1);
    }
}
