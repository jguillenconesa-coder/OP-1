// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Fractional246.h"
#include <array>
#include <cmath>
#include <cstring>
namespace pocket::reference {
using Voice246TuningTable=std::array<int32_t,2400>;
// Reconstruction of original table-generating tail 0x01a85454..0x01a854ca.
// Build during preparation, not in an audio callback. No module/event setup,
// allocator, pitch-buffer provider or plugin integration is implied.
inline void voice246BuildTuningTable(Voice246TuningTable& table) {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
    for(std::size_t i=0;i<table.size();++i){
        const double exponent=double(int(i)-1200)/1200.0;
        const float value=float(std::pow(2.0,exponent)/2.0);
        uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));
        table[i]=fractional246Q31FromBits(bits);
    }
}
}
