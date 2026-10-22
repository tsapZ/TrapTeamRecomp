# TrapTeamRecomp

An unofficial native Windows port of **Skylanders: Trap Team** (2014, Xbox 360), made with
static recompilation (ReXGlue). Work in progress: Chapter 1 plays with a figure on the emulated portal.

![Skylanders: Trap Team in play](docs/images/play.jpg)

> **Unofficial fan project.** Not affiliated with or endorsed by Activision or Toys for Bob. This
> repository contains **no game data**: you need your own copy of the game.

## Status

| Area | State |
|---|---|
| Boot, menus | ✅ logo movies (skippable), title screen, menus |
| Chapter 1 | ✅ with a figure on the emulated portal |
| Rest of the game | 🟡 not played through yet |
| Frame rate | ✅ 60 FPS, 144 and more |
| Portal | ✅ emulated; traps and the Traptanium portal's speaker still to do |
| Portal menu | ✅ opens by itself at the prompt, with the game's collection cards and fonts, hats and upgrade trees |
| Settings | ✅ native 3D picture, the game's FXAA at the internal resolution, bloom and motion blur strength, the controller picture |
| Ultrawide | ✅ the 3D view and the HUD at the edges |
| Installer tested on a clean PC | ⬜ planned |

Where each part stands and what comes next: [docs/roadmap.md](docs/roadmap.md); session details and
known bugs: [docs/STATUS.md](docs/STATUS.md). Part of six ports of the Xbox 360 Skylanders games,
started together from [Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight).

## What it adds

- **Native Windows program**: the game's Xbox 360 code recompiled to x86-64 (no emulator of the
  CPU), the GPU through Xenia's D3D12 path.
- **Picture**: internal resolution 1x, 2x or 3x of the console's 720p, sharpened with AMD CAS,
  16x texture filtering, presets *Performance*, *Quality* (2x, recommended) and *Best*. The game's
  own 3D picture at its native size instead of the console's smaller one is an experimental
  option.
- **60 FPS** (what the installer sets); 30 as on the console, or 144, 200 and more.
- **Ultrawide**: a wider window shows more to the sides, with the HUD at the window's edges.
- **The Portal of Power** emulated (figures as `.dump` files), a real USB portal, or both at once.
  The **portal menu** (Guide, Back + Start or F6) is drawn in the game's own fonts with its own
  pictures of every figure, their hats and upgrade trees, per player, for controller and keyboard.
- **A settings menu** in the same style (F4 or Back + RB): graphics, frame rate, language, portal,
  controls with the game's controller picture.
- **Achievements**, shown in the portal menu and in the Core of Light launcher.
- English and German menus; French, Spanish and Italian are offered too.

| The portal menu | A figure's upgrades | The settings menu (F4) |
|---|---|---|
| ![The portal menu](docs/images/portal-menu.jpg) | ![A figure's upgrades](docs/images/details.jpg) | ![The settings menu (F4)](docs/images/settings.jpg) |

## Which disc

The European Xbox 360 disc (PAL, title `41560911`). Its SHA-256 is pinned in
[docs/game/default.xex.sha256](docs/game/default.xex.sha256): the game, the installer and the launcher
check it. Other regions have another `default.xex` and are refused at start.

## Installing

- **Installer (beta):** `just package-installer` builds `installer/Output/TrapTeamRecompSetup-<version>.exe`
  (Inno Setup; there are no prebuilt downloads yet). It installs into your user folder
  (`%LOCALAPPDATA%\Programs\Trap Team Recompiled`, no admin rights) and asks for your disc, as an ISO or an
  already extracted folder (it checks the version, then copies the files next to the game), the
  game language, the portal (emulated, real USB or none), the window size and the internal
  resolution. Everything else starts at the settings above (60 FPS, sharpening, 16x filtering) and is changed later in the F4 menu.
- **[Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight)**, the launcher
  for all six ports, finds the installed game, sets up its disc from your ISO, keeps profiles with
  their own saves and figures and starts the game.
- **From source:** build it (below), put the extracted disc into `rom/` ([rom/README.md](rom/README.md))
  and run `just play-release`.

Your figures are `.dump` files in `Documents\Skylanders\Figures`, shared by every game, like real
figures next to the portal. The saves are in `Documents\trapteamrecomp`, the settings file next to the
game's exe.

## Building

The same as the other Skylanders ports: see [docs/development.md](docs/development.md).

## Credits

- **[GiantsRecomp](https://github.com/tsapZ/GiantsRecomp)**, **[SSARecomp](https://github.com/tsapZ/SSARecomp)**
  and **[SwapForceRecomp](https://github.com/tsapZ/SwapForceRecomp)**: this project is scaffolded
  from them; app, portal, menus and tooling come from there.
- **[GiantsRecomp by TheBiemGamer](https://github.com/TheBiemGamer/GiantsRecomp)** brought
  Skylanders Giants to PC first and is the template these ports started from.
- **[ReXGlue](https://github.com/rexglue/rexglue-sdk)**, the static recompilation SDK, here in a
  fork with our fixes on top.
- **[extract-xiso](https://github.com/XboxDev/extract-xiso)** is included (prebuilt, unmodified)
  for the installer (`installer/extract-xiso.LICENSE.txt`).

Skylanders is a trademark of Activision. This project is not affiliated with or endorsed by
Activision.
