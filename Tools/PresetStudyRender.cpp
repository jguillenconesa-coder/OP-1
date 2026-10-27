// SPDX-License-Identifier: AGPL-3.0-only
// Deterministic offline rendering of the current Phase/Digital CORE integration.
// Stored oscillator + first four ADSR controls; no original FX/LFO/play mode.
#include "Engine.h"
#include "Digital246Presets.h"
#include "Phase246Presets.h"
#include "Phase246Envelopes.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
int integer(const char* text) {
    size_t used=0;
    const std::string value(text);
    const int result=std::stoi(value,&used);
    if(used!=value.size())throw std::runtime_error("Expected integer argument");
    return result;
}
void word(std::ostream& output,uint32_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;++i)output.put(char((value>>(i*8))&255));
}
}
int main(int argc,char** argv) {try {
    if(argc!=9)throw std::runtime_error(
        "Usage: preset_study_render NEW.wav RATE phase|digital PRESET_INDEX MIDI_NOTE VELOCITY GATE_FRAMES TOTAL_FRAMES");
    const int rate=integer(argv[2]),index=integer(argv[4]),note=integer(argv[5]);
    const int velocity=integer(argv[6]),gate=integer(argv[7]),frames=integer(argv[8]);
    const std::string family(argv[3]);
    if(rate<8000||rate>192000||index<0||index>=16||note<0||note>127||
       velocity<1||velocity>127||gate<1||gate>=frames||frames>rate*16||
       (family!="phase"&&family!="digital"))
        throw std::runtime_error("Invalid family, patch, note, velocity or render dimensions");
    const std::filesystem::path path(argv[1]);
    if(std::filesystem::exists(path))throw std::runtime_error("Choose a new output path");
    auto engine=std::make_unique<pocket::Engine>();
    engine->prepare(rate);
    pocket::Parameters controls;
    controls.engine=family=="phase"?1:0;
    controls.effect=controls.drive=controls.lfoDepth=0;
    controls.volume=1;
    if(family=="phase"){
        controls.phase246=controls.phase246Envelope=true;
        controls.phase246Controls=pocket::phase246Presets[size_t(index)].controls;
        controls.phase246Adsr=pocket::phase246Envelopes[size_t(index)];
    }else{
        controls.digital246=controls.digital246Envelope=true;
        controls.digital246Controls=pocket::digital246Presets[size_t(index)].controls;
        controls.digital246Adsr=pocket::digital246Presets[size_t(index)].adsr;
    }
    std::ofstream output(path,std::ios::binary);
    if(!output)throw std::runtime_error("Cannot create output WAV");
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
    output.write("RIFF",4);word(output,uint32_t(50+frames*4),4);output.write("WAVEfmt ",8);
    word(output,18,4);word(output,3,2);word(output,1,2);word(output,uint32_t(rate),4);
    word(output,uint32_t(rate*4),4);word(output,4,2);word(output,32,2);
    word(output,0,2);output.write("fact",4);word(output,4,4);word(output,uint32_t(frames),4);
    output.write("data",4);word(output,uint32_t(frames*4),4);
    engine->noteOn(note,float(velocity)/127);
    for(int i=0;i<frames;++i){
        if(i==gate)engine->noteOff(note);
        const float sample=engine->next(controls);
        if(!std::isfinite(sample))throw std::runtime_error("Nonfinite render sample");
        uint32_t bits=0;std::memcpy(&bits,&sample,4);word(output,bits,4);
    }
    output.close();
    if(!output)throw std::runtime_error("Cannot finish WAV");
    const char* name=family=="phase"?pocket::phase246Presets[size_t(index)].name:
                                         pocket::digital246Presets[size_t(index)].name;
    std::cout<<family<<" / "<<name<<", MIDI "<<note<<", velocity "<<velocity
             <<", "<<rate<<" Hz, gate "<<gate<<", frames "<<frames
             <<"; CORE integration only\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
