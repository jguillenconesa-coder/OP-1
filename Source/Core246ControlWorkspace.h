// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace pocket::reference {
// Bounded image 0xff8016d8..0xff8017bc, including neighbors/guards.
// Original 0x01968774 initializes two workspaces and ONLY the low word at 16dc.
// This is initialization, not the per-call producer of synthesis/ENV parameters.
using Core246ControlWorkspace=std::array<uint8_t,228>;
inline void core246InitializeControlWorkspaces(Core246ControlWorkspace& image) noexcept {
    image[4]=image[5]=0;
    std::fill(image.begin()+76,image.begin()+92,uint8_t(0));
    std::fill(image.begin()+156,image.begin()+220,uint8_t(0));
}
}
