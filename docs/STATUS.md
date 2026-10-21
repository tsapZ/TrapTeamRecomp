# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-21)

| Area | State |
|---|---|
| Repository | scaffolded from GiantsRecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, achievements, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (2124 files), `default.xex` hash pinned |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with the other games |
| Codegen | runs; 14 function starts the analyzer put one instruction late are registered in `config/default.toml` before anything calls them |
| Boot | ✅ to the title screen on the first try: the copyright screen, the Activision and Toys for Bob logo movies, then "Drücke A zum Starten" (German by language "auto") |
| Menus | ✅ autosave notice, save slot, difficulty, "Spiel wählen" (Story); Start skips the opening movie |
| Logo movies | skipped (`skip_intro`, on): the title screen after about 15 s; the opening movie optional (`skip_opening_movie`, off) |
| In-game | ✅ Chapter 1 (Sprudelquell) loads, the pause menu opens, the cutscene plays |
| Portal | ✅ emulated: the game asks "Spieler 1: Setze einen Skylander auf das Traptanium-Portal", a Spyro's Adventure figure put on by script (Chop Chop) gets its intro and plays with the HUD |
| Portal menu | ✅ opens by itself at the game's "Setze einen Skylander" prompt (player 1) and shows the game's own HUD portraits for every Skylander and Mini, each figure's variant where the game has one (Series 2, LightCore, Legendary, Eon's Elite, ...) |

## 2026-10-21: session log

- **Own repository**, like the other games: each playable on its own, later through the hub
  (Skylanders: Core of Light).
- **Scaffold:** GiantsRecomp's tracked files, renamed; its Giants-specific parts left out.
- **Disc:** the same layout as Giants (`character\`, `item\`, `level\`, `misc\`, `movies\`, `outfit\`, `permanent\`, `.arc` / `.bld`), but the archives are IGA version 11 (Giants: 8), so Giants' readers for portraits, upgrades, hats and item names are left out until they read this version.
- **First codegen and boot:** no codegen fixes needed so far (Giants needed four Bink jump tables); the title screen comes up after the logo movies.
- **First level with a figure** (script, test copy): the menus behind the title, the opening movie (Start skips it), Chapter 1 and the portal prompt; Chop Chop put on the emulated portal comes in with his intro and plays. Next: the menu opening by itself at the prompt (the game's prompt object), the logo and opening movie skip, the game's pictures (IGA v11).
- **14 late function starts registered:** SuperChargers and Imaginators stopped at boot on functions the analyzer had started one instruction late; the same search in this game's code found 14, none reached yet. Registered; Chapter 1 with the figure checked again afterwards.
- **Logo movie skip** ported from GiantsRecomp: the midasm hook at 0x821A3DA0 right after BinkOpen reads the 44-byte header (here at r1+112), the movies recognised by their size and frame count (Activision 226, Toys for Bob 300 frames; the opening movie FMV1.mov 6941). Checked: both logos skipped, the title at 15 s.
- **Portal menu with Guide** (script, in Chapter 1): it waits for the game's portal screen, then lists every game's figures up to Trap Team (tabs Zuletzt, Spyro's Adventure, Giants, Swap Force, Trap Team, Magische Objekte) with the element symbols (no pictures yet: no reader for this game's archives); choosing Cynder opened the variant chooser as in the other games.

## 2026-10-21: session log (morning)

- **The game's pictures in the portal menu** (src/game/toy_portraits.cpp): every Skylander's scene
  archive (rom/character/NNN_<model>.bld, IGA version 11, little-endian header,
  src/game/alchemy/iga11_archive.cpp) holds its HUD portraits in its level.bld (IGZ version 8,
  big-endian, the layout of Swap Force's version 7, src/game/alchemy/igz8_file.cpp) as 214x107
  DXT5 textures, one per figure of that character across the games: "ssa_whirlwind",
  "sg_whirlwind_series2", "sf_whirlwindhornblast", "2014_whirlwind_eonselite". The catalog
  variant picks the game's prefix, then legendary / other decos, Series 2 (Eon's Elite, re-pose)
  and LightCore. The Minis' scenes are numbered apart from their ids and are read by name.
  Stored upside down like Giants'; centred in a square for the menu's tiles. Indexed on the
  disc: 252 portraits and 19 Mini pictures in 520 archives, read in 2.4 s at start-up. Checked by
  script: Magic, Earth, Water and Fire of Spyro's Adventure show the right Skylanders.
- **The portal menu opens by itself** (src/game/portal_prompt_watch.cpp): memory snapshots of the
  script heap at the prompt (two runs) and in play showed the variable "player state macro"
  (ScriptReference vtable 0x821095A8) holding "Player State - Need Toy" only at the prompt; the
  game's names are pooled and referenced by tagged pointers (address + 1). The watch finds the
  variable by a heap scan every half second and opens the menu for player 1. Checked by script:
  the menu came up by itself 0.1 s after the prompt. Player 2's prompt and the "too many" screens
  are still to look at.
- **Player 2's prompt** (checked by script: player 1 plays Chop Chop, player 2 presses A): the game
  makes a second "player state macro" variable (at 0xB79B7918) with "Player State - Need Toy", so the
  watch opens the menu, but for player 1: the variable doesn't say whose it is, and both players'
  variables point to the same state object. Nothing near it names the player (snapshot at player
  2's prompt); still to find, e.g. the player object the state machine runs for.
  **Solved the same day (#1):** the menu now opens for the first player without a Skylander on the
  emulated portal (skylanders-portal `AutoOpenPortalMenuForPlayerWithoutFigure`), the order in which
  the game hands figures to players. Checked by script: player 1 took Double Trouble through the
  menu at the first prompt; player 2 pressed A to join and the menu came up for player 2, with
  Double Trouble shown taken by player 1.
- **Native 3D picture, experimental** (src/hooks/scene_size_hook.cpp, `scene_size`, Graphics page,
  off by default): the 360 draws the 3D scene at 1040x620 (mainScene in renderer/tfbRenderer.igx)
  and stretches it to 720p. SwapForceRecomp's hook, the same render code: the target size function
  (0x8243F650) and the three EDRAM places (planning heap, tiling decision, depth base) found in a
  dump of this game's code by their instructions. With it on the scene is drawn at 1264x704 with the
  SDK's 4096-tile EDRAM (D3D12). Checked by script in Chapter 1: the target goes 1040x620 ->
  1264x704, the G-buffer pass (2816 tiles) goes untiled, the scene draws right, the edges about 12 %
  sharper (mean gradient of the same frame), no crash.
- **Ultrawide** (src/hooks/ultrawide_hook.cpp, `ultrawide_fov`, on): a window wider than 16:9
  (`resolution`) shows more to the sides instead of black bars, as in SwapForceRecomp. The
  perspective builder (sub_823B1AF0) found by Swap Force's instructions in a dump of this game's
  code; the scene camera's call with a probe, a hook at each of the builder's callers logging its
  arguments in play: 0x8230F784 and 0x8230F7F0 (the camera builds its projection twice a frame) runs
  every frame with 16:9 and the camera's horizontal FOV. The hook widens the aspect and the FOV
  there, keeping the vertical FOV; the app turns the letterbox off. Checked by script: Chapter 1 in
  a 2560x1080 window: the proportions right (the posts and the figure as in 16:9), more of the level
  at both sides; the HUD stretched.
- **High frame rate measured** (F4 > Bildrate, `frame_rate_limit`): the game swaps with interval 1,
  so its rate follows the emulated vblank: 59.6 / 58.8 FPS in Chapter 1 with 60, 110 / 96 with 144.
  Game speed: with the same script (2 s of stick forward), the figure stands on the same spot at
  144 as at 30 FPS (screenshots); the figure's arrival (portal state "Clear" to play) took 6.8 s at
  30 and 5.7 s at 144, as it includes loading the figure. No hook needed, as in Giants.
