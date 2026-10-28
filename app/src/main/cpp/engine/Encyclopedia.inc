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
    {"Mechaniky", "Beton", "concrete", CONCRETE, "Betonový prášek v 16 barvách padá jako písek. Ve vodě nebo v dešti ztvrdne na beton. Šedý prašný beton se vypálí v peci."},
    {"Mechaniky", "Barviva", "", DYE, "Z květin, listí, uhlí, hlíny, kaktusu a powerstonu; další barvy smícháním. Barví vlnu a beton."},
    {"Mechaniky", "Kamenořez a dřevořez", "cutters", CUTTER, "Kamenořez dělá z bloků schody a desky bez ztrát. Dřevořez dává 6 prken z kmene, schody, desky a ploty."},
    {"Mechaniky", "Portály", "portal", OBSIDIAN, "Obsidiánový portál vede do Dimenze úhelného kamene, portál v pevnosti do Karmínové prázdnoty, rám s 12 hlavami do Nebeské říše."},
};
const char* const kEncCategories[] = {"Dimenze", "Biomy", "Stavby", "Mechaniky", "Bloky a předměty", "Moby"};
}  // namespace
