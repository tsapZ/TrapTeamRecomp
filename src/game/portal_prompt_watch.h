#pragma once

// Opens the portal menu when the game asks a player for a Skylander ("Spieler 1: Setze einen
// Skylander auf das Traptanium-Portal") and closes it again if the game stops waiting before a
// figure was chosen. Reads the game's portal script from guest memory; see portal_prompt_watch.cpp.

namespace rex::memory {
class Memory;
}  // namespace rex::memory

namespace trapteamrecomp {

// Starts the watcher thread. Call once, after the runtime is set up.
void StartPortalPromptWatch(rex::memory::Memory* memory);
// Stops and joins it; call before the runtime (and its guest memory) goes away.
void StopPortalPromptWatch();
// Whether the game is busy with a figure change: its prompt or a Skylander's arrival (see the .cpp).
bool PortalPromptActive();

}  // namespace trapteamrecomp
