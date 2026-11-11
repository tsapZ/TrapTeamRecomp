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
| Portal menu | ✅ opens by itself at the game's "Setze einen Skylander" prompt (for the first player without a Skylander) and shows the cards of the game's collection screens for every Skylander, Mini, trap and magic item, each figure's own variant where the game has one; titles and text in the game's own fonts, the element symbols (Light and Dark too) and the gold coin from the game; the details view shows the hat (its name in the menu's language, icon and bonuses with the stats screen's symbols) and every Skylander's upgrade tree with the game's pictures and prices, magic items have their German names |

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
  the menu came up by itself 0.1 s after the prompt. Player 2's prompt: below; the "too many" screens:
  2026-10-23.
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
- **The HUD at the window's edges with ultrawide** (`ultrawide_hud`, on; F4 > Grafik > "HUD bei
  Ultrawide", #4): the UI lays itself out on its screen object's width (+188, 1280), set by
  sub_821DD170 from the display's size; a hook at that store (0x821DD2D4) sets it to the height x
  the window's aspect, as GiantsRecomp does. Found from the code loading the 1280 / 720 floats.
  Checked by script in a 2560x1080 window: the HUD keeps its proportions at the left edge, the
  dialogue box, the autosave notice, the portal prompt and the pause menu look right.
- **Movies on ultrawide** (2026-10-21): the logo movies and the opening movie are 16:9 Bink
  movies; while one plays the presenter shows it with black bars at the sides instead of
  stretched (present_use_actual_aspect off and present_letterbox on for that time, not saved).
  Start: BinkOpen's header hook; end: a hook at the start of BinkClose (sub_821A5FE8, called only
  from the game's movie close). Checked by script in a 2560x1080 window with skip_intro off: the
  logos 16:9 in the middle, the screen after them fills the window again.
- **High frame rate measured** (F4 > Bildrate, `frame_rate_limit`): the game swaps with interval 1,
  so its rate follows the emulated vblank: 59.6 / 58.8 FPS in Chapter 1 with 60, 110 / 96 with 144.
  Game speed: with the same script (2 s of stick forward), the figure stands on the same spot at
  144 as at 30 FPS (screenshots); the figure's arrival (portal state "Clear" to play) took 6.8 s at
  30 and 5.7 s at 144, as it includes loading the figure. No hook needed, as in Giants.

## 2026-10-21: session log (afternoon)

- **The collection's cards in the portal menu** (src/game/toy_portraits.cpp): the HUD portraits
  (214x107, cut off at the sides) looked poor next to the other games' pictures. The game's
  collection screens have their own: rom/misc/UI_Collection_Champions.arc (Skylanders, Minis, magic
  items) and rom/misc/ui_collection_traps.arc (traps), 347 textures of 128x128 DXT5 with the card's
  frame, each in a folder named after its figure ("519_12288_triggersnappy.png": id 519, variant
  12288, the catalog's numbers). 345 of the catalog's 434 figures up to this game have a card of
  their own variant, all but 34 one of their id (the nearest variant is taken); Swap Force's bottom
  halves show their top's card. Read in about 55 ms at start-up; the HUD portraits stay as the
  fallback for an id without a card. Checked by script: every tab (Spyro's Adventure, Swap Force,
  Trap Team, Magische Objekte) shows the cards, upright.
- **The game's fonts in the portal menu** (src/game/game_fonts.cpp, #3): the text in font_001
  (permanent/bootstrap.bld), the titles in the bold font_002 (permanent/global.bld), both
  igBitmapFont with the layout of the other games' fonts. Every cell has a 4-pixel padding on each
  side that the stored advance includes; measured against the title screen's "Starten", the game
  draws each advance 8 pixels shorter, and so does the menu now (with the stored advance the
  letters stood visibly too far apart).
- **The game's element symbols and gold coin** (src/game/toy_portraits.cpp): the menu drew its
  own; the scene of permanent/global.bld has the UI's, "Levels/Includes/UI_Main/sprites/
  elementalIcons/<element>_64.png" (85x85, Light and Dark among them) and "UI_Main/sprites/stats/
  coin.png", stored upside down like the cards. The portal menu had no key for Light and Dark yet
  (skylanders-portal: "element:light", "element:dark").
- **Trap Team's characters in their element groups:** the portal library knew the elements of the
  first three games' characters only, so Trap Team's tab listed Gusto, Knight Light and the others
  under none. Its table now has Trap Team's, SuperChargers' and Imaginators' characters, read from
  the games' files: every Trap Team character scene names its element ("Elemental_Fire",
  "Portal_EntryFX_Fire.lvl"; checked: all 196 earlier characters these scenes have agree with the
  table), SuperChargers' and Imaginators' toy data have it at +0x0C. Checked by script: the Trap
  Team tab shows Blastermind, Enigma, Cobra Cadabra, Deja Vu and Spry under Magic, Krypt King,
  Short Cut, Bat Spin, Funny Bone and Hijinx under Undead, and the Light and Dark symbols.

## 2026-10-22: session log (night)

- **Hats in the details view** (src/game/toy_hats.cpp, as Giants' menu): the names from permanent/global.bld's "HatName(kkk)" texts in ENGLISH.pak and GERMAN.pak (kkk is the hat number minus one up to 95 and the number itself from 97 on, checked against SuperChargers' hat table: all 245 names agree), the bonuses from misc/StoreItem_Hat_kkk.bld (72 hats), the icons from the .arc next to it (35), the stat symbols from character/UI_StatsScreen_Extra.bld. Checked on a copy of a Chop Chop with the flower hat: "Blumenhut", its icon, "+2 Kritischer Treffer", "+2 Rüstung" with their symbols.
- **Upgrades in the details view** (src/game/toy_upgrades.cpp, ported from Giants, and src/game/game_text.cpp for the scenes' texts): 87 Skylanders from their character archives, the Wow Pow by the figure's year (upgrade 10 + the year), this game's button glyphs ("]" X, "{" Y, "}" B). Checked on the Chop Chop copy: the whole tree with the pictures, the German path names and the bought upgrades ticked. Swap Force's halves don't get theirs yet (#7).
- **Magic items' German names** from rom/item (11 matched to the catalog by their English name; the newer items' archives have no English text to match).
- **Portal library** (83cc22e, 141d32e): the cards say "Fahrzeug", "Sensei", "Falle", "Mini-Skylander", "Serie 2" instead of the catalog's English plurals; a plain hat symbol where a game has no hat picture.
- **New issues** for what the other games' menus have and this one doesn't yet: #5 bloom and motion blur options, #6 item changes waiting for the figure change, #7 the Swap Force halves' upgrades, #8 the game's controller picture.

## 2026-10-22: later the same night

- **The controls page shows the game's controller picture** (level/Title.arc's controls_360.png, the photo of the pad with its light blue lines and their shadow painted over), #8 closed.
- **The text font's button placeholders left out**: its first glyph list has a white circle for most code points its symbol list draws as buttons (& * < = > [ ] ` { }), so every ">" of the settings menu was a dot.
- **Item changes wait for the figure change** (portal_prompt_watch.cpp): the game counts as busy while its player state variable has a state ("Need Toy", then "Clear" during the Skylander's arrival, about 7 s), #6 closed.
- The settings example and the installer's template set skip_intro twice (an invalid file); fixed, and the example says 60 FPS.
- **Settings file read even with a repeated key** (SDK fork 8761248): a key set twice in a table makes the whole TOML file invalid, and the game then ignored every setting in it without a visible error (the error went to a log that didn't exist yet). The SDK now reads such a file with the repeats left out and logs which key was repeated.
- **`--settings_schema=<file>`** (skylanders-portal 4521ced): the game writes its F4 menu's pages with their values and quits before a window opens; the Core of Light launcher shows them as the game's settings and writes into this game's settings file.
- **Figure pictures for the launcher** (skylanders-portal e17786b): while the game runs, every figure picture it reads from the disc goes into `Documents\Skylanders\Pictures\<game>` once, for the launcher's figure library; later the same night (skylanders-portal 6c3c6a3) also the hats it knows (hats.txt, their pictures) and the upgrade trees of the figures in the figure folder; and
(skylanders-portal f571d26) the game's achievement list with the title's own pictures, for the
launcher's achievements screen.

## 2026-10-22: day

- **Motion blur (F4 > Grafik > Bewegungsunschärfe), as in Swap Force, off by default:** the
  exposure time the renderer stores into the motion blur attr (`sub_82304028`, 0x8230427C, the same
  store as Swap Force's) is scaled by the setting. Checked in the first level with the figure
  running and the camera turning: the picture's mean gradient 4.34 / 4.57 at 0, 4.11 / 4.10 at the
  game's 1, 3.98 / 3.76 at 2, the ground and rocks visibly smeared at 1 and 2.
- **Bloom (F4 > Grafik > Leuchten (Bloom)), as in Swap Force (#5):** the level keeps its bloom in
  the environment settings, one object per zone (19 in the first level), _bloomLuminanceCutoff
  at +140 and the inner, outer and diffuse scales after it. `sub_8230E670` copies the four from
  the environment it renders with into the pixel shader constant the scene's draws read; the
  hook after those stores (0x8230EBA4) scales the three amounts. The copy into the renderer's
  CBloomData (`sub_82304CF0`), tried first, doesn't reach the draws (a RenderDoc capture with
  its cutoff forced to 0 still had the level's 1.01 in 288 draws); a scan of guest memory for
  the level's values found the zones. Checked in the first level, the figure standing: the
  picture's mean brightness 98.8 at 0 (clouds and sky without glow), 111.5 at the game's 1,
  126.7 at 3 (strong glow around the clouds and the island).
- **Nothing skipped by default** (the owner's choice, 2026-10-22): `skip_intro` is off, the logo
  movies play as on the console (the option skips them); the settings templates say so too. The
  16x texture filter now comes from skylanders-portal (`ApplyGraphicsDefaults`), so the Core of
  Light launcher shows 16x as the game uses it.

## 2026-10-23: session log (night)

- **Title music from the disc** for the Core of Light launcher (skylanders-portal 6e5c284,
  rexglue-sdk 63e87be): `--export_title_music=<file.wav>` decodes the track the title screen
  streams (found with a run logging the game's file reads) from XMA2 with the SDK's FFmpeg and
  quits before a window opens, in under a second; each normal start leaves `title_music.wav` in
  the game's picture folder once. Checked: 0x03c98809.wav.hz of level/title.arc (52.7 s); a quiet ambience under it left out; identical to FFmpeg 8.1's own decode of the
  track (-63 dB).
- **Leaving the game with the pad:** F4 > Spiel ends with "Spiel beenden" and "Spiel neu
  starten" (both ask, "Nein" first; restart not offered when the launcher started the game; the
  new instance waits for the old one), the player menu is a row on the Portal page, the settings
  menu's buttons have no "Aus" any more (it locked a pad out), "Alle Einstellungen" says it is for
  keyboard and mouse (skylanders-portal 5fff7aa).
- **HD texture packs:** rexglue-sdk 37d3e77 dumps the game's BC1-BC3 textures
  (`texture_dump_dir`) and loads `<hash>.dds` replacements of any size (`texture_replace_dir`);
  a pack in `Documents\Skylanders\Textures\Trap Team` is used by itself (skylanders-portal 94175f0).
  Checked in Swap Force's hub (see SwapForceRecomp docs/texture-packs.md, #62).
- **F4 > Grafik:** "Schärfe-Stärke" (CAS's additional sharpness) and "Farbverläufe glätten
  (Dithering)", dithering on by default.
- **Installer:** its own AppId (all six shared one); `/NOROM=1` installs without the disc for the
  launcher (the settings file is still written); reasons a silent install stops go into its log.
  Silent install from the owner's ISO, start (German title screen), silent uninstall: checked.
- **Who talks from a trap (#10, closed):** a trapped villain's voice comes from the portal
  speaker (PC speakers with the emulated portal); meanwhile the villain's picture from the villain
  vault (misc/ui_villainvault_stream.arc, 46 pictures) shows at the bottom left with a green ring
  pulsing to the voice (skylanders-portal 1fc25fb). Checked by script: a trap with Chompy Mage put
  on in Chapter 1, the game summoned him, the speaker sounded after 5 s (2.2 s) and again a minute
  later, the overlay showed. F4 > Portal: "Portal-Lautsprecher", its volume, "Zeigen, wer
  spricht". Input script: `trap <slot> <villain>`.
- **Portal check on the "too many" screens** ("Es befinden sich zu viele Skylander / Magische
  Objekte auf dem Traptanium Portal"), as in the other five games: the script variable "portal
  state - tag state" holds "too many minions" / "too many items" there (Giants' "portal error
  string" stays 0 in this game), found by comparing the script heap's variables in play and on
  both screens. Checked by script in Chapter 1 with blank figure files: a second Skylander and a
  second magic item each opened the check with the right title and figures, and it closed when the
  figure went off.
- **German names of this game's own magic items and adventure packs** (Schicksalshand,
  Sparschwein, Raketen-Bock, Tiki-Tratschkopf; Mitternachts-Museum, Rätselhafter Spiegel,
  Grusel-Express, Sonnenkratzer) from the game's toy lists in character/Init_Setup.bld and the level
  names in permanent/global.bld; the LightCore packs keep "(LightCore)", a variant the catalog names
  otherwise ("Platinum Treasure Chest") keeps the catalog's name. 33 names, checked in F6 >
  Magische Objekte.

## 2026-10-23: day

- **Weiterspielen into the save played last** (Core of Light's tile, `--continue_game=true`): the
  "Speicherslot wählen" screen always starts on slot 1, checked with saves in slots 1 and 2 (2
  written last). `src/game/continue_slot.cpp` takes the slot whose `Save_Slot_<n>.xsg` was written
  last and, before the continue's A on that screen, moves the cursor there (Right / Down) through
  the library's new continue step. The script variable "selection cur" (the highlighted slot's
  "item info base" object) says when the slot screen is up and when the cursor has moved; found by
  comparing the script heap's variables with the cursor on slot 1, 2, 4 and 2. Checked by script:
  no input, the cursor went to slot 2 and "Spiel fortsetzen" loaded that save. The script variable
  helpers are shared now (`src/game/script_vars.h`, also used by the portal prompt watch).

- **Weiterspielen removed again** (the owner's decision the same evening): blind A presses through
  the menus are too unsafe, and a game is at its title in seconds, where the player picks the right
  save himself. `src/game/continue_slot.cpp` and the library's continue step are gone again.

## 2026-10-24: 0.1.0-beta.7 (a colleague's evening on SteamOS)

- **Ultrawide by itself in fullscreen** (skylanders-portal bd1b318): the ultrawide camera and the
  presenter take the aspect from `resolution`, so "Fenstergröße: Automatisch" stayed 16:9 on a
  21:9 screen. `ResolutionFromScreenIfAutomatic`, first thing in OnPreSetup, sets it for the start
  to the primary screen's mode (nothing written; not for the launcher's schema).
- **Window sizes** add 2560x1080 and 3440x1440 (21:9), 3840x1600 (24:10) and 5120x1440 (32:9).
- **"Tastatur spielt als"** is greyed and can't be changed while "Tastatur und Maus" is off
  (skylanders-portal 10a00cd, `enabled_by`, also in the schema for Core of Light).

## 2026-10-24: 0.1.0-beta.8

- **Later games' versions in the portal menu's version choice** (skylanders-portal 202b4df): Y on a
  Skylander lists its versions from the later games too (Spyro in Spyro's Adventure: Series 2 and
  3, LightCore, Eon's Elite; the game plays them by their id as its own; the owner's wish). The
  tabs stay this game's own; "Figuren späterer Spiele" now only adds the later games' tabs.
- Ultrawide checked in a 2560x1080 window from the title to the first menus in Spyro's Adventure,
  Giants, Swap Force, SuperChargers and Imaginators (menus centred, nothing stretched, movies with
  bars); Trap Team's window couldn't be captured here (black at 16:9 too), so not by picture.

## 2026-10-24: 0.1.0-beta.9

- **One data folder for every game** (the owner's call; skylanders-portal data_folder.h): the saves,
  achievements and shader cache are in `Documents\Skylanders\Trap Team` now, next to Figures and
  Pictures, or in the folder picked in Core of Light (`%APPDATA%\Skylanders\data-folder.txt`). The
  first start copies the old `Documents\trapteamrecomp` there (complete or not at all) and leaves a
  note in it; Core of Light asks before the old folder goes. `--user_data_root` works as before.
- **Keyboard keys changeable in F4** (SwapForceRecomp #63): a "Tastatur" page after "Steuerung", a
  row per controller button; A and then the key. Y also saves with a German keyboard now (the
  menu reads keys by their US place, where that key is Z).
- The portal menu's Portal of Power: a band of light round its side, ridges on the rim, rings of
  light running out over the floor (SwapForceRecomp #57).
- Checked: every game boots to its first menus on fresh scratch data with the new data folder
  code (with --user_data_root, so the copy itself was checked in the launcher on a scratch
  Documents).

## 2026-10-25: 0.2.0-beta.1

- **Traps next to the Skylander** (#11; skylanders-portal 2278b99): a trap goes in a slot of its
  own, one at a time (a new one replaces the old one), and no longer lifts the player's Skylander.
- **The old sidekicks are Minis here** (Core of Light #32): they go on as a player's Skylander.
- Checked: Double Trouble placed, then Air Hourglass: the trap in slot 1, Double Trouble stays.
- **Mouse wheel in the menus** (SwapForceRecomp #65; skylanders-portal ccc7009): F4's rows, the
  portal menu's cards and its variant list scroll a row per notch; the chosen row stays, and the view
  follows it again once the keys or the pad move it.
- **A chosen 4x texture filter stays 4x** (Core of Light #21, a colleague's report and fix;
  skylanders-portal 23be5f2): 4x is the SDK's own default, so it counted as unset and became 16x.

## 2026-10-25 (night): portal requests

- **The trap request opens the menu** (#12): the portal script's player state is "Player State -
  Vault Capture" from "Stecke eine ...-Falle ins Portal" until the trap is on; found in the owner's
  capture with a temporary trace of every script variable about traps, villains and the mode. The
  menu opens on a "Fallen" tab of its own (by element) without lifting the Skylander and puts the
  trap on at once (skylanders-portal AutoOpenPortalMenuForCompanion). Opened with the button it had
  lifted the Skylander: the game switched to "Need Toy", forgot the request, and the trap only
  caught the villain when the game asked again.
- **Traps sorted by element** in the Trap Team tab's trap group, in their element's colour, the
  group named "Fallen" (#13).
- **The "who's talking" circle goes away with the trap** (#14): the speaker counts as quiet once no
  audio came for a quarter second.
- Not checked in a capture yet (needs a beaten villain): the request itself. Checked: the trap
  group's order and colours; the same request path with SuperChargers' vehicle.

## 2026-10-25: 0.2.0-beta.2

- The setup's version for what came since 0.2.0-beta.1: the trap request opens the portal menu on a "Fallen" tab (#12, not yet seen in a real capture), traps by element (#13), the speaker circle goes (#14). Also in every game: the portal overview (Back + Y / F5: what is on the portal, taken off one by one without lifting the
  Skylander) and F4 > Controller (button jobs, vibration strength per player; not yet tried with a
  real controller), skylanders-portal f2c4940 / 6e96c16; an A press right after the menu opened by
  itself no longer picks a figure (SwapForceRecomp #72); one bad command-line option no longer drops
  the others (rexglue-sdk 605000d).

## 2026-10-25: afternoon (after 0.2.0-beta.2)

- **The trap request opens the requested element's traps** (#12): the script variable "trapping
  monster definition" names the villain to catch ("009 - tutorial ally"); its number is the
  villain's id as the traps keep it (9 Gulper, trap 211, Water). Without it after 1.2 s the menu
  opens on the traps as a whole. Not seen in a capture yet.
- **"Needs a break"**: the script variable "portal error string" (as in Giants) with a `304_` key
  opens player 1's menu and marks the Skylander tired. Not measured yet; the key is logged.
- **Enigma's upgrade pictures** (and the other 52 of this game's own characters): they sit under
  `/ui_sky4/upgrades/<id>_<name>/` in the character archive, not under `/upgradeposes/`. Checked:
  Enigma's tree shows its pictures.
- **The portal overview holds the game** while it is open (rexglue-sdk adf2a90, a frame gate in
  VdSwap: the guest waits before handing over its next frame while the menus keep drawing;
  skylanders-portal ab11cbc). It closes when the portal menu opens and after a vehicle is taken
  off there (the game's next request took the Skylander off with the same A, SuperChargersRecomp).
  Its cards stand just above the portal, and the portal's band of light goes all the way round.
- **Tired Skylanders** (skylanders-portal `PlayerSkylanderTired`): when the game says a player's
  Skylander is out of health, the menu opens and that figure gets a red cross until it has been on
  the portal a while again (a new level). It stays pickable, so a wrong guess can't lock a figure.
- **A hat shows only in a game that knows it** (Enigma's SuperChargers hat showed as "Hut Nr. 269").
- **F4 opens on "Spiel"**, which now has the game's **volume** (0-100 %, SwapForceRecomp #68) and
  the **frame rate display** (the SDK's F3 window, Core of Light #28). **F4 > Tastatur** names each
  key as this PC's keyboard prints it ("Ö / Leertaste", "Umschalt+Pfeil hoch") and has "Alle
  Tasten zurücksetzen" (SwapForceRecomp #66). skylanders-portal 218defd.
- The "who's talking" circle sits higher, above the game's health bar.
- **Settings descriptions broken into lines** (SwapForceRecomp #69): one line on what a setting does,
  the choices on lines of their own, notes ("Kostet ...", "Braucht D3D12") after a blank line;
  the same wording in every game where the setting is the same. The Back + Y row is called
  "Portal-Übersicht" now (it opens the portal overview since skylanders-portal f2c4940).
  skylanders-portal 2a97055.
- **Hints in the pad's own buttons** (Core of Light #29, skylanders-portal 39ab210): after a press on a
  PlayStation pad the menus' hints show its symbols and L1 / R1 / Options, on a Nintendo pad its
  letters in their places and L / R / ZL / ZR / +; Xbox-like pads stay as they were.
- **The HUD's dark band** (the owner, 2026-10-25): checked at every resolution scale, native 3D on
  and off, ROV and RTV, windowed and fullscreen: the HUD is drawn straight into the back buffer at
  full size, sharp; the band is the game's own backing panel, whose texture fades only left to
  right, so its top and bottom edges are hard as on the console. A 1-pixel line at its right end
  comes from the HD pack builder (an edge padded by colour only, 37 such edges in this game's pack).

## 2026-10-25: 0.2.0-beta.3

- The setup's version for what came since the last one: the trap request on the wanted element (#12), the upgrade pictures of this game's own characters (Enigma) and "needs a break". Also in every game: the portal overview holds the game while open, a Skylander out of health gets the menu and a red cross, hats only where the game knows them, F4 opens on "Spiel" with volume and frame rate rows, key names as the keyboard prints them and a reset row, descriptions in short lines, hints in a PlayStation or Nintendo pad's own buttons, and figure changes with an HD pack without the stutter (skylanders-portal 39ab210, rexglue-sdk ce9dbb6).

## 2026-10-26

- **Nightmare from the start** (`nightmare_from_start`, F4 > Game, off; SwapForceRecomp #74, as in
  Swap Force and Imaginators): the lock is in a script, the difficulty menu's macro "Menu - 3d -
  Diff - Create" in Title.bld (system flag 3 == 1 sets "allow nightmare"); a scan of all 86 level
  archives found that check only there. A mid-asm hook after the compare in OpCheckValue's execute
  (0x82285CE0, src/hooks/nightmare_hook.cpp) makes exactly that check true. Name pointers in the
  script objects carry a tag bit at runtime (an odd address; the strings start at even ones).
  Checked on empty scratch data: on, the menu has four entries and "Albtraum" can be chosen; off,
  three. The save isn't touched.
- rexglue-sdk 07f1106: no null read in the presenter's vblank thread when the window moves to
  another monitor at start.

## 2026-10-26: 0.2.0-beta.4

- The setup's version for what came since the last one: "Nightmare from the start" (F4 > Game, off by default, the save isn't changed). Also in every game: no crash at start when the window moves to another monitor (rexglue-sdk 07f1106), and the portal menu no longer lifts a Skylander the game is still loading from the portal (skylanders-portal d4ce7b6).

## 2026-10-26: night

- **The portal speaker plays each line to its end** (#15). The game ends a voice once its XMA input is used up and its output read. Our decoder marked the input used up while it still held the last two frames (1024 samples, 128 ms at 8 kHz), so every trapped villain's line lost its end. rexglue-sdk 90f3bad keeps the last input buffer valid until all of its output is out. Measured on Kaos' portal lines: before, the portal stopped 1384 samples before the end of the decoded line; now it plays it to the end (r 0.998 against the decoded line), and the decoded line still matches vgmstream (r 1.0000). Long streams flush the decoder's carry as often as before (about once per 47 000 samples).
- skylanders-portal 8cbf30d: the picture cache also leaves the game's element symbols for the launcher's figure library (SkylandersCoreOfLight #38). Checked: all ten.

## 2026-10-26: 0.2.0-beta.5

- The setup's version for what came since the last one: the portal speaker plays each line to its end (#15). Also in every game: a sound the game stops right at its end keeps its last 1024 samples (rexglue-sdk 90f3bad), and the game leaves its element symbols for the launcher (skylanders-portal 8cbf30d).

## 2026-10-27: night

- rexglue-sdk 5201aa3: an option for the pixel position shaders read at resolution scale 2 and up (each host pixel's centre instead of its corner), off by default and here: this game's title screen lines up with scale 1 as it is (SSARecomp #40).

## 2026-10-27: shadows, start-up

- **Softer shadows and blur at resolution scale 2 and up** (ImaginatorsRecomp #12): skylanders-portal afba1dd sets draw_resolution_scaled_texture_offsets = false unless it was set. The SDK divided texture fetch offsets by the resolution scale, so the games' fixed blur kernels (the shadow mask's 9-tap blur, bloom) shrank with the scale. Compared here at scale 2, on and off: the shadows on the Academy's tree stump soft again instead of stair-stepped. Scale 1 is the same either way.
- **A start-up crash less** (rexglue-sdk 807af87): SDL's audio subsystem starts on the UI thread before the game's audio driver asks for it. Started from the audio thread it could race SDL_UpdateAudio in the UI thread's event pump and end in exception C0000264 (once in Trap Team, 2026-10-27).

## 2026-10-27: 0.2.0-beta.6

- The setup's version for what came since the last one: softer shadows and blur at resolution scale 2 and up (texture fetch offsets in the console's texels, skylanders-portal afba1dd) and SDL's audio started on the UI thread against a rare start-up crash (rexglue-sdk 807af87).

## 2026-10-27: the villain vault's trap element

- **The portal menu opens on the right element at the villain vault** (#12): the vault's `trapping monster definition` is 1000 plus the villain (`1020 - sheep` for Sheep Creep, 20; the tutorial's `009 - tutorial ally` has none), so the three-digit reading found nothing and the menu opened on all traps. Now four digits, the thousands dropped. Checked on the owner's route (a copied save, the Academy, straight to the vault, RT, Sheep Creep): `the trap is for "1020 - sheep" (Sheep Creep): Life trap`, the menu on the Life traps. The value is logged when it changes.

## 2026-10-27: 0.2.0-beta.7

- The setup's version for what came since the last one: the portal menu opens on the villain's element at the villain vault (#12).

## 2026-10-27: shared libraries

- SDK 7be1eb2: two upstream fixes (kernel objects freed again, saved-game results checked properly) and resolved memory read back on request.

## 2026-10-27: shared libraries

- SDK 642311f: menus over a held game (the portal overview) present at the frame cap instead of every 100 ms: no more 10 FPS stutter in the overview.

## 2026-10-27: 0.3.0-beta.1

- The setup's version for what came since the last one: one version for all six games from now on; the portal overview (Back + Y, F5) runs smoothly while it holds the game (rexglue-sdk 642311f), and two upstream SDK fixes: kernel objects freed again, saved-game results checked properly (rexglue-sdk 4a34314, 3cb725a).

## 2026-10-28: shared libraries

- SDK cf59726: the XMA "no bits to copy" line is debug only now; library 433cb9f: the crash report also logs where the SDK's fatal errors happen (SSARecomp #42), and developer tools (pollwatch, the write watch works in Trap Team).

## 2026-10-28: shared libraries

- SDK 88d6730: cheaper register writes on the GPU thread (Spyro's Adventure Chapter 1: 51-55 -> 60 FPS at 1080p, frame cap off).

## 2026-10-28: 0.3.0-beta.2

- The setup's version for what came since the last one: the GPU thread writes registers more cheaply (rexglue-sdk 88d6730: Spyro's Adventure's Chapter 1 51-55 -> 60 FPS at 1080p), the crash report also says where the SDK's fatal errors happen (skylanders-portal 433cb9f, SSARecomp #42), and the XMA "no bits to copy" line is debug only (rexglue-sdk 7d147fe).

## 2026-10-28: 0.3.0-beta.3

- The setup's version for what came since the last one: setjmp/longjmp are wired up for codegen, so an error the game catches no longer runs into exit() and a crash (SSARecomp #42, Oilspill Island)

## 2026-10-28: 0.3.0-beta.4

- The setup's version for what came since the last one: an XMA sound stream stuck past the end of its buffer goes on (ImaginatorsRecomp #16), who a trap holds in the portal menu (TrapTeamRecomp #17), the log's Where lines (Spyro's Adventure)

## 2026-10-29: shared libraries

- SDK 86a0e8e: the audio queue follows the device's buffer, a warning when sound runs dry

## 2026-10-29: 0.3.0-beta.5

- The setup's version for what came since the last one: 0.3.0-beta.5: the audio queue follows the device's buffer

## 2026-10-29: shared libraries

- SDK 6430f40: the sound-ran-dry warning from 10 a minute on

## 2026-10-29: 0.3.0-beta.6

- The setup's version for what came since the last one: 0.3.0-beta.6: the log's "Where:" line (level and player 1's place) in Swap Force, SuperChargers and Imaginators; the SDK tells the game's code which files it opens

## 2026-10-30: shared libraries

- portable data next to Core of Light (#47); the portal menu says which Skylander the game wants back

## 2026-10-30: Nightmare from the start, no slowdown (#16)

- With "Nightmare from the start" on, the game ran at about 5 FPS and its sound stuttered: the hook looked at every script check that came out false again, a dozen memory queries each. Now each check is looked at once (kept by its address and its two sides); 60 FPS with the option on and off, the difficulty menu still has Nightmare.

## 2026-10-30: shared libraries

- The log keeps your user name, user folder and PC name out, and its first line names the game's version and build and the machine; less work per draw on the graphics thread.

## 2026-10-30: 0.3.0-beta.7

- The setup's version for what came since the last one: Nightmare from the start no longer slows the game down (#16); smoother where the graphics thread is the limit, no user name, user folder or PC name in the log, and its first line names the version, the build and the machine

## 2026-10-31: shared libraries

- Smooth shadow edges at resolution scale 2; the Discord status's text in the log

## 2026-10-31: 0.3.0-beta.8

- The setup's version for what came since the last one: a Discord status (off unless you switch it on, F4 > Game), smooth shadow edges at resolution scale 2 and above, a little less work per draw

## 2026-10-31: shared libraries

- Water ripples where a Skylander wades are smooth at resolution scale 2 and above (they were grainy)

## 2026-10-31: shared libraries

- Files the game overwrites are cut short in place instead of deleted and made anew: safer on exFAT/FAT drives such as USB sticks (portable data).

## 2026-10-31: shared libraries

- The FidelityFX library is built into each build folder (a test build's could end up in another one).

## 2026-10-31: 0.3.0-beta.9

- The setup's version for what came since the last one: files overwritten in place (safer on USB sticks), a profile-guided build (faster where the CPU is the limit)

## 2026-10-31: Other pressings

- The disc check also compares the code: another pressing with the same code (another language set) runs too, and a refused disc's message names its title, region and hash.

## 2026-10-31: 0.4.0-beta.1

- The setup's version for what came since the last one: Another pressing of the disc with the same code (another European language set) runs too, and the setup's message for a refused disc names its title, region and hash.

## 2026-11-01: 0.4.0-beta.2

- The setup's version for what came since the last one: 0.4.0-beta.2: level music layers switch again (SC/SF/IM), setups extract ISOs without the VC++ runtime, Extras page, menus smooth over loading, pipeline and AMD fixes

## 2026-11-01: shared libraries

- SDK: outdated texture bindings reload only when used, unchanged register writes skipped

## 2026-11-01: villain time only on hits (#18)

- **Villain time only drops on hits** (`cheat_villain_time_hits_only`, F4 > Extras, off; a cheat, so
  achievements are held while it's on). Villain time is a script, not code: "Villain Timer
  Internal - Update" in the global Sky_Allies.ai, read from a heap snapshot of the Academy by
  decoding its ops (types, names, skip counts). While a villain plays it subtracts the frame time
  from `control.timer value`; hits subtract `damage / 100 * timer - reduction per 100 hp of damage`
  in "Villain - On Hurt Timer Reduction"; the update is skipped where the levels turn on
  "Villain - Timer - Set Timer Unlimited Mode" (`timer update disabled`: the Academy, Level_302's
  ride down the dune, where the bar still falls from hits) and in challenges. A mid-asm hook on the scripts' float subtraction
  (0x82288160 in sub_822880B0, now src/hooks/script_cheats_hook.cpp) subtracts 0 for exactly the
  expression "control.timer value - time elapsed this frame" (the only one of the 536 loaded
  subtractions with that right side), so hits, the tag-out at 0, the recharge and the bar work as
  before. The script readers the Nightmare hook had are shared in src/hooks/script_objects.h.
  Tested in the Academy with a test build that forced the unlimited mode off: without the option
  the time fell every frame (the bar at a third after 46 s), with it the bar stayed full for a
  minute of walking.

## 2026-11-01: 0.4.0-beta.3

- The setup's version for what came since the last one: A bit faster renderer (unchanged shader constants skipped, only changed textures reloaded); Trap Team also gets the villain time cheat (#18)

## 2026-11-02: no health loss

- **No health loss** (`cheat_no_health_loss`, F4 > Extras, off; the same setting name as Spyro's
  Adventure's, a cheat). Health is a script value too: "On Hurt - Player On Hurt" does
  `creature info temp.hp = creature info temp.hp - damage` (two ints), then beats the creature
  below 0. src/hooks/script_cheats_hook.cpp (the villain time hook, renamed, now on all three paths
  of the scripts' subtraction: floats, a float minus an int, two ints) subtracts 0 there, but only
  for the player's creature: the macro serves every creature (the enemies' hits came through it in a
  test). The creature is told by its own "hp" variable, which sub_822877A8 gets as the assignment's
  target (r5, noted by a hook at 0x822877B4); the player's is learned from the HUD's bar ("UI - Health
  Bar - Update" takes the same hit off its own copy right after, for the player's bar only), so the
  first hit after a Skylander comes in still counts. Those bar updates are skipped too. Tested in Level_302 with Enigma among the
  first enemies: his first hit landed, the next eight were kept off (the bar stayed at 349 of 379)
  and all his hits on the enemies landed. Found by decoding the scripts from a heap snapshot after
  twelve runs of searching memory for the value missed it.

## 2026-11-03: shared libraries

- Xbox 360 portal over WinUSB: each read can hold two 32-byte frames (the portal sends full packets), each now goes to the game; the first 12 commands are logged (TrapTeamRecomp #19)

## 2026-11-03: 0.4.0-beta.6

- The setup's version for what came since the last one: Xbox 360 portal: every frame of a WinUSB read reaches the game (TrapTeamRecomp #19)

## 2026-11-03: shared libraries

- Quick swap: portal_quick_swap_buttons (Back+X) / bind_portal_quick_swap (F2) put the newest Recent entry not on the portal back on without the menu; Xbox 360 portal commands padded to 32 bytes, pipes reset on open (TrapTeamRecomp #19)

## 2026-11-03: 0.4.0-beta.7

- The setup's version for what came since the last one: Quick swap (Back+X / F2) and the Xbox 360 portal's third try (TrapTeamRecomp #19)

## 2026-11-03: shared libraries

- Xbox 360 portal: overlapped reads and writes, the first 12 commands' write results logged (TrapTeamRecomp #19)

## 2026-11-03: 0.4.0-beta.8

- The setup's version for what came since the last one: Xbox 360 portal fourth try (TrapTeamRecomp #19)

## 2026-11-03: Traptanium boost always on (TrapTeamRecomp #20)

- F4 > Extras > Traptanium boost always on (`cheat_traptanium_boost`, off, a cheat). The boost is the Trap
  Masters' "Guardian Crystal powerup" (S4_GuardianCrystalPowerup_macros.ai, loaded with each Trap
  Master): "Minion - Guardian Crystal Power - Manage" grows and lights the weapon and turns on
  "Trapper - Boosted Damage Mult" only while the powerup's master switch is on, and level scripts set
  that switch through "Minion - Guardian Crystal Power - Switch". src/hooks/nightmare_hook.cpp (the
  scripts' check-value hook at 0x82285CE0) reads the checks of "guardian crystal powerup data
  ref.master switch" against 0 as if it were on, classified once per op.
- Tested in Level_302 with Enigma (the developer kit's level command; routes in the kit's notes):
  standing still his orb glows white and bigger with it, dark purple without; his hits on the enemies
  30 / 66 / 138 without it, 45 / 99 / 207 with it (1.5 times). Superball and the Rage Mage orb,
  the issue's other two, are still open.

## 2026-11-03: 0.4.0-beta.9

- The setup's version for what came since the last one: Trap Team: Traptanium boost always on (Extras cheat, TrapTeamRecomp #20)

## 2026-11-03: shared libraries

- Xbox 360 portal: every endpoint logged at open, every non-status reply logged (TrapTeamRecomp #19)

## 2026-11-03: 0.4.1-beta.1

- The setup's version for what came since the last one: Xbox 360 portal diagnostics (TrapTeamRecomp #19); Trap Team's Traptanium boost cheat reaches the others' bundle

## 2026-11-03: shared libraries

- The Xbox 360 portal gets the game's frames as they are again, no longer padded to 32 bytes (TrapTeamRecomp #19); Extras > Full gold, shared (SkylandersCoreOfLight #48).

## 2026-11-03: 0.4.1-beta.2

- The setup's version for what came since the last one: the Xbox 360 portal gets the game's frames as they are (TrapTeamRecomp #19), Extras > Full gold (SkylandersCoreOfLight #48).

## 2026-11-03: shared libraries

- The command processor checks the shared memory's valid pages before taking a lock every draw asked for: Spyro's Adventure Level 6 52.8-56.8 -> 59.6-62.8 FPS uncapped, Level 36 +4 %.

## 2026-11-03: 0.4.1-beta.3

- The setup's version for what came since the last one: the graphics command thread no longer waits for a lock on almost every draw (up to 12 % more FPS where the CPU is the limit).

## 2026-11-03: shared libraries

- GPU commands are replayed and submitted on a thread of their own (SDK ec3bdbf): Spyro's Adventure Level 36 about +6 % FPS where the CPU is the limit

## 2026-11-03: shared libraries

- Direct resolve (SDK 45280bf): most copies from the emulated render memory skip the dump, +3-4 % FPS at scale 2; Extras > Show cheats (lib f4e72e1)

## 2026-11-03: shared libraries

- Render target updates of unchanged draws skip the lookups and EDRAM ownership walks (SDK ef9c6f9): Spyro's Adventure Level 36 about +4 % FPS where the CPU is the limit

## 2026-11-03: 0.4.1-beta.6

- The setup's version for what came since the last one: Less CPU work per draw: render target updates of unchanged draws are skipped (about +4 % FPS where the CPU is the limit)

## 2026-11-04: shared libraries

- Draws wait up to 4 ms per frame for a shader pipeline still being created instead of being skipped (SDK 9e359ad): far fewer objects missing for a frame the first time they show

## 2026-11-04: 0.4.1-beta.7

- The setup's version for what came since the last one: Fewer objects missing for a moment the first time they show (draws wait briefly for new shaders)

## 2026-11-04: shared libraries

- Full-screen passes that overwrite whole tile rows no longer get the old EDRAM contents transferred in first (SDK 36d9239): about 1 % more FPS at scale 2 in Trap Team

## 2026-11-04: 0.4.1-beta.8

- The setup's version for what came since the last one: Less copying before full-screen passes (about 1 % more FPS at 2x in Trap Team)

## 2026-11-04: shared libraries

- Overwritten-transfer skip also for full-screen triangles drawn with culling on (SDK 463a021): SuperChargers +1.5 %, Imaginators +0.7 % at scale 2; the portal menu says why the Imaginators tab is empty (lib 6424139, ImaginatorsRecomp #17)

## 2026-11-04: 0.4.1-beta.9

- The setup's version for what came since the last one: Less copying before full-screen passes in SuperChargers and Imaginators too; the portal menu explains the missing Imaginators toys

## 2026-11-04: shared libraries

- Overwritten-transfer skip also for full-screen rectangle lists (SDK c7d8aba): with the skip on vs off at scale 2, Swap Force +2.8 %, Trap Team +1.8 %

## 2026-11-04: 0.4.2-beta.1

- The setup's version for what came since the last one: Less copying before full-screen passes drawn as rectangles (Swap Force, Trap Team)

## 2026-11-04: shared libraries

- No wait at the swap for the submit thread (SDK 00bf733): Spyro's Adventure l036 61.6 -> 68.5 FPS where the CPU is the limit

## 2026-11-04: 0.4.2-beta.2

- The setup's version for what came since the last one: No more waiting at the end of each frame (Spyro's Adventure about 11 % faster where the CPU is the limit)

## 2026-11-04: shared libraries

- Less CPU work per frame (SDK 0ddcfc4: logger asked once per primary buffer, texture extents cached at a scale): Spyro's Adventure l036 68.2 -> 70.8 FPS

## 2026-11-04: 0.4.2-beta.3

- The setup's version for what came since the last one: A bit less CPU work per frame (Spyro's Adventure about 4 % faster where the CPU is the limit)

## 2026-11-05: shared libraries

- Far fewer copies to the graphics card (SDK fa164f6, clear_memory_page_state off as in Xenia): hubs at scale 2 +9 to +37 %, SuperChargers GPU-bound; Imaginators: every crystal and chest file in the portal menu (portal feee2f5, ImaginatorsRecomp #18)

## 2026-11-05: 0.4.2-beta.4

- The setup's version for what came since the last one: Far fewer copies to the graphics card (the hubs 9 to 37 % faster where the CPU is the limit); Imaginators: every crystal and chest file in the portal menu (#18)

## 2026-11-05: shared libraries

- Less CPU work per draw (SDK a0fa267: render state register blocks stored in one go, the scaled resolve answer kept per texture): Spyro's Adventure Level_036 80.8 -> 84.3, Level_006 91.1 -> 94.8 FPS at scale 2; the dev kit's shot <name> full (portal 0599229)

## 2026-11-05: 0.4.2-beta.5

- The setup's version for what came since the last one: Less CPU work per draw (Spyro's Adventure about 4 % faster where the CPU is the limit); the setup starts at 1x on weak graphics cards

## 2026-11-06: shared libraries

- Fewer copies into depth buffers at higher resolutions (SDK 20dd8d1, transfer_skip_overwritten_depth): hubs at scale 2 Trap Team 109.7 -> 112.1, Swap Force 115.8 -> 117.4, Imaginators 122.0 -> 123.3 FPS; the shared Extras rows can be cheats with options (gold and XP multipliers, portal 7be41de)

## 2026-11-06: 0.4.2-beta.6

- The setup's version for what came since the last one: Fewer copies into depth buffers at higher resolutions (Trap Team about 2 % faster at 2x); Swap Force, SuperChargers and Imaginators: always crit, elemental power everywhere, gold and XP multipliers as cheats

## 2026-11-06: always crit and elemental power

- Extras: always crit and elemental power everywhere, behind Show cheats (CoL #48, #20; Swap Force, SuperChargers and Imaginators have them since beta.6). Trap Team rolls both in the script macro "On Hurt - Player On Hurt" (heap snapshot of Level_302): its parameter "critical hit" comes in as -1; only for a player's hit the macro checks `< 0` and sets it to the hurter's stats.total crit, then `<= 0` means no crit, and after `critical hit - 100` with the roll `> 0` means damage x1.5; the element bonus (damage * total elemental strength * 0.01) needs the global ZoneElement_P1_Correct / P2_Correct `== 1`, whose only checks are these two. src/hooks/attribute_cheats_hook.cpp classifies the check ops once (cheap tests on the op first, the names last) and changes their results from NightmareCheckHook (0x82285CE0): the `< 0` check marks a player's hit, its `<= 0` comes out false and its `> 0` true; the zone checks true. The compare sub_822878C0 decoded: operators 0 <=, 1 ==, 2 >=, 3 <, 4 >, 5 !=. Developer kit runs in Level_302 with Enigma (TEMP DIAG of the checks): without the cheats 5 crits in 34 hits, 30 damage each; with both every roll won, 30 -> 45 -> 56 and 200 -> 300 -> 375, the enemies' hits on him (38, 80) unchanged. The first version classified every check with a dozen memory protection queries and made a 70 ms hitch in the hub's samples; now hub A/B at scale 2 112.2 off / 112.2 on, no long frames.

## 2026-11-06: shared libraries

- Fewer copies before narrow full-screen passes at higher resolutions (SDK b23e3f8, transfer_skip_overwritten_partial): hubs at scale 2 Swap Force 119.4 -> 120.4, Trap Team 112.1 -> 112.5, Imaginators 123.2 -> 123.9 FPS; the RenderDoc dumps tool prints dispatch sizes (portal ffe4030)

## 2026-11-06: 0.4.2-beta.7

- The setup's version for what came since the last one: Fewer copies before narrow full-screen passes at higher resolutions (Swap Force about 1 % faster at 2x); Trap Team: always crit and elemental power everywhere as cheats

## 2026-11-06: gold and XP multipliers

- Extras: gold and XP multipliers (x2 to x10, the rows from skylanders-portal, behind Show cheats; CoL #48). Trap Team pays out both in script macros (heap snapshot of Level_302): "Player - Minion - Award Money" reads the player's money into val, then `amount = val + amount` (capped at 65000 by the game) and saves it; "Player - Minion - Award Experience" takes `amount = exp value * amount` and adds it. The scripts' addition sub_82287DB0 and multiplication sub_822883B0 are built as the subtraction (the expression in r30, three paths by the values' types); src/hooks/multiplier_cheats_hook.cpp hooks all six, tells the two expressions by their names (the right side the macro's own parameter "amount", its macro +0x38; the left side "val" / "exp value", so the macro's statistics additions of amount stay as they are), keeps each answer in a direct-mapped table and scales the right value when it is positive. Developer kit at x10: Level_302's fight 'XP ... (1 -> 10)', a sweep of Level_300 and Level_305 'gold ... (1 -> 10)'; hub A/B at scale 2, 3 pairs: on 122.4, off 122.3, the pairs split, no frame over 50 ms.

## 2026-11-06: 0.4.2-beta.8

- The setup's version for what came since the last one: Trap Team: gold and XP multipliers as cheats

## 2026-11-06: shared libraries

- Settings are read again when the Windows user name has an accent or umlaut (SDK 62929a0); a real portal's traffic goes into the log (lib d48858a).

## 2026-11-06: 0.4.2-beta.9

- The setup's version for what came since the last one: Settings are read again with an accent or umlaut in the Windows user name; portal traffic in the log.

## 2026-11-06: shared libraries

- Any movie can be skipped with a button (Start; F4 > Game), and the settings texts read better.

## 2026-11-07: 0.5.0-beta.1

- The setup's version for what came since the last one: Any movie can be skipped with a button (Start; F4 > Game); clearer settings texts.

## 2026-11-07: shared libraries

- Raising the frame rate in F4 takes effect at once (it needed a restart).

## 2026-11-07: shared libraries

- Portal menu, Recent tab: R3 pins a figure on top, L3 removes it (keyboard F and Delete).

## 2026-11-07: 0.5.0-beta.2

- The setup's version for what came since the last one: Raising the frame rate in F4 takes effect at once; Recent tab: pin and remove figures.

## 2026-11-07: shared libraries

- 2026-11-07: release bundle: frame rate in F4, Recent pins, Enter no longer opens the pause menu, DS4Windows twin left out, idle cursor, Discord on by default.

## 2026-11-08: shared libraries

- 2026-11-08: SDK 3395338: HD texture files load in the background, Intel Arc takes the faster render target path.

## 2026-11-08: shared libraries

- The portal fixes from Core of Light #58 and #60: a portal on the WinUSB driver (set up for Dolphin or Cemu) is found, the portal menu says when no real portal is found, F4 > Portal shows the mode when Core of Light started the game, and TheBiemGamer's six portal bugs (figure names outside the code page, dumps, replugging, one file in one slot, failed saves, swaps). skylanders-portal 6f272ee.

## 2026-11-08: shared libraries

- Hybrid mode (Automatic) with a real portal (Core of Light #62, TheBiemGamer): a portal that misses the activate right after it is plugged in is set up again, a stale status no longer starts a read too early, Trap Team's J and L lights reach the real portal, and a toy whose backup file is already on the portal through the menu is left out with a message. skylanders-portal 214aed4.

## 2026-11-09: graphics

- More FPS where the graphics card is the limit: the console's 4x clears run at the sample count of the picture they clear, and a resolve copies its result straight into the textures that view it (6 to 13 % at 3x resolution). gpu_msaa_fill_at_1x = false and gpu_native_resolves = false go back. SDK 4c8f3b3.

## 2026-11-09: 0.5.0-beta.4

- The setup's version for what came since the last one: a real portal used by itself (Automatic), the Xbox 360 portal and portals on the WinUSB driver found, TheBiemGamer's portal fixes (Core of Light #60, #62), and more FPS where the graphics card is the limit (4x clears at their picture's sample count, resolves straight into the textures).

## 2026-11-09: shared libraries

- Sound output setting: Stereo for headphones on a 5.1/7.1 output (movie voices on the center channel were lost, Core of Light #61)

## 2026-11-11: shared libraries

- SDK 9e32a59: the resolution scale threshold, direct resolves from depth, integer and full-format colour targets, lazy resolve copies on by default (also at 1x). skylanders-portal 5bbc6f3: missing language folder check (Core of Light #64), portal_audio = pc (#67), older portals light up in the newer games, quicker swaps and the menu lock with a real portal (TrapTeamRecomp #22), the Monitor row (#75).

## 2026-11-11: 0.5.0-beta.5

- The setup's version for what came since the last one: more FPS where the graphics card is the limit (small render targets at 1x, direct resolves, lazy copies, native surfaces), the native 3D picture and Automatic sharpening (FSR 1 when enlarging) by default, As on the console in the presets (Core of Light #72), the Monitor row (#75), older portals light up, quicker swaps and the menu lock with a real portal (TrapTeamRecomp #22), Nederlands (#68), Portal speaker: PC speakers (#67).
