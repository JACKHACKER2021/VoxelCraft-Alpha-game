# VoxelCraft 2.8.0 – VC jazyk

Celá hra přešla na **VC jazyk**. Tvorové, bloky, předměty, loot, shadery i stavby už nejsou natvrdo v kódu, ale v souborech, které jde upravit a které může přepsat doplněk.

## 🐷 Tvorové
- Nové modely prasete, ovce, slepice, krávy a ducha s jasně viditelnými obličeji.
- Všech 26 tvorů je v souborech GLB a jde je otevřít v Blockbench nebo Blenderu. Patří k nim i bossové s údery, sliz, který se smáčkne, liška s obojkem a nebeské prase a ovce.
- Forgotten man má nový skin.

## 🧩 VC jazyk a doplňky
- `blocks.json` popisuje všechny bloky a předměty: název, textury, tvrdost a velikost stacku.
- `loot.json` určuje obsah truhel ve stavbách.
- Shadery jsou v souborech `shaders/*.vsh` a `shaders/*.fsh`.
- Stavby jsou v souborech `.vcstructure`. Doplněk může libovolnou stavbu světa nahradit.
- Dimenze úhelného kamene má `cornerstone.json` se svými biomy (lom, pobřeží, monolit, pustina, louka). Starý `nether.json` se dál čte kvůli starším balíčkům.
- Výchozí doplněk obsahuje textury, shadery a téma menu. Každý doplněk jde exportovat jako `.vcaddon`.

## 🛠️ Tvůrce
- Bloky, předměty, moby (s tělem kteréhokoli tvora), modely z kostek s malováním textury, recepty, shadery a balíček. Vše bez psaní JSON.

## 🚇 Metro a hra
- Automaty na jízdenky, turnikety, celodenní jízdenka.
- Hlášení „Příští stanice…“ česky i anglicky.
- Deník hráče a deset nových úspěchů.
- Vánoční a halloweenské věci jsou dostupné jen se zapnutým doplňkem.

Starší světy se nemění. APK: `VoxelCraft-2.8.0.apk` · Preview: 2.9.0 (opuštěné nádraží, vozovna, cesta k 3.0.0 bez železa).
