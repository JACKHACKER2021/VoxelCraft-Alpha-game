# VoxelCraft

Vlastní voxelový engine v C++ pro **Android 16** (arm64), inspirovaný starými
verzemi Pocket Edition. Kód je kompletně náš, nic od Mojangu neobsahuje.

## Novinky ve v1.5.0 – Forgotten Powerstone

Kdysi tu žila civilizace, která svítila a stavěla powerstonem. Pak se při pokusech s portály něco pokazilo.

- **Powerstonová pustina**: nový biom se suchou olivovou trávou, **suchou trávou**, květy **Grasser**, zbytky
  sloupů, mrtvými kmeny, štěrkem s nálezy a powerstonovou rudou kousek pod povrchem.
- **Zapomenuté vesnice** v pustině: rozpadlé domy z powerstonových cihel, popraskané cesty, náměstí se sloupem,
  lampy, které pořád svítí (napájí je powerstonový blok), truhly s relikviemi a stránkami kroniky.
- **Zatopené ruiny** pod jezery a mořem a **rozpadlé portály** v Overworldu (některé napůl zasypané).
  V truhle u portálu je vždy **kompas k pevnosti**: v dimenzi úhelného kamene ukazuje na pevnost,
  v Overworldu na místo, kde postavit portál, aby vedl k ní.
- **Archeologie**: **štětec** (pírko + železný ingot + klacek). Podrž ho na **štěrku s nálezy** a vyhrabeš
  lebku kostlivce, starověkou minci, střep, relikvii nebo **starý dokument**. Bez štětce zbyde jen štěrk.
- **Osm starých dokumentů** – příběh „Pád zapomenutého království“; ťukni s dokumentem v ruce a přečti si ho.
- **Husking**: bezruký muž s hlavou nakloněnou do strany. Drží si odstup, ustupuje a plive ohnivé koule.
  Žije v pustině, ve dne i v noci.
- **Nové bloky**: powerstonové cihly (i popraskané a zdobené), powerstonové sklo, schody a desky, lebka
  kostlivce, popraskané a zdobené netherové cihly. Cihly, sklo, schody ani desky samy nesvítí – svítí jen lampy.
- **Ploty** z dalších pěti materiálů a v inventáři konečně vypadají jako ploty.
- **Nether je teď dimenze úhelného kamene**; pevnosti mají popraskané a zdobené cihly a ploty z netherových cihel.
- Nová kůra stromů, kapradí v lesích, květ Grasser i na loukách, 8 nových úspěchů, `/summon husking`.
- Oprava: hořící TNT a letící šípy měly od 1.4.0 špatnou texturu.

## Novinky ve v1.4.0 – Online a módy

Internetová hra s kódem místnosti, hra přes hotspot, VoxelCraft Mods a import Java módů, jeskyně 2.0, nové příšery,
schody a desky, výroční modpack, zálohy a export světů. Podrobně v `docs/HISTORIE.md`.

## Novinky ve v1.3.0 – Farma a vesnice

- **Zemědělství**: motykou uděláš z trávy ornici, zasadíš **semínka pšenice** (padají z vysoké trávy) a za půl minuty
  roste zralá pšenice. Z pšenice je **chléb** a **balík sena**.
- **Nová zvířata**: **slepice** (kuřecí maso a peří) a **kráva** (hovězí; do kbelíku ji podojíš na **mléko**).
- **Luk a šípy**: podrž prst a natáhni luk, pusť a vystřel. Šípy (pazourek + klacek + pírko) jdou sebrat zpět.
- **Vesnice** na pláních a v lesích: domy kolem studny, cesty, lampy a pole pšenice. Žijí v nich **vesničané** –
  farmář, kovář a obchodník. Ťukni na vesničana a **obchoduj** (za rubíny).
- **Klidný režim** (Nastavení → Zvuk): žádné příšery, tišší zvuky, bez rudého záblesku a vibrací.
- **Napájecí kabel** už není křížek: drát vede tam, kam se napojuje (rovně, do rohu, do T, do kříže).
- **Hrát** má dvě části: **Singleplayer** (tvoje světy) a **Multiplayer** (hry v LAN, připojení přes IP, hostování
  světa jedním ťuknutím). V LAN hře všichni vidí stejný čas a počasí a v pauze je seznam hráčů.
- **Větší ovládání**: D-pad a tlačítko skoku jsou o kus větší.
- Značka **VoxelSoftworks** (dohromady), úvodní animace s kostkami před menu, 4 nové úspěchy.
- Staré světy: bloky odstraněné v 1.2.0 (soul sand, glowstone, quartz) se při načtení převedou.

## Novinky ve v1.2.0

- **Předělaný Nether**: netherrack je teď **úhelný kámen**, k němu **úhelná tráva** a rostlina **grasser**.
  Soul sand a quartz už nejsou, místo glowstonu svítí **powerstonové světlo**. Rostou tu **netherperkové stromy**.
- **Powerstone**: ruda → prach, powerstonový blok (zdroj proudu), **napájecí kabel**, **vypínač**,
  **powerstonová lampa** a **powerstonový čip**. Proud rozsvítí lampu, otevře železné dveře a odpálí TNT.
- **TNT** (písek + powerstonový prach) se zapaluje křesadlem nebo proudem. Postel v Netheru a Crimson Void vybuchne.
- **Kbelík**: nabere a vylije vodu i lávu (voda se v Netheru vypaří). **Meče** ze šesti materiálů.
- **Dřeva a sazenice**: dub, borovice, void, netherperk. Zasazená sazenice vyroste za 20 s,
  listí bez kmene opadá, z listí častěji padá jablko a sazenice.
- **Struktury**: v Crimson Void **kobky** a velké **věže** – v jejich truhlách bývá **kompas k aréně** bosse.
  V Overworldu **pouštní chrámy**, **iglú** a **strážní věže**. Na dně světa je nezničitelný **Konec světa**.
- **Spawn vejce**, nové experimenty (rychlý růst sazenic, nízká gravitace, velké výbuchy), 5 nových úspěchů.
- **Obchod** (pořád jen za mince z hraní): **VIP** (3000), **Zimní doplněk** (850), **Halloween** (950),
  **Výroční balíček** jen kódem z kola štěstí, **denní odměny** den 1–7 a **kolo štěstí**.
  Doplňky se dají v obchodě zapnout a vypnout.
- Celý přehled od první verze je v `docs/HISTORIE.md`.

## Novinky ve v1.1.0 – Crimson Void

- **Nový Nether**: dva biomy (netherová louka s netherovou trávou a výhonky, holý Nether) a **netherové pevnosti**
  z netherových cihel: chodby, čtyři místnosti a velký sál s hromadou lootu. Uprostřed sálu stojí obří
  rám z prázdnotného kamene (7 × 9).
- **Prázdnotné křesadlo** (z truhel v pevnostech) zapálí prázdnotný portál → **Crimson Void**, nová dimenze:
  fialový les se stromy s okem v kmeni (fialová tma), karmínová poušť z rudého písku a lávová pustina
  s **arénou bosse**. Všechno stojí na prázdnotném kameni; hluboko jsou rubíny a ocel.
- **Bytosti**: prase a ovce (maso a vlna), v noci **dýňový sliz** a **zapomenutý muž**, v aréně boss
  **Karmínový muž** (300 životů, ukazatel zdraví, velká odměna). Útok se dělá ťuknutím na bytost, sekery bijí nejvíc.
- **Postel** (3 bílá vlna + 3 prkna): nastaví místo oživení a v noci přeskočí na ráno.
- **Obloha**: 20minutový den – ráno, den, večer, noc, půlnoc; slunce, měsíc, hvězdy a mraky.
  **Počasí**: déšť v běžných biomech, sníh v zasněžených, v poušti neprší.
- Pečené vepřové a skopové z pece, prázdnotné cihly, 5 nových úspěchů, příkazy `/summon`, `/locate`, `/weather`.
- Bytosti, truhly a počasí se v LAN hře zatím nesdílí (každý hráč má své).

## Aktualizace 1.0.0 (build 11–12)

- **Biomy**: zasněžený jehličnatý les, ledové pláně, poušť s kaktusy, vysoké hory s tvrdým kamenem.
- **Struktury s lootem**: zřícenina, potopená loď, kobka z kamenných cihel, opuštěný důl; vyrobitelné truhly.
- **Dveře** (dubové, železné), dubový poklop, ploty, **loďka**; brnění a držené věci vidět i ve 3. osobě.
- **Úspěchy**: 20 cílů v Přežití, za každý bonusové mince (Nastavení → Profil → Úspěchy).
- **Hra v síti LAN**: v pauze „Otevřít pro LAN“; ostatní na stejné Wi-Fi vidí hru nahoře v seznamu světů
  (nebo „Připojit přes IP“, port 19133). Synchronizují se bloky, pozice, skiny, brnění a chat.
  Truhly, pece, předměty na zemi a loďky zůstávají zatím každému hráči zvlášť.
- **Načítací obrazovky**: načítání světa / stavění terénu s průběhem, fialová obrazovka při cestě portálem.
- **Chat a příkazy** (tlačítko s bublinou vedle pauzy): `/help`, `/list`, `/seed`, `/pos`, a s povolenými
  příkazy (Kreativní svět nebo volba „Povolit příkazy“) také `/gamemode`, `/tp x y z | hráč` (s `~`),
  `/give věc [počet]`, `/time day|noon|night|midnight|tiky`, `/spawn`, `/heal`, `/clear`, `/kill`.

## Novinky ve v1.0
- **Jazyky**: angličtina (hlavní), čeština, španělština, ruština a němčina. Přepíná se v Nastavení →
  Profil → Jazyk; pixelové písmo umí azbuku i ñ, ü, ä, ö, ß, ¿, ¡. Překlady jsou v
  `app/src/main/cpp/engine/LangTable.inc` (klíčem je český text ze zdrojového kódu).
- **VoxelSoftworks** na titulní obrazovce a **střídající se texty** pod logem (při každém návratu do menu jiný).
- **Nové rudy**: bronzová (od kamenného krumpáče), ocelová (od železného/bronzového) a rubínová
  (od ocelového, jen hluboko pod y = 20). Taví se na ingoty, z rubínové rudy padá rubín.
- **Nástroje** z bronzu, oceli a rubínu a **brnění** z pěti materiálů (dřevo, dlažební kámen, bronz,
  ocel, rubín): helma, kyrys, nohavice, boty. Brnění chrání před pádem a lávou, opotřebovává se,
  ukazuje se nad srdíčky a nasadí se ťuknutím nebo do slotů vedle postavy v inventáři.
- **Větší kreativní inventář** se záložkami (Stavební, Příroda, Nástroje, Brnění, Suroviny) a nové bloky:
  vlna v 16 barvách, pískovec, kamenné cihly, mechový kámen, knihovna, sníh, jíl, kovové bloky.
- **Ostřejší textury**: tvary s obrysem a stínováním, kámen s prasklinami, dlažba z oblých kamenů atd.
- **3D předměty**: nástroje a předměty v ruce i na zemi jsou vytažené do hloubky jako v klasické hře.
- **Mapa** vpravo nahoře (ťuknutím se zvětší): krajina, spawn, místo smrti a postavené struktury.
  **Pauza** je nově uprostřed nahoře.
- **Hudba**: v menu téma *VoxelCraft Theme*, v Overworldu skladba *Overworld* a střídají se s hudbou
  biomu (pláně, les, poušť, hory, oceán; Nether má vlastní). Vlastní písně stačí dát do složky hry
  `music/` jako `menu`, `overworld`, `plains`, `forest`, `desert`, `mountains`, `ocean` nebo `nether`
  (.mp3/.ogg/.wav), nebo do `music/` balíčku.
- **Vývojářský přístup**: Nastavení → Profil → Vývojářský přístup → Přihlásit (jméno Hoppy_Hopsy21 a
  heslo, které znáš). Ve hře je jen jeho SHA-256 otisk. Vývojářské menu (i z pauzy): nesmrtelnost,
  létání v přežití, okamžité těžení, průchod bloky, rychlost x1–x4, ladicí přehled, přepnutí režimu,
  uzdravení, návrat na spawn, výbava; **experimenty**: den a noc, rychlé pece, rudy na mapě.

## Novinky ve v0.9
- **Stromy opravené**: generátor stavěl strom uprostřed vyplňování terénu a další sloupce mu pak
  přepsaly polovinu koruny vzduchem. Vegetace se teď sází až po celém terénu, koruny jsou celé.
- **Stínování**: listí stíní kmeny a zem pod korunou a rohy bloků mají ambient occlusion
  (plynulé ztmavení u sousedních bloků), takže stromy a krajina mají hloubku.
- **Inventář**: ťukni na slot a v panelu vedle zvol počet (−, 1, +, Půlka, Vše). Dalším ťuknutím
  přesuneš zvolený počet: na prázdný slot (rozdělení), na stejný blok (spojení do 64), na jiný
  blok (výměna celé hromádky). Rychlý přesun mezi hotbarem a inventářem a tlačítko **Vyhodit**.
- **Předměty ve světě**: co se nevejde do plného inventáře (těžba, mřížka výroby), vypadne na
  zem a sebere se, jakmile je místo. Ukládá se se světem, po smrti se inventář rozsype na místě.
- **Ruka v první osobě**: holá ruka podle skinu nebo blok či předmět ze zvoleného slotu. Animace:
  dýchání, pohupování při chůzi, údery při těžbě, krátký pohyb při položení bloku (jen když se blok
  opravdu položí), použití předmětu a výměna při přepnutí slotu nebo vyčerpání hromádky.

## Novinky ve v0.7
- **VCStructures**: vestavěné stavby (domek, věž, studna), import `.mcstructure` z Bedrocku a vlastní
  formát `.vcstructure`. V kreativním režimu Pauza → **Struktury**: otočení, zrcadlení, náhled zepředu,
  umístění před sebe a vrácení zpět. Formát a co zatím chybí: [docs/STRUKTURY.md](docs/STRUKTURY.md).
- **Stromy**: koruny už nejsou duté. Listí kreslí i stěny mezi sousedními listy (průhledná textura).
- **Tráva a květiny**: vysoká tráva, pampelišky a mák (křížové modely), hustota podle biomu
  (`"plants": {"grass": 0.25, "flowers": 0.03}` v JSON biomu).
- **Částice při těžení**: úlomky s texturou bloku během kopání a výbuch kousků při rozbití.
- **Voda**: z vody se dá vylézt na břeh z mělké i hluboké vody (ponoření se počítá od nohou i těla).
- **D-pad** místo joysticku: 4 směry + úhlopříčky v rozích, více prstů najednou, nic se nezasekne.
- **Retro HUD**: tlačítka s pixelovým zkosením a zeleným stisknutím, zapuštěné sloty hotbaru.

## Co už umí (v0.6)
- **Obchod VoxelCraftu**: vlastní obsah (skiny, balíčky textur, modpacky světů) za **mince získané hraním**:
  1 mince za 10 vytěžených bloků v Přežití a 50 denně. Žádné skutečné peníze.
- **Skiny**: 7 vlastních skinů (3 zdarma, 4 v obchodě) a nahrání vlastního PNG 64×64 nebo 64×32.
  Postava je vidět v inventáři, v profilu a ve **3D při pohledu z třetí osoby** (zezadu nebo zepředu).
- **Zvuk**: zvuky (těžení, pokládání, kroky, zranění, jídlo, mince, kliknutí) a klidná hudba.
  Vše syntetizuje hra sama, žádné převzaté nahrávky.
- **Profil**: jméno hráče, skin, mince. **Úprava světa**: název, režim, souřadnice, ponechání inventáře.
- **Nové retro UI** podle návrhů (`docs/ui-concept/`): tmavé panely, zelené zvýraznění, nastavení po
  kategoriích (Dotyk, Video, Globální zdroje), zaškrtávátka, posuvníky, rozbalovací volby.
- **Balíčky a doplňky (modpacky)**: biomy, Overworld, Nether, recepty, tavení a textury v JSON.
  Upravíš je **přímo ve hře** v editoru s ověřením a formátováním, další importuješ ze `.zip`.
  Formát popisuje [docs/BALICKY.md](docs/BALICKY.md), ukázka je `examples/ohnivy_nether.zip`.
- **Biomy**: pláně, les, poušť, hory, oceán (a cokoli, co přidáš v JSON)
- **Nový svět**: herní režim, typ (nekonečný nebo plochý), seed s kostkou, souřadnice, ponechání inventáře
- **Nastavení**: citlivost, invertovaná osa Y, automatický skok, vibrace, rozložení pro leváky,
  velikost tlačítek, velikost GUI, jas, zorné pole, dohled, skrytí GUI
- **Menu jako v PE 1.1**: titulní obrazovka s otáčejícím se světem, výběr světů, vytvoření světa
  (název, seed, režim), mazání s potvrzením, nastavení (dohled, citlivost, zorné pole, balíček textur),
  pauza a ukládání. Vlastní pixelové písmo s českou diakritikou.
- **Ukládání světů**: ukládají se jen změněné chunky, zbytek se znovu generuje ze seedu.
  Automaticky každou minutu a při odchodu z aplikace.
- **Kreativní režim**: katalog všech bloků, nekonečné bloky, okamžité ničení,
  **létání** (dvakrát ťukni na skok; šipky nahoru a dolů)
- **Přežití**: srdíčka, hlad, kyslík pod vodou, poškození pádem, lávou a utonutím, smrt a oživení.
  Těžení trvá podle tvrdosti bloku a z bloků padají předměty (tráva → hlína, kámen → dlažební kámen,
  z listí občas jablko). Inventář 36 slotů s počty: ťukni na slot a pak na jiný, tím věc přesuneš.
- **Crafting**: inventář s postavičkou a mřížkou 2×2, **pracovní stůl** 3×3 (ťukni na něj).
  Recepty: prkna, klacky, pracovní stůl, pec, krumpáče, sekery a lopaty (dřevo, kámen, železo), křesadlo.
- **Pec**: surovina, palivo a výsledek. Taví i se zavřeným oknem a ukládá se se světem.
  Kámen, sklo, železné ingoty, uhlí z kmenů, netherové cihly. Paliva: uhlí, dřevo, klacky.
- **Nástroje**: zrychlují těžení a mají výdrž (ukazatel pod ikonou). Kámen a rudy dají bez krumpáče nic,
  železná ruda potřebuje aspoň kamenný krumpáč.
- **Rudy**: uhlí a železo v kameni; ze štěrku občas padne pazourek.
- Nekonečný svět z chunků 16×16×128 (stejné rozměry jako PE): kopce, pláže, moře, jeskyně, stromy
- **Nether** a **portály** z obsidiánu (zapalují se křesadlem), přepočet souřadnic 1:8
- **Balíčky textur**: klasický `terrain.png` nebo `.zip`/`.mcpack` (Bedrock i Java)
- Dotykové ovládání jako ve starém PE: D-pad, ťuknutí položí blok,
  podržení ho rozbije, „...“ otevře inventář, vpravo nahoře je pauza
- Zarovnání na 16 KB stránky (vyžaduje Android 15+)

## Sestavení
```bash
./gradlew assembleRelease        # APK: app/build/outputs/apk/release/app-release.apk
adb install -r app/build/outputs/apk/release/app-release.apk
./tests/run_tests.sh             # testy enginu, menu a herní logiky na PC (bez telefonu)
SNAPSHOTS=1 ./tests/run_tests.sh # navíc uloží obrázky všech obrazovek do build/snapshots/
```
Potřeba: JDK 17+, Android SDK 36, NDK 27.2, CMake 3.22.

## Struktura
```
app/src/main/cpp/
  main.cpp            Android vstup: EGL, smyčka, dotyky
  engine/World.*      chunky, generátor Overworldu a Netheru
  engine/Mesher.*     převod chunku na trojúhelníky (skryté stěny se vynechávají)
  engine/Renderer.*   OpenGL ES 3: shadery, mlha, UI, texture atlas
  engine/Player.*     fyzika, kolize, raycast
  engine/Portal.*     zapálení, rozpad a hledání portálů
  engine/Game.*       menu a přepínání obrazovek
  engine/Session.*    hraní jednoho světa: režimy, zdraví, hlad, těžení, HUD
  engine/Ui.*, Font.* GUI (tlačítka, posuvníky, text) a pixelové písmo
  engine/Save.*       ukládání světů a nastavení
  engine/Items.*      předměty, inventář, nástroje, tvrdost bloků a co z nich padá
  engine/Crafting.*   recepty, tavení a pec (z JSON)
  engine/Skin.*       skiny (formát 64×64) a vestavěné skiny
  engine/PlayerModel.* 3D model postavy pro pohled z třetí osoby
  engine/Profile.*    profil hráče, mince, denní bonus
  engine/Store.*      katalog obchodu a instalace položek
  engine/Audio.*      syntéza zvuků a hudby do WAV
  engine/Json.*       parser a formátovač JSON s českými chybovými hláškami
  engine/GenConfig.*  biomy, Overworld a Nether z JSON a výchozí balíček
  engine/Packs.*      správa balíčků a jejich slučování
  engine/Particles.*  částice při těžení (úlomky s texturou bloku)
  engine/Nbt.*        čtení a zápis NBT (little endian, Bedrock)
  engine/Structure.*  VCStructures: .vcstructure, import .mcstructure, umístění a vrácení
  engine/Drops.*      předměty ležící ve světě (přeplněný inventář, vyhozené věci)
  engine/FirstPerson.* ruka v první osobě a její animace
  engine/ItemModel.*  3D modely předmětů vytažené z pixelů
  engine/Lang.*       jazyky a tabulka překladů (LangTable.inc)
  engine/Minimap.*    mapa v rohu obrazovky
  engine/Developer.*  vývojářský přístup (SHA-256)
tools/legacy-apk/     cesta A: zprovoznění vlastního originálního APK
tests/                testy jádra pro PC
```

## Plán
1. Moby (prasata, zombie; v Netheru pigmeni a ghasti), den a noc
2. Lepší osvětlení (šíření světla z glowstonu a lávy), tekoucí voda a láva
3. Načítání světů z původní 1.1.2 (LevelDB)
