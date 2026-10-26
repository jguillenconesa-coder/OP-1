// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Stable host identity, with the range and value of the currently visible control.
// The parameter is owned by AudioProcessor, after the existing APVTS parameters.
class ScreenKnobParameter final : public juce::RangedAudioParameter,
                                  private juce::AudioProcessorParameter::Listener {
public:
    ScreenKnobParameter(const juce::String& id,const juce::String& name)
        : RangedAudioParameter(juce::ParameterID{id,1},name,juce::AudioProcessorParameterWithIDAttributes{}) {}
    void disconnect() {
        if(trackMode.exchange(false))for(auto* p:tracks)p->removeListener(this);
        if(auto* p=alternative.exchange(nullptr))p->removeListener(this);
        if(auto* p=digitalAlternative.exchange(nullptr))p->removeListener(this);
        if(auto* p=target.exchange(nullptr))p->removeListener(this);
    }
    void follow(juce::RangedAudioParameter* next) {
        if(!trackMode.load()&&!alternative.load()&&!digitalAlternative.load()&&currentTarget()==next)return;
        disconnect();target.store(next);
        if(next){next->addListener(this);sendValueChangedMessageToListeners(next->getValue());}
    }
    void followAlternative(juce::RangedAudioParameter* base,juce::RangedAudioParameter* other,std::atomic<float>& enabled,std::atomic<float>& engine,std::atomic<float>* prerequisite=nullptr) {
        disconnect();selection=&enabled;engineSelection=&engine;guard.store(prerequisite);target.store(base);alternative.store(other);
        base->addListener(this);other->addListener(this);sendValueChangedMessageToListeners(getValue());
    }
    void followSwitch(juce::RangedAudioParameter* base,juce::RangedAudioParameter* other,std::atomic<float>& enabled) {
        disconnect();selection=&enabled;engineSelection=nullptr;guard=nullptr;target=base;alternative=other;
        base->addListener(this);other->addListener(this);sendValueChangedMessageToListeners(getValue());
    }
    void followNative(juce::RangedAudioParameter* base,juce::RangedAudioParameter* phase,juce::RangedAudioParameter* digital,
                      std::atomic<float>& phaseEnabled,std::atomic<float>& digitalEnabled,std::atomic<float>& engine,
                      std::atomic<float>* phaseGuard=nullptr,std::atomic<float>* digitalGuard=nullptr) {
        followAlternative(base,phase,phaseEnabled,engine,phaseGuard);
        digitalSelection=&digitalEnabled;digitalPrerequisite.store(digitalGuard);digitalAlternative.store(digital);
        digital->addListener(this);sendValueChangedMessageToListeners(getValue());
    }
    void enableTapeTracks(const std::array<juce::RangedAudioParameter*,4>& parameters,std::atomic<float>& selection) {tracks=parameters;trackSelection=&selection;}
    void followTapeTrack() {
        disconnect();trackMode.store(true);for(auto* p:tracks)p->addListener(this);
        sendValueChangedMessageToListeners(getValue());
    }
    juce::RangedAudioParameter* followedParameter() const {return currentTarget();}
    void enableTapeCursor(std::atomic<float>& position,std::atomic<float>& pending,std::atomic<float>& length) {cursor=&position;seek=&pending;tapeLength=&length;}
    float getValue() const override {if(auto* p=currentTarget())return p->getValue();return cursor?std::clamp(cursor->load()/duration(),0.f,1.f):0;}
    void setValue(float value) override {if(auto* p=currentTarget())p->setValueNotifyingHost(std::clamp(value,0.f,1.f));else if(seek)seek->store(std::clamp(value,0.f,1.f)*duration());}
    float getDefaultValue() const override {if(auto* p=currentTarget())return p->getDefaultValue();return 0;}
    juce::String getText(float value,int length) const override {if(auto* p=currentTarget())return p->getText(value,length);return juce::String(value*duration(),2)+" s";}
    float getValueForText(const juce::String& text) const override {if(auto* p=currentTarget())return p->getValueForText(text);return std::clamp(text.getFloatValue()/duration(),0.f,1.f);}
    int getNumSteps() const override {return juce::AudioProcessor::getDefaultNumParameterSteps();}
    const juce::NormalisableRange<float>& getNormalisableRange() const override {
        if(auto* p=currentTarget())return p->getNormalisableRange();return fallback;
    }
private:
    void parameterValueChanged(int index,float value) override {
        if(auto* p=currentTarget())if(p->getParameterIndex()==index)sendValueChangedMessageToListeners(value);
    }
    void parameterGestureChanged(int,bool) override {}
    std::atomic<juce::RangedAudioParameter*> target{nullptr};
    std::atomic<juce::RangedAudioParameter*> alternative{nullptr};
    std::atomic<juce::RangedAudioParameter*> digitalAlternative{nullptr};
    std::atomic<std::atomic<float>*> digitalSelection{nullptr},digitalPrerequisite{nullptr};
    std::atomic<std::atomic<float>*> selection{nullptr},engineSelection{nullptr},guard{nullptr};
    std::array<juce::RangedAudioParameter*,4> tracks{};
    std::atomic<float>* trackSelection=nullptr;
    std::atomic<bool> trackMode{false};
    juce::RangedAudioParameter* currentTarget() const {if(trackMode.load())return tracks[size_t(std::clamp(int(trackSelection->load()),0,3))];
        if(auto* p=digitalAlternative.load())if(digitalSelection.load()->load()>.5f&&int(engineSelection.load()->load())==0){auto* condition=digitalPrerequisite.load();if(!condition||condition->load()>.5f)return p;}
        if(auto* p=alternative.load())if(selection.load()->load()>.5f){auto* engine=engineSelection.load();if(!engine||int(engine->load())==1){auto* condition=guard.load();if(!condition||condition->load()>.5f)return p;}}
        return target.load();}
    std::atomic<float>* cursor=nullptr,*seek=nullptr,*tapeLength=nullptr;
    float duration() const {return tapeLength?float(4<<std::clamp(int(tapeLength->load()),0,2)):1.f;}
    const juce::NormalisableRange<float> fallback{0.f,1.f};
};
