// A shorter Skylander intro on the portal (figure_intro_hook.cpp), applied from the scripts'
// check-value hook (nightmare_hook.cpp).

#pragma once

#include <cstdint>

#include <rex/ppc/context.h>

namespace trapteamrecomp::figure_intro {

// Whether the option is on (figure_intro_seconds above 0).
bool On();
// Whether the check op `op` is one of the magic moment's waits (the caller caches it by op).
bool IsIntroWait(uint8_t* base, uint32_t op);
// Called for each magic moment wait as it is checked: once the magic moment has run
// figure_intro_seconds, a wait that came out false (r3 below 1) comes out true.
void Apply(PPCRegister& r3);

}  // namespace trapteamrecomp::figure_intro
