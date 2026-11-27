# Zvuky tvorů (k modelům 2.8.0)

Zvuky pro všech 26 tvorů z `models_2.8.0/mobs`, syntetizované skriptem `tools/mob_sounds.py`
(numpy + ffmpeg): hlasy jsou harmonické tóny tvarované formanty samohlásek, k tomu šum, bublání, cvakání.
Žádné nahrávky, nic z jiných her. Do hry zatím zapojené nejsou.

Každý tvor má v `mobs/` čtyři zvuky (mono Ogg Vorbis 44,1 kHz jako ostatní zvuky hry):
`mob_<jméno>_say_1.ogg`, `mob_<jméno>_say_2.ogg` (sám od sebe), `mob_<jméno>_hurt.ogg` (zranění),
`mob_<jméno>_death.ogg` (smrt). Hlasitost je vyrovnaná.

Ukázka všech: `mob_sounds_demo.mp3` (3:12), u každého tvora v pořadí say 1, say 2, hurt, death.

| Čas | Tvor | Zvuk |
|---|---|---|
| 0:00 | prase | chrochtání nosem, kvičení |
| 0:04 | kráva | bučení „mmúú“ |
| 0:11 | ovce | mečení s chvěním |
| 0:16 | slepice | kvokání, kdákání, třepot |
| 0:21 | liška | štěkavé „jip“, vytí |
| 0:26 | popelavý lišák | hlubší liška, k tomu praskání žhavých uhlíků |
| 0:31 | netopýr | vysoké pištění, mávání křídel |
| 0:36 | vesničan | „hmm“, „hm-ah“ |
| 0:41 | sněhulák | křupání sněhu, dobrácké „óó“ |
| 0:45 | duch | kvílení s ozvěnou |
| 0:56 | pavouk | cvakání kusadel, syčení |
| 1:01 | dýňový sliz | čvachtání a bublinky |
| 1:07 | zapomenutý muž | šepot a dutý sten v ozvěně |
| 1:17 | TNT zombie | chraplavé sténání, syčící doutnák, smrt výbuchem |
| 1:24 | Husking | chrčivý dech, plivnutí ohnivé koule |
| 1:30 | dehtové monstrum | bublání dehtu, hluboké vrčení |
| 1:39 | hogman | hluboké chrochtání |
| 1:45 | úhelný golem | skřípění kamene, dunivé údery, rozpad |
| 1:53 | mechový plaz | šustění mechu, kvákavé vrčení |
| 1:59 | sporovka | obláčky spor, tenké pípání |
| 2:04 | žhavá můra | třepotání křídel, jiskry |
| 2:10 | Celestling | cvrlikání se zvonkem |
| 2:17 | Reach Stalker | bzučení, „pozpátku nasátý“ zvuk při mrknutí |
| 2:25 | Sky Drifter | táhlé volání jako velryba v oblacích |
| 2:38 | Sky Guardian (boss) | mohutné troubení se sborovým dozvukem |
| 2:56 | Crimson Man (boss) | zkreslený hluboký hlas o oktávu níž, nasátí ze tmy |

    python3 tools/mob_sounds.py   # znovu vytvoří všechny zvuky i ukázku

# Zvuky hráče

Krátké, suché a úderné zvuky ve stylu klasických kostičkových her, skript `tools/player_sounds.py`
(numpy + scipy). Místo čistých tónů jsou z fyzikálních modelů, aby zněly přirozeně: hlas jsou pulzy
hlasivek (s chvěním a dechem) prohnané rezonancemi hrdla a úst, nárazy, křupání, led a oheň jsou šum
a drobné impulzy přes rezonátory, respawn jsou drnknuté struny. V `player/`, mono Ogg Vorbis.
Ukázka všech: `player_sounds_demo.mp3` (0:27), v tomto pořadí:

| Čas | Soubor | Zvuk |
|---|---|---|
| 0:00 | `player_damage_1.ogg` … `player_damage_3.ogg` | zranění: úder do těla a krátké „uf“, tři varianty |
| 0:03 | `player_freeze_hit.ogg` | zásah mrazem: prasknutí ledu, odlétající střípky, „uf“ |
| 0:04 | `player_fall.ogg` | pád: dopad těla a krátké „uh“ |
| 0:05 | `player_fall_big.ogg` | velký pád: těžký dopad, křupnutí, štěrk, „aúgh“ |
| 0:07 | `player_death.ogg` | smrt: „uaagh“, pád na zem, poslední výdech |
| 0:09 | `player_burn_1.ogg`, `player_burn_2.ogg` | hoření: praskání a hučení ohně, „uf“ |
| 0:12 | `player_husking_flame.ogg` | ohnivá koule Huskingu: svist, výbuch plamene, hoření, „uf“ |
| 0:14 | `player_death_fire.ogg` | smrt v ohni: plameny, výkřik, pád |
| 0:16 | `player_respawn.ogg` | respawn: závan vzduchu a drnknutý akord C–E–G–C |
| 0:19 | `player_hunger.ogg` | hlad: kručení v břiše s bubláním |
| 0:21 | `player_eat_1.ogg` … `player_eat_3.ogg` | jedení: křupavá kousnutí |
| 0:25 | `player_eat_finish.ogg` | dojedení: polknutí a říhnutí |

    python3 tools/player_sounds.py   # znovu vytvoří zvuky hráče i ukázku (potřebuje numpy a scipy)
