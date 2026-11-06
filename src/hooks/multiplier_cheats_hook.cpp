// Gold and XP multipliers, as cheats (SkylandersCoreOfLight #48; Swap Force, Giants, SuperChargers and
// Imaginators have them under the same setting names, the rows from skylanders-portal).
//
// Trap Team pays out both in its scripts (read 2026-11-06 from a heap snapshot of Level_302):
//
//   "Player - Minion - Award Money" (parameter amount):       ... val = the player's money;
//                                                             amount = val + amount
//                                                             amount > 65000 -> amount = 65000; saved
//   "Player - Minion - Award Experience" (parameter amount):  amount = exp value * amount, then added
//                                                             to the Skylander
//
// Every gold pickup and XP award goes through them. The scripts' addition (sub_82287DB0) and
// multiplication (sub_822883B0) are built as the subtraction script_cheats_hook.cpp hooks: the
// expression in r30, the left value into the result at r31, the right value from r1+80, three paths
// by the values' types (config/default.toml). The hooks there tell those two expressions by their
// names (the right side the macro's own parameter "amount", its macro +0x38 named as above; the left
// side "val" / "exp value": the macro's other additions of amount, its statistics, are left alone)
// and scale the right value, the award, by the multiplier when it is positive (a negative amount
// would be money spent). The game's own cap of 65000 gold stays. Nothing is written to the game's
// memory; the gold and the levels a Skylander gets are saved to its figure by the game as usual.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "portal_rex/achievements_ui.h"
#include "script_objects.h"

namespace {

using rex::memory::load_and_swap;
using trapteamrecomp::script::LastItem;
using trapteamrecomp::script::NameIs;
using trapteamrecomp::script::ReadU32;

constexpr uint32_t kParameter = 0x8210B0E0;  // a macro parameter: name +0x1C, its macro +0x38

enum class Kind : uint8_t { kNone, kGold, kXp };

uint8_t* Base() { return rex::system::kernel_state()->memory()->virtual_membase(); }

// The last item of the path the stack at `stack_field` holds, read directly (the game is evaluating
// it right now), or 0.
uint32_t LastItemDirect(uint8_t* base, uint32_t stack_field) {
  const uint32_t stack = load_and_swap<uint32_t>(base + stack_field);
  if (!stack) return 0;
  const uint32_t count = load_and_swap<uint32_t>(base + stack + 8);
  const uint32_t items = load_and_swap<uint32_t>(base + stack + 0x14);
  if (!count || count > 16 || !items) return 0;
  return load_and_swap<uint32_t>(base + items + 4 * (count - 1));
}

Kind Classify(uint8_t* base, uint32_t expression, Kind want) {
  // The cheap test first: the right side is a macro parameter. Only those go on to the names, which
  // ask the memory's protection (system calls).
  const uint32_t right = LastItemDirect(base, expression + 0x10);
  if (!right || load_and_swap<uint32_t>(base + right) != kParameter) return Kind::kNone;
  const uint32_t left = LastItem(base, expression + 8);
  uint32_t macro = 0;
  if (!left || !NameIs(base, right + 0x1C, "amount") || !ReadU32(base, right + 0x38, macro) || !macro) {
    return Kind::kNone;
  }
  if (want == Kind::kGold && NameIs(base, left + 0x1C, "val") &&
      NameIs(base, macro + 8, "Player - Minion - Award Money")) {
    return Kind::kGold;
  }
  if (want == Kind::kXp && NameIs(base, left + 0x1C, "exp value") &&
      NameIs(base, macro + 8, "Player - Minion - Award Experience")) {
    return Kind::kXp;
  }
  return Kind::kNone;
}

// The multiplier for the right value of the expression in r30, or 0 when it isn't the award `want`
// or its multiplier is off.
int Factor(const PPCRegister& r30, Kind want) {
  const int k = want == Kind::kGold ? skylanders::CheatGoldMultiplier() : skylanders::CheatXpMultiplier();
  if (k <= 1) return 0;
  uint8_t* base = Base();
  const uint32_t expression = r30.u32;
  // Every addition / multiplication of every script passes here while a multiplier is on, so each
  // expression's answer is kept in a direct-mapped table (a hash map's lookups cost Giants' hub about
  // 1 %, 2026-11-06), with both sides' pointers to tell another level's expression at the same
  // address; a slot two expressions share is only worked out again, cheaply (Classify's first test).
  struct Slot {
    uint32_t expression;
    Kind kind;
    uint64_t sides;
  };
  constexpr uint32_t kSlots = 8192;  // the index below is the hash's top 13 bits
  // Made on first use: a thread_local array would be reserved in every thread of the process.
  thread_local std::unique_ptr<Slot[]> tables[2];
  std::unique_ptr<Slot[]>& table = tables[want == Kind::kGold ? 0 : 1];
  if (!table) table.reset(new Slot[kSlots]());
  const uint64_t sides = (uint64_t(load_and_swap<uint32_t>(base + expression + 8)) << 32) |
                         load_and_swap<uint32_t>(base + expression + 0x10);
  Slot& slot = table[(expression * 2654435761u) >> 19];
  if (slot.expression != expression || slot.sides != sides) {
    slot = {expression, Classify(base, expression, want), sides};
  }
  return slot.kind == want ? k : 0;
}

void LogOnce(Kind kind, double before, double after) {
  static bool gold_logged = false, xp_logged = false;
  bool& logged = kind == Kind::kGold ? gold_logged : xp_logged;
  if (logged) return;
  logged = true;
  REXLOG_INFO("Extras: the {} multiplier scaled an award ({} -> {})", kind == Kind::kGold ? "gold" : "XP",
              before, after);
}

void ScaleInt(PPCRegister& value, Kind kind, int k) {
  const int64_t before = value.s32;
  if (before <= 0) return;
  const int64_t after = std::min<int64_t>(before * k, std::numeric_limits<int32_t>::max());
  value.s64 = after;
  LogOnce(kind, double(before), double(after));
}

void ScaleFloat(PPCRegister& value, Kind kind, int k) {
  const double before = value.f64;
  if (!(before > 0.0)) return;
  value.f64 = double(float(before * k));
  LogOnce(kind, before, value.f64);
}

}  // namespace

// '+' (sub_82287DB0): add r8,r10,r11, two ints, r11 the right.
void ScriptAddIntsHook(PPCRegister& r30, PPCRegister& r11) {
  if (const int k = Factor(r30, Kind::kGold)) ScaleInt(r11, Kind::kGold, k);
}

// fadds f12,f0,f13: the right a float in f13.
void ScriptAddFloatHook(PPCRegister& r30, PPCRegister& f13) {
  if (const int k = Factor(r30, Kind::kGold)) ScaleFloat(f13, Kind::kGold, k);
}

// fadds f10,f11,f0: a float plus an int, the int already made a float in f11.
void ScriptAddIntRightHook(PPCRegister& r30, PPCRegister& f11) {
  if (const int k = Factor(r30, Kind::kGold)) ScaleFloat(f11, Kind::kGold, k);
}

// '*' (sub_822883B0): mullw r8,r11,r9, two ints, r9 the right.
void ScriptMultiplyIntsHook(PPCRegister& r30, PPCRegister& r9) {
  if (const int k = Factor(r30, Kind::kXp)) ScaleInt(r9, Kind::kXp, k);
}

// fmuls f12,f0,f13: the right a float in f13.
void ScriptMultiplyFloatHook(PPCRegister& r30, PPCRegister& f13) {
  if (const int k = Factor(r30, Kind::kXp)) ScaleFloat(f13, Kind::kXp, k);
}

// fmuls f10,f11,f0: a float times an int, the int already made a float in f11.
void ScriptMultiplyIntRightHook(PPCRegister& r30, PPCRegister& f11) {
  if (const int k = Factor(r30, Kind::kXp)) ScaleFloat(f11, Kind::kXp, k);
}
