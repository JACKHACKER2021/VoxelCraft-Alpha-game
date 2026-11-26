<div align="center">

# ❓ VoxelCraft – FAQ

**[🇨🇿 Čeština](#cestina)** · **[🇬🇧 English](#english)**

*Hoppy_Hopsy21 · VoxelSoftworks*

🎬 **[FAQ trailer (3 min)](../trailers/t13_faq.mp4)**

</div>

---

<a id="cestina"></a>
<details open>
<summary><b>🇨🇿 Čeština</b> (klikni pro sbalení / rozbalení)</summary>

### 📱 Proč zatím není release na Androidu?

![Android release](img/faq/release.jpg)

- **Play Console:** vývojářský účet a stránka hry v obchodě Google Play ještě nejsou hotové.
- **Hledání APK:** sestavená APK se dřív špatně hledala. Teď je každá verze ke stažení ve složce
  [`release/`](../release/) tady na GitHubu.

### 🔢 Jak bude release vypadat?

![Číslování verzí](img/faq/versions.jpg)

Release zatím není. Hra je stavěná na číslování verzí z Android manifestu (`versionName`):

| Teď | |
|---|---|
| **0.1 – 2.x** | první verze až po Cornerstone a metro |
| **3.x, 4.x, 5.x** | další velké aktualizace |

V budoucnu číslování převedeme na nové, které začne znovu od **1.x.x.x**,
například `1.0.0.0`, `1.0.1.0`, `1.1.0.0`.

### 🪨 Co se bude dít s dimenzemi Cornerstone a Crimson Void před vydáním?

![Cornerstone a Crimson Void](img/faq/dimensions.jpg)

Před vydáním bude potřeba opravit mnoho chyb a bugů:

- 🌱 **Crimson Void:** chybí sazenice karmínového stromu.
- 💎 **Cornerstone:** v písku je rubínová ruda z obyčejného Overworldu.
- ⛏️ **Cornerstone** dostane **vlastní nové rudy**, protože teď používá špatné verze rud z Overworldu.
- 🐾 **Cornerstone** má **málo mobů**, přibudou noví tvorové.
- ✏️ Slovo **„Nether“** se všude úplně nahradí názvem **Cornerstone**.

### 🪙 Co bude s mincemi?

![Voxel Bucks](img/faq/bucks.jpg)

- **„Coins“ se přejmenují na „Voxel Bucks“.** Změní se i jejich tvar v menu: místo mince bude **kostička**.
- V obchodě přibude **nová sekce**, kde si Voxel Bucks můžeš **dokoupit za peníze**.
- **Free spins** (kolo štěstí) a denní odměny zůstanou zdarma jako dosud.
- **VIP** bude fungovat stejně jako kdysi, jen se **speciálním bonusem pro verze 1.x.x.x**.

### 🖥️ Jak bude hra vypadat?

![Zelené menu](img/faq/ui.jpg)

Vzhled menu se během vývoje několikrát měnil. **Současný zelený vzhled už zůstane** a dál se měnit nebude.

### 🔤 Jak to bude s písmem?

![Písmo](img/faq/fonts.jpg)

- Dřívější alpha a klasické písmo ze starších verzí **0.1 – 1.9.9** zůstane jen tam, kam patří:
  v modpacku **Pre-Alpha Style**, ve starších verzích a ve **VoxelCraft Classic**.
- Všude jinde ho nahradí **nové písmo (Tiny Caps)**.

---

## 🚀 Release

### 📅 Kdy vyjde hra na Google Play?

![Google Play 2027](img/faq/date.jpg)

Během roku **2027**. Možná **1. 1. 2027**, nebo **1. 2. 2027**, kdy bude VoxelCraft slavit **rok od vzniku**.

### 💸 Bude hra zdarma?

![Zdarma](img/faq/free.jpg)

- **Ano.** Stáhnout i hrát půjde zdarma.
- **Voxel Bucks** si můžeš i koupit za peníze.
- **Žádné reklamy.**

### 💾 Přenesou se moje světy a předměty do releasu?

![Světy](img/faq/saves.jpg)

**Ano.** Uložené světy, pevné seedy struktur i inventář zůstanou.

### 📱 Na jakých telefonech hra poběží?

![Telefony](img/faq/phones.jpg)

- Hra je pro **Android** a bude se podporovat i dál, dokud bude fungovat **Google Play**.
- Hra poběží na **Androidu 5.0 a novějším**.

### 🌐 Bude multiplayer přes internet?

![Multiplayer](img/faq/online.jpg)

**Ano, bude.** Teď se dá hrát jen přes **LAN** (hráči na stejné Wi-Fi).

### 🍎 Bude verze pro iPhone nebo PC?

![iPhone a PC](img/faq/platforms.jpg)

Do budoucna je to **v plánu**.

### 🧪 Co je preview a co normální verze?

![Normální a preview](img/faq/preview.jpg)

- **Normální verze** (teď **2.7.1**) je hotová verze. Může ji mít **kdokoli**.
- **Preview** (teď **2.8.0 Preview**) je další update, který se ještě dělá. Má ji **jen developer**.
- Obě jsou samostatné aplikace, takže můžou být v telefonu **vedle sebe**.

### 🐞 Jak nahlásit chybu?

![Nahlášení chyby](img/faq/bugs.jpg)

**Ve hře:** Nastavení → **Nahlásit chybu**. Napíšeš krátký název a popis a hra otevře GitHub s vyplněným hlášením (verze hry, svět, telefon). Stačí ho odeslat.

Nebo založ nový **Issue** přímo tady na GitHubu ([Issues](../../../issues)) a napiš:

1. **Co se stalo** a co jsi čekal(a).
2. **Jak to zopakovat**, krok po kroku.
3. **Verzi hry** a **telefon** (model a verzi Androidu).
4. Pokud můžeš, přidej **screenshot** nebo video.

### 🎥 Můžu točit videa a streamovat VoxelCraft?

![Videa](img/faq/videos.jpg)

**Ano!** Videa i streamy jsou vítané.

### 🔑 Proč se staré APK nedají nainstalovat přes nové?

![Staré a nové APK](img/faq/apk.jpg)

- Starší verze byly podepsané **jiným klíčem**. Android update s jiným klíčem nepovolí.
- Od verze **2.4.0** má hra **nové ID aplikace** (`cz.voxelsoftworks.voxelcraft`) a **trvalý klíč**.
- Každé nové APK se od 2.4.0 nainstaluje přes předchozí, bez odinstalování.

## 🎮 Hra, výkon a soubory

### 🧱 Co se dá ve VoxelCraftu dělat?

![Co se dá dělat](img/faq/play.jpg)

- **Stavět a vyrábět**: stovky bloků, nástroje, brnění, batohy, peněženka, vlaky, koleje i metro.
- **Prozkoumávat**: čtyři dimenze, vesnice, metro, ztracené město, vraky, chrámy, doly a tábory.
- **Bojovat a starat se**: tvorové, bossové Crimson Man a Sky Guardian, farma, zvířata, ochočený lišák.
- **Tvořit a hrát s ostatními**: kreativ, doplňky a Tvůrce, struktury, LAN multiplayer, vlastní skiny.

### 🦊 Jak ochočím lišáka a co umí?

![Lišák](img/faq/fox.jpg)

- Klepni na lišáka **syrovým masem** (vepřové, hovězí, skopové nebo kuřecí). Je tvůj a dostane červený obojek.
- Chodí za tebou a **kouše příšery**. Klepnutím v **plížení** si sedne a čeká, dalším klepnutím jde zase s tebou.
- Klepnutím **barvivem** obojek přebarvíš (16 barev).
- Ochočit jde i **popelavý lišák** z Cornerstone.

### 🗺️ Kde najdu metro, ztracené město a další stavby?

![Stavby na mapě](img/faq/structures.jpg)

- Na **mapě** má každá stavba svůj domeček a barvu: metro **růžové**, rozpadlé nádraží **bílé**, ztracené město **mátové**, vesnice **zelené**, vraky vlaků **červenohnědé**.
- Metro se na mapě ukáže, když jsi do **300 bloků** od něj.
- Příkaz `/locate metro` (a `/locate city`, `/locate station`…) ukáže nejbližší místo.

### 🟫 Jak funguje rezavění oceli a bronzu?

![Rez a vosk](img/faq/rust.jpg)

- Ocelové a bronzové bloky časem **zrezaví**, zhruba o stupeň za herní den.
- **Med** blok navoskuje a ten už nikdy nezrezaví.
- **Sekera** oškrábe vosk, nebo rez o jeden stupeň.

### 🌌 Jak se dostanu do všech čtyř dimenzí?

![Čtyři dimenze](img/faq/dimensions4.jpg)

- **Cornerstone**: postav rám z **obsidiánu** a zapal ho **křesadlem**.
- **Crimson Void**: v Cornerstone postav rám z **void stone** a zapal ho **void křesadlem**. Čeká tam boss **Crimson Man**.
- **Celestial Reach**: polož **12 hlav Karmínového muže** na nebeský rám. Čeká tam **Sky Guardian**.

### 🐢 Hra se mi seká, co mám dělat?

![Výkon zařízení](img/faq/performance.jpg)

- **Nastavení > Grafika > Výkon zařízení**: **Plný**, **Smoother+** nebo **Slabé**. Hra ho nastaví sama podle grafického čipu a paměti.
- **Slabé** má lehkou grafiku, 30 FPS a kratší dohled. Hodí se třeba pro Teclast P30T.
- **Smoother+** zkrátí dohled a nechá shadery, dokud je telefon stíhá. Hodí se pro Redmi Note 13 5G a Galaxy A14.
- **Úspora baterie** se zapne sama při 35 % a méně.

### ✨ Proč se mi vypnuly shadery?

![Shadery](img/faq/shaders.jpg)

- Hra po vstupu do světa nebo dimenze **20 sekund počká**, než se svět načte. Pak, když dvakrát za sebou **10 sekund drží pod 20 FPS**, shadery se **samy vypnou** a ukáže se zpráva (od 2.7.1; ve 2.7.0 se vypínaly moc brzy).
- V **Nastavení > Grafika** je zapneš znovu tlačítkem **Zkusit znovu**, nebo přepnutím Grafiky na Vylepšenou.

### 💾 Jak zálohuju a přenesu svět?

![Záloha světa](img/faq/backup.jpg)

- V seznamu světů **Upravit > Exportovat svět** uloží celý svět do jednoho souboru **.vcworld**.
- Na jiném telefonu nebo po přeinstalaci ho přidáš přes **Import**.
- **Aktualizace hry uložené světy nemění.** Nové stavby a rudy se objeví jen v nových světech.

### ⛏️ Můžu si otevřít svět z Minecraftu?

![Svět z Minecraftu](img/faq/minecraft.jpg)

- Ano, bloky se převedou na naše: deepslate na hlubinný kámen, redstone na powerstone, měď na bronz, quartz na bílý beton.
- Schody a desky si drží směr a ploché světy zůstanou ploché.
- Tvorové, obsah truhel a dveře se **zatím nepřenesou**.

### 🧩 Proč mód z Bedrocku nebo Javy nefunguje úplně?

![Co se převede](img/faq/partial.jpg)

- Převést jde **data a vzhled**: bloky, předměty, tvory, recepty, textury a atlasy.
- **Kód** (třídy Javy, skripty Bedrocku) VoxelCraft spustit neumí, a tak se přeskočí.
- **UI** cizích módů se nepřebírá. Takový doplněk má štítek **un-UI**.
- Co přesně se stalo, ukáže tlačítko **Chyby** ve Správci doplňků.

### 🖼️ Jak udělám vlastní texture pack?

![Texture pack](img/faq/texturepack.jpg)

- Nakresli obrázky **PNG 16×16**, jeden na blok nebo předmět. Dej je do složek `blocks/` a `items/`, nebo udělej jeden starý `terrain.png`.
- Zabal je do **.zip**, **.mcpack** nebo **.vcpack**.
- Nahraješ je v **Nastavení > Globální zdroje** nebo tlačítkem **+ Importovat** ve Správci doplňků. Co chybí, zůstane v našem vzhledu.

### 🛡️ Je modování bezpečné?

![Bezpečné modování](img/faq/safe.jpg)

- Doplňky VoxelCraftu jsou **jen data a obrázky**. Žádný kód se nespouští.
- Doplněk nemůže nic posílat na internet.
- Každý doplněk jde kdykoli **vypnout** nebo **smazat**.

### 📂 Co jsou .vcworld, .vcr, .vct, .vcstructure, .vcpack a .vcaddon?

![Soubory VoxelCraftu](img/faq/files.jpg)

| Soubor | Co to je | Jak to funguje |
|---|---|---|
| **.vcworld** | celý svět v jednom souboru | zip se světem: `level.dat`, tvorové, truhly, regiony; vznikne exportem, nahraje se importem |
| **.vcr** | region světa (32 × 32 chunků) | binární a komprimovaný; tak hra ukládá svět v telefonu |
| **.vct** | stejný region jako čitelný text | ve .vcworld místo .vcr; jde otevřít a přečíst, při importu se převede zpět na .vcr |
| **.vcstructure** | uložená stavba | ukládá ji **structure block**, jde ji vložit do jiného světa (a umíme i `.mcstructure`) |
| **.vcpack** | doplněk jen se **vzhledem** (Resource Voxel) | zip s `manifest.json` a texturami, atlasy, modely nebo shadery |
| **.vcaddon** | doplněk se **vzhledem i chováním** | zip s `manifest.json`, `content.json` (bloky, předměty, tvorové), recepty a texturami |

## 🛠️ Vývoj

### 🔁 Jak vzniká nová verze?

![Jak vzniká verze](img/faq/versions_made.jpg)

1. **Nápady**: co se má změnit, se nejdřív nakreslí jako návrh a schválí.
2. **Preview buildy**: každý build se otestuje, nahraje na GitHub a dostane číslo (build 1, 2, 3…).
3. **Vydání**: hotové APK jde do `release/` pro všechny a preview se posune na další verzi.
4. **Archiv**: stará hudba, UI a textury zůstávají v `archived/` a staré verze v `release/`.

### 🚫 Proč hra nepoužívá nic od Mojangu?

![Vlastní hra](img/faq/own.jpg)

- VoxelCraft má **vlastní engine** v C++ a OpenGL ES.
- Každou texturu **kreslí kód hry** pixel po pixelu a hudba je naše vlastní.
- Je to neoficiální fanouškovský projekt, který nechce nic kopírovat.

### 🔮 Co bude ve 3.0.0?

![3.0.0](img/faq/v300.jpg)

- **Železné věci zmizí** a ocel je nahradí natrvalo.
- Plán velkého updatu zveřejníme, až bude hotový.
- Tvoje světy zůstanou, jak jsou.

## 🛠️ Změny ve 2.7.0

### ⛓️ Proč bylo odstraněno železo?

![Ingoty a hardiová ruda](img/faq/iron.jpg)

- Železo se od 2.7.0 **na nic nevyrábí**. Ve všech receptech je místo něj **ocel**.
- Železný ingot dáš s uhlím do výroby a vznikne **ocelová směs**. V peci z ní je ocel.
- Železné nástroje a brnění se už nevyrábí. Úplně zmizí ve **3.0.0**. VoxelCraft je vlastní hra a nechce kopírovat cizí sady nástrojů.
- Železo mělo hezčí barvu, a tak ji dostala **ocelová sada**: krumpáč, sekera, lopata, meč i brnění.

### 🎨 Proč většina věcí dostala nový vzhled?

![Nové textury 2.7.0](img/faq/redesign.jpg)

- Hra roste a staré obrázky z prvních verzí vedle nových věcí působily chudě.
- Ve 2.7.0 se překreslila **vajíčka** (každý tvor má svůj vzor), **maso**, **šíp**, **provázek**, **ingoty**, **hardiová ruda**, **bronzové dveře** a **zarážedlo na koleji**.
- Staré textury jsou schované v `archived/ui_2.6.0/` a hrají dál ve verzi 2.6.0, třeba na natáčení.

### 🖌️ Proč byly textury dřív tak jednoduché?

![Textury do 2.6.0](img/faq/old_textures.jpg)

- VoxelCraft nepoužívá žádné obrázky od Mojangu. Všechny textury **kreslí přímo kód hry** pixel po pixelu.
- V prvních verzích šlo hlavně o to, aby věci fungovaly. Obrázky byly rychlé, s pár barvami a tečkami.
- Teď je čas je vyladit: stínování, obrys, odlesk a vzor, který je hned poznat.

### 🎒 Jak fungují batohy?

![Batohy na zádech](img/faq/backpacks.jpg)

- Šest úrovní: **kůže, ocel, ocelobronz, bronzorubín, hardium, celestium**. Od 9 do 54 míst.
- Uvnitř je **výroba** (2×2, od ocelobronzu 3×3) a od bronzorubínu i **pec**. Tlačítka jsou v řádku pod místy batohu.
- Batoh polož do slotu pro hrudní brnění: přesune se **na záda** a brnění můžeš nasadit přes něj. Ve hře ho otevřeš tlačítkem nad „…“.
- Vylepšení: batoh doprostřed a kolem 4× kov další úrovně. **Obsah zůstane.**

![Okno batohu](img/faq/backpack_window.jpg)

### 🧩 Jak funguje nové modování?

![Správce doplňků](img/faq/addons.jpg)

- Menu > **Módy a balíčky** otevře **Správce doplňků** se záložkami **Doplňky, Behavior, Resource, Tvůrce, Obchod**.
- Barevný proužek ukazuje původ: zelená VC, oranžová Java, modrá Bedrock, zlatá Obchod, červená chyba.
- **+ Importovat** vezme `.vcaddon`, `.vcpack`, `.mcaddon`, `.mcpack`, `.jar` i `.zip`.
- **Behavior Voxel** je chování (bloky, předměty, moby, recepty, svět), **Resource Voxel** je vzhled (textury, atlasy, modely, shadery).

### 🔄 Co se stane s módem z Bedrocku nebo Javy?

![Převod Bedrock > VC](img/faq/bedrock.jpg)

- Hra ho **převede do VC jazyka**: bloky, předměty, moby, textury, atlasy `terrain_texture.json` a `item_texture.json`, z `geo.json` pozná tělo tvora.
- Ke každému souboru napíše, co se stalo: **+ převedeno, ! částečně, x nejde**. Najdeš to pod tlačítkem **Chyby**. Nic už neselže potichu.
- **Kód** (Java třídy, Bedrock skripty) spustit nejde a **UI** cizích módů se nepřebírá. Takový doplněk má štítek **un-UI**.

### 🖌️ Jak si udělám vlastní blok nebo moba?

![Tvůrce](img/faq/creator.jpg)

- Záložka **Tvůrce**: přepni **Blok** nebo **Mob**, napiš název, nastav tvrdost a světlo (blok) nebo tělo a životy (mob).
- Namaluj texturu 16×16 **štětcem** nebo **výplní** z 12 barev a stiskni **Uložit do .vcaddon**.
- Všechno se uloží do doplňku **Moje tvorba** a hned je ve hře.

### 🐾 Jaké chyby měli tvorové a kaktusy?

- Tvorové brali **napadaný sníh** jako celý blok a vylézali na něj. Teď jím projdou.
- V **lávě** dřív poskakovali, protože je láva odhazovala jako úder. Teď do ní zapadnou a pálí je. Žhavá můra, popelavý lišák a prasák lávu snesou.
- **Kaktusy** jednu dobu nepíchaly. Opraveno ve 2.5.0.
- Tyhle chyby se rády vrací. Když uvidíš tvora levitovat, skákat v lávě nebo kaktus, který nepíchá, **nahlaš to přes Nahlásit chybu** v nastavení (otevře GitHub Issues).


## 📖 Encyklopedie hry (2.7.1)

Celá hra na jednom místě: dimenze, biomy, stavby, mechaniky, tvorové, všechny bloky a předměty s recepty, úspěchy a příkazy. Vygenerováno přímo z dat hry (`tools/encyclopedia_doc.cpp`), takže sedí s tím, co ve hře je.

**Obsah:**

- Dimenze
- Biomy
- Stavby
- Mechaniky
- Bloky a předměty
- Moby
- Recepty a suroviny
- Úspěchy
- Příkazy

### Dimenze

#### <img src="img/enc/icons/2.png" width="24"> Overworld

<img src="img/enc/plains.jpg" width="384">

Výchozí svět. Biomy od plání po zasněžené hory, jeskyně a hlubinný kámen, vesnice, doly, tábory a další stavby.

#### <img src="img/enc/icons/87.png" width="24"> Dimenze úhelného kamene

<img src="img/enc/cornerstone.jpg" width="384">

Dimenze za obsidiánovým portálem: úhelný kámen, lávová moře, powerstonová pustina, hogmani a velká pevnost s portálem do Karmínové prázdnoty.

#### <img src="img/enc/icons/14.png" width="24"> Karmínová prázdnota

<img src="img/enc/void_purple.jpg" width="384">

Fialový les s očními stromy, karmínová poušť, lávové pláně s arénou Karmínového muže, Zapomenutý Overworld a tři nové biomy.

#### <img src="img/enc/icons/1560.png" width="24"> Nebeská říše

<img src="img/enc/celestial.jpg" width="384">

Plovoucí ostrovy nad bílou oblohou, spojené mosty. Vstup otevře 12 hlav Karmínového muže v rámu portálu. Pád dolů je konečný.

#### <img src="img/enc/icons/2.png" width="24"> Starý svět

<img src="img/enc/old_world.jpg" width="384">

Typ světa s generací verzí 0.1–0.3: starý terén, textury, ovládání s joystickem a menu. Jen kreativní, bez mobů a hudby.

### Biomy

#### <img src="img/enc/icons/2.png" width="24"> Pláně

<img src="img/enc/plains.jpg" width="384">

Otevřená zelená krajina s květinami a ojedinělými duby.

#### <img src="img/enc/icons/1565.png" width="24"> Strakatý les

<img src="img/enc/dappled.jpg" width="384">

Šedé stromy s červeným, oranžovým a žlutým listím, oranžová tráva. Většinou sněží, vzácně slabě prší. Často v něm stojí tábory.

#### <img src="img/enc/icons/159.png" width="24"> Hory

<img src="img/enc/mountains.jpg" width="384">

Vysoké kopce s tvrzeným kamenem na srázech a sněhem na vrcholech.

#### <img src="img/enc/icons/17.png" width="24"> Les

<img src="img/enc/forest.jpg" width="384">

Hustý dubový les, ve stínu kapradí a houby. Dobré místo na dřevo.

#### <img src="img/enc/icons/12.png" width="24"> Poušť

<img src="img/enc/desert.jpg" width="384">

Písek, kaktusy a pískovec. Stojí v ní pouštní chrámy.

#### <img src="img/enc/icons/162.png" width="24"> Zasněžená tajga

<img src="img/enc/taiga.jpg" width="384">

Smrkové lesy pod sněhem. Sníh tu padá a zůstává ležet.

#### <img src="img/enc/icons/79.png" width="24"> Ledové pláně

<img src="img/enc/ice.jpg" width="384">

Zamrzlá krajina se zamrzlou vodou a iglú.

#### <img src="img/enc/icons/21.png" width="24"> Fialový les

<img src="img/enc/void_purple.jpg" width="384">

Les Karmínové prázdnoty: prázdnotná tráva, stromy s okem v kmeni.

#### <img src="img/enc/icons/23.png" width="24"> Karmínová poušť

<img src="img/enc/void_crimson.jpg" width="384">

Duny rudého písku, skalní věže z karmínového kamene, krystaly a zasypané ruiny.

#### <img src="img/enc/icons/1978.png" width="24"> Crimsoniový les

<img src="img/enc/crimsonium.jpg" width="384">

Hustý les rudých crimsoniových stromů. Pod zemí vzácná crimsoniová ruda, mezi stromy lesní svatyně s lootem.

#### <img src="img/enc/icons/1981.png" width="24"> Roztříštěná prázdnota

<img src="img/enc/shattered.jpg" width="384">

Rozlámané skály s puklinami, skalní oblouky, rozpadlé věže a krystaly prázdnotných střepů.

#### <img src="img/enc/icons/1984.png" width="24"> Poušť prázdnotných střepů

<img src="img/enc/shard_desert.jpg" width="384">

Temné duny se shluky krystalů, zasypané ruiny a skryté komory pod pískem, prozrazené jen krystalem na sloupku.

### Stavby

#### <img src="img/enc/icons/1575.png" width="24"> Tábor

<img src="img/enc/camp.jpg" width="384">

Stan z vlny, ohniště a barel se zásobami. Nejčastěji ve Strakatém lese.

#### <img src="img/enc/icons/1557.png" width="24"> Mosty Nebeské říše

<img src="img/enc/bridges.jpg" width="384">

První stavby Nebeské říše: rovné mosty mezi ostrovy se zábradlím, sloupky a řetězy.

#### <img src="img/enc/icons/1988.png" width="24"> Lesní svatyně

<img src="img/enc/shrine.jpg" width="384">

Sloupy z crimsonia, střecha z desek a krystal nahoře. Truhla s crimsoniem a střepy.

#### <img src="img/enc/icons/1989.png" width="24"> Skrytá komora

<img src="img/enc/shard_desert.jpg" width="384">

Místnost z temného pískovce 13 bloků pod dunami. Hledej krystal na sloupku.

#### <img src="img/enc/icons/153.png" width="24"> Vesnice

<img src="img/enc/village.jpg" width="384">

Kruhová osada s domy, studnou, cestami a poli pšenice. Žijí v ní vesničané, se kterými můžeš obchodovat.

#### <img src="img/enc/icons/4.png" width="24"> Zničená vesnice

<img src="img/enc/ruined_village.jpg" width="384">

Opuštěná vesnice s rozpadlými domy a zarostlými cestami. Žádní vesničané, ale truhly zůstaly.

#### <img src="img/enc/icons/50.png" width="24"> Zřícenina domu

<img src="img/enc/ruin.jpg" width="384">

Pobořené kamenné a dřevěné zdi na pláních a v lesích. Uvnitř truhla s kořistí.

#### <img src="img/enc/icons/51.png" width="24"> Ztroskotaná loď

<img src="img/enc/shipwreck.jpg" width="384">

Vrak dřevěné lodi na dně moře nebo na břehu. Truhla s mapami, jídlem a železem.

#### <img src="img/enc/icons/52.png" width="24"> Kobka

<img src="img/enc/dungeon.jpg" width="384">

Místnost z mechových kamenných cihel hluboko pod zemí s truhlou.

#### <img src="img/enc/icons/1548.png" width="24"> Jeskynní kobka

<img src="img/enc/cave_dungeon.jpg" width="384">

Malá místnost ve skále s líhní příšer a kořistí. Líheň znič, než tě přemůže.

#### <img src="img/enc/icons/53.png" width="24"> Opuštěný důl

Dlouhé dřevěné chodby pod zemí, spojené s jeskyněmi. Truhly s rudami a průzkumnými mapami.

#### <img src="img/enc/icons/94.png" width="24"> Pouštní chrám

<img src="img/enc/desert_temple.jpg" width="384">

Pyramida z pískovce v poušti. Uvnitř skrytá truhla.

#### <img src="img/enc/icons/95.png" width="24"> Iglú

<img src="img/enc/igloo.jpg" width="384">

Sněhová kopule na ledových pláních a v tajze, s truhlou a kobercem.

#### <img src="img/enc/icons/96.png" width="24"> Strážní věž

<img src="img/enc/watchtower.jpg" width="384">

Vysoká věž s dřevěnou plošinou nahoře a truhlou. Krásný výhled do okolí.

#### <img src="img/enc/icons/55.png" width="24"> Velká pevnost

<img src="img/enc/fortress.jpg" width="384">

Obří pevnost z nether cihel v Dimenzi úhelného kamene: archiv, kovárna, trůnní sál a krypta. V hale portál do Karmínové prázdnoty.

#### <img src="img/enc/icons/1515.png" width="24"> Zapomenutá vesnice

<img src="img/enc/forgotten_village.jpg" width="384">

Opuštěná vesnice zapomenutého powerstonového království v pustině, se sloupem lamp uprostřed.

#### <img src="img/enc/icons/1516.png" width="24"> Potopené ruiny

<img src="img/enc/drowned_ruins.jpg" width="384">

Zbytky staveb zapomenutého království pod hladinou jezer a moří.

#### <img src="img/enc/icons/1517.png" width="24"> Rozbitý portál

<img src="img/enc/broken_portal.jpg" width="384">

Ruina starého portálu. Ukazuje cestu k pevnosti v Dimenzi úhelného kamene.

#### <img src="img/enc/icons/1532.png" width="24"> Karmínový důl

Opuštěné chodby z karmínového dřeva v Dimenzi úhelného kamene.

#### <img src="img/enc/icons/93.png" width="24"> Kobka prázdnoty

<img src="img/enc/void_dungeon.jpg" width="384">

Místnost z prázdnotných cihel pod zemí Karmínové prázdnoty.

#### <img src="img/enc/icons/26.png" width="24"> Věž prázdnoty

<img src="img/enc/void_tower.jpg" width="384">

Vysoká temná věž v Karmínové prázdnotě s truhlou a kompasem k aréně.

#### <img src="img/enc/icons/39.png" width="24"> Strážní věž prázdnoty

<img src="img/enc/void_watchtower.jpg" width="384">

Strážní věž z prázdnotného dřeva nad fialovým lesem.

#### <img src="img/enc/icons/1525.png" width="24"> Aréna Karmínového muže

<img src="img/enc/arena.jpg" width="384">

Kruhová aréna nad lávou se sloupy s očima. Tady čeká boss Karmínový muž.

#### <img src="img/enc/icons/1552.png" width="24"> Rám portálu do Nebeské říše

<img src="img/enc/portal.jpg" width="384">

Dvanáct rámů v Zapomenutém Overworldu. Vlož do nich 12 hlav Karmínového muže a portál se otevře. Ukazuje na ně nebeský kompas.

#### <img src="img/enc/icons/1579.png" width="24"> Nebeský chrám

<img src="img/enc/temple.jpg" width="384">

Ostrov z bílého kamene s kruhem dvanácti sloupů s krystaly. Na pódiu čeká boss Strážce Nebe, který dá Nebeská křídla.

### Mechaniky

#### <img src="img/enc/icons/1946.png" width="24"> Beton

<img src="img/enc/concrete.jpg" width="384">

Betonový prášek v 16 barvách padá jako písek. Ve vodě nebo v dešti ztvrdne na beton. Šedý prašný beton se vypálí v peci.

#### <img src="img/enc/icons/1628.png" width="24"> Barviva

Z květin, listí, uhlí, hlíny, kaktusu a powerstonu; další barvy smícháním. Barví vlnu a beton.

#### <img src="img/enc/icons/1570.png" width="24"> Kamenořez a dřevořez

<img src="img/enc/cutters.jpg" width="384">

Kamenořez dělá z bloků schody a desky bez ztrát. Dřevořez dává 6 prken z kmene, schody, desky a ploty.

#### <img src="img/enc/icons/49.png" width="24"> Portály

<img src="img/enc/portal.jpg" width="384">

Obsidiánový portál vede do Dimenze úhelného kamene, portál v pevnosti do Karmínové prázdnoty, rám s 12 hlavami do Nebeské říše.

### Moby

| | Tvor | Životy | Útok | Druh |
|---|---|---|---|---|
| <img src="img/enc/icons/196.png" width="24"> | **Prase** | 10 | 0 | mírumilovný |
| <img src="img/enc/icons/197.png" width="24"> | **Ovce** | 8 | 0 | mírumilovný |
| <img src="img/enc/icons/198.png" width="24"> | **Dýňový sliz** | 12 | 2 | nepřátelský |
| <img src="img/enc/icons/199.png" width="24"> | **Zapomenutý muž** | 20 | 3 | nepřátelský |
| <img src="img/enc/icons/1619.png" width="24"> | **Karmínový muž** | 300 | 9 | boss |
| <img src="img/enc/icons/155.png" width="24"> | **Slepice** | 4 | 0 | mírumilovný |
| <img src="img/enc/icons/156.png" width="24"> | **Kráva** | 10 | 0 | mírumilovný |
| <img src="img/enc/icons/157.png" width="24"> | **Vesničan** | 20 | 0 | mírumilovný |
| <img src="img/enc/icons/1303.png" width="24"> | **Pavouk** | 16 | 2 | nepřátelský |
| <img src="img/enc/icons/1304.png" width="24"> | **Netopýr** | 6 | 0 | mírumilovný |
| <img src="img/enc/icons/1305.png" width="24"> | **TNT zombie** | 20 | 0 | nepřátelský |
| <img src="img/enc/icons/1306.png" width="24"> | **Dehtová příšera** | 30 | 5 | nepřátelský |
| <img src="img/enc/icons/1307.png" width="24"> | **Husking** | 24 | 4 | nepřátelský |
| <img src="img/enc/icons/1308.png" width="24"> | **Prasák** | 22 | 5 | nepřátelský |
| <img src="img/enc/icons/60289.png" width="24"> | **Celestling** | 8 | 0 | mírumilovný |
| <img src="img/enc/icons/60290.png" width="24"> | **Reach Stalker** | 26 | 5 | nepřátelský |
| <img src="img/enc/icons/60291.png" width="24"> | **Sky Drifter** | 14 | 0 | mírumilovný |
| <img src="img/enc/icons/1996.png" width="24"> | **Strážce Nebe** | 260 | 8 | boss |
| <img src="img/enc/icons/60292.png" width="24"> | **Úhelný golem** | 60 | 7 | mírumilovný |
| <img src="img/enc/icons/60293.png" width="24"> | **Mechový plaz** | 16 | 3 | nepřátelský |
| <img src="img/enc/icons/60294.png" width="24"> | **Sporovka** | 8 | 0 | mírumilovný |
| <img src="img/enc/icons/60295.png" width="24"> | **Žhavá můra** | 4 | 0 | mírumilovný |
| <img src="img/enc/icons/60296.png" width="24"> | **Popelavý lišák** | 12 | 0 | mírumilovný |
| <img src="img/enc/icons/60297.png" width="24"> | **Lišák** | 12 | 0 | mírumilovný |
| <img src="img/enc/icons/60298.png" width="24"> | **Duch** | 10 | 0 | mírumilovný |
| <img src="img/enc/icons/60299.png" width="24"> | **Sněhulák** | 20 | 0 | mírumilovný |

### Bloky a předměty

#### <img src="img/enc/icons/98.png" width="24"> Stavební bloky (189)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/1.png" width="24"> | Kámen | pec: Dlažební kámen |
| <img src="img/enc/icons/4.png" width="24"> | Dlažební kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/48.png" width="24"> | Mechový dlažební kámen | výroba: Dlažební kámen + Dubové listí |
| <img src="img/enc/icons/98.png" width="24"> | Kamenné cihly | výroba: 4× Kámen → 4× |
| <img src="img/enc/icons/45.png" width="24"> | Cihly | pec: Jíl |
| <img src="img/enc/icons/5.png" width="24"> | Dubová prkna | výroba: Dubový kmen → 4×<br>výroba: Smrková prkna<br>výroba: Prázdnotná prkna |
| <img src="img/enc/icons/17.png" width="24"> | Dubový kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/24.png" width="24"> | Pískovec | výroba: 4× Písek |
| <img src="img/enc/icons/49.png" width="24"> | Obsidián | těžba, nález nebo kreativ |
| <img src="img/enc/icons/184.png" width="24"> | Ocelový blok | výroba: 9× Ocelový ingot |
| <img src="img/enc/icons/60155.png" width="24"> | Tesaný úhelný kámen | výroba: 4× Úhelný kámen → 4× |
| <img src="img/enc/icons/60156.png" width="24"> | Zlacená dlažba | výroba: 5× Úhelný kámen + 4× Bronzový ingot → 5× |
| <img src="img/enc/icons/60157.png" width="24"> | Jantarová pryskyřice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60159.png" width="24"> | Prizmatický sloup | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60160.png" width="24"> | Prachovec | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60161.png" width="24"> | Světélkující sůl | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60164.png" width="24"> | Sporová podložka | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60165.png" width="24"> | Zkamenělá kůra | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60166.png" width="24"> | Mechový úhelný kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60171.png" width="24"> | Praskající led | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60172.png" width="24"> | Ledové cihly | výroba: 4× Led → 4× |
| <img src="img/enc/icons/60173.png" width="24"> | Blok sněhových vloček | výroba: 5× Sněhový blok → 2× |
| <img src="img/enc/icons/60174.png" width="24"> | Suchý sníh | těžba, nález nebo kreativ |
| <img src="img/enc/icons/185.png" width="24"> | Rubínový blok | výroba: 9× Rubín |
| <img src="img/enc/icons/160.png" width="24"> | Smrková prkna | výroba: Smrkový kmen → 4× |
| <img src="img/enc/icons/162.png" width="24"> | Smrkový kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/159.png" width="24"> | Tvrdý kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/26.png" width="24"> | Prázdnotné cihly | výroba: 4× Prázdnotný kámen → 4× |
| <img src="img/enc/icons/39.png" width="24"> | Prázdnotná prkna | výroba: Prázdnotný kmen → 4× |
| <img src="img/enc/icons/36.png" width="24"> | Netherperková prkna | výroba: Netherperkový kmen → 4× |
| <img src="img/enc/icons/1000.png" width="24"> | Dubové schody | výroba: 6× Dubová prkna → 4× |
| <img src="img/enc/icons/1008.png" width="24"> | Smrkové schody | výroba: 6× Smrková prkna → 4× |
| <img src="img/enc/icons/1016.png" width="24"> | Prázdnotné schody | výroba: 6× Prázdnotná prkna → 4× |
| <img src="img/enc/icons/1024.png" width="24"> | Netherperkové schody | výroba: 6× Netherperková prkna → 4× |
| <img src="img/enc/icons/1032.png" width="24"> | Schody z dlažebního kamene | výroba: 6× Dlažební kámen → 4× |
| <img src="img/enc/icons/1040.png" width="24"> | Kamenné schody | výroba: 6× Kámen → 4× |
| <img src="img/enc/icons/1048.png" width="24"> | Schody z kamenných cihel | výroba: 6× Kamenné cihly → 4× |
| <img src="img/enc/icons/1056.png" width="24"> | Schody z mechového kamene | výroba: 6× Mechový dlažební kámen → 4× |
| <img src="img/enc/icons/1064.png" width="24"> | Cihlové schody | výroba: 6× Cihly → 4× |
| <img src="img/enc/icons/1072.png" width="24"> | Pískovcové schody | výroba: 6× Pískovec → 4× |
| <img src="img/enc/icons/1080.png" width="24"> | Schody z cihel úhelného kamene | výroba: 6× Cihly z úhelného kamene → 4× |
| <img src="img/enc/icons/1088.png" width="24"> | Schody z prázdnotných cihel | výroba: 6× Prázdnotné cihly → 4× |
| <img src="img/enc/icons/1100.png" width="24"> | Dubová deska | výroba: 3× Dubová prkna → 6× |
| <img src="img/enc/icons/1103.png" width="24"> | Smrková deska | výroba: 3× Smrková prkna → 6× |
| <img src="img/enc/icons/1106.png" width="24"> | Prázdnotná deska | výroba: 3× Prázdnotná prkna → 6× |
| <img src="img/enc/icons/1109.png" width="24"> | Netherperková deska | výroba: 3× Netherperková prkna → 6× |
| <img src="img/enc/icons/1112.png" width="24"> | Deska z dlažebního kamene | výroba: 3× Dlažební kámen → 6× |
| <img src="img/enc/icons/1115.png" width="24"> | Kamenná deska | výroba: 3× Kámen → 6× |
| <img src="img/enc/icons/1118.png" width="24"> | Deska z kamenných cihel | výroba: 3× Kamenné cihly → 6× |
| <img src="img/enc/icons/1121.png" width="24"> | Deska z mechového kamene | výroba: 3× Mechový dlažební kámen → 6× |
| <img src="img/enc/icons/1124.png" width="24"> | Cihlová deska | výroba: 3× Cihly → 6× |
| <img src="img/enc/icons/1127.png" width="24"> | Pískovcová deska | výroba: 3× Pískovec → 6× |
| <img src="img/enc/icons/1130.png" width="24"> | Deska z cihel úhelného kamene | výroba: 3× Cihly z úhelného kamene → 6× |
| <img src="img/enc/icons/1133.png" width="24"> | Deska z prázdnotných cihel | výroba: 3× Prázdnotné cihly → 6× |
| <img src="img/enc/icons/1201.png" width="24"> | Ztuhlý dehet | výroba: 4× Dehtová koule |
| <img src="img/enc/icons/1160.png" width="24"> | Karmínové schody | výroba: 6× Karmínová prkna → 4× |
| <img src="img/enc/icons/1168.png" width="24"> | Deepstonové schody | výroba: 6× Hlubinný kámen → 4× |
| <img src="img/enc/icons/1176.png" width="24"> | Schody z deepstonových cihel | výroba: 6× Cihly z hlubinného kamene → 4× |
| <img src="img/enc/icons/1184.png" width="24"> | Schody z mechových deepstonových cihel | výroba: 6× Mechové cihly z hlubinného kamene → 4× |
| <img src="img/enc/icons/1148.png" width="24"> | Karmínová deska | výroba: 3× Karmínová prkna → 6× |
| <img src="img/enc/icons/1151.png" width="24"> | Deepstonová deska | výroba: 3× Hlubinný kámen → 6× |
| <img src="img/enc/icons/1154.png" width="24"> | Deska z deepstonových cihel | výroba: 3× Cihly z hlubinného kamene → 6× |
| <img src="img/enc/icons/1157.png" width="24"> | Deska z mechových deepstonových cihel | výroba: 3× Mechové cihly z hlubinného kamene → 6× |
| <img src="img/enc/icons/1513.png" width="24"> | Popraskané cihly z úhelného kamene | pec: Cihly z úhelného kamene |
| <img src="img/enc/icons/1514.png" width="24"> | Zdobené cihly z úhelného kamene | výroba: 2× Deska z cihel úhelného kamene |
| <img src="img/enc/icons/1525.png" width="24"> | Oko v prázdnotných cihlách | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1532.png" width="24"> | Karmínová prkna | výroba: Karmínový kmen → 4× |
| <img src="img/enc/icons/1543.png" width="24"> | Cihly z hlubinného kamene | výroba: 4× Hlubinný kámen → 4× |
| <img src="img/enc/icons/1544.png" width="24"> | Popraskané cihly z hlubinného kamene | pec: Cihly z hlubinného kamene |
| <img src="img/enc/icons/1545.png" width="24"> | Mechové cihly z hlubinného kamene | výroba: Cihly z hlubinného kamene + Liány |
| <img src="img/enc/icons/1546.png" width="24"> | Shnilá prkna | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1552.png" width="24"> | Rám nebeského portálu | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1557.png" width="24"> | Nebeská prkna | výroba: Nebeský kmen → 4× |
| <img src="img/enc/icons/1558.png" width="24"> | Nebeské kamenné cihly | výroba: 4× Nebeský duševní kámen → 4× |
| <img src="img/enc/icons/1563.png" width="24"> | Prašný beton | výroba: Písek + Písek + Štěrk + Štěrk → 4× |
| <img src="img/enc/icons/1564.png" width="24"> | Tvrzený beton | pec: Prašný beton |
| <img src="img/enc/icons/1566.png" width="24"> | Šedá prkna | výroba: Šedý kmen → 4× |
| <img src="img/enc/icons/1720.png" width="24"> | Schody z prašného betonu | výroba: 6× Prašný beton → 4× |
| <img src="img/enc/icons/1728.png" width="24"> | Schody z tvrzeného betonu | výroba: 6× Tvrzený beton → 4× |
| <img src="img/enc/icons/1736.png" width="24"> | Šedé dřevěné schody | výroba: 6× Šedá prkna → 4× |
| <img src="img/enc/icons/1744.png" width="24"> | Nebeské schody | výroba: 6× Nebeská prkna → 4× |
| <img src="img/enc/icons/1752.png" width="24"> | Schody z nebeských cihel | výroba: 6× Nebeské kamenné cihly → 4× |
| <img src="img/enc/icons/1700.png" width="24"> | Deska z prašného betonu | výroba: 3× Prašný beton → 6× |
| <img src="img/enc/icons/1703.png" width="24"> | Deska z tvrzeného betonu | výroba: 3× Tvrzený beton → 6× |
| <img src="img/enc/icons/1706.png" width="24"> | Šedá dřevěná deska | výroba: 3× Šedá prkna → 6× |
| <img src="img/enc/icons/1709.png" width="24"> | Nebeská deska | výroba: 3× Nebeská prkna → 6× |
| <img src="img/enc/icons/1712.png" width="24"> | Deska z nebeských cihel | výroba: 3× Nebeské kamenné cihly → 6× |
| <img src="img/enc/icons/1570.png" width="24"> | Kamenořez | výroba: 3× Kámen + 1× Ocelový ingot |
| <img src="img/enc/icons/1572.png" width="24"> | Dřevořez | výroba: 3× Dubová prkna + 1× Ocelový ingot |
| <img src="img/enc/icons/1571.png" width="24"> | Barel | výroba: 6× Dubová prkna + 2× Dubová deska |
| <img src="img/enc/icons/1581.png" width="24"> | Táborák | výroba: 3× Dubový kmen + 3× Klacek + 1× Uhlí |
| <img src="img/enc/icons/1583.png" width="24"> | Medový blok | výroba: 4× Medová kulička |
| <img src="img/enc/icons/1979.png" width="24"> | Crimsoniová prkna | výroba: Crimsoniový kmen → 4× |
| <img src="img/enc/icons/1982.png" width="24"> | Cihly z roztříštěného kamene | výroba: 4× Roztříštěný kámen → 4× |
| <img src="img/enc/icons/1985.png" width="24"> | Temný pískovec | výroba: 4× Temný písek |
| <img src="img/enc/icons/1660.png" width="24"> | Crimsoniové schody | výroba: 6× Crimsoniová prkna → 4× |
| <img src="img/enc/icons/1668.png" width="24"> | Schody z roztříštěného kamene | výroba: 6× Roztříštěný kámen → 4× |
| <img src="img/enc/icons/1676.png" width="24"> | Schody z cihel roztříštěného kamene | výroba: 6× Cihly z roztříštěného kamene → 4× |
| <img src="img/enc/icons/1684.png" width="24"> | Schody z temného pískovce | výroba: 6× Temný pískovec → 4× |
| <img src="img/enc/icons/1646.png" width="24"> | Crimsoniová deska | výroba: 3× Crimsoniová prkna → 6× |
| <img src="img/enc/icons/1649.png" width="24"> | Deska z roztříštěného kamene | výroba: 3× Roztříštěný kámen → 6× |
| <img src="img/enc/icons/1652.png" width="24"> | Deska z cihel roztříštěného kamene | výroba: 3× Cihly z roztříštěného kamene → 6× |
| <img src="img/enc/icons/1655.png" width="24"> | Deska z temného pískovce | výroba: 3× Temný pískovec → 6× |
| <img src="img/enc/icons/1930.png" width="24"> | Bílý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Bílé barvivo → 8× |
| <img src="img/enc/icons/1931.png" width="24"> | Oranžový betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Oranžové barvivo → 8× |
| <img src="img/enc/icons/1932.png" width="24"> | Purpurový betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Purpurové barvivo → 8× |
| <img src="img/enc/icons/1933.png" width="24"> | Světle modrý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Světle modré barvivo → 8× |
| <img src="img/enc/icons/1934.png" width="24"> | Žlutý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Žluté barvivo → 8× |
| <img src="img/enc/icons/1935.png" width="24"> | Limetkový betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Limetkové barvivo → 8× |
| <img src="img/enc/icons/1936.png" width="24"> | Růžový betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Růžové barvivo → 8× |
| <img src="img/enc/icons/1937.png" width="24"> | Šedý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Šedé barvivo → 8× |
| <img src="img/enc/icons/1938.png" width="24"> | Světle šedý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Světle šedé barvivo → 8× |
| <img src="img/enc/icons/1939.png" width="24"> | Azurový betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Azurové barvivo → 8× |
| <img src="img/enc/icons/1940.png" width="24"> | Fialový betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Fialové barvivo → 8× |
| <img src="img/enc/icons/1941.png" width="24"> | Modrý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Modré barvivo → 8× |
| <img src="img/enc/icons/1942.png" width="24"> | Hnědý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Hnědé barvivo → 8× |
| <img src="img/enc/icons/1943.png" width="24"> | Zelený betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Zelené barvivo → 8× |
| <img src="img/enc/icons/1944.png" width="24"> | Červený betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Červené barvivo → 8× |
| <img src="img/enc/icons/1945.png" width="24"> | Černý betonový prášek | výroba: Písek + Písek + Písek + Písek + Štěrk + Štěrk + Štěrk + Štěrk + Černé barvivo → 8× |
| <img src="img/enc/icons/1946.png" width="24"> | Bílý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1947.png" width="24"> | Oranžový beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1948.png" width="24"> | Purpurový beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1949.png" width="24"> | Světle modrý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1950.png" width="24"> | Žlutý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1951.png" width="24"> | Limetkový beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1952.png" width="24"> | Růžový beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1953.png" width="24"> | Šedý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1954.png" width="24"> | Světle šedý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1955.png" width="24"> | Azurový beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1956.png" width="24"> | Fialový beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1957.png" width="24"> | Modrý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1958.png" width="24"> | Hnědý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1959.png" width="24"> | Zelený beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1960.png" width="24"> | Červený beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1961.png" width="24"> | Černý beton | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1962.png" width="24"> | Bílý koberec | výroba: 2× Bílá vlna → 3× |
| <img src="img/enc/icons/1963.png" width="24"> | Oranžový koberec | výroba: 2× Oranžová vlna → 3× |
| <img src="img/enc/icons/1964.png" width="24"> | Purpurový koberec | výroba: 2× Purpurová vlna → 3× |
| <img src="img/enc/icons/1965.png" width="24"> | Světle modrý koberec | výroba: 2× Světle modrá vlna → 3× |
| <img src="img/enc/icons/1966.png" width="24"> | Žlutý koberec | výroba: 2× Žlutá vlna → 3× |
| <img src="img/enc/icons/1967.png" width="24"> | Limetkový koberec | výroba: 2× Limetková vlna → 3× |
| <img src="img/enc/icons/1968.png" width="24"> | Růžový koberec | výroba: 2× Růžová vlna → 3× |
| <img src="img/enc/icons/1969.png" width="24"> | Šedý koberec | výroba: 2× Šedá vlna → 3× |
| <img src="img/enc/icons/1970.png" width="24"> | Světle šedý koberec | výroba: 2× Světle šedá vlna → 3× |
| <img src="img/enc/icons/1971.png" width="24"> | Azurový koberec | výroba: 2× Azurová vlna → 3× |
| <img src="img/enc/icons/1972.png" width="24"> | Fialový koberec | výroba: 2× Fialová vlna → 3× |
| <img src="img/enc/icons/1973.png" width="24"> | Modrý koberec | výroba: 2× Modrá vlna → 3× |
| <img src="img/enc/icons/1974.png" width="24"> | Hnědý koberec | výroba: 2× Hnědá vlna → 3× |
| <img src="img/enc/icons/1975.png" width="24"> | Zelený koberec | výroba: 2× Zelená vlna → 3× |
| <img src="img/enc/icons/1976.png" width="24"> | Červený koberec | výroba: 2× Červená vlna → 3× |
| <img src="img/enc/icons/1977.png" width="24"> | Černý koberec | výroba: 2× Černá vlna → 3× |
| <img src="img/enc/icons/1810.png" width="24"> | Schody z tvrzeného kamene | výroba: 6× Tvrdý kámen → 4× |
| <img src="img/enc/icons/1818.png" width="24"> | Schody z prázdnotného kamene | výroba: 6× Prázdnotný kámen → 4× |
| <img src="img/enc/icons/1826.png" width="24"> | Netherrackové schody | výroba: 6× Úhelný kámen → 4× |
| <img src="img/enc/icons/1834.png" width="24"> | Obsidiánové schody | výroba: 6× Obsidián → 4× |
| <img src="img/enc/icons/1842.png" width="24"> | Schody z karmínového kamene | výroba: 6× Karmínový kámen → 4× |
| <img src="img/enc/icons/1850.png" width="24"> | Schody z duševního kamene | výroba: 6× Duševní kámen → 4× |
| <img src="img/enc/icons/1858.png" width="24"> | Schody z nebeského kamene | výroba: 6× Nebeský duševní kámen → 4× |
| <img src="img/enc/icons/1866.png" width="24"> | Krápníkové schody | výroba: 6× Krápníkový blok → 4× |
| <img src="img/enc/icons/1874.png" width="24"> | Schody z tvrdého dehtu | výroba: 6× Ztuhlý dehet → 4× |
| <img src="img/enc/icons/1890.png" width="24"> | Schody z popraskaných cihel úhelného kamene | výroba: 6× Popraskané cihly z úhelného kamene → 4× |
| <img src="img/enc/icons/1898.png" width="24"> | Schody z popraskaných deepstonových cihel | výroba: 6× Popraskané cihly z hlubinného kamene → 4× |
| <img src="img/enc/icons/1906.png" width="24"> | Schody z trouchnivých prken | výroba: 6× Shnilá prkna → 4× |
| <img src="img/enc/icons/1914.png" width="24"> | Schody z mechového deepstonu | výroba: 6× Mechový hlubinný kámen → 4× |
| <img src="img/enc/icons/1760.png" width="24"> | Deska z tvrzeného kamene | výroba: 3× Tvrdý kámen → 6× |
| <img src="img/enc/icons/1763.png" width="24"> | Deska z prázdnotného kamene | výroba: 3× Prázdnotný kámen → 6× |
| <img src="img/enc/icons/1766.png" width="24"> | Netherracková deska | výroba: 3× Úhelný kámen → 6× |
| <img src="img/enc/icons/1769.png" width="24"> | Obsidiánová deska | výroba: 3× Obsidián → 6× |
| <img src="img/enc/icons/1772.png" width="24"> | Deska z karmínového kamene | výroba: 3× Karmínový kámen → 6× |
| <img src="img/enc/icons/1775.png" width="24"> | Deska z duševního kamene | výroba: 3× Duševní kámen → 6× |
| <img src="img/enc/icons/1778.png" width="24"> | Deska z nebeského kamene | výroba: 3× Nebeský duševní kámen → 6× |
| <img src="img/enc/icons/1781.png" width="24"> | Krápníková deska | výroba: 3× Krápníkový blok → 6× |
| <img src="img/enc/icons/1784.png" width="24"> | Deska z tvrdého dehtu | výroba: 3× Ztuhlý dehet → 6× |
| <img src="img/enc/icons/1790.png" width="24"> | Deska z popraskaných cihel úhelného kamene | výroba: 3× Popraskané cihly z úhelného kamene → 6× |
| <img src="img/enc/icons/1793.png" width="24"> | Deska z popraskaných deepstonových cihel | výroba: 3× Popraskané cihly z hlubinného kamene → 6× |
| <img src="img/enc/icons/1796.png" width="24"> | Deska z trouchnivých prken | výroba: 3× Shnilá prkna → 6× |
| <img src="img/enc/icons/1799.png" width="24"> | Deska z mechového deepstonu | výroba: 3× Mechový hlubinný kámen → 6× |
| <img src="img/enc/icons/19.png" width="24"> | Prázdnotný kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/21.png" width="24"> | Oční kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/33.png" width="24"> | Netherperkový kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1533.png" width="24"> | Karmínový kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1556.png" width="24"> | Nebeský kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1565.png" width="24"> | Šedý kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1978.png" width="24"> | Crimsoniový kmen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60309.png" width="24"> | Voskovaný ocelový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60310.png" width="24"> | Zrezivělý ocelový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60311.png" width="24"> | Voskovaný zrezivělý ocelový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60312.png" width="24"> | Silně zrezivělý ocelový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60313.png" width="24"> | Voskovaný silně zrezivělý ocelový blok | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/20.png" width="24"> Sklo (18)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/20.png" width="24"> | Sklo | pec: Písek<br>pec: Rudý písek |
| <img src="img/enc/icons/1210.png" width="24"> | Bílé sklo | výroba: 8× Sklo + 1× Bílá vlna → 8× |
| <img src="img/enc/icons/1211.png" width="24"> | Oranžové sklo | výroba: 8× Sklo + 1× Oranžová vlna → 8× |
| <img src="img/enc/icons/1212.png" width="24"> | Purpurové sklo | výroba: 8× Sklo + 1× Purpurová vlna → 8× |
| <img src="img/enc/icons/1213.png" width="24"> | Světle modré sklo | výroba: 8× Sklo + 1× Světle modrá vlna → 8× |
| <img src="img/enc/icons/1214.png" width="24"> | Žluté sklo | výroba: 8× Sklo + 1× Žlutá vlna → 8× |
| <img src="img/enc/icons/1215.png" width="24"> | Limetkové sklo | výroba: 8× Sklo + 1× Limetková vlna → 8× |
| <img src="img/enc/icons/1216.png" width="24"> | Růžové sklo | výroba: 8× Sklo + 1× Růžová vlna → 8× |
| <img src="img/enc/icons/1217.png" width="24"> | Šedé sklo | výroba: 8× Sklo + 1× Šedá vlna → 8× |
| <img src="img/enc/icons/1218.png" width="24"> | Světle šedé sklo | výroba: 8× Sklo + 1× Světle šedá vlna → 8× |
| <img src="img/enc/icons/1219.png" width="24"> | Azurové sklo | výroba: 8× Sklo + 1× Azurová vlna → 8× |
| <img src="img/enc/icons/1220.png" width="24"> | Fialové sklo | výroba: 8× Sklo + 1× Fialová vlna → 8× |
| <img src="img/enc/icons/1221.png" width="24"> | Modré sklo | výroba: 8× Sklo + 1× Modrá vlna → 8× |
| <img src="img/enc/icons/1222.png" width="24"> | Hnědé sklo | výroba: 8× Sklo + 1× Hnědá vlna → 8× |
| <img src="img/enc/icons/1223.png" width="24"> | Zelené sklo | výroba: 8× Sklo + 1× Zelená vlna → 8× |
| <img src="img/enc/icons/1224.png" width="24"> | Červené sklo | výroba: 8× Sklo + 1× Červená vlna → 8× |
| <img src="img/enc/icons/1225.png" width="24"> | Černé sklo | výroba: 8× Sklo + 1× Černá vlna → 8× |
| <img src="img/enc/icons/1503.png" width="24"> | Powerstonové sklo | výroba: 8× Sklo + 1× Powerstonový prach → 8× |

</details>

#### <img src="img/enc/icons/1527.png" width="24"> Osvětlení (3)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/59.png" width="24"> | Powerstonová lampa | výroba: 4× Sklo + 1× Powerstonový čip<br>výroba: 1× Sklo + 1× Powerstonový prach + 1× Pochodeň |
| <img src="img/enc/icons/86.png" width="24"> | Powerstonové světlo | výroba: 1× Sklo + 2× Powerstonový prach → 2× |
| <img src="img/enc/icons/1527.png" width="24"> | Pochodeň | výroba: 1× Klacek + 1× Uhlí → 4× |

</details>

#### <img src="img/enc/icons/2.png" width="24"> Příroda (86)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/2.png" width="24"> | Travnatý blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/3.png" width="24"> | Hlína | těžba, nález nebo kreativ |
| <img src="img/enc/icons/12.png" width="24"> | Písek | těžba, nález nebo kreativ |
| <img src="img/enc/icons/13.png" width="24"> | Štěrk | těžba, nález nebo kreativ |
| <img src="img/enc/icons/82.png" width="24"> | Jíl | těžba, nález nebo kreativ |
| <img src="img/enc/icons/80.png" width="24"> | Sněhový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/79.png" width="24"> | Led | těžba, nález nebo kreativ |
| <img src="img/enc/icons/81.png" width="24"> | Kaktus | těžba, nález nebo kreativ |
| <img src="img/enc/icons/18.png" width="24"> | Dubové listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/161.png" width="24"> | Smrkové jehličí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/31.png" width="24"> | Vysoká tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/37.png" width="24"> | Pampeliška | těžba, nález nebo kreativ |
| <img src="img/enc/icons/38.png" width="24"> | Mák | těžba, nález nebo kreativ |
| <img src="img/enc/icons/9.png" width="24"> | Voda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/11.png" width="24"> | Láva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/16.png" width="24"> | Uhelná ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/15.png" width="24"> | Železná ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/180.png" width="24"> | Bronzová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/181.png" width="24"> | Ocelová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/182.png" width="24"> | Rubínová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/87.png" width="24"> | Úhelný kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/112.png" width="24"> | Cihly z úhelného kamene | pec: Úhelný kámen |
| <img src="img/enc/icons/6.png" width="24"> | Úhelná tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/8.png" width="24"> | Grasser | těžba, nález nebo kreativ |
| <img src="img/enc/icons/10.png" width="24"> | Prázdnotný kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/14.png" width="24"> | Fialová tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/22.png" width="24"> | Prázdnotné listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/23.png" width="24"> | Rudý písek | těžba, nález nebo kreativ |
| <img src="img/enc/icons/28.png" width="24"> | Karmínové kořeny | těžba, nález nebo kreativ |
| <img src="img/enc/icons/27.png" width="24"> | Dýně | těžba, nález nebo kreativ |
| <img src="img/enc/icons/34.png" width="24"> | Netherperková koruna | těžba, nález nebo kreativ |
| <img src="img/enc/icons/40.png" width="24"> | Dubová sazenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/41.png" width="24"> | Borová sazenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/42.png" width="24"> | Prázdnotná sazenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/43.png" width="24"> | Netherperková sazenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/44.png" width="24"> | Powerstonová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/97.png" width="24"> | Ornice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/163.png" width="24"> | Semínka pšenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1200.png" width="24"> | Dehet | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1202.png" width="24"> | Krápníkový blok | výroba: 4× Krápník |
| <img src="img/enc/icons/1203.png" width="24"> | Krápník | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1205.png" width="24"> | Sněhová pokrývka | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1509.png" width="24"> | Pustá tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1510.png" width="24"> | Suchá tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1511.png" width="24"> | Květ Grasser | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1512.png" width="24"> | Kapradí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1504.png" width="24"> | Štěrk s nálezy | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1526.png" width="24"> | Karmínový kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1538.png" width="24"> | Hlubinný kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1539.png" width="24"> | Hardiová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1540.png" width="24"> | Mechový hlubinný kámen | výroba: Hlubinný kámen + Liány |
| <img src="img/enc/icons/1541.png" width="24"> | Blok mechu | výroba: 4× Liány |
| <img src="img/enc/icons/1542.png" width="24"> | Houbový hlubinný kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60175.png" width="24"> | Propastná skála | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1547.png" width="24"> | Liány | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1549.png" width="24"> | Duševní tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1550.png" width="24"> | Duševní hlína | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1551.png" width="24"> | Duševní kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1560.png" width="24"> | Nebeská duševní tráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1561.png" width="24"> | Nebeská duševní hlína | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1562.png" width="24"> | Nebeský duševní kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1559.png" width="24"> | Nebeské listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1567.png" width="24"> | Červené listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1568.png" width="24"> | Oranžové listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1569.png" width="24"> | Žluté listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1573.png" width="24"> | Šedá sazenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1574.png" width="24"> | Nebeská sazenice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1980.png" width="24"> | Crimsoniové listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1981.png" width="24"> | Roztříštěný kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1983.png" width="24"> | Blok prázdnotných střepů | výroba: 4× Prázdnotný střep |
| <img src="img/enc/icons/1984.png" width="24"> | Temný písek | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1986.png" width="24"> | Shluk střepů | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1987.png" width="24"> | Crimsoniová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1576.png" width="24"> | Bledý kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1577.png" width="24"> | Nebeský kámen | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1578.png" width="24"> | Celestiová ruda | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1579.png" width="24"> | Bledý krystal | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1582.png" width="24"> | Spadané listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60158.png" width="24"> | Měděný mech | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60163.png" width="24"> | Skleněný rákos | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60167.png" width="24"> | Zářící listí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60168.png" width="24"> | Keř hvězdnic | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60162.png" width="24"> | Éterový lák | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60271.png" width="24"> | Železná ruda Cornerstone | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60272.png" width="24"> | Bronzová ruda Cornerstone | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60273.png" width="24"> | Ocelová ruda Cornerstone | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/47.png" width="24"> Dekorace (32)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/47.png" width="24"> | Knihovna | výroba: 6× Dubová prkna + 3× Klacek |
| <img src="img/enc/icons/58.png" width="24"> | Pracovní stůl | výroba: 4× Dubová prkna |
| <img src="img/enc/icons/61.png" width="24"> | Pec | výroba: 8× Dlažební kámen |
| <img src="img/enc/icons/54.png" width="24"> | Truhla | výroba: 8× Dubová prkna |
| <img src="img/enc/icons/85.png" width="24"> | Plot | výroba: 4× Dubová prkna + 2× Klacek → 3× |
| <img src="img/enc/icons/63.png" width="24"> | Dubový poklop | výroba: 6× Dubová prkna → 2× |
| <img src="img/enc/icons/207.png" width="24"> | Dubové dveře | výroba: 6× Dubová prkna → 3× |
| <img src="img/enc/icons/208.png" width="24"> | Železné dveře | těžba, nález nebo kreativ |
| <img src="img/enc/icons/35.png" width="24"> | Bílá vlna | těžba, nález nebo kreativ |
| <img src="img/enc/icons/164.png" width="24"> | Oranžová vlna | výroba: Bílá vlna + Oranžové barvivo |
| <img src="img/enc/icons/165.png" width="24"> | Purpurová vlna | výroba: Bílá vlna + Purpurové barvivo |
| <img src="img/enc/icons/166.png" width="24"> | Světle modrá vlna | výroba: Bílá vlna + Světle modré barvivo |
| <img src="img/enc/icons/167.png" width="24"> | Žlutá vlna | výroba: Bílá vlna + Žluté barvivo |
| <img src="img/enc/icons/168.png" width="24"> | Limetková vlna | výroba: Bílá vlna + Limetkové barvivo |
| <img src="img/enc/icons/169.png" width="24"> | Růžová vlna | výroba: Bílá vlna + Růžové barvivo |
| <img src="img/enc/icons/170.png" width="24"> | Šedá vlna | výroba: Bílá vlna + Šedé barvivo |
| <img src="img/enc/icons/171.png" width="24"> | Světle šedá vlna | výroba: Bílá vlna + Světle šedé barvivo |
| <img src="img/enc/icons/172.png" width="24"> | Azurová vlna | výroba: Bílá vlna + Azurové barvivo |
| <img src="img/enc/icons/173.png" width="24"> | Fialová vlna | výroba: Bílá vlna + Fialové barvivo |
| <img src="img/enc/icons/174.png" width="24"> | Modrá vlna | výroba: Bílá vlna + Modré barvivo |
| <img src="img/enc/icons/175.png" width="24"> | Hnědá vlna | výroba: Bílá vlna + Hnědé barvivo |
| <img src="img/enc/icons/176.png" width="24"> | Zelená vlna | výroba: Bílá vlna + Zelené barvivo |
| <img src="img/enc/icons/177.png" width="24"> | Červená vlna | výroba: Bílá vlna + Červené barvivo |
| <img src="img/enc/icons/178.png" width="24"> | Černá vlna | výroba: Bílá vlna + Černé barvivo |
| <img src="img/enc/icons/253.png" width="24"> | Postel | výroba: 3× Dubová prkna + 3× Bílá vlna |
| <img src="img/enc/icons/153.png" width="24"> | Balík sena | výroba: 9× Pšenice |
| <img src="img/enc/icons/1520.png" width="24"> | Smrkový plot | výroba: 4× Smrková prkna + 2× Klacek → 3× |
| <img src="img/enc/icons/1521.png" width="24"> | Prázdnotný plot | výroba: 4× Prázdnotná prkna + 2× Klacek → 3× |
| <img src="img/enc/icons/1522.png" width="24"> | Netherperkový plot | výroba: 4× Netherperková prkna + 2× Klacek → 3× |
| <img src="img/enc/icons/1523.png" width="24"> | Plot z cihel úhelného kamene | výroba: 6× Cihly z úhelného kamene → 6× |
| <img src="img/enc/icons/1505.png" width="24"> | Lebka kostlivce | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60008.png" width="24"> | Sedák | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/59.png" width="24"> Powerstone a mechanismy (19)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/30.png" width="24"> | TNT | výroba: 5× Písek + 4× Powerstonový prach |
| <img src="img/enc/icons/46.png" width="24"> | Powerstonový blok | výroba: 9× Powerstonový prach |
| <img src="img/enc/icons/56.png" width="24"> | Napájecí kabel | výroba: 3× Powerstonový prach → 8× |
| <img src="img/enc/icons/83.png" width="24"> | Vypínač | výroba: 1× Dlažební kámen + 1× Powerstonový čip + 1× Klacek |
| <img src="img/enc/icons/1500.png" width="24"> | Powerstonové cihly | výroba: 2× Kamenné cihly + 2× Powerstonový prach → 2× |
| <img src="img/enc/icons/1501.png" width="24"> | Popraskané powerstonové cihly | pec: Powerstonové cihly |
| <img src="img/enc/icons/1502.png" width="24"> | Zdobené powerstonové cihly | výroba: 2× Powerstonová deska |
| <img src="img/enc/icons/1140.png" width="24"> | Powerstonové schody | výroba: 6× Powerstonové cihly → 4× |
| <img src="img/enc/icons/1136.png" width="24"> | Powerstonová deska | výroba: 3× Powerstonové cihly → 6× |
| <img src="img/enc/icons/1524.png" width="24"> | Powerstonový plot | výroba: 6× Powerstonové cihly → 6× |
| <img src="img/enc/icons/1534.png" width="24"> | Powerstonová pec | výroba: 1× Pec + 5× Powerstonový čip + 3× Powerstonové cihly |
| <img src="img/enc/icons/1587.png" width="24"> | Zvukový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60176.png" width="24"> | Rozhlas | výroba: 1× Powerstonový prach + 7× Ocelový ingot + 1× Zvukový blok |
| <img src="img/enc/icons/1882.png" width="24"> | Schody z popraskaných powerstonových cihel | výroba: 6× Popraskané powerstonové cihly → 4× |
| <img src="img/enc/icons/1922.png" width="24"> | Schody z vytesaných powerstonových cihel | výroba: 6× Zdobené powerstonové cihly → 4× |
| <img src="img/enc/icons/1787.png" width="24"> | Deska z popraskaných powerstonových cihel | výroba: 3× Popraskané powerstonové cihly → 6× |
| <img src="img/enc/icons/1802.png" width="24"> | Deska z vytesaných powerstonových cihel | výroba: 3× Zdobené powerstonové cihly → 6× |
| <img src="img/enc/icons/99.png" width="24"> | Powerstonový prach | výroba: Starověká mince + Starověká mince + Starověká mince + Starověká mince → 2×<br>pec: Powerstonová ruda |
| <img src="img/enc/icons/100.png" width="24"> | Powerstonový čip | výroba: 2× Powerstonový prach + 1× Ocelový ingot → 2× |

</details>

#### <img src="img/enc/icons/212.png" width="24"> Nástroje a zbraně (50)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/210.png" width="24"> | Dřevěný krumpáč | výroba: 3× Dubová prkna + 2× Klacek |
| <img src="img/enc/icons/213.png" width="24"> | Dřevěná sekera | výroba: 3× Dubová prkna + 2× Klacek |
| <img src="img/enc/icons/216.png" width="24"> | Dřevěná lopata | výroba: 1× Dubová prkna + 2× Klacek |
| <img src="img/enc/icons/211.png" width="24"> | Kamenný krumpáč | výroba: 3× Dlažební kámen + 2× Klacek |
| <img src="img/enc/icons/214.png" width="24"> | Kamenná sekera | výroba: 3× Dlažební kámen + 2× Klacek |
| <img src="img/enc/icons/217.png" width="24"> | Kamenná lopata | výroba: 1× Dlažební kámen + 2× Klacek |
| <img src="img/enc/icons/212.png" width="24"> | Železný krumpáč | těžba, nález nebo kreativ |
| <img src="img/enc/icons/215.png" width="24"> | Železná sekera | těžba, nález nebo kreativ |
| <img src="img/enc/icons/218.png" width="24"> | Železná lopata | těžba, nález nebo kreativ |
| <img src="img/enc/icons/219.png" width="24"> | Bronzový krumpáč | výroba: 2× Klacek + 3× Bronzový ingot |
| <img src="img/enc/icons/220.png" width="24"> | Bronzová sekera | výroba: 2× Klacek + 3× Bronzový ingot |
| <img src="img/enc/icons/221.png" width="24"> | Bronzová lopata | výroba: 2× Klacek + 1× Bronzový ingot |
| <img src="img/enc/icons/222.png" width="24"> | Ocelový krumpáč | výroba: 2× Klacek + 3× Ocelový ingot |
| <img src="img/enc/icons/223.png" width="24"> | Ocelová sekera | výroba: 2× Klacek + 3× Ocelový ingot |
| <img src="img/enc/icons/224.png" width="24"> | Ocelová lopata | výroba: 2× Klacek + 1× Ocelový ingot |
| <img src="img/enc/icons/225.png" width="24"> | Rubínový krumpáč | výroba: 2× Klacek + 3× Rubín |
| <img src="img/enc/icons/226.png" width="24"> | Rubínová sekera | výroba: 2× Klacek + 3× Rubín |
| <img src="img/enc/icons/227.png" width="24"> | Rubínová lopata | výroba: 2× Klacek + 1× Rubín |
| <img src="img/enc/icons/255.png" width="24"> | Křesadlo | výroba: Pazourek + Ocelový ingot |
| <img src="img/enc/icons/248.png" width="24"> | Prázdnotné křesadlo | těžba, nález nebo kreativ |
| <img src="img/enc/icons/190.png" width="24"> | Dřevěný meč | výroba: 2× Dubová prkna + 1× Klacek |
| <img src="img/enc/icons/191.png" width="24"> | Kamenný meč | výroba: 2× Dlažební kámen + 1× Klacek |
| <img src="img/enc/icons/192.png" width="24"> | Železný meč | těžba, nález nebo kreativ |
| <img src="img/enc/icons/193.png" width="24"> | Bronzový meč | výroba: 1× Klacek + 2× Bronzový ingot |
| <img src="img/enc/icons/194.png" width="24"> | Ocelový meč | výroba: 1× Klacek + 2× Ocelový ingot |
| <img src="img/enc/icons/195.png" width="24"> | Rubínový meč | výroba: 1× Klacek + 2× Rubín |
| <img src="img/enc/icons/186.png" width="24"> | Kbelík | výroba: 3× Ocelový ingot |
| <img src="img/enc/icons/187.png" width="24"> | Kbelík vody | těžba, nález nebo kreativ |
| <img src="img/enc/icons/188.png" width="24"> | Kbelík lávy | těžba, nález nebo kreativ |
| <img src="img/enc/icons/102.png" width="24"> | Motyka | výroba: 2× Dubová prkna + 2× Klacek<br>výroba: 2× Klacek + 2× Ocelový ingot |
| <img src="img/enc/icons/103.png" width="24"> | Luk | výroba: 3× Klacek + 3× Provázek |
| <img src="img/enc/icons/104.png" width="24"> | Šíp | výroba: 1× Pírko + 1× Klacek + 1× Pazourek → 4× |
| <img src="img/enc/icons/1600.png" width="24"> | Štětec | výroba: 1× Pírko + 1× Klacek + 1× Ocelový ingot |
| <img src="img/enc/icons/1613.png" width="24"> | Štít | výroba: 6× Dubová prkna + 1× Ocelový ingot |
| <img src="img/enc/icons/1618.png" width="24"> | Hardiový krumpáč | výroba: 2× Klacek + 3× Hardium |
| <img src="img/enc/icons/1620.png" width="24"> | Nebeský kompas | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1621.png" width="24"> | Hardiový meč | výroba: 1× Klacek + 2× Hardium |
| <img src="img/enc/icons/1622.png" width="24"> | Hardiová sekera | výroba: 2× Klacek + 3× Hardium |
| <img src="img/enc/icons/1623.png" width="24"> | Hardiová lopata | výroba: 2× Klacek + 1× Hardium |
| <img src="img/enc/icons/1991.png" width="24"> | Celestiový krumpáč | výroba: 2× Klacek + 3× Celestium |
| <img src="img/enc/icons/1992.png" width="24"> | Celestiový meč | výroba: 1× Klacek + 2× Celestium |
| <img src="img/enc/icons/1993.png" width="24"> | Celestiová sekera | výroba: 2× Klacek + 3× Celestium |
| <img src="img/enc/icons/1994.png" width="24"> | Celestiová lopata | výroba: 2× Klacek + 1× Celestium |
| <img src="img/enc/icons/60301.png" width="24"> | Peněženka | výroba: 1× Provázek + 7× Kůže |
| <img src="img/enc/icons/60302.png" width="24"> | Kožený batoh | výroba: 1× Truhla + 1× Provázek + 7× Kůže |
| <img src="img/enc/icons/60303.png" width="24"> | Ocelový batoh | výroba: 4× Ocelový ingot + 1× Kožený batoh |
| <img src="img/enc/icons/60304.png" width="24"> | Ocelobronzový batoh | výroba: 4× Bronzový ingot + 1× Ocelový batoh |
| <img src="img/enc/icons/60305.png" width="24"> | Bronzorubínový batoh | výroba: 4× Rubín + 1× Ocelobronzový batoh |
| <img src="img/enc/icons/60306.png" width="24"> | Hardiový batoh | výroba: 4× Hardium + 1× Bronzorubínový batoh |
| <img src="img/enc/icons/60307.png" width="24"> | Celestiový batoh | výroba: 4× Celestium + 1× Hardiový batoh |

</details>

#### <img src="img/enc/icons/237.png" width="24"> Brnění (26)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/228.png" width="24"> | Dřevěná helma | výroba: 5× Dubová prkna |
| <img src="img/enc/icons/229.png" width="24"> | Dřevěný kyrys | výroba: 8× Dubová prkna |
| <img src="img/enc/icons/230.png" width="24"> | Dřevěné nohavice | výroba: 7× Dubová prkna |
| <img src="img/enc/icons/231.png" width="24"> | Dřevěné boty | výroba: 4× Dubová prkna |
| <img src="img/enc/icons/232.png" width="24"> | Kamenná helma | výroba: 5× Dlažební kámen |
| <img src="img/enc/icons/233.png" width="24"> | Kamenný kyrys | výroba: 8× Dlažební kámen |
| <img src="img/enc/icons/234.png" width="24"> | Kamenné nohavice | výroba: 7× Dlažební kámen |
| <img src="img/enc/icons/235.png" width="24"> | Kamenné boty | výroba: 4× Dlažební kámen |
| <img src="img/enc/icons/236.png" width="24"> | Bronzová helma | výroba: 5× Bronzový ingot |
| <img src="img/enc/icons/237.png" width="24"> | Bronzový kyrys | výroba: 8× Bronzový ingot |
| <img src="img/enc/icons/238.png" width="24"> | Bronzové nohavice | výroba: 7× Bronzový ingot |
| <img src="img/enc/icons/239.png" width="24"> | Bronzové boty | výroba: 4× Bronzový ingot |
| <img src="img/enc/icons/240.png" width="24"> | Ocelová helma | výroba: 5× Ocelový ingot |
| <img src="img/enc/icons/241.png" width="24"> | Ocelový kyrys | výroba: 8× Ocelový ingot |
| <img src="img/enc/icons/242.png" width="24"> | Ocelové nohavice | výroba: 7× Ocelový ingot |
| <img src="img/enc/icons/243.png" width="24"> | Ocelové boty | výroba: 4× Ocelový ingot |
| <img src="img/enc/icons/244.png" width="24"> | Rubínová helma | výroba: 5× Rubín |
| <img src="img/enc/icons/245.png" width="24"> | Rubínový kyrys | výroba: 8× Rubín |
| <img src="img/enc/icons/246.png" width="24"> | Rubínové nohavice | výroba: 7× Rubín |
| <img src="img/enc/icons/247.png" width="24"> | Rubínové boty | výroba: 4× Rubín |
| <img src="img/enc/icons/1624.png" width="24"> | Hardiová helma | výroba: 5× Hardium |
| <img src="img/enc/icons/1625.png" width="24"> | Hardiový kyrys | výroba: 8× Hardium |
| <img src="img/enc/icons/1626.png" width="24"> | Hardiové nohavice | výroba: 7× Hardium |
| <img src="img/enc/icons/1627.png" width="24"> | Hardiové boty | výroba: 4× Hardium |
| <img src="img/enc/icons/1995.png" width="24"> | Celestiový kyrys | výroba: 1× Bledý krystal + 7× Celestium |
| <img src="img/enc/icons/1996.png" width="24"> | Nebeská křídla | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/1301.png" width="24"> Mapy a zvláštní předměty (17)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/7.png" width="24"> | Podloží | těžba, nález nebo kreativ |
| <img src="img/enc/icons/189.png" width="24"> | Kompas k aréně | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1301.png" width="24"> | Mapa | výroba: 8× Pšenice + 1× Uhlí |
| <img src="img/enc/icons/1601.png" width="24"> | Kompas k pevnosti | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1614.png" width="24"> | Průzkumná mapa | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1615.png" width="24"> | Kniha a brk | výroba: Pírko + Pšenice + Uhlí |
| <img src="img/enc/icons/1602.png" width="24"> | Starý dokument I | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1603.png" width="24"> | Starý dokument II | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1604.png" width="24"> | Starý dokument III | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1605.png" width="24"> | Starý dokument IV | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1606.png" width="24"> | Starý dokument V | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1607.png" width="24"> | Starý dokument VI | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1608.png" width="24"> | Starý dokument VII | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1609.png" width="24"> | Starý dokument VIII | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1610.png" width="24"> | Starověká mince | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1611.png" width="24"> | Powerstonová relikvie | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1612.png" width="24"> | Střep | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/107.png" width="24"> Jídlo a suroviny (50)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/60169.png" width="24"> | Prázdnotný lanýž | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60170.png" width="24"> | Hvězdnice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1644.png" width="24"> | Prázdnotný střep | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1645.png" width="24"> | Crimsonium | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1628.png" width="24"> | Bílé barvivo | výroba: Jíl → 2× |
| <img src="img/enc/icons/1629.png" width="24"> | Oranžové barvivo | výroba: Oranžové listí → 2× |
| <img src="img/enc/icons/1630.png" width="24"> | Purpurové barvivo | výroba: Růžové barvivo + Fialové barvivo → 2× |
| <img src="img/enc/icons/1631.png" width="24"> | Světle modré barvivo | výroba: Bílé barvivo + Modré barvivo → 2× |
| <img src="img/enc/icons/1632.png" width="24"> | Žluté barvivo | výroba: Pampeliška → 2×<br>výroba: Žluté listí → 2× |
| <img src="img/enc/icons/1633.png" width="24"> | Limetkové barvivo | výroba: Bílé barvivo + Zelené barvivo → 2× |
| <img src="img/enc/icons/1634.png" width="24"> | Růžové barvivo | výroba: Bílé barvivo + Červené barvivo → 2× |
| <img src="img/enc/icons/1635.png" width="24"> | Šedé barvivo | výroba: Bílé barvivo + Černé barvivo → 2× |
| <img src="img/enc/icons/1636.png" width="24"> | Světle šedé barvivo | výroba: Bílé barvivo + Šedé barvivo → 2× |
| <img src="img/enc/icons/1637.png" width="24"> | Azurové barvivo | výroba: Modré barvivo + Zelené barvivo → 2× |
| <img src="img/enc/icons/1638.png" width="24"> | Fialové barvivo | výroba: Modré barvivo + Červené barvivo → 2× |
| <img src="img/enc/icons/1639.png" width="24"> | Modré barvivo | výroba: Powerstonový prach → 2× |
| <img src="img/enc/icons/1640.png" width="24"> | Hnědé barvivo | výroba: Hlína → 2× |
| <img src="img/enc/icons/1641.png" width="24"> | Zelené barvivo | výroba: Kaktus → 2× |
| <img src="img/enc/icons/1642.png" width="24"> | Červené barvivo | výroba: Mák → 2×<br>výroba: Červené listí → 2× |
| <img src="img/enc/icons/1643.png" width="24"> | Černé barvivo | výroba: Uhlí → 2× |
| <img src="img/enc/icons/200.png" width="24"> | Klacek | výroba: 2× Dubová prkna → 4×<br>výroba: Shnilá prkna → 2× |
| <img src="img/enc/icons/201.png" width="24"> | Uhlí | pec: Dubový kmen<br>pec: Smrkový kmen |
| <img src="img/enc/icons/202.png" width="24"> | Železný ingot | pec: Železná ruda<br>pec: Železná ruda Cornerstone |
| <img src="img/enc/icons/204.png" width="24"> | Bronzový ingot | výroba: Bronzový blok → 9×<br>pec: Bronzová ruda<br>pec: Bronzová ruda Cornerstone |
| <img src="img/enc/icons/205.png" width="24"> | Ocelový ingot | výroba: Ocelový blok → 9×<br>pec: Ocelová ruda<br>pec: Ocelová ruda Cornerstone<br>pec: Ocelová směs |
| <img src="img/enc/icons/206.png" width="24"> | Rubín | výroba: Rubínový blok → 9× |
| <img src="img/enc/icons/203.png" width="24"> | Pazourek | těžba, nález nebo kreativ |
| <img src="img/enc/icons/254.png" width="24"> | Jablko | těžba, nález nebo kreativ |
| <img src="img/enc/icons/209.png" width="24"> | Loďka | výroba: 5× Dubová prkna |
| <img src="img/enc/icons/249.png" width="24"> | Syrové vepřové | těžba, nález nebo kreativ |
| <img src="img/enc/icons/250.png" width="24"> | Pečené vepřové | pec: Syrové vepřové |
| <img src="img/enc/icons/251.png" width="24"> | Syrové skopové | těžba, nález nebo kreativ |
| <img src="img/enc/icons/252.png" width="24"> | Pečené skopové | pec: Syrové skopové |
| <img src="img/enc/icons/105.png" width="24"> | Pírko | těžba, nález nebo kreativ |
| <img src="img/enc/icons/106.png" width="24"> | Pšenice | výroba: Balík sena → 9× |
| <img src="img/enc/icons/107.png" width="24"> | Chléb | výroba: 3× Pšenice |
| <img src="img/enc/icons/108.png" width="24"> | Syrové kuře | těžba, nález nebo kreativ |
| <img src="img/enc/icons/109.png" width="24"> | Pečené kuře | pec: Syrové kuře |
| <img src="img/enc/icons/110.png" width="24"> | Syrové hovězí | těžba, nález nebo kreativ |
| <img src="img/enc/icons/111.png" width="24"> | Steak | pec: Syrové hovězí |
| <img src="img/enc/icons/154.png" width="24"> | Kbelík mléka | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1300.png" width="24"> | Provázek | výroba: Bílá vlna → 4× |
| <img src="img/enc/icons/1302.png" width="24"> | Dehtová koule | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1619.png" width="24"> | Hlava Karmínového muže | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1617.png" width="24"> | Hardium | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1990.png" width="24"> | Celestium | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1997.png" width="24"> | Medová kulička | výroba: Medový blok → 4× |
| <img src="img/enc/icons/60274.png" width="24"> | Žhavý prach | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60308.png" width="24"> | Ocelová směs | výroba: Uhlí + Železný ingot |
| <img src="img/enc/icons/60300.png" width="24"> | Kůže | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/196.png" width="24"> Vejce (24)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/196.png" width="24"> | Vejce: prase | těžba, nález nebo kreativ |
| <img src="img/enc/icons/197.png" width="24"> | Vejce: ovce | těžba, nález nebo kreativ |
| <img src="img/enc/icons/198.png" width="24"> | Vejce: dýňový sliz | těžba, nález nebo kreativ |
| <img src="img/enc/icons/199.png" width="24"> | Vejce: zapomenutý muž | těžba, nález nebo kreativ |
| <img src="img/enc/icons/155.png" width="24"> | Vejce: slepice | těžba, nález nebo kreativ |
| <img src="img/enc/icons/156.png" width="24"> | Vejce: kráva | těžba, nález nebo kreativ |
| <img src="img/enc/icons/157.png" width="24"> | Vejce: vesničan | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1303.png" width="24"> | Vejce: pavouk | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1304.png" width="24"> | Vejce: netopýr | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1305.png" width="24"> | Vejce: TNT zombie | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1306.png" width="24"> | Vejce: dehtová příšera | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1307.png" width="24"> | Vejce: Husking | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1308.png" width="24"> | Vejce: Prasák | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60289.png" width="24"> | Vejce: Celestling | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60290.png" width="24"> | Vejce: Reach Stalker | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60291.png" width="24"> | Vejce: Sky Drifter | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60292.png" width="24"> | Vejce: Úhelný golem | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60293.png" width="24"> | Vejce: Mechový plaz | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60294.png" width="24"> | Vejce: Sporovka | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60295.png" width="24"> | Vejce: Žhavá můra | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60296.png" width="24"> | Vejce: Popelavý lišák | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60297.png" width="24"> | Vejce: Lišák | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60298.png" width="24"> | Vejce: Duch | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60299.png" width="24"> | Vejce: Sněhulák | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/1586.png" width="24"> Speciální bloky (5)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/1548.png" width="24"> | Líheň příšer | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1584.png" width="24"> | Blok struktur | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1585.png" width="24"> | Bariéra | těžba, nález nebo kreativ |
| <img src="img/enc/icons/1586.png" width="24"> | Příkazový blok | těžba, nález nebo kreativ |
| <img src="img/enc/icons/29.png" width="24"> | Konec světa | těžba, nález nebo kreativ |

</details>

#### <img src="img/enc/icons/60013.png" width="24"> Železnice (28)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/1588.png" width="24"> | Kolej | výroba: 1× Smrkový kmen + 6× Ocelový ingot → 16× |
| <img src="img/enc/icons/60016.png" width="24"> | Šikmá kolej | výroba: 3× Dlažební kámen + 3× Kolej → 3× |
| <img src="img/enc/icons/60187.png" width="24"> | Mírné stoupání koleje | výroba: 6× Kolej → 4× |
| <img src="img/enc/icons/60000.png" width="24"> | Výhybka | výroba: 1× Vypínač + 3× Kolej |
| <img src="img/enc/icons/60043.png" width="24"> | Točna | výroba: 7× Dlažební kámen + 1× Powerstonový prach + 1× Kolej |
| <img src="img/enc/icons/60020.png" width="24"> | Urychlovač vlaku | výroba: 4× Powerstonový prach + 4× Ocelový ingot + 1× Kolej → 2× |
| <img src="img/enc/icons/60021.png" width="24"> | Přejezdový zvonek | výroba: 1× Powerstonový prach + 8× Ocelový ingot |
| <img src="img/enc/icons/60022.png" width="24"> | Houkačka | výroba: 1× Powerstonový prach + 8× Ocelový ingot |
| <img src="img/enc/icons/60023.png" width="24"> | Přejezdové světlo | výroba: 2× Bílá vlna + 2× Powerstonová lampa + 2× Ocelový ingot |
| <img src="img/enc/icons/60035.png" width="24"> | Závora | výroba: 1× Bílá vlna + 1× Powerstonová lampa + 1× Červená vlna + 2× Ocelový ingot |
| <img src="img/enc/icons/60009.png" width="24"> | Zarážka | výroba: 5× Ocelový ingot + 3× Kolej |
| <img src="img/enc/icons/60013.png" width="24"> | Lokomotiva | výroba: 1× Pec + 5× Ocelový blok + 2× Ocelový ingot |
| <img src="img/enc/icons/60014.png" width="24"> | Osobní vagon | výroba: 3× Dubová prkna + 3× Sklo + 3× Ocelový ingot |
| <img src="img/enc/icons/60015.png" width="24"> | Nákladní vagon | výroba: 4× Dubová prkna + 1× Truhla + 3× Ocelový ingot |
| <img src="img/enc/icons/60045.png" width="24"> | Vrtací vůz | výroba: 1× Pec + 1× Ocelový blok + 6× Ocelový ingot + 1× Kolej |
| <img src="img/enc/icons/60244.png" width="24"> | Čelo metra | výroba: 3× Sklo + 1× Powerstonový prach + 1× Ocelový blok + 4× Ocelový ingot |
| <img src="img/enc/icons/60245.png" width="24"> | Vůz metra | výroba: 3× Sklo + 5× Ocelový ingot |
| <img src="img/enc/icons/60246.png" width="24"> | Betonové kolejové lože | výroba: 1× Štěrk + 2× Tvrzený beton → 3× |
| <img src="img/enc/icons/60247.png" width="24"> | Nástupiště | výroba: 3× Tvrzený beton + 3× Žluté barvivo → 3× |
| <img src="img/enc/icons/60248.png" width="24"> | Příhradový nosník | výroba: 5× Ocelový ingot → 4× |
| <img src="img/enc/icons/60249.png" width="24"> | Kolejové křížení | výroba: 5× Kolej |
| <img src="img/enc/icons/60250.png" width="24"> | Šikmé kolejové křížení | výroba: 5× Kolej |
| <img src="img/enc/icons/60265.png" width="24"> | Obklad metra | výroba: 4× Tvrzený beton → 4× |
| <img src="img/enc/icons/60266.png" width="24"> | Modrý obklad metra | výroba: Modré barvivo + Obklad metra |
| <img src="img/enc/icons/60267.png" width="24"> | Popraskaný obklad | výroba: Štěrk + Obklad metra |
| <img src="img/enc/icons/60268.png" width="24"> | Cedule stanice | výroba: 1× Modré barvivo + 5× Obklad metra → 2× |
| <img src="img/enc/icons/60269.png" width="24"> | Lavička | výroba: 2× Smrková prkna + 3× Smrková deska → 2× |
| <img src="img/enc/icons/60270.png" width="24"> | Mapa metra | výroba: Mapa + Kolej |

</details>

#### <img src="img/enc/icons/60047.png" width="24"> Bronz (23)

<details>
<summary>Rozbalit</summary>

| | Název | Jak ho získat |
|---|---|---|
| <img src="img/enc/icons/183.png" width="24"> | Bronzový blok | výroba: 9× Bronzový ingot |
| <img src="img/enc/icons/60046.png" width="24"> | Řezaný bronz | výroba: 4× Bronzový blok → 4× |
| <img src="img/enc/icons/60047.png" width="24"> | Zdobený bronz | výroba: 2× Řezaný bronz |
| <img src="img/enc/icons/60048.png" width="24"> | Bronzová mřížka | výroba: 4× Bronzový blok → 4× |
| <img src="img/enc/icons/60049.png" width="24"> | Bronzová žárovka | výroba: 1× Powerstonový prach + 3× Bronzový blok + 1× Pochodeň → 4× |
| <img src="img/enc/icons/60136.png" width="24"> | Bronzový blok (zašlý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60138.png" width="24"> | Bronzový blok (rezavý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60141.png" width="24"> | Řezaný bronz (zašlý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60143.png" width="24"> | Řezaný bronz (rezavý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60146.png" width="24"> | Zdobený bronz (zašlý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60148.png" width="24"> | Zdobený bronz (rezavý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60151.png" width="24"> | Bronzová mřížka (zašlý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60153.png" width="24"> | Bronzová mřížka (rezavý) | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60135.png" width="24"> | Bronzový blok (voskovaný) | výroba: Bronzový blok + Medová kulička |
| <img src="img/enc/icons/60140.png" width="24"> | Řezaný bronz (voskovaný) | výroba: Medová kulička + Řezaný bronz |
| <img src="img/enc/icons/60145.png" width="24"> | Zdobený bronz (voskovaný) | výroba: Medová kulička + Zdobený bronz |
| <img src="img/enc/icons/60150.png" width="24"> | Bronzová mřížka (voskovaný) | výroba: Medová kulička + Bronzová mřížka |
| <img src="img/enc/icons/60080.png" width="24"> | Bronzové schody | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60088.png" width="24"> | Schody z řezaného bronzu | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60096.png" width="24"> | Bronzová deska | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60099.png" width="24"> | Deska z řezaného bronzu | těžba, nález nebo kreativ |
| <img src="img/enc/icons/60134.png" width="24"> | Bronzové dveře | výroba: 6× Bronzový ingot → 3× |
| <img src="img/enc/icons/60118.png" width="24"> | Bronzový poklop | výroba: 6× Bronzový ingot → 2× |

</details>

### Úspěchy

| Úspěch | Jak ho získat | Voxel Bucks |
|---|---|---|
| **Na začátek** | Vytěž první blok. | 10 |
| **Dílna** | Vyrob pracovní stůl. | 15 |
| **Doba kamenná** | Vyrob kamenný krumpáč. | 20 |
| **Horké zboží** | Vytav železný ingot. | 25 |
| **Doba bronzová** | Získej bronzový ingot. | 30 |
| **Ocelové srdce** | Získej ocelový ingot. | 40 |
| **Rudý poklad** | Najdi rubín. | 60 |
| **V plné zbroji** | Nos všechny čtyři kusy brnění. | 50 |
| **Vítej doma** | Postav dveře. | 10 |
| **Zřícenina** | Otevři truhlu ve zřícenině. | 30 |
| **Vrak** | Otevři truhlu v potopené lodi. | 50 |
| **Kobka** | Otevři truhlu v kobce. | 50 |
| **Opuštěný důl** | Otevři truhlu v dole. | 50 |
| **Námořník** | Pluj na loďce. | 25 |
| **Za branou** | Vstup do dimenze úhelného kamene. | 60 |
| **Zimní výlet** | Navštiv zasněžený kraj. | 15 |
| **Au!** | Píchni se o kaktus. | 10 |
| **Horník** | Vytěž 100 bloků. | 40 |
| **Krtek** | Vytěž 1000 bloků. | 100 |
| **Spolu** | Hraj s někým v síti. | 40 |
| **Pán pevnosti** | Otevři truhlu v netherové pevnosti. | 60 |
| **Do prázdnoty** | Vstup do Crimson Void. | 100 |
| **Lovec příšer** | Poraz nějakou příšeru. | 20 |
| **Dobrou noc** | Vyspi se v posteli. | 15 |
| **Přemožitel** | Poraz Karmínového muže. | 500 |
| **Poklady prázdnoty** | Otevři truhlu ve věži nebo kobce Crimson Void. | 80 |
| **Cestovatel** | Otevři truhlu v chrámu, iglú nebo strážní věži. | 40 |
| **Bum!** | Odpal TNT. | 20 |
| **Elektrikář** | Rozsviť powerstonovou lampu. | 30 |
| **Zahradník** | Vypěstuj strom ze sazenice. | 20 |
| **Farmář** | Skliď zralou pšenici. | 25 |
| **Lukostřelec** | Zasáhni bytost šípem. | 25 |
| **Čerstvé mléko** | Podoj krávu do kbelíku. | 15 |
| **Obchodník** | Uzavři obchod s vesničanem. | 30 |
| **Dehtobijec** | Poraz dehtovou příšeru. | 60 |
| **Kartograf** | Vyrob si mapu. | 25 |
| **Výroční výstava** | Navštiv výroční výstavu VoxelCraftu. | 30 |
| **Oslava** | Postav balónek. | 15 |
| **Konfety!** | Vystřel konfety. | 15 |
| **Sběratel** | Vyrob výroční medaili z pěti žetonů. | 100 |
| **Zapomenutá země** | Navštiv Powerstonovou pustinu. | 20 |
| **Opuštěný domov** | Otevři truhlu v zapomenuté vesnici. | 40 |
| **Pod hladinou** | Otevři truhlu v zatopených ruinách. | 50 |
| **Rozbitá brána** | Otevři truhlu u rozpadlého portálu. | 40 |
| **Táborník** | Otevři sud v táboře poutníků. | 30 |
| **Archeolog** | Očisti štěrk s nálezy štětcem. | 30 |
| **Kronikář** | Přečti starý dokument. | 20 |
| **Pád zapomenutého království** | Přečti všech osm starých dokumentů. | 150 |
| **Konec Huskinga** | Poraz Huskinga. | 50 |
| **Světlo do tmy** | Postav pochodeň. | 10 |
| **Kryt** | Zastav útok štítem. | 25 |
| **Obchod s Prasákem** | Dej Prasákovi powerstone. | 40 |
| **Po stopách horníků** | Najdi důl podle průzkumné mapy. | 60 |
| **Nad oblaky** | Vstup do Nebeské říše. | 120 |
| **Brána do nebes** | Vlož dvanáct hlav Karmínového muže do rámu nebeského portálu. | 100 |

### Příkazy

| Příkaz | Co dělá | Jen s povolenými příkazy |
|---|---|---|
| `/help` | Seznam příkazů |  |
| `/list` | Hráči ve hře |  |
| `/seed` | Seed světa |  |
| `/pos` | Kde právě jsi |  |
| `/gamemode <survival\|creative>` | Změní herní režim | ✓ |
| `/tp <x> <y> <z> \| <hráč>` | Teleport (~ = od tvé pozice) | ✓ |
| `/give <věc> [počet]` | Dá ti věc (např. ruby, iron_ingot) | ✓ |
| `/time [day\|noon\|evening\|night\|midnight\|tiky]` | Denní doba | ✓ |
| `/wave` | Zamávat (Ahoj!) |  |
| `/music [on\|off]` | Hudba ve světě zapnout / vypnout |  |
| `/weather <clear\|rain>` | Počasí | ✓ |
| `/summon <pig\|sheep\|slime\|forgotten\|crimson\|husking\|...>` | Přivolá bytost | ✓ |
| `/locate <village\|forgotten\|mineshaft\|fortress\|arena\|frame\|temple>` | Najde nejbližší strukturu |  |
| `/say <text>` | Zpráva do chatu (barvy: &c, &l...) |  |
| `/age <0\|1\|2>` | Stáří bronzu v okruhu 8 bloků: lesklý, zašlý, rezavý | ✓ |
| `/wax` | Navoskuje bronz v okruhu 8 bloků | ✓ |
| `/spawn` | Teleport na spawn | ✓ |
| `/heal` | Doplní zdraví a jídlo | ✓ |
| `/clear` | Vyprázdní inventář | ✓ |
| `/kill` | Zemřeš (jen v Přežití) | ✓ |


</details>

---

<a id="english"></a>
<details>
<summary><b>🇬🇧 English</b> (click to open / close)</summary>

### 📱 Why is there no Android release yet?

![Android release](img/faq/release.jpg)

- **Play Console:** the developer account and the game's Google Play listing are not finished yet.
- **Finding the APKs:** the builds used to be hard to find. Now every version can be downloaded
  from the [`release/`](../release/) folder here on GitHub.

### 🔢 What will the release look like?

![Version numbers](img/faq/versions.jpg)

There is no release yet. The game uses the version numbers of its Android manifest (`versionName`):

| Now | |
|---|---|
| **0.1 – 2.x** | from the first versions up to the Cornerstone and the subway |
| **3.x, 4.x, 5.x** | the next big updates |

Later the numbering will start again from **1.x.x.x**,
for example `1.0.0.0`, `1.0.1.0`, `1.1.0.0`.

### 🪨 What happens to the Cornerstone and the Crimson Void before the release?

![The Cornerstone and the Crimson Void](img/faq/dimensions.jpg)

Many mistakes and bugs need fixing first:

- 🌱 **Crimson Void:** the crimson tree has no sapling yet.
- 💎 **Cornerstone:** its sand has the ruby ore of the normal Overworld.
- ⛏️ **The Cornerstone** gets **its own new ores**, because it uses the wrong Overworld versions now.
- 🐾 **The Cornerstone** has **too few mobs**; new creatures will come.
- ✏️ The word **"Nether"** will be replaced by **Cornerstone** everywhere.

### 🪙 What happens to the coins?

![Voxel Bucks](img/faq/bucks.jpg)

- **"Coins" will be renamed "Voxel Bucks"**, and their look in the menus changes from a coin to a **little cube**.
- The store gets a **new section** where Voxel Bucks can be **bought with real money**.
- **Free spins** (the wheel) and daily rewards stay free, as now.
- **VIP** works as it used to, with a **special bonus for versions 1.x.x.x**.

### 🖥️ How will the game look?

![The green menu](img/faq/ui.jpg)

The look of the menus changed a few times during development. **The current green look stays** and will not change again.

### 🔤 What about the font?

![The font](img/faq/fonts.jpg)

- The earlier alpha and classic font of versions **0.1 – 1.9.9** stays only where it belongs:
  in the **Pre-Alpha Style** modpack, in the older versions and in **VoxelCraft Classic**.
- Everywhere else the **new font (Tiny Caps)** replaces it.

---

## 🚀 Release

### 📅 When does the game come to Google Play?

![Google Play 2027](img/faq/date.jpg)

During **2027**. Maybe on **1 January 2027**, or on **1 February 2027**, when VoxelCraft turns **one year old**.

### 💸 Will the game be free?

![Free](img/faq/free.jpg)

- **Yes.** Downloading and playing are free.
- **Voxel Bucks** can also be bought with money.
- **No ads.**

### 💾 Will my worlds and items carry over to the release?

![Worlds](img/faq/saves.jpg)

**Yes.** Saved worlds, the fixed structure seeds and your inventory all stay.

### 📱 Which phones will the game run on?

![Phones](img/faq/phones.jpg)

- The game is for **Android** and stays supported for as long as **Google Play** works.
- The game runs on **Android 5.0 and newer**.

### 🌐 Will there be online multiplayer?

![Multiplayer](img/faq/online.jpg)

**Yes.** For now only **LAN** play works (players on the same Wi-Fi).

### 🍎 Will there be an iPhone or PC version?

![iPhone and PC](img/faq/platforms.jpg)

It is **planned** for the future.

### 🧪 What is the preview and what is the normal version?

![Normal and preview](img/faq/preview.jpg)

- The **normal version** (now **2.7.1**) is the finished one. **Anyone** can have it.
- The **preview** (now **2.8.0 Preview**) is the next update, still being made. **Only the developer** has it.
- They are separate apps, so both can be installed **side by side**.

### 🐞 How do I report a bug?

![Report a bug](img/faq/bugs.jpg)

**In the game:** Settings → **Report a bug**. Write a short title and a description, and the game opens GitHub with the report filled in (game version, world, phone). Just submit it.

Or open a new **issue** right here on GitHub ([Issues](../../../issues)) and write:

1. **What happened**, and what you expected.
2. **How to make it happen again**, step by step.
3. The **game version** and your **phone** (model and Android version).
4. A **screenshot** or video, if you can.

### 🎥 Can I make videos and stream VoxelCraft?

![Videos](img/faq/videos.jpg)

**Yes!** Videos and streams are welcome.

### 🔑 Why can't old APKs be installed over new ones?

![Old and new APKs](img/faq/apk.jpg)

- Older versions were signed with **another key**, and Android refuses an update signed with a different key.
- Since **2.4.0** the game has a **new app id** (`cz.voxelsoftworks.voxelcraft`) and a **lasting key**.
- From 2.4.0 on, every new APK installs over the previous one without uninstalling.

## 🎮 The game, performance and files

### 🧱 What can you do in VoxelCraft?

![What you can do](img/faq/play.jpg)

- **Build and craft**: hundreds of blocks, tools, armour, backpacks, a wallet, trains, rails and the subway.
- **Explore**: four dimensions, villages, the subway, the lost city, wrecks, temples, mines and camps.
- **Fight and look after**: creatures, the bosses Crimson Man and Sky Guardian, a farm, animals, a tamed fox.
- **Create and play together**: creative mode, add-ons and the Creator, structures, LAN multiplayer, your own skins.

### 🦊 How do I tame a fox and what does it do?

![The fox](img/faq/fox.jpg)

- Tap a fox with **raw meat** (pork, beef, mutton or chicken): it is yours and gets a red collar.
- It follows you and **bites monsters**; tap it while **sneaking** to make it sit and wait, again to take it along.
- Tap it with a **dye** to recolour the collar (16 colours). The Cornerstone's **ash fox** can be tamed too.

### 🗺️ Where do I find the subway, the lost city and other structures?

![Structures on the map](img/faq/structures.jpg)

- On the **map** every structure has its house and colour: subway **pink**, ruined station **white**, lost city **mint**, villages **green**, train wrecks **red-brown**.
- The subway shows on the map within **300 blocks**.
- `/locate metro` (and `/locate city`, `/locate station`…) shows the nearest one.

### 🟫 How do steel and bronze rust?

![Rust and wax](img/faq/rust.jpg)

- Steel and bronze blocks **rust** over time, about a stage per game day.
- **Honey** waxes a block: it never rusts again.
- An **axe** scrapes the wax off, or the rust by one stage.

### 🌌 How do I get into all four dimensions?

![Four dimensions](img/faq/dimensions4.jpg)

- **Cornerstone**: build an **obsidian** frame and light it with **flint and steel**.
- **Crimson Void**: in the Cornerstone build a **void stone** frame and light it with **void flint and steel**; the boss **Crimson Man** waits there.
- **Celestial Reach**: put **12 Crimson Man heads** on the celestial frame; the **Sky Guardian** waits there.

### 🐢 The game stutters, what do I do?

![Device power](img/faq/performance.jpg)

- **Settings > Graphics > Device power**: **Full**, **Smoother+** or **Weak**. The game picks it by itself from the graphics chip and the memory.
- **Weak**: light graphics, 30 FPS, a shorter view (Teclast P30T).
- **Smoother+**: a shorter view, shaders while the phone keeps up (Redmi Note 13 5G, Galaxy A14).
- The **battery saver** turns on by itself at 35 % and less.

### ✨ Why did my shaders switch off?

![Shaders](img/faq/shaders.jpg)

- After entering a world or a dimension the game **waits 20 seconds** for it to load; then, if it stays **under 20 FPS for two 10-second windows in a row**, the shaders **switch off by themselves** with a message (since 2.7.1; 2.7.0 switched them off too soon).
- In **Settings > Graphics** press **Try again**, or switch Graphics to Enhanced.

### 💾 How do I back up and move a world?

![World backup](img/faq/backup.jpg)

- In the world list, **Edit > Export world** saves the whole world as one **.vcworld** file.
- On another phone or after a reinstall add it with **Import**.
- **Game updates never change saved worlds**: new structures and ores show up in new worlds.

### ⛏️ Can I open a Minecraft world?

![A Minecraft world](img/faq/minecraft.jpg)

- Yes: blocks become ours (deepslate > deepstone, redstone > powerstone, copper > bronze, quartz > white concrete).
- Stairs and slabs keep their direction, flat worlds stay flat.
- Mobs, chest contents and doors do **not come across yet**.

### 🧩 Why does a Bedrock or Java mod not work fully?

![What converts](img/faq/partial.jpg)

- **Data and look** convert: blocks, items, creatures, recipes, textures and atlases.
- **Code** (Java classes, Bedrock scripts) cannot run in VoxelCraft, so it is skipped.
- Other games' **UI** is not taken: such an add-on is marked **un-UI**. The **Errors** button in the Add-on Manager shows exactly what happened.

### 🖼️ How do I make my own texture pack?

![Texture pack](img/faq/texturepack.jpg)

- Draw **16×16 PNG** pictures, one per block or item, in `blocks/` and `items/` (or one old `terrain.png`).
- Zip them as **.zip**, **.mcpack** or **.vcpack**.
- Load them in **Settings > Global resources** or with **+ Import** in the Add-on Manager. Anything missing keeps our look.

### 🛡️ Is modding safe?

![Safe modding](img/faq/safe.jpg)

- VoxelCraft add-ons are **only data and pictures**: no code runs.
- An add-on cannot send anything to the internet.
- Every add-on can be **switched off** or **deleted** at any time.

### 📂 What are .vcworld, .vcr, .vct, .vcstructure, .vcpack and .vcaddon?

![VoxelCraft files](img/faq/files.jpg)

| File | What it is | How it works |
|---|---|---|
| **.vcworld** | a whole world in one file | a zip of the world: `level.dat`, creatures, chests, regions; made by Export, loaded by Import |
| **.vcr** | a world region (32 × 32 chunks) | binary and compressed: how the game keeps the world on the phone |
| **.vct** | the same region as readable text | in a .vcworld instead of .vcr; you can open and read it; Import turns it back into .vcr |
| **.vcstructure** | a saved build | saved by the **structure block**, placed into another world (`.mcstructure` works too) |
| **.vcpack** | an add-on with the **look** only (Resource Voxel) | a zip with `manifest.json` and textures, atlases, models or shaders |
| **.vcaddon** | an add-on with **look and behaviour** | a zip with `manifest.json`, `content.json` (blocks, items, creatures), recipes and textures |

## 🛠️ Development

### 🔁 How is a new version made?

![How a version is made](img/faq/versions_made.jpg)

1. **Ideas**: what changes is drawn as a proposal and approved first.
2. **Preview builds**: every build is tested, put on GitHub and numbered (build 1, 2, 3…).
3. **Release**: the finished APK goes to `release/` for everyone; the preview moves on.
4. **Archive**: old music, UI and textures stay in `archived/`, old versions in `release/`.

### 🚫 Why does the game use nothing from Mojang?

![Our own game](img/faq/own.jpg)

- VoxelCraft has **its own engine** in C++ and OpenGL ES.
- **The game's code draws every texture** pixel by pixel; the music is our own.
- It is an unofficial fan project that does not want to copy anything.

### 🔮 What comes in 3.0.0?

![3.0.0](img/faq/v300.jpg)

- **Iron things go**, steel takes their place for good.
- The plan of the big update is shared when it is ready.
- Your worlds stay as they are.

## 🛠️ Changes in 2.7.0

### ⛓️ Why was iron removed?

![Ingots and hardium ore](img/faq/iron.jpg)

- From 2.7.0 iron is **not made into anything**; every recipe takes **steel** instead.
- An iron ingot and coal make a **steel mix**, which the furnace turns into steel.
- Iron tools and armour are no longer made and go completely in **3.0.0**: VoxelCraft is its own game and does not copy other games' tool sets.
- Iron had the nicer colour, so the **steel set** (pickaxe, axe, shovel, sword, armour) took it.

### 🎨 Why did most things get a new look?

![The new textures of 2.7.0](img/faq/redesign.jpg)

- The game grew and the pictures of the first versions looked poor next to the new things.
- 2.7.0 redraws the **spawn eggs** (a pattern per creature), **meat**, **arrow**, **string**, **ingots**, **hardium ore**, **bronze door** and the **buffer stop**.
- The old textures are kept in `archived/ui_2.6.0/` and still play in 2.6.0, for filming.

### 🖌️ Why were the textures so simple before?

![The textures up to 2.6.0](img/faq/old_textures.jpg)

- VoxelCraft uses no Mojang pictures: **the game's code draws every texture** pixel by pixel.
- The first versions were about making things work; the pictures were quick, a few colours and dots.
- Now is the time to polish them: shading, an outline, a shine and a pattern you recognise at once.

### 🎒 How do backpacks work?

![Backpacks on the back](img/faq/backpacks.jpg)

- Six tiers: **leather, steel, steel-bronze, bronze-ruby, hardium, celestium**, from 9 to 54 slots.
- Inside there is **crafting** (2×2, 3×3 from steel-bronze) and from bronze-ruby a **furnace**; the buttons are in a row under the slots.
- Put a backpack on the chestplate slot: it goes **on the back** and a chestplate fits over it. In the game the button above "…" opens it.
- Upgrade: the backpack in the middle and 4 metals of the next tier around it. **The contents stay.**

![The backpack window](img/faq/backpack_window.jpg)

### 🧩 How does the new modding work?

![The add-on manager](img/faq/addons.jpg)

- Menu > **Mods and Packs** opens the **Add-on Manager** with the tabs **Add-ons, Behavior, Resource, Creator, Store**.
- The coloured stripe shows the origin: green VC, orange Java, blue Bedrock, gold Store, red error.
- **+ Import** takes `.vcaddon`, `.vcpack`, `.mcaddon`, `.mcpack`, `.jar` and `.zip`.
- **Behavior Voxel** is the behaviour (blocks, items, mobs, recipes, the world), **Resource Voxel** the look (textures, atlases, models, shaders).

### 🔄 What happens to a Bedrock or Java mod?

![Bedrock > VC conversion](img/faq/bedrock.jpg)

- The game **converts it into the VC language**: blocks, items, mobs, textures, the `terrain_texture.json` and `item_texture.json` atlases, and from `geo.json` it picks the creature's body.
- For every file it writes what happened: **+ converted, ! partly, x not possible**, under the **Errors** button. Nothing fails silently any more.
- **Code** (Java classes, Bedrock scripts) cannot run and the **UI** of other games' mods is not taken: such an add-on is marked **un-UI**.

### 🖌️ How do I make my own block or mob?

![The Creator](img/faq/creator.jpg)

- The **Creator** tab: switch **Block** or **Mob**, type a name, set hardness and light (block) or body and health (mob).
- Paint the 16×16 texture with the **brush** or **fill** in 12 colours and press **Save to .vcaddon**.
- It goes into the add-on **My Creations** and is in the game at once.

### 🐾 What problems did the creatures and cacti have?

- Creatures took **fallen snow** for a whole block and climbed onto it; now they walk through it.
- In **lava** they bounced, because lava pushed them like a hit; now they sink and burn. The ember moth, ash fox and hogman don't mind lava.
- **Cacti** did not hurt for a while; fixed in 2.5.0.
- These bugs like to come back. If you see a creature float, bounce in lava or a cactus that doesn't hurt, **report it with Report a bug** in the settings (it opens GitHub Issues).


## 📖 The game encyclopedia (2.7.1)

The whole game in one place: dimensions, biomes, structures, mechanics, creatures, every block and item with its recipes, achievements and commands. Generated from the game's own data (`tools/encyclopedia_doc.cpp`), so it matches the game.

**Contents:**

- Dimensions
- Biomes
- Structures
- Mechanics
- Blocks and items
- Mobs
- Recipes and materials
- Achievements
- Commands

### Dimensions

#### <img src="img/enc/icons/2.png" width="24"> Overworld

<img src="img/enc/plains.jpg" width="384">

The starting world. Biomes from plains to snowy mountains, caves and deepstone, villages, mineshafts, camps and other structures.

#### <img src="img/enc/icons/87.png" width="24"> Cornerstone Dimension

<img src="img/enc/cornerstone.jpg" width="384">

The dimension behind the obsidian portal: cornerstone, lava seas, the Powerstone Wasteland, hogmen and the great fortress with the portal to the Crimson Void.

#### <img src="img/enc/icons/14.png" width="24"> Crimson Void

<img src="img/enc/void_purple.jpg" width="384">

The purple forest of eye trees, the crimson desert, the lava lands with the Crimson Man's arena, the Forgotten Overworld and three new biomes.

#### <img src="img/enc/icons/1560.png" width="24"> The Celestial Reach

<img src="img/enc/celestial.jpg" width="384">

Floating islands over a white sky, joined by bridges. Twelve heads of the Crimson Man in the portal frame open the way. A fall is final.

#### <img src="img/enc/icons/2.png" width="24"> Old World

<img src="img/enc/old_world.jpg" width="384">

A world type with the generator of versions 0.1-0.3: old terrain, textures, joystick controls and menu. Creative only, no mobs or music.

### Biomes

#### <img src="img/enc/icons/2.png" width="24"> Plains

<img src="img/enc/plains.jpg" width="384">

Open green land with flowers and the odd oak.

#### <img src="img/enc/icons/1565.png" width="24"> Dappled Forest

<img src="img/enc/dappled.jpg" width="384">

Gray trees with red, orange and yellow leaves, orange grass. Mostly snowy, now and then a light rain. Camps are often found here.

#### <img src="img/enc/icons/159.png" width="24"> Mountains

<img src="img/enc/mountains.jpg" width="384">

Tall hills with hardened stone on the cliffs and snow on the peaks.

#### <img src="img/enc/icons/17.png" width="24"> Forest

<img src="img/enc/forest.jpg" width="384">

A dense oak forest with ferns in the shade. A good place for wood.

#### <img src="img/enc/icons/12.png" width="24"> Desert

<img src="img/enc/desert.jpg" width="384">

Sand, cacti and sandstone. Desert temples stand here.

#### <img src="img/enc/icons/162.png" width="24"> Snowy Taiga

<img src="img/enc/taiga.jpg" width="384">

Spruce forests under snow. Snow falls here and stays.

#### <img src="img/enc/icons/79.png" width="24"> Ice Plains

<img src="img/enc/ice.jpg" width="384">

Frozen land with frozen water and igloos.

#### <img src="img/enc/icons/21.png" width="24"> Purple Forest

<img src="img/enc/void_purple.jpg" width="384">

The Crimson Void's forest: void grass and trees with an eye in the trunk.

#### <img src="img/enc/icons/23.png" width="24"> Crimson Desert

<img src="img/enc/void_crimson.jpg" width="384">

Dunes of red sand, spires of crimson stone, crystals and buried ruins.

#### <img src="img/enc/icons/1978.png" width="24"> Crimsonium Forest

<img src="img/enc/crimsonium.jpg" width="384">

A dense forest of red crimsonium trees. Rare crimsonium ore under the floor, forest shrines with loot among the trees.

#### <img src="img/enc/icons/1981.png" width="24"> Shattered Void

<img src="img/enc/shattered.jpg" width="384">

Broken rock split by clefts, rock arches, crumbling towers and crystals of void shards.

#### <img src="img/enc/icons/1984.png" width="24"> Void Shards Desert

<img src="img/enc/shard_desert.jpg" width="384">

Dark dunes with crystal clusters, buried ruins and hidden chambers under the sand, given away only by a crystal on a little pillar.

### Structures

#### <img src="img/enc/icons/1575.png" width="24"> Camp

<img src="img/enc/camp.jpg" width="384">

A wool tent, a fire pit and a barrel of supplies. Most often in the Dappled Forest.

#### <img src="img/enc/icons/1557.png" width="24"> Celestial Bridges

<img src="img/enc/bridges.jpg" width="384">

The Celestial Reach's first structures: level bridges between the islands with railings, posts and chains.

#### <img src="img/enc/icons/1988.png" width="24"> Forest Shrine

<img src="img/enc/shrine.jpg" width="384">

Crimsonium pillars, a slab roof with a crystal on top. A chest with crimsonium and shards.

#### <img src="img/enc/icons/1989.png" width="24"> Hidden Chamber

<img src="img/enc/shard_desert.jpg" width="384">

A dark sandstone room 13 blocks under the dunes. Look for a crystal on a small pillar.

#### <img src="img/enc/icons/153.png" width="24"> Village

<img src="img/enc/village.jpg" width="384">

A round settlement with houses, a well, paths and wheat fields. Villagers live there; you can trade with them.

#### <img src="img/enc/icons/4.png" width="24"> Ruined Village

<img src="img/enc/ruined_village.jpg" width="384">

An abandoned village of crumbling houses and overgrown paths. No villagers, but the chests are still there.

#### <img src="img/enc/icons/50.png" width="24"> Ruined House

<img src="img/enc/ruin.jpg" width="384">

Broken stone and wooden walls on plains and in forests, with a loot chest inside.

#### <img src="img/enc/icons/51.png" width="24"> Shipwreck

<img src="img/enc/shipwreck.jpg" width="384">

A wooden ship wrecked on the sea floor or a shore. A chest with maps, food and iron.

#### <img src="img/enc/icons/52.png" width="24"> Dungeon

<img src="img/enc/dungeon.jpg" width="384">

A room of mossy stone bricks deep underground, with a chest.

#### <img src="img/enc/icons/1548.png" width="24"> Cave Dungeon

<img src="img/enc/cave_dungeon.jpg" width="384">

A small room in the rock with a monster spawner and loot. Break the spawner before it overwhelms you.

#### <img src="img/enc/icons/53.png" width="24"> Abandoned Mine

Long wooden corridors underground, joined to the caves. Chests with ores and explorer maps.

#### <img src="img/enc/icons/94.png" width="24"> Desert Temple

<img src="img/enc/desert_temple.jpg" width="384">

A sandstone pyramid in the desert, with a hidden chest inside.

#### <img src="img/enc/icons/95.png" width="24"> Igloo

<img src="img/enc/igloo.jpg" width="384">

A dome of snow on the ice plains and in the taiga, with a chest and a carpet.

#### <img src="img/enc/icons/96.png" width="24"> Watchtower

<img src="img/enc/watchtower.jpg" width="384">

A tall tower with a wooden platform on top and a chest. A fine view around.

#### <img src="img/enc/icons/55.png" width="24"> Great Fortress

<img src="img/enc/fortress.jpg" width="384">

A huge nether brick fortress in the Cornerstone Dimension: archive, forge, throne hall and crypt. The portal to the Crimson Void is in its hall.

#### <img src="img/enc/icons/1515.png" width="24"> Forgotten Village

<img src="img/enc/forgotten_village.jpg" width="384">

An abandoned village of the forgotten powerstone kingdom in the wasteland, with a column of lamps in the middle.

#### <img src="img/enc/icons/1516.png" width="24"> Drowned Ruins

<img src="img/enc/drowned_ruins.jpg" width="384">

The remains of the forgotten kingdom's buildings under lakes and seas.

#### <img src="img/enc/icons/1517.png" width="24"> Broken Portal

<img src="img/enc/broken_portal.jpg" width="384">

The ruin of an old portal. It points the way to a fortress in the Cornerstone Dimension.

#### <img src="img/enc/icons/1532.png" width="24"> Crimson Mineshaft

Abandoned corridors of crimson wood in the Cornerstone Dimension.

#### <img src="img/enc/icons/93.png" width="24"> Void Dungeon

<img src="img/enc/void_dungeon.jpg" width="384">

A room of void bricks under the ground of the Crimson Void.

#### <img src="img/enc/icons/26.png" width="24"> Void Tower

<img src="img/enc/void_tower.jpg" width="384">

A tall dark tower in the Crimson Void, with a chest and a compass to the arena.

#### <img src="img/enc/icons/39.png" width="24"> Void Watchtower

<img src="img/enc/void_watchtower.jpg" width="384">

A watchtower of void wood over the purple forest.

#### <img src="img/enc/icons/1525.png" width="24"> Crimson Man's Arena

<img src="img/enc/arena.jpg" width="384">

A round arena over the lava with pillars of eyes. The boss, the Crimson Man, waits here.

#### <img src="img/enc/icons/1552.png" width="24"> Celestial Portal Frame

<img src="img/enc/portal.jpg" width="384">

Twelve frames in the Forgotten Overworld. Put the 12 heads of the Crimson Man in them and the portal opens. The celestial compass points to them.

#### <img src="img/enc/icons/1579.png" width="24"> Celestial Temple

<img src="img/enc/temple.jpg" width="384">

An island of white stone with a ring of twelve crystal pillars. On the dais waits the boss, the Sky Guardian, who gives the Celestial Wings.

### Mechanics

#### <img src="img/enc/icons/1946.png" width="24"> Concrete

<img src="img/enc/concrete.jpg" width="384">

Concrete powder in 16 colours falls like sand. In water or in the rain it sets into concrete. Gray dusty concrete is fired in the furnace.

#### <img src="img/enc/icons/1628.png" width="24"> Dyes

From flowers, leaves, coal, clay, cactus and powerstone; more colours by mixing. They colour wool and concrete.

#### <img src="img/enc/icons/1570.png" width="24"> Cutters

<img src="img/enc/cutters.jpg" width="384">

The stonecutter cuts blocks into stairs and slabs with no waste. The wood cutter gives 6 planks from a log, stairs, slabs and fences.

#### <img src="img/enc/icons/49.png" width="24"> The Portals

<img src="img/enc/portal.jpg" width="384">

The obsidian portal leads to the Cornerstone Dimension, the fortress portal to the Crimson Void, the frame with 12 heads to the Celestial Reach.

### Mobs

| | Creature | Health | Attack | Kind |
|---|---|---|---|---|
| <img src="img/enc/icons/196.png" width="24"> | **Pig** | 10 | 0 | peaceful |
| <img src="img/enc/icons/197.png" width="24"> | **Sheep** | 8 | 0 | peaceful |
| <img src="img/enc/icons/198.png" width="24"> | **Pumpkin Slime** | 12 | 2 | hostile |
| <img src="img/enc/icons/199.png" width="24"> | **Forgotten Man** | 20 | 3 | hostile |
| <img src="img/enc/icons/1619.png" width="24"> | **Crimson Man** | 300 | 9 | boss |
| <img src="img/enc/icons/155.png" width="24"> | **Chicken** | 4 | 0 | peaceful |
| <img src="img/enc/icons/156.png" width="24"> | **Cow** | 10 | 0 | peaceful |
| <img src="img/enc/icons/157.png" width="24"> | **Villager** | 20 | 0 | peaceful |
| <img src="img/enc/icons/1303.png" width="24"> | **Spider** | 16 | 2 | hostile |
| <img src="img/enc/icons/1304.png" width="24"> | **Bat** | 6 | 0 | peaceful |
| <img src="img/enc/icons/1305.png" width="24"> | **TNT Zombie** | 20 | 0 | hostile |
| <img src="img/enc/icons/1306.png" width="24"> | **Tar Monster** | 30 | 5 | hostile |
| <img src="img/enc/icons/1307.png" width="24"> | **Husking** | 24 | 4 | hostile |
| <img src="img/enc/icons/1308.png" width="24"> | **Hogman** | 22 | 5 | hostile |
| <img src="img/enc/icons/60289.png" width="24"> | **Celestling** | 8 | 0 | peaceful |
| <img src="img/enc/icons/60290.png" width="24"> | **Reach Stalker** | 26 | 5 | hostile |
| <img src="img/enc/icons/60291.png" width="24"> | **Sky Drifter** | 14 | 0 | peaceful |
| <img src="img/enc/icons/1996.png" width="24"> | **Sky Guardian** | 260 | 8 | boss |
| <img src="img/enc/icons/60292.png" width="24"> | **Cornerstone Golem** | 60 | 7 | peaceful |
| <img src="img/enc/icons/60293.png" width="24"> | **Moss Lurker** | 16 | 3 | hostile |
| <img src="img/enc/icons/60294.png" width="24"> | **Sporeling** | 8 | 0 | peaceful |
| <img src="img/enc/icons/60295.png" width="24"> | **Ember Moth** | 4 | 0 | peaceful |
| <img src="img/enc/icons/60296.png" width="24"> | **Ash Fox** | 12 | 0 | peaceful |
| <img src="img/enc/icons/60297.png" width="24"> | **Fox** | 12 | 0 | peaceful |
| <img src="img/enc/icons/60298.png" width="24"> | **Ghost** | 10 | 0 | peaceful |
| <img src="img/enc/icons/60299.png" width="24"> | **Snowman** | 20 | 0 | peaceful |

### Blocks and items

#### <img src="img/enc/icons/98.png" width="24"> Building Blocks (189)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/1.png" width="24"> | Stone | furnace: Cobblestone |
| <img src="img/enc/icons/4.png" width="24"> | Cobblestone | mining, found or creative |
| <img src="img/enc/icons/48.png" width="24"> | Mossy cobblestone | crafting: Cobblestone + Oak leaves |
| <img src="img/enc/icons/98.png" width="24"> | Stone bricks | crafting: 4× Stone → 4× |
| <img src="img/enc/icons/45.png" width="24"> | Bricks | furnace: Clay |
| <img src="img/enc/icons/5.png" width="24"> | Oak planks | crafting: Oak log → 4×<br>crafting: Spruce Planks<br>crafting: Void Planks |
| <img src="img/enc/icons/17.png" width="24"> | Oak log | mining, found or creative |
| <img src="img/enc/icons/24.png" width="24"> | Sandstone | crafting: 4× Sand |
| <img src="img/enc/icons/49.png" width="24"> | Obsidian | mining, found or creative |
| <img src="img/enc/icons/184.png" width="24"> | Steel block | crafting: 9× Steel ingot |
| <img src="img/enc/icons/60155.png" width="24"> | Carved Cornerstone | crafting: 4× Cornerstone → 4× |
| <img src="img/enc/icons/60156.png" width="24"> | Gilded Cobble | crafting: 5× Cornerstone + 4× Bronze ingot → 5× |
| <img src="img/enc/icons/60157.png" width="24"> | Amber Resin | mining, found or creative |
| <img src="img/enc/icons/60159.png" width="24"> | Prismatide Pillar | mining, found or creative |
| <img src="img/enc/icons/60160.png" width="24"> | Siltstone | mining, found or creative |
| <img src="img/enc/icons/60161.png" width="24"> | Luminous Salt | mining, found or creative |
| <img src="img/enc/icons/60164.png" width="24"> | Spore Pad | mining, found or creative |
| <img src="img/enc/icons/60165.png" width="24"> | Petrified Bark | mining, found or creative |
| <img src="img/enc/icons/60166.png" width="24"> | Mossy Cornerstone | mining, found or creative |
| <img src="img/enc/icons/60171.png" width="24"> | Thin Ice | mining, found or creative |
| <img src="img/enc/icons/60172.png" width="24"> | Ice Bricks | crafting: 4× Ice → 4× |
| <img src="img/enc/icons/60173.png" width="24"> | Snowflake Block | crafting: 5× Snow block → 2× |
| <img src="img/enc/icons/60174.png" width="24"> | Dried Snow | mining, found or creative |
| <img src="img/enc/icons/185.png" width="24"> | Ruby block | crafting: 9× Ruby |
| <img src="img/enc/icons/160.png" width="24"> | Spruce Planks | crafting: Spruce Log → 4× |
| <img src="img/enc/icons/162.png" width="24"> | Spruce Log | mining, found or creative |
| <img src="img/enc/icons/159.png" width="24"> | Hardened Stone | mining, found or creative |
| <img src="img/enc/icons/26.png" width="24"> | Void Stone Bricks | crafting: 4× Void Stone → 4× |
| <img src="img/enc/icons/39.png" width="24"> | Void Planks | crafting: Void Log → 4× |
| <img src="img/enc/icons/36.png" width="24"> | Netherperk Planks | crafting: Netherperk Log → 4× |
| <img src="img/enc/icons/1000.png" width="24"> | Oak Stairs | crafting: 6× Oak planks → 4× |
| <img src="img/enc/icons/1008.png" width="24"> | Spruce Stairs | crafting: 6× Spruce Planks → 4× |
| <img src="img/enc/icons/1016.png" width="24"> | Void Stairs | crafting: 6× Void Planks → 4× |
| <img src="img/enc/icons/1024.png" width="24"> | Netherperk Stairs | crafting: 6× Netherperk Planks → 4× |
| <img src="img/enc/icons/1032.png" width="24"> | Cobblestone Stairs | crafting: 6× Cobblestone → 4× |
| <img src="img/enc/icons/1040.png" width="24"> | Stone Stairs | crafting: 6× Stone → 4× |
| <img src="img/enc/icons/1048.png" width="24"> | Stone Brick Stairs | crafting: 6× Stone bricks → 4× |
| <img src="img/enc/icons/1056.png" width="24"> | Mossy Cobblestone Stairs | crafting: 6× Mossy cobblestone → 4× |
| <img src="img/enc/icons/1064.png" width="24"> | Brick Stairs | crafting: 6× Bricks → 4× |
| <img src="img/enc/icons/1072.png" width="24"> | Sandstone Stairs | crafting: 6× Sandstone → 4× |
| <img src="img/enc/icons/1080.png" width="24"> | Cornerstone Brick Stairs | crafting: 6× Cornerstone Bricks → 4× |
| <img src="img/enc/icons/1088.png" width="24"> | Void Brick Stairs | crafting: 6× Void Stone Bricks → 4× |
| <img src="img/enc/icons/1100.png" width="24"> | Oak Slab | crafting: 3× Oak planks → 6× |
| <img src="img/enc/icons/1103.png" width="24"> | Spruce Slab | crafting: 3× Spruce Planks → 6× |
| <img src="img/enc/icons/1106.png" width="24"> | Void Slab | crafting: 3× Void Planks → 6× |
| <img src="img/enc/icons/1109.png" width="24"> | Netherperk Slab | crafting: 3× Netherperk Planks → 6× |
| <img src="img/enc/icons/1112.png" width="24"> | Cobblestone Slab | crafting: 3× Cobblestone → 6× |
| <img src="img/enc/icons/1115.png" width="24"> | Stone Slab | crafting: 3× Stone → 6× |
| <img src="img/enc/icons/1118.png" width="24"> | Stone Brick Slab | crafting: 3× Stone bricks → 6× |
| <img src="img/enc/icons/1121.png" width="24"> | Mossy Cobblestone Slab | crafting: 3× Mossy cobblestone → 6× |
| <img src="img/enc/icons/1124.png" width="24"> | Brick Slab | crafting: 3× Bricks → 6× |
| <img src="img/enc/icons/1127.png" width="24"> | Sandstone Slab | crafting: 3× Sandstone → 6× |
| <img src="img/enc/icons/1130.png" width="24"> | Cornerstone Brick Slab | crafting: 3× Cornerstone Bricks → 6× |
| <img src="img/enc/icons/1133.png" width="24"> | Void Brick Slab | crafting: 3× Void Stone Bricks → 6× |
| <img src="img/enc/icons/1201.png" width="24"> | Hardened Tar | crafting: 4× Tar Ball |
| <img src="img/enc/icons/1160.png" width="24"> | Crimson Stairs | crafting: 6× Crimson Planks → 4× |
| <img src="img/enc/icons/1168.png" width="24"> | Deepstone Stairs | crafting: 6× Deepstone → 4× |
| <img src="img/enc/icons/1176.png" width="24"> | Deepstone Brick Stairs | crafting: 6× Deepstone Bricks → 4× |
| <img src="img/enc/icons/1184.png" width="24"> | Mossy Deepstone Brick Stairs | crafting: 6× Mossy Deepstone Bricks → 4× |
| <img src="img/enc/icons/1148.png" width="24"> | Crimson Slab | crafting: 3× Crimson Planks → 6× |
| <img src="img/enc/icons/1151.png" width="24"> | Deepstone Slab | crafting: 3× Deepstone → 6× |
| <img src="img/enc/icons/1154.png" width="24"> | Deepstone Brick Slab | crafting: 3× Deepstone Bricks → 6× |
| <img src="img/enc/icons/1157.png" width="24"> | Mossy Deepstone Brick Slab | crafting: 3× Mossy Deepstone Bricks → 6× |
| <img src="img/enc/icons/1513.png" width="24"> | Cracked Cornerstone Bricks | furnace: Cornerstone Bricks |
| <img src="img/enc/icons/1514.png" width="24"> | Chiseled Cornerstone Bricks | crafting: 2× Cornerstone Brick Slab |
| <img src="img/enc/icons/1525.png" width="24"> | Void Brick Eye | mining, found or creative |
| <img src="img/enc/icons/1532.png" width="24"> | Crimson Planks | crafting: Crimson Log → 4× |
| <img src="img/enc/icons/1543.png" width="24"> | Deepstone Bricks | crafting: 4× Deepstone → 4× |
| <img src="img/enc/icons/1544.png" width="24"> | Cracked Deepstone Bricks | furnace: Deepstone Bricks |
| <img src="img/enc/icons/1545.png" width="24"> | Mossy Deepstone Bricks | crafting: Deepstone Bricks + Vines |
| <img src="img/enc/icons/1546.png" width="24"> | Rotten Planks | mining, found or creative |
| <img src="img/enc/icons/1552.png" width="24"> | Celestial Portal Frame | mining, found or creative |
| <img src="img/enc/icons/1557.png" width="24"> | Celestial Planks | crafting: Celestial Log → 4× |
| <img src="img/enc/icons/1558.png" width="24"> | Celestial Stone Bricks | crafting: 4× Celestial Soul Stone → 4× |
| <img src="img/enc/icons/1563.png" width="24"> | Dusty Concrete | crafting: Sand + Sand + Gravel + Gravel → 4× |
| <img src="img/enc/icons/1564.png" width="24"> | Hardened Concrete | furnace: Dusty Concrete |
| <img src="img/enc/icons/1566.png" width="24"> | Gray Planks | crafting: Gray Log → 4× |
| <img src="img/enc/icons/1720.png" width="24"> | Dusty Concrete Stairs | crafting: 6× Dusty Concrete → 4× |
| <img src="img/enc/icons/1728.png" width="24"> | Hardened Concrete Stairs | crafting: 6× Hardened Concrete → 4× |
| <img src="img/enc/icons/1736.png" width="24"> | Gray Wood Stairs | crafting: 6× Gray Planks → 4× |
| <img src="img/enc/icons/1744.png" width="24"> | Celestial Stairs | crafting: 6× Celestial Planks → 4× |
| <img src="img/enc/icons/1752.png" width="24"> | Celestial Brick Stairs | crafting: 6× Celestial Stone Bricks → 4× |
| <img src="img/enc/icons/1700.png" width="24"> | Dusty Concrete Slab | crafting: 3× Dusty Concrete → 6× |
| <img src="img/enc/icons/1703.png" width="24"> | Hardened Concrete Slab | crafting: 3× Hardened Concrete → 6× |
| <img src="img/enc/icons/1706.png" width="24"> | Gray Wood Slab | crafting: 3× Gray Planks → 6× |
| <img src="img/enc/icons/1709.png" width="24"> | Celestial Slab | crafting: 3× Celestial Planks → 6× |
| <img src="img/enc/icons/1712.png" width="24"> | Celestial Brick Slab | crafting: 3× Celestial Stone Bricks → 6× |
| <img src="img/enc/icons/1570.png" width="24"> | Stonecutter | crafting: 3× Stone + 1× Steel ingot |
| <img src="img/enc/icons/1572.png" width="24"> | Wood Cutter | crafting: 3× Oak planks + 1× Steel ingot |
| <img src="img/enc/icons/1571.png" width="24"> | Barrel | crafting: 6× Oak planks + 2× Oak Slab |
| <img src="img/enc/icons/1581.png" width="24"> | Campfire | crafting: 3× Oak log + 3× Stick + 1× Coal |
| <img src="img/enc/icons/1583.png" width="24"> | Honey Block | crafting: 4× Honey Ball |
| <img src="img/enc/icons/1979.png" width="24"> | Crimsonium Planks | crafting: Crimsonium Log → 4× |
| <img src="img/enc/icons/1982.png" width="24"> | Shattered Stone Bricks | crafting: 4× Shattered Stone → 4× |
| <img src="img/enc/icons/1985.png" width="24"> | Dark Sandstone | crafting: 4× Dark Sand |
| <img src="img/enc/icons/1660.png" width="24"> | Crimsonium Stairs | crafting: 6× Crimsonium Planks → 4× |
| <img src="img/enc/icons/1668.png" width="24"> | Shattered Stone Stairs | crafting: 6× Shattered Stone → 4× |
| <img src="img/enc/icons/1676.png" width="24"> | Shattered Brick Stairs | crafting: 6× Shattered Stone Bricks → 4× |
| <img src="img/enc/icons/1684.png" width="24"> | Dark Sandstone Stairs | crafting: 6× Dark Sandstone → 4× |
| <img src="img/enc/icons/1646.png" width="24"> | Crimsonium Slab | crafting: 3× Crimsonium Planks → 6× |
| <img src="img/enc/icons/1649.png" width="24"> | Shattered Stone Slab | crafting: 3× Shattered Stone → 6× |
| <img src="img/enc/icons/1652.png" width="24"> | Shattered Brick Slab | crafting: 3× Shattered Stone Bricks → 6× |
| <img src="img/enc/icons/1655.png" width="24"> | Dark Sandstone Slab | crafting: 3× Dark Sandstone → 6× |
| <img src="img/enc/icons/1930.png" width="24"> | White Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + White Dye → 8× |
| <img src="img/enc/icons/1931.png" width="24"> | Orange Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Orange Dye → 8× |
| <img src="img/enc/icons/1932.png" width="24"> | Magenta Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Magenta Dye → 8× |
| <img src="img/enc/icons/1933.png" width="24"> | Light Blue Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Light Blue Dye → 8× |
| <img src="img/enc/icons/1934.png" width="24"> | Yellow Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Yellow Dye → 8× |
| <img src="img/enc/icons/1935.png" width="24"> | Lime Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Lime Dye → 8× |
| <img src="img/enc/icons/1936.png" width="24"> | Pink Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Pink Dye → 8× |
| <img src="img/enc/icons/1937.png" width="24"> | Gray Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Gray Dye → 8× |
| <img src="img/enc/icons/1938.png" width="24"> | Light Gray Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Light Gray Dye → 8× |
| <img src="img/enc/icons/1939.png" width="24"> | Cyan Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Cyan Dye → 8× |
| <img src="img/enc/icons/1940.png" width="24"> | Purple Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Purple Dye → 8× |
| <img src="img/enc/icons/1941.png" width="24"> | Blue Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Blue Dye → 8× |
| <img src="img/enc/icons/1942.png" width="24"> | Brown Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Brown Dye → 8× |
| <img src="img/enc/icons/1943.png" width="24"> | Green Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Green Dye → 8× |
| <img src="img/enc/icons/1944.png" width="24"> | Red Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Red Dye → 8× |
| <img src="img/enc/icons/1945.png" width="24"> | Black Concrete Powder | crafting: Sand + Sand + Sand + Sand + Gravel + Gravel + Gravel + Gravel + Black Dye → 8× |
| <img src="img/enc/icons/1946.png" width="24"> | White Concrete | mining, found or creative |
| <img src="img/enc/icons/1947.png" width="24"> | Orange Concrete | mining, found or creative |
| <img src="img/enc/icons/1948.png" width="24"> | Magenta Concrete | mining, found or creative |
| <img src="img/enc/icons/1949.png" width="24"> | Light Blue Concrete | mining, found or creative |
| <img src="img/enc/icons/1950.png" width="24"> | Yellow Concrete | mining, found or creative |
| <img src="img/enc/icons/1951.png" width="24"> | Lime Concrete | mining, found or creative |
| <img src="img/enc/icons/1952.png" width="24"> | Pink Concrete | mining, found or creative |
| <img src="img/enc/icons/1953.png" width="24"> | Gray Concrete | mining, found or creative |
| <img src="img/enc/icons/1954.png" width="24"> | Light Gray Concrete | mining, found or creative |
| <img src="img/enc/icons/1955.png" width="24"> | Cyan Concrete | mining, found or creative |
| <img src="img/enc/icons/1956.png" width="24"> | Purple Concrete | mining, found or creative |
| <img src="img/enc/icons/1957.png" width="24"> | Blue Concrete | mining, found or creative |
| <img src="img/enc/icons/1958.png" width="24"> | Brown Concrete | mining, found or creative |
| <img src="img/enc/icons/1959.png" width="24"> | Green Concrete | mining, found or creative |
| <img src="img/enc/icons/1960.png" width="24"> | Red Concrete | mining, found or creative |
| <img src="img/enc/icons/1961.png" width="24"> | Black Concrete | mining, found or creative |
| <img src="img/enc/icons/1962.png" width="24"> | White Carpet | crafting: 2× White wool → 3× |
| <img src="img/enc/icons/1963.png" width="24"> | Orange Carpet | crafting: 2× Orange wool → 3× |
| <img src="img/enc/icons/1964.png" width="24"> | Magenta Carpet | crafting: 2× Magenta wool → 3× |
| <img src="img/enc/icons/1965.png" width="24"> | Light Blue Carpet | crafting: 2× Light blue wool → 3× |
| <img src="img/enc/icons/1966.png" width="24"> | Yellow Carpet | crafting: 2× Yellow wool → 3× |
| <img src="img/enc/icons/1967.png" width="24"> | Lime Carpet | crafting: 2× Lime wool → 3× |
| <img src="img/enc/icons/1968.png" width="24"> | Pink Carpet | crafting: 2× Pink wool → 3× |
| <img src="img/enc/icons/1969.png" width="24"> | Gray Carpet | crafting: 2× Gray wool → 3× |
| <img src="img/enc/icons/1970.png" width="24"> | Light Gray Carpet | crafting: 2× Light gray wool → 3× |
| <img src="img/enc/icons/1971.png" width="24"> | Cyan Carpet | crafting: 2× Cyan wool → 3× |
| <img src="img/enc/icons/1972.png" width="24"> | Purple Carpet | crafting: 2× Purple wool → 3× |
| <img src="img/enc/icons/1973.png" width="24"> | Blue Carpet | crafting: 2× Blue wool → 3× |
| <img src="img/enc/icons/1974.png" width="24"> | Brown Carpet | crafting: 2× Brown wool → 3× |
| <img src="img/enc/icons/1975.png" width="24"> | Green Carpet | crafting: 2× Green wool → 3× |
| <img src="img/enc/icons/1976.png" width="24"> | Red Carpet | crafting: 2× Red wool → 3× |
| <img src="img/enc/icons/1977.png" width="24"> | Black Carpet | crafting: 2× Black wool → 3× |
| <img src="img/enc/icons/1810.png" width="24"> | Hardened Stone Stairs | crafting: 6× Hardened Stone → 4× |
| <img src="img/enc/icons/1818.png" width="24"> | Void Stone Stairs | crafting: 6× Void Stone → 4× |
| <img src="img/enc/icons/1826.png" width="24"> | Netherrack Stairs | crafting: 6× Cornerstone → 4× |
| <img src="img/enc/icons/1834.png" width="24"> | Obsidian Stairs | crafting: 6× Obsidian → 4× |
| <img src="img/enc/icons/1842.png" width="24"> | Crimson Stone Stairs | crafting: 6× Crimson Stone → 4× |
| <img src="img/enc/icons/1850.png" width="24"> | Souled Stone Stairs | crafting: 6× Souled Stone → 4× |
| <img src="img/enc/icons/1858.png" width="24"> | Celestial Stone Stairs | crafting: 6× Celestial Soul Stone → 4× |
| <img src="img/enc/icons/1866.png" width="24"> | Dripstone Stairs | crafting: 6× Dripstone Block → 4× |
| <img src="img/enc/icons/1874.png" width="24"> | Tar Stairs | crafting: 6× Hardened Tar → 4× |
| <img src="img/enc/icons/1890.png" width="24"> | Cracked Cornerstone Brick Stairs | crafting: 6× Cracked Cornerstone Bricks → 4× |
| <img src="img/enc/icons/1898.png" width="24"> | Cracked Deepstone Brick Stairs | crafting: 6× Cracked Deepstone Bricks → 4× |
| <img src="img/enc/icons/1906.png" width="24"> | Rotten Plank Stairs | crafting: 6× Rotten Planks → 4× |
| <img src="img/enc/icons/1914.png" width="24"> | Mossy Deepstone Stairs | crafting: 6× Mossy Deepstone → 4× |
| <img src="img/enc/icons/1760.png" width="24"> | Hardened Stone Slab | crafting: 3× Hardened Stone → 6× |
| <img src="img/enc/icons/1763.png" width="24"> | Void Stone Slab | crafting: 3× Void Stone → 6× |
| <img src="img/enc/icons/1766.png" width="24"> | Netherrack Slab | crafting: 3× Cornerstone → 6× |
| <img src="img/enc/icons/1769.png" width="24"> | Obsidian Slab | crafting: 3× Obsidian → 6× |
| <img src="img/enc/icons/1772.png" width="24"> | Crimson Stone Slab | crafting: 3× Crimson Stone → 6× |
| <img src="img/enc/icons/1775.png" width="24"> | Souled Stone Slab | crafting: 3× Souled Stone → 6× |
| <img src="img/enc/icons/1778.png" width="24"> | Celestial Stone Slab | crafting: 3× Celestial Soul Stone → 6× |
| <img src="img/enc/icons/1781.png" width="24"> | Dripstone Slab | crafting: 3× Dripstone Block → 6× |
| <img src="img/enc/icons/1784.png" width="24"> | Tar Slab | crafting: 3× Hardened Tar → 6× |
| <img src="img/enc/icons/1790.png" width="24"> | Cracked Cornerstone Brick Slab | crafting: 3× Cracked Cornerstone Bricks → 6× |
| <img src="img/enc/icons/1793.png" width="24"> | Cracked Deepstone Brick Slab | crafting: 3× Cracked Deepstone Bricks → 6× |
| <img src="img/enc/icons/1796.png" width="24"> | Rotten Plank Slab | crafting: 3× Rotten Planks → 6× |
| <img src="img/enc/icons/1799.png" width="24"> | Mossy Deepstone Slab | crafting: 3× Mossy Deepstone → 6× |
| <img src="img/enc/icons/19.png" width="24"> | Void Log | mining, found or creative |
| <img src="img/enc/icons/21.png" width="24"> | Eye Log | mining, found or creative |
| <img src="img/enc/icons/33.png" width="24"> | Netherperk Log | mining, found or creative |
| <img src="img/enc/icons/1533.png" width="24"> | Crimson Log | mining, found or creative |
| <img src="img/enc/icons/1556.png" width="24"> | Celestial Log | mining, found or creative |
| <img src="img/enc/icons/1565.png" width="24"> | Gray Log | mining, found or creative |
| <img src="img/enc/icons/1978.png" width="24"> | Crimsonium Log | mining, found or creative |
| <img src="img/enc/icons/60309.png" width="24"> | Waxed Steel Block | mining, found or creative |
| <img src="img/enc/icons/60310.png" width="24"> | Rusty Steel Block | mining, found or creative |
| <img src="img/enc/icons/60311.png" width="24"> | Waxed Rusty Steel Block | mining, found or creative |
| <img src="img/enc/icons/60312.png" width="24"> | Very Rusty Steel Block | mining, found or creative |
| <img src="img/enc/icons/60313.png" width="24"> | Waxed Very Rusty Steel Block | mining, found or creative |

</details>

#### <img src="img/enc/icons/20.png" width="24"> Glass (18)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/20.png" width="24"> | Glass | furnace: Sand<br>furnace: Red Sand |
| <img src="img/enc/icons/1210.png" width="24"> | White Stained Glass | crafting: 8× Glass + 1× White wool → 8× |
| <img src="img/enc/icons/1211.png" width="24"> | Orange Stained Glass | crafting: 8× Glass + 1× Orange wool → 8× |
| <img src="img/enc/icons/1212.png" width="24"> | Magenta Stained Glass | crafting: 8× Glass + 1× Magenta wool → 8× |
| <img src="img/enc/icons/1213.png" width="24"> | Light Blue Stained Glass | crafting: 8× Glass + 1× Light blue wool → 8× |
| <img src="img/enc/icons/1214.png" width="24"> | Yellow Stained Glass | crafting: 8× Glass + 1× Yellow wool → 8× |
| <img src="img/enc/icons/1215.png" width="24"> | Lime Stained Glass | crafting: 8× Glass + 1× Lime wool → 8× |
| <img src="img/enc/icons/1216.png" width="24"> | Pink Stained Glass | crafting: 8× Glass + 1× Pink wool → 8× |
| <img src="img/enc/icons/1217.png" width="24"> | Gray Stained Glass | crafting: 8× Glass + 1× Gray wool → 8× |
| <img src="img/enc/icons/1218.png" width="24"> | Light Gray Stained Glass | crafting: 8× Glass + 1× Light gray wool → 8× |
| <img src="img/enc/icons/1219.png" width="24"> | Cyan Stained Glass | crafting: 8× Glass + 1× Cyan wool → 8× |
| <img src="img/enc/icons/1220.png" width="24"> | Purple Stained Glass | crafting: 8× Glass + 1× Purple wool → 8× |
| <img src="img/enc/icons/1221.png" width="24"> | Blue Stained Glass | crafting: 8× Glass + 1× Blue wool → 8× |
| <img src="img/enc/icons/1222.png" width="24"> | Brown Stained Glass | crafting: 8× Glass + 1× Brown wool → 8× |
| <img src="img/enc/icons/1223.png" width="24"> | Green Stained Glass | crafting: 8× Glass + 1× Green wool → 8× |
| <img src="img/enc/icons/1224.png" width="24"> | Red Stained Glass | crafting: 8× Glass + 1× Red wool → 8× |
| <img src="img/enc/icons/1225.png" width="24"> | Black Stained Glass | crafting: 8× Glass + 1× Black wool → 8× |
| <img src="img/enc/icons/1503.png" width="24"> | Powerstone Glass | crafting: 8× Glass + 1× Powerstone Dust → 8× |

</details>

#### <img src="img/enc/icons/1527.png" width="24"> Lighting (3)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/59.png" width="24"> | Powerstone Lamp | crafting: 4× Glass + 1× Powerstone Chip<br>crafting: 1× Glass + 1× Powerstone Dust + 1× Torch |
| <img src="img/enc/icons/86.png" width="24"> | Powerstone Light | crafting: 1× Glass + 2× Powerstone Dust → 2× |
| <img src="img/enc/icons/1527.png" width="24"> | Torch | crafting: 1× Stick + 1× Coal → 4× |

</details>

#### <img src="img/enc/icons/2.png" width="24"> Nature (86)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/2.png" width="24"> | Grass block | mining, found or creative |
| <img src="img/enc/icons/3.png" width="24"> | Dirt | mining, found or creative |
| <img src="img/enc/icons/12.png" width="24"> | Sand | mining, found or creative |
| <img src="img/enc/icons/13.png" width="24"> | Gravel | mining, found or creative |
| <img src="img/enc/icons/82.png" width="24"> | Clay | mining, found or creative |
| <img src="img/enc/icons/80.png" width="24"> | Snow block | mining, found or creative |
| <img src="img/enc/icons/79.png" width="24"> | Ice | mining, found or creative |
| <img src="img/enc/icons/81.png" width="24"> | Cactus | mining, found or creative |
| <img src="img/enc/icons/18.png" width="24"> | Oak leaves | mining, found or creative |
| <img src="img/enc/icons/161.png" width="24"> | Spruce Needles | mining, found or creative |
| <img src="img/enc/icons/31.png" width="24"> | Tall grass | mining, found or creative |
| <img src="img/enc/icons/37.png" width="24"> | Dandelion | mining, found or creative |
| <img src="img/enc/icons/38.png" width="24"> | Poppy | mining, found or creative |
| <img src="img/enc/icons/9.png" width="24"> | Water | mining, found or creative |
| <img src="img/enc/icons/11.png" width="24"> | Lava | mining, found or creative |
| <img src="img/enc/icons/16.png" width="24"> | Coal ore | mining, found or creative |
| <img src="img/enc/icons/15.png" width="24"> | Iron ore | mining, found or creative |
| <img src="img/enc/icons/180.png" width="24"> | Bronze ore | mining, found or creative |
| <img src="img/enc/icons/181.png" width="24"> | Steel ore | mining, found or creative |
| <img src="img/enc/icons/182.png" width="24"> | Ruby ore | mining, found or creative |
| <img src="img/enc/icons/87.png" width="24"> | Cornerstone | mining, found or creative |
| <img src="img/enc/icons/112.png" width="24"> | Cornerstone Bricks | furnace: Cornerstone |
| <img src="img/enc/icons/6.png" width="24"> | Cornerstone Grass | mining, found or creative |
| <img src="img/enc/icons/8.png" width="24"> | Grasser | mining, found or creative |
| <img src="img/enc/icons/10.png" width="24"> | Void Stone | mining, found or creative |
| <img src="img/enc/icons/14.png" width="24"> | Purple Grass | mining, found or creative |
| <img src="img/enc/icons/22.png" width="24"> | Void Leaves | mining, found or creative |
| <img src="img/enc/icons/23.png" width="24"> | Red Sand | mining, found or creative |
| <img src="img/enc/icons/28.png" width="24"> | Crimson Roots | mining, found or creative |
| <img src="img/enc/icons/27.png" width="24"> | Pumpkin | mining, found or creative |
| <img src="img/enc/icons/34.png" width="24"> | Netherperk Cap | mining, found or creative |
| <img src="img/enc/icons/40.png" width="24"> | Oak Sapling | mining, found or creative |
| <img src="img/enc/icons/41.png" width="24"> | Pine Sapling | mining, found or creative |
| <img src="img/enc/icons/42.png" width="24"> | Void Sapling | mining, found or creative |
| <img src="img/enc/icons/43.png" width="24"> | Netherperk Sapling | mining, found or creative |
| <img src="img/enc/icons/44.png" width="24"> | Powerstone Ore | mining, found or creative |
| <img src="img/enc/icons/97.png" width="24"> | Farmland | mining, found or creative |
| <img src="img/enc/icons/163.png" width="24"> | Wheat Seeds | mining, found or creative |
| <img src="img/enc/icons/1200.png" width="24"> | Tar | mining, found or creative |
| <img src="img/enc/icons/1202.png" width="24"> | Dripstone Block | crafting: 4× Pointed Dripstone |
| <img src="img/enc/icons/1203.png" width="24"> | Pointed Dripstone | mining, found or creative |
| <img src="img/enc/icons/1205.png" width="24"> | Snow Layer | mining, found or creative |
| <img src="img/enc/icons/1509.png" width="24"> | Wasteland Grass | mining, found or creative |
| <img src="img/enc/icons/1510.png" width="24"> | Dry Grass | mining, found or creative |
| <img src="img/enc/icons/1511.png" width="24"> | Grasser Flower | mining, found or creative |
| <img src="img/enc/icons/1512.png" width="24"> | Fern | mining, found or creative |
| <img src="img/enc/icons/1504.png" width="24"> | Debris Gravel | mining, found or creative |
| <img src="img/enc/icons/1526.png" width="24"> | Crimson Stone | mining, found or creative |
| <img src="img/enc/icons/1538.png" width="24"> | Deepstone | mining, found or creative |
| <img src="img/enc/icons/1539.png" width="24"> | Hardium Ore | mining, found or creative |
| <img src="img/enc/icons/1540.png" width="24"> | Mossy Deepstone | crafting: Deepstone + Vines |
| <img src="img/enc/icons/1541.png" width="24"> | Moss Block | crafting: 4× Vines |
| <img src="img/enc/icons/1542.png" width="24"> | Fungal Deepstone | mining, found or creative |
| <img src="img/enc/icons/60175.png" width="24"> | Abyssal Rock | mining, found or creative |
| <img src="img/enc/icons/1547.png" width="24"> | Vines | mining, found or creative |
| <img src="img/enc/icons/1549.png" width="24"> | Souled Grass Block | mining, found or creative |
| <img src="img/enc/icons/1550.png" width="24"> | Souled Dirt | mining, found or creative |
| <img src="img/enc/icons/1551.png" width="24"> | Souled Stone | mining, found or creative |
| <img src="img/enc/icons/1560.png" width="24"> | Celestial Soul Grass | mining, found or creative |
| <img src="img/enc/icons/1561.png" width="24"> | Celestial Soul Dirt | mining, found or creative |
| <img src="img/enc/icons/1562.png" width="24"> | Celestial Soul Stone | mining, found or creative |
| <img src="img/enc/icons/1559.png" width="24"> | Celestial Leaves | mining, found or creative |
| <img src="img/enc/icons/1567.png" width="24"> | Red Leaves | mining, found or creative |
| <img src="img/enc/icons/1568.png" width="24"> | Orange Leaves | mining, found or creative |
| <img src="img/enc/icons/1569.png" width="24"> | Yellow Leaves | mining, found or creative |
| <img src="img/enc/icons/1573.png" width="24"> | Gray Sapling | mining, found or creative |
| <img src="img/enc/icons/1574.png" width="24"> | Celestial Sapling | mining, found or creative |
| <img src="img/enc/icons/1980.png" width="24"> | Crimsonium Leaves | mining, found or creative |
| <img src="img/enc/icons/1981.png" width="24"> | Shattered Stone | mining, found or creative |
| <img src="img/enc/icons/1983.png" width="24"> | Void Shard Block | crafting: 4× Void Shard |
| <img src="img/enc/icons/1984.png" width="24"> | Dark Sand | mining, found or creative |
| <img src="img/enc/icons/1986.png" width="24"> | Shard Cluster | mining, found or creative |
| <img src="img/enc/icons/1987.png" width="24"> | Crimsonium Ore | mining, found or creative |
| <img src="img/enc/icons/1576.png" width="24"> | Pale Stone | mining, found or creative |
| <img src="img/enc/icons/1577.png" width="24"> | Sky Stone | mining, found or creative |
| <img src="img/enc/icons/1578.png" width="24"> | Celestium Ore | mining, found or creative |
| <img src="img/enc/icons/1579.png" width="24"> | Pale Crystal | mining, found or creative |
| <img src="img/enc/icons/1582.png" width="24"> | Leaf Litter | mining, found or creative |
| <img src="img/enc/icons/60158.png" width="24"> | Copper Moss | mining, found or creative |
| <img src="img/enc/icons/60163.png" width="24"> | Glass Reeds | mining, found or creative |
| <img src="img/enc/icons/60167.png" width="24"> | Glowcap Leaves | mining, found or creative |
| <img src="img/enc/icons/60168.png" width="24"> | Starberry Bush | mining, found or creative |
| <img src="img/enc/icons/60162.png" width="24"> | Aether Brine | mining, found or creative |
| <img src="img/enc/icons/60271.png" width="24"> | Cornerstone iron ore | mining, found or creative |
| <img src="img/enc/icons/60272.png" width="24"> | Cornerstone bronze ore | mining, found or creative |
| <img src="img/enc/icons/60273.png" width="24"> | Cornerstone steel ore | mining, found or creative |

</details>

#### <img src="img/enc/icons/47.png" width="24"> Decoration (32)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/47.png" width="24"> | Bookshelf | crafting: 6× Oak planks + 3× Stick |
| <img src="img/enc/icons/58.png" width="24"> | Crafting table | crafting: 4× Oak planks |
| <img src="img/enc/icons/61.png" width="24"> | Furnace | crafting: 8× Cobblestone |
| <img src="img/enc/icons/54.png" width="24"> | Chest | crafting: 8× Oak planks |
| <img src="img/enc/icons/85.png" width="24"> | Fence | crafting: 4× Oak planks + 2× Stick → 3× |
| <img src="img/enc/icons/63.png" width="24"> | Oak Trapdoor | crafting: 6× Oak planks → 2× |
| <img src="img/enc/icons/207.png" width="24"> | Oak Door | crafting: 6× Oak planks → 3× |
| <img src="img/enc/icons/208.png" width="24"> | Iron Door | mining, found or creative |
| <img src="img/enc/icons/35.png" width="24"> | White wool | mining, found or creative |
| <img src="img/enc/icons/164.png" width="24"> | Orange wool | crafting: White wool + Orange Dye |
| <img src="img/enc/icons/165.png" width="24"> | Magenta wool | crafting: White wool + Magenta Dye |
| <img src="img/enc/icons/166.png" width="24"> | Light blue wool | crafting: White wool + Light Blue Dye |
| <img src="img/enc/icons/167.png" width="24"> | Yellow wool | crafting: White wool + Yellow Dye |
| <img src="img/enc/icons/168.png" width="24"> | Lime wool | crafting: White wool + Lime Dye |
| <img src="img/enc/icons/169.png" width="24"> | Pink wool | crafting: White wool + Pink Dye |
| <img src="img/enc/icons/170.png" width="24"> | Gray wool | crafting: White wool + Gray Dye |
| <img src="img/enc/icons/171.png" width="24"> | Light gray wool | crafting: White wool + Light Gray Dye |
| <img src="img/enc/icons/172.png" width="24"> | Cyan wool | crafting: White wool + Cyan Dye |
| <img src="img/enc/icons/173.png" width="24"> | Purple wool | crafting: White wool + Purple Dye |
| <img src="img/enc/icons/174.png" width="24"> | Blue wool | crafting: White wool + Blue Dye |
| <img src="img/enc/icons/175.png" width="24"> | Brown wool | crafting: White wool + Brown Dye |
| <img src="img/enc/icons/176.png" width="24"> | Green wool | crafting: White wool + Green Dye |
| <img src="img/enc/icons/177.png" width="24"> | Red wool | crafting: White wool + Red Dye |
| <img src="img/enc/icons/178.png" width="24"> | Black wool | crafting: White wool + Black Dye |
| <img src="img/enc/icons/253.png" width="24"> | Bed | crafting: 3× Oak planks + 3× White wool |
| <img src="img/enc/icons/153.png" width="24"> | Hay Bale | crafting: 9× Wheat |
| <img src="img/enc/icons/1520.png" width="24"> | Spruce Fence | crafting: 4× Spruce Planks + 2× Stick → 3× |
| <img src="img/enc/icons/1521.png" width="24"> | Void Fence | crafting: 4× Void Planks + 2× Stick → 3× |
| <img src="img/enc/icons/1522.png" width="24"> | Netherperk Fence | crafting: 4× Netherperk Planks + 2× Stick → 3× |
| <img src="img/enc/icons/1523.png" width="24"> | Cornerstone Brick Fence | crafting: 6× Cornerstone Bricks → 6× |
| <img src="img/enc/icons/1505.png" width="24"> | Skeleton Head | mining, found or creative |
| <img src="img/enc/icons/60008.png" width="24"> | Cushion | mining, found or creative |

</details>

#### <img src="img/enc/icons/59.png" width="24"> Powerstone & Mechanisms (19)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/30.png" width="24"> | TNT | crafting: 5× Sand + 4× Powerstone Dust |
| <img src="img/enc/icons/46.png" width="24"> | Powerstone Block | crafting: 9× Powerstone Dust |
| <img src="img/enc/icons/56.png" width="24"> | Power Cable | crafting: 3× Powerstone Dust → 8× |
| <img src="img/enc/icons/83.png" width="24"> | Switch | crafting: 1× Cobblestone + 1× Powerstone Chip + 1× Stick |
| <img src="img/enc/icons/1500.png" width="24"> | Powerstone Bricks | crafting: 2× Stone bricks + 2× Powerstone Dust → 2× |
| <img src="img/enc/icons/1501.png" width="24"> | Cracked Powerstone Bricks | furnace: Powerstone Bricks |
| <img src="img/enc/icons/1502.png" width="24"> | Chiseled Powerstone Bricks | crafting: 2× Powerstone Slab |
| <img src="img/enc/icons/1140.png" width="24"> | Powerstone Stairs | crafting: 6× Powerstone Bricks → 4× |
| <img src="img/enc/icons/1136.png" width="24"> | Powerstone Slab | crafting: 3× Powerstone Bricks → 6× |
| <img src="img/enc/icons/1524.png" width="24"> | Powerstone Fence | crafting: 6× Powerstone Bricks → 6× |
| <img src="img/enc/icons/1534.png" width="24"> | Powerstone Furnace | crafting: 1× Furnace + 5× Powerstone Chip + 3× Powerstone Bricks |
| <img src="img/enc/icons/1587.png" width="24"> | Sound block | mining, found or creative |
| <img src="img/enc/icons/60176.png" width="24"> | Village Radio | crafting: 1× Powerstone Dust + 7× Steel ingot + 1× Sound block |
| <img src="img/enc/icons/1882.png" width="24"> | Cracked Powerstone Brick Stairs | crafting: 6× Cracked Powerstone Bricks → 4× |
| <img src="img/enc/icons/1922.png" width="24"> | Chiseled Powerstone Brick Stairs | crafting: 6× Chiseled Powerstone Bricks → 4× |
| <img src="img/enc/icons/1787.png" width="24"> | Cracked Powerstone Brick Slab | crafting: 3× Cracked Powerstone Bricks → 6× |
| <img src="img/enc/icons/1802.png" width="24"> | Chiseled Powerstone Brick Slab | crafting: 3× Chiseled Powerstone Bricks → 6× |
| <img src="img/enc/icons/99.png" width="24"> | Powerstone Dust | crafting: Ancient Coin + Ancient Coin + Ancient Coin + Ancient Coin → 2×<br>furnace: Powerstone Ore |
| <img src="img/enc/icons/100.png" width="24"> | Powerstone Chip | crafting: 2× Powerstone Dust + 1× Steel ingot → 2× |

</details>

#### <img src="img/enc/icons/212.png" width="24"> Tools & Weapons (50)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/210.png" width="24"> | Wooden pickaxe | crafting: 3× Oak planks + 2× Stick |
| <img src="img/enc/icons/213.png" width="24"> | Wooden axe | crafting: 3× Oak planks + 2× Stick |
| <img src="img/enc/icons/216.png" width="24"> | Wooden shovel | crafting: 1× Oak planks + 2× Stick |
| <img src="img/enc/icons/211.png" width="24"> | Stone pickaxe | crafting: 3× Cobblestone + 2× Stick |
| <img src="img/enc/icons/214.png" width="24"> | Stone axe | crafting: 3× Cobblestone + 2× Stick |
| <img src="img/enc/icons/217.png" width="24"> | Stone shovel | crafting: 1× Cobblestone + 2× Stick |
| <img src="img/enc/icons/212.png" width="24"> | Iron pickaxe | mining, found or creative |
| <img src="img/enc/icons/215.png" width="24"> | Iron axe | mining, found or creative |
| <img src="img/enc/icons/218.png" width="24"> | Iron shovel | mining, found or creative |
| <img src="img/enc/icons/219.png" width="24"> | Bronze pickaxe | crafting: 2× Stick + 3× Bronze ingot |
| <img src="img/enc/icons/220.png" width="24"> | Bronze axe | crafting: 2× Stick + 3× Bronze ingot |
| <img src="img/enc/icons/221.png" width="24"> | Bronze shovel | crafting: 2× Stick + 1× Bronze ingot |
| <img src="img/enc/icons/222.png" width="24"> | Steel pickaxe | crafting: 2× Stick + 3× Steel ingot |
| <img src="img/enc/icons/223.png" width="24"> | Steel axe | crafting: 2× Stick + 3× Steel ingot |
| <img src="img/enc/icons/224.png" width="24"> | Steel shovel | crafting: 2× Stick + 1× Steel ingot |
| <img src="img/enc/icons/225.png" width="24"> | Ruby pickaxe | crafting: 2× Stick + 3× Ruby |
| <img src="img/enc/icons/226.png" width="24"> | Ruby axe | crafting: 2× Stick + 3× Ruby |
| <img src="img/enc/icons/227.png" width="24"> | Ruby shovel | crafting: 2× Stick + 1× Ruby |
| <img src="img/enc/icons/255.png" width="24"> | Flint and steel | crafting: Flint + Steel ingot |
| <img src="img/enc/icons/248.png" width="24"> | Void Flint and Steel | mining, found or creative |
| <img src="img/enc/icons/190.png" width="24"> | Wooden Sword | crafting: 2× Oak planks + 1× Stick |
| <img src="img/enc/icons/191.png" width="24"> | Stone Sword | crafting: 2× Cobblestone + 1× Stick |
| <img src="img/enc/icons/192.png" width="24"> | Iron Sword | mining, found or creative |
| <img src="img/enc/icons/193.png" width="24"> | Bronze Sword | crafting: 1× Stick + 2× Bronze ingot |
| <img src="img/enc/icons/194.png" width="24"> | Steel Sword | crafting: 1× Stick + 2× Steel ingot |
| <img src="img/enc/icons/195.png" width="24"> | Ruby Sword | crafting: 1× Stick + 2× Ruby |
| <img src="img/enc/icons/186.png" width="24"> | Bucket | crafting: 3× Steel ingot |
| <img src="img/enc/icons/187.png" width="24"> | Water Bucket | mining, found or creative |
| <img src="img/enc/icons/188.png" width="24"> | Lava Bucket | mining, found or creative |
| <img src="img/enc/icons/102.png" width="24"> | Hoe | crafting: 2× Oak planks + 2× Stick<br>crafting: 2× Stick + 2× Steel ingot |
| <img src="img/enc/icons/103.png" width="24"> | Bow | crafting: 3× Stick + 3× String |
| <img src="img/enc/icons/104.png" width="24"> | Arrow | crafting: 1× Feather + 1× Stick + 1× Flint → 4× |
| <img src="img/enc/icons/1600.png" width="24"> | Brusher | crafting: 1× Feather + 1× Stick + 1× Steel ingot |
| <img src="img/enc/icons/1613.png" width="24"> | Shield | crafting: 6× Oak planks + 1× Steel ingot |
| <img src="img/enc/icons/1618.png" width="24"> | Hardium Pickaxe | crafting: 2× Stick + 3× Hardium |
| <img src="img/enc/icons/1620.png" width="24"> | Celestial Compass | mining, found or creative |
| <img src="img/enc/icons/1621.png" width="24"> | Hardium Sword | crafting: 1× Stick + 2× Hardium |
| <img src="img/enc/icons/1622.png" width="24"> | Hardium Axe | crafting: 2× Stick + 3× Hardium |
| <img src="img/enc/icons/1623.png" width="24"> | Hardium Shovel | crafting: 2× Stick + 1× Hardium |
| <img src="img/enc/icons/1991.png" width="24"> | Celestium Pickaxe | crafting: 2× Stick + 3× Celestium |
| <img src="img/enc/icons/1992.png" width="24"> | Celestium Sword | crafting: 1× Stick + 2× Celestium |
| <img src="img/enc/icons/1993.png" width="24"> | Celestium Axe | crafting: 2× Stick + 3× Celestium |
| <img src="img/enc/icons/1994.png" width="24"> | Celestium Shovel | crafting: 2× Stick + 1× Celestium |
| <img src="img/enc/icons/60301.png" width="24"> | Wallet | crafting: 1× String + 7× Leather |
| <img src="img/enc/icons/60302.png" width="24"> | Leather Backpack | crafting: 1× Chest + 1× String + 7× Leather |
| <img src="img/enc/icons/60303.png" width="24"> | Steel Backpack | crafting: 4× Steel ingot + 1× Leather Backpack |
| <img src="img/enc/icons/60304.png" width="24"> | Steel-Bronze Backpack | crafting: 4× Bronze ingot + 1× Steel Backpack |
| <img src="img/enc/icons/60305.png" width="24"> | Bronze-Ruby Backpack | crafting: 4× Ruby + 1× Steel-Bronze Backpack |
| <img src="img/enc/icons/60306.png" width="24"> | Hardium Backpack | crafting: 4× Hardium + 1× Bronze-Ruby Backpack |
| <img src="img/enc/icons/60307.png" width="24"> | Celestium Backpack | crafting: 4× Celestium + 1× Hardium Backpack |

</details>

#### <img src="img/enc/icons/237.png" width="24"> Armour (26)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/228.png" width="24"> | Wooden helmet | crafting: 5× Oak planks |
| <img src="img/enc/icons/229.png" width="24"> | Wooden chestplate | crafting: 8× Oak planks |
| <img src="img/enc/icons/230.png" width="24"> | Wooden leggings | crafting: 7× Oak planks |
| <img src="img/enc/icons/231.png" width="24"> | Wooden boots | crafting: 4× Oak planks |
| <img src="img/enc/icons/232.png" width="24"> | Stone helmet | crafting: 5× Cobblestone |
| <img src="img/enc/icons/233.png" width="24"> | Stone chestplate | crafting: 8× Cobblestone |
| <img src="img/enc/icons/234.png" width="24"> | Stone leggings | crafting: 7× Cobblestone |
| <img src="img/enc/icons/235.png" width="24"> | Stone boots | crafting: 4× Cobblestone |
| <img src="img/enc/icons/236.png" width="24"> | Bronze helmet | crafting: 5× Bronze ingot |
| <img src="img/enc/icons/237.png" width="24"> | Bronze chestplate | crafting: 8× Bronze ingot |
| <img src="img/enc/icons/238.png" width="24"> | Bronze leggings | crafting: 7× Bronze ingot |
| <img src="img/enc/icons/239.png" width="24"> | Bronze boots | crafting: 4× Bronze ingot |
| <img src="img/enc/icons/240.png" width="24"> | Steel helmet | crafting: 5× Steel ingot |
| <img src="img/enc/icons/241.png" width="24"> | Steel chestplate | crafting: 8× Steel ingot |
| <img src="img/enc/icons/242.png" width="24"> | Steel leggings | crafting: 7× Steel ingot |
| <img src="img/enc/icons/243.png" width="24"> | Steel boots | crafting: 4× Steel ingot |
| <img src="img/enc/icons/244.png" width="24"> | Ruby helmet | crafting: 5× Ruby |
| <img src="img/enc/icons/245.png" width="24"> | Ruby chestplate | crafting: 8× Ruby |
| <img src="img/enc/icons/246.png" width="24"> | Ruby leggings | crafting: 7× Ruby |
| <img src="img/enc/icons/247.png" width="24"> | Ruby boots | crafting: 4× Ruby |
| <img src="img/enc/icons/1624.png" width="24"> | Hardium Helmet | crafting: 5× Hardium |
| <img src="img/enc/icons/1625.png" width="24"> | Hardium Chestplate | crafting: 8× Hardium |
| <img src="img/enc/icons/1626.png" width="24"> | Hardium Leggings | crafting: 7× Hardium |
| <img src="img/enc/icons/1627.png" width="24"> | Hardium Boots | crafting: 4× Hardium |
| <img src="img/enc/icons/1995.png" width="24"> | Celestium Chestplate | crafting: 1× Pale Crystal + 7× Celestium |
| <img src="img/enc/icons/1996.png" width="24"> | Celestial Wings | mining, found or creative |

</details>

#### <img src="img/enc/icons/1301.png" width="24"> Maps & Special Items (17)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/7.png" width="24"> | Bedrock | mining, found or creative |
| <img src="img/enc/icons/189.png" width="24"> | Arena Compass | mining, found or creative |
| <img src="img/enc/icons/1301.png" width="24"> | Map | crafting: 8× Wheat + 1× Coal |
| <img src="img/enc/icons/1601.png" width="24"> | Fortress Compass | mining, found or creative |
| <img src="img/enc/icons/1614.png" width="24"> | Explorer Map | mining, found or creative |
| <img src="img/enc/icons/1615.png" width="24"> | Book and Quill | crafting: Feather + Wheat + Coal |
| <img src="img/enc/icons/1602.png" width="24"> | Old Document I | mining, found or creative |
| <img src="img/enc/icons/1603.png" width="24"> | Old Document II | mining, found or creative |
| <img src="img/enc/icons/1604.png" width="24"> | Old Document III | mining, found or creative |
| <img src="img/enc/icons/1605.png" width="24"> | Old Document IV | mining, found or creative |
| <img src="img/enc/icons/1606.png" width="24"> | Old Document V | mining, found or creative |
| <img src="img/enc/icons/1607.png" width="24"> | Old Document VI | mining, found or creative |
| <img src="img/enc/icons/1608.png" width="24"> | Old Document VII | mining, found or creative |
| <img src="img/enc/icons/1609.png" width="24"> | Old Document VIII | mining, found or creative |
| <img src="img/enc/icons/1610.png" width="24"> | Ancient Coin | mining, found or creative |
| <img src="img/enc/icons/1611.png" width="24"> | Powerstone Relic | mining, found or creative |
| <img src="img/enc/icons/1612.png" width="24"> | Pottery Shard | mining, found or creative |

</details>

#### <img src="img/enc/icons/107.png" width="24"> Food & Materials (50)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/60169.png" width="24"> | Void Truffle | mining, found or creative |
| <img src="img/enc/icons/60170.png" width="24"> | Starberries | mining, found or creative |
| <img src="img/enc/icons/1644.png" width="24"> | Void Shard | mining, found or creative |
| <img src="img/enc/icons/1645.png" width="24"> | Crimsonium | mining, found or creative |
| <img src="img/enc/icons/1628.png" width="24"> | White Dye | crafting: Clay → 2× |
| <img src="img/enc/icons/1629.png" width="24"> | Orange Dye | crafting: Orange Leaves → 2× |
| <img src="img/enc/icons/1630.png" width="24"> | Magenta Dye | crafting: Pink Dye + Purple Dye → 2× |
| <img src="img/enc/icons/1631.png" width="24"> | Light Blue Dye | crafting: White Dye + Blue Dye → 2× |
| <img src="img/enc/icons/1632.png" width="24"> | Yellow Dye | crafting: Dandelion → 2×<br>crafting: Yellow Leaves → 2× |
| <img src="img/enc/icons/1633.png" width="24"> | Lime Dye | crafting: White Dye + Green Dye → 2× |
| <img src="img/enc/icons/1634.png" width="24"> | Pink Dye | crafting: White Dye + Red Dye → 2× |
| <img src="img/enc/icons/1635.png" width="24"> | Gray Dye | crafting: White Dye + Black Dye → 2× |
| <img src="img/enc/icons/1636.png" width="24"> | Light Gray Dye | crafting: White Dye + Gray Dye → 2× |
| <img src="img/enc/icons/1637.png" width="24"> | Cyan Dye | crafting: Blue Dye + Green Dye → 2× |
| <img src="img/enc/icons/1638.png" width="24"> | Purple Dye | crafting: Blue Dye + Red Dye → 2× |
| <img src="img/enc/icons/1639.png" width="24"> | Blue Dye | crafting: Powerstone Dust → 2× |
| <img src="img/enc/icons/1640.png" width="24"> | Brown Dye | crafting: Dirt → 2× |
| <img src="img/enc/icons/1641.png" width="24"> | Green Dye | crafting: Cactus → 2× |
| <img src="img/enc/icons/1642.png" width="24"> | Red Dye | crafting: Poppy → 2×<br>crafting: Red Leaves → 2× |
| <img src="img/enc/icons/1643.png" width="24"> | Black Dye | crafting: Coal → 2× |
| <img src="img/enc/icons/200.png" width="24"> | Stick | crafting: 2× Oak planks → 4×<br>crafting: Rotten Planks → 2× |
| <img src="img/enc/icons/201.png" width="24"> | Coal | furnace: Oak log<br>furnace: Spruce Log |
| <img src="img/enc/icons/202.png" width="24"> | Iron ingot | furnace: Iron ore<br>furnace: Cornerstone iron ore |
| <img src="img/enc/icons/204.png" width="24"> | Bronze ingot | crafting: Bronze block → 9×<br>furnace: Bronze ore<br>furnace: Cornerstone bronze ore |
| <img src="img/enc/icons/205.png" width="24"> | Steel ingot | crafting: Steel block → 9×<br>furnace: Steel ore<br>furnace: Cornerstone steel ore<br>furnace: Steel Mix |
| <img src="img/enc/icons/206.png" width="24"> | Ruby | crafting: Ruby block → 9× |
| <img src="img/enc/icons/203.png" width="24"> | Flint | mining, found or creative |
| <img src="img/enc/icons/254.png" width="24"> | Apple | mining, found or creative |
| <img src="img/enc/icons/209.png" width="24"> | Boat | crafting: 5× Oak planks |
| <img src="img/enc/icons/249.png" width="24"> | Raw Porkchop | mining, found or creative |
| <img src="img/enc/icons/250.png" width="24"> | Cooked Porkchop | furnace: Raw Porkchop |
| <img src="img/enc/icons/251.png" width="24"> | Raw Mutton | mining, found or creative |
| <img src="img/enc/icons/252.png" width="24"> | Cooked Mutton | furnace: Raw Mutton |
| <img src="img/enc/icons/105.png" width="24"> | Feather | mining, found or creative |
| <img src="img/enc/icons/106.png" width="24"> | Wheat | crafting: Hay Bale → 9× |
| <img src="img/enc/icons/107.png" width="24"> | Bread | crafting: 3× Wheat |
| <img src="img/enc/icons/108.png" width="24"> | Raw Chicken | mining, found or creative |
| <img src="img/enc/icons/109.png" width="24"> | Cooked Chicken | furnace: Raw Chicken |
| <img src="img/enc/icons/110.png" width="24"> | Raw Beef | mining, found or creative |
| <img src="img/enc/icons/111.png" width="24"> | Steak | furnace: Raw Beef |
| <img src="img/enc/icons/154.png" width="24"> | Milk Bucket | mining, found or creative |
| <img src="img/enc/icons/1300.png" width="24"> | String | crafting: White wool → 4× |
| <img src="img/enc/icons/1302.png" width="24"> | Tar Ball | mining, found or creative |
| <img src="img/enc/icons/1619.png" width="24"> | Crimson Man's Head | mining, found or creative |
| <img src="img/enc/icons/1617.png" width="24"> | Hardium | mining, found or creative |
| <img src="img/enc/icons/1990.png" width="24"> | Celestium | mining, found or creative |
| <img src="img/enc/icons/1997.png" width="24"> | Honey Ball | crafting: Honey Block → 4× |
| <img src="img/enc/icons/60274.png" width="24"> | Ember dust | mining, found or creative |
| <img src="img/enc/icons/60308.png" width="24"> | Steel Mix | crafting: Coal + Iron ingot |
| <img src="img/enc/icons/60300.png" width="24"> | Leather | mining, found or creative |

</details>

#### <img src="img/enc/icons/196.png" width="24"> Spawn Eggs (24)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/196.png" width="24"> | Pig Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/197.png" width="24"> | Sheep Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/198.png" width="24"> | Pumpkin Slime Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/199.png" width="24"> | Forgotten Man Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/155.png" width="24"> | Chicken Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/156.png" width="24"> | Cow Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/157.png" width="24"> | Villager Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/1303.png" width="24"> | Spider Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/1304.png" width="24"> | Bat Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/1305.png" width="24"> | TNT Zombie Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/1306.png" width="24"> | Tar Monster Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/1307.png" width="24"> | Husking Spawn Egg | mining, found or creative |
| <img src="img/enc/icons/1308.png" width="24"> | Spawn Egg: Hogman | mining, found or creative |
| <img src="img/enc/icons/60289.png" width="24"> | Spawn Egg: Celestling | mining, found or creative |
| <img src="img/enc/icons/60290.png" width="24"> | Spawn Egg: Reach Stalker | mining, found or creative |
| <img src="img/enc/icons/60291.png" width="24"> | Spawn Egg: Sky Drifter | mining, found or creative |
| <img src="img/enc/icons/60292.png" width="24"> | Spawn Egg: Cornerstone Golem | mining, found or creative |
| <img src="img/enc/icons/60293.png" width="24"> | Spawn Egg: Moss Lurker | mining, found or creative |
| <img src="img/enc/icons/60294.png" width="24"> | Spawn Egg: Sporeling | mining, found or creative |
| <img src="img/enc/icons/60295.png" width="24"> | Spawn Egg: Ember Moth | mining, found or creative |
| <img src="img/enc/icons/60296.png" width="24"> | Spawn Egg: Ash Fox | mining, found or creative |
| <img src="img/enc/icons/60297.png" width="24"> | Spawn Egg: Fox | mining, found or creative |
| <img src="img/enc/icons/60298.png" width="24"> | Spawn Egg: Ghost | mining, found or creative |
| <img src="img/enc/icons/60299.png" width="24"> | Spawn Egg: Snowman | mining, found or creative |

</details>

#### <img src="img/enc/icons/1586.png" width="24"> Special blocks (5)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/1548.png" width="24"> | Monster Spawner | mining, found or creative |
| <img src="img/enc/icons/1584.png" width="24"> | Structure Block | mining, found or creative |
| <img src="img/enc/icons/1585.png" width="24"> | Barrier | mining, found or creative |
| <img src="img/enc/icons/1586.png" width="24"> | Command Block | mining, found or creative |
| <img src="img/enc/icons/29.png" width="24"> | World End | mining, found or creative |

</details>

#### <img src="img/enc/icons/60013.png" width="24"> Railway (28)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/1588.png" width="24"> | Rail | crafting: 1× Spruce Log + 6× Steel ingot → 16× |
| <img src="img/enc/icons/60016.png" width="24"> | Sloped rail | crafting: 3× Cobblestone + 3× Rail → 3× |
| <img src="img/enc/icons/60187.png" width="24"> | Gentle Rail Slope | crafting: 6× Rail → 4× |
| <img src="img/enc/icons/60000.png" width="24"> | Rail switch | crafting: 1× Switch + 3× Rail |
| <img src="img/enc/icons/60043.png" width="24"> | Turntable | crafting: 7× Cobblestone + 1× Powerstone Dust + 1× Rail |
| <img src="img/enc/icons/60020.png" width="24"> | Train booster | crafting: 4× Powerstone Dust + 4× Steel ingot + 1× Rail → 2× |
| <img src="img/enc/icons/60021.png" width="24"> | Crossing chime | crafting: 1× Powerstone Dust + 8× Steel ingot |
| <img src="img/enc/icons/60022.png" width="24"> | Horn maker | crafting: 1× Powerstone Dust + 8× Steel ingot |
| <img src="img/enc/icons/60023.png" width="24"> | Crossing signal | crafting: 2× White wool + 2× Powerstone Lamp + 2× Steel ingot |
| <img src="img/enc/icons/60035.png" width="24"> | Crossing gate | crafting: 1× White wool + 1× Powerstone Lamp + 1× Red wool + 2× Steel ingot |
| <img src="img/enc/icons/60009.png" width="24"> | Buffer stop | crafting: 5× Steel ingot + 3× Rail |
| <img src="img/enc/icons/60013.png" width="24"> | Locomotive | crafting: 1× Furnace + 5× Steel block + 2× Steel ingot |
| <img src="img/enc/icons/60014.png" width="24"> | Passenger wagon | crafting: 3× Oak planks + 3× Glass + 3× Steel ingot |
| <img src="img/enc/icons/60015.png" width="24"> | Freight wagon | crafting: 4× Oak planks + 1× Chest + 3× Steel ingot |
| <img src="img/enc/icons/60045.png" width="24"> | Drill car | crafting: 1× Furnace + 1× Steel block + 6× Steel ingot + 1× Rail |
| <img src="img/enc/icons/60244.png" width="24"> | Subway cab car | crafting: 3× Glass + 1× Powerstone Dust + 1× Steel block + 4× Steel ingot |
| <img src="img/enc/icons/60245.png" width="24"> | Subway car | crafting: 3× Glass + 5× Steel ingot |
| <img src="img/enc/icons/60246.png" width="24"> | Concrete track bed | crafting: 1× Gravel + 2× Hardened Concrete → 3× |
| <img src="img/enc/icons/60247.png" width="24"> | Platform | crafting: 3× Hardened Concrete + 3× Yellow Dye → 3× |
| <img src="img/enc/icons/60248.png" width="24"> | Lattice girder | crafting: 5× Steel ingot → 4× |
| <img src="img/enc/icons/60249.png" width="24"> | Track crossing | crafting: 5× Rail |
| <img src="img/enc/icons/60250.png" width="24"> | Diagonal track crossing | crafting: 5× Rail |
| <img src="img/enc/icons/60265.png" width="24"> | Subway tiles | crafting: 4× Hardened Concrete → 4× |
| <img src="img/enc/icons/60266.png" width="24"> | Blue subway tiles | crafting: Blue Dye + Subway tiles |
| <img src="img/enc/icons/60267.png" width="24"> | Cracked tiles | crafting: Gravel + Subway tiles |
| <img src="img/enc/icons/60268.png" width="24"> | Station sign | crafting: 1× Blue Dye + 5× Subway tiles → 2× |
| <img src="img/enc/icons/60269.png" width="24"> | Bench | crafting: 2× Spruce Planks + 3× Spruce Slab → 2× |
| <img src="img/enc/icons/60270.png" width="24"> | Subway map | crafting: Map + Rail |

</details>

#### <img src="img/enc/icons/60047.png" width="24"> Bronze (23)

<details>
<summary>Open</summary>

| | Name | How to get it |
|---|---|---|
| <img src="img/enc/icons/183.png" width="24"> | Bronze block | crafting: 9× Bronze ingot |
| <img src="img/enc/icons/60046.png" width="24"> | Cut bronze | crafting: 4× Bronze block → 4× |
| <img src="img/enc/icons/60047.png" width="24"> | Chiseled bronze | crafting: 2× Cut bronze |
| <img src="img/enc/icons/60048.png" width="24"> | Bronze grate | crafting: 4× Bronze block → 4× |
| <img src="img/enc/icons/60049.png" width="24"> | Bronze bulb | crafting: 1× Powerstone Dust + 3× Bronze block + 1× Torch → 4× |
| <img src="img/enc/icons/60136.png" width="24"> | Block of bronze (tarnished) | mining, found or creative |
| <img src="img/enc/icons/60138.png" width="24"> | Block of bronze (rusty) | mining, found or creative |
| <img src="img/enc/icons/60141.png" width="24"> | Cut bronze (tarnished) | mining, found or creative |
| <img src="img/enc/icons/60143.png" width="24"> | Cut bronze (rusty) | mining, found or creative |
| <img src="img/enc/icons/60146.png" width="24"> | Chiseled bronze (tarnished) | mining, found or creative |
| <img src="img/enc/icons/60148.png" width="24"> | Chiseled bronze (rusty) | mining, found or creative |
| <img src="img/enc/icons/60151.png" width="24"> | Bronze grate (tarnished) | mining, found or creative |
| <img src="img/enc/icons/60153.png" width="24"> | Bronze grate (rusty) | mining, found or creative |
| <img src="img/enc/icons/60135.png" width="24"> | Block of bronze (waxed) | crafting: Bronze block + Honey Ball |
| <img src="img/enc/icons/60140.png" width="24"> | Cut bronze (waxed) | crafting: Honey Ball + Cut bronze |
| <img src="img/enc/icons/60145.png" width="24"> | Chiseled bronze (waxed) | crafting: Honey Ball + Chiseled bronze |
| <img src="img/enc/icons/60150.png" width="24"> | Bronze grate (waxed) | crafting: Honey Ball + Bronze grate |
| <img src="img/enc/icons/60080.png" width="24"> | Bronze stairs | mining, found or creative |
| <img src="img/enc/icons/60088.png" width="24"> | Cut bronze stairs | mining, found or creative |
| <img src="img/enc/icons/60096.png" width="24"> | Bronze slab | mining, found or creative |
| <img src="img/enc/icons/60099.png" width="24"> | Cut bronze slab | mining, found or creative |
| <img src="img/enc/icons/60134.png" width="24"> | Bronze door | crafting: 6× Bronze ingot → 3× |
| <img src="img/enc/icons/60118.png" width="24"> | Bronze trapdoor | crafting: 6× Bronze ingot → 2× |

</details>

### Achievements

| Achievement | How to get it | Voxel Bucks |
|---|---|---|
| **Getting Started** | Mine your first block. | 10 |
| **Workshop** | Craft a crafting table. | 15 |
| **Stone Age** | Craft a stone pickaxe. | 20 |
| **Hot Stuff** | Smelt an iron ingot. | 25 |
| **Bronze Age** | Get a bronze ingot. | 30 |
| **Heart of Steel** | Get a steel ingot. | 40 |
| **Red Treasure** | Find a ruby. | 60 |
| **Suit Up** | Wear all four pieces of armor. | 50 |
| **Welcome Home** | Build a door. | 10 |
| **Ruins** | Open a chest in a ruin. | 30 |
| **Shipwreck** | Open a chest in a sunken ship. | 50 |
| **Dungeon** | Open a chest in a dungeon. | 50 |
| **Abandoned Mine** | Open a chest in a mine. | 50 |
| **Sailor** | Ride a boat. | 25 |
| **Through the Gate** | Enter the Cornerstone Dimension. | 60 |
| **Winter Trip** | Visit a snowy land. | 15 |
| **Ouch!** | Prick yourself on a cactus. | 10 |
| **Miner** | Mine 100 blocks. | 40 |
| **Mole** | Mine 1000 blocks. | 100 |
| **Together** | Play with someone over the network. | 40 |
| **Lord of the Fortress** | Open a chest in a Nether fortress. | 60 |
| **Into the Void** | Enter the Crimson Void. | 100 |
| **Monster Hunter** | Defeat a monster. | 20 |
| **Good Night** | Sleep in a bed. | 15 |
| **Conqueror** | Defeat the Crimson Man. | 500 |
| **Treasures of the Void** | Open a chest in a Crimson Void tower or dungeon. | 80 |
| **Traveller** | Open a chest in a temple, igloo or watchtower. | 40 |
| **Boom!** | Set off TNT. | 20 |
| **Electrician** | Light a powerstone lamp. | 30 |
| **Gardener** | Grow a tree from a sapling. | 20 |
| **Farmer** | Harvest ripe wheat. | 25 |
| **Archer** | Hit a creature with an arrow. | 25 |
| **Fresh Milk** | Milk a cow into a bucket. | 15 |
| **Trader** | Make a trade with a villager. | 30 |
| **Tar Slayer** | Defeat a tar monster. | 60 |
| **Cartographer** | Make a map. | 25 |
| **Anniversary Showcase** | Visit the VoxelCraft anniversary showcase. | 30 |
| **Party Time** | Place a balloon. | 15 |
| **Confetti!** | Fire a party popper. | 15 |
| **Collector** | Make the anniversary medal from five tokens. | 100 |
| **Forgotten Land** | Visit the Powerstone Wasteland. | 20 |
| **Abandoned Home** | Open a chest in a forgotten village. | 40 |
| **Below the Surface** | Open a chest in the drowned ruins. | 50 |
| **Broken Gate** | Open a chest at a broken portal. | 40 |
| **Táborník** | Otevři sud v táboře poutníků. | 30 |
| **Archaeologist** | Brush debris gravel with the brusher. | 30 |
| **Chronicler** | Read an old document. | 20 |
| **The Fall of the Forgotten Kingdom** | Read all eight old documents. | 150 |
| **The End of a Husking** | Defeat a Husking. | 50 |
| **Light in the Dark** | Place a torch. | 10 |
| **Covered** | Stop an attack with a shield. | 25 |
| **Trading with a Hogman** | Give a Hogman powerstone. | 40 |
| **In the Miners' Footsteps** | Find a mineshaft with an explorer map. | 60 |
| **Above the Clouds** | Enter the Celestial Reach. | 120 |
| **Gate to the Heavens** | Put the Crimson Man's twelve heads on a Celestial portal frame. | 100 |

### Commands

| Command | What it does | Cheats only |
|---|---|---|
| `/help` | List of commands |  |
| `/list` | Players in the game |  |
| `/seed` | World seed |  |
| `/pos` | Where you are |  |
| `/gamemode <survival\|creative>` | Changes the game mode | ✓ |
| `/tp <x> <y> <z> \| <hráč>` | Teleport (~ = from your position) | ✓ |
| `/give <věc> [počet]` | Gives you an item (e.g. ruby, iron_ingot) | ✓ |
| `/time [day\|noon\|evening\|night\|midnight\|tiky]` | Time of day | ✓ |
| `/wave` | Wave (Hello!) |  |
| `/music [on\|off]` | Music in the world on / off |  |
| `/weather <clear\|rain>` | Weather | ✓ |
| `/summon <pig\|sheep\|slime\|forgotten\|crimson\|husking\|...>` | Summons a creature | ✓ |
| `/locate <village\|forgotten\|mineshaft\|fortress\|arena\|frame\|temple>` | Finds the nearest structure |  |
| `/say <text>` | A message in the chat (colours: &c, &l...) |  |
| `/age <0\|1\|2>` | Age of bronze within 8 blocks: bright, tarnished, rusty | ✓ |
| `/wax` | Waxes the bronze within 8 blocks | ✓ |
| `/spawn` | Teleport to spawn | ✓ |
| `/heal` | Restores health and food | ✓ |
| `/clear` | Empties the inventory | ✓ |
| `/kill` | You die (Survival only) | ✓ |


</details>

---

<div align="center">

[⬅️ README](../README.md)

</div>
