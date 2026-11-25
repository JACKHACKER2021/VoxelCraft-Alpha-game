// 2.5.0 preview build 5: developer-only maps and structures.
//
// Six flat test worlds that only a logged-in developer can create (see Game::newWorldScreen), and
// five developer structures that stand in them. Everything is laid out from fixed coordinates, so
// the maps are the same in every copy and every update: tests/core_test.cpp checks their layout.
#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

#include "Blocks.h"
#include "GenConfig.h"
#include "Items.h"
#include "Mobs.h"
#include "Structure.h"
#include "World.h"
#include "WorldStructures.h"

namespace vc {

namespace {

// Concrete and wool colours (both in the order white, orange, magenta, light blue, yellow, lime,
// pink, gray, light gray, cyan, purple, blue, brown, green, red, black).
ItemId concrete(int c) { return (ItemId)(CONCRETE + c); }
ItemId wool(int c) { return c == 0 ? (ItemId)WOOL : (ItemId)(WOOL_ORANGE + c - 1); }
enum { White, Orange, Magenta, LightBlue, Yellow, Lime, Pink, Gray, LightGray, Cyan, Purple, Blue, Brown, Green, Red, Black };

Structure box(const char* name, int sx, int sy, int sz, ItemId fill = Structure::kVoid) {
    Structure s;
    s.name = name;
    s.sx = sx, s.sy = sy, s.sz = sz;
    s.cells.assign((size_t)sx * sy * sz, fill);
    return s;
}

void fillBox(Structure& s, int x0, int y0, int z0, int x1, int y1, int z1, ItemId id) {
    for (int y = std::max(0, y0); y <= std::min(s.sy - 1, y1); ++y)
        for (int z = std::max(0, z0); z <= std::min(s.sz - 1, z1); ++z)
            for (int x = std::max(0, x0); x <= std::min(s.sx - 1, x1); ++x) s.at(x, y, z) = id;
}

// Hollow walls of a box (no floor or roof).
void walls(Structure& s, int x0, int y0, int z0, int x1, int y1, int z1, ItemId id) {
    fillBox(s, x0, y0, z0, x1, y1, z0, id);
    fillBox(s, x0, y0, z1, x1, y1, z1, id);
    fillBox(s, x0, y0, z0, x0, y1, z1, id);
    fillBox(s, x1, y0, z0, x1, y1, z1, id);
}

// Big pixel letters (3 wide, 5 high) on the z face, read from the -z side (x counts from the right): V and S.
void letters(Structure& s, const char* text, int x, int y, int z, ItemId id) {
    for (const char* p = text; *p; ++p, x += 4) {
        const char* g = *p == 'V' ? "X.XX.XX.XX.X.X." : *p == 'S' ? "XXXX..XXX..XXXX" : nullptr;
        if (!g) continue;
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (g[row * 3 + col] == 'X' && x + col < s.sx && y + 4 - row < s.sy) s.at(s.sx - 1 - (x + col), y + 4 - row, z) = id;  // read from the -z side
    }
}

}  // namespace

// ---- The five developer structures ----

// VoxelSoftworks HQ: a concrete and glass studio with the logo, offices, a server room and the
// developer's journal (a board that reads out the notes of the updates).
Structure devHeadquarters() {
    Structure s = box("VoxelSoftworks HQ", 33, 19, 23, AIR);
    fillBox(s, 0, 0, 0, 32, 0, 22, concrete(LightGray));                 // the yard
    fillBox(s, 2, 1, 4, 30, 1, 20, concrete(White));                      // the ground floor
    walls(s, 2, 1, 4, 30, 12, 20, concrete(White));
    for (int y : {2, 3, 4, 7, 8, 9})                                     // bands of glass all round
        for (int x = 3; x <= 29; ++x)
            if (x % 4 != 0) s.at(x, y, 4) = s.at(x, y, 20) = GLASS;
    for (int y : {2, 3, 4, 7, 8, 9})
        for (int z = 5; z <= 19; ++z)
            if (z % 4 != 0) s.at(2, y, z) = s.at(30, y, z) = GLASS;
    fillBox(s, 3, 6, 5, 29, 6, 19, concrete(White));                      // the first floor
    fillBox(s, 2, 12, 4, 30, 12, 20, concrete(Gray));                     // the roof
    fillBox(s, 14, 2, 4, 18, 4, 4, AIR);                                  // the door
    fillBox(s, 15, 0, 0, 17, 0, 3, concrete(Lime));                       // a green path to it
    // The logo over the door: "VS" in lime concrete on a green band, lanterns on the roof.
    fillBox(s, 9, 13, 4, 23, 17, 4, concrete(Black));
    letters(s, "VS", 13, 13, 3, concrete(Lime));
    for (int x = 4; x <= 28; x += 6) s.at(x, 13, 6) = POWERSTONE_LAMP_ON;
    // Offices: desks of planks with a lamp each, along the windows.
    for (int x = 5; x <= 11; x += 3) {
        s.at(x, 2, 7) = PLANKS, s.at(x, 3, 7) = POWERSTONE_LAMP_ON, s.at(x, 2, 8) = BENCH;
        s.at(x, 7, 7) = PLANKS, s.at(x, 8, 7) = POWERSTONE_LAMP_ON, s.at(x, 7, 8) = BENCH;
    }
    // The server room: racks of steel and powerstone lights in rows.
    for (int x = 21; x <= 28; x += 2)
        for (int y = 2; y <= 4; ++y)
            for (int z = 8; z <= 17; ++z) s.at(x, y, z) = y == 3 && z % 2 ? (ItemId)POWERSTONE_LAMP_ON : (ItemId)STEEL_BLOCK;
    // The library and the journal: shelves and the board in the middle of the ground floor.
    for (int x = 5; x <= 12; ++x) s.at(x, 2, 19) = s.at(x, 3, 19) = BOOKSHELF;
    s.at(16, 2, 14) = STATION_SIGN;
    s.at(16, 1, 14) = concrete(Lime);
    // Stairs of slabs to the first floor.
    for (int i = 0; i < 5; ++i) s.at(4 + i, 1 + i, 17) = SLAB, s.at(4 + i, 1 + i, 18) = SLAB;
    fillBox(s, 4, 6, 17, 9, 6, 18, AIR);
    // A sign by the door with the name of the studio.
    s.at(13, 2, 3) = STATION_SIGN;
    return s;
}

// Hoppy's tower: a tall signature tower of stone bricks and glass with stairs round the inside,
// a lookout with a fence on top and a chest of developer things.
Structure devHoppyTower() {
    Structure s = box("Hoppy_Hopsy21 Tower", 11, 46, 11, AIR);
    fillBox(s, 0, 0, 0, 10, 0, 10, STONE_BRICKS);
    walls(s, 1, 1, 1, 9, 38, 9, STONE_BRICKS);
    for (int y = 4; y < 38; y += 6)  // windows on every side
        for (int d = 4; d <= 6; ++d) s.at(d, y, 1) = s.at(d, y, 9) = s.at(1, y, d) = s.at(9, y, d) = GLASS;
    for (int y = 1; y <= 38; y += 6)  // orange bands, Hoppy's colour
        for (int d = 1; d <= 9; ++d) s.at(d, y + 2, 1) = s.at(d, y + 2, 9) = s.at(1, y + 2, d) = s.at(9, y + 2, d) = concrete(Orange);
    fillBox(s, 4, 1, 1, 6, 3, 1, AIR);  // the door
    // Stairs: one slab per block of height round the inner wall.
    static const int kRing[][2] = {{2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2}, {7, 2}, {8, 2}, {8, 3}, {8, 4}, {8, 5}, {8, 6}, {8, 7},
                                   {8, 8}, {7, 8}, {6, 8}, {5, 8}, {4, 8}, {3, 8}, {2, 8}, {2, 7}, {2, 6}, {2, 5}, {2, 4}, {2, 3}};
    for (int y = 1; y <= 38; ++y) {
        const int* p = kRing[(y - 1) % 24];
        s.at(p[0], y, p[1]) = STONE_BRICKS;
        if (y + 1 <= 38) s.at(p[0], y + 1, p[1]) = AIR, s.at(p[0], y + 2, p[1]) = AIR;
    }
    // The lookout: a wider floor with a fence, a lamp in each corner, the chest and the name board.
    fillBox(s, 0, 39, 0, 10, 39, 10, STONE_BRICKS);
    fillBox(s, 2, 39, 3, 2, 39, 3, AIR);
    walls(s, 0, 40, 0, 10, 40, 10, FENCE);
    for (int x : {0, 10})
        for (int z : {0, 10}) s.at(x, 41, z) = POWERSTONE_LAMP_ON;
    s.at(5, 40, 5) = LOOT_CHEST_METRO;
    s.at(6, 40, 5) = STATION_SIGN;
    fillBox(s, 3, 44, 3, 7, 44, 7, concrete(Orange));  // a roof on posts
    for (int x : {3, 7})
        for (int z : {3, 7}) fillBox(s, x, 41, z, x, 43, z, FENCE);
    s.at(5, 45, 5) = POWERSTONE_LAMP_ON;
    return s;
}

// The version archive: a corridor with a room for each big version: 0.1, 1.0, 1.5, 2.0, 2.4, 2.5.
// Each room shows a little scene of what was new then, and its name board.
Structure devVersionArchive() {
    constexpr int kRooms = 6, kRoom = 10;
    Structure s = box("Version archive", kRooms * kRoom + 1, 7, 21, AIR);
    fillBox(s, 0, 0, 0, s.sx - 1, 0, 20, STONE_BRICKS);
    fillBox(s, 0, 6, 0, s.sx - 1, 6, 20, STONE_BRICKS);
    walls(s, 0, 1, 0, s.sx - 1, 5, 20, STONE_BRICKS);
    fillBox(s, 1, 1, 8, s.sx - 2, 5, 8, STONE_BRICKS);  // the corridor runs between z 9 and 11
    fillBox(s, 1, 1, 12, s.sx - 2, 5, 12, STONE_BRICKS);
    fillBox(s, 0, 1, 9, 0, 3, 11, AIR);                  // the way in
    for (int r = 0; r < kRooms; ++r) {
        const int x0 = r * kRoom;
        fillBox(s, x0, 1, 0, x0, 5, 8, STONE_BRICKS);  // the walls between rooms
        fillBox(s, x0 + 4, 1, 8, x0 + 6, 3, 8, AIR);   // the doorway from the corridor
        s.at(x0 + 7, 2, 9) = STATION_SIGN;             // its version on a board
        s.at(x0 + 5, 6, 4) = POWERSTONE_LAMP_ON;
        const int cx = x0 + 5, cz = 4;
        switch (r) {
            case 0:  // 0.1: grass, dirt and cobblestone, nothing else
                fillBox(s, cx - 2, 1, cz - 2, cx + 2, 1, cz + 2, GRASS);
                s.at(cx, 2, cz) = COBBLESTONE, s.at(cx + 1, 2, cz) = DIRT;
                break;
            case 1:  // 1.0: a little plank house
                walls(s, cx - 2, 1, cz - 2, cx + 2, 3, cz + 2, PLANKS);
                fillBox(s, cx - 2, 4, cz - 2, cx + 2, 4, cz + 2, SLAB);
                fillBox(s, cx, 1, cz + 2, cx, 2, cz + 2, AIR);
                break;
            case 2:  // 1.5: the powerstone kingdom
                fillBox(s, cx - 2, 1, cz - 2, cx + 2, 1, cz + 2, POWERSTONE_BRICKS);
                s.at(cx, 2, cz) = POWERSTONE_BLOCK, s.at(cx, 3, cz) = POWERSTONE_LAMP_ON;
                break;
            case 3:  // 2.0: cushions round a table
                s.at(cx, 1, cz) = PLANKS;
                s.at(cx - 1, 1, cz) = s.at(cx + 1, 1, cz) = CUSHION;
                break;
            case 4:  // 2.4: a platform and track
                for (int x = cx - 3; x <= cx + 3; ++x) s.at(x, 1, cz) = TRACK_BED, s.at(x, 2, cz) = RAIL_EW, s.at(x, 1, cz + 1) = PLATFORM;
                break;
            default:  // 2.5: the metro tiles and a bench
                fillBox(s, cx - 3, 1, cz - 3, cx + 3, 4, cz - 3, METRO_TILE);
                fillBox(s, cx - 3, 3, cz - 3, cx + 3, 3, cz - 3, METRO_TILE_BLUE);
                s.at(cx, 1, cz) = BENCH, s.at(cx + 2, 1, cz) = STATION_SIGN;
                break;
        }
    }
    return s;
}

// The test track: an oval of rails on a track bed with a depot shed, and a board every
// 10 blocks along the long straight (distance marks for measuring speed).
Structure devTestTrack() {
    Structure s = box("Test track", 71, 7, 31, Structure::kVoid);
    const int x0 = 2, z0 = 2, x1 = 68, z1 = 28;
    for (int x = x0; x <= x1; ++x) s.at(x, 0, z0) = s.at(x, 0, z1) = TRACK_BED, s.at(x, 1, z0) = s.at(x, 1, z1) = RAIL_EW;
    for (int z = z0; z <= z1; ++z) s.at(x0, 0, z) = s.at(x1, 0, z) = TRACK_BED, s.at(x0, 1, z) = s.at(x1, 1, z) = RAIL_NS;
    s.at(x0, 1, z0) = RAIL_CURVE + 1, s.at(x1, 1, z0) = RAIL_CURVE + 2;  // N-W corner turns east-south...
    s.at(x1, 1, z1) = RAIL_CURVE + 3, s.at(x0, 1, z1) = RAIL_CURVE + 0;
    for (int x = x0 + 4; x < x1; x += 10) s.at(x, 1, z0 - 1) = STATION_SIGN;  // the distance marks
    // The depot: a shed of steel over the far straight, open at both ends.
    walls(s, 24, 1, z1 - 3, 46, 5, z1 + 2, STEEL_BLOCK);
    fillBox(s, 24, 1, z1, 24, 3, z1, AIR);
    fillBox(s, 46, 1, z1, 46, 3, z1, AIR);
    fillBox(s, 24, 6, z1 - 3, 46, 6, z1 + 2, LATTICE);
    for (int x = 26; x <= 44; ++x) s.at(x, 1, z1 - 1) = PLATFORM;
    return s;
}

// The broken world: a "glitched" building on purpose: blocks hang in the air, chunks of it are
// missing, and the walls show the black and magenta checks of a missing texture.
Structure devGlitch() {
    Structure s = box("Broken world", 21, 24, 21, Structure::kVoid);
    for (int y = 0; y < 12; ++y)
        for (int z = 3; z <= 17; ++z)
            for (int x = 3; x <= 17; ++x) {
                const bool wall = x == 3 || x == 17 || z == 3 || z == 17 || y == 0 || y == 11;
                if (!wall) continue;
                if (hash3(x / 3, y / 3, z / 3, 0x6117u) % 5 == 0) continue;  // missing chunks
                s.at(x, y, z) = (x + y + z) % 2 ? wool(Black) : wool(Magenta);
            }
    // Blocks floating above it, out of place and out of line.
    for (int i = 0; i < 70; ++i) {
        const uint32_t h = hash3(i, 7, i * 3, 0x6118u);
        const int x = (int)(h % 21), y = 13 + (int)((h >> 8) % 11), z = (int)((h >> 16) % 21);
        static const ItemId kOdd[] = {GRASS, WATER, GLASS, POWERSTONE_LAMP_ON, CONCRETE + Magenta, STONE_BRICKS, METRO_TILE_CRACKED, SAND};
        s.at(x, y, z) = kOdd[(h >> 24) % 8];
    }
    s.at(10, 1, 2) = STATION_SIGN;
    return s;
}

// ---- The six developer maps ----

const char* devMapName(int map) {
    static const char* const kNames[kDevMaps + 1] = {"Žádná", "Testovací svět struktur", "Galerie bloků", "Metro laboratoř",
                                                     "Biomový šachovnicový svět", "Mob aréna", "Trailer studio"};
    return kNames[std::clamp(map, 0, kDevMaps)];
}

namespace {

struct DevPiece {
    Structure s;
    int ox, oy, oz;
    std::string sign;  // the text of the boards in it (\n-free)
};

struct DevSign {
    int x, y, z;
    std::string text;
    std::vector<std::string> pages;  // more than one: a tap turns to the next (the HQ journal)
    int mark = -1;                   // >= 0: a distance mark of the test track (metres from the start)
};

// The developer's journal in the HQ: the notes of the updates, one page per tap.
const std::vector<std::string>& hqJournal() {
    static const std::vector<std::string> kPages = {
        "Deník vývojáře: VoxelCraft od Hoppy_Hopsy21, VoxelSoftworks. Klepni znovu pro další stránku.",
        "0.1 - 1.0: první kostky, tráva, hlína a kámen; pak domy z prken, nástroje a přežití.",
        "1.5: zapomenuté powerstonové království, opuštěné vesnice a rozbité portály.",
        "2.0: módy a balíčky, polštáře a nové menu; 2.1: koleje a vlaky.",
        "2.2: Cornerstone a mráz: hlubší Overworld, tenký led a Rozhlas s naším hlasem.",
        "2.3: první metro a bronzové síně; 2.4: nádraží, nástupiště, kolejové lože a mosty.",
        "2.5: síť metra, automatické metro, ztracené město, deníky průzkumníka a vraky vlaků.",
        "2.5: seedy struktur, Voxel Bucks, nové pixelové skiny, Kapitán Moře a hlášení chyb.",
        "Příště ve 2.6: noví tvorové v Cornerstone, lepší Rozhlas a hra přes internet.",
        "Vydání na Google Play: během roku 2027, rok od vzniku VoxelCraftu. Díky, že hraješ!",
    };
    return kPages;
}

struct DevLayout {
    std::vector<DevPiece> pieces;
    std::vector<DevSign> signs;
};

constexpr int kGround = 4;  // flat ground: the classic layers end at y = 3

// The names of the world structures, for the boards of the test world.
const char* structureName(WorldStructure k) {
    switch (k) {
        case WorldStructure::Ruin: return "Ruina";
        case WorldStructure::Shipwreck: return "Vrak lodi";
        case WorldStructure::Dungeon: return "Kobka";
        case WorldStructure::Mineshaft: return "Důl";
        case WorldStructure::Fortress: return "Pevnost";
        case WorldStructure::BossArena: return "Aréna";
        case WorldStructure::VoidDungeon: return "Kobka Prázdnoty";
        case WorldStructure::VoidTower: return "Věž Prázdnoty";
        case WorldStructure::DesertTemple: return "Pouštní chrám";
        case WorldStructure::Igloo: return "Iglú";
        case WorldStructure::Watchtower: return "Strážní věž";
        case WorldStructure::Village: return "Vesnice";
        case WorldStructure::ForgottenVillage: return "Zapomenutá vesnice";
        case WorldStructure::DrownedRuins: return "Potopené ruiny";
        case WorldStructure::BrokenPortal: return "Rozbitý portál";
        case WorldStructure::CrimsonMineshaft: return "Karmínový důl";
        case WorldStructure::GreatFortress: return "Velká pevnost";
        case WorldStructure::VoidMineshaft: return "Důl Prázdnoty";
        case WorldStructure::VoidWatchtower: return "Strážní věž Prázdnoty";
        case WorldStructure::RuinedVillage: return "Zničená vesnice";
        case WorldStructure::CaveDungeon: return "Jeskynní kobka";
        case WorldStructure::Camp: return "Tábor";
        case WorldStructure::RuinedStation: return "Zničená stanice";
        case WorldStructure::TrainWreck: return "Vrak vlaku";
        default: return "?";
    }
}

void addPiece(DevLayout& L, Structure s, int ox, int oz, const std::string& name) {
    // The structure's own boards get its name; one more board in front of it. The HQ's journal
    // board holds the journal, the test track's boards along the straight their distances.
    const bool track = s.name == "Test track";
    for (int y = 0; y < s.sy; ++y)
        for (int z = 0; z < s.sz; ++z)
            for (int x = 0; x < s.sx; ++x) {
                if (s.at(x, y, z) != STATION_SIGN) continue;
                DevSign sign{ox + x, kGround + y, oz + z, name, {}, -1};
                if (s.name == "VoxelSoftworks HQ" && x == 16 && y == 2 && z == 14) sign.pages = hqJournal();
                if (track && y == 1 && z == 1) sign.mark = (x - 6) * 1;  // the marks stand every 10 blocks from x 6
                L.signs.push_back(sign);
            }
    L.signs.push_back({ox + s.sx / 2, kGround, oz - 3, name});
    L.pieces.push_back({std::move(s), ox, kGround, oz, name});
}

DevLayout buildLayout(int map) {
    DevLayout L;
    switch (map) {
        case 1: {
            // Every world structure in one row along +x (8 blocks apart), the developer ones in a second row.
            int x = 0;
            for (int k = 0; k < (int)WorldStructure::Count; ++k) {
                if (!kFeatures250 && k == (int)WorldStructure::TrainWreck) continue;
                if (!kFeatures240 && k == (int)WorldStructure::RuinedStation) continue;
                Structure s = buildWorldStructure((WorldStructure)k, 0x5EEDu + k);
                if (!s.valid()) continue;
                const int w = s.sx;
                addPiece(L, std::move(s), x, 10, structureName((WorldStructure)k));
                x += w + 8;
            }
            int dx = 0;
            const std::pair<Structure, const char*> dev[] = {{devHeadquarters(), "VoxelSoftworks HQ"}, {devHoppyTower(), "Hoppyho věž"},
                                                             {devVersionArchive(), "Archiv verzí"}, {devTestTrack(), "Zkušební dráha"},
                                                             {devGlitch(), "Rozbitý svět"}};
            for (const auto& [s, n] : dev) {
                const int w = s.sx;
                addPiece(L, s, dx, -110, n);
                dx += w + 10;
            }
            break;
        }
        case 2: {
            // A row for each creative category, a stone brick pedestal for each block, 3 blocks apart.
            for (int cat = 0; cat < CatCount; ++cat) {
                const std::vector<ItemId>& items = creativeCategoryItems(cat);
                Structure s = box("Gallery row", (int)std::max<size_t>(1, items.size()) * 3 + 2, 2, 3, Structure::kVoid);
                int i = 0;
                for (ItemId id : items) {
                    if (!isPlaceable(id) || id >= kExtIdBase + 4096) continue;
                    s.at(1 + i * 3, 0, 1) = STONE_BRICKS;
                    s.at(1 + i * 3, 1, 1) = id;
                    ++i;
                }
                fillBox(s, 0, 0, 0, s.sx - 1, 0, 0, concrete(Lime));
                addPiece(L, std::move(s), 0, cat * 8, creativeCategoryName(cat));
            }
            break;
        }
        case 3: {
            // The metro lab: the test track as its loop, with a north-south line across it that meets
            // the loop at two crossings, and platforms of all four station sizes along the loop.
            Structure t = devTestTrack();
            const int mid = t.sx / 2;
            for (int z = 0; z < t.sz; ++z) {
                if (t.at(mid, 0, z) == Structure::kVoid || t.at(mid, 0, z) == AIR) t.at(mid, 0, z) = TRACK_BED;
                t.at(mid, 1, z) = z == 2 || z == 28 ? (ItemId)RAIL_CROSS : (ItemId)RAIL_NS;
            }
            addPiece(L, std::move(t), 0, 0, "Metro laboratoř");
            // Lines out of the loop north and south, to the end of the line both ways.
            Structure line = box("Lab line", 1, 2, 40, Structure::kVoid);
            for (int z = 0; z < 40; ++z) line.at(0, 0, z) = TRACK_BED, line.at(0, 1, z) = RAIL_NS;
            L.pieces.push_back({line, mid, kGround, -40, ""});
            L.pieces.push_back({line, mid, kGround, 31, ""});
            static const int kLens[4] = {40, 48, 44, 64};
            static const char* const kKinds[4] = {"Zastávka (40)", "Depo (48)", "Konečná (44)", "Uzel (64)"};
            for (int k = 0; k < 4; ++k) {
                Structure p = box("Lab platform", 3, 1, kLens[k] / 4, PLATFORM);
                addPiece(L, std::move(p), mid + 2, k < 2 ? -38 + k * 18 : 33 + (k - 2) * 18, kKinds[k]);
            }
            break;
        }
        case 4: {
            // Boards only: the squares themselves come from biomeAt() and the ground layers.
            break;
        }
        case 5: {
            // A pen of glass for every kind of mob, a spawner in the middle, its name in front.
            for (int m = 0; m < (int)MobType::Count; ++m) {
                Structure p = box("Pen", 11, 6, 11, Structure::kVoid);
                fillBox(p, 0, 0, 0, 10, 0, 10, STONE_BRICKS);
                walls(p, 0, 1, 0, 10, 4, 10, GLASS);
                fillBox(p, 1, 1, 1, 9, 4, 9, AIR);
                p.at(5, 1, 5) = MOB_SPAWNER;
                p.at(5, 5, 5) = POWERSTONE_LAMP_ON;
                addPiece(L, std::move(p), (m % 6) * 14, (m / 6) * 16, mobInfo((MobType)m).name);
            }
            break;
        }
        case 6: {
            // The studio: a lime green floor and a curved green screen behind it, lamps all round.
            Structure s = box("Trailer studio", 61, 22, 41, Structure::kVoid);
            fillBox(s, 0, 0, 0, 60, 0, 40, concrete(Lime));
            for (int x = 0; x <= 60; ++x)
                for (int y = 1; y < 22; ++y) s.at(x, y, 0) = concrete(Lime);
            for (int z = 1; z <= 12; ++z)  // the sides curve round a little
                for (int y = 1; y < 22; ++y) s.at(0, y, z) = s.at(60, y, z) = concrete(Lime);
            for (int x = 4; x <= 56; x += 8) s.at(x, 21, 20) = POWERSTONE_LAMP_ON, s.at(x, 21, 36) = POWERSTONE_LAMP_ON;
            addPiece(L, std::move(s), -30, -10, "Trailer studio");
            break;
        }
        default: break;
    }
    return L;
}

const DevLayout& layout(int map) {
    static std::once_flag once[kDevMaps + 1];
    static DevLayout cache[kDevMaps + 1];
    map = std::clamp(map, 0, kDevMaps);
    std::call_once(once[map], [map] { cache[map] = buildLayout(map); });
    return cache[map];
}

}  // namespace

void World::stampDevMap(Chunk& c) {
    const int map = config_.devMap;
    // The biome checkerboard: the ground of each 48-block square in its biome's blocks, and trees.
    if (map == 4 && !config_.biomes.empty()) {
        for (int x = 0; x < kChunkSize; ++x)
            for (int z = 0; z < kChunkSize; ++z) {
                const int wx = c.cx * kChunkSize + x, wz = c.cz * kChunkSize + z;
                const BiomeDef& b = *biomeAt(wx, wz);
                if (b.filler) c.set(x, 1, z, b.filler), c.set(x, 2, z, b.filler);
                if (b.surface) c.set(x, 3, z, b.surface);
                if (b.freeze) c.set(x, 3, z, SNOW_BLOCK);
            }
        for (int x = 2; x < kChunkSize - 2; ++x)
            for (int z = 2; z < kChunkSize - 2; ++z) {
                const int wx = c.cx * kChunkSize + x, wz = c.cz * kChunkSize + z;
                const BiomeDef& b = *biomeAt(wx, wz);
                const uint32_t h = hash3(wx, 404, wz, 0xD3Bu);
                if (b.trunk && b.leaves && b.treeDensity > 0 && (h % 1000) < (uint32_t)(b.treeDensity * 1000 * 0.6f) && wx % 48 > 4 && wz % 48 > 4)
                    placeTree(c, x, kGround, z, h, b.trunk, b.leaves);
            }
    }
    const DevLayout& L = layout(map);
    for (const DevPiece& p : L.pieces) stampBlueprint(c, p.s, p.ox, p.oy, p.oz, false);
    // The boards in front of the pieces.
    const int x0 = c.cx * kChunkSize, z0 = c.cz * kChunkSize;
    for (const DevSign& s : L.signs)
        if (s.x >= x0 && s.x < x0 + kChunkSize && s.z >= z0 && s.z < z0 + kChunkSize && c.get(s.x - x0, s.y, s.z - z0) == AIR)
            c.set(s.x - x0, s.y, s.z - z0, STATION_SIGN);
}

std::string World::devSignText(int x, int y, int z, int page, int* pages, int* mark) const {
    if (pages) *pages = 1;
    if (mark) *mark = -1;
    if (config_.devMap <= 0) return "";
    for (const DevSign& s : layout(config_.devMap).signs) {
        if (s.x != x || s.y != y || s.z != z) continue;
        if (mark) *mark = s.mark;
        if (s.mark >= 0) return s.text + ": " + std::to_string(s.mark) + " m";
        if (s.pages.empty()) return s.text;
        const int n = (int)s.pages.size(), i = ((page % n) + n) % n;
        if (pages) *pages = n;
        return std::to_string(i + 1) + "/" + std::to_string(n) + "  " + s.pages[(size_t)i];
    }
    return "";
}

const BiomeDef* World::devBiomeAt(int x, int z) const {
    const int n = (int)config_.biomes.size();
    const int sq = (floorDiv(x, 48) % 4 + 4) % 4 + ((floorDiv(z, 48) % 2 + 2) % 2) * 4;  // 4 x 2 squares, repeated
    return &config_.biomes[(size_t)(sq % n)];
}

int devMapSpawn(int map, int& x, int& z) {
    switch (map) {
        case 1: x = -6, z = 4; break;    // between the two rows
        case 2: x = -6, z = 4; break;
        case 3: x = 35, z = 15; break;   // inside the loop
        case 4: x = 96, z = 48; break;   // where four squares meet
        case 5: x = 38, z = -6; break;
        case 6: x = 0, z = 15; break;
        default: x = z = 0; return 0;
    }
    return kGround;
}

int devMapCount() { return kDevMaps; }

}  // namespace vc
