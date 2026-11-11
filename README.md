# The Hollow Crypt

A small, single-player isometric action RPG for Windows, written in C++20. The refuge, road, stairway, four crypt rooms, and return passage share one continuous map.

## Play

Run `build\HollowCrypt.exe`. To build from source, install Visual Studio or Visual Studio Build Tools with **Desktop development with C++**, then run `.\build.cmd` from this folder. The game uses Win32, GDI, and WinMM. Its art and sound are generated in code.

- **Left click** walks to a tile, attacks an enemy, or picks up loot.
- **Right click** cleaves nearby enemies. Watch the cooldown in the HUD.
- **Q** drinks a healing draught. The town healer also restores life and replenishes draughts.
- **I** opens the pack. Click an item to equip it or right-click to drop it.
- **E** interacts with the healer or stash when nearby, picks up nearby loot in the field, or respawns you in town after death. When the stash is open, right-click a pack item to store it and click a stashed item to withdraw it.
- **M** toggles sound. **Esc** closes the pack.
- **R** starts a new run once the Warden is dead and you're back in town.

Follow the road out of Ashwick Refuge. The first enemy drops a weapon upgrade. Defeat the Warden, collect the rare item, and take the unlocked passage back toward town. New runs keep your equipment, pack, and stash for the current session and reroll enemy composition and loot. Closing the game ends the session.

## Verify

Run `.\build.cmd test`. It simulates 24 seeded runs through combat, loot, the boss, and the shortcut, and checks healing, inventory, stash, run reset, and respawning.
