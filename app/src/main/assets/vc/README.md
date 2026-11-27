# VC jazyk – data hry (2.8.0 preview)

Vše, co hra dřív měla jen v kódu, je teď v souborech. Hra je při instalaci rozbalí do `<data>/vc`.
Doplněk (pack) může kterýkoli z nich přepsat vlastní složkou `vc/` (nebo `models/` u tvorů).

| Soubor | Co v něm je | Jak ho přepíše doplněk |
|---|---|---|
| `models/mobs/<id>.glb` | těla všech 26 tvorů (GLB), `_sit` sedící póza, `_celestial` nebeská varianta | `models/mobs/<id>.glb` v resource packu |
| `blocks.json` | každý blok a předmět: `key`, `name`, `tiles` (6 stěn), `hardness`, `stack` | `vc/blocks.json`, položky se stejným `key` nahradí hru |
| `loot.json` | co je v truhlách staveb: `rolls`, `entries` (`item`, `weight`, `count`) | `vc/loot.json` |
| `shaders/*.vsh, *.fsh` | shadery světa (GLSL ES 3.00) | shader pack: `shaders/world.fsh` a `shader.json` |
| `structures_examples/*.vcstructure` | každá stavba světa jako vzor | zkopíruj do `vc/structures/<jméno>.vcstructure` (nebo `_1` … `_9` varianty) |

Pohyb tvorů je v GLB uzlech: `"extras": {"vc": {"swing", "flap", "attack", "pivot", "show": "collar"}}`,
v kořenovém uzlu `flapSpeed`, `flapAmp`, `flapRest` a `squash`. Bloky bez vlastního jména mají klíč `vc:id/<číslo>`.

Recepty, biomy a dimenze jsou JSON odjakživa (`recipes/`, `biomes/`, `dimensions/` vestavěného balíčku).
Vše se dá vytvořit i v Tvůrci (Doplňky > Tvůrce) bez psaní JSON.
