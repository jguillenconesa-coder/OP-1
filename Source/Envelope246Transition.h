// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Envelope246.h"

namespace pocket::reference {
// Original 0x019120ba writes five complete words and ONLY the low gain half.
// frame/retriggerStep/finished are adapter bookkeeping, outside the 24-byte image.
inline void adsr246Initialize(Adsr246State& state) noexcept {
    state.stage=state.blocks=state.field12=state.field16=0;
    state.level=0;state.gain=(state.gain&0xffff0000u)|32767u;
    state.frame=0;state.retriggerStep=0;state.finished=false;
}

// Original 0x019120fc takes two distinct source addresses. Here their values
// are explicit caller inputs: no original pitch/velocity mapping is inferred.
// The current level and gain survive the request. Stage 6 fades for one block,
// stage 7 starts the new attack on the following render (existing kernel).
inline void adsr246RequestRestart(Adsr246State& state,uint32_t pendingWord12,
                                  uint16_t pendingWord16) noexcept {
    state.stage=6;state.blocks=0;state.field12=pendingWord12;
    state.field16=uint32_t(pendingWord16)<<16;
    state.frame=0;state.finished=false;
}
}
