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

#include "attribute_cheats_hook.h"
#include "figure_intro_hook.h"
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
// The owner's #18 (2026-11-17, as in Spyro's Adventure): talks and cutscenes skippable with the skip
// button. Trap Team's cutscene update (permanent\global.bld; decoded 2026-11-17 from a heap snapshot
// of the Academy, kit tt_script_ops.py) handles a skip request (a player's press) with
//
//   check [current cutscene.cutscene definition.skippable] == 0       (skip the next 13 ops if false)
//     check [a developer flag] != 0, then [DEBUG - allow all cutscenes to skip] != 0: skippable
//     check [current cutscene.cutscene definition.skippable in replays] > 0 and the adventure's
//       "complete": skippable on a replay
//     else the request is turned down
//
// and a talk's line the same way (`check [dialogue ui.text data.skippable] == 0` jumps past the skip;
// seen in the Level_302 snapshot, kit tt_checks_named.py). With skip_talks on, a check of a cutscene
// definition's or a talk line's "skippable" against the literal 0 comes out as if it were 1 (== 0
// false, > 0 true), so a press skips every scene and line; nothing is skipped without a press, and
// the scene's own data stays as it is. (The developer switch alone doesn't do: its check only runs
// behind the flag.)
REXCVAR_DEFINE_BOOL(skip_talks, false, "Game",
                    "Every cutscene and talk can be skipped, also those the game only lets you skip "
                    "on a replay");
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

// A check of a cutscene definition's or a talk line's "skippable" against the literal 0 (skip_talks):
// its operator (1 ==, 4 >), or 0 for any other check.
uint32_t SkippableCheckOperator(uint8_t* base, uint32_t op) {
  using rex::memory::load_and_swap;
  const uint32_t oper = load_and_swap<uint32_t>(base + op + 0x20);  // the op's own fields
  if (oper != 1 && oper != 4) return 0;
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);
  const uint32_t literal = rhs ? load_and_swap<uint32_t>(base + rhs + 8) : 0;
  if (!literal || load_and_swap<uint32_t>(base + literal + 8) != 0 ||
      load_and_swap<uint32_t>(base + literal + 0x24) != 0) {
    return 0;
  }
  const uint32_t last = LastItem(base, op + 0x1C);
  const uint32_t owner = trapteamrecomp::script::StackItem(base, op + 0x1C, -2);
  if (!last || !owner || !NameIs(base, last + 0x1C, "skippable") ||
      !(NameIs(base, owner + 0x1C, "cutscene definition") || NameIs(base, owner + 0x1C, "text data"))) {
    return 0;
  }
  return oper;
}

// A check of the Guardian Crystal powerup's master switch against the literal 0 ("guardian crystal
// powerup data ref.master switch"): its operator (1 ==, 4 >), or 0 for any other check.
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

#if defined(SKYLANDERS_DEVKIT)
// Dev kit only (2026-11-17, finding the scripts' checks): with --kit_log_checks=true each check op is
// logged the first time it runs, with its left side's path names, its operator and its result.
REXCVAR_DEFINE_BOOL(kit_log_checks, false, "Kit", "Log each script check the first time it runs (dev kit)");

namespace {
void LogCheck(uint8_t* base, uint32_t op, int32_t result) {
  thread_local std::unordered_map<uint32_t, uint64_t> logged;
  const uint64_t sides = (uint64_t(rex::memory::load_and_swap<uint32_t>(base + op + 0x1C)) << 32) |
                         rex::memory::load_and_swap<uint32_t>(base + op + 0x24);
  if (logged.size() > 100000) return;
  auto [it, fresh] = logged.try_emplace(op, sides);
  if (!fresh && it->second == sides) return;
  it->second = sides;
  std::string path;
  for (int i = 0; i < 6; ++i) {
    const uint32_t item = trapteamrecomp::script::StackItem(base, op + 0x1C, i);
    if (!item) break;
    uint32_t name = 0;
    const uint8_t* text = nullptr;
    if (ReadU32(base, item + 0x1C, name)) text = trapteamrecomp::script::Readable(base, name & ~1u, 1);
    path += i ? "." : "";
    path += text ? std::string(reinterpret_cast<const char*>(text),
                               strnlen(reinterpret_cast<const char*>(text), 60))
                 : "?";
  }
  REXLOG_INFO("Kit: check {:08X} [{}] op{} -> {}", op, path,
              rex::memory::load_and_swap<uint32_t>(base + op + 0x20), result);
}
}  // namespace
#endif

void NightmareCheckHook(PPCRegister& r3, PPCRegister& r31) {
#if defined(SKYLANDERS_DEVKIT)
  if (REXCVAR_GET(kit_log_checks)) {
    LogCheck(rex::system::kernel_state()->memory()->virtual_membase(), r31.u32, r3.s32);
  }
#endif
  const bool nightmare_on = REXCVAR_GET(nightmare_from_start);
  const bool boost_on = REXCVAR_GET(cheat_traptanium_boost);
  const bool attribute_on = trapteamrecomp::attribute_cheats::AnyOn();
  // figure_intro_hook.cpp: the magic moment's waits, true or false (it times the magic moment).
  const bool intro_on = trapteamrecomp::figure_intro::On();
  const bool skip_on = REXCVAR_GET(skip_talks);
  // Nightmare only needs the checks that came out false; the boost's == 0 check comes out true, and
  // so does skip talks' "skippable == 0".
  if (!(nightmare_on && r3.s32 < 1) && !boost_on && !attribute_on && !intro_on && !skip_on) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  // Every check that comes out false passes here, many a frame, and the walk to the names asks
  // the memory's protection a dozen times (system calls): with the option on, the game slowed down
  // and its sound stuttered (TrapTeamRecomp #16). So each op is looked at once: its answer is
  // kept by its address, with its left and right side's pointers to tell a new script's op at the
  // same address (another level) from it. Each answer is worked out only once an option that needs
  // it is on (`known` bits), so an option that is off costs nothing.
  struct Seen {
    uint64_t sides;
    uint8_t known;  // 1 nightmare, 2 master_switch, 4 attribute, 8 intro, 16 skippable
    bool nightmare;
    bool intro;  // figure_intro::IsIntroWait
    uint32_t skippable;  // SkippableCheckOperator
    uint32_t master_switch;  // MasterSwitchCheckOperator
    trapteamrecomp::attribute_cheats::Kind attribute;
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint32_t op = r31.u32;
  const uint64_t sides = (uint64_t(rex::memory::load_and_swap<uint32_t>(base + op + 0x1C)) << 32) |
                         rex::memory::load_and_swap<uint32_t>(base + op + 0x24);
  auto it = seen.find(op);
  if (it == seen.end() || it->second.sides != sides) {
    if (seen.size() > 65536) seen.clear();
    it = seen.insert_or_assign(op, Seen{sides, 0, false, false, 0, 0, trapteamrecomp::attribute_cheats::Kind::kNone})
             .first;
  }
  Seen& entry = it->second;
  if (intro_on && !(entry.known & 8)) {
    entry.intro = trapteamrecomp::figure_intro::IsIntroWait(base, op);
    entry.known |= 8;
  }
  if (intro_on && entry.intro) {
    trapteamrecomp::figure_intro::Apply(r3);
    return;
  }
  if (skip_on) {
    if (!(entry.known & 16)) {
      entry.skippable = SkippableCheckOperator(base, op);
      entry.known |= 16;
    }
    if (entry.skippable) {
      static bool logged_skip = false;
      if (!logged_skip) {
        logged_skip = true;
        REXLOG_INFO("Skip talks: a cutscene's or talk's skippable check read as skippable");
      }
      r3.u64 = entry.skippable == 4 ? 1 : 0;  // > 0 true, == 0 false
      return;
    }
  }
  if (nightmare_on && r3.s32 < 1 && !(entry.known & 1)) {
    entry.nightmare = IsNightmareCheck(base, op);
    entry.known |= 1;
  }
  if (boost_on && !(entry.known & 2)) {
    entry.master_switch = MasterSwitchCheckOperator(base, op);
    entry.known |= 2;
  }
  if (attribute_on && !(entry.known & 4)) {
    entry.attribute = trapteamrecomp::attribute_cheats::Classify(base, op);
    entry.known |= 4;
  }
  const bool nightmare = (entry.known & 1) && entry.nightmare;
  const uint32_t master_switch = (entry.known & 2) ? entry.master_switch : 0;
  const auto attribute = attribute_on ? entry.attribute : trapteamrecomp::attribute_cheats::Kind::kNone;
  // attribute_cheats_hook.cpp: always crit and elemental power everywhere.
  if (attribute != trapteamrecomp::attribute_cheats::Kind::kNone) {
    trapteamrecomp::attribute_cheats::Apply(attribute, r3);
    return;
  }
  if (boost_on && master_switch) {
    static bool logged_boost = false;
    if (!logged_boost) {
      logged_boost = true;
      REXLOG_INFO("Extras: Traptanium boost experiment forces a master switch check");
    }
    r3.u64 = master_switch == 4 ? 1 : 0;  // > 0 true, == 0 false
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
