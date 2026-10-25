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
//
// The trap request ("Stecke eine Wasser-Falle ins Portal", to catch a beaten villain) is the same
// player state variable with the state "Player State - Vault Capture", from the request until the
// trap is on ("Player State - Clear" then). Found 2026-10-25 in the owner's capture with a trace of
// every script variable about traps, villains and the mode (the villain in "trapping monster
// definition", e.g. "009 - tutorial ally"; "cur mode" went "world to portal" -> "idle"). The menu
// opens on the traps then without lifting the Skylander (skylanders::
// AutoOpenPortalMenuForCompanion): opened with the button it lifted the Skylander, the game
// switched to "Need Toy" and forgot the request, and the trap that went on afterwards only caught
// the villain when the game asked again (TrapTeamRecomp #12).
//
// Which element's trap it wants: the script variable "trapping monster definition" holds the
// villain to catch, an object named "<villain number> - <name>" ("009 - tutorial ally" in the
// owner's capture of 2026-10-25, where the game asked for a water trap). The number is the
// villain's id as the traps keep it (skylanders-portal trap_villain.h: 9 Gulper, whose trap is toy
// 211, Water), so the menu opens on that element's traps. The variable lives at the request only
// (at 0xBBBC63DC in that capture); without it the menu opens on the traps as a whole.
//
// A Skylander out of health: the game's text key for it is "304_PortalError_NeedsRemoval_Minion"
// ("this Skylander needs a break, take it off", in global.bld's portal error keys, as in Giants).
// Giants shows its portal texts through the script variable "portal error string" (its StringInfo,
// +8 the key); this game has the variable too. While it holds a 304 key the portal menu opens, to
// swap the Skylander for another (the owner, 2026-10-25: in every game). Not measured here yet:
// the key and the tag state are logged when they change, so a test round shows what the game does.

#include "portal_prompt_watch.h"
#include "script_vars.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <rex/logging.h>

#include "portal/figure_catalog.h"
#include "portal/figure_info.h"
#include "portal/trap_villain.h"
#include "portal_rex/portal_menu.h"

namespace trapteamrecomp {
namespace {

using Clock = std::chrono::steady_clock;

using script::IsVariable;
using script::ValueName;

constexpr std::string_view kStateVariable = "player state macro";
constexpr std::string_view kNeedToyState = "Player State - Need Toy";
constexpr std::string_view kNeedTrapState = "Player State - Vault Capture";
constexpr std::string_view kTagStateVariable = "portal state - tag state";
constexpr std::string_view kTrappingVariable = "trapping monster definition";
constexpr std::string_view kErrorVariable = "portal error string";
constexpr std::string_view kNeedsBreakKey = "304_";
// How long the trap request may wait for the villain's variable before the menu opens without it.
constexpr auto kTrapElementWait = std::chrono::milliseconds(1200);

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
constexpr auto kRescanInterval = std::chrono::milliseconds(500);
// How long the game must keep waiting before the menu opens, so it comes up with the prompt
// rather than during a one-frame flicker.
constexpr auto kOpenDelay = std::chrono::milliseconds(100);

std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_busy{false};  // the player state variable had a state at the last poll

bool IsStateVariable(rex::memory::Memory* memory, uint32_t object) {
  return IsVariable(memory, object, kStateVariable);
}

const TagError* FindTagError(const std::string& tag_state) {
  for (const TagError& error : kTagErrors) {
    if (tag_state.starts_with(error.state_prefix)) return &error;
  }
  return nullptr;
}

struct Variables {
  uint32_t state = 0;     // "player state macro"
  uint32_t tag = 0;       // "portal state - tag state"
  uint32_t trapping = 0;  // "trapping monster definition"
  uint32_t error = 0;     // "portal error string"
};

// Scans the physical heap for the variables. Takes a few tens of milliseconds.
Variables FindVariables(rex::memory::Memory* memory) {
  const std::vector<uint32_t> found =
      script::FindVariables(memory, {kStateVariable, kTagStateVariable, kTrappingVariable, kErrorVariable});
  return {found[0], found[1], found[2], found[3]};
}

// The element of the trap the game asks for, from the villain to catch ("009 - tutorial ally":
// villain 9, Gulper, a water trap), or kNone.
skylanders::portal::Element TrapElementWanted(rex::memory::Memory* memory, uint32_t trapping) {
  namespace portal = skylanders::portal;
  if (!trapping) return portal::Element::kNone;
  const std::string name = ValueName(memory, trapping);
  int id = 0;
  size_t digits = 0;
  while (digits < name.size() && digits < 3 && name[digits] >= '0' && name[digits] <= '9') {
    id = id * 10 + (name[digits] - '0');
    ++digits;
  }
  if (!digits || id <= 0 || id > 255) return portal::Element::kNone;
  const portal::TrapVillain* villain = portal::FindTrapVillain(uint8_t(id), false);
  if (!villain) return portal::Element::kNone;
  for (const portal::SkylanderInfo& sky : portal::AllSkylanders()) {
    if (sky.id == villain->trap_id && sky.category == "Traps") {
      REXLOG_INFO("Portal prompt: the trap is for \"{}\" ({}): {} trap", name, villain->name,
                  portal::ElementName(portal::TrapElement(sky)));
      return portal::TrapElement(sky);
    }
  }
  return portal::Element::kNone;
}

void Watch(rex::memory::Memory* memory) {
  Variables vars;
  std::optional<Clock::time_point> last_scan;
  std::string last_state, last_tag, last_error;
  std::optional<uint32_t> waiting_for;  // player whose prompt is up
  Clock::time_point since{};
  bool opened = false;
  bool trap_request = false, trap_opened = false;
  Clock::time_point trap_since{};
  std::optional<Clock::time_point> error_since, check_shown;
  while (!g_stop.load()) {
    const auto now = Clock::now();
    if (vars.state && !IsStateVariable(memory, vars.state)) vars.state = 0;
    if (vars.tag && !IsVariable(memory, vars.tag, kTagStateVariable)) vars.tag = 0;
    if (vars.trapping && !IsVariable(memory, vars.trapping, kTrappingVariable)) vars.trapping = 0;
    if (vars.error && !IsVariable(memory, vars.error, kErrorVariable)) vars.error = 0;
    if ((!vars.state || !vars.tag || !vars.error || (trap_request && !vars.trapping)) &&
        (!last_scan || now - *last_scan >= kRescanInterval)) {
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
      if (!vars.trapping && found.trapping) {
        vars.trapping = found.trapping;
        REXLOG_INFO("Portal prompt: the trapping variable is at {:08X}", vars.trapping);
      }
      if (!vars.error && found.error) {
        vars.error = found.error;
        REXLOG_INFO("Portal prompt: the portal error variable is at {:08X}", vars.error);
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
    const std::string error_key = vars.error ? ValueName(memory, vars.error) : std::string();
    if (error_key != last_error) {
      REXLOG_INFO("Portal prompt: portal error \"{}\"", error_key);
      if (error_key.starts_with(kNeedsBreakKey)) skylanders::PlayerSkylanderTired(0);
      last_error = error_key;
    }
    g_busy.store(!state.empty());
    std::optional<uint32_t> player;
    if (state == kNeedToyState || error_key.starts_with(kNeedsBreakKey)) player = 0;
    if (player != waiting_for) {
      if (waiting_for && opened) skylanders::CloseAutoOpenedPortalMenu();
      waiting_for = player;
      since = now;
      opened = false;
    }
    if (waiting_for && !opened && now - since >= kOpenDelay) {
      if (error_key.starts_with(kNeedsBreakKey)) {
        // The tired Skylander is still on the portal: player 1's menu, to swap it.
        REXLOG_INFO("Portal prompt: a Skylander needs a break (the menu opens for player 1)");
        skylanders::AutoOpenPortalMenu(0);
      } else {
        REXLOG_INFO("Portal prompt: the game asks for a Skylander (the menu opens for the first "
                    "player without one)");
        skylanders::AutoOpenPortalMenuForPlayerWithoutFigure();
      }
      opened = true;
    }
    // The trap request: the menu on the traps, the Skylander stays on. The trap is shared, so the
    // menu is player 1's; the element row is the player's to pick (the game's text names it).
    const bool trap_wanted = state == kNeedTrapState;
    if (trap_wanted != trap_request) {
      trap_request = trap_wanted;
      trap_since = now;
      trap_opened = false;
      if (!trap_wanted) skylanders::CompanionRequestEnded();
    }
    // On the requested element's traps: once the villain is known, or after a moment without it.
    if (trap_request && !trap_opened && now - trap_since >= kOpenDelay) {
      const skylanders::portal::Element element = TrapElementWanted(memory, vars.trapping);
      if (element != skylanders::portal::Element::kNone || now - trap_since >= kTrapElementWait) {
        REXLOG_INFO("Portal prompt: the game asks for a trap (the menu opens on the traps)");
        skylanders::AutoOpenPortalMenuForCompanion(0, element);
        trap_opened = true;
      }
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
