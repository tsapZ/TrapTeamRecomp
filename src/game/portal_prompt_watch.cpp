// Opens the portal menu when the game asks a player for a Skylander.
//
// Like Giants (GiantsRecomp src/game/portal_prompt_watch.cpp), this game runs its portal screens
// from a game script, and its script variables are ScriptReference objects: here vtable
// 0x821095A8, +8 the name, +0x1C the value. Unlike Giants' "portal error string", which stays 0 at
// this game's first prompt, the variable that says it is "player state macro": it exists while the
// portal UI runs a player's state machine ("Portal - UI - Player State Macro") and holds the state,
// whose name (+8 of the value) is "Player State - Need Toy" while the game waits for a Skylander.
// In play the variable is gone (its memory holds other variables). Found 2026-10-21 by comparing
// guest memory snapshots of the script heap (input script `save`, B6000000 + 64 MB) at the prompt
// in two runs and in play: the object was at 0xB6B9F5DC in both prompt snapshots.
// Names are pooled strings, and the pointers to them are tagged: the string's address + 1 (seen
// on every name, "player state macro" at 0xB7842BF2 is referenced as 0xB7842BF3).
//
// The variable only lives while the prompt is up, so it is looked for by a scan of the physical
// heap (class and name) every half second until it is found, and checked again on every poll.
// Polling instead of a mid-asm hook keeps the generated code untouched and costs a few guest reads
// every 30 ms. It only reads.
//
// The variable doesn't say whose prompt it is: at player 2's (player 1 playing, player 2 pressing
// A) the game makes a second one with the same state, and both point to the same state object
// (2026-10-21). The menu therefore opens for the first player without a Skylander on the emulated
// portal (skylanders::AutoOpenPortalMenuForPlayerWithoutFigure), the order in which the game itself
// hands figures to players: player 1 at the first prompt, player 2 while player 1 plays.
//
// The same variable tells when the game is busy with a figure change, for the portal menu's magic
// items (skylanders::SetGameBusyProvider): after a figure arrives the state goes from "Need Toy" to
// "Player State - Clear" for the Skylander's arrival (about 7 s in the logs of 2026-10-21: placed
// at 10.6 s, "Clear" at 11.0 s, the variable gone at 18.2 s), and the variable goes away when the
// player is back in play. So the game counts as busy while the variable has a state.
//
// The "too many" screens ("Es befinden sich zu viele Skylander / Magische Objekte auf dem
// Traptanium Portal") open the portal check, as in the other games. Giants' "portal error string"
// exists here too but stays 0 on them; the variable that says it is "portal state - tag state"
// (a ScriptReference like the other, at 0xB6BC17FC in the runs of 2026-10-23), 0 in play and on
// such a screen the tag state, an object named after it (+8): "too many minions" for a second
// Skylander of one player, "too many items" for a second magic item. Found 2026-10-23 by comparing
// the script heap's variables (input script `save`, B6000000 + 64 MB) in play, with a second
// Skylander and with a second magic item on the emulated portal (blank figure files); "cur mode"
// went from "world" to "portal tag error" at the same time. The script's other tag states, the
// objects next to these: "too many minions P1" / "P2", "too many traps", "too many tags", "tag
// error", "tag not usable", "data mismatch" (kTagErrors; only the two measured).

#include "portal_prompt_watch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/system/xmemory.h>
#include <rex/types.h>

#include "portal_rex/portal_menu.h"

namespace trapteamrecomp {
namespace {

using Clock = std::chrono::steady_clock;

constexpr uint32_t kScriptReferenceVtable = 0x821095A8;
constexpr uint32_t kNameOffset = 0x08;
constexpr uint32_t kValueOffset = 0x1C;
constexpr uint32_t kVariableSize = kValueOffset + 4;
constexpr std::string_view kStateVariable = "player state macro";
constexpr std::string_view kNeedToyState = "Player State - Need Toy";
constexpr std::string_view kTagStateVariable = "portal state - tag state";

// The portal check's reasons (Swap Force's EMagicMomentReason values, which the library uses).
constexpr uint32_t kCheckReasonOther = 0;
constexpr uint32_t kCheckReasonTooManySkylanders = 1;
constexpr uint32_t kCheckReasonTooManyItems = 2;

struct TagError {
  std::string_view state_prefix;
  uint32_t reason;
};
// The tag states that are a "too many" or figure error screen, by their name's start.
constexpr TagError kTagErrors[] = {
    {"too many minions", kCheckReasonTooManySkylanders},  // measured; also "... P1", "... P2"
    {"too many items", kCheckReasonTooManyItems},         // measured
    {"too many traps", kCheckReasonOther},
    {"too many tags", kCheckReasonOther},
    {"tag error", kCheckReasonOther},
    {"tag not usable", kCheckReasonOther},
    {"data mismatch", kCheckReasonOther},
};
// The error screen must stay this long before the portal check opens, and if it was closed with
// the error still up, it comes back after a while.
constexpr auto kErrorDelay = std::chrono::milliseconds(400);
constexpr auto kErrorRepeat = std::chrono::seconds(8);
// The view of physical memory the game's heap is in (the other two alias it); 64 KB pages.
constexpr uint32_t kScanStart = 0xA0000000;
constexpr uint32_t kScanEnd = 0xC0000000;
constexpr uint32_t kScanStep = 0x10000;
constexpr auto kRescanInterval = std::chrono::milliseconds(500);
// How long the game must keep waiting before the menu opens, so it comes up with the prompt
// rather than during a one-frame flicker.
constexpr auto kOpenDelay = std::chrono::milliseconds(100);

std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_busy{false};  // the player state variable had a state at the last poll

// How many bytes from `address` on are committed, readable guest memory (0 if none).
uint32_t ReadableBytes(rex::memory::Memory* memory, uint32_t address) {
  rex::memory::BaseHeap* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead)) {
    return 0;
  }
  // The region is counted in whole pages from the one that holds `address`.
  const uint64_t end = uint64_t(address & ~(heap->page_size() - 1)) + info.region_size;
  return end > address ? uint32_t(std::min<uint64_t>(end - address, UINT32_MAX)) : 0;
}

uint32_t ReadU32(rex::memory::Memory* memory, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(memory->TranslateVirtual<const uint8_t*>(address));
}

// The pooled name a tagged pointer refers to; empty if it isn't readable or doesn't end in time.
std::string ReadName(rex::memory::Memory* memory, uint32_t tagged) {
  constexpr uint32_t kMaxName = 128;
  const uint32_t address = tagged & ~1u;
  if (!address) return {};
  const uint32_t readable = std::min(ReadableBytes(memory, address), kMaxName);
  if (!readable) return {};
  const char* text = memory->TranslateVirtual<const char*>(address);
  const char* end = static_cast<const char*>(std::memchr(text, 0, readable));
  return end ? std::string(text, end) : std::string();
}

std::string ObjectName(rex::memory::Memory* memory, uint32_t object) {
  if (!object || ReadableBytes(memory, object) < kNameOffset + 4) return {};
  return ReadName(memory, ReadU32(memory, object + kNameOffset));
}

bool IsVariable(rex::memory::Memory* memory, uint32_t object, std::string_view name) {
  return object && ReadableBytes(memory, object) >= kVariableSize &&
         ReadU32(memory, object) == kScriptReferenceVtable && ObjectName(memory, object) == name;
}

bool IsStateVariable(rex::memory::Memory* memory, uint32_t object) {
  return IsVariable(memory, object, kStateVariable);
}

// The name of what the script variable at `variable` holds, empty if nothing.
std::string ValueName(rex::memory::Memory* memory, uint32_t variable) {
  return ObjectName(memory, ReadU32(memory, variable + kValueOffset));
}

const TagError* FindTagError(const std::string& tag_state) {
  for (const TagError& error : kTagErrors) {
    if (tag_state.starts_with(error.state_prefix)) return &error;
  }
  return nullptr;
}

struct Variables {
  uint32_t state = 0;  // "player state macro"
  uint32_t tag = 0;    // "portal state - tag state"
};

// Scans the physical heap for the variables. Takes a few tens of milliseconds.
Variables FindVariables(rex::memory::Memory* memory) {
  Variables found;
  const uint32_t needle = rex::byte_swap(kScriptReferenceVtable);  // as it is in guest memory
  uint64_t address = kScanStart;
  while (address < kScanEnd && !(found.state && found.tag)) {
    rex::memory::BaseHeap* heap = memory->LookupHeap(uint32_t(address));
    rex::memory::HeapAllocationInfo info{};
    if (!heap || !heap->QueryRegionInfo(uint32_t(address), &info) || !info.region_size) {
      address += kScanStep;
      continue;
    }
    const uint64_t region_end = address + info.region_size;  // `address` is page aligned
    if ((info.state & rex::memory::kMemoryAllocationCommit) &&
        (info.protect & rex::memory::kMemoryProtectRead) && info.region_size >= kVariableSize) {
      const uint8_t* host = memory->TranslateVirtual<const uint8_t*>(uint32_t(address));
      const uint64_t words = (info.region_size - kVariableSize) / 4 + 1;
      for (uint64_t i = 0; i < words; ++i) {
        uint32_t word;
        std::memcpy(&word, host + 4 * i, 4);
        if (word != needle) continue;
        const uint32_t object = uint32_t(address + 4 * i);
        if (!found.state && IsStateVariable(memory, object)) {
          found.state = object;
        } else if (!found.tag && IsVariable(memory, object, kTagStateVariable)) {
          found.tag = object;
        }
      }
    }
    address = std::max(region_end, address + kScanStep);
  }
  return found;
}

void Watch(rex::memory::Memory* memory) {
  Variables vars;
  std::optional<Clock::time_point> last_scan;
  std::string last_state, last_tag;
  std::optional<uint32_t> waiting_for;  // player whose prompt is up
  Clock::time_point since{};
  bool opened = false;
  std::optional<Clock::time_point> error_since, check_shown;
  while (!g_stop.load()) {
    const auto now = Clock::now();
    if (vars.state && !IsStateVariable(memory, vars.state)) vars.state = 0;
    if (vars.tag && !IsVariable(memory, vars.tag, kTagStateVariable)) vars.tag = 0;
    if ((!vars.state || !vars.tag) && (!last_scan || now - *last_scan >= kRescanInterval)) {
      const Variables found = FindVariables(memory);
      last_scan = now;
      if (!vars.state && found.state) {
        vars.state = found.state;
        REXLOG_INFO("Portal prompt: the player state variable is at {:08X}", vars.state);
      }
      if (!vars.tag && found.tag) {
        vars.tag = found.tag;
        REXLOG_INFO("Portal prompt: the tag state variable is at {:08X}", vars.tag);
      }
    }
    const std::string state = vars.state ? ValueName(memory, vars.state) : std::string();
    if (state != last_state) {
      REXLOG_INFO("Portal prompt: player state \"{}\"", state);
      last_state = state;
    }
    const std::string tag = vars.tag ? ValueName(memory, vars.tag) : std::string();
    if (tag != last_tag) {
      REXLOG_INFO("Portal prompt: tag state \"{}\"", tag);
      last_tag = tag;
    }
    if (const TagError* error = FindTagError(tag)) {
      if (!error_since) error_since = now;
      if (now - *error_since >= kErrorDelay && (!check_shown || now - *check_shown >= kErrorRepeat)) {
        REXLOG_INFO("Portal prompt: the game shows \"{}\", opening the portal check", tag);
        skylanders::ShowPortalCheck(error->reason);
        check_shown = now;
      }
    } else if (error_since) {
      if (check_shown) skylanders::ClosePortalCheck();
      error_since.reset();
      check_shown.reset();
    }
    g_busy.store(!state.empty());
    std::optional<uint32_t> player;
    if (state == kNeedToyState) player = 0;
    if (player != waiting_for) {
      if (waiting_for && opened) skylanders::CloseAutoOpenedPortalMenu();
      waiting_for = player;
      since = now;
      opened = false;
    }
    if (waiting_for && !opened && now - since >= kOpenDelay) {
      REXLOG_INFO("Portal prompt: the game asks for a Skylander (the menu opens for the first "
                  "player without one)");
      skylanders::AutoOpenPortalMenuForPlayerWithoutFigure();
      opened = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
}

}  // namespace

void StartPortalPromptWatch(rex::memory::Memory* memory) {
  if (!memory || g_thread.joinable()) return;
  g_stop.store(false);
  g_thread = std::thread(Watch, memory);
}

void StopPortalPromptWatch() {
  g_stop.store(true);
  if (g_thread.joinable()) g_thread.join();
}

bool PortalPromptActive() { return g_busy.load(); }

}  // namespace trapteamrecomp
