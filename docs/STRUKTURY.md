# VCStructures (VoxelCraft Structures)

Stavby, které jde vložit do světa: domy, věže, ruiny. Kód: `app/src/main/cpp/engine/Structure.*`
(formáty, otáčení, umístění) a `engine/Nbt.*` (čtení a zápis NBT z Bedrocku).

## Kde je najdeš ve hře
Kreativní svět → pauza → **Struktury**. Seznam obsahuje:
1. vestavěné stavby (Dřevěný domek, Strážní věž, Studna),
2. soubory ve složce hry `structures/` (sem je kopíruje tlačítko **Importovat soubor**),
3. složky `structures/` zapnutých balíčků (`packs/<id>/structures/*.mcstructure|*.vcstructure`).

Tlačítka: **Otočit** (po 90°), **Zrcadlit**, **Umístit před sebe**, **Vrátit zpět** (posledních 10
umístění v dané dimenzi), **Importovat soubor**, **Obnovit seznam**. Náhled ukazuje stranu,
která bude otočená k hráči. Stavba se položí na blok, na který míříš, jinak 2 bloky před tebe,
a nikdy ne přes hráče.

Bezpečnost: nejdřív se zkontroluje, že je celé místo načtené a stavba se vejde výškově
(y 1 až 127). Teprve potom se mění bloky, takže nikdy nezůstane půlka stavby. Podloží (bedrock)
se nepřepisuje. Změny se ukládají stejně jako ruční stavění (jen změněné chunky), takže starší
uložené světy fungují dál beze změny formátu.

## Formát `.vcstructure` (verze 1, little endian)
```
"VCST"            4 B   značka
u16 version       = 1
u16 sx, sy, sz    rozměry (max 256 × 128 × 256)
u16 nameLength, name (UTF-8)
u16 paletteSize, pak pro každou položku: u8 délka + klíč bloku ("stone", "planks", "void", ...)
běhy (u16 počet, u16 index palety) přes buňky v pořadí y, z, x (x nejrychleji)
```
- `void` = buňka nechá blok ve světě beze změny (jako structure void), `air` blok vymaže.
- Klíče místo čísel: soubor zůstane platný, i když se čísla bloků v budoucnu změní.
- Čtení kontroluje každou délku a index; poškozený soubor se odmítne s hláškou, hra nespadne.

## Import `.mcstructure` (Minecraft Bedrock)
Nekomprimované NBT v little endian: `size` [x, y, z], `structure.block_indices` (dvě vrstvy,
pořadí z → y → x, -1 = prázdno), `structure.palette.default.block_palette` (`name`, `states`).
- Názvy bloků se převádějí na bloky VoxelCraftu (`minecraft:oak_planks` → prkna,
  `spruce_log` → kmen, `brick_block`/`bricks` → cihly, `short_grass` → tráva, ...).
- Bloky, které VoxelCraft nemá, se vynechají (buňka = void) a obrazovka ukáže, kolik jich bylo.
- Export do `.mcstructure` umí `exportMcStructure()` (ověřeno knihovnou nbtlib), ukázky jsou
  v `examples/structures/` (`domek.mcstructure`, `vez.vcstructure`).

## Co je hotové a ověřené
- Načtení `.mcstructure` i `.vcstructure`, umístění do skutečného světa, vrácení zpět
  (automatické testy `tests/core_test.cpp` → `testStructures`, včetně průchodu přes menu hry).
- Odmítnutí poškozených souborů: všechna zkrácení a 300 náhodně poškozených kopií bez pádu.
- Otočení o 0–270° a zrcadlení (testy na přesné pozice bloků).
- Soubor zapsaný cizí knihovnou (nbtlib) se načte správně a naopak.
- Na skutečném telefonu zatím netestováno, jen na PC a v softwarovém vykreslování.

## Co zatím chybí
- **Stavy bloků** (`states`): natočení kmenů, schodů, dveří apod. se ignoruje; VoxelCraft zatím
  nemá bloky s natočením, takže otáčení stavby mění jen polohu bloků, ne jejich orientaci.
- **Bloky, které VoxelCraft nemá** (schody, dveře, ploty, vlna, lampy, ...) se vynechávají.
- **Druhá vrstva** (`block_indices[1]`, voda v blocích „waterlogged“) se ignoruje.
- **Entity a obsah truhel** (`entities`, `block_position_data`) se neimportují.
- **Uložení stavby ze světa** do souboru: funkce `captureStructure()` a `writeVcStructure()` /
  `exportMcStructure()` existují a jsou otestované, ale ve hře pro ně zatím není výběr oblasti.
- **Generování staveb ve světě** (vesnice, ruiny podle biomu) zatím není.
