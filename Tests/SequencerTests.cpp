// SPDX-License-Identifier: AGPL-3.0-only
#include "Sequencer.h"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int length=1;length<=16;++length)for(int direction=0;direction<4;++direction)for(int tick=-128;tick<128;++tick){const int index=pocket::sequenceIndex(tick,length,direction);check(index>=0&&index<length,"Direction escaped pattern length");check(index==pocket::sequenceIndex(tick,length,direction),"Random sequence is not seek reproducible");}
 const int pendulum[]={0,1,2,3,2,1,0,1};for(int i=0;i<8;++i)check(pocket::sequenceIndex(i,4,2)==pendulum[i],"Pendulum repeats endpoints");
 check(pocket::sequenceIndex(0,4,1)==3&&pocket::sequenceIndex(3,4,1)==0,"Reverse order wrong");
 for(double division:{1.,2.,4.,8.})for(double swing:{0.,.15,.45}){const auto a=pocket::sequenceTiming(0,division,swing),b=pocket::sequenceTiming((1+swing+.000001)/division,division,swing),c=pocket::sequenceTiming(2/ division,division,swing);check(a.tick==0&&b.tick==1&&c.tick==2,"Swing changes pair duration");check(pocket::sequenceTiming((1+swing-.000001)/division,division,swing).tick==0,"Swing triggers early");}
 check(pocket::sequenceTiming(-.1,4,0).tick==-1,"Negative host PPQ fails");
 for(int length=1;length<=16;++length)for(int pulses=0;pulses<=length;++pulses)for(int rotation=0;rotation<length;++rotation){int count=0;for(int tick=0;tick<length;++tick)count+=pocket::euclideanPulse(tick,length,pulses,rotation)?1:0;check(count==pulses,"Euclidean mask pulse count changed with rotation");}
 pocket::HeldNotes notes;check(notes.empty()&&notes.note(0,0,3)==-1,"Empty arpeggio is not silent");
 notes.set(1,60,true);notes.set(2,60,true);notes.set(1,64,true);notes.set(1,67,true);
 const int arp[]={60,64,67,72,76,79};for(int i=0;i<6;++i)check(notes.note(i,0,2)==arp[i],"Arpeggio octave order wrong");
 check(notes.note(0,1,1)==67&&notes.note(3,2,1)==64,"Arpeggio directions wrong");
 notes.clear(1);check(!notes.empty()&&notes.note(0,0,1)==60,"Note-off on another MIDI channel lost held key");notes.set(2,60,false);check(notes.empty(),"Held key counter stuck");
 notes.set(1,127,true);check(notes.note(100,0,3)==127,"Octave exceeds MIDI range");notes.clear();check(notes.empty(),"Panic failed");
 std::cout<<"PASS: directions, deterministic random, negative PPQ, swing timing, held keys, channels and octave limits\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
