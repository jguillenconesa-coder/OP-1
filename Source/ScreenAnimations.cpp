// SPDX-License-Identifier: AGPL-3.0-only
// Original JUCE drawings inspired by OTTO-1's vector, parameter and transport-driven screens.
#include "ScreenAnimations.h"
#include "Engine.h"
namespace pocket {
namespace {
const std::array<juce::Colour,4> colors{juce::Colour(0xff39b6eb),juce::Colour(0xff60d2a2),juce::Colour(0xffe4eaf5),juce::Colour(0xfff08c47)};
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size,juce::Colour color) {g.setColour(color);g.setFont(juce::FontOptions(size));g.drawText(s,r,juce::Justification::centredLeft);}
void dot(juce::Graphics& g,float x,float y,float radius) {g.fillEllipse(x-radius,y-radius,radius*2,radius*2);}
}
void drawScreen(juce::Graphics& g,const ScreenFrame& f) {
    g.fillAll(juce::Colour(0xff070b10));
    const auto title=f.page>=5?juce::StringArray{"MASTER EQ","MASTER FX","MASTER OUT"}[std::clamp(f.page-5,0,2)]:f.page==4?(f.mode==3?"MIX":"TAPE"):f.page==1?"ENVELOPE":f.page==2?"LFO":f.page==3?(f.punch246?"PUNCH 246":"ECHO / DRIVE"):juce::StringArray{f.digital246?"DIGITAL 246 CORE":"FM / REACTOR",f.phase246?"PHASE 246 CORE":"PHASE","STRING","SAMPLER","DRUM","SAMPLE KIT"}[std::clamp(f.engine,0,5)];
    label(g,title,{10,4,172,14},10,colors[2]);label(g,juce::String(f.voices)+" VOICES",{183,5,63,12},8,colors[0]);
    const float activity=std::clamp(f.level*6,0.f,1.f),phase=f.time*.7f;
    juce::Path curve;
    if(f.page>=5){for(int i=0;i<4;++i){g.setColour(colors[size_t(i)]);const float value=f.values[size_t(i)];const float normalized=f.page==5&&i<3?(value+12)/24:std::clamp(value,0.f,1.f);g.drawRoundedRectangle(18.f+i*58,27,40,59,3,1);g.fillRoundedRectangle(21.f+i*58,82-normalized*51,34,std::max(1.f,normalized*51),2);} }
    else if(f.page==4) {
        if(f.mode==3) {
            for(int i=0;i<4;++i){label(g,juce::String(i+1),{10,24+i*17,13,14},9,colors[size_t(i)]);g.setColour(colors[size_t(i)].withAlpha(.22f));g.fillRoundedRectangle(29,27.f+i*17,206,9,2);g.setColour(colors[size_t(i)]);g.fillRoundedRectangle(29,27.f+i*17,206*f.trackGains[size_t(i)]*(.35f+.65f*activity),9,2);}
        } else {
            const float progress=std::clamp(f.tapePosition/f.tapeLength,0.f,1.f);
            for(int reel=0;reel<2;++reel){const float x=58.f+reel*133,y=54,r=29;g.setColour(colors[2]);g.drawEllipse(x-r,y-r,r*2,r*2,1);g.drawEllipse(x-6,y-6,12,12,1);dot(g,x,y,2);
                g.setColour(colors[reel==0?0:2].withAlpha(.4f));const float wound=12+12*(reel==0?1-progress:progress);g.drawEllipse(x-wound,y-wound,wound*2,wound*2,2);
                g.setColour(colors[2]);for(int spoke=0;spoke<3;++spoke){const float angle=f.tapePosition*float(pi)+spoke*float(pi)*2/3;g.drawLine(x+std::cos(angle)*9,y+std::sin(angle)*9,x+std::cos(angle)*25,y+std::sin(angle)*25,1);}}
            g.setColour(colors[2].withAlpha(.6f));g.drawLine(87,54,162,54,1);g.drawLine(58,83,191,83,1);
            for(int track=0;track<4;++track){g.setColour(colors[size_t(track)].withAlpha(track==f.track?1.f:.4f));g.drawHorizontalLine(89+track*3,13,235);}
            g.setColour(colors[0]);g.drawVerticalLine(int(13+progress*222),88,101);
            label(g,juce::String(f.tapePosition,2)+"s",{100,67,57,13},9,colors[1]);
        }
    } else if(f.page==1) {
        const float a=13+f.attack*55,d=a+20+f.decay*40,s=211-f.release*32,end=237,y=84,top=28,sustain=y-f.sustain*56;
        curve.startNewSubPath(12,y);curve.lineTo(a,top);curve.lineTo(d,sustain);curve.lineTo(s,sustain);curve.lineTo(end,y);
        g.setColour(colors[0]);g.strokePath(curve,juce::PathStrokeType(1.4f));
        for(auto point:{juce::Point<float>{a,top},{d,sustain},{s,sustain}})dot(g,point.x,point.y,2);
        float x=f.stage==1?12+f.envelope*(a-12):f.stage==2?a+(1-f.envelope)/std::max(.001f,1-f.sustain)*(d-a):f.stage==4?s+(1-f.envelope/std::max(.001f,f.sustain))*(end-s):s;
        g.setColour(colors[3]);if(f.voices>0)dot(g,std::clamp(x,12.f,end),y-f.envelope*56,3.2f);
    } else if(f.page==2&&f.value246) {
        const auto colour=colors[size_t(std::clamp(f.valueColour,0,3))];
        for(int i=0;i<225;++i){const float x=13.f+i,y=56-float(std::sin(2*pi*(double(i)/224-f.lfoPhase)))*23;if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);}
        g.setColour(colour.withAlpha(f.valueDestination?1.f:.3f));g.strokePath(curve,juce::PathStrokeType(1.4f));
        g.fillEllipse(123,52-float(std::sin(2*pi*f.lfoPhase))*23,8,8);
        label(g,"VALUE 246 > "+juce::String(!f.valueDestination?"OFF":f.valueVoice?(f.valueEnvelope?"ADSR":"INSTRUMENT"):"PUNCH"),{15,25,220,12},9,colour);
        label(g,juce::String(f.lfoRate,2)+" Hz  /  "+juce::StringArray{"BLUE","GREEN","WHITE","ORANGE"}[std::clamp(f.valueColour,0,3)],{15,86,220,12},8,colors[2]);
    } else if(f.page==2) {
        for(int i=0;i<225;++i){const double p=double(i)/224*2,w=p-std::floor(p);const float v=float(f.lfoShape==1?1-4*std::abs(w-.5):f.lfoShape==2?(w<.5?1:-1):std::sin(2*pi*p));const float x=13.f+i,y=56-v*25;if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);}
        g.setColour(colors[1]);g.strokePath(curve,juce::PathStrokeType(1.4f));g.setColour(colors[3]);g.drawVerticalLine(int(13+f.lfoPhase*224),26,85);label(g,juce::String(f.lfoRate,2)+" Hz",{15,86,100,12},8,colors[2]);
    } else if(f.page==3&&f.punch246) {
        const int stages=std::clamp(int(f.values[2])/1024,1,24);
        for(int i=0;i<24;++i){const auto x=15.f+i*9.2f;g.setColour(colors[size_t(i%4)].withAlpha(i<stages?.4f+activity*.6f:.12f));const auto h=8+activity*(10+10*std::sin(phase+i*.4f));g.fillRoundedRectangle(x,56-h,6,2*h,2);}
        label(g,juce::String(stages)+" STAGES",{15,83,100,10},8,colors[2]);
    } else if(f.page==3) {
        const std::array<float,3> x{32,126,220};for(int i=0;i<3;++i){g.setColour(colors[size_t(i)]);g.drawEllipse(x[size_t(i)]-12,44,24,24,1);dot(g,x[size_t(i)],56,2+activity*4);}
        g.setColour(colors[2].withAlpha(.4f));g.drawLine(44,56,114,56,1);g.drawLine(138,56,208,56,1);
        for(int i=0;i<8;++i){const float progress=std::fmod(f.time/std::max(.05f,f.delay)+i*.125f,1.f);g.setColour(colors[3].withAlpha((.1f+activity*.9f)*std::pow(f.feedback,float(i))));dot(g,45+progress*163,56,2.2f);}
        label(g,"DRIVE",{17,75,45,11},8,colors[0]);label(g,"ECHO",{111,75,45,11},8,colors[1]);label(g,"OUT",{210,75,35,11},8,colors[2]);
    } else if(f.engine==3) {
        for(size_t i=0;i<f.sample.size();++i){const float x=13+float(i)*224/127,amplitude=f.sample[i]*26;g.setColour(colors[2].withAlpha(.7f));g.drawLine(x,56-amplitude,x,56+amplitude,1);}
        const float start=f.tone*.9f,end=start+(1-start)*f.color;
        g.setColour(colors[0]);g.drawVerticalLine(int(13+start*224),27,86);g.setColour(colors[1]);g.drawVerticalLine(int(13+end*224),27,86);
        if(f.voices>0){g.setColour(colors[3]);g.drawVerticalLine(int(13+f.samplePosition*224),24,89);}
    } else if(f.engine>=4) {
        const int pad=f.engine==5?Engine::kitSlot(f.note,f.kitPads):f.note%12==0||f.note%12==7?0:f.note%12==2||f.note%12==4?1:2;
        if(f.engine==5&&f.kitPads==24){for(int i=0;i<24;++i){const float x=15.f+(i%8)*28,y=29.f+(i/8)*23;const bool hit=i==pad&&f.voices>0;g.setColour(colors[size_t(i%4)].withAlpha(hit?.5f+activity*.5f:.3f));g.drawRoundedRectangle(x,y,23,19,2,1);if(hit)g.fillRoundedRectangle(x+1,y+1,21,17,2);label(g,juce::String(i+1),{int(x+2),int(y+3),19,12},8,hit?juce::Colour(0xff071015):colors[size_t(i%4)]);}}
        else for(int i=0;i<8;++i){const float x=15.f+i*28;const bool hit=i==pad&&f.voices>0;g.setColour(colors[size_t(i%4)].withAlpha(hit?.5f+activity*.5f:.23f));g.drawRoundedRectangle(x,33,23,43,4,1);if(hit){g.fillRoundedRectangle(x+2,57-activity*20,19,17+activity*20,2);g.drawEllipse(x+4-activity*2,39-activity*2,15+activity*4,15+activity*4,1);}label(g,juce::String(i+1),{int(x+8),79,16,12},8,colors[size_t(i%4)]);}
    } else if(f.engine==2) {
        for(int string=0;string<4;++string){juce::Path wire;const float y=35.f+string*13;for(int i=0;i<180;++i){const float u=float(i)/179;const float x=25+u*200,v=y+std::sin(u*float(pi)*(1+string))*std::sin(phase*12+u*14)*activity*(3+f.color*5);if(i==0)wire.startNewSubPath(x,v);else wire.lineTo(x,v);}g.setColour(colors[size_t(string)]);g.strokePath(wire,juce::PathStrokeType(1));dot(g,25,y,2);dot(g,225,y,2);}
    } else if(f.engine==1) {
        const double pivot=.08+f.tone*.84;
        for(int i=0;i<225;++i){const double p=double(i)/224,w=p<pivot?.5*p/pivot:.5+.5*(p-pivot)/(1-pivot);const float x=13.f+i,y=57-float(std::cos(2*pi*w))*(16+activity*9);if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);}
        g.setColour(colors[1]);g.strokePath(curve,juce::PathStrokeType(1.5f));const float marker=std::fmod(phase*(.1f+f.color*.5f),1.f);g.setColour(colors[0]);g.drawVerticalLine(int(13+marker*224),27,86);
    } else if(f.digital246) {
        // Our feedback/XOR visualization, driven by note activity and raw controls.
        for(int column=0;column<24;++column){const auto bits=unsigned(int(32767*std::sin(column*.3f+phase*(.2f+f.color))))^unsigned(f.tone*32767);
            for(int bit=0;bit<8;++bit){g.setColour(colors[size_t(bit%4)].withAlpha(bits&(1u<<bit)?.35f+activity*.6f:.08f));g.fillRect(17+column*9,29+bit*7,7,5);}}
    } else {
        // Nuke-inspired scientific dial language, redrawn with original geometry.
        const float cx=61,cy=56;
        for(int orbit=0;orbit<3;++orbit){g.setColour(colors[size_t(orbit)].withAlpha(.75f));juce::Path ring;ring.addEllipse(-32,-12-orbit*3,64,24+orbit*6);g.strokePath(ring,juce::PathStrokeType(1),juce::AffineTransform::rotation(phase*.3f+orbit*float(pi)/3+f.tone).translated(cx,cy));const float angle=phase*(1+f.color*4)+orbit*2.1f;dot(g,cx+std::cos(angle)*30,cy+std::sin(angle)*21,2+activity*2);}
        g.setColour(colors[3]);dot(g,cx,cy,3+activity*6);
        for(int i=0;i<126;++i){const double p=double(i)/125;const float x=109.f+i,y=57-float(std::sin(2*pi*p+f.tone*7*std::sin(2*pi*p*(1+std::round(f.color*7))+phase)))*(12+activity*8);if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);}g.setColour(colors[0]);g.strokePath(curve,juce::PathStrokeType(1.2f));
    }
    if(f.page!=4)for(int step=0;step<16;++step){g.setColour(step==f.step?colors[3]:juce::Colour(0xff27394a));dot(g,14.f+step*14.7f,95,step==f.step?2.f:1.f);}
    for(int i=0;i<4;++i){label(g,f.labels[size_t(i)].substring(0,5),{10+i*59,102,58,9},7,colors[size_t(i)]);label(g,juce::String(f.values[size_t(i)],(f.page==2&&i>=2)||(f.page==0&&(f.phase246||f.digital246))||(f.page==1&&f.phaseEnvelope)||(f.page==3&&f.punch246)?0:2),{10+i*59,111,58,11},9,colors[size_t(i)]);}
}
}
