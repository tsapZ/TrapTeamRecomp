// Two cheats on the scripts' subtraction: villain time only on hits (TrapTeamRecomp #18) and no
// health loss (as Spyro's Adventure's, under the same setting name).
//
// Trap Team counts both in its scripts, not in code, and both go through the scripts' "-",
// sub_822880B0: it evaluates the expression's left value (+8) and right value (+0x10) and stores left
// minus right, on one of three paths picked by the two values' types (config/default.toml): two
// floats (fsubs f12,f0,f13 at 0x82288160), a float minus an int (fsubs f10,f0,f11 at 0x8228818C) or
// two ints (subf r8,r9,r11 at 0x82288120). r30 holds the expression on all three. A hook makes the
// right value 0 for exactly the two expressions below, told by their variables' names (read
// 2026-11-01/02 from heap snapshots, the ops decoded by their types; the kit's docs/tt.md).
//
// Villain time. The global include Sky_Allies.ai has the macro "Villain Timer Internal - Update", run
// every frame:
//
//   if control.timer update disabled or the level's ^unlimited timer: nothing
//   if control.timer depleting != 0                      (the villain is in play)
//     control.timer change this frame = 0 - time elapsed this frame
//     control.timer value = control.timer value - time elapsed this frame    <- skipped
//     once control.timer value is used up: the villain goes back into the trap
//   else: recharge by "timer - recharge time/sec" up to "timer - base amount"
//   control.timer pct = control.timer value / timer - base amount           (the HUD's bar)
//
// "timer update disabled" follows the macro "Villain - Timer - Set Timer Unlimited Mode", which the
// levels turn on where villain time shouldn't run (the Academy, Level_302's ride down the dune).
// Hits take time off in another macro, "Villain - On Hurt Timer Reduction", in either mode, through
// the same subtraction with a temporary on the right, so they stay, and so do the tag-out at 0, the
// warning music, the recharge and the bar. Of the 536 subtractions loaded in the Academy, only the
// skipped one has "time elapsed this frame" on its right. Tested 2026-11-01 in the Academy with a
// test build that forced the unlimited mode off: without the option the time fell every frame (the
// bar at a third after 46 s), with it the bar stayed full for a minute (the float path).
//
// Health. The macro "On Hurt - Player On Hurt" takes a hit off the player's creature:
//
//   damage = damage + element add damage
//   unless immune or invulnerable, and damage > 0:
//     creature info temp.hp = creature info temp.hp - damage        <- skipped
//     if creature info temp.hp < 0: the creature is beaten (and a killing blow is noted)
//     knockback, freeze frames, the hurt sound and animation
//
// Only that subtraction has "creature info temp.hp" on the left and "damage" on the right. With it
// skipped the hit still lands (knockback, sound, animation), the health stays. The HUD's bar keeps a
// copy of its own: "UI - Health Bar - Update" takes "damage taken" off <the bar>.hp right after, so
// with only the first skipped the Skylander stood on with an empty bar (tested). That macro serves
// every bar, so its subtraction is skipped only when it follows a hit kept off the player within
// half a second with the same amount (the ints 30, 36, 38 ... in the test came in pairs). Tested
// 2026-11-02 in Level_302 (route tt_health_test.txt, Enigma idle among the first enemies): without
// the option 379 -> beaten before 20 s, with it the bar stayed at 379 for 40 s of hits and the
// enemies still went down to his attacks.
//
// Nothing is written to the game's memory; turned off again, both run from where they are.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "script_objects.h"

REXCVAR_DEFINE_BOOL(cheat_villain_time_hits_only, false, "Extras",
                    "A villain played from a trap loses time only when it takes hits, not by itself "
                    "(the game's per-second drain is skipped)");
REXCVAR_DEFINE_BOOL(cheat_no_health_loss, false, "Extras",
                    "Skylanders don't lose health when they get hit (achievements don't unlock while it's on)");

namespace {

using trapteamrecomp::script::LastItem;
using trapteamrecomp::script::NameIs;
using trapteamrecomp::script::StackItem;

enum class Kind : uint8_t { kOther, kVillainTimeDrain, kPlayerDamage, kBarDamage };

// Which of the two subtractions the expression at `rhs` is, if any.
Kind Classify(uint8_t* base, uint32_t rhs) {
  const uint32_t left_first = StackItem(base, rhs + 8, 0);
  const uint32_t left_last = LastItem(base, rhs + 8);
  const uint32_t right_last = LastItem(base, rhs + 0x10);
  if (!left_first || !left_last || !right_last) return Kind::kOther;
  if (NameIs(base, right_last + 0x1C, "time elapsed this frame") && NameIs(base, left_last + 0x1C, "timer value") &&
      NameIs(base, left_first + 0x1C, "control")) {
    return Kind::kVillainTimeDrain;
  }
  if (NameIs(base, right_last + 0x1C, "damage") && NameIs(base, left_last + 0x1C, "hp") &&
      NameIs(base, left_first + 0x1C, "creature info temp")) {
    return Kind::kPlayerDamage;
  }
  // "UI - Health Bar - Update": <the bar>.hp - damage taken (the bar is found at run time, unnamed).
  if (NameIs(base, right_last + 0x1C, "damage taken") && NameIs(base, left_last + 0x1C, "hp")) {
    return Kind::kBarDamage;
  }
  return Kind::kOther;
}

// Whether the subtraction of the expression in r30 is to subtract 0. `path` names the hook for the
// log line.
bool SkipSubtraction(const PPCRegister& r30, double right, const char* path) {
  const bool villain = REXCVAR_GET(cheat_villain_time_hits_only);
  const bool health = REXCVAR_GET(cheat_no_health_loss);
  if (!villain && !health) return false;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  // Every subtraction of every script passes here, and the names cost a dozen memory protection
  // queries (system calls; see nightmare_hook.cpp, TrapTeamRecomp #16), so each expression is looked
  // at once: its answer is kept by its address, with its two sides' pointers (which the game has
  // just read) to tell another level's expression at the same address from it.
  struct Seen {
    uint64_t sides;
    Kind kind;
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint32_t rhs = r30.u32;
  const uint64_t sides = (uint64_t(rex::memory::load_and_swap<uint32_t>(base + rhs + 8)) << 32) |
                         rex::memory::load_and_swap<uint32_t>(base + rhs + 0x10);
  Kind kind;
  if (auto it = seen.find(rhs); it != seen.end() && it->second.sides == sides) {
    kind = it->second.kind;
  } else {
    if (seen.size() > 65536) seen.clear();
    kind = Classify(base, rhs);
    seen[rhs] = {sides, kind};
  }
  if (kind == Kind::kVillainTimeDrain && villain) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      REXLOG_INFO("Villain time: only hits take it off (cheat_villain_time_hits_only, {})", path);
    }
    return true;
  }
  // The hits kept off the player, for their HUD bar: the bar macro takes the same hit off its own
  // copy of the health right after, and a bar of a hit that was kept off would run empty while the
  // Skylander stands. Only a bar update that follows such a hit with the same amount is skipped, so a
  // boss's or another creature's bar still goes down.
  struct Hit {
    double amount;
    std::chrono::steady_clock::time_point when;
  };
  thread_local std::deque<Hit> kept_off;
  const auto now = std::chrono::steady_clock::now();
  while (!kept_off.empty() && now - kept_off.front().when > std::chrono::milliseconds(500)) kept_off.pop_front();
  if (kind == Kind::kPlayerDamage && health) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      REXLOG_INFO("Extras: no health loss kept a hit's damage off ({})", path);
    }
    if (kept_off.size() < 16) kept_off.push_back({right, now});
    return true;
  }
  if (kind == Kind::kBarDamage && health) {
    for (auto it = kept_off.begin(); it != kept_off.end(); ++it) {
      if (std::fabs(it->amount - right) < 0.01) {
        kept_off.erase(it);
        return true;
      }
    }
  }
  return false;
}

}  // namespace

// fsubs f12,f0,f13: both values floats.
void ScriptSubtractFloatHook(PPCRegister& r30, PPCRegister& f13) {
  if (SkipSubtraction(r30, f13.f64, "floats")) f13.f64 = 0.0;
}

// fsubs f10,f0,f11: a float minus an int, the int already made a float in f11.
void ScriptSubtractIntRightHook(PPCRegister& r30, PPCRegister& f11) {
  if (SkipSubtraction(r30, f11.f64, "float minus int")) f11.f64 = 0.0;
}

// subf r8,r9,r11: two ints, r11 - r9.
void ScriptSubtractIntsHook(PPCRegister& r30, PPCRegister& r9) {
  if (SkipSubtraction(r30, double(r9.s32), "ints")) r9.u64 = 0;
}
