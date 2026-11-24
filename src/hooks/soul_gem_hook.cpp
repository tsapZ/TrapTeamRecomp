// A Soul Gem picked up, from Trap Team's own scripts (Core of Light #71, the Soul Gem run). For now only log lines, to
// be checked against a real pickup before the run uses it.
//
// 2026-11-24: a player's three real pickups in Level_313_Tutorial (TT 0.6.0-beta.7: three "Soul Gem movie" lines) gave
// no "Soul Gem (script)" line, so Trap Team's gem doesn't use "Check Soul Gem Achievement" as Giants' does. The other
// gem macros of the level scripts (the Academy snapshot's names) are logged now, each use with the level, a few
// times each per session: the next report with a pickup says which one is the pickup.
//
// Giants' gem object (ChampionUpgradeToken.ai) sets the gem collected with "Save - Collection Get/Set - Champion Token"
// and right after it uses "System - Achievements - Check Soul Gem Achievement", whatever the player answers at the
// preview; that macro has no parameters and no other use (GiantsRecomp's soul_gem_hook.cpp, 2026-11-21). Trap Team's
// global level.bld has the same two macros (found 2026-11-17), the same engine and script layout (script_objects.h),
// so the same use is taken as the pickup here; the movie the lib watches (soul_gems.h, movies\soulGem_<toy id>.mov)
// may open only on a Yes, as in Giants.
//
// A use-macro op runs in its execute, sub_82268AF8: the op type's vtable 8210B158 (docs of the dev kit) has it in slot
// 26, found 2026-11-21 against Giants' (vtable 820E0028, slot 27 = sub_821C4490): the three op-specific slots sit
// 0x120 / 0x3E0 apart in both, and the two functions call the same helpers in the same order (the path walker, then
// two pairs twice). r3 the op, its macro definition at +0x3C, a definition's name at +8 (a name's pointer may carry a
// tag bit). Every macro use of every script passes here, so the definition and its name pointer are read directly
// (the op is the one being run) and compared with the gem macros' once found; any other definition is looked at once
// (its name compared straight) and remembered in a table. Reads only.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "dev/where.h"
#include "hooks/script_objects.h"

namespace {

// The gem macros by name (Trap Team's Academy snapshot, runs\kit\tt_image): Giants' pickup signal first, then the
// level objective's count, the gem's map tag, the gem toy's creation and the preview's data-mining event.
constexpr const char* kGemMacros[] = {
    "System - Achievements - Check Soul Gem Achievement",
    "Objective - Soul Gems Collected",
    "Remove Tag - Soul Gem",
    "Create Toy - Soul Gem",
    "System - Data Mining - Event - Soul Gem Video Start",
};
constexpr int kGemMacroCount = int(sizeof(kGemMacros) / sizeof(kGemMacros[0]));

// The gem macros' definitions once seen (the snapshots had two per macro: global and per level) with their index, and
// the others looked at, each by the definition's address and its name pointer together: a level's definitions go at
// the next load and their memory is used again, so by the address alone a gem macro's definition where another
// macro's was before would never be looked at (review 2026-11-24). A slot written over by another one only means one
// more name read.
std::array<std::atomic<uint64_t>, 16> g_gem{};
std::array<std::atomic<int>, 16> g_gem_index{};
std::array<std::atomic<int>, kGemMacroCount> g_told{};  // log lines per macro this session
std::array<std::atomic<uint64_t>, 8192> g_other{};

// The definition's name compared straight, without script_objects.h's checked reads: those ask the system for the
// page protection twice per read, and this runs for every macro use of every script. With the cache below missing
// (more definitions than slots: they are per object), that cost Trap Team 0.6.0-beta.5 up to 40 % of its frame rate
// (CoL #71, a player's report 2026-11-21; dev kit A/B 176 -> 110 FPS in the Academy, 145 -> 85 in Level_313). The
// op is the one the game is running, so its definition and the definition's name are live; a name pointer outside
// the guest's heap and image is taken as no name.
// Which of kGemMacros the name is, or -1. Byte by byte up to the first difference, so a short name at the end of the
// memory it sits in is never read past its terminator. The first byte decides most of them: all start "S", "O", "R"
// or "C", and the game's other macro names are mostly "Portal - ...", "Minion - ..." and the like.
int GemMacroOfName(const uint8_t* base, uint32_t name) {
  name &= ~1u;  // the tag bit off
  if (name < 0x40000000u || name > 0xBFFFFF00u) return -1;
  const uint8_t* text = base + name;
  for (int i = 0; i < kGemMacroCount; ++i) {
    const char* want = kGemMacros[i];
    size_t k = 0;
    while (want[k] != 0 && text[k] == uint8_t(want[k])) ++k;
    if (want[k] == 0 && text[k] == 0) return i;
  }
  return -1;
}

int GemMacro(uint8_t* base, uint32_t def) {
  const uint32_t name = rex::memory::load_and_swap<uint32_t>(base + def + 8);
  const uint64_t key = (uint64_t(def) << 32) | name;
  for (size_t i = 0; i < g_gem.size(); ++i) {
    if (g_gem[i].load(std::memory_order_relaxed) == key) return g_gem_index[i].load(std::memory_order_relaxed);
  }
  std::atomic<uint64_t>& other = g_other[(def >> 2) % g_other.size()];
  if (other.load(std::memory_order_relaxed) == key) return -1;
  const int index = GemMacroOfName(base, name);
  if (index < 0) {
    other.store(key, std::memory_order_relaxed);
    return -1;
  }
  for (size_t i = 0; i < g_gem.size(); ++i) {
    uint64_t empty = 0;
    if (g_gem[i].compare_exchange_strong(empty, key)) {
      g_gem_index[i].store(index, std::memory_order_relaxed);
      break;
    }
  }
  return index;
}

void MacroUsed(uint8_t* base, uint32_t op) {
  if (op < 0x10000 || op > 0xFFFFFF00u) return;
  const uint32_t def = rex::memory::load_and_swap<uint32_t>(base + op + 0x3C);
  if (def < 0x40000000u || def > 0xBFFFFF00u) return;  // (the name is read at +8)
#if defined(SKYLANDERS_DEVKIT)
  // The developer kit says once that the hook runs (no pickup needed to see it work).
  static std::atomic<bool> told{false};
  if (!told.load(std::memory_order_relaxed) && !told.exchange(true)) {  // (no atomic write per call)
    uint32_t name = 0;
    const uint8_t* text = trapteamrecomp::script::ReadU32(base, def + 8, name)
                              ? trapteamrecomp::script::Readable(base, name & ~1u, 1)
                              : nullptr;
    REXLOG_INFO("Kit: use-macro hook live, first macro {}",
                text ? std::string(reinterpret_cast<const char*>(text), strnlen(reinterpret_cast<const char*>(text), 96))
                     : std::string("?"));
  }
#endif
  const int index = def < 0x10000 ? -1 : GemMacro(base, def);
  if (index < 0) return;
  // At most 40 lines per macro and session: one that runs every frame shows itself without filling the log.
  if (g_told[index].fetch_add(1, std::memory_order_relaxed) >= 40) return;
  REXLOG_INFO("Soul Gem (script): '{}' in {}", kGemMacros[index], skylanders::dev::CurrentLevel());
  if (index == 0 || index == 1 || index == 2) skylanders::dev::LogPlace("Soul Gem macro");
}

}  // namespace

extern "C" void __imp__sub_82268AF8(PPCContext& __restrict ctx, uint8_t* base);

REX_HOOK_RAW(sub_82268AF8) {
  MacroUsed(base, ctx.r3.u32);
  __imp__sub_82268AF8(ctx, base);
}
