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
- **Portal library** (cbc71c6, 0eb2564): the cards say "Fahrzeug", "Sensei", "Falle", "Mini-Skylander", "Serie 2" instead of the catalog's English plurals; a plain hat symbol where a game has no hat picture.
- **New issues** for what the other games' menus have and this one doesn't yet: #5 bloom and motion blur options, #6 item changes waiting for the figure change, #7 the Swap Force halves' upgrades, #8 the game's controller picture.

## 2026-10-22: later the same night

- **The controls page shows the game's controller picture** (level/Title.arc's controls_360.png, the photo of the pad with its light blue lines and their shadow painted over), #8 closed.
- **The text font's button placeholders left out**: its first glyph list has a white circle for most code points its symbol list draws as buttons (& * < = > [ ] ` { }), so every ">" of the settings menu was a dot.
- **Item changes wait for the figure change** (portal_prompt_watch.cpp): the game counts as busy while its player state variable has a state ("Need Toy", then "Clear" during the Skylander's arrival, about 7 s), #6 closed.
- The settings example and the installer's template set skip_intro twice (an invalid file); fixed, and the example says 60 FPS.
- **Settings file read even with a repeated key** (SDK fork 96273bb): a key set twice in a table makes the whole TOML file invalid, and the game then ignored every setting in it without a visible error (the error went to a log that didn't exist yet). The SDK now reads such a file with the repeats left out and logs which key was repeated.
- **`--settings_schema=<file>`** (skylanders-portal 25dec20): the game writes its F4 menu's pages with their values and quits before a window opens; the Core of Light launcher shows them as the game's settings and writes into this game's settings file.
- **Figure pictures for the launcher** (skylanders-portal e87feb3): while the game runs, every figure picture it reads from the disc goes into `Documents\Skylanders\Pictures\<game>` once, for the launcher's figure library; later the same night (skylanders-portal 7f6a34a) also the hats it knows (hats.txt, their pictures) and the upgrade trees of the figures in the figure folder; and
(skylanders-portal ca65339) the game's achievement list with the title's own pictures, for the
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

- **Title music from the disc** for the Core of Light launcher (skylanders-portal 7bd504a,
  rexglue-sdk 1e74678): `--export_title_music=<file.wav>` decodes the track the title screen
  streams (found with a run logging the game's file reads) from XMA2 with the SDK's FFmpeg and
  quits before a window opens, in under a second; each normal start leaves `title_music.wav` in
  the game's picture folder once. Checked: 0x03c98809.wav.hz of level/title.arc (52.7 s); a quiet ambience under it left out; identical to FFmpeg 8.1's own decode of the
  track (-63 dB).
- **Leaving the game with the pad:** F4 > Spiel ends with "Spiel beenden" and "Spiel neu
  starten" (both ask, "Nein" first; restart not offered when the launcher started the game; the
  new instance waits for the old one), the player menu is a row on the Portal page, the settings
  menu's buttons have no "Aus" any more (it locked a pad out), "Alle Einstellungen" says it is for
  keyboard and mouse (skylanders-portal 60e8eeb).
- **HD texture packs:** rexglue-sdk f02a4b5 dumps the game's BC1-BC3 textures
  (`texture_dump_dir`) and loads `<hash>.dds` replacements of any size (`texture_replace_dir`);
  a pack in `Documents\Skylanders\Textures\Trap Team` is used by itself (skylanders-portal ebb5e85).
  Checked in Swap Force's hub (see SwapForceRecomp docs/texture-packs.md, #62).
- **F4 > Grafik:** "Schärfe-Stärke" (CAS's additional sharpness) and "Farbverläufe glätten
  (Dithering)", dithering on by default.
- **Installer:** its own AppId (all six shared one); `/NOROM=1` installs without the disc for the
  launcher (the settings file is still written); reasons a silent install stops go into its log.
  Silent install from the owner's ISO, start (German title screen), silent uninstall: checked.
- **Who talks from a trap (#10, closed):** a trapped villain's voice comes from the portal
  speaker (PC speakers with the emulated portal); meanwhile the villain's picture from the villain
  vault (misc/ui_villainvault_stream.arc, 46 pictures) shows at the bottom left with a green ring
  pulsing to the voice (skylanders-portal 52b0fe0). Checked by script: a trap with Chompy Mage put
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
