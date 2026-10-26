// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
struct SoundInfo {
    juce::String id,name,bank,category,variant,source;
    int factory=-1,user=-1,phase246=-1,digital246=-1,punch246=-1,value246=-1,engine=0;
    bool sample=false,kit=false;
    float root=60;
};
