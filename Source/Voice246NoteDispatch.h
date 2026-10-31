// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DrWave246NoteModes.h"
#include "DrWave246HeldNoteModes.h"
namespace pocket::reference {
enum class Voice246NoteDispatchResult { rejected, gated, duplicate, appended };
// Complete 0x01a85750 instrument route: discriminator zero, metadata/pitch zero,
// selectors 1-3 and initialized Dr. Wave/ADSR/null auxiliary context.
// No notification, assert or drum route; guards/status are adapter policy.
inline Voice246NoteDispatchResult voice246NoteDispatch(
        Voice246NoteArgument& argument,uint32_t discriminator,uint8_t& noteFlag,
        Voice246NoteArgument& localArgument,DrWave246Pool& waves,
        Voice246Registers& registers,std::array<Adsr246State,6>& envelopes,
        Voice246ReleaseList& list,uint8_t gate,int loaded,int selected,
        int16_t selectorWord,int32_t tuning,uint32_t& random) noexcept {
    const auto word=[&](size_t p){return uint32_t(argument[p])|(uint32_t(argument[p+1])<<8)|
        (uint32_t(argument[p+2])<<16)|(uint32_t(argument[p+3])<<24);};
    const auto mode=floorDivide(selectorWord,4096);
    if(word(0)>=128||discriminator!=0||argument[8]!=0||word(12)!=0||
       mode<1||mode>3||list.count>128||loaded<0||loaded>=8||selected<0||selected>=8)return Voice246NoteDispatchResult::rejected;
    // Capacity errors can occur after original mutation; excluded from adapter.
    if(!gate&&list.count==128){
        bool duplicate=false;
        for(size_t i=0;i<list.count;++i){const size_t p=i*20;
            const auto n=uint32_t(list.bytes[p])|(uint32_t(list.bytes[p+1])<<8)|
                (uint32_t(list.bytes[p+2])<<16)|(uint32_t(list.bytes[p+3])<<24);
            duplicate|=n==word(0);}
        if(!duplicate)return Voice246NoteDispatchResult::rejected;
    }
    noteFlag=1;
    if(word(4)==0xffffffffu){argument[4]=127;argument[5]=argument[6]=argument[7]=0;}
    for(size_t i=0;i<16;++i)if(i<9||i>=12)localArgument[i]=argument[i];
    if(list.count==0){
        const Voice246Note note{word(0),word(4),word(8),0};
        const auto result=drWave246NoteModes(waves,registers,envelopes,list,note,gate,loaded,selected,int(mode),tuning,random);
        return result.status==DrWave246NoteModesStatus::appended?Voice246NoteDispatchResult::appended:
               result.status==DrWave246NoteModesStatus::gated?Voice246NoteDispatchResult::gated:Voice246NoteDispatchResult::rejected;
    }
    const auto result=drWave246HeldNoteModes(registers,envelopes,list,localArgument,gate,selectorWord,tuning,random);
    using S=DrWave246HeldNoteModesStatus;
    return result.status==S::appended?Voice246NoteDispatchResult::appended:
           result.status==S::duplicate?Voice246NoteDispatchResult::duplicate:
           result.status==S::gated?Voice246NoteDispatchResult::gated:Voice246NoteDispatchResult::rejected;
}
}
