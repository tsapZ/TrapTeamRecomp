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
// every 30 ms. It only reads. The prompt is taken as player 1's; player 2's is still to be seen.

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

bool IsStateVariable(rex::memory::Memory* memory, uint32_t object) {
  return object && ReadableBytes(memory, object) >= kVariableSize &&
         ReadU32(memory, object) == kScriptReferenceVtable &&
         ObjectName(memory, object) == kStateVariable;
}

// Scans the physical heap for the variable. Takes a few tens of milliseconds.
uint32_t FindStateVariable(rex::memory::Memory* memory) {
  const uint32_t needle = rex::byte_swap(kScriptReferenceVtable);  // as it is in guest memory
  uint64_t address = kScanStart;
  while (address < kScanEnd) {
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
        if (IsStateVariable(memory, object)) return object;
      }
    }
    address = std::max(region_end, address + kScanStep);
  }
  return 0;
}

void Watch(rex::memory::Memory* memory) {
  uint32_t variable = 0;
  std::optional<Clock::time_point> last_scan;
  std::string last_state;
  std::optional<uint32_t> waiting_for;  // player whose prompt is up
  Clock::time_point since{};
  bool opened = false;
  while (!g_stop.load()) {
    const auto now = Clock::now();
    if (variable && !IsStateVariable(memory, variable)) variable = 0;
    if (!variable && (!last_scan || now - *last_scan >= kRescanInterval)) {
      variable = FindStateVariable(memory);
      last_scan = now;
      if (variable) REXLOG_INFO("Portal prompt: the player state variable is at {:08X}", variable);
    }
    std::string state;
    if (variable) {
      const uint32_t value = ReadU32(memory, variable + kValueOffset);
      state = ObjectName(memory, value);
    }
    if (state != last_state) {
      REXLOG_INFO("Portal prompt: player state \"{}\"", state);
      last_state = state;
    }
    std::optional<uint32_t> player;
    if (state == kNeedToyState) player = 0;
    if (player != waiting_for) {
      if (waiting_for && opened) skylanders::CloseAutoOpenedPortalMenu();
      waiting_for = player;
      since = now;
      opened = false;
    }
    if (waiting_for && !opened && now - since >= kOpenDelay) {
      REXLOG_INFO("Portal prompt: the game asks player {} for a Skylander", *waiting_for + 1);
      skylanders::AutoOpenPortalMenu(*waiting_for);
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

}  // namespace trapteamrecomp
