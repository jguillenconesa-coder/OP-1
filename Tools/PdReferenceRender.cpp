// SPDX-License-Identifier: AGPL-3.0-only
// Offline study renderer. Tuning is a host NCO convention, not recovered OP-1
// note preparation. No original envelope/FX/LFO or control conversion is applied.
#include "ReferenceDsp.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int integer(const char* text) {
    size_t used=0;const std::string value(text);const int result=std::stoi(value,&used);
    if(used!=value.size())throw std::runtime_error("Expected an integer argument");
    return result;
}
int main(int argc,char** argv) {try {
    if(argc!=9)throw std::runtime_error("Usage: pd_reference_render NEW.pcm RATE MIDI_NOTE FRAMES BLUE GREEN WHITE ORANGE");
    const int rate=integer(argv[2]),note=integer(argv[3]),frames=integer(argv[4]);
    if(rate<8000||rate>192000||note<0||note>127||frames<1||frames>rate*16)throw std::runtime_error("Render dimensions outside limits");
    const double frequency=440*std::exp2((note-69)/12.0);
    if(frequency>=rate*.49)throw std::runtime_error("Note is too close to or above Nyquist at this rate");
    std::array<int16_t,4> raw{};
    for(size_t i=0;i<4;++i){const auto value=integer(argv[i+5]);if(value<-32768||value>32767)throw std::runtime_error("Control outside signed16 range");raw[i]=int16_t(value);}
    const std::filesystem::path path(argv[1]);if(std::filesystem::exists(path))throw std::runtime_error("Choose a new output path");
    std::array<int16_t,8192> table{};
    const double pi=std::acos(-1.0);
    for(size_t i=0;i<table.size();++i)table[i]=int16_t(std::lround(std::sin(double(i)*pi/4096)*32767));
    const auto increment=uint32_t(std::llround(frequency/rate*4294967296.0));
    const pocket::reference::PdCoefficients coefficients(raw);pocket::reference::PdState state;
    std::ofstream output(path,std::ios::binary);if(!output)throw std::runtime_error("Cannot create output PCM");
    for(int frame=0;frame<frames;++frame){const auto value=uint16_t(pocket::reference::pdSample(state,increment,coefficients,table));const char bytes[2]={char(value&255),char(value>>8)};output.write(bytes,2);}
    output.close();if(!output)throw std::runtime_error("Could not finish PCM output");
    std::cout<<"Rendered "<<frames<<" frames, "<<rate<<" Hz, MIDI "<<note<<", increment "<<increment<<"; raw Pd core study only\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
