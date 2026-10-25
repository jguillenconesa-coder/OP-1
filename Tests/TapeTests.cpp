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
 auto edit=std::make_unique<pocket::Tape>();edit->prepare(8000);
 for(int i=0;i<8000;++i)edit->process(.25f,true,true,false,0,1,4,gains);
 edit->process(0,false,false,false,0,1,4,gains);check(edit->getCuts()[0].size()==1,"Recorded take end was not marked");
 edit->seek(.5);check(edit->split(0,false,4),"Take split failed");check(edit->getCuts()[0].size()==2,"Split boundary missing");
 edit->seek(.25);check(edit->lift(0,false,4,false),"Lift failed");check(edit->getTracks()[0][0]==0&&edit->getTracks()[0][5000]==.25f,"Lift erased outside its take");
 check(edit->undo(),"Tape undo failed");check(edit->getTracks()[0][0]==.25f,"Undo did not restore lifted audio");
 edit->seek(2);check(edit->drop(1,4),"Drop to another track failed");check(edit->getTracks()[1][16000]==.25f,"Drop changed take audio");check(std::abs(edit->getPosition()-2.5)<.001,"Drop did not advance head");
 edit->seek(3);check(edit->drop(1,4),"Tape clipboard was consumed by first drop");
 edit->seek(.25);check(edit->slide(0,1.5,4),"Slide into an empty region failed");check(edit->getTracks()[0][0]==0&&edit->getTracks()[0][12000]==.25f,"Slide failed to move audio");
 edit->seek(1.6);check(edit->split(0,true,4),"Join failed");
 edit->setRegion(true,1,2);edit->seek(1.1);for(int i=0;i<30000;++i)edit->process(0,true,false,false,0,1,4,gains);check(edit->getPosition()>=1&&edit->getPosition()<=2,"Custom loop escaped bounds");
 edit->seek(1.25);edit->setChop(true,120);for(int i=0;i<10000;++i)edit->process(0,true,false,false,0,1,4,gains);check(edit->getPosition()>=1.25&&edit->getPosition()<=1.376,"Chop escaped tempo grain");
 std::cout<<"PASS: tape record, overdub, mix, clear, reverse and rate conversion\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
