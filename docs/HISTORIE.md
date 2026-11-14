# VoxelCraft – co všechno je hotové (0.0.1 → 1.5.0)

Autor: Hoppy_Hopsy21 aneb VoxelSoftworks. Vlastní C++ engine pro Android 16 (NativeActivity, GLES3),
bez jakýchkoli assetů nebo kódu od Mojangu.

## 0.0.1 – 0.4.0: základ
- Vlastní voxelový engine v C++: chunky, generování terénu (šum), mesher, kamera, fyzika hráče.
- Nether s portálem z obsidiánu.
- Texture packy: klasický terrain.png nebo Bedrock/Java balíček vybraný v telefonu.
- Menu, ukládání světů, režimy Přežití a Kreativní, inventář, zdraví a hlad.
- Výroba: inventář s mřížkou 2×2, pracovní stůl 3×3, pec, nástroje a rudy (uhlí, železo).

## 0.5.0
- Retro vzhled menu.
- Biomy, Nether a recepty v JSON (data-driven), modpacky a JSON editor přímo ve hře.

## 0.6.0
- Obchod VoxelCraftu – jen mince vydělané hraním (1 mince za 10 vytěžených bloků).
- Skiny s 3D modelem z pohledu třetí osoby, import vlastního skinu (PNG).
- Zvuky a hudba, profil hráče, úprava světa (název, režim, souřadnice, ponechat inventář).

## 0.7.0
- Tráva, pampelišky a vlčí máky, hustota podle biomu.
- Částice při těžení a rozbití bloku.
- Lepší voda (vylézání na břeh), D-pad místo joysticku.
- VCStructures: vlastní formát struktur, import .mcstructure, otáčení/zrcadlení, zpět (undo).

## 0.9.0
- Celé koruny stromů, stínování (ambient occlusion), stín pod stromy.
- Dělení stacků v inventáři, vyhazování věcí, věci ležící na zemi (ukládají se se světem).
- Ruka v první osobě s animacemi (chůze, těžení, pokládání).

## 1.0.0
- 5 jazyků: angličtina (hlavní), čeština, španělština, ruština, němčina (včetně azbuky).
- Titulka s „Voxel Softworks“ a měnícími se splash texty.
- Bronz, ocel, rubín: rudy, nástroje; brnění z 5 materiálů (sloty, ochrana, opotřebení).
- Nové bloky: 16 barev vlny, pískovec, kamenné cihly, mechový kámen, knihovna, sníh, jíl, kovové bloky.
- Kreativní inventář se záložkami, ostřejší textury, 3D modely předmětů v ruce i na zemi.
- Minimapa (spawn, místo smrti, struktury), pauza nahoře uprostřed.
- Vývojářský přístup (jméno + heslo) s režimy a experimenty (den/noc, rychlé pece, rudy na mapě).
- Hudba: menu téma, skladby podle biomů, vlastní hudba ve složce music/.

## Update po 1.0.0 (build 10–12)
- Tvarované bloky: dveře (dřevěné, železné), poklop, plot, truhla, kaktus; smrk, led, tvrdý kámen.
- Truhly s úložištěm a lootem, kaktus zraňuje.
- Biomy: zasněžený borový les, ledové pláně, poušť s kaktusy, vysoké skalnaté hory.
- Struktury s lootem: ruiny, vraky lodí, dungeony, doly.
- Loď (sezení a plavba), vývojářský skin pro Hoppy_Hopsy21.
- Třetí osoba: viditelné brnění, máchání rukou, předmět v ruce.
- Achievementy s bonusovými mincemi.
- Multiplayer přes LAN: otevřít svět do LAN, připojit se ze seznamu světů.
- Načítací obrazovky: načítání světa, stavění terénu, cesta do jiné dimenze.
- Chat a příkazy (/gamemode, /tp, /give, /time, /weather, /summon…), volba „Povolit příkazy“.
- Videa: 36s ukázka, český 60s trailer, anglický 2min příběhový trailer s namluvením.

## 1.1.0 „Crimson Void“ (build 13)
- Nether: nether tráva, 2 biomy, pevnost z nether cihel s lootem a obrovským portálem z void stone.
- Void zapalovač → nová dimenze Crimson Void: fialový les s oko-stromy, karmínová poušť
  (červený písek), lávový biom s arénou bosse.
- Moby: prase, ovce (maso, vlna), dýňový sliz a zapomenutý muž (v noci), boss Karmínový muž.
- Postel (vlna + prkna): spaní přes noc, bod oživení.
- Obloha: ráno, den, večer, noc, půlnoc; slunce, měsíc, hvězdy, mraky.
- Počasí: déšť v běžných biomech, sníh v zasněžených, v poušti nic.
- Nové achievementy a recepty, jídlo (syrové/pečené maso).

## 1.1.1 – opravy (build 14)
- Ruce hráče i zapomenutých lidí se už nehýbou obráceně.
- Helma nezakrývá obličej, opravené nohy a hlava ovce.
- Déšť padá dolů (ne ze země), postel se texturou otáčí podle směru.

## 1.2.0 (build 15)
**Svět a struktury**
- Crimson Void: kobky a velké spirálové věže s lootem; v něm kompas, který vede k aréně bosse.
- Overworld: pouštní chrámy, iglú, strážní věže.
- Nezničitelný blok „Konec světa“ na úplném dně (y = 0) ve všech dimenzích.

**Předělaný Nether**
- Netherrack → úhelný kámen (cornerstone), úhelná tráva a rostlina grasser.
- Soul sand, quartz a quartz blok už nejsou; místo glowstonu powerstonové světlo.
- Netherperkové stromy (nový strom v Netheru).

**Powerstone a elektronika**
- Powerstonová ruda → prach, powerstonový blok (zdroj), napájecí kabel, vypínač,
  powerstonová lampa, powerstonový čip; proud otevírá železné dveře a zapaluje TNT.

**Hratelnost**
- TNT (zapalovačem nebo proudem) a výbuchy; postel v Netheru a Crimson Void vybuchne.
- Kbelík: nabrat/vylít vodu a lávu (voda se v Netheru vypaří).
- Meče: dřevěný, kamenný, železný, bronzový, ocelový, rubínový.
- Nová dřeva: dub, borovice, void, netherperk (prkna).
- Sazenice: dub, borovice, void, netherperk – vyroste za 20 sekund.
- Listí bez kmene opadá, větší šance na jablko, listí pouští sazenice.
- Spawn vejce: prase, ovce, dýňový sliz, zapomenutý muž.
- Nové experimenty: rychlý růst sazenic (5 s), nízká gravitace, velké výbuchy.
- 5 nových achievementů (Bum!, Elektrikář, Zahradník, Poklady prázdnoty, Cestovatel).

**Obchod**
- VIP za 3000 mincí: zlaté [VIP] jméno, 2× mince z denních odměn, točení navíc, 2 VIP skiny.
- Zimní doplněk (850): všude sníh a sněžení, zasněžené textury, dárky, cukrové hůlky, denní dárek.
- Halloween (950): rudá obloha, hustá mlha, hordy zombie (přebarvení zapomenutí lidé), jaderné TNT.
- Výroční balíček (1. narozeniny): jen kódem z kola štěstí – konfety, dort.
- Denní odměny den 1–7 (mince, točení, předměty, na 7. den doplněk na 8 h).
- Kolo štěstí: 10–500 mincí, skin zdarma, VIP na 24 h, doplněk na 8 h, výroční kód.
- Doplňky se dají zapnout/vypnout; předměty doplňků mají vlastní záložku v kreativním inventáři.

## 1.3.0 – Farma a vesnice (build 17)
- Zemědělství: motyka, ornice, semínka, pšenice ve 4 fázích, chléb, balík sena.
- Slepice a kráva (maso, peří, mléko), nová vejce: slepice, kráva, vesničan.
- Luk a šípy (natahování, sbírání šípů zpět).
- Vesnice s domy, studnou, poli a lampami; vesničané (farmář, kovář, obchodník) a obchodování za rubíny.
- Klidný režim pro citlivé hráče.
- Napájecí kabel se tvaruje podle sousedů.
- Hrát rozdělené na Singleplayer a Multiplayer; hostování z menu, sdílený čas a počasí, seznam hráčů v pauze.
- Větší D-pad, značka VoxelSoftworks, úvodní animace s kostkami.

## 1.4.0 – Online a módy (build 18)
- 16bitová čísla bloků a předmětů (až 65 000), atlas 32×32 dlaždic, nové formáty chunků a sítě.
- Jeskyně 2.0: obří dutiny, podzemní jezera, krápníky; pavouk (provázek na luk) a netopýři.
- Nové příšery: dehtové monstrum v jeskyních (z poražených zvířat dělá dehet a nová monstra) a TNT zombie.
- Schody a desky z 12 materiálů, barevné sklo v 16 barvách, mapa jako předmět.
- Lampy a powerstone světlo opravdu svítí v noci; spínač a kabel jdou položit na zem i na zeď.
- Sníh při sněžení zůstává ležet jako pokrývka.
- Výroční modpack: výstava s logem VOXELCRAFT, balónky, lampiony, malovaná prkna, sběratelské předměty, úspěchy, hustší listí a tráva.
- VoxelCraft Mods (JSON + PNG), správce módů, import Java módů a datapacků.
- Online hra s kódem místnosti, hra přes hotspot bez Wi-Fi a dat, sdílené truhly, pece a zvířata, přepínač PvP, ochrana spawnu.
- Automatické zálohy, export a import světa (.vcworld) do Souborů, kniha receptů, tutoriál.

## 1.5.0 – Forgotten Powerstone (build 19)
- Biom Powerstonová pustina: suchá tráva, květy Grasser, zbytky sloupů, mrtvé kmeny, štěrk s nálezy, powerstonová ruda.
- Zapomenuté vesnice, zatopené ruiny a rozpadlé portály s vlastním lootem; kompas k pevnosti.
- Archeologie se štětcem, lebky kostlivců, starověké mince, střepy, relikvie a osm stránek příběhu.
- Husking: bezruká příšera s nakloněnou hlavou, drží odstup a plive ohnivé koule.
- Powerstonové cihly, sklo, schody a desky (nesvítí); ploty z pěti dalších materiálů s novou ikonou.
- Nether přejmenován na dimenzi úhelného kamene; pevnosti s popraskanými a zdobenými cihlami.
- Nová kůra stromů, kapradí, 8 úspěchů; oprava textury hořícího TNT a šípů.

## 1.5.1 – opravy a experimenty
- Pustina, Grasser, Husking a zapomenuté vesnice přesunuty do dimenze úhelného kamene; nová kůra stromů, modely krumpáče a sekery.
- Crimson Desert předělaný; experimentální funkce pro každý svět (Ore Redesign).

## 1.5.5 – Shadery
- Grafika Classic a Enhanced, shader packy (vytvořit/importovat), nastavení videa.

## 1.5.7 / 1.5.9
- Pochodně, štít a druhá ruka, padající písek a štěrk, tekoucí voda a láva (experiment), mapy průzkumníka.
- Větší pevnost, powerstonová pec, velká mapa světa, chat, psací kniha, titulky s galerií trailerů, šetřič baterie.
- Kategorie kreativního inventáře, důlní šachty a strážní věže v Karmínové prázdnotě, zničené vesnice.

## 1.6.0 – Caves and Tunes
- Hlubinný kámen, hardium, velké jeskyně s otvory, jezera lávy a dehtu, mechové jeskyně, hudba v jeskyních.

## 1.7.0 – 1.7.2 – The Celestial Reach
- První verze plovoucích ostrovů, nové hlavní menu, opravy.

## 1.8.0 – Concrete & Old World
- Starý svět (generace 0.1–0.3 se starým vzhledem a zvuky), balíček Pre-Alpha, beton, Strakatý les.

## 1.9.0 – Celestial Reach & Seasons
- Nová Nebeská říše s nízkou gravitací, celestium (nástroje, kyrys), nebeská prasata a ovce.
- Oheň ze zapalovače a táborák, padající listí, tekoucí voda a láva všude, plynulý pád bloků.
- Sezóny podle data, 6 vzhledů plochého světa, průvodce „Jak vyrobit?“, experimenty 2.0 (bouřky, hvězdy, chov).

## 1.9.5
- Počasí 2.0, teplota, Celestling, Reach Stalker, Sky Drifter, Nebeský chrám se Sky Guardianem, křídla.
- Zvuky Minetest Game podle materiálu a stylu, ohraničený svět, presety seedu, výběr světa s obrázky,
  21 struktur v encyklopedii, med, structure block, bariéra, příkazový blok.

## 2.0.0 – The Whole Story
- Balíčky pro každý svět, Pre-Alpha vzhled pro všechny bloky, zvuky podle bloku, nové logo.
- Zvukový blok, spojené kabely na zemi/zdi/stropě, spínač na zeď, sedák se sezením.
- Import světů z Minecraft Bedrock (.mcworld), složka Speciální bloky, 17minutový příběh projektu.
- Vlastní rozsah ID (60000+) pro nové bloky, aby se nekryly s deskami.

## 2.1.0 – Trains & Bronze
- Vlaky (lokomotiva, osobní a nákladní vagon, vrtací vůz), 3D koleje, oblouky, výhybky, svahy, zarážedla, točna.
- Přejezd (výstražník, závora, zvonek, houkačka), booster; pixel-art textury vlaků, kouř, náklon na svahu.
- Bronzový update: řezaný a zdobený bronz, mřížka, žárovka. Recepty na koleje (ocel + smrk) a vagony.
- Psaní přímo ve hře bez okna Androidu; novinky na úvodní obrazovce a v menu.
- Preview buildy 1–6 prošly testováním v chatu, než šlo vše do hlavní verze.

## Nápady, na které se dá navázat
- Další věci do doplňků (vánoční bloky, halloweenské dýně s obličeji, narozeninové balónky).
- Nové struktury v Netheru, další bossové, zbraně na dálku (luk).
- Složitější elektronika (opakovač, senzor, píst).
