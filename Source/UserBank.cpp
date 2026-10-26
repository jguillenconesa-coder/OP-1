// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
namespace {
bool safeAudioPath(const juce::String& path) {
    return path.startsWith("samples/")&&!path.contains("..")&&!path.containsChar(':')&&!path.containsChar('\\')
        &&(path.endsWithIgnoreCase(".wav")||path.endsWithIgnoreCase(".aif")||path.endsWithIgnoreCase(".aiff"));
}
}
juce::StringArray PocketProcessor::userPresetNames() const {
    juce::StringArray names;for(const auto& p:userPresets) names.add(p.name);return names;
}
void PocketProcessor::refreshUserPresets() {
    userPresets.clear();
    auto banks=libraryDirectory.findChildFiles(juce::File::findDirectories,false);banks.sort();
    for(const auto& bank:banks) {
        auto file=bank.getChildFile("bank.json");if(file.getSize()>2000000) continue;
        auto json=juce::JSON::parse(file.loadFileAsString());if(int(json["formatVersion"])!=1) continue;
        auto* presets=json["presets"].getArray();if(!presets||presets->size()>256) continue;
        for(const auto& item:*presets) {
            UserPreset p;p.bank=json["name"].toString();if(p.bank.isEmpty())p.bank="User bank";p.source=item["source"].toString();p.name=item["name"].toString().substring(0,100);p.parameters=item["parameters"];p.pads=item["pads"];
            if(p.name.isEmpty()||!p.parameters.isObject()) continue;
            auto* slots=item["kit"].getArray();p.isKit=slots!=nullptr;
            bool valid=true;
            if(p.isKit) {
                if(slots->size()!=8&&slots->size()!=24) continue;p.kitCount=slots->size();
                for(int i=0;i<p.kitCount;++i) {const auto path=(*slots)[i].toString();p.kit[size_t(i)]=bank.getChildFile(path);valid&=safeAudioPath(path)&&p.kit[size_t(i)].existsAsFile();}
            } else {const auto path=item["sample"].toString();p.sample=bank.getChildFile(path);valid=safeAudioPath(path)&&p.sample.existsAsFile();}
            p.id="user:"+p.bank+":"+p.name+":"+(p.isKit?p.kit[0].getFileName():p.sample.getFileName());
            if(p.source.isEmpty())p.source=p.isKit?juce::String(p.kitCount)+" samples":p.sample.getFileName();
            if(valid&&std::none_of(userPresets.begin(),userPresets.end(),[&](const auto& existing){return existing.id==p.id;})) userPresets.push_back(std::move(p));
        }
    }
}
bool PocketProcessor::importUserBank(const juce::File& file,juce::String& error) {
    if(file.getSize()>256000000) {error="Bank exceeds the 256 MB limit.";return false;}
    juce::ZipFile zip(file);if(zip.getNumEntries()<2||zip.getNumEntries()>512) {error="Invalid sound bank ZIP.";return false;}
    const int manifestIndex=zip.getIndexOfFileName("bank.json");
    if(manifestIndex<0){error="bank.json is missing.";return false;}
    auto destination=libraryDirectory.getNonexistentChildFile("Imported bank","",false);
    if(!destination.createDirectory()) {error="Cannot create the user sound library.";return false;}
    auto fail=[&](const juce::String& message){error=message;destination.deleteRecursively();return false;};
    juce::int64 total=0;
    for(int i=0;i<zip.getNumEntries();++i) {
        const auto* entry=zip.getEntry(i);const auto name=entry->filename;
        if(name.endsWithChar('/')) continue;
        // Ignore human-readable bank notes; only write supported, bounded assets.
        if(name!="bank.json"&&!safeAudioPath(name)) continue;
        total+=entry->uncompressedSize;
        if(entry->uncompressedSize<0||entry->uncompressedSize>16000000||total>256000000) return fail("Bank audio exceeds the size limit.");
        auto target=destination.getChildFile(name);if(!target.isAChildOf(destination)) return fail("Invalid bank entry path.");
        if(!target.getParentDirectory().createDirectory()) return fail("Cannot create the bank sample folder.");
        std::unique_ptr<juce::InputStream> input(zip.createStreamForEntry(i));juce::MemoryBlock data;
        if(!input||input->readIntoMemoryBlock(data,entry->uncompressedSize+1)!=entry->uncompressedSize) return fail("Cannot read bank entry.");
        if(!target.replaceWithData(data.getData(),data.getSize())) return fail("Cannot write bank entry.");
    }
    auto manifest=destination.getChildFile("bank.json");
    if(!manifest.existsAsFile()||manifest.getSize()>2000000) return fail("bank.json is missing or too large.");
    auto json=juce::JSON::parse(manifest.loadFileAsString());auto* entries=json["presets"].getArray();
    if(int(json["formatVersion"])!=1||!entries||entries->isEmpty()||entries->size()>256) return fail("Unsupported or empty sound bank manifest.");
    // Validate every referenced audio file before making the bank selectable.
    juce::StringArray checked;
    for(const auto& item:*entries) {
        if(item["name"].toString().trim().isEmpty()||!item["parameters"].isObject()) return fail("Invalid preset description.");
        juce::Array<juce::var> paths;if(auto* kit=item["kit"].getArray()) {if(kit->size()!=8&&kit->size()!=24) return fail("Kits require eight or 24 sample slots.");paths=*kit;}else paths.add(item["sample"]);
        if(item.hasProperty("pads")){
            auto* pads=item["pads"].getArray();if(!item["kit"].isArray()||!pads||pads->size()>paths.size())return fail("Pad settings require a kit and at most one object per sample.");
            for(const auto& pad:*pads){if(!pad.isObject())return fail("Invalid pad settings object.");for(const auto* id:{"start","end","gain","pitch","reverse","gated"})if(pad.hasProperty(id)){const auto value=pad[id];if(!(value.isBool()||value.isInt()||value.isInt64()||value.isDouble())||!std::isfinite(double(value)))return fail("Pad settings must be finite numbers or booleans.");}}
        }
        for(const auto& path:paths) {
            const auto name=path.toString();if(!safeAudioPath(name)) return fail("Invalid sample path in bank manifest.");
            if(!checked.contains(name)) {auto sample=readSample(destination.getChildFile(name),error);if(sample.frames.empty()) return fail("Cannot decode "+name+": "+error);checked.add(name);}
        }
    }
    // Fully validate a fresh copy before replacing an identical manifest. This
    // repairs missing/changed audio too, rather than treating it as installed.
    const auto canonical=juce::JSON::toString(json,true);
    for(const auto& installed:libraryDirectory.findChildFiles(juce::File::findDirectories,false)){
        if(installed==destination||installed.isSymbolicLink())continue;
        auto previous=installed.getChildFile("bank.json");
        if(previous.getSize()>2000000||!previous.existsAsFile()||juce::JSON::toString(juce::JSON::parse(previous.loadFileAsString()),true)!=canonical)continue;
        if(!installed.deleteRecursively(false)){refreshUserPresets();error="Bank validated, but an old copy could not be deleted. Close other instances and purge the bank before importing again.";return false;}
    }
    refreshUserPresets();return true;
}
bool PocketProcessor::purgeUserBanks(const juce::String& name,juce::String& error) {
    if(state.getRawParameterValue("tapePlay")->load()>.5f||inputCapturing.load()){error="Stop tape and input capture before purging banks.";return false;}
    juce::StringArray removedIds;
    for(const auto& p:userPresets)if(name.isEmpty()||p.bank==name)removedIds.addIfNotAlreadyThere(p.id);
    bool success=true;
    for(const auto& directory:libraryDirectory.findChildFiles(juce::File::findDirectories,false)){
        if(directory.isSymbolicLink())continue;
        const auto manifest=directory.getChildFile("bank.json");if(!manifest.existsAsFile()||manifest.getSize()>2000000)continue;
        const auto json=juce::JSON::parse(manifest.loadFileAsString());if(int(json["formatVersion"])!=1)continue;
        auto bankName=json["name"].toString();if(bankName.isEmpty())bankName="User bank";
        if(name.isNotEmpty()&&name!=bankName)continue;
        // A damaged/missing sample can disappear from a rescan while its audio
        // is still embedded in the current project. Retire its identity too.
        if(auto* presets=json["presets"].getArray())for(const auto& item:*presets){
            auto path=item["sample"].toString();if(auto* kit=item["kit"].getArray())if(!kit->isEmpty())path=(*kit)[0].toString();
            removedIds.addIfNotAlreadyThere("user:"+bankName+":"+item["name"].toString().substring(0,100)+":"+path.fromLastOccurrenceOf("/",false,false));
        }
        success=directory.deleteRecursively(false)&&success;
    }
    refreshUserPresets();
    // Clear stale references even if an individual file could not be deleted.
    const auto active=selectedSound();
    const bool clearLoaded=removedIds.contains(active);
    {const juce::ScopedLock lock(libraryStateLock);
        for(const auto& id:removedIds)favorites.removeString(id);
        for(int mode=0;mode<2;++mode)for(int slot=0;slot<8;++slot)if(removedIds.contains(soundSlotIds[size_t(mode)][size_t(slot)])){soundSlotIds[size_t(mode)][size_t(slot)]="factory:"+juce::String(mode==0?0:22);slotSettings[size_t(mode)][size_t(slot)]={};}
        if(removedIds.contains(lastSynthSound))lastSynthSound="factory:0";
        if(removedIds.contains(lastDrumSound))lastDrumSound="factory:22";
        if(removedIds.contains(soundClipboardId)){soundClipboard={};soundClipboardId.clear();clipboardKind.store(0);}
        if(clearLoaded){selectedSoundId.clear();for(auto& pad:kitNames)pad="Empty pad";}
    }
    if(clearLoaded){
        pocket::Sample oldSample;pocket::Kit oldKit;
        {const juce::ScopedLock lock(getCallbackLock());engine.swapSample(oldSample);engine.swapKit(oldKit,8);engine.reset();heldNotes.clear();seqNote=-1;sequenceNote.store(-1);}
        publishSamplePreview({});for(int pad=0;pad<pocket::kitCapacity;++pad)publishPadPreview(pad,{});
        sampleFrames.store(0);kitPads.store(8);currentFactoryPreset.store(-1);requestPanic();
    }
    currentUserPreset.store(-1);
    if(!clearLoaded)for(size_t i=0;i<userPresets.size();++i)if(userPresets[i].id==active)currentUserPreset.store(int(i));
    if(!success)error="Some bank files could not be deleted. Close other instances and retry PURGE BANK.";
    return success;
}
bool PocketProcessor::applyUserPreset(int index,juce::String& error) {
    if(index<0||index>=int(userPresets.size())) {error="User preset no longer exists.";return false;}
    const auto& p=userPresets[size_t(index)];
    pocket::Sample sample;pocket::Kit kit;
    if(p.isKit) {for(size_t i=0;i<size_t(p.kitCount);++i) {kit[i]=readSample(p.kit[i],error);if(kit[i].frames.empty()) return false;}}
    else {sample=readSample(p.sample,error);if(sample.frames.empty()) return false;}
    applyPreset(p.isKit?22:18);
    if(!p.isKit) publishSamplePreview(sample);
    if(p.isKit){const juce::ScopedLock lock(libraryStateLock);for(int pad=0;pad<pocket::kitCapacity;++pad)kitNames[size_t(pad)]=pad<p.kitCount?p.kit[size_t(pad)].getFileName():"Empty pad";}
    if(p.isKit)for(int pad=0;pad<pocket::kitCapacity;++pad)publishPadPreview(pad,kit[size_t(pad)]);
    {const juce::ScopedLock lock(getCallbackLock());if(p.isKit){engine.swapKit(kit,p.kitCount);kitPads.store(p.kitCount);}else {sampleFrames.store(int(sample.frames.size()));engine.swapSample(sample);}}
    // Sound changes intentionally preserve output level, tempo, sequence and tape.
    const char* allowed[]={"tone","color","envelope","effect","attack","decay","sustain","release","sampleRoot","sampleLoop","sampleClean","lfoRate","lfoDepth","lfoDestination","lfoShape","delayTime","delayFeedback","drive"};
    for(const auto* id:allowed) if(p.parameters.hasProperty(id)) {
        const float value=float(p.parameters[id]);if(!std::isfinite(value)) continue;
        auto* parameter=state.getParameter(id);parameter->beginChangeGesture();parameter->setValueNotifyingHost(juce::jlimit(0.f,1.f,parameter->convertTo0to1(value)));parameter->endChangeGesture();
    }
    if(p.isKit)if(auto* pads=p.pads.getArray())for(int pad=0;pad<std::min(p.kitCount,pads->size());++pad)for(const auto* field:{"start","end","gain","pitch","reverse","gated"})if((*pads)[pad].hasProperty(field)){auto* parameter=state.getParameter(padParameterId(pad,field));const float value=float((*pads)[pad][field]);if(std::isfinite(value))parameter->setValueNotifyingHost(std::clamp(parameter->convertTo0to1(value),0.f,1.f));}
    setSelectedSound(p.id);currentFactoryPreset.store(-1);currentUserPreset.store(index);requestPanic();return true;
}
