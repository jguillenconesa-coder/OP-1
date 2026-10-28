// SPDX-License-Identifier: AGPL-3.0-only
#include "Voice246TuningTable.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pocket::reference;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
int main(int argc,char** argv){try{
    check(argc==2,"Expected fixture directory");std::ifstream f(std::string(argv[1])+"/voice246-tuning-table-output.bin",std::ios::binary);
    check(bool(f),"Missing table-producer fixture");std::vector<uint8_t> oracle{std::istreambuf_iterator<char>(f),{}};
    check(oracle.size()==3*9624,"Incomplete table-producer fixture");
    Voice246TuningTable table{};const uint32_t patterns[3]={0,0xa5a5a5a5,0xffffffff};
    for(size_t row=0;row<3;++row){
        table.fill(row==0?0:-1);voice246BuildTuningTable(table);const auto* e=oracle.data()+row*9624;
        for(size_t i=0;i<2400;++i)if(uint32_t(table[i])!=u32(e+4+i*4)){
            std::cerr<<"Tuning entry "<<i<<", got "<<uint32_t(table[i])<<", expected "<<u32(e+4+i*4)<<'\n';
            throw std::runtime_error("Original table-generator coefficient differs");
        }
        check(u32(e)==patterns[row]&&u32(e+9604)==patterns[row],"Original generator changed table guards");
        for(size_t i=0;i<4;++i)check(u32(e+9608+i*4)==patterns[row],"Original generator changed index/neighbors");
    }
    check(table[0]==0x20000000&&table[1200]==0x40000000&&table[2399]==0x7fed1400,"Directed table coefficients differ");
    for(size_t i=1;i<table.size();++i)check(table[i]>table[i-1],"Generated tuning table is not strictly increasing");
    std::cout<<"PASS: 2400 original tuning coefficients across three preloads, guarded table and preserved index; no module/event setup or audio callback integration\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
