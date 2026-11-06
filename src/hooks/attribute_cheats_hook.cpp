// Always crit and elemental power everywhere, as cheats (SkylandersCoreOfLight #48, TrapTeamRecomp #20;
// Swap Force, SuperChargers and Imaginators have them under the same setting names).
//
// Trap Team rolls both in its scripts, in the macro "On Hurt - Player On Hurt" that every hit goes
// through (read 2026-11-06 from a heap snapshot of Level_302, debug notes in the kit). Its check ops
// run in sub_82285C88, and NightmareCheckHook (nightmare_hook.cpp, 0x82285CE0) sees each one's result
// in r3 before the op acts on it (below 1 skips the op's next +0x1A ops). The compare, sub_822878C0,
// knows six operators: 0 <=, 1 ==, 2 >=, 3 <, 4 >, 5 !=.
//
// Crit. The macro's parameter "critical hit" comes in as -1. Only when a player's creature dealt the
// hit (a reference check before it), the macro checks `critical hit < 0` and sets it to the hurter's
// stats.total crit (base + stat boosts + added + hat, 12 for Enigma). Then for every hit:
//
//   check critical hit <= 0                   -> no crit (the enemies' hits all end here, with -1)
//   critical hit = critical hit - 100 (with the roll)
//   check critical hit > 0                    -> damage = damage + damage * 0.5, critical hit = 1
//
// A test run (a diagnostic build, Enigma in Level_302's opening, 2026-11-06) logged these three checks: for his
// hits on the enemies -1, then 12, then a random float from about -88 to +12 (positive 5 times in 34);
// for the enemies' hits on him only the first one, with -1. With the cheat on, the `< 0` check marks the
// hit as a player's, and for that hit the `<= 0` check comes out false and the `> 0` one true: every
// player hit is critical, the enemies' hits are untouched.
//
// Element. The macro adds "element add damage" = damage * stats.total elemental strength * 0.01 when the
// hurter is player 1's creature and the global ZoneElement_P1_Correct is 1 (player 2's: P2), i.e. it
// stands in an elemental zone of its own element. Those two `== 1` checks are the only checks of the two
// variables in the scripts loaded there (debug tool checks_of.py over the snapshot); with the cheat on
// they come out true, as if every area were the Skylander's zone.
//
// Each op is classified once by its variables' names (the caller keeps the answer by op address).
// Nothing is written to the game's memory.

#include "attribute_cheats_hook.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>

#include "script_objects.h"

REXCVAR_DEFINE_BOOL(cheat_always_crit, false, "Extras",
                    "Every hit of your Skylanders is a critical hit");
REXCVAR_DEFINE_BOOL(cheat_elemental_power, false, "Extras",
                    "Your Skylanders get their element's bonus in every area, as if each area were "
                    "theirs");

namespace trapteamrecomp::attribute_cheats {
namespace {

using script::NameIs;

// The check's right side is the literal `value` (a ValueRHSVariant whose value stack has no items).
bool RightIsLiteral(uint8_t* base, uint32_t op, uint32_t value) {
  using rex::memory::load_and_swap;
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);
  const uint32_t literal = rhs ? load_and_swap<uint32_t>(base + rhs + 8) : 0;
  return literal && load_and_swap<uint32_t>(base + literal + 8) == 0 &&
         load_and_swap<uint32_t>(base + literal + 0x24) == value;
}

// The hit being checked is a player's (the crit checks of one hit run in order on the scripts' thread).
thread_local bool g_player_hit = false;

}  // namespace

bool AnyOn() { return REXCVAR_GET(cheat_always_crit) || REXCVAR_GET(cheat_elemental_power); }

Kind Classify(uint8_t* base, uint32_t op) {
  using rex::memory::load_and_swap;
  // With either cheat on, every check op of every script comes here once, so the cheap tests come
  // first and read only what the game has just used (the op, its right side's literal, its left path
  // and the variable it resolved): most checks end there. Only the few left get the name compare,
  // which asks the memory's protection (system calls; a dozen per op made a 70 ms hitch when a new
  // area's scripts first ran, 2026-11-06).
  const uint32_t oper = load_and_swap<uint32_t>(base + op + 0x20);
  uint32_t literal;
  if (oper == 1) {
    literal = 1;
  } else if (oper == 0 || oper == 3 || oper == 4) {
    literal = 0;
  } else {
    return Kind::kNone;
  }
  if (!RightIsLiteral(base, op, literal)) return Kind::kNone;
  const uint32_t path = load_and_swap<uint32_t>(base + op + 0x1C);
  const uint32_t count = path ? load_and_swap<uint32_t>(base + path + 8) : 0;
  const uint32_t items = path ? load_and_swap<uint32_t>(base + path + 0x14) : 0;
  if (!count || count > 16 || !items) return Kind::kNone;
  const uint32_t last = load_and_swap<uint32_t>(base + items + 4 * (count - 1));
  if (!last) return Kind::kNone;
  const uint32_t type = load_and_swap<uint32_t>(base + last);
  if (oper == 1) {
    // A global variable instance (its name at +8).
    if (type != 0x82109360) return Kind::kNone;
    if (NameIs(base, last + 8, "ZoneElement_P1_Correct") ||
        NameIs(base, last + 8, "ZoneElement_P2_Correct")) {
      return Kind::kZoneCorrect;
    }
    return Kind::kNone;
  }
  // The macro parameter alone (a path of one item; "hurt scratch.critical hit" elsewhere has two).
  if (count != 1 || type != 0x8210B0E0 || !NameIs(base, last + 0x1C, "critical hit")) {
    return Kind::kNone;
  }
  return oper == 3 ? Kind::kCritUnset : oper == 0 ? Kind::kCritNone : Kind::kCritRoll;
}

void Apply(Kind kind, PPCRegister& r3) {
  switch (kind) {
    case Kind::kCritUnset:
      // Reached only for a player's hit; the result itself stays.
      g_player_hit = true;
      return;
    case Kind::kCritNone:
      if (g_player_hit && REXCVAR_GET(cheat_always_crit)) r3.u64 = 0;
      return;
    case Kind::kCritRoll:
      if (g_player_hit && REXCVAR_GET(cheat_always_crit)) {
        static bool logged = false;
        if (!logged) {
          logged = true;
          REXLOG_INFO("Extras: always crit made a hit critical (roll {})", r3.s32 >= 1 ? "won" : "lost");
        }
        r3.u64 = 1;
      }
      // The later `> 0` checks of the same hit read the crit's outcome: no forcing there.
      g_player_hit = false;
      return;
    case Kind::kZoneCorrect:
      if (REXCVAR_GET(cheat_elemental_power)) {
        static bool logged = false;
        if (!logged) {
          logged = true;
          REXLOG_INFO("Extras: elemental power everywhere ({} zone)", r3.s32 >= 1 ? "own" : "other");
        }
        r3.u64 = 1;
      }
      return;
    case Kind::kNone:
      return;
  }
}

}  // namespace trapteamrecomp::attribute_cheats
