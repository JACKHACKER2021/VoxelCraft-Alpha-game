# Starý formát (před přestavbou na VC jazyk, 2.8.0)

Kopie kódu, jak hra stavěla modely **před** VcModel. Jen pro srovnání, nekompiluje se.

- `Mobs.cpp` – tvorové jako ručně psané krabice v C++ (Builder, yaw 0 = -z).
- `PlayerModel.*` – hráč, pevné krabice, skin 64×64.
- Módy (content.json) uměly jen **kostku s texturou** nebo přebarvení vestavěného tvora;
  geo.json z Bedrocku se ignoroval (tvor dostal tělo krávy).

Nový formát: `engine/VcModel.h` – kosti (pivot, rotace, rodič) a kostky (origin, size, uv, inflate),
čte geo.json 1.12+, starší `geometry.*` i vlastní `*.vcmodel.json`. Referenční modely: `assets_src/models_2.8.0` (GLB).

## 2.8.0 build 6: tvorové jako GLB

Všech 26 tvorů se v preview kreslí z GLB souborů `app/src/main/assets/vc/models/mobs/*.glb`
(do hry se rozbalí do `<data>/vc`, resource pack je může nahradit vlastním `models/mobs/<id>.glb`).
Pohyb je v GLB uzlech: `"extras": {"vc": {"swing", "flap", "pivot"}}`. Stará těla v `Mobs.cpp` (kopie zde)
zůstávají jen jako záloha: normální edice, módy s přebarvením, nebeské prase a ovce, liška s obojkem,
bossové a dýňový sliz, a když GLB chybí.

## 2.8.0 build 7: celá hra ve VC jazyce

Data jsou v `app/src/main/assets/vc` (viz README tam): tvorové (GLB, i bossové, sliz, liška s obojkem,
nebeské prase a ovce), bloky a předměty (`blocks.json`), loot (`loot.json`), shadery (`shaders/`) a stavby
(`structures_examples/`, doplněk je nahradí přes `vc/structures/`). Hodnoty v kódu zůstávají jen jako záloha,
když soubor chybí, a pro normální edici, která se nemění. Generátory staveb v kódu dál tvoří náhodné varianty;
soubor stavby z doplňku má přednost.

## 2.8.0 (vydání): kopie kódu, ze kterého se VC soubory vyexportovaly

`Loot.cpp`, `Shaders.cpp`, `WorldStructures.cpp`, `GenConfig.cpp` (s `nether.json`), `Items.cpp`: tabulky a funkce,
jak je hra měla v kódu. Ve hře zůstávají jen jako záloha, když soubor ve VC jazyce chybí.
