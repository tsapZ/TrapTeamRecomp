# Portal

The Portal of Power comes from the portal library (`thirdparty/skylanders-portal`, our fork's
branch `feature/portal-menu`), the same code in all six ports. Its protocol, the figure files and
the menus are described in SwapForceRecomp's
[portal.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/portal.md); this page covers
what is particular to Trap Team.

## Modes

`portal_mode` (F4 > Portal):

| Value | What |
|---|---|
| `software` | the emulated portal, filled from the portal menu (default) |
| `hybrid` | the emulated portal and the real USB portal together |
| `usb` | the real USB portal only |
| `none` | no portal |

The portal menu lists the figures of Spyro's Adventure, Giants, Swap Force and Trap Team (`portal_menu_games`).

## Figures

`Documents\Skylanders\Figures`, one folder for every game (portal fork `SharedFiguresDir`), with a
subfolder per game. Test runs with a copied `--user_data_root` get their own folder next to the
copy.

## The menu at the game's portal screens

This game runs its portal screens from a game script, as Giants does (GiantsRecomp
`src/game/portal_prompt_watch.cpp`), and `src/game/portal_prompt_watch.cpp` reads two of the
script's variables, ScriptReference objects (vtable 0x821095A8, +8 the name, +0x1C the value),
found by a scan of the physical heap and polled every 30 ms:

- **"player state macro":** exists while the portal UI runs a player's state machine; its state is
  `Player State - Need Toy` while the game waits for a Skylander (the menu opens for the first
  player without one on the emulated portal: the variable doesn't say whose prompt it is) and
  `Player State - Clear` while a figure arrives (about 7 s; the game counts as busy then, for the
  portal menu's magic items).
- **"portal state - tag state":** 0 in play; on a "too many" screen the tag state, an object named
  after it: `too many minions` (a second Skylander of one player) and `too many items` (a second
  magic item), both measured; the script also has `too many traps`, `too many tags`, `tag error`,
  `tag not usable` and `data mismatch`, which open the check with its general title.

## Portal check

Lists everything on the emulated portal, marks what there is too much of, and takes off what the
player picks; B closes it, and while the error stays it comes back after 8 s. The menu itself never
puts a second item or Skylander on for one player, so this is the fail-safe for figures put on some
other way (a real portal in hybrid mode, the developer tools).

## The portal speaker

The Traptanium Portal has a speaker: a trapped villain talks from it. With the emulated portal the
game's speaker stream (G.726) plays from the PC's speakers (skylanders-portal `g726.cpp`, F4 >
Portal > "Portal-Lautsprecher", its volume), and the overlay above shows who is talking.
