// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Voice246NoteDispatch.h"
#include "Voice246ReleaseSelectors.h"
namespace pocket::reference {
// Bounded original notification behavior with 0x01dfe03c explicitly ZERO.
// Guards are adapter policy. No packet preparation or transport emulation.
inline bool voice246QuietNotifyOn(uint32_t note,uint32_t velocity,
        std::array<uint8_t,128>& map,uint32_t outputWord) noexcept {
    if(note>=128||outputWord!=0)return false;
    if(velocity==0)map[note]=0;
    else if(map[note]==0)map[note]=1;
    return true;
}
inline bool voice246QuietNotifyOff(uint32_t note,
        std::array<uint8_t,128>& map,uint32_t outputWord) noexcept {
    if(note>=128||outputWord!=0)return false;
    map[note]=0;return true;
}
inline uint32_t voice246QuietWord(const Voice246NoteArgument& a,size_t p) noexcept {
    return uint32_t(a[p])|(uint32_t(a[p+1])<<8)|(uint32_t(a[p+2])<<16)|(uint32_t(a[p+3])<<24);
}
// Full outer instrument on entry 0x01a85750, selectors 1-3, pitch zero,
// metadata preserved, ready ADSR/null auxiliaries and output word zero.
inline Voice246NoteDispatchResult voice246QuietNoteOn(
        Voice246NoteArgument& argument,Voice246NoteArgument& local,uint8_t& flag,
        std::array<uint8_t,128>& map,uint32_t outputWord,DrWave246Pool& waves,
        Voice246Registers& registers,std::array<Adsr246State,6>& envelopes,
        Voice246ReleaseList& list,uint8_t gate,int loaded,int selected,
        int16_t selectorWord,int32_t tuning,uint32_t& random) noexcept {
    using S=Voice246NoteDispatchResult;
    const auto note=voice246QuietWord(argument,0);const auto mode=floorDivide(selectorWord,4096);
    if(note>=128||outputWord!=0||voice246QuietWord(argument,12)!=0||mode<1||mode>3||
       list.count>128||loaded<0||loaded>=8||selected<0||selected>=8)return S::rejected;
    if(!gate&&list.count==128){
        bool duplicate=false;
        for(size_t i=0;i<list.count;++i){const size_t p=i*20;
            const auto n=uint32_t(list.bytes[p])|(uint32_t(list.bytes[p+1])<<8)|
                (uint32_t(list.bytes[p+2])<<16)|(uint32_t(list.bytes[p+3])<<24);
            duplicate|=n==note;}
        if(!duplicate)return S::rejected;
    }
    flag=1;
    if(voice246QuietWord(argument,4)==0xffffffffu){argument[4]=127;argument[5]=argument[6]=argument[7]=0;}
    for(size_t i=0;i<16;++i)if(i<9||i>=12)local[i]=argument[i];
    S status=S::rejected;
    if(list.count==0){
        const Voice246Note n{note,voice246QuietWord(argument,4),voice246QuietWord(argument,8),0};
        const auto r=drWave246NoteModes(waves,registers,envelopes,list,n,gate,loaded,selected,int(mode),tuning,random);
        if(r.status==DrWave246NoteModesStatus::appended)status=S::appended;
        else if(r.status==DrWave246NoteModesStatus::gated)status=S::gated;
    }else{
        const auto r=drWave246HeldNoteModes(registers,envelopes,list,local,gate,selectorWord,tuning,random);
        using H=DrWave246HeldNoteModesStatus;
        status=r.status==H::appended?S::appended:r.status==H::gated?S::gated:r.status==H::duplicate?S::duplicate:S::rejected;
    }
    if(status!=S::rejected&&argument[8])voice246QuietNotifyOn(note,voice246QuietWord(argument,4),map,outputWord);
    return status;
}
// Full instrument off entry 0x01a85840 in the same quiet context. Notification
// runs after the interior release even for an absent note; metadata unchanged.
// Original release requires its outer/list gate byte zero (explicit context).
inline Voice246ReleaseResult voice246QuietNoteOff(const Voice246NoteArgument& argument,
        Voice246NoteArgument& local,std::array<uint8_t,128>& map,uint32_t outputWord,
        Voice246ReleaseList& list,int16_t selectorWord,std::array<Adsr246State,6>& envelopes,
        int16_t sustain,int32_t tuning,std::array<int32_t,6>& pitch,uint32_t& random) noexcept {
    const auto note=voice246QuietWord(argument,0);const auto mode=floorDivide(selectorWord,4096);
    if(note>=128||outputWord!=0||mode<1||mode>3||list.count>128||sustain<0)return Voice246ReleaseResult::rejected;
    const auto r=voice246ReleaseSelectors(list,note,selectorWord,envelopes,sustain,tuning,pitch,random);
    if(r==Voice246ReleaseResult::rejected)return r;
    for(size_t i=0;i<16;++i)if(i<9||i>=12)local[i]=argument[i];
    if(argument[8])voice246QuietNotifyOff(note,map,outputWord);
    return r;
}
}
