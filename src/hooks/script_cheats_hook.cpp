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
// Only that subtraction has "creature info temp.hp" on the left and "damage" on the right, but the
// macro serves every creature: the enemies' hits come through it too (seen in a test: 200, 300, 66 ...
// from Enigma's attacks), so it is skipped only for the player's creature. The scripts run each
// creature's macros with a context of its own, the r5 that sub_822877A8 (evaluate an expression,
// assign it) hands on (config/default.toml); every player hit in the test came with one context, the
// enemies' with others. Which context is the player's is learned from the HUD: "UI - Health Bar -
// Update" takes the same hit off the bar's own copy of the health right after ("damage taken" off
// <the bar>.hp), for the player's bar only. So a hit from an unknown context lands; a bar update
// that follows it within half a second with the same amount names its context as a player's, and
// from then on that context's hits are kept off, and so are their bar updates (the bar would run
// empty while the Skylander stands). A kept-off hit that no bar update follows means the context
// isn't a player's (any more: a level loads its creatures anew), and it is forgotten. So the first
// hit a Skylander takes after coming in still counts, none after it. Tested 2026-11-02 in Level_302
// (route tt_health_attack.txt, Enigma among the first enemies, attacking): his first hit (30) landed
// and named his context, the next eight (30 to 81) were kept off, the bar stayed at 349 of 379, and
// all of his hits on the enemies (30 to 300, a dozen contexts) landed.
//
// Nothing is written to the game's memory; turned off again, both run from where they are.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>

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

// The context of the expression being evaluated (ScriptEvalContextHook, on the scripts' thread).
thread_local uint32_t g_eval_context = 0;

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
  // The recent hits, kept off or not, waiting for the player's bar update (see the top).
  struct Hit {
    double amount;
    uint32_t context;
    bool kept_off;
    std::chrono::steady_clock::time_point when;
  };
  thread_local std::deque<Hit> recent;
  thread_local std::unordered_set<uint32_t> player_contexts;
  const auto now = std::chrono::steady_clock::now();
  while (!recent.empty() && now - recent.front().when > std::chrono::milliseconds(500)) {
    if (recent.front().kept_off) player_contexts.erase(recent.front().context);  // no bar followed
    recent.pop_front();
  }
  if (kind == Kind::kPlayerDamage && health) {
    const bool keep_off = player_contexts.count(g_eval_context) != 0;
    if (recent.size() < 32) recent.push_back({right, g_eval_context, keep_off, now});
    if (keep_off) {
      static bool logged = false;
      if (!logged) {
        logged = true;
        REXLOG_INFO("Extras: no health loss kept a hit's damage off ({})", path);
      }
    }
    return keep_off;
  }
  if (kind == Kind::kBarDamage && health) {
    for (auto it = recent.begin(); it != recent.end(); ++it) {
      if (std::fabs(it->amount - right) >= 0.01) continue;
      const bool kept_off = it->kept_off;
      if (!kept_off && player_contexts.size() < 8 && player_contexts.insert(it->context).second) {
        REXLOG_INFO("Extras: no health loss knows a player's creature now (after its first hit)");
      }
      recent.erase(it);
      return kept_off;
    }
  }
  return false;
}

}  // namespace

// The entry of sub_822877A8 (evaluate the expression in r3, assign it to r4): r5 is the context it
// hands on, one per creature running the macro (see the top). Only noted.
void ScriptEvalContextHook(PPCRegister& r5) { g_eval_context = r5.u32; }

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
