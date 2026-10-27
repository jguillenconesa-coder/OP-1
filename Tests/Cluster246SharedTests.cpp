// SPDX-License-Identifier: AGPL-3.0-only
#include "Cluster246.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing shared-initializer fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int16_t s16(const uint8_t* p){return cluster246Wrap16(int(p[0])+(int(p[1])<<8));}
void put16(uint8_t* p,int16_t value){const auto word=uint16_t(value);p[0]=uint8_t(word);p[1]=uint8_t(word>>8);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto in=read(dir+"/fractional246-input.bin"),out=read(dir+"/fractional246-output.bin");
    check(in.size()==65536*4&&out.size()==65536*8,"Incomplete fractional-conversion fixtures");
    size_t failures=0;
    for(size_t i=0;i<65536;++i){
        const auto bits=u32(in.data()+i*4);
        if(int32_t(fractional246Q15FromBits(bits))!=signedPhase(u32(out.data()+i*8))||fractional246Q31FromBits(bits)!=signedPhase(u32(out.data()+i*8+4))){
            if(failures++<8)std::cerr<<"Conversion differs for input bits 0x"<<std::hex<<bits<<std::dec<<'\n';
        }
    }
    check(failures==0,"Original Q15/Q31 conversion differs");
    const auto shared=read(dir+"/cluster246-shared-input.bin"),expected=read(dir+"/cluster246-shared-output.bin");
    check(shared.size()==256*228&&expected.size()==256*196,"Incomplete shared-initializer fixtures");
    for(size_t i=0;i<256;++i){
        const auto* p=shared.data()+i*228;const auto* e=expected.data()+i*196;
        std::array<uint32_t,8> source{};for(size_t j=0;j<8;++j)source[j]=u32(p+j*4);
        std::array<uint8_t,64> image{};std::memcpy(image.data(),p+32,64);
        Cluster246Tables tables;Cluster246Context context;auto controlWord=s16(image.data()+8);
        for(size_t j=0;j<8;++j){tables.normalization[j]=s16(image.data()+12+j*2);context.weights[j]=s16(image.data()+28+j*2);context.display[j]=s16(image.data()+44+j*2);}
        context.random=u32(p+96);for(size_t j=0;j<32;++j)tables.damping[j]=signedPhase(u32(p+100+j*4));
        cluster246InitializeShared(tables,context,controlWord,source);
        put16(image.data()+8,controlWord);
        for(size_t j=0;j<8;++j){put16(image.data()+12+j*2,tables.normalization[j]);put16(image.data()+28+j*2,context.weights[j]);put16(image.data()+44+j*2,context.display[j]);}
        check(std::memcmp(image.data(),e,64)==0,"Shared control/table/context/guard bytes differ");
        check(context.random==u32(e+64),"Initializer changed RNG unexpectedly");
        for(size_t j=0;j<32;++j)check(tables.damping[j]==signedPhase(u32(e+68+j*4)),"Initializer changed damping unexpectedly");
        if(i<64)for(int count:{1,4,5,6,7})check(tables.normalization[size_t(count-1)]==*cluster246ObservedNormalization(count),"Observed normalization changed");
    }
    std::cout<<"PASS: 131072 Q15/Q31 conversions and 256 complete shared initializer states, guard bytes, RNG/damping preservation; original counts 2/3/8 still unknown\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
