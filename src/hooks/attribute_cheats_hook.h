// Always crit and elemental power everywhere (attribute_cheats_hook.cpp), applied from the scripts'
// check-value hook (nightmare_hook.cpp).

#pragma once

#include <cstdint>

#include <rex/ppc/context.h>

namespace trapteamrecomp::attribute_cheats {

enum class Kind : uint8_t {
  kNone,
  kCritUnset,    // critical hit < 0: reached only for a player's hit
  kCritNone,     // critical hit <= 0: no crit
  kCritRoll,     // critical hit > 0 after the roll: a crit
  kZoneCorrect,  // ZoneElement_P1_Correct / ZoneElement_P2_Correct == 1
};

// Whether either cheat is on.
bool AnyOn();
// What the check op `op` is to these cheats (its variables' names; the caller caches it by op).
Kind Classify(uint8_t* base, uint32_t op);
// Changes the check's result r3 (below 1 = false) as the cheats that are on want.
void Apply(Kind kind, PPCRegister& r3);

}  // namespace trapteamrecomp::attribute_cheats
