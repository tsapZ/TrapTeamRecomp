// Nightmare from the start, as an option (SwapForceRecomp #74; Swap Force and Imaginators have it
// as nightmare_from_start too).
//
// Trap Team unlocks its hardest difficulty in a script, not in code: the difficulty menu's macro
// "Menu - 3d - Diff - Create" (in the levels' level.bld) reads system flag 3 (Story Mode finished)
// through the macro "System - Save - Get/Set System Flag" and, when its "value" is 1, sets
// "allow nightmare":
//
//   check [disallow nightmare setting] == 0
//     use "System - Save - Get/Set System Flag" (setting ID 3, value -1, do save 1)
//     check [value] == 1            <- this check
//       set [allow nightmare] = 1
//
// An OpCheckValue runs in sub_82285C88: the compare (sub_822878C0) leaves its result in r3 at
// 0x82285CE0 (config/default.toml), and a result below 1 skips the op's next +0x1A ops. With
// nightmare_from_start on, the hook makes exactly that check true: operator 1 (equal), one op
// skipped, the right side the literal 1, the left side the macro parameter "value" of "System -
// Save - Get/Set System Flag", and the op after it setting "allow nightmare" (in the script the
// check (0x28 bytes) is followed by its left side's stack (0x20) and then that op, so +0x48). The
// object layout is the script file's (IGZ version 8; checked 2026-10-26 in the front end's
// level.bld): a check's left side +0x1C, operator +0x20, right side +0x24 (a ValueRHSVariant
// whose +8 is the value stack); a stack's count +8, its items +0x14, a literal +0x24; a macro
// parameter's or variable's name +0x1C, a macro parameter's macro +0x38, a macro's name +8. The
// op's own fields are read directly (the game has just used them); every pointer past them is
// checked to be readable first. A name's pointer has its lowest bit set in memory (the strings
// start at even addresses; seen 2026-10-26: "state" at ...24 read as ...25). Nothing is written:
// the save keeps its own flag, so with the setting off again Nightmare is locked until the game
// unlocks it.

#include <cstdint>
#include <cstring>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "script_objects.h"

REXCVAR_DEFINE_BOOL(nightmare_from_start, false, "Game",
                    "Nightmare difficulty can be picked from the start, without finishing Story Mode "
                    "first (the save is not changed)");
// TrapTeamRecomp #20: the Trap Masters' Traptanium boost (the Guardian Crystal powerup: a bigger,
// glowing weapon and "Trapper - Boosted Damage Mult" on hits) is switched on by level scripts through
// "Minion - Guardian Crystal Power - Switch" (master switch), and "Minion - Guardian Crystal Power -
// Manage" checks that switch against 0. With this on, those checks come out as if it were on.
// Measured 2026-11-03 in Level_302 with Enigma: his hits on the enemies 30 / 66 / 138 without it,
// 45 / 99 / 207 with it (1.5 times), and his orb glows white and bigger.
REXCVAR_DEFINE_BOOL(cheat_traptanium_boost, false, "Extras",
                    "Trap Masters' Traptanium boost always on: their weapon glows and grows, and their "
                    "hits do 1.5 times the damage (achievements don't unlock while it's on)");

namespace {

using trapteamrecomp::script::LastItem;
using trapteamrecomp::script::NameIs;
using trapteamrecomp::script::ReadU32;

bool IsNightmareCheck(uint8_t* base, uint32_t op) {
  // The op and its right side the game has just read (no check needed, and this runs for every
  // check that comes out false): one op skipped, equal, the literal 1.
  using rex::memory::load_and_swap;
  if (load_and_swap<uint16_t>(base + op + 0x1A) != 1 || load_and_swap<uint32_t>(base + op + 0x20) != 1) {
    return false;
  }
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);  // a ValueRHSVariant
  const uint32_t literal = rhs ? load_and_swap<uint32_t>(base + rhs + 8) : 0;  // its value stack
  if (!literal || load_and_swap<uint32_t>(base + literal + 8) != 0 ||
      load_and_swap<uint32_t>(base + literal + 0x24) != 1) {
    return false;
  }
  const uint32_t value = LastItem(base, op + 0x1C);
  uint32_t macro = 0;
  if (!value || !NameIs(base, value + 0x1C, "value") || !ReadU32(base, value + 0x38, macro) ||
      !NameIs(base, macro + 8, "System - Save - Get/Set System Flag")) {
    return false;
  }
  const uint32_t allow = LastItem(base, op + 0x48 + 0x1C);
  return allow && NameIs(base, allow + 0x1C, "allow nightmare");
}

// A check of the Guardian Crystal powerup's master switch against the literal 0 ("guardian crystal
// powerup data ref.master switch"): its operator (1 ==, 4 !=), or 0 for any other check.
uint32_t MasterSwitchCheckOperator(uint8_t* base, uint32_t op) {
  using rex::memory::load_and_swap;
  const uint32_t oper = load_and_swap<uint32_t>(base + op + 0x20);
  if (oper != 1 && oper != 4) return 0;
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);
  const uint32_t literal = rhs ? load_and_swap<uint32_t>(base + rhs + 8) : 0;
  if (!literal || load_and_swap<uint32_t>(base + literal + 8) != 0 ||
      load_and_swap<uint32_t>(base + literal + 0x24) != 0) {
    return 0;
  }
  const uint32_t last = LastItem(base, op + 0x1C);
  const uint32_t first = trapteamrecomp::script::StackItem(base, op + 0x1C, 0);
  if (!last || !first || !NameIs(base, last + 0x1C, "master switch") ||
      !NameIs(base, first + 0x1C, "guardian crystal powerup data ref")) {
    return 0;
  }
  return oper;
}

}  // namespace

void NightmareCheckHook(PPCRegister& r3, PPCRegister& r31) {
  const bool nightmare_on = REXCVAR_GET(nightmare_from_start);
  const bool boost_on = REXCVAR_GET(cheat_traptanium_boost);
  // Nightmare only needs the checks that came out false; the boost's == 0 check comes out true.
  if (!(nightmare_on && r3.s32 < 1) && !boost_on) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  // Every check that comes out false passes here, many a frame, and the walk to the names asks
  // the memory's protection a dozen times (system calls): with the option on, the game slowed down
  // and its sound stuttered (TrapTeamRecomp #16). So each op is looked at once: its answer is
  // kept by its address, with its left and right side's pointers to tell a new script's op at the
  // same address (another level) from it.
  struct Seen {
    uint64_t sides;
    bool nightmare;
    uint32_t master_switch;  // MasterSwitchCheckOperator
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint32_t op = r31.u32;
  const uint64_t sides = (uint64_t(rex::memory::load_and_swap<uint32_t>(base + op + 0x1C)) << 32) |
                         rex::memory::load_and_swap<uint32_t>(base + op + 0x24);
  bool nightmare;
  uint32_t master_switch;
  if (auto it = seen.find(op); it != seen.end() && it->second.sides == sides) {
    nightmare = it->second.nightmare;
    master_switch = it->second.master_switch;
  } else {
    if (seen.size() > 65536) seen.clear();
    nightmare = IsNightmareCheck(base, op);
    master_switch = MasterSwitchCheckOperator(base, op);
    seen[op] = {sides, nightmare, master_switch};
  }
  if (boost_on && master_switch) {
    static bool logged_boost = false;
    if (!logged_boost) {
      logged_boost = true;
      REXLOG_INFO("Extras: Traptanium boost experiment forces a master switch check");
    }
    r3.u64 = master_switch == 4 ? 1 : 0;  // != 0 true, == 0 false
    return;
  }
  if (!nightmare || !nightmare_on || r3.s32 >= 1) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Nightmare difficulty open from the start (nightmare_from_start)");
  }
  r3.u64 = 1;
}
