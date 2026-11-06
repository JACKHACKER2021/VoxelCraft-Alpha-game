# VoxelCraft Trains

Vlakový systém pro engine, napsaný od nuly (`engine/Trains.h`, `engine/Trains.cpp`). Z My_Own_Trains je převzatý jen **nápad**
(koleje, pražce, parní lokomotiva, boxcar, passenger car), žádný kód ani modely.

## Jak to funguje
- **Kolej** je dílek na bloku (jeden dílek = jedna buňka, zatím rovně bez svahů): `straight`, `curve`, `cross`, `stop`.
  Směry jako u dveří: 0 sever (-z), 1 východ (+x), 2 jih (+z), 3 západ (-x).
- **Vlak** je řada vozů (`steam`, `boxcar`, `passenger`). Každý vůz má vlastní polohu na trati (buňka, vstupní strana, 0..1)
  a všechny jedou stejnou vzdálenost, takže rozestup drží přesně a zatáčky fungují bez dalšího počítání.
- Couvání jen otočí stavy vozů (lokomotiva pak tlačí). Zarážka (`stop`) vlak zastaví před sebou; konec trati také.
- Dílky bez pevného bloku pod sebou po vteřině zmizí. Dílky v nenačtených chuncích zůstávají.
- Ukládá se do `trains.dat` ve složce světa.

## Příkazy (světy s povolenými příkazy / Kreativ)
| Příkaz | Co dělá |
|---|---|
| `/rail line 12` | položí 12 rovných kolejí před tebe |
| `/rail straight [north/east/south/west]` | rovná kolej tam, kde stojíš |
| `/rail curve [dir]` | zatáčka, porty jsou `dir` a další po směru hodin (`north` = sever+východ) |
| `/rail cross` | křižovatka (vlak jede rovně) |
| `/rail stop [dir]` | zarážka na konci trati; `dir` = strana, odkud trať přichází (výchozí: za tebou) |
| `/rail remove` | odstraní dílek |
| `/train spawn [steam boxcar passenger ...]` | postaví vlak na koleji, kde stojíš, směrem pohledu |
| `/train go [rychlost]` | rozjede nejbližší vlak (výchozí 6, max 14 bloků/s) |
| `/train stop` · `/train reverse` · `/train ride` · `/train remove` | zastavit, otočit, nastoupit (skok = vystoupit), smazat |

## Vzhled: `trains.json` v packu
Pack (složka v `packs/`) může mít `trains.json`. Materiály jsou dlaždice atlasu, pojmenované bloky z modů (`namespace:id`,
textury přes `content.json` v `textures/mod/`) nebo klíče vanilla bloků:
```json
{"materials": {"tie": "trains:spruce_tie", "rail": "trains:iron_rail", "steam_body": "black_wool"},
 "models": {"boxcar": [{"from": [-0.45, 0.2, -0.88], "to": [0.45, 1.1, 0.88], "material": "boxcar_body"}]}}
```
Materiály: `tie rail buffer wheel steam_body steam_boiler steam_dark steam_trim boxcar_body boxcar_roof boxcar_trim passenger_body passenger_roof passenger_window passenger_trim`.
Model je seznam kvádrů v blocích (x napříč, y nahoru, z podél vozu, **předek je -z**). Alternativně `{"origin": [...], "size": [...]}`.
Vůz by měl být asi 1,8 bloku dlouhý a 1 široký (rozestup vozů je 2,2). Hotový příklad je v `packs_examples/trains/`.

`tools/geo_to_trains.py` převede vlastní model z Blockbenche (`.geo.json`) na boxy pro `trains.json`.
Používej jen modely, které jsi vyrobil sám nebo je smíš použít.

## Co zatím není (návrh, jak být lepší než Minecraft)
1. Pokládání kolejí položkou a klepnutím (teď jen příkazy).
2. Svahy a mosty (stav má jen x,y,z buňky, svah = dílek s výškou 0..1).
3. Výhybky a návěstidla, stanice se jménem a automatickým zastavením.
4. Nákladní boxcar s inventářem, cestující v passenger car, palivo do parního stroje.
5. Multiplayer (vlaky se zatím nesynchronizují).

## Test
`tests/trains/test_trains.cpp` ověří bez Androidu: spawn, jízdu přes zatáčku, zastavení u zarážky, couvání, ukládání a načtení, tvorbu geometrie a `trains.json`.
```
g++ -std=c++17 -Iapp/src/main/cpp/engine tests/trains/test_trains.cpp tests/trains/stubs.cpp \
    app/src/main/cpp/engine/Trains.cpp app/src/main/cpp/engine/Json.cpp -Wl,--unresolved-symbols=ignore-in-object-files -o /tmp/t && /tmp/t
```
