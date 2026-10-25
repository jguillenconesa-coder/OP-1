// SPDX-License-Identifier: AGPL-3.0-only
#include "Tape.h"
#include <iostream>
#include <stdexcept>
#include <memory>
void check(bool ok,const char* text){if(!ok) throw std::runtime_error(text);}
int main(){try {
 auto t=std::make_unique<pocket::Tape>(); t->prepare(8000); std::array<float,4> gains{1,1,1,1};
 check(t->process(.2f,false,true,false,0,1,4,gains)==.2f,"Stopped tape changed monitor");
 for(int i=0;i<32000;++i) t->process(.25f,true,true,false,0,1,4,gains);
 t->rewind(); check(std::abs(t->process(0,true,false,false,0,1,4,gains)-.25f)<1e-6,"Recorded audio missing");
 t->rewind(); for(int i=0;i<32000;++i) t->process(.1f,true,true,true,0,1,4,gains);
 t->rewind(); check(std::abs(t->process(0,true,false,false,0,1,4,gains)-.35f)<1e-6,"Overdub failed");
 t->rewind(); for(int i=0;i<32000;++i) t->process(.2f,true,true,false,1,1,4,gains);
 t->rewind(); check(std::abs(t->process(0,true,false,false,0,-1,4,gains)-.55f)<1e-6,"Track mix failed");
 t->clear(0); t->rewind(); check(std::abs(t->process(0,true,false,false,0,1,4,gains)-.2f)<1e-6,"Track clear erased wrong material");
 t->prepare(16000); check(std::abs(t->process(0,true,false,false,0,1,4,gains)-.2f)<1e-6,"Sample-rate change lost tape");
 for(int i=0;i<10000;++i) check(std::isfinite(t->process(1,true,true,true,2,-2,4,gains)),"Tape is unstable");
 std::cout<<"PASS: tape record, overdub, mix, clear, reverse and rate conversion\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
