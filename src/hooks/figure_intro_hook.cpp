// A shorter Skylander intro when a figure comes onto the portal (Core of Light #65, option, off by
// default; Swap Force, SuperChargers and Imaginators have it as figure_intro_seconds too).
//
// Trap Team's portal UI (Portal_UI.ai) runs the magic moment script "mmsky4": while its mode is
// "object magic moment" (about 4.5 s) it runs the macro "mmsky4 - show minion - update" every
// frame, then "object leap off" runs "mmsky4 - minion to world - update", the jump into the level
// (about 1.4 s). The show minion update is a row of stages (mm seq data.stage); each one waits
// with the check
//
//   check [<the game's time>] > mm seq data.timer
//
// and then plays its part (the portal light, the name card, the figure's own animation, its
// battle cry) and sets the next deadline: 1 s, 0.42 s, 0.25 s, 0.15 s, the length of the figure's
// animation (or 1.8 s), the battle cry's length. After the last stage it calls "Portal - UI - Mode
// Macro", the portal UI's "mode complete", and leap off begins.
//
// With figure_intro_seconds set, the hook makes those waits come out true once the magic moment has
// run that long: the remaining stages run within a few frames and the game's own leap off follows.
// The jump into the level keeps its time (it waits for the jump animation), and the capture of a
// villain ("mmsky4 - capture confirm - update", checks of the same shape) keeps its own.
//
// The waits are told apart by their sides (the operator >, the right side the path "mm seq data"
// "timer", the left side one variable) and by the macro whose op list holds them: a loaded script
// keeps a flat list of op pointers, each macro's ops after its definition (type 8210AF70 or
// 8210AEF8, the name at +8). So the list entry that points at the op is searched within 2 MB of it
// (in the 2026-11-11 snapshots 0x25000 to 0x46000 below the ops) and followed back to the
// definition. That happens once per op: nightmare_hook.cpp keeps the answer by the op's address.
// The magic moment begins with the first such wait after more than a second without one (leap off
// and the world between two figures have none). Host time, as the option is about the seconds a
// player waits.
//
// Found 2026-11-11 from the portal UI's mode objects in heap snapshots of a scripted arrival (Chop
// Chop placed in the Academy, on a copy of a save), decoding the scripts' op lists.

#include "figure_intro_hook.h"

#include <chrono>
#include <cstring>
#include <unordered_set>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>

#include "script_objects.h"

REXCVAR_DEFINE_DOUBLE(figure_intro_seconds, 0.0, "Game",
                      "How long a Skylander's intro may last when it comes onto the portal, in "
                      "seconds (0: as long as the game's, about 4.5 s)");

namespace trapteamrecomp::figure_intro {
namespace {

using script::NameIs;
using script::Readable;
using script::StackItem;

constexpr uint32_t kGreater = 4;  // a check's operator at +0x20: 0 <=, 1 ==, 2 >=, 3 <, 4 >, 5 !=
constexpr uint32_t kExpression = 0x82107CFC;
constexpr uint32_t kMacroDef = 0x8210AF70;
constexpr uint32_t kFlowMacroDef = 0x8210AEF8;
constexpr uint32_t kPage = 0x1000;
constexpr uint32_t kSearchSpan = 0x200000;
constexpr uint32_t kMaxOpsBack = 4096;

// Guest words read with one readability check per 4 KB page: the walk back through an op list
// reads thousands of words on a few dozen pages, and each check is two system calls.
class PageReader {
 public:
  explicit PageReader(uint8_t* base) : base_(base) {}

  bool Read(uint32_t address, uint32_t& out) {
    if (address & 3) return false;
    const uint32_t page = address & ~(kPage - 1);
    if (!readable_.count(page)) {
      if (!Readable(base_, page, kPage)) return false;
      readable_.insert(page);
    }
    out = rex::memory::load_and_swap<uint32_t>(base_ + address);
    return true;
  }

 private:
  uint8_t* base_;
  std::unordered_set<uint32_t> readable_;
};

// The macro definition whose op list holds the entry at `entry`: the entries before it are op
// pointers back to the definition. 0 if `entry` isn't in an op list.
uint32_t MacroOfListEntry(PageReader& reader, uint32_t entry) {
  for (uint32_t back = 0; back < kMaxOpsBack && entry >= 4 * back; ++back) {
    uint32_t item = 0, type = 0;
    if (!reader.Read(entry - 4 * back, item) || !reader.Read(item, type)) return 0;
    if (type == kMacroDef || type == kFlowMacroDef) return item;
    if (type < 0x82100000 || type >= 0x82120000) return 0;  // not an op (vtables are there)
  }
  return 0;
}

// The macro whose op list holds `op`, or 0.
uint32_t MacroOf(uint8_t* base, uint32_t op) {
  PageReader reader(base);
  uint8_t want[4];
  rex::memory::store_and_swap<uint32_t>(want, op);
  const uint32_t from = (op > kSearchSpan ? op - kSearchSpan : 0) & ~(kPage - 1);
  const uint32_t to = op < 0xFFFFFFFFu - kSearchSpan ? op + kSearchSpan : 0xFFFFF000u;
  for (uint32_t page = from; page < to; page += kPage) {
    const uint8_t* p = Readable(base, page, kPage);
    if (!p) continue;
    for (uint32_t at = 0; at < kPage; at += 4) {
      if (std::memcmp(p + at, want, 4) != 0) continue;
      if (const uint32_t macro = MacroOfListEntry(reader, page + at)) return macro;
    }
  }
  return 0;
}

}  // namespace

bool On() { return REXCVAR_GET(figure_intro_seconds) > 0.0; }

bool IsIntroWait(uint8_t* base, uint32_t op) {
  using rex::memory::load_and_swap;
  // Every check op of every script comes here once while the option is on, so the cheap tests come
  // first and read only what the game has just used to compare (the op, its right side's expression
  // and value stack, its left path's stack): no system calls for most checks. The right side: an
  // expression with only a left value (+8, a stack; +0x10 the right value, 0), the path mm seq
  // data.timer; the left side one variable.
  if (load_and_swap<uint32_t>(base + op + 0x20) != kGreater) return false;
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);
  if (!rhs || load_and_swap<uint32_t>(base + rhs) != kExpression || load_and_swap<uint32_t>(base + rhs + 0x10) != 0) {
    return false;
  }
  const uint32_t right_stack = load_and_swap<uint32_t>(base + rhs + 8);
  const uint32_t left_stack = load_and_swap<uint32_t>(base + op + 0x1C);
  if (!right_stack || !left_stack || load_and_swap<uint32_t>(base + right_stack + 8) != 2 ||
      load_and_swap<uint32_t>(base + left_stack + 8) != 1) {
    return false;
  }
  const uint32_t data = StackItem(base, rhs + 8, 0);
  const uint32_t timer = StackItem(base, rhs + 8, 1);
  if (!data || !timer || !NameIs(base, data + 0x1C, "mm seq data") || !NameIs(base, timer + 0x1C, "timer")) {
    return false;
  }
  const uint32_t macro = MacroOf(base, op);
  return macro && NameIs(base, macro + 8, "mmsky4 - show minion - update");
}

void Apply(PPCRegister& r3) {
  using Clock = std::chrono::steady_clock;
  thread_local Clock::time_point start, last;
  thread_local bool running = false;
  const auto now = Clock::now();
  if (!running || now - last > std::chrono::seconds(1)) start = now;  // a new magic moment
  running = true;
  last = now;
  const double limit = REXCVAR_GET(figure_intro_seconds);
  if (limit <= 0.0 || r3.s32 >= 1 || std::chrono::duration<double>(now - start).count() < limit) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Skylander intro: ends after {:.1f} s (figure_intro_seconds)", limit);
  }
  r3.u64 = 1;
}

}  // namespace trapteamrecomp::figure_intro
