#include "Achievements.h"

namespace vc {

const Achievement kAchievements[] = {
    {"first_block", "Na začátek", "Vytěž první blok.", 10},
    {"crafting_table", "Dílna", "Vyrob pracovní stůl.", 15},
    {"stone_pickaxe", "Doba kamenná", "Vyrob kamenný krumpáč.", 20},
    {"iron_ingot", "Horké zboží", "Vytav železný ingot.", 25},
    {"bronze_ingot", "Doba bronzová", "Získej bronzový ingot.", 30},
    {"steel_ingot", "Ocelové srdce", "Získej ocelový ingot.", 40},
    {"ruby", "Rudý poklad", "Najdi rubín.", 60},
    {"full_armor", "V plné zbroji", "Nos všechny čtyři kusy brnění.", 50},
    {"door", "Vítej doma", "Postav dveře.", 10},
    {"loot_ruin", "Zřícenina", "Otevři truhlu ve zřícenině.", 30},
    {"loot_ship", "Vrak", "Otevři truhlu v potopené lodi.", 50},
    {"loot_dungeon", "Kobka", "Otevři truhlu v kobce.", 50},
    {"loot_mine", "Opuštěný důl", "Otevři truhlu v dole.", 50},
    {"boat", "Námořník", "Pluj na loďce.", 25},
    {"nether", "Za branou", "Vstup do dimenze úhelného kamene.", 60},
    {"snow", "Zimní výlet", "Navštiv zasněžený kraj.", 15},
    {"cactus", "Au!", "Píchni se o kaktus.", 10},
    {"miner_100", "Horník", "Vytěž 100 bloků.", 40},
    {"miner_1000", "Krtek", "Vytěž 1000 bloků.", 100},
    {"multiplayer", "Spolu", "Hraj s někým v síti.", 40},
    {"loot_fortress", "Pán pevnosti", "Otevři truhlu v netherové pevnosti.", 60},
    {"crimson_void", "Do prázdnoty", "Vstup do Crimson Void.", 100},
    {"hunter", "Lovec příšer", "Poraz nějakou příšeru.", 20},
    {"sleep", "Dobrou noc", "Vyspi se v posteli.", 15},
    {"crimson_man", "Přemožitel", "Poraz Karmínového muže.", 500},
    {"loot_void", "Poklady prázdnoty", "Otevři truhlu ve věži nebo kobce Crimson Void.", 80},
    {"loot_explorer", "Cestovatel", "Otevři truhlu v chrámu, iglú nebo strážní věži.", 40},
    {"boom", "Bum!", "Odpal TNT.", 20},
    {"electrician", "Elektrikář", "Rozsviť powerstonovou lampu.", 30},
    {"gardener", "Zahradník", "Vypěstuj strom ze sazenice.", 20},
    {"farmer", "Farmář", "Skliď zralou pšenici.", 25},
    {"archer", "Lukostřelec", "Zasáhni bytost šípem.", 25},
    {"milk", "Čerstvé mléko", "Podoj krávu do kbelíku.", 15},
    {"trader", "Obchodník", "Uzavři obchod s vesničanem.", 30},
    // 1.4.0
    {"tar_hunter", "Dehtobijec", "Poraz dehtovou příšeru.", 60},
    {"cartographer", "Kartograf", "Vyrob si mapu.", 25},
    {"anniv_showcase", "Výroční výstava", "Navštiv výroční výstavu VoxelCraftu.", 30},
    {"anniv_party", "Oslava", "Postav balónek.", 15},
    {"anniv_popper", "Konfety!", "Vystřel konfety.", 15},
    {"anniv_collector", "Sběratel", "Vyrob výroční medaili z pěti žetonů.", 100},
    // 1.5.0 Forgotten Powerstone
    {"wasteland", "Zapomenutá země", "Navštiv Powerstonovou pustinu.", 20},
    {"loot_forgotten", "Opuštěný domov", "Otevři truhlu v zapomenuté vesnici.", 40},
    {"loot_drowned", "Pod hladinou", "Otevři truhlu v zatopených ruinách.", 50},
    {"loot_portal", "Rozbitá brána", "Otevři truhlu u rozpadlého portálu.", 40},
    {"archaeologist", "Archeolog", "Očisti štěrk s nálezy štětcem.", 30},
    {"historian", "Kronikář", "Přečti starý dokument.", 20},
    {"lore_master", "Pád zapomenutého království", "Přečti všech osm starých dokumentů.", 150},
    {"husking_hunter", "Konec Huskinga", "Poraz Huskinga.", 50},
};
const int kAchievementCount = sizeof(kAchievements) / sizeof(kAchievements[0]);

const Achievement* findAchievement(const std::string& id) {
    for (const Achievement& a : kAchievements)
        if (id == a.id) return &a;
    return nullptr;
}

}  // namespace vc
