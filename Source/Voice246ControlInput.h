// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace pocket::reference {
// Bounded successful path of VoiceCtl 0x01a84f8c. Status/channel dispatch is
// external. Bytes are unsigned; value is not masked to seven bits. Own guards
// reject invalid input before mutation rather than emulating original asserts.
inline bool voice246DecodeControl(std::array<uint32_t,128>& values,
        const uint8_t* message,std::size_t bytes) noexcept {
    if(!message||bytes<3||message[1]>=values.size())return false;
    values[message[1]]=message[2];
    return true;
}
}
