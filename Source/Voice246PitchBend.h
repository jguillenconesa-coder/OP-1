// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace pocket::reference {
// Bounded decoder for VoiceCtl 0x01a84efa, NOT its MIDI dispatcher or the
// original pitch provider. Image covers 0xff801048..0xff801058 (exclusive).
// The original reads bytes 1 and 2 unsigned, without masking their high bits.
// Status and channel selection belong to the caller. Invalid-length guards
// and the bool return are adapter protections, not original exception emulation.
inline bool voice246DecodePitchBend(std::array<uint8_t,16>& image,
        const uint8_t* message,std::size_t bytes) noexcept {
    if(!message||bytes<3)return false;
    const int raw=int(message[1])+(int(message[2])<<7);
    const auto word=uint16_t((raw-8192)*(raw<8192?2:4));
    image[4]=uint8_t(word);image[5]=uint8_t(word>>8);
    image[6]=0;image[10]=0;image[11]=1;
    return true;
}
}
