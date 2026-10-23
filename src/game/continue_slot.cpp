// Weiterspielen into the save played last (Core of Light #16, skylanders-portal continue_game).
//
// The library's continue presses A for player 1 until the game asks for a Skylander. That gets
// into the save played last only where the save slot screen starts on it (Swap Force,
// SuperChargers). This game's "Speicherslot wählen" (four slots, 1 2 / 3 4) always starts on
// slot 1: checked 2026-10-23 with saves in slots 1 and 2, slot 2 written last, the cursor on 1
// after a restart. So with a save played last in slot 2-4 a step before the presses' A moves the
// cursor there (Right for 2, Down for 3, both for 4) and lets the A go once it has moved.
//
// The slot played last: the Save_Slot_<n>.xsg written last (<user data>\<profile>\41560911\
// 00000001\Save_Slot_<n>\). The game makes all four files at its first start, the empty ones
// byte for byte the same, and writes a slot's file when a game in it starts or saves.
//
// Where the cursor is: the menu script keeps the highlighted entry in the script variable
// "selection cur" (script_vars.h), a pointer to the slot's "item info base" object, one per slot.
// Found 2026-10-23 by comparing the script variables in snapshots of the script heap with the
// cursor on slot 1, 2, 4 and 2 again (input script `save`, A0000000 + 512 MB); "current menu" was
// the slot page in all four. "selection cur" has no value before the first menu with entries; the
// step takes its first value after the start (the slot screen, the cursor on slot 1) as the place
// the cursor starts from, and a different value after the move as having arrived.
//
// It only reads guest memory and taps the d-pad as player 1; a real button press ends the
// continue presses and with them this step (the library's RealButtonPresses).

#include "continue_slot.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/input/input.h>
#include <rex/logging.h>

#include "portal_rex/portal_menu.h"
#include "portal_rex/portal_menu_input.h"
#include "script_vars.h"

REXCVAR_DECLARE(bool, continue_game);  // skylanders-portal portal_menu.cpp

namespace trapteamrecomp {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr const char* kTitleId = "41560911";
constexpr std::string_view kSelectionVariable = "selection cur";

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// 1-4: the slot whose save file was written last, 0 if none differs from the others (no save).
int LastPlayedSlot(const fs::path& user_data_root) {
  std::error_code ec;
  int best = 0;
  fs::file_time_type best_time{};
  std::vector<std::string> contents;
  for (const auto& profile : fs::directory_iterator(user_data_root, ec)) {
    const fs::path dir = profile.path() / kTitleId / "00000001";
    if (!fs::is_directory(dir, ec)) continue;
    for (int n = 1; n <= 4; ++n) {
      const std::string name = "Save_Slot_" + std::to_string(n);
      const fs::path file = dir / name / (name + ".xsg");
      const auto time = fs::last_write_time(file, ec);
      if (ec) continue;
      contents.push_back(ReadFile(file));
      if (!best || time > best_time) {
        best = n;
        best_time = time;
      }
    }
    break;  // the first profile folder with the game's saves
  }
  bool differ = false;
  for (const std::string& c : contents) differ |= c != contents.front();
  return differ ? best : 0;
}

struct Step {
  rex::memory::Memory* memory;
  int target;  // 2-4
  uint32_t variable = 0;
  uint32_t start = 0;  // the entry the cursor started on (slot 1)
  int checks = 0;      // calls since the move
  int moves = 0;
  bool done = false;
  std::chrono::steady_clock::time_point last_scan{};
};

void Move(int target) {
  using namespace skylanders::menu_input;
  if (target == 2 || target == 4) TapButton(0, rex::input::X_INPUT_GAMEPAD_DPAD_RIGHT);
  if (target == 4) std::this_thread::sleep_for(std::chrono::milliseconds(400));
  if (target == 3 || target == 4) TapButton(0, rex::input::X_INPUT_GAMEPAD_DPAD_DOWN);
}

// Before each continue A: true while it should wait (the cursor is on its way).
bool RunStep(Step& s) {
  if (s.done) return false;
  rex::memory::Memory* memory = s.memory;
  if (s.variable && !script::IsVariable(memory, s.variable, kSelectionVariable)) s.variable = 0;
  if (!s.variable) {
    const auto now = Clock::now();
    if (now - s.last_scan < std::chrono::milliseconds(1000)) return false;
    s.last_scan = now;
    s.variable = script::FindVariables(memory, {kSelectionVariable})[0];
    if (!s.variable) return false;  // no menu with entries yet: A as usual
    REXLOG_INFO("Continue slot: \"selection cur\" is at {:08X}", s.variable);
  }
  const uint32_t value = script::Value(memory, s.variable);
  if (!value) return false;
  if (!s.start) {
    s.start = value;
    REXLOG_INFO("Continue slot: on the slot screen ({:08X}), moving to slot {}", value, s.target);
    Move(s.target);
    s.moves = 1;
    return true;
  }
  if (value != s.start) {
    REXLOG_INFO("Continue slot: the cursor is on slot {} ({:08X})", s.target, value);
    s.done = true;
    return false;
  }
  if (++s.checks < 3) return true;
  if (s.moves < 2) {
    REXLOG_INFO("Continue slot: the cursor didn't move, once more");
    Move(s.target);
    s.moves = 2;
    s.checks = 0;
    return true;
  }
  REXLOG_WARN("Continue slot: the cursor stays on slot 1, A as usual");
  s.done = true;
  return false;
}

}  // namespace

void StartContinueSlot(rex::memory::Memory* memory, const fs::path& user_data_root) {
  if (!memory || !REXCVAR_GET(continue_game)) return;
  const int target = LastPlayedSlot(user_data_root);
  REXLOG_INFO("Continue slot: the save played last is in slot {}", target ? std::to_string(target) : "(none)");
  if (target < 2) return;
  auto step = std::make_shared<Step>(Step{memory, target});
  auto mutex = std::make_shared<std::mutex>();
  skylanders::SetContinueStepProvider([step, mutex] {
    std::lock_guard lock(*mutex);
    return RunStep(*step);
  });
}

}  // namespace trapteamrecomp
