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

REXCVAR_DEFINE_BOOL(nightmare_from_start, false, "Game",
                    "Nightmare difficulty can be picked from the start, without finishing Story Mode "
                    "first (the save is not changed)");

namespace {

// The guest bytes at `address`, or null when they aren't all readable.
const uint8_t* Readable(uint8_t* base, uint32_t address, uint32_t size) {
  if (address < 0x10000 || address > 0xFFFFFFFFu - size) return nullptr;
  for (const uint32_t at : {address, address + size - 1}) {
    size_t length = 1;
    rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
    if (!rex::memory::QueryProtect(base + at, length, access) || access == rex::memory::PageAccess::kNoAccess) {
      return nullptr;
    }
  }
  return base + address;
}

bool ReadU32(uint8_t* base, uint32_t address, uint32_t& out) {
  const uint8_t* p = Readable(base, address, 4);
  if (!p) return false;
  out = rex::memory::load_and_swap<uint32_t>(p);
  return true;
}

// Whether the name pointer at `address` points to exactly `text` (with its terminator).
bool NameIs(uint8_t* base, uint32_t address, const char* text) {
  uint32_t string = 0;
  if (!ReadU32(base, address, string)) return false;
  const uint32_t size = uint32_t(std::strlen(text)) + 1;
  const uint8_t* p = Readable(base, string & ~1u, size);  // the tag bit off
  return p && std::memcmp(p, text, size) == 0;
}

// The last item of the stack at `address` (a pointer to it), or 0.
uint32_t LastItem(uint8_t* base, uint32_t stack_field) {
  uint32_t stack = 0, count = 0, items = 0, item = 0;
  if (!ReadU32(base, stack_field, stack) || !ReadU32(base, stack + 8, count) || count == 0 || count > 16 ||
      !ReadU32(base, stack + 0x14, items) || !ReadU32(base, items + 4 * (count - 1), item)) {
    return 0;
  }
  return item;
}

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

}  // namespace

void NightmareCheckHook(PPCRegister& r3, PPCRegister& r31) {
  if (!REXCVAR_GET(nightmare_from_start) || r3.s32 >= 1) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  // Every check that comes out false passes here, many a frame, and the walk to the names asks
  // the memory's protection a dozen times (system calls): with the option on, the game slowed down
  // and its sound stuttered (TrapTeamRecomp #16). So each op is looked at once: its answer is
  // kept by its address, with its left and right side's pointers to tell a new script's op at the
  // same address (another level) from it.
  struct Seen {
    uint64_t sides;
    bool nightmare;
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint32_t op = r31.u32;
  const uint64_t sides = (uint64_t(rex::memory::load_and_swap<uint32_t>(base + op + 0x1C)) << 32) |
                         rex::memory::load_and_swap<uint32_t>(base + op + 0x24);
  bool nightmare;
  if (auto it = seen.find(op); it != seen.end() && it->second.sides == sides) {
    nightmare = it->second.nightmare;
  } else {
    if (seen.size() > 65536) seen.clear();
    nightmare = IsNightmareCheck(base, op);
    seen[op] = {sides, nightmare};
  }
  if (!nightmare) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Nightmare difficulty open from the start (nightmare_from_start)");
  }
  r3.u64 = 1;
}
