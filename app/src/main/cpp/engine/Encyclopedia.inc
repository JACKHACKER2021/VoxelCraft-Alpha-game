// 1.7.2 build 3: the entries of the encyclopedia (Czech keys; LangTable.inc has the translations).
namespace {
struct EncEntry {
    const char* category;
    const char* title;
    const char* picture;  // a picture id from EncyclopediaPictures.inc, or ""
    ItemId icon;
    const char* text;
};
const EncEntry kEncyclopedia[] = {
    {"Dimenze", "Overworld", "plains", GRASS, "Výchozí svět. Biomy od plání po zasněžené hory, jeskyně a hlubinný kámen, vesnice, doly, tábory a další stavby."},
    {"Dimenze", "Dimenze úhelného kamene", "cornerstone", NETHERRACK, "Dimenze za obsidiánovým portálem: úhelný kámen, lávová moře, powerstonová pustina, hogmani a velká pevnost s portálem do Karmínové prázdnoty."},
    {"Dimenze", "Karmínová prázdnota", "void_purple", VOID_GRASS, "Fialový les s očními stromy, karmínová poušť, lávové pláně s arénou Karmínového muže, Zapomenutý Overworld a tři nové biomy."},
    {"Dimenze", "Nebeská říše", "celestial", CELESTIAL_GRASS, "Plovoucí ostrovy nad bílou oblohou, spojené mosty. Vstup otevře 12 hlav Karmínového muže v rámu portálu. Pád dolů je konečný."},
    {"Dimenze", "Starý svět", "old_world", GRASS, "Typ světa s generací verzí 0.1–0.3: starý terén, textury, ovládání s joystickem a menu. Jen kreativní, bez mobů a hudby."},
    {"Biomy", "Pláně", "plains", GRASS, "Otevřená zelená krajina s květinami a ojedinělými duby."},
    {"Biomy", "Strakatý les", "dappled", GRAY_LOG, "Šedé stromy s červeným, oranžovým a žlutým listím, oranžová tráva. Většinou sněží, vzácně slabě prší. Často v něm stojí tábory."},
    {"Biomy", "Hory", "mountains", HARDENED_STONE, "Vysoké kopce s tvrzeným kamenem na srázech a sněhem na vrcholech."},
    {"Biomy", "Les", "forest", LOG, "Hustý dubový les, ve stínu kapradí a houby. Dobré místo na dřevo."},
    {"Biomy", "Poušť", "desert", SAND, "Písek, kaktusy a pískovec. Stojí v ní pouštní chrámy."},
    {"Biomy", "Zasněžená tajga", "taiga", SPRUCE_LOG, "Smrkové lesy pod sněhem. Sníh tu padá a zůstává ležet."},
    {"Biomy", "Ledové pláně", "ice", ICE, "Zamrzlá krajina se zamrzlou vodou a iglú."},
    {"Biomy", "Fialový les", "void_purple", EYE_LOG, "Les Karmínové prázdnoty: prázdnotná tráva, stromy s okem v kmeni."},
    {"Biomy", "Karmínová poušť", "void_crimson", RED_SAND, "Duny rudého písku, skalní věže z karmínového kamene, krystaly a zasypané ruiny."},
    {"Biomy", "Crimsoniový les", "crimsonium", CRIMSONIUM_LOG, "Hustý les rudých crimsoniových stromů. Pod zemí vzácná crimsoniová ruda, mezi stromy lesní svatyně s lootem."},
    {"Biomy", "Roztříštěná prázdnota", "shattered", SHATTERED_STONE, "Rozlámané skály s puklinami, skalní oblouky, rozpadlé věže a krystaly prázdnotných střepů."},
    {"Biomy", "Poušť prázdnotných střepů", "shard_desert", DARK_SAND, "Temné duny se shluky krystalů, zasypané ruiny a skryté komory pod pískem, prozrazené jen krystalem na sloupku."},
    {"Stavby", "Tábor", "camp", LOOT_BARREL_CAMP, "Stan z vlny, ohniště a barel se zásobami. Nejčastěji ve Strakatém lese."},
    {"Stavby", "Mosty Nebeské říše", "bridges", CELESTIAL_PLANKS, "První stavby Nebeské říše: rovné mosty mezi ostrovy se zábradlím, sloupky a řetězy."},
    {"Stavby", "Lesní svatyně", "shrine", LOOT_CHEST_SHRINE, "Sloupy z crimsonia, střecha z desek a krystal nahoře. Truhla s crimsoniem a střepy."},
    {"Stavby", "Skrytá komora", "shard_desert", LOOT_CHEST_CHAMBER, "Místnost z temného pískovce 13 bloků pod dunami. Hledej krystal na sloupku."},
    {"Stavby", "Vesnice", "village", HAY_BALE, "Kruhová osada s domy, studnou, cestami a poli pšenice. Žijí v ní vesničané, se kterými můžeš obchodovat."},
    {"Stavby", "Zničená vesnice", "ruined_village", COBBLESTONE, "Opuštěná vesnice s rozpadlými domy a zarostlými cestami. Žádní vesničané, ale truhly zůstaly."},
    {"Stavby", "Zřícenina domu", "ruin", LOOT_CHEST_RUIN, "Pobořené kamenné a dřevěné zdi na pláních a v lesích. Uvnitř truhla s kořistí."},
    {"Stavby", "Ztroskotaná loď", "shipwreck", LOOT_CHEST_SHIP, "Vrak dřevěné lodi na dně moře nebo na břehu. Truhla s mapami, jídlem a železem."},
    {"Stavby", "Kobka", "dungeon", LOOT_CHEST_DUNGEON, "Místnost z mechových kamenných cihel hluboko pod zemí s truhlou."},
    {"Stavby", "Jeskynní kobka", "cave_dungeon", MOB_SPAWNER, "Malá místnost ve skále s líhní příšer a kořistí. Líheň znič, než tě přemůže."},
    {"Stavby", "Opuštěný důl", "", LOOT_CHEST_MINE, "Dlouhé dřevěné chodby pod zemí, spojené s jeskyněmi. Truhly s rudami a průzkumnými mapami."},
    {"Stavby", "Pouštní chrám", "desert_temple", LOOT_CHEST_TEMPLE, "Pyramida z pískovce v poušti. Uvnitř skrytá truhla."},
    {"Stavby", "Iglú", "igloo", LOOT_CHEST_IGLOO, "Sněhová kopule na ledových pláních a v tajze, s truhlou a kobercem."},
    {"Stavby", "Strážní věž", "watchtower", LOOT_CHEST_TOWER, "Vysoká věž s dřevěnou plošinou nahoře a truhlou. Krásný výhled do okolí."},
    {"Stavby", "Velká pevnost", "fortress", LOOT_CHEST_FORTRESS, "Obří pevnost z nether cihel v Dimenzi úhelného kamene: archiv, kovárna, trůnní sál a krypta. V hale portál do Karmínové prázdnoty."},
    {"Stavby", "Zapomenutá vesnice", "forgotten_village", LOOT_CHEST_FORGOTTEN, "Opuštěná vesnice zapomenutého powerstonového království v pustině, se sloupem lamp uprostřed."},
    {"Stavby", "Potopené ruiny", "drowned_ruins", LOOT_CHEST_DROWNED, "Zbytky staveb zapomenutého království pod hladinou jezer a moří."},
    {"Stavby", "Rozbitý portál", "broken_portal", LOOT_CHEST_PORTAL, "Ruina starého portálu. Ukazuje cestu k pevnosti v Dimenzi úhelného kamene."},
    {"Stavby", "Karmínový důl", "", CRIMSON_PLANKS, "Opuštěné chodby z karmínového dřeva v Dimenzi úhelného kamene."},
    {"Stavby", "Kobka prázdnoty", "void_dungeon", LOOT_CHEST_VOID, "Místnost z prázdnotných cihel pod zemí Karmínové prázdnoty."},
    {"Stavby", "Věž prázdnoty", "void_tower", VOID_STONE_BRICKS, "Vysoká temná věž v Karmínové prázdnotě s truhlou a kompasem k aréně."},
    {"Stavby", "Strážní věž prázdnoty", "void_watchtower", VOID_PLANKS, "Strážní věž z prázdnotného dřeva nad fialovým lesem."},
    {"Stavby", "Aréna Karmínového muže", "arena", VOID_BRICK_EYE, "Kruhová aréna nad lávou se sloupy s očima. Tady čeká boss Karmínový muž."},
    {"Stavby", "Rám portálu do Nebeské říše", "portal", CELESTIAL_FRAME, "Dvanáct rámů v Zapomenutém Overworldu. Vlož do nich 12 hlav Karmínového muže a portál se otevře. Ukazuje na ně nebeský kompas."},
    {"Stavby", "Nebeský chrám", "temple", PALE_CRYSTAL, "Ostrov z bílého kamene s kruhem dvanácti sloupů s krystaly. Na pódiu čeká boss Strážce Nebe, který dá Nebeská křídla."},
    {"Mechaniky", "Beton", "concrete", CONCRETE, "Betonový prášek v 16 barvách padá jako písek. Ve vodě nebo v dešti ztvrdne na beton. Šedý prašný beton se vypálí v peci."},
    {"Mechaniky", "Barviva", "", DYE, "Z květin, listí, uhlí, hlíny, kaktusu a powerstonu; další barvy smícháním. Barví vlnu a beton."},
    {"Mechaniky", "Kamenořez a dřevořez", "cutters", CUTTER, "Kamenořez dělá z bloků schody a desky bez ztrát. Dřevořez dává 6 prken z kmene, schody, desky a ploty."},
    {"Mechaniky", "Portály", "portal", OBSIDIAN, "Obsidiánový portál vede do Dimenze úhelného kamene, portál v pevnosti do Karmínové prázdnoty, rám s 12 hlavami do Nebeské říše."},
};
const char* const kEncCategories[] = {"Dimenze", "Biomy", "Stavby", "Mechaniky", "Bloky a předměty", "Moby"};
}  // namespace
