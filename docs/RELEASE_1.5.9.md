# VoxelCraft Build 3 — Update 1.5.9

## New
- **Map expansion (new worlds, generator generation 3):**
  - Crimson Void mineshafts: void log posts, void plank beams, void stone floor and void loot.
  - Crimson Void watchtowers: void stone bricks with an eye on the lookout. Crimson lands only.
  - Ruined villages in the Overworld: mossy and fallen-down houses, chests with ruin loot.
  - Existing structures are unchanged: mines, crimson mines (Cornerstone), temples, villages, forgotten villages, fortresses, boss arenas, watchtowers, void towers.
- **Creative inventory:**
  - Eleven categories shown as icon tabs: Building Blocks, Glass, Lighting, Nature, Decoration, Powerstone & Mechanisms, Tools & Weapons, Armor, Maps & Special Items, Food & Materials, Spawn Eggs. Add-on and mod tabs are kept.
  - Variants are grouped behind one button marked "+": stained glass, stairs, slabs, fences, wool, logs, planks, ores, cables, swords, pickaxes, axes, shovels, saplings, leaves and old documents. Tapping the button opens the group; Back returns to the category.
  - A player preview shows the worn skin.
- **Torch:** a wall torch is a thin 3D stick that leans out from its wall. Torches on the floor stay upright.
- **Explorer maps** are a standard feature in new worlds. Older worlds keep their 1.5.7 experiment setting.
- **Developer build:** the developer account has testing access to the Anniversary, Winter, Halloween and VIP packs. Nothing is written to its purchases. Players are not affected.

## Fixes
- Crimson Void grass no longer grows the Cornerstone Dimension's sprouts (new worlds). Void roots grow there instead.
- The map, chat and book screens are translated into English, Spanish, Russian and German.
- The creative inventory title no longer overlaps the category name.

## Experiments
| Feature | Status |
|---|---|
| Explorer maps | stable, standard in new worlds |
| Redesigned ores | stable, standard since 1.5.7 |
| Flowing water and lava | still experimental; can be turned on per world |
| Developer experiments (day/night, fast furnace, ore map, fast growth, low gravity, big boom) | developer only, unchanged |

## Compatibility
- The package name and save format are unchanged.
- Old worlds keep their generator: no new structures appear in them, so new chunks do not cut buildings in half at chunk borders.

## Not done yet / known issues
- The modern UI redesign from the previews is planned for 1.6.0.
- Nothing was tested on a real device. Testing used automated tests, UI snapshots and the desktop test renderer only.
- The trailer gallery does not list the 1.5.7 trailer yet.
