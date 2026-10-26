# VoxelCraft Mods (od verze 1.4.0)

Mód je obyčejný .zip s JSON a PNG soubory. V telefonu ho naimportuješ v **Módy a balíčky → Importovat**
a ve správci módů ho zapneš nebo vypneš. Módy přidávají jen obsah: bloky, předměty, recepty, textury
a moby. Nikdy nespouštějí žádný kód.

## Struktura .zip

```
muj_mod.zip
├── manifest.json          {"name": "Rubínový mód", "description": "...", "version": "1.0"}
├── content.json           bloky, předměty a moby (níže)
├── recipes/ruby.json      recepty ve formátu VoxelCraftu
└── textures/mod/*.png     textury 16×16 (nebo 32×32, 64×64)
```

## content.json

```json
{
  "namespace": "ruby",
  "blocks": [
    {"id": "ruby_bricks", "name": "Ruby Bricks", "name_cs": "Rubínové cihly",
     "texture": "ruby_bricks.png", "hardness": 2, "tool": "pickaxe",
     "transparent": false, "light": false, "drop": "ruby:ruby_dust"},
    {"id": "ruby_log", "name": "Ruby Log",
     "textures": {"top": "ruby_log_top.png", "side": "ruby_log.png", "bottom": "ruby_log_top.png"}}
  ],
  "items": [
    {"id": "ruby_apple", "name": "Ruby Apple", "name_cs": "Rubínové jablko",
     "texture": "ruby_apple.png", "food": 6, "stack": 64}
  ],
  "mobs": [
    {"id": "ruby_pig", "name": "Ruby Pig", "base": "pig", "texture": "ruby_pig.png",
     "face": "ruby_pig_face.png", "health": 12, "spawn": 0.2,
     "drops": [{"item": "ruby:ruby_apple", "count": 1}]}
  ]
}
```

- `tool`: `pickaxe`, `axe`, `shovel` nebo nic.
- `base` mobů: `pig`, `sheep`, `cow`, `chicken`, `spider`, `zombie`, `slime`, `villager`.
  Mob má tělo a chování základního moba, ale vlastní texturu, zdraví a dropy.
- Recepty se na nové věci odkazují celým jménem, např. `"ruby:ruby_bricks"`.

Každý nový blok a předmět dostane číslo od 2000 výš. Čísla si hra pamatuje v `mod_ids.txt`,
takže světy své bloky neztratí ani po vypnutí a zapnutí módu.

## Java módy a datapacky

Importovat jde i **.jar** mód z Java Edition nebo datapack či resource pack (.zip). Hra z nich převede:

- textury bloků a předmětů z balíčku módu;
- anglické a české názvy (`lang/en_us.json`, `lang/cs_cz.json`);
- jednoduché kostkové bloky (`cube_all`, `cube_column`, `cube_bottom_top`) a předměty s plochou texturou;
- recepty (shaped, shapeless, smelting) a co bloky po vytěžení dávají (loot tables).

Nepřevede se:

- nic ze samotného Minecraftu (`assets/minecraft`, `data/minecraft`);
- bloky jiného tvaru než kostka;
- recepty se štítky (`#tag`);
- chování napsané v Javě (kód se nikdy nespouští).

Po importu ukáže správce módů zprávu, co prošlo a co se přeskočilo.
