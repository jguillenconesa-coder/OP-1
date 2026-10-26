// SPDX-License-Identifier: AGPL-3.0-only
#include "ReferenceDsp.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
std::vector<uint8_t> read(const std::string& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Missing oracle fixture: "+path);return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());}
uint16_t u16(const std::vector<uint8_t>& b,size_t p){return uint16_t(unsigned(b.at(p))|(unsigned(b.at(p+1))<<8));}
int16_t s16(const std::vector<uint8_t>& b,size_t p){const int value=u16(b,p);return int16_t(value<32768?value:value-65536);}
uint32_t u32(const std::vector<uint8_t>& b,size_t p){return uint32_t(u16(b,p))|(uint32_t(u16(b,p+2))<<16);}
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    check(argc==2,"Expected oracle fixture directory");const std::string root=std::string(argv[1])+"/";
    const auto fm=read(root+"fm-fields.bin");check(fm.size()==65536*16,"Wrong FM oracle size");
    for(unsigned word=0;word<65536;++word){const auto fields=pocket::reference::decodeFmFields(uint16_t(word),uint16_t(word));const uint32_t selector=u32(fm,word*16);const int signedSelector=selector<=0x7fffffff?int(selector):-int(0xffffffff-selector)-1;
        check(fields.selector==signedSelector,"FM selector differs from original instruction sequence");
        for(size_t i=0;i<3;++i)check(uint32_t(0x02000000+12+int(i)*16+fields.tableIndices[i]*2)==u32(fm,word*16+4+i*4),"FM table index differs from original instruction sequence");}
    const auto input=read(root+"q15-input.bin"),output=read(root+"q15-output.bin");check(input.size()==65536*4&&output.size()==65536*2,"Wrong multiply oracle size");
    for(size_t i=0;i<65536;++i)check(pocket::reference::multiplyQ15(s16(input,i*4),s16(input,i*4+2))==s16(output,i*2),"Q15 multiply differs from original instruction");
    const auto pd=read(root+"pd-input.bin"),samples=read(root+"pd-output.bin"),lut=read(root+"synthetic-sine.bin");check(pd.size()==1024*140&&samples.size()==1024*128*2&&lut.size()==8192*2,"Wrong Pd oracle size");
    std::array<int16_t,8192> table{};for(size_t i=0;i<table.size();++i)table[i]=s16(lut,i*2);
    for(size_t block=0;block<1024;++block){uint32_t phase=u32(pd,block*140+132);const auto increment=u32(pd,block*140+136);const auto gain=s16(pd,block*140+2);
        for(size_t frame=0;frame<128;++frame)check(pocket::reference::pdSteadySample(phase,increment,gain,table)==s16(samples,(block*128+frame)*2),"Pd steady sample differs from original kernel");}
    const auto transient=read(root+"pd-transient-input.bin"),rendered=read(root+"pd-transient-output.bin"),verified=read(root+"verified-sine.bin");
    check(transient.size()==8192*152&&rendered.size()==8192*272&&verified.size()==8193*2,"Wrong Pd transient oracle size");
    for(size_t i=0;i<table.size();++i)table[i]=s16(verified,i*2);
    size_t resets=0,activeFrames=0;
    for(size_t block=0;block<8192;++block){const auto in=block*152,out=block*272;
        const pocket::reference::PdCoefficients controls({s16(transient,in),s16(transient,in+2),s16(transient,in+4),s16(transient,in+6)});
        pocket::reference::PdState state{u32(transient,in+132),u32(transient,in+140),u32(transient,in+144),u32(transient,in+148)!=0};
        if(u32(transient,in+148)==2){state.reset();++resets;}
        for(size_t frame=0;frame<128;++frame){if(state.secondaryActive)++activeFrames;check(pocket::reference::pdSample(state,u32(transient,in+136),controls,table)==s16(rendered,out+frame*2),"Pd sample differs in secondary/reset/wrap branch");}
        check(state.phase==u32(rendered,out+256)&&state.secondaryPhase==u32(rendered,out+260)&&state.counter==u32(rendered,out+264)&&state.secondaryActive==(u32(rendered,out+268)!=0),"Pd final state differs from original kernel");
    }
    check(resets>2000&&activeFrames>100000,"Pd transient cases did not exercise reset/secondary states");
    const auto stream=read(root+"pd-stream-input.bin"),continuous=read(root+"pd-stream-output.bin");
    check(stream.size()==2304*648&&continuous.size()==2304*272,"Wrong Pd stream oracle size");
    pocket::reference::PdState carried;size_t sequences=0;
    for(size_t block=0;block<2304;++block){const auto in=block*648,out=block*272;
        if(u32(stream,in+644)!=0){carried.reset();++sequences;}
        const pocket::reference::PdCoefficients controls({s16(stream,in),s16(stream,in+2),s16(stream,in+4),s16(stream,in+6)});
        for(size_t frame=0;frame<128;++frame)check(pocket::reference::pdSample(carried,u32(stream,in+132+frame*4),controls,table)==s16(continuous,out+frame*2),"Pd continuous/glide/vibrato sample differs from original kernel");
        check(carried.phase==u32(continuous,out+256)&&carried.secondaryPhase==u32(continuous,out+260)&&carried.counter==u32(continuous,out+264)&&carried.secondaryActive==(u32(continuous,out+268)!=0),"Pd stream lost state across blocks");
    }
    check(sequences==144,"Pd stream sequence count wrong");
    std::cout<<"PASS: original Blackfin simulator oracles: 65536 FM fields, 65536 Q15 pairs, 131072 steady + 1048576 transient Pd samples and 8192 final states; no mismatches\n";
    std::cout<<"PASS: 294912 additional stream samples / 2304 blocks, 16 raw preset control sets, 3 rates and steady/glide/vibrato increments; no mismatches\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
