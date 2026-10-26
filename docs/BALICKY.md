# Balíčky a doplňky (modpacky)

Balíček je složka nebo `.zip`/`.mcpack` s touto strukturou:

```
manifest.json                {"name", "description", "version"}
biomes/<id>.json             jeden biom na soubor
dimensions/overworld.json    hladina moře, jeskyně, rudy, barva oblohy
dimensions/nether.json       lávové moře, bloky, světlit, mlha
recipes/*.json               recepty, tavení, paliva
textures/                    terrain.png, blocks/*.png, items/*.png (jako balíček textur)
```

Ve hře: **Balíčky a doplňky** → zaškrtni balíček, vyber ho a **Upravit soubory JSON**.
Import: **Importovat .zip** (balíček se rovnou zapne). Ukázka: `examples/ohnivy_nether.zip`.

## Jak se balíčky skládají
Zapnuté balíčky se použijí **odshora dolů**, pozdější přepisují dřívější (pořadí mění tlačítka Výš a Níž).

- **dimensions/*.json**: přepíšou se jen klíče, které soubor obsahuje.
- **biomes**: biom se stejným `"id"` nahradí dřívější. `{"id": "desert", "enabled": false}` poušť odebere.
- **recipes**: recepty se přidávají. `"replace": true` nejdřív smaže všechny dosavadní.

Svět si **při vytvoření uloží kopii** nastavení generátoru (`generator.json`), takže pozdější úpravy
balíčků nezmění terén světů, které už existují. Recepty a textury platí hned pro všechny světy.
Komentáře `//` a čárka za posledním prvkem jsou v souborech povolené.

## Biom
```json
{
  "id": "plains",
  "name": "Pláně",
  "climate": {"temperature": 0.5, "humidity": 0.35},
  "terrain": {"height": 66, "variation": 5},
  "blocks": {"surface": "grass", "filler": "dirt", "filler_depth": 3, "beach": "sand", "underwater": "sand"},
  "trees": {"density": 0.003, "trunk": "log", "leaves": "leaves"}
}
```
- `climate`: kde biom leží na mapě klimatu (0–1). Každé místo světa dostane biom s nejbližším klimatem.
- `terrain.height`: základní výška povrchu (hladina moře je 62), `variation`: jak moc je kopcovitý.
- `trees.density`: pravděpodobnost stromu na sloupec (0.05 = hustý les).

## Overworld (`dimensions/overworld.json`)
`sea_level`, `caves` (`enabled`, `size`), `biome_size` (velikost biomů v blocích), `sky_color` [r, g, b],
`ores`: seznam `{"block", "veins", "size", "min_y", "max_y"}`.

## Nether (`dimensions/nether.json`)
`lava_level`, `blocks` (`fill`, `liquid`, `floor`, `shore`, `ore`, `ceiling`), `floor_amount` (0–1),
`ore_chance`, `glowstone_clusters`, `caves` (`scale`, `openness`), `fog_color` [r, g, b].

## Recepty (`recipes/*.json`)
```json
{
  "recipes": [
    {"type": "shaped", "pattern": ["AAA", ".S.", ".S."], "key": {"A": "planks", "S": "stick"}, "result": "wooden_pickaxe"},
    {"type": "shapeless", "ingredients": ["log"], "result": "planks", "count": 4}
  ],
  "smelting": [{"input": "sand", "output": "glass"}],
  "fuels": [{"item": "coal", "seconds": 80}]
}
```
Vzor má nejvýš 3×3 a dá se zrcadlit. `.` je prázdné pole.

## Názvy bloků a předmětů
`stone, grass, dirt, cobblestone, planks, bedrock, water, lava, sand, gravel, iron_ore, coal_ore, log, leaves,
glass, bricks, obsidian, crafting_table, furnace, netherrack, soul_sand, glowstone, nether_brick, quartz_ore,
stick, coal, iron_ingot, flint, wooden_/stone_/iron_pickaxe, wooden_/stone_/iron_axe, wooden_/stone_/iron_shovel,
apple, flint_and_steel`. Lze psát i `minecraft:stone` nebo `oak_log`.

Nové bloky a předměty zatím balíčky přidávat neumí, jen kombinovat ty existující.
