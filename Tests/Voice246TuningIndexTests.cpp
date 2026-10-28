// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246TuningIndex.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing tuning-index fixture");return {std::istreambuf_iterator<char>(f),{}};}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int32_t s32(uint32_t x){return x<0x80000000u?int32_t(x):int32_t(int64_t(x)-0x100000000LL);}
void put32(uint8_t* p,uint32_t x){for(size_t i=0;i<4;++i)p[i]=uint8_t(x>>(i*8));}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");const std::string dir=argv[1];
    const auto input=read(dir+"/voice246-tuning-index-input.bin"),output=read(dir+"/voice246-tuning-index-output.bin");
    constexpr size_t cartesian=23*199,cases=65536+cartesian+256;
    check(input.size()==cases*32&&output.size()==cases*16,"Incomplete tuning-index fixtures");
    std::array<uint8_t,16> image{};std::array<bool,cartesian> pairs{};std::array<bool,2400> indices{};
    bool lowWrap=false,highWrap=false;
    for(size_t test=0;test<cases;++test){
        const auto* p=input.data()+test*32;const auto* e=output.data()+test*16;
        check(u32(p)==uint32_t(test<=65536+cartesian),"Unexpected tuning reload schedule");
        if(u32(p))std::copy_n(p+12,16,image.begin());
        const auto coarse=s32(u32(p+4)),fine=s32(u32(p+8));
        const auto index=voice246TuningIndex(coarse,fine);put32(image.data()+8,index);
        if(!std::equal(image.begin(),image.end(),e)){std::cerr<<"Index case "<<test<<'\n';throw std::runtime_error("Original index/neighbor image differs");}
        const int64_t wide=(int64_t(coarse)+12)*100+fine;lowWrap|=wide<0;highWrap|=wide>0xffffffffLL;
        if(test>=65536&&test<65536+cartesian){
            check(coarse>=-11&&coarse<=11&&fine>=-99&&fine<=99,"Unexpected control-range fixture");
            const auto pair=size_t(coarse+11)*199+size_t(fine+99);check(!pairs[pair],"Duplicate control pair");pairs[pair]=true;
            check(index>=1&&index<2400,"Control-range index outside table");indices[index]=true;
        }
    }
    for(bool used:pairs)check(used,"Missing coarse/fine pair");
    for(size_t i=1;i<2400;++i)check(indices[i],"Missing bounded table index");
    check(lowWrap&&highWrap&&voice246TuningIndex(0,0)==1200,"Missing wrap/default-index boundary");
    std::cout<<"PASS: 65536 signed32 stress cases, 4577 control pairs covering indices 1..2399, 256 continuous setter calls and preserved neighbors\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
