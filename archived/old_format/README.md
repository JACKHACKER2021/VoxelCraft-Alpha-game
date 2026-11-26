# Starý formát (před přestavbou na VC jazyk, 2.8.0)

Kopie kódu, jak hra stavěla modely **před** VcModel. Jen pro srovnání, nekompiluje se.

- `Mobs.cpp` – tvorové jako ručně psané krabice v C++ (Builder, yaw 0 = -z).
- `PlayerModel.*` – hráč, pevné krabice, skin 64×64.
- Módy (content.json) uměly jen **kostku s texturou** nebo přebarvení vestavěného tvora;
  geo.json z Bedrocku se ignoroval (tvor dostal tělo krávy).

Nový formát: `engine/VcModel.h` – kosti (pivot, rotace, rodič) a kostky (origin, size, uv, inflate),
čte geo.json 1.12+, starší `geometry.*` i vlastní `*.vcmodel.json`. Referenční modely: `assets_src/models_2.8.0` (GLB).
