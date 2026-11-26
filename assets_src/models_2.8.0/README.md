# 🧍 Modely ze hry (2.8.0) – standard pro přestavbu na VC jazyk

Modely tvorů a hráče přesně tak, jak je engine skládá ve verzi 2.8.0 (ruční krabice v `Mobs.cpp`
a `PlayerModel.cpp`). Při přestavbě na VC jazyk (`VcModel`) podle nich modely postavíme, aby se nic nezměnilo.

- `mobs/*.glb`: 26 tvorů, u těch, co sedí, i póza `_sit` (31 souborů).
- `player/*.glb`: hráč ve všech 9 zabudovaných skinech.
- Každá krabice je samostatný uzel (`cube_0`, `cube_1`, …), jednotka = 1 blok, osa Y nahoru.
- Textura je uvnitř souboru: u tvorů jen použité dlaždice atlasu, u hráče skin 64 × 64.
- Otevře se v Blockbench, Blenderu nebo jakémkoli prohlížeči glTF.

Vytvořeno nástrojem `tools/export_models.cpp` (sestavení a spuštění je v jeho hlavičce).

# 🧍 Models from the game (2.8.0) – the standard for the VC language rebuild

The creatures and the player exactly as the engine builds them in 2.8.0, as GLB: one node per box,
1 unit = 1 block, Y up, the texture packed inside. Made by `tools/export_models.cpp`.
