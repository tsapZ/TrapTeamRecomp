// Villain time only on hits, as an option (TrapTeamRecomp #18): a villain played from a trap stays
// until it takes hits, its time bar working like a health bar.
//
// Trap Team counts villain time in a script, not in code: the global include Sky_Allies.ai has the
// macro "Villain Timer Internal - Update", run every frame, which (read 2026-11-01 from heap
// snapshots, the ops decoded by their types) does
//
//   if control.timer update disabled or the level's ^unlimited timer: nothing
//   if control.timer depleting != 0                      (the villain is in play)
//     control.timer change this frame = 0 - time elapsed this frame
//     control.timer value = control.timer value - time elapsed this frame    <- this subtraction
//     once control.timer value is used up: the villain goes back into the trap
//   else: recharge by "timer - recharge time/sec" up to "timer - base amount"
//   control.timer pct = control.timer value / timer - base amount           (the HUD's bar)
//
// "timer update disabled" follows the macro "Villain - Timer - Set Timer Unlimited Mode", which the
// levels turn on where villain time shouldn't run (the Academy, Level_302's ride down the dune).
// Hits take time off in another macro, "Villain - On Hurt Timer Reduction", in either mode: control's
// timer value minus incoming damage / 100 * the villain's "timer - reduction per 100 hp of damage".
// It goes through the same subtraction but with a temporary on the right, so it stays as it is, and
// so do the tag-out at 0, the warning music, the recharge and the bar.
//
// The subtraction is sub_822880B0 (the scripts' "-": evaluates the expression's left value, +8, and
// right value, +0x10, and stores left - right). 0x82288160 is its fsubs f12,f0,f13 for two floats:
// f0 the left, f13 the right, r30 the expression (config/default.toml). With
// cheat_villain_time_hits_only on, the hook makes f13 zero for exactly the expression "control.timer
// value - time elapsed this frame", told by its variables' names: of the 536 subtractions loaded in
// the Academy, only that one has "time elapsed this frame" on its right. Nothing is written to the
// game's memory; turned off again, the time runs from where it is.
//
// Tested 2026-11-01 in the Academy with a test build that forced the unlimited mode off: without the
// option the time went down every frame (45.00, 44.95, 44.90 ...; the bar at a third after 46 s),
// with it the bar stayed full for a minute and the time was only written once, when the villain came
// out.

#include <cstdint>
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

namespace {

using trapteamrecomp::script::LastItem;
using trapteamrecomp::script::NameIs;
using trapteamrecomp::script::StackItem;

// Whether the expression at `rhs` is "control.timer value - time elapsed this frame".
bool IsTimeDrain(uint8_t* base, uint32_t rhs) {
  const uint32_t left_first = StackItem(base, rhs + 8, 0);
  const uint32_t left_last = LastItem(base, rhs + 8);
  const uint32_t right_last = LastItem(base, rhs + 0x10);
  return left_first && left_last && right_last && NameIs(base, right_last + 0x1C, "time elapsed this frame") &&
         NameIs(base, left_last + 0x1C, "timer value") && NameIs(base, left_first + 0x1C, "control");
}

}  // namespace

void VillainTimeDrainHook(PPCRegister& r30, PPCRegister& f13) {
  if (!REXCVAR_GET(cheat_villain_time_hits_only)) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  // Every float subtraction of every script passes here, and the names cost a dozen memory
  // protection queries (system calls; see nightmare_hook.cpp, TrapTeamRecomp #16), so each
  // expression is looked at once: its answer is kept by its address, with its two sides' pointers
  // (which the game has just read) to tell another level's expression at the same address from it.
  struct Seen {
    uint64_t sides;
    bool drain;
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint32_t rhs = r30.u32;
  const uint64_t sides = (uint64_t(rex::memory::load_and_swap<uint32_t>(base + rhs + 8)) << 32) |
                         rex::memory::load_and_swap<uint32_t>(base + rhs + 0x10);
  bool drain;
  if (auto it = seen.find(rhs); it != seen.end() && it->second.sides == sides) {
    drain = it->second.drain;
  } else {
    if (seen.size() > 65536) seen.clear();
    drain = IsTimeDrain(base, rhs);
    seen[rhs] = {sides, drain};
  }
  if (!drain) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Villain time: only hits take it off (cheat_villain_time_hits_only)");
  }
  f13.f64 = 0.0;
}
