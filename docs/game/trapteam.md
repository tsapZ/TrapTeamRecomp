# Skylanders: Trap Team (Xbox 360)

What we know about the game and the dump this project targets, and what still has to be checked.

## The game

- The fourth Skylanders game, 2014, published by Activision, developed by Toys for Bob, with the Traptanium portal (traps for villains). It reads every earlier game's figures (Swap Force's swappers as whole figures) and its own: Trap Team Skylanders, Minis, traps and magic items.

## Target dump

| Property | Value | Source |
|---|---|---|
| Disc content | 2124 files | `extract-xiso`, measured |
| `default.xex` size | 9,252,864 bytes, in the disc root | measured |
| `default.xex` SHA-256 | `f1d2373e4507e21cc371e6f22d02e909602a1b5bc4704896c3c099cc64002a35` | measured, in `default.xex.sha256` |

Top level of the disc: `$SystemUpdate`, `alchemy.xml`, `character`, `default.xex`, `item`, `level`, `misc`, `movies`, `nxeart`, `outfit`, `permanent`, `renderer`.

## Differences that matter

- **Scaffolded from GiantsRecomp** (app, portal, menus, tooling); none of its game hooks or file
  readers, which are for Giants' executable and its archives (IGA version 8).
