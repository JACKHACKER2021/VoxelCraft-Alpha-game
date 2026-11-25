#include <set>
#include <tuple>
#include "Session.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BlockShapes.h"
#include "Blocks.h"
#include "Lang.h"
#include "Mods.h"
#include "Portal.h"
#include "Sky.h"
#include "Trading.h"
#include "WorldStructures.h"
#include "Commands.h"
#include "Structure.h"

namespace vc {

namespace {
constexpr double kHoldToBreak = 0.3;    // creative: a finger must rest this long before breaking
constexpr double kBreakRepeat = 0.25;   // creative: repeated breaking while holding
constexpr double kSurvivalDelay = 0.15; // survival: mining starts after this
constexpr double kDoubleTap = 0.3;      // double-tap jump toggles creative flight
constexpr float kLookPerGuiPixel = 0.016f;
constexpr float kPortalDelay = 2.0f;
constexpr float kMaxHealth = 20, kMaxFood = 20, kMaxAir = 15;
constexpr float kAutosave = 60;

const ItemId kCreativeHotbar[kHotbarSize] = {GRASS, COBBLESTONE, PLANKS, LOG, GLASS, BRICKS, NETHERRACK, OBSIDIAN, FLINT_AND_STEEL};
uint64_t cellKey(int x, int y, int z) { return World::liquidCellKey(x, y, z); }
bool fallsDown(ItemId b) { return b == SAND || b == GRAVEL || b == RED_SAND || b == DEBRIS_GRAVEL || b == DARK_SAND || isConcretePowder(b); }  // 1.7.2 build 2: dusty concrete is a powder
// What a falling block or a flowing liquid simply replaces.
bool washedAway(ItemId b) { return b == AIR || isPlant(b) || isTorch(b) || b == SNOW_LAYER || b == LEAF_LITTER; }
// 1.9.5: what a block sounds like underfoot and when it is dug (the Minetest Game sound sets).
enum class SoundMat { Grass, Dirt, Sand, Gravel, Stone, Wood, Snow, Ice, Glass, Leaves };
SoundMat soundMaterial(ItemId b) {
    const ItemId base = isStairs(b) || isSlab(b) ? shapeMaterial(b) : b;
    const int m = shapeMaterialOf(base);
    if (base == AMBER_RESIN || base == PRISMATIDE_PILLAR || base == LUMINOUS_SALT || base == GLASS_REEDS) return SoundMat::Glass;
    if (base == GLASS || base == POWERSTONE_GLASS || isStainedGlass(base) || base == PALE_CRYSTAL || base == VOID_SHARD_BLOCK) return SoundMat::Glass;
    if (base == ICE) return SoundMat::Ice;
    if (base == SNOW_BLOCK || base == SNOW_LAYER) return SoundMat::Snow;
    if (isLeaves(base) || isPlant(base) || base == LEAF_LITTER || base == VINES || base == MOSS_BLOCK || base == HAY_BALE || woolColor(base) >= 0 || isCarpet(base))
        return SoundMat::Leaves;
    if (base == SAND || base == RED_SAND || base == DARK_SAND || isConcretePowder(base)) return SoundMat::Sand;
    if (base == GRAVEL || base == DEBRIS_GRAVEL || base == CLAY) return SoundMat::Gravel;
    if (isLog(base) || (m >= 0 && isWoodMaterial(m)) || isChest(base) || isFence(base) || base == BARREL || base == CRAFTING_TABLE || base == BOOKSHELF ||
        isDoor(base) || isTrapdoor(base) || base == CAMPFIRE || base == WOOD_CUTTER)
        return SoundMat::Wood;
    if (base == GRASS || base == NETHER_GRASS || base == VOID_GRASS || base == SOULED_GRASS || base == CELESTIAL_GRASS || base == WASTELAND_GRASS)
        return SoundMat::Grass;
    if (base == DIRT || base == FARMLAND || base == SOULED_DIRT || base == CELESTIAL_DIRT) return SoundMat::Dirt;
    return SoundMat::Stone;
}
// 1.9.5: the sound style of the blocks Minecraft does not have: deep for the Cornerstone
// Dimension, dark and echoing for the Crimson Void, airy for the Celestial Reach, ringing for
// crystals and buzzing for powerstone ("" = the plain sound).
const char* blockSoundStyle(ItemId b) {
    const ItemId base = isStairs(b) || isSlab(b) ? shapeMaterial(b) : b;
    switch (base) {
        case PALE_CRYSTAL: case VOID_SHARD_BLOCK: case SHARD_CLUSTER: return "crystal";
        case POWERSTONE_ORE: case POWERSTONE_BLOCK: case POWERSTONE_LAMP: case POWERSTONE_LAMP_ON: case POWERSTONE_LIGHT:
        case POWERSTONE_BRICKS: case CRACKED_POWERSTONE_BRICKS: case CHISELED_POWERSTONE_BRICKS: case POWERSTONE_GLASS: case POWERSTONE_FENCE:
        case POWERSTONE_FURNACE: case POWERSTONE_FURNACE_ON: return "power";
        case GILDED_COBBLE: return "crystal";  // 2.2.0: a metal clink underfoot
        case CELESTIAL_GRASS: case CELESTIAL_DIRT: case CELESTIAL_STONE: case CELESTIAL_STONE_BRICKS: case CELESTIAL_LOG: case CELESTIAL_PLANKS:
        case CELESTIAL_LEAVES: case PALE_STONE: case SKY_STONE: case CELESTIUM_ORE: return "sky";
        case VOID_STONE: case VOID_GRASS: case VOID_LOG: case VOID_PLANKS: case VOID_LEAVES: case VOID_STONE_BRICKS: case EYE_LOG: case VOID_BRICK_EYE:
        case CRIMSON_STONE: case CRIMSONIUM_LOG: case CRIMSONIUM_PLANKS: case CRIMSONIUM_LEAVES: case CRIMSONIUM_ORE: case SHATTERED_STONE:
        case SHATTERED_BRICKS: case DARK_SAND: case DARK_SANDSTONE: case SOULED_GRASS: case SOULED_DIRT: case SOULED_STONE: return "void";
        case NETHERRACK: case NETHER_GRASS: case NETHER_BRICK: case CRACKED_NETHER_BRICKS: case CHISELED_NETHER_BRICKS: case NETHER_BRICK_FENCE:
        case NETHERPERK_LOG: case NETHERPERK_CAP: case NETHERPERK_PLANKS: case WASTELAND_GRASS: return "cs";
        default: return "";
    }
}

Sound stepSound(SoundMat m) {
    static const Sound k[] = {Sound::Step, Sound::Step, Sound::StepSand, Sound::StepGravel, Sound::StepStone, Sound::StepWood, Sound::StepSnow,
                              Sound::StepIce, Sound::StepIce, Sound::Step};
    return k[(int)m];
}
Sound digSound(SoundMat m) {
    static const Sound k[] = {Sound::DigDirt, Sound::DigDirt, Sound::DigDirt, Sound::DigGravel, Sound::Dig, Sound::DigWood, Sound::DigDirt,
                              Sound::DigIce, Sound::Glass, Sound::DigLeaves};
    return k[(int)m];
}

// 1.9.0 build 2: what fire catches on and burns away.
bool isFlammable(ItemId b) {
    if (b == FIRE || b == SHARD_CLUSTER) return false;
    if (isLog(b) || isLeaves(b) || isPlant(b) || woolColor(b) >= 0 || isCarpet(b) || isTnt(b) || b == LEAF_LITTER) return true;
    if (b == BOOKSHELF || b == BARREL || b == LOOT_BARREL_CAMP || b == CHEST || b == WOOD_CUTTER || b == CRAFTING_TABLE || b == HAY_BALE)
        return true;
    if (isFence(b)) return b != NETHER_BRICK_FENCE && b != POWERSTONE_FENCE;
    const int m = shapeMaterialOf(isStairs(b) || isSlab(b) ? shapeMaterial(b) : b);
    return m >= 0 && isWoodMaterial(m);
}
}  // namespace

Session::Session(const WorldInfo& info, const PlayerState& player)
    : info_(info),
      state_(player),
      config_(loadGenerator(info.folder)),
      overworld_(info.seed, Dimension::Overworld, config_),
      nether_(info.seed, Dimension::Nether, config_),
      void_(info.seed, Dimension::Void, config_),
      celestial_(info.seed, Dimension::Celestial, config_),
      world_(&overworld_) {
    overworld_.setStorageDir(info.folder + "/overworld");
    nether_.setStorageDir(info.folder + "/nether");
    void_.setStorageDir(info.folder + "/void");
    celestial_.setStorageDir(info.folder + "/celestial");
    overworld_.changeLog = &changes_[0];
    nether_.changeLog = &changes_[1];
    void_.changeLog = &changes_[2];
    celestial_.changeLog = &changes_[3];
    overworld_.flowLevels = &flow_[0];
    nether_.flowLevels = &flow_[1];
    void_.flowLevels = &flow_[2];
    celestial_.flowLevels = &flow_[3];
    loadFlow();
    {
        std::ifstream in(info.folder + "/waypoints.txt");
        Waypoint w;
        while (in >> w.dim >> w.x >> w.z && std::getline(in, w.name) && waypoints_.size() < 200) {
            if (!w.name.empty() && w.name[0] == ' ') w.name.erase(0, 1);
            waypoints_.push_back(w);
        }
    }
    {
        std::ifstream in(info.folder + "/books.txt");
        std::string word, s;
        int id = 0;
        size_t pages = 0;
        auto line = [&]() {
            std::getline(in, s);
            std::replace(s.begin(), s.end(), '\x01', '\n');
            return s;
        };
        while (in >> word >> id >> pages && word == "book" && pages <= 100) {
            std::getline(in, s);
            Book& b = books_[id];
            b.title = line(), b.author = line();
            b.pages.clear();
            for (size_t i = 0; i < pages; ++i) b.pages.push_back(line());
            if (b.pages.empty()) b.pages.push_back("");
        }
    }
    rng_ ^= info.seed | 1;
    furnaces_ = loadFurnaces(info.folder);
    drops_.load(info.folder);
    chests_ = loadChests(info.folder);
    boats_.load(info.folder);
    trains_.load(info.folder);
    // 2.1.0: trains are a preview feature: on in every preview world, off in the normal edition.
    info_.expRails = !oldMode();  // 2.1.0: rails and trains in every new-style world
    mobs_.load(info.folder);
    paperMaps_.load(info.folder);
    loadTimers();
    spawnRng_ ^= info.seed * 2654435761u;

    // 2.2.0: a world saved before the deeper Overworld: its land now lies kDeepShift blocks higher.
    if (state_.valid && state_.legacyHeight && overworld_.yShift() > 0) {
        const float s = (float)overworld_.yShift();
        state_.spawn.y += s;
        if (state_.dimension == 0) state_.pos.y += s;
        if (state_.hasShowcase) state_.showcaseY += overworld_.yShift();
        state_.legacyHeight = false;
    }
    if (state_.valid) {
        world_ = &worldAt(state_.dimension);
        player_.pos = state_.pos;
        player_.yaw = state_.yaw;
        player_.pitch = state_.pitch;
        player_.flying = state_.flying && !survival();
        int cx = floorDiv((int)std::floor(player_.pos.x), kChunkSize), cz = floorDiv((int)std::floor(player_.pos.z), kChunkSize);
        world_->ensureChunksAround(cx, cz, 1, 9);
    } else {
        state_ = PlayerState{};
        state_.valid = true;
        if (!survival())
            for (int i = 0; i < kHotbarSize; ++i) state_.inventory.slots[i] = {kCreativeHotbar[i], 1};
        findSpawn();
        player_.pos = state_.spawn;
    }
    camera_ = player_;
}

void Session::findSpawn() {
    int x = overworld_.config().spawnX, z = overworld_.config().spawnZ;
    // Walk outward until we find dry land.
    if (overworld_.config().boundedChunks > 0) {
        // 1.9.5 build 2: a bounded world: dry land inside the square, spiralling out from the middle.
        const int half = overworld_.config().boundedChunks * kChunkSize / 2 - 3;
        x = z = 0;
        for (int r = 0, found = 0; r <= half && !found; r += 2)
            for (int dx = -r; dx <= r && !found; dx += 2)
                for (int dz = -r; dz <= r && !found; dz += 2)
                    if (overworld_.surfaceHeight(dx, dz) > overworld_.seaLevel()) x = dx, z = dz, found = 1;
    }
    for (int i = 0; i < 64 && !overworld_.config().flat && overworld_.config().boundedChunks <= 0 && overworld_.surfaceHeight(x, z) <= overworld_.seaLevel(); ++i)
        x += 16;
    overworld_.ensureChunksAround(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize), 1, 9);
    state_.spawn = Vec3(x + 0.5f, (float)overworld_.skyHeight(x, z) + 0.01f, z + 0.5f);
}

void Session::landFallingBlocks() {
    for (const FallingBlock& f : fallingBlocks_) {
        World& w = worldAt(f.dim);
        const int x = (int)std::floor(f.pos.x), z = (int)std::floor(f.pos.z);
        int y = (int)std::floor(f.pos.y);
        while (y > 0) {
            const ItemId below = w.getBlock(x, y - 1, z);
            if (!washedAway(below) && !isLiquid(below)) break;
            --y;
        }
        if (y >= 0 && y < kChunkHeight) w.setBlock(x, y, z, f.id);
    }
    fallingBlocks_.clear();
}

void Session::save() {
    landFallingBlocks();
    state_.pos = player_.pos;
    state_.yaw = player_.yaw;
    state_.pitch = player_.pitch;
    state_.dimension = dimIndex();
    state_.flying = player_.flying;
    info_.lastPlayed = unixNow();
    overworld_.saveModifiedChunks();
    nether_.saveModifiedChunks();
    void_.saveModifiedChunks();
    celestial_.saveModifiedChunks();
    saveLevel(info_, state_);
    saveFurnaces(info_.folder, furnaces_);
    drops_.save(info_.folder);
    saveChests(info_.folder, chests_);
    boats_.save(info_.folder);
    trains_.save(info_.folder);
    mobs_.save(info_.folder);
    paperMaps_.save(info_.folder);
    saveTimers();
    saveFlow();
    {
        std::ofstream out(info_.folder + "/waypoints.txt");
        for (const Waypoint& w : waypoints_) out << w.dim << " " << w.x << " " << w.z << " " << w.name << "\n";
    }
    {
        // books.txt: "book <id> <pages>", then the title, the author and each page on a line
        // ('\n' inside a page is written as '\x01').
        std::ofstream out(info_.folder + "/books.txt");
        auto line = [&](std::string s) {
            std::replace(s.begin(), s.end(), '\n', '\x01');
            out << s << "\n";
        };
        for (const auto& [id, b] : books_) {
            out << "book " << id << " " << b.pages.size() << "\n";
            line(b.title), line(b.author);
            for (const std::string& p : b.pages) line(p);
        }
    }
}

Furnace* Session::furnace(const FurnaceKey& key) {
    auto it = furnaces_.find(key);
    return it == furnaces_.end() ? nullptr : &it->second;
}

ChestContents* Session::chest(const FurnaceKey& key) {
    auto it = chests_.find(key);
    return it == chests_.end() ? nullptr : &it->second;
}

ChestContents Session::structureLoot(ItemId block, uint32_t seed) const {
    ChestContents c = generateLoot(block, seed);
    // 1.5.7 Exploration Maps experiment: a quarter of the structure chests hold a map to a mineshaft.
    if (explorationMaps() && (seed >> 7) % 100 < 25)
        for (ItemStack& slot : c)
            if (slot.empty()) {
                slot = {EXPLORER_MAP, 1, 0};
                break;
            }
    return c;
}

ChestContents& Session::chestFor(const FurnaceKey& key) {
    if (!chests_.count(key)) {
        const ItemId block = worldAt(key.dim).getBlock(key.x, key.y, key.z);
        const uint32_t seed = info_.seed ^ (uint32_t)(key.x * 73856093) ^ (uint32_t)(key.y * 19349663) ^ (uint32_t)(key.z * 83492791);
        chests_[key] = (block != CHEST && isChest(block)) || block == LOOT_BARREL_CAMP ? structureLoot(block, seed) : ChestContents{};
    }
    return chests_[key];
}

Furnace& Session::furnaceFor(const FurnaceKey& key) { return furnaces_[key]; }

void Session::openChestAt(const FurnaceKey& key) {
    sound(Sound::Chest);  // 1.9.5
    World& w = worldAt(key.dim);
    ItemId block = w.getBlock(key.x, key.y, key.z);
    if (!chests_.count(key)) {
        // A structure chest: its loot is decided by the world seed and the position.
        uint32_t seed = info_.seed ^ (uint32_t)(key.x * 73856093) ^ (uint32_t)(key.y * 19349663) ^ (uint32_t)(key.z * 83492791);
        chests_[key] = block != CHEST ? structureLoot(block, seed) : ChestContents{};
        // The Anniversary modpack hides collectible tokens in structure chests.
        if (block != CHEST && settings_.addonAnniversary && seed % 100 < 40)
            for (ItemStack& slot : chests_[key])
                if (slot.empty()) {
                    slot = {ANNIV_TOKEN, 1, 0};
                    break;
                }
    }
    if (block == LOOT_BARREL_CAMP) {
        w.setBlock(key.x, key.y, key.z, BARREL);
        achieve("loot_camp");
    }
    if (block != CHEST && isChest(block)) {
        w.setBlock(key.x, key.y, key.z, CHEST);
        achieve(block == LOOT_CHEST_SHIP ? "loot_ship" : block == LOOT_CHEST_DUNGEON ? "loot_dungeon"
                : block == LOOT_CHEST_MINE ? "loot_mine" : block == LOOT_CHEST_FORTRESS ? "loot_fortress"
                : block == LOOT_CHEST_VOID ? "loot_void" : block == LOOT_CHEST_RUIN ? "loot_ruin"
                : block == LOOT_CHEST_FORGOTTEN ? "loot_forgotten" : block == LOOT_CHEST_DROWNED ? "loot_drowned"
                : block == LOOT_CHEST_PORTAL ? "loot_portal" : "loot_explorer");
    }
    openChest = key;
    wantsChest = true;
}

void Session::uncoverFind(int x, int y, int z) {
    // The gravel is ordinary now, and what it hid pops out on top.
    World& w = *world_;
    w.setBlock(x, y, z, GRAVEL);
    const uint32_t seed = info_.seed ^ (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(z * 83492791);
    ItemStack find = archaeologyFind(seed);
    if (explorationMaps() && (seed >> 9) % 100 < 18) find = {EXPLORER_MAP, 1, 0};  // 1.5.7 experiment
    drops_.spawn(dimIndex(), Vec3(x + 0.5f, y + 1.1f, z + 0.5f), {0, 2.5f, 0}, find);
    particles_.spawnChips(w, x, y, z, 0, 1, 0, GRAVEL, 6);
    if (onMessage) onMessage(trf("Nález: {0}", {tr(itemName(find.id))}));
    if (survival()) damageHeldTool();
    hand_.use();
    sound(Sound::Coin);
    achieve("archaeologist");
}

bool Session::bookSigned(int id) const {
    auto it = books_.find(id);
    return it != books_.end() && !it->second.title.empty();
}

void Session::signBook(int id, const std::string& title, const std::string& author) {
    if (title.empty() || !books_.count(id)) return;
    books_[id].title = title.substr(0, 32), books_[id].author = author;
    for (ItemStack& s : state_.inventory.slots)
        if (s.id == WRITABLE_BOOK && s.damage == id) s.id = WRITTEN_BOOK;
}

void Session::readDocument(int page) {
    if (page < 0 || page >= kDocumentPages) return;
    wantsDocument = page;
    state_.documentsRead |= 1 << page;
    achieve("historian");
    if (state_.documentsRead == (1 << kDocumentPages) - 1) achieve("lore_master");
    hand_.use();
    sound(Sound::Page);  // 1.9.5: a page turns
}

bool Session::toggleDoor(int x, int y, int z) {
    World& w = *world_;
    ItemId id = w.getBlock(x, y, z);
    if (isTrapdoor(id)) {
        DoorState s = doorState(id);
        s.open = !s.open;
        w.setBlock(x, y, z, doorId(trapdoorBase(id), s));
        sound(id >= BRONZE_TRAPDOOR ? Sound::DoorIron : Sound::Door);
        return true;
    }
    if (!isDoor(id)) return false;
    const ItemId base = doorBase(id);
    DoorState s = doorState(id);
    const int lowerY = s.upper ? y - 1 : y;
    s.open = !s.open;
    // Both halves swing together.
    for (int half = 0; half < 2; ++half) {
        ItemId part = w.getBlock(x, lowerY + half, z);
        if (!isDoor(part)) continue;
        DoorState ps = doorState(part);
        ps.open = s.open;
        w.setBlock(x, lowerY + half, z, doorId(base, ps));
    }
    sound(base == OAK_DOOR ? Sound::Door : Sound::DoorIron);
    return true;
}

bool Session::placeSpecial(const RayHit& hit, ItemStack& held, int x, int y, int z) {
    World& w = *world_;
    // The side the player looks towards, snapped to north/east/south/west.
    Vec3 f = player_.forward();
    int facing = std::fabs(f.x) > std::fabs(f.z) ? (f.x > 0 ? 1 : 3) : (f.z > 0 ? 2 : 0);
    if (held.id == OAK_DOOR_ITEM || held.id == IRON_DOOR_ITEM || held.id == BRONZE_DOOR_ITEM) {
        // A door needs two free blocks on solid ground; it closes on the far side of the cell.
        ItemId up = w.getBlock(x, y + 1, z);
        if (!(up == AIR || up == TALL_GRASS) || !isOpaque(w.getBlock(x, y - 1, z))) return true;
        if (player_.intersectsBlock(x, y, z) || player_.intersectsBlock(x, y + 1, z)) return true;
        ItemId base = held.id == OAK_DOOR_ITEM ? (ItemId)OAK_DOOR : held.id == BRONZE_DOOR_ITEM ? (ItemId)BRONZE_DOOR : (ItemId)IRON_DOOR;
        w.setBlock(x, y, z, doorId(base, {facing, false, false}));
        w.setBlock(x, y + 1, z, doorId(base, {facing, false, true}));
        achieve("door");
    } else if (isTrapdoor(held.id)) {
        // On the underside of a block it hangs at the top; it swings up on the near side.
        bool top = hit.ny == -1;
        w.setBlock(x, y, z, doorId(trapdoorBase(held.id), {(facing + 2) % 4, false, top}));
    } else if (held.id == BED_ITEM) {
        // The foot goes where the player taps, the head one block further in the looking direction.
        static const int kDir[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        const int hx = x + kDir[facing][0], hz = z + kDir[facing][1];
        ItemId there = w.getBlock(hx, y, hz);
        if (!(there == AIR || there == TALL_GRASS) || !isSolid(w.getBlock(x, y - 1, z)) || !isSolid(w.getBlock(hx, y - 1, hz)))
            return true;
        if (player_.intersectsBlock(hx, y, hz)) return true;
        w.setBlock(x, y, z, (ItemId)(BED + facing * 2));
        w.setBlock(hx, y, hz, (ItemId)(BED + facing * 2 + 1));
    } else if (isSkeletonHead(held.id)) {
        // The face looks back at the player.
        if (player_.intersectsBlock(x, y, z)) return true;
        w.setBlock(x, y, z, (ItemId)(SKELETON_HEAD + facing));
    } else if (held.id == TORCH) {
        // 1.5.7: on top of a block it stands; on a side it hangs on that wall; never under a ceiling.
        const ItemId support = w.getBlock(hit.x, hit.y, hit.z);
        if (!isOpaque(support) || hit.ny < 0) return true;
        if (hit.ny > 0) {
            w.setBlock(x, y, z, TORCH);
        } else {
            const int wall = hit.nz > 0 ? 0 : hit.nx < 0 ? 1 : hit.nz < 0 ? 2 : 3;  // where the wall is
            w.setBlock(x, y, z, (ItemId)(TORCH_WALL + wall));
        }
        achieve("torch");
    } else if (held.id == CACTUS) {
        ItemId below = w.getBlock(x, y - 1, z);
        if (below != SAND && below != CACTUS) return true;
        w.setBlock(x, y, z, CACTUS);
    } else {
        return false;
    }
    if (survival()) state_.inventory.removeOne(state_.selectedSlot);
    hand_.place();
    styledSound(soundMaterial(w.getBlock(x, y, z)) == SoundMat::Stone ? Sound::PlaceStone : Sound::Place, w.getBlock(x, y, z));  // 2.0.0: by the block
    return true;
}

void Session::placeShape(const RayHit& hit, ItemStack& held, int x, int y, int z) {
    World& w = *world_;
    // The upper half when the tap lands on a ceiling or high on a wall.
    const bool upper = hit.ny == -1 || (hit.ny == 0 && hit.fy > 0.5f);
    ItemId place = AIR;
    if (isSlab(held.id)) {
        const int m = slabMaterial(held.id);
        const ItemId bottom = slabId(m, 0), top = slabId(m, 1);
        const ItemId target = w.getBlock(hit.x, hit.y, hit.z);
        // A second slab of the same kind on a half one makes a full double slab.
        if ((target == bottom && (hit.ny == 1 || hit.faceY == 1)) || (target == top && (hit.ny == -1 || hit.faceY == -1))) {
            x = hit.x, y = hit.y, z = hit.z;
            place = slabId(m, 2);
        } else if (ItemId there = w.getBlock(x, y, z); there == bottom || there == top) {
            place = slabId(m, 2);
        } else {
            place = upper ? top : bottom;
        }
    } else {
        // The tall back side faces the way the player looks, so they walk up it.
        Vec3 f = player_.forward();
        const int facing = std::fabs(f.x) > std::fabs(f.z) ? (f.x > 0 ? 1 : 3) : (f.z > 0 ? 2 : 0);
        place = stairsId(stairsMaterial(held.id), facing, upper);
    }
    const ItemId existing = w.getBlock(x, y, z);
    const bool merging = isDoubleSlab(place);
    if (!merging && existing != AIR && existing != TALL_GRASS && !isLiquid(existing)) return;
    if (player_.intersectsBlock(x, y, z)) return;
    if (!w.setBlock(x, y, z, place)) return;
    if (survival()) state_.inventory.removeOne(state_.selectedSlot);
    hand_.place();
    styledSound(soundMaterial(place) == SoundMat::Stone ? Sound::PlaceStone : Sound::Place, place);  // 2.0.0: by the block
}

void Session::giveItem(const ItemStack& stack) {
    if (stack.empty()) return;
    int left = state_.inventory.add(stack);
    if (left > 0) {
        ItemStack rest = stack;
        rest.count = (uint8_t)left;
        drops_.spawn(dimIndex(), player_.pos + Vec3(0, 0.6f, 0), {0, 2, 0}, rest);
    }
}

void Session::throwItem(const ItemStack& stack) {
    if (stack.empty()) return;
    Vec3 f = player_.forward();
    drops_.spawn(dimIndex(), player_.eye() - Vec3(0, 0.3f, 0) + f * 0.6f, f * 5 + Vec3(0, 1.5f, 0), stack);
}

void Session::openFurnaceAt(const FurnaceKey& key) {
    openFurnace = key;
    furnaces_[key];  // creates an empty one the first time
    wantsFurnace = true;
}

void Session::updateFurnaces(float dt) {
    // Furnaces keep smelting while their window is closed.
    if (settings_.expFastFurnace) dt *= 4;  // experiment
    for (auto& [k, f] : furnaces_) {
        World& w = worldAt(k.dim);
        const ItemId here = w.getBlock(k.x, k.y, k.z);
        const bool ps = isPowerstoneFurnace(here);
        if (ps && !f.burning() && f.canSmelt()) {
            // 1.5.7: wired to power (a live cable, a switch or a powerstone block) it needs no fuel.
            bool powered = false;
            const int d[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            for (auto& o : d) {
                const ItemId b = w.getBlock(k.x + o[0], k.y + o[1], k.z + o[2]);
                powered |= cableOn(b) || switchOn(b) || b == POWERSTONE_BLOCK;
            }
            if (powered) f.burnLeft = f.burnTotal = 4;
        }
        if (!f.update(ps ? dt * 3 : dt)) continue;
        if (ps) w.setBlock(k.x, k.y, k.z, f.burning() ? POWERSTONE_FURNACE_ON : POWERSTONE_FURNACE);
        else if (isFurnace(here)) w.setBlock(k.x, k.y, k.z, f.burning() ? LIT_FURNACE : FURNACE);
    }
}

void Session::damageHeldTool() {
    ItemStack& held = state_.inventory.slots[state_.selectedSlot];
    int durability = toolDurability(held.id);
    if (held.empty() || durability == 0) return;
    if (++held.damage >= durability) {
        held = {};  // the tool breaks
        sound(Sound::ToolBreak);
    }
}

void Session::healAndFeed() {
    state_.health = kMaxHealth;
    state_.food = kMaxFood;
    state_.saturation = 5;
    state_.air = kMaxAir;
}

void Session::teleportToSpawn() {
    world_ = &overworld_;
    overworld_.ensureChunksAround(floorDiv((int)state_.spawn.x, kChunkSize), floorDiv((int)state_.spawn.z, kChunkSize), 1, 9);
    player_.pos = state_.spawn;
    player_.vel = {};
    player_.fallDistance = 0;
}

void Session::sleepInBed(int x, int y, int z) {
    auto say = [&](const char* text) {
        if (onMessage) onMessage(tr(text));
    };
    if (world_ != &overworld_) {
        // Like the classic game: a bed in the Nether or the Crimson Void blows up.
        world_->setBlock(x, y, z, AIR);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz)
                if (isBed(world_->getBlock(x + dx, y, z + dz))) world_->setBlock(x + dx, y, z + dz, AIR);
        explode(Vec3(x + 0.5f, y + 0.5f, z + 0.5f), 4.5f, false);
        say("Postel tady vybuchla!");
        return;
    }
    // Waking up here from now on.
    Vec3 spawn(x + 0.5f, y + 0.6f, z + 0.5f);
    bool newSpawn = (state_.spawn - spawn).length() > 0.5f;
    state_.spawn = spawn;
    const float t = (float)std::fmod(state_.dayTime, 24000.0);
    if (!(isNight(t) || t >= 12500)) {
        say(newSpawn ? "Místo oživení nastaveno. Spát můžeš jen v noci." : "Spát můžeš jen v noci");
        return;
    }
    if (monstersNear(8)) {
        say("Nemůžeš spát, poblíž jsou příšery");
        return;
    }
    sleepTimer_ = 0.001f;
    player_.vel = {};
    achieve("sleep");
}

bool Session::monstersNear(float radius) const { return mobs_.hostileNear(dimIndex(), player_.pos, radius); }

// ---------------------------------------------------------------- 1.2.0: TNT, buckets, trees, power

void Session::igniteTnt(int x, int y, int z, float fuse) {
    tnt_.push_back({dimIndex(), Vec3(x + 0.5f, (float)y, z + 0.5f), {0, 2.5f, 0}, fuse, false});
    sound(Sound::Fuse);
}

void Session::explode(const Vec3& c, float radius, bool nuke) {
    World& w = *world_;
    const int R = (int)std::ceil(radius);
    const int cx = (int)std::floor(c.x), cy = (int)std::floor(c.y), cz = (int)std::floor(c.z);
    int bursts = 0, dropped = 0;
    std::vector<std::array<int, 3>> chain;
    for (int dx = -R; dx <= R; ++dx)
        for (int dy = -R; dy <= R; ++dy)
            for (int dz = -R; dz <= R; ++dz) {
                // A ragged sphere, like the classic blast.
                float d2 = (float)(dx * dx + dy * dy + dz * dz);
                float edge = radius * radius * (0.75f + 0.25f * ((hash3(cx + dx, cy + dy, cz + dz, 0xB00Bu) % 100) / 100.0f));
                if (d2 > edge) continue;
                const int x = cx + dx, y = cy + dy, z = cz + dz;
                const ItemId id = w.getBlock(x, y, z);
                if (id == AIR || id == WORLD_END || id == BEDROCK || isLiquid(id) || isPortal(id) || isChest(id)) continue;
                if (id == OBSIDIAN && !nuke) continue;
                if (isTnt(id)) {
                    chain.push_back({x, y, z});
                    w.setBlock(x, y, z, AIR);
                    continue;
                }
                if (bursts < 24 && (hash3(x, y, z, 7) % 6) == 0) {
                    particles_.spawnBurst(w, x, y, z, id);
                    ++bursts;
                }
                w.setBlock(x, y, z, AIR);
                if (survival() && dropped < 30 && hash3(x, y, z, 11) % (uint32_t)std::max(3, R) == 0) {
                    ItemId drop = dropFor(id, (hash3(x, z, y, 3) % 100) / 100.0f, IRON_PICKAXE);
                    if (drop != AIR) drops_.spawn(dimIndex(), Vec3(x + 0.5f, y + 0.5f, z + 0.5f), {0, 3, 0}, {drop, 1, 0});
                    ++dropped;
                }
            }
    for (auto& t : chain) igniteTnt(t[0], t[1], t[2], 0.4f + (hash3(t[0], t[1], t[2], 5) % 100) / 100.0f);
    // Everyone nearby gets hurt and thrown.
    const float reach = radius * 2;
    Vec3 to = player_.pos + Vec3(0, 0.9f, 0) - c;
    float d = to.length();
    if (d < reach) {
        float power = 1 - d / reach;
        hurt(power * (nuke ? 60.0f : 20.0f), true, true);
        player_.vel = player_.vel + to.normalized() * (power * 14) + Vec3(0, power * 6, 0);
    }
    for (size_t i = 0; i < mobs_.mobs().size(); ++i) {
        Mob& m = mobs_.mobs()[i];
        if (m.dim != dimIndex()) continue;
        float md = (m.pos - c).length();
        if (md < reach) mobs_.hurt((int)i, (1 - md / reach) * (nuke ? 80.0f : 25.0f), c);
    }
    sound(Sound::Explode);
    achieve("boom");
    if (settings_.vibrate && !settings_.calmMode && onVibrate) onVibrate(nuke ? 600 : 180);
}

bool Session::useBucket(ItemStack& held) {
    World& w = *world_;
    // Water is invisible to the normal crosshair ray, so march along the view.
    const Vec3 eye = camera_.eye(), dir = camera_.forward();
    if (held.id == BUCKET) {
        for (float t = 0.5f; t < Player::kReach + cameraDistance_; t += 0.1f) {
            Vec3 p = eye + dir * t;
            int x = (int)std::floor(p.x), y = (int)std::floor(p.y), z = (int)std::floor(p.z);
            ItemId b = w.getBlock(x, y, z);
            if (b == WATER || b == LAVA) {
                w.setBlock(x, y, z, AIR);
                ItemId full = b == WATER ? WATER_BUCKET : LAVA_BUCKET;
                if (!survival()) {
                    giveItem({full, 1, 0});
                } else if (held.count > 1) {
                    --held.count;
                    giveItem({full, 1, 0});
                } else {
                    held = {full, 1, 0};
                }
                hand_.use();
                sound(Sound::Splash);
                return true;
            }
            if (isSolid(b)) break;
        }
        return false;
    }
    RayHit hit = raycast(w, eye, dir, Player::kReach + cameraDistance_);
    if (!hit.hit) return false;
    int x = hit.x + hit.nx, y = hit.y + hit.ny, z = hit.z + hit.nz;
    ItemId there = w.getBlock(x, y, z);
    if (!(there == AIR || isPlant(there))) return false;
    if (held.id == WATER_BUCKET && world_ == &nether_) {
        sound(Sound::Fuse);  // it boils away
        if (onMessage) onMessage(tr("Voda se v dimenzi úhelného kamene vypařila"));
    } else {
        w.setBlock(x, y, z, held.id == WATER_BUCKET ? WATER : LAVA);
    }
    if (survival()) held = {BUCKET, 1, 0};
    hand_.place();
    sound(Sound::Splash);
    return true;
}

void Session::scheduleLeafDecay(int x, int y, int z) {
    World& w = *world_;
    for (int dx = -5; dx <= 5; ++dx)
        for (int dy = -5; dy <= 6; ++dy)
            for (int dz = -5; dz <= 5; ++dz) {
                if (!isLeaves(w.getBlock(x + dx, y + dy, z + dz))) continue;
                if (leafSupported(w, x + dx, y + dy, z + dz)) continue;
                float delay = 0.5f + (hash3(x + dx, y + dy, z + dz, 0x1EAFu) % 550) / 100.0f;
                fallingLeaves_.push_back({dimIndex(), x + dx, y + dy, z + dz, delay});
            }
}

namespace {
const Sound kSoundBlockList[] = {Sound::Click, Sound::Coin, Sound::Door, Sound::Chest, Sound::Glass, Sound::Splash,
                                 Sound::Thunder, Sound::Explode, Sound::Grunt, Sound::Confirm};
const char* const kSoundBlockNames[] = {"Cvaknutí", "Mince", "Dveře", "Truhla", "Sklo", "Šplouchnutí", "Hrom", "Výbuch", "Zabručení", "Zvonek"};
}  // namespace

const char* Session::soundBlockName(int i) { return kSoundBlockNames[((i % kSoundBlockSounds) + kSoundBlockSounds) % kSoundBlockSounds]; }

void Session::playSoundBlock(const FurnaceKey& key) {
    const int i = std::atoi(blockText(key).c_str());
    sound(kSoundBlockList[((i % kSoundBlockSounds) + kSoundBlockSounds) % kSoundBlockSounds]);
    // A little note rises from the block.
    for (int i = 0; i < 3; ++i)
        particles_.spawnAmbient(*world_, Vec3(key.x + 0.3f + 0.2f * i, key.y + 1.1f, key.z + 0.5f), Vec3(0, 1.2f, 0), SOUND_BLOCK, 0.8f, 0.18f, 0, 1.2f);
}

void Session::playRadio(const FurnaceKey& key) {
    const std::string text = blockText(key);
    if (text.empty()) {
        if (onMessage) onMessage(tr("Rozhlas zatím nemá text. Klepni na něj a napiš ho."));
        return;
    }
    for (int i = 0; i < 4; ++i)
        particles_.spawnAmbient(*world_, Vec3(key.x + 0.2f + 0.2f * i, key.y + 1.1f, key.z + 0.5f), Vec3(0, 1.4f, 0), SOUND_BLOCK, 0.8f, 0.18f, 0, 1.4f);
    if (onRadio) onRadio(text);
}

// 2.3.0: the flexible switch takes its shape from the track around it: its points face the rail behind the
// player (or the end of track that points at the cell), the straight goes on opposite, and the 45-degree
// branch leans to the side where a rail waits (else to the right).
ItemId Session::shapeSwitch(int x, int y, int z, int look) {
    static constexpr int px[8] = {0, 1, 1, 1, 0, -1, -1, -1}, pz[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    World& w = *world_;
    auto pointsAt = [&](int p) {
        const ItemId id = w.getBlock(x + px[p], y, z + pz[p]);
        int a, b, out;
        if (TrainSystem::railSides(id, a, b)) return a == ((p + 4) & 7) || b == ((p + 4) & 7);
        return isRail(id) && TrainSystem::nextSide(id, (p + 4) & 7, out);
    };
    auto apart = [](int u, int v) { const int d = (u - v) & 7; return std::min(d, 8 - d); };
    const int behind = (look * 2 + 4) & 7;
    int entry = behind, best = 9;
    for (int p = 0; p < 8; ++p)
        if (pointsAt(p) && apart(p, behind) < best) best = apart(p, behind), entry = p;
    const int right = (entry + 5) & 7, left = (entry + 3) & 7;
    const bool toLeft = isRail(w.getBlock(x + px[left], y, z + pz[left])) && !isRail(w.getBlock(x + px[right], y, z + pz[right]));
    return railSwitch8Id(entry, toLeft, 0);
}

// Build 12: rails in eight directions. A new rail joins the open ends that point at its cell (sides and
// corners); when fewer than two do, a neighbouring rail with a free end turns towards it (a straight
// piece, a diagonal, a quarter curve or a 45-degree bend), the way track is laid piece by piece.
ItemId Session::shapeRail(int x, int y, int z, int look) {
    static constexpr int px[8] = {0, 1, 1, 1, 0, -1, -1, -1}, pz[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    World& w = *world_;
    auto plain = [&](int cx, int cz, int& a, int& b) {
        const ItemId id = w.getBlock(cx, y, cz);
        return !isRailSlope(id) && !isTurntable(id) && TrainSystem::railSides(id, a, b);
    };
    auto pointsAt = [&](int cx, int cz, int port) {  // does the rail at (cx, cz) have an end at its port `port`?
        const ItemId id = w.getBlock(cx, y, cz);
        int a, b, out;
        if (TrainSystem::railSides(id, a, b)) return a == port || b == port;
        return (isRailSwitch(id) || isRailSwitch8(id) || isBufferStop(id)) && TrainSystem::nextSide(id, port, out);
    };
    // 2.3.0: a flexible switch beside the new rail whose branch leads nowhere turns its branch to the
    // other side when that side is this cell.
    for (int p = 0; p < 8; ++p) {
        const int nx = x + px[p], nz = z + pz[p];
        const ItemId sw = w.getBlock(nx, y, nz);
        if (!isRailSwitch8(sw)) continue;
        const int e = railSwitch8Entry(sw), branch = railSwitch8Branch(sw), other = (e + 4 + (railSwitch8Left(sw) ? 1 : -1)) & 7;
        if (other != ((p + 4) & 7) || isRail(w.getBlock(nx + px[branch], y, nz + pz[branch]))) continue;
        w.setBlock(nx, y, nz, railSwitch8Id(e, !railSwitch8Left(sw), railSwitch8State(sw)));
    }
    std::vector<int> fixed;
    for (int p : {0, 2, 4, 6, 1, 3, 5, 7})
        if (fixed.size() < 2 && pointsAt(x + px[p], z + pz[p], (p + 4) & 7)) fixed.push_back(p);
    for (int p : {0, 2, 4, 6, 1, 3, 5, 7}) {
        if (fixed.size() >= 2) break;
        if (std::find(fixed.begin(), fixed.end(), p) != fixed.end()) continue;
        const int nx = x + px[p], nz = z + pz[p], back = (p + 4) & 7;
        // Only a neighbour ahead of or behind the player (within 45 degrees of the way they look) turns
        // towards the new rail: a track laid beside another one stays straight.
        const int lookPort = look * 2;
        auto apart = [](int u, int v) { const int d = (u - v) & 7; return std::min(d, 8 - d); };
        if (std::min(apart(p, lookPort), apart(p, (lookPort + 4) & 7)) > 1) continue;
        int a, b;
        if (!plain(nx, nz, a, b)) continue;
        // Keep the neighbour's end that is joined to the track (or the one farther from us), turn the other to us.
        const bool aJoined = pointsAt(nx + px[a], nz + pz[a], (a + 4) & 7), bJoined = pointsAt(nx + px[b], nz + pz[b], (b + 4) & 7);
        if (aJoined && bJoined) continue;
        auto away = [&](int q) { const int d = (q - back) & 7; return std::min(d, 8 - d); };
        // A lone rail (neither end joined) simply turns to run straight through to the new one.
        const int keep = aJoined ? a : bJoined ? b : (back + 4) & 7;
        const ItemId turned = TrainSystem::railFor(keep, back);
        if (turned == AIR) continue;
        if (!fixed.empty() && TrainSystem::railFor(fixed[0], p) == AIR) continue;
        w.setBlock(nx, y, nz, turned);
        fixed.push_back(p);
    }
    if (fixed.size() >= 2) {
        const ItemId both = TrainSystem::railFor(fixed[0], fixed[1]);
        if (both != AIR) return both;
    }
    if (!fixed.empty()) return TrainSystem::railFor(fixed[0], (fixed[0] + 4) & 7);
    return (look == 1 || look == 3) ? RAIL_EW : RAIL_NS;
}

// VoxelCraft Classic 0.0.3: the legend of the Double. Someone with the player's own skin and empty white
// eyes stands far off in the haze, watching. Come near or stare back and he is gone. Now and then he runs
// at the player instead, strikes once and vanishes; and he leaves things in the world: a little sand
// pyramid, a tree stripped of its leaves, a square tunnel into a hill.
void Session::updateDoppel(float dt) {
    if (world_ != &overworld_ || dead_) {
        doppel_.active = false;
        return;
    }
    World& w = *world_;
    auto groundAt = [&](int x, int z) { return w.skyHeight(x, z); };
    if (!doppel_.active) {
        if ((doppel_.wait -= dt) > 0) return;
        doppel_.wait = 70 + fireRandom() * 140;
        const float roll = fireRandom();
        if (roll < 0.25f) {
            // A sign of him: built out of sight, behind the player.
            const float a = player_.yaw + kPi + (fireRandom() - 0.5f) * 1.5f;
            const int bx = (int)std::floor(player_.pos.x + std::sin(a) * -22), bz = (int)std::floor(player_.pos.z + std::cos(a) * 22);
            const int gy = groundAt(bx, bz);
            if (gy <= 2 || !w.getChunk(floorDiv(bx, 16), floorDiv(bz, 16))) return;
            const int kind = (int)(fireRandom() * 3);
            if (kind == 0) {
                for (int l = 0; l < 3; ++l)
                    for (int dx = -2 + l; dx <= 2 - l; ++dx)
                        for (int dz = -2 + l; dz <= 2 - l; ++dz) w.setBlock(bx + dx, gy + l, bz + dz, SAND);
            } else if (kind == 1) {
                for (int dx = -6; dx <= 6; ++dx)
                    for (int dz = -6; dz <= 6; ++dz)
                        for (int y = gy - 2; y < gy + 10; ++y)
                            if (w.getBlock(bx + dx, y, bz + dz) == LEAVES) w.setBlock(bx + dx, y, bz + dz, AIR);
            } else {
                for (int k = 0; k < 6; ++k)
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dz = 0; dz < 2; ++dz)
                            if (w.getBlock(bx + k, gy - 3 + dy, bz + dz) != BEDROCK) w.setBlock(bx + k, gy - 3 + dy, bz + dz, AIR);
            }
            return;
        }
        // He appears at the edge of sight, ahead of the player, in the haze.
        const float a = player_.yaw + (fireRandom() - 0.5f) * 1.2f, dist = 26 + fireRandom() * 12;
        const float fx = player_.pos.x - std::sin(a) * dist, fz = player_.pos.z - std::cos(a) * dist;
        const int gy = groundAt((int)std::floor(fx), (int)std::floor(fz));
        if (gy <= 2) return;
        doppel_.active = true;
        doppel_.pos = Vec3(std::floor(fx) + 0.5f, (float)gy, std::floor(fz) + 0.5f);
        doppel_.time = 0;
        doppel_.attack = survival() && roll > 0.8f;
        return;
    }
    doppel_.time += dt;
    Vec3 to = player_.pos - doppel_.pos;
    const float d = std::sqrt(to.x * to.x + to.z * to.z);
    doppel_.yaw = std::atan2(-to.x, -to.z);
    auto vanish = [&] {
        for (int i = 0; i < 3; ++i)
            particles_.spawnBurst(*world_, (int)std::floor(doppel_.pos.x), (int)std::floor(doppel_.pos.y) + i % 2, (int)std::floor(doppel_.pos.z), OBSIDIAN);
        doppel_.active = false;
        doppel_.wait = 90 + fireRandom() * 150;
    };
    if (doppel_.attack) {
        // He runs at the player, strikes once, and is gone.
        if (d > 0.01f) {
            const float step = std::min(d, 6.5f * dt);
            doppel_.pos.x += to.x / d * step, doppel_.pos.z += to.z / d * step;
            doppel_.pos.y = (float)groundAt((int)std::floor(doppel_.pos.x), (int)std::floor(doppel_.pos.z));
        }
        if (d < 1.6f) {
            hurt(4, true, true);
            vanish();
        } else if (doppel_.time > 12) {
            vanish();
        }
        return;
    }
    // Watching: gone when the player comes close, looks straight at him for a while, or after a time.
    const Vec3 look = camera_.forward();
    const float facing = d > 0.01f ? -(look.x * to.x + look.z * to.z) / d : 0;
    doppel_.stare = facing > 0.97f ? doppel_.stare + dt : 0;
    if (d < 14 || doppel_.stare > 1.3f || doppel_.time > 20) vanish();
}

PlayerPose Session::doppelPose() const {
    PlayerPose p;
    p.feet = doppel_.pos;
    p.yaw = doppel_.yaw;
    p.swing = doppel_.attack ? std::sin(doppel_.time * 12) * 0.8f : 0;
    return p;
}

void Session::powerChanged(int x, int y, int z) {
    std::vector<std::array<int, 3>> ignite;
    updatePower(*world_, x, y, z, ignite);
    // 1.9.5 build 3: command blocks run their command when power reaches them (once per switch-on).
    for (int dx = -2; dx <= 2; ++dx)
        for (int dy = -2; dy <= 2; ++dy)
            for (int dz = -2; dz <= 2; ++dz) {
                const int cx = x + dx, cy = y + dy, cz = z + dz;
                const ItemId here = world_->getBlock(cx, cy, cz);
                if (here != COMMAND_BLOCK && here != SOUND_BLOCK && here != RADIO) continue;
                bool powered = false;
                static const int k6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                for (const auto& o : k6) {
                    const ItemId n = world_->getBlock(cx + o[0], cy + o[1], cz + o[2]);
                    powered |= cableOn(n) || switchOn(n) || n == POWERSTONE_BLOCK;
                }
                const FurnaceKey key{dimIndex(), cx, cy, cz};
                if (powered && poweredCommands_.insert(key).second) {
                    if (here == SOUND_BLOCK) playSoundBlock(key);  // 2.0.0
                    else if (here == RADIO) playRadio(key);    // 2.2.0
                    else runCommandBlock(key);
                }
                if (!powered) poweredCommands_.erase(key);
            }
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz)
                if (world_->getBlock(x + dx, y + dy, z + dz) == POWERSTONE_LAMP_ON) achieve("electrician");
    for (auto& t : ignite) {
        ItemId b = world_->getBlock(t[0], t[1], t[2]);
        if (!isTnt(b)) continue;
        world_->setBlock(t[0], t[1], t[2], AIR);
        igniteTnt(t[0], t[1], t[2], b == NUKE_TNT ? 8.0f : 4.0f);
        tnt_.back().nuke = b == NUKE_TNT;
    }
}

void Session::updateBlocksOverTime(float dt) {
    updateBlockPhysics(dt);
    // Lit TNT: falls if nothing holds it, then goes off.
    for (size_t i = 0; i < tnt_.size(); ++i) {
        PrimedTnt& t = tnt_[i];
        if (t.dim != dimIndex()) continue;
        World& w = *world_;
        t.vel.y -= 20 * dt;
        Vec3 next = t.pos + t.vel * dt;
        if (isSolid(w.getBlock((int)std::floor(next.x), (int)std::floor(next.y), (int)std::floor(next.z)))) {
            t.vel = {};
            next = t.pos;
        }
        t.pos = next;
        if ((t.fuse -= dt) <= 0) {
            PrimedTnt done = t;
            tnt_.erase(tnt_.begin() + (long)i);
            explode(done.pos + Vec3(0, 0.5f, 0), (done.nuke ? 12.0f : 4.0f) * (settings_.expBigBoom ? 2.0f : 1.0f), done.nuke);
            --i;
        }
    }
    // 1.9.0: falling blocks speed up, then land on the first block that holds them.
    for (size_t i = 0; i < fallingBlocks_.size(); ++i) {
        FallingBlock& f = fallingBlocks_[i];
        World& w = worldAt(f.dim);
        f.vel = std::min(f.vel + 22 * dt, 30.0f);
        const float next = f.pos.y - f.vel * dt;
        const int x = (int)std::floor(f.pos.x), z = (int)std::floor(f.pos.z);
        const int cell = (int)std::floor(next);
        const ItemId below = cell >= 0 ? w.getBlock(x, cell, z) : (ItemId)BEDROCK;
        if (cell < 0 || (!washedAway(below) && !isLiquid(below))) {
            const int y = std::max(0, cell + 1);
            if (y < kChunkHeight) w.setBlock(x, y, z, f.id);
            fallingBlocks_[i] = fallingBlocks_.back();
            fallingBlocks_.pop_back();
            --i;
            continue;
        }
        f.pos.y = next;
    }
    // Saplings growing into trees.
    for (size_t i = 0; i < saplings_.size(); ++i) {
        Timed& s = saplings_[i];
        if ((s.time -= dt) > 0) continue;
        World& w = worldAt(s.dim);
        ItemId b = w.getBlock(s.x, s.y, s.z);
        bool loaded = w.getChunk(floorDiv(s.x, kChunkSize), floorDiv(s.z, kChunkSize)) != nullptr;
        if (!loaded) {
            s.time = 5;  // try again when the player is back
            continue;
        }
        if (isCrop(b) && cropStage(b) < 3) {
            // One stage every 12 seconds (3 with fast growth), on farmland only.
            if (w.getBlock(s.x, s.y - 1, s.z) == FARMLAND) {
                w.setBlock(s.x, s.y, s.z, cropBlock(cropStage(b) + 1));
                if (cropStage(b) + 1 < 3) {
                    s.time = settings_.expFastGrowth ? 3.0f : 12.0f;
                    continue;
                }
            }
        }
        if (isSapling(b)) {
            if (!growTree(w, s.x, s.y, s.z, treeForSapling(b), hash3(s.x, s.y, s.z, info_.seed))) {
                s.time = 10;  // blocked: try again later
                continue;
            }
            achieve("gardener");
        }
        saplings_.erase(saplings_.begin() + (long)i);
        --i;
    }
    // Leaves without a trunk fall one by one.
    for (size_t i = 0; i < fallingLeaves_.size(); ++i) {
        Timed& l = fallingLeaves_[i];
        if ((l.time -= dt) > 0) continue;
        World& w = worldAt(l.dim);
        ItemId b = w.getBlock(l.x, l.y, l.z);
        if (isLeaves(b) && !leafSupported(w, l.x, l.y, l.z)) {
            w.setBlock(l.x, l.y, l.z, AIR);
            if (l.dim == dimIndex()) particles_.spawnBurst(w, l.x, l.y, l.z, b);
            ItemId drop = dropFor(b, (hash3(l.x, l.y, l.z, 0xD0Fu) % 1000) / 1000.0f, AIR);
            if (drop != AIR && survival()) drops_.spawn(l.dim, Vec3(l.x + 0.5f, l.y + 0.5f, l.z + 0.5f), {0, 1, 0}, {drop, 1, 0});
        }
        fallingLeaves_.erase(fallingLeaves_.begin() + (long)i);
        --i;
    }
}


void Session::updateBlockPhysics(float dt) {
    if (mobsFromHost) {  // a guest: the host moves the blocks and sends the results
        for (auto& c : changes_) c.clear();
        return;
    }
    const bool liquids = !oldMode();  // 1.9.0 build 3: water and lava flow in every world (no longer an experiment)
    // New changes wake the blocks around them.
    for (int d = 0; d < 4; ++d) {
        if (changes_[d].empty()) continue;
        World& w = worldAt(d);
        std::vector<std::array<int, 3>> list;
        list.swap(changes_[d]);
        static const int kAround[7][3] = {{0, 0, 0}, {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const auto& c : list)
            for (const auto& o : kAround) {
                const int x = c[0] + o[0], y = c[1] + o[1], z = c[2] + o[2];
                if (y < 0 || y >= kChunkHeight) continue;
                const ItemId b = w.getBlock(x, y, z);
                if (isConcretePowder(b)) {
                    // 1.7.2 build 3: concrete powder that touches water sets into hard concrete.
                    bool wet = false;
                    for (const auto& k : kAround) wet |= w.getBlock(x + k[0], y + k[1], z + k[2]) == WATER;
                    if (wet) {
                        w.setBlock(x, y, z, hardenedConcrete(b));
                        continue;
                    }
                }
                const bool liquid = liquids && (b == WATER || b == LAVA);
                if ((!fallsDown(b) || oldMode()) && !liquid && b != FIRE && !flow_[d].count(cellKey(x, y, z))) continue;  // 1.9.5: sand stays put in the Old World
                if (!ticking_[d].insert(cellKey(x, y, z)).second || blockTicks_.size() > 20000) continue;
                const float delay = b == FIRE ? 1.0f + fireRandom() : fallsDown(b) ? 0.05f : b == LAVA ? (d == 1 ? 0.5f : 1.0f) : 0.25f;
                blockTicks_.push_back({d, x, y, z, delay});
            }
    }
    std::vector<Timed> due;
    for (size_t i = 0; i < blockTicks_.size(); ++i)
        if ((blockTicks_[i].time -= dt) <= 0) {
            due.push_back(blockTicks_[i]);
            blockTicks_[i] = blockTicks_.back();
            blockTicks_.pop_back();
            --i;
        }
    for (const Timed& t : due) {
        ticking_[t.dim].erase(cellKey(t.x, t.y, t.z));
        World& w = worldAt(t.dim);
        const ItemId b = w.getBlock(t.x, t.y, t.z);
        if (fallsDown(b)) {
            // Sand and gravel drop one block at a time until something holds them.
            const ItemId below = t.y > 0 ? w.getBlock(t.x, t.y - 1, t.z) : (ItemId)BEDROCK;
            if (washedAway(below) || isLiquid(below)) {
                // 1.9.0: it leaves the grid and falls smoothly; it lands where something holds it.
                w.setBlock(t.x, t.y, t.z, AIR);
                fallingBlocks_.push_back({t.dim, Vec3(t.x + 0.5f, (float)t.y, t.z + 0.5f), 0, b});
            }
        } else if (b == FIRE) {
            tickFire(t.dim, t.x, t.y, t.z);
        } else if (liquids) {
            tickLiquid(t.dim, t.x, t.y, t.z);
        } else {
            flow_[t.dim].erase(cellKey(t.x, t.y, t.z));
        }
    }
}

// 1.9.5: temperature. The place sets what the body warms or cools towards: the biome's climate,
// the dimension, height, night, rain and snow, water; fire, campfires, lava and torches warm it up.
// The body follows slowly. Hot makes you hungry faster, cold slows you down, very cold hurts.
float Session::targetTemperature() const {
    const World& w = *world_;
    const int x = (int)std::floor(player_.pos.x), y = (int)std::floor(player_.pos.y), z = (int)std::floor(player_.pos.z);
    float t = 0.3f;
    if (world_ == &overworld_) {
        const BiomeDef* b = overworld_.biomeAt(x, z);
        if (b) t = (b->temperature - 0.5f) * 3.0f + 0.3f;
        if (b && (b->freeze || b->id == "ice_plains")) t -= 1.2f;
        if (b && b->id == "snowy_taiga") t -= 0.6f;
        if (y - overworld_.yShift() > 95) t -= (y - overworld_.yShift() - 95) / 25.0f;  // high peaks are cold
        if (dayLight_ < 0.5f) t -= 0.5f;
        if (state_.raining) t -= precipitation() == Precipitation::Snow ? 0.8f : 0.4f;
        if (snowySeason(settings_.season)) t -= 0.5f;
    } else if (world_ == &nether_) {
        t = 1.6f;
    } else if (world_ == &void_) {
        t = 0.2f;
    } else {
        t = -0.4f;  // the Celestial Reach: thin cold air up there
    }
    if (player_.inWater) t -= 1.0f;
    if (player_.inDriedSnow) t -= 2.5f;  // 2.2.0: buried in dried snow, you freeze fast
    if (player_.inLava) t += 3.0f;
    for (const auto& f : fireSpots_) {
        const float d = Vec3(f[0] + 0.5f - player_.pos.x, f[1] + 0.5f - player_.pos.y, f[2] + 0.5f - player_.pos.z).length();
        if (d < 4) t = std::max(t, std::min(t + 2.0f, 0.9f));  // the warmth of a fire: comfortable, not hot
    }
    if (isTorch(state_.inventory.slots[state_.selectedSlot].id) || state_.offhand.id == TORCH) t = std::max(t, std::min(t + 0.6f, 0.6f));
    float warm = -100;
    for (int dx = -2; dx <= 2; ++dx)
        for (int dz = -2; dz <= 2; ++dz)
            for (int dy = -1; dy <= 1; ++dy) {
                const ItemId b = w.getBlock(x + dx, y + dy, z + dz);
                if (b == LAVA) return std::max(t + 1.5f, 1.6f);
                // 2.2.0: a lit furnace, a lit bulb or a lamp next to you warms you up too.
                if (b == LIT_FURNACE || b == POWERSTONE_FURNACE_ON) warm = std::max(warm, 0.9f);
                else if (b == POWERSTONE_LIGHT || b == POWERSTONE_LAMP_ON || bronzeBulbLit(b) || b == AMBER_RESIN) warm = std::max(warm, 0.5f);
            }
    if (warm > -100) t = std::max(t, std::min(t + 1.5f, warm));
    return t;
}

int Session::temperatureLevel() const {
    return bodyTemp_ > 1.2f ? 1 : bodyTemp_ > -0.6f ? 0 : bodyTemp_ > -1.6f ? -1 : -2;  // hot, warm, cold, very cold
}

void Session::updateTemperature(float dt) {
    if (oldMode()) {
        bodyTemp_ = 0.3f;  // 1.9.5: no temperature in the Old World
        return;
    }
    bodyTemp_ += (targetTemperature() - bodyTemp_) * std::min(1.0f, dt * 0.08f);
    const int level = temperatureLevel();
    if (level == 1) addExhaustion(dt * 0.06f);  // hot: hungry sooner
    if (level == -2 && (freezeTimer_ += dt) >= 4) {
        freezeTimer_ = 0;
        if (state_.health > 2) hurt(1, true);  // very cold: freezing, but not to death
    } else if (level != -2) {
        freezeTimer_ = 0;
    }
}

void Session::updateAmbientParticles(float dt) {
    if (oldMode()) return;  // 1.9.5: no falling leaves, leaf litter, smoke or crackle in the Old World
    World& w = *world_;
    const int px = (int)std::floor(player_.pos.x), py = (int)std::floor(player_.pos.y), pz = (int)std::floor(player_.pos.z);
    if ((ambientScan_ -= dt) <= 0) {
        // Twice a second: where the fires and campfires near the player are.
        ambientScan_ = 0.5f;
        fireSpots_.clear();
        for (int dx = -8; dx <= 8; ++dx)
            for (int dz = -8; dz <= 8; ++dz)
                for (int dy = -4; dy <= 5 && fireSpots_.size() < 24; ++dy) {
                    const ItemId b = w.getBlock(px + dx, py + dy, pz + dz);
                    if (b == FIRE || b == CAMPFIRE) fireSpots_.push_back({px + dx, py + dy, pz + dz, b == CAMPFIRE});
                }
    }
    if (state_.inventory.slots[state_.selectedSlot].id == BARRIER && (barrierScan_ -= dt) <= 0) {
        // 1.9.5 build 3: with a barrier in hand, the barriers around show as red signs.
        barrierScan_ = 0.5f;
        for (int dx = -8; dx <= 8; ++dx)
            for (int dy = -5; dy <= 6; ++dy)
                for (int dz = -8; dz <= 8; ++dz)
                    if (w.getBlock(px + dx, py + dy, pz + dz) == BARRIER)
                        particles_.spawnAmbient(w, Vec3(px + dx + 0.5f, py + dy + 0.5f, pz + dz + 0.5f), Vec3(), BARRIER, 0.6f, 0.42f, 0, 1.4f, true);
    }
    if (!fireSpots_.empty() && (fireSound_ -= dt) <= 0) {
        // 1.9.5: the crackle of the nearest fire.
        fireSound_ = 2.5f + fireRandom() * 2;
        sound(Sound::Fire);
    }
    for (const auto& f : fireSpots_) {
        // Flames lick up; campfires send a column of smoke into the sky.
        if (fireRandom() < dt * 6)
            particles_.spawnAmbient(w, Vec3(f[0] + 0.3f + fireRandom() * 0.4f, f[1] + 0.3f, f[2] + 0.3f + fireRandom() * 0.4f),
                                    Vec3(0, 0.6f, 0), FIRE, 0.5f + fireRandom() * 0.3f, 0.07f, -0.05f, 1.4f);
        if (f[3] && fireRandom() < dt * 3)
            particles_.spawnAmbient(w, Vec3(f[0] + 0.4f + fireRandom() * 0.2f, f[1] + 0.9f, f[2] + 0.4f + fireRandom() * 0.2f),
                                    Vec3((fireRandom() - 0.5f) * 0.2f, 1.3f, (fireRandom() - 0.5f) * 0.2f), SNOW_BLOCK, 3.5f, 0.16f, -0.02f, 0.4f);
    }
    // 1.9.5: leaf litter rots away little by little (faster in the rain).
    for (int k = 0; k < 4; ++k) {
        const int x = px + (int)(fireRandom() * 32) - 16, z = pz + (int)(fireRandom() * 32) - 16, y = py + (int)(fireRandom() * 12) - 6;
        if (w.getBlock(x, y, z) == LEAF_LITTER && fireRandom() < dt * (state_.raining ? 6.0f : 2.0f)) w.setBlock(x, y, z, AIR);
    }
    if (!settings_.fallingLeaves) return;
    // A few leaves drift down from the crowns round the player.
    ambientLeaf_ += dt * 14;
    for (; ambientLeaf_ >= 1; ambientLeaf_ -= 1) {
        const int x = px + (int)(fireRandom() * 24) - 12, z = pz + (int)(fireRandom() * 24) - 12, y = py + (int)(fireRandom() * 14) - 2;
        const ItemId b = w.getBlock(x, y, z);
        if (!isLeaves(b) || w.getBlock(x, y - 1, z) != AIR) continue;
        particles_.spawnAmbient(w, Vec3(x + fireRandom(), (float)y - 0.05f, z + fireRandom()),
                                Vec3((fireRandom() - 0.5f) * 0.6f + wind_ * 1.2f, -0.3f, (fireRandom() - 0.5f) * 0.6f), b, 5.0f + fireRandom() * 2, 0.07f, 0.04f,
                                0.85f * dayLight_ + 0.15f);
        // 1.9.5: and some of them stay: brown leaf litter gathers on the ground under the trees (more
        // in autumn), in loose patches, never on top of other litter.
        if (world_ == &overworld_ && fireRandom() < (settings_.season == Season::Halloween ? 0.08f : 0.03f)) {
            const int lx = x + (int)std::round((fireRandom() - 0.5f) * 3), lz = z + (int)std::round((fireRandom() - 0.5f) * 3);
            int gy = y - 1;
            while (gy > 1 && w.getBlock(lx, gy, lz) == AIR) --gy;
            const ItemId g = w.getBlock(lx, gy, lz);
            if ((g == GRASS || g == DIRT || g == MOSS_BLOCK) && w.getBlock(lx, gy + 1, lz) == AIR) w.setBlock(lx, gy + 1, lz, LEAF_LITTER);
        }
    }
}

// 2.2.0 preview: a few random blocks round the player each second. Bronze that is not waxed ages
// (bright, tarnished, rusty), and dirt next to grass grows grass when nothing covers it.
void Session::updateBronzeAndGrass(float dt) {
    if ((bronzeTimer_ -= dt) > 0) return;
    bronzeTimer_ = 1.0f;
    World& w = *world_;
    const int px = (int)std::floor(player_.pos.x), py = (int)std::floor(player_.pos.y), pz = (int)std::floor(player_.pos.z);
    for (int k = 0; k < 600; ++k) {
        const int x = px + (int)(fireRandom() * 49) - 24, y = py + (int)(fireRandom() * 25) - 12, z = pz + (int)(fireRandom() * 49) - 24;
        if (y < 1 || y >= kChunkHeight - 1) continue;
        const ItemId b = w.getBlock(x, y, z);
        const int base = bronzeAgingBase(b);
        if (base >= 0) {
            // About an in-game day for each stage on average.
            if (!bronzeWaxed(b) && bronzeStage(b) < 2 && fireRandom() < 0.02f) w.setBlock(x, y, z, bronzeVariant(base, bronzeStage(b) + 1, false));
            continue;
        }
        if (b != DIRT || isOpaque(w.getBlock(x, y + 1, z)) || isLiquid(w.getBlock(x, y + 1, z))) continue;
        bool near = false;
        for (int dx = -1; dx <= 1 && !near; ++dx)
            for (int dz = -1; dz <= 1 && !near; ++dz)
                for (int dy = -1; dy <= 1 && !near; ++dy)
                    if ((dx || dz) && w.getBlock(x + dx, y + dy, z + dz) == GRASS) near = true;
        if (near && fireRandom() < 0.5f) w.setBlock(x, y, z, GRASS);
    }
}

void Session::updateExperiments20(float dt) {
    lightningFlash_ = std::max(0.0f, lightningFlash_ - dt * 3);
    if (world_ != &overworld_ || oldMode()) return;  // 1.9.5: nothing of this in the Old World
    World& w = *world_;
    if (thunderIn_ >= 0 && (thunderIn_ -= dt) < 0) sound(Sound::Thunder);  // thunder, late by the distance
    if ((storm_ || info_.expStorms) && state_.raining && precipitation() == Precipitation::Rain && (stormTimer_ -= dt) <= 0) {
        // 1.9.5: thunderstorms (the 2.0 preview of 1.9.0): lightning strikes the ground near the
        // player with a flash; the thunder follows a moment later the farther it struck, and dry
        // ground it hits catches fire.
        stormTimer_ = 5 + fireRandom() * 12;
        const int x = (int)std::floor(player_.pos.x + (fireRandom() - 0.5f) * 60), z = (int)std::floor(player_.pos.z + (fireRandom() - 0.5f) * 60);
        const int y = w.skyHeight(x, z);
        if (y > 1 && y < kChunkHeight - 2) {
            for (int k = 0; k < 40; ++k) {
                const float h = y + k * 1.2f;
                particles_.spawnAmbient(w, Vec3(x + 0.5f + std::sin(k * 1.7f) * 0.6f, h, z + 0.5f + std::cos(k * 2.3f) * 0.6f), Vec3(0, 0, 0), SNOW_BLOCK,
                                        0.35f, 0.35f, 0, 3.0f);
            }
            const float far = (Vec3(x + 0.5f, (float)y, z + 0.5f) - player_.pos).length();
            lightningFlash_ = std::clamp(1.2f - far / 60, 0.4f, 1.0f);
            thunderIn_ = far / 34.0f;  // about the speed of sound, scaled to the game
            if (w.getBlock(x, y, z) == AIR && isSolid(w.getBlock(x, y - 1, z)) && fireRandom() < 0.5f) w.setBlock(x, y, z, FIRE);
            if ((Vec3(x + 0.5f, (float)y, z + 0.5f) - player_.pos).length() < 2.5f) hurt(5, true, true);
        }
    }
    if (info_.expStars && dayLight_ < 0.35f && !state_.raining && (starTimer_ -= dt) <= 0) {
        // 2.0 preview: a shooting star streaks across the night sky.
        starTimer_ = 3 + fireRandom() * 9;
        const float a = fireRandom() * 2 * kPi;
        const Vec3 from = player_.pos + Vec3(std::cos(a) * 30, 45 + fireRandom() * 15, std::sin(a) * 30);
        const Vec3 dir = Vec3(-std::sin(a) - std::cos(a) * 0.3f, -0.25f, std::cos(a) - std::sin(a) * 0.3f).normalized();
        for (int k = 0; k < 14; ++k)
            particles_.spawnAmbient(w, from - dir * (k * 0.7f), dir * 26.0f, SNOW_BLOCK, 0.9f - k * 0.04f, 0.18f - k * 0.008f, 0, 2.4f);
    }
}

void Session::styledSound(Sound s, ItemId block) const {
    // The block's own style, or the dimension's (steps on ordinary blocks in another dimension).
    const char* style = blockSoundStyle(block);
    if (!*style) style = world_ == &nether_ ? "cs" : world_ == &void_ ? "void" : world_ == &celestial_ ? "sky" : "";
    soundStyle_ = style;
    sound(s);
    soundStyle_ = "";
}

std::string Session::blockText(const FurnaceKey& key) const {
    auto it = blockTexts_.find(key);
    return it == blockTexts_.end() ? std::string() : it->second;
}

void Session::runCommandBlock(const FurnaceKey& key) {
    const std::string cmd = blockText(key);
    if (cmd.empty()) return;
    for (const std::string& line : runCommand(*this, cmd[0] == '/' ? cmd : "/" + cmd, true))
        if (onMessage) onMessage(line);
}

std::string Session::setBlockText(const FurnaceKey& key, const std::string& text) {
    // The command of a command block, or the job of a structure block:
    //   save <name> <width> <height> <depth>   (the box starts on top of the block)
    //   load <name>
    blockTexts_[key] = text;
    World& w = worldAt(key.dim);
    if (w.getBlock(key.x, key.y, key.z) == RADIO) return trf("Rozhlas nastaven: {0}", {text});
    if (w.getBlock(key.x, key.y, key.z) == STATION_SIGN) return trf("Stanice pojmenována: {0}", {text});
    if (w.getBlock(key.x, key.y, key.z) != STRUCTURE_BLOCK) return trf("Příkaz uložen: {0}", {text});
    std::istringstream in(text);
    std::string mode, name;
    in >> mode >> name;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(info_.folder + "/structures", ec);
    const std::string path = info_.folder + "/structures/" + name + ".vcs";
    if (name.empty()) return tr("Napiš: save <název> <šířka> <výška> <hloubka>, nebo load <název>");
    if (mode == "save") {
        int sx = 0, sy = 0, sz = 0;
        in >> sx >> sy >> sz;
        if (sx < 1 || sy < 1 || sz < 1 || sx > 64 || sy > 64 || sz > 64) return tr("Rozměry 1 až 64 bloků");
        Structure st;
        std::string error;
        if (!captureStructure(w, key.x, key.y + 1, key.z, sx, sy, sz, st, error)) return error;
        st.name = name;
        const std::vector<uint8_t> data = writeVcStructure(st);
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        return trf("Struktura {0} uložena ({1} bloků)", {name, std::to_string(st.blockCount())});
    }
    if (mode == "load") {
        std::ifstream f(path, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        Structure st;
        std::string error;
        if (data.empty() || !readVcStructure(data, st, error)) return trf("Struktura {0} nenalezena", {name});
        std::vector<BlockChange> undo;
        if (!placeStructure(w, st, key.x, key.y + 1, key.z, undo, error)) return error;
        return trf("Struktura {0} postavena", {name});
    }
    return tr("Napiš: save <název> <šířka> <výška> <hloubka>, nebo load <název>");
}

void Session::saveBlockTexts() const {
    std::ofstream out(info_.folder + "/blocktexts.dat");
    out << "VCBT1\n";
    for (const auto& [k, t] : blockTexts_) out << k.dim << " " << k.x << " " << k.y << " " << k.z << " " << t << "\n";
}

void Session::loadBlockTexts() {
    std::ifstream in(info_.folder + "/blocktexts.dat");
    std::string line;
    if (!std::getline(in, line) || line != "VCBT1") return;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        FurnaceKey k;
        if (!(l >> k.dim >> k.x >> k.y >> k.z)) continue;
        std::string t;
        std::getline(l, t);
        if (!t.empty() && t[0] == ' ') t.erase(0, 1);
        blockTexts_[k] = t;
    }
}

float Session::fireRandom() {
    fireSeed_ = fireSeed_ * 1664525u + 1013904223u;
    return (fireSeed_ >> 8) / 16777216.0f;
}

// 1.9.0 build 2: a fire burns for a few seconds, catches the flammable blocks around it and burns
// them away; it goes out in the rain. On netherrack it burns for ever.
void Session::tickFire(int d, int x, int y, int z) {
    World& w = worldAt(d);
    if (w.getBlock(x, y, z) != FIRE) return;
    const ItemId below = y > 0 ? w.getBlock(x, y - 1, z) : (ItemId)AIR;
    static const int k6[6][3] = {{0, -1, 0}, {0, 1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    bool fuel = false;
    for (const auto& o : k6) fuel |= isFlammable(w.getBlock(x + o[0], y + o[1], z + o[2]));
    const bool rained = d == 0 && state_.raining && precipitation() == Precipitation::Rain && y >= w.skyHeight(x, z);
    const bool forever = below == NETHERRACK || below == SOULED_STONE;
    if (rained || (!fuel && !isSolid(below)) || (!forever && fireRandom() < (fuel ? 0.07f : 0.3f))) {
        w.setBlock(x, y, z, AIR);
        if (d == dimIndex() && (Vec3(x + 0.5f, (float)y, z + 0.5f) - player_.pos).length() < 12) sound(Sound::Extinguish);
        return;
    }
    // Catch the neighbours: each flammable one may burn away into a new fire.
    if (blockTicks_.size() < 4000)
        for (const auto& o : k6) {
            const int nx = x + o[0], ny = y + o[1], nz = z + o[2];
            const ItemId n = w.getBlock(nx, ny, nz);
            if (!isFlammable(n) || fireRandom() > 0.45f) continue;
            if (isTnt(n) && d == dimIndex()) {
                w.setBlock(nx, ny, nz, AIR);
                igniteTnt(nx, ny, nz, 2.5f);
                continue;
            }
            w.setBlock(nx, ny, nz, FIRE);
        }
    if (ticking_[d].insert(cellKey(x, y, z)).second) blockTicks_.push_back({d, x, y, z, 1.0f + fireRandom()});
}

bool Session::lightFire(World& w, int x, int y, int z) {
    if (oldMode()) return false;  // 1.9.5: flint and steel only lights portals in the Old World
    // Flint and steel sets fire to the cell in front of the tapped face, on any block.
    const ItemId here = w.getBlock(x, y, z);
    if (here != AIR && !(isPlant(here) && here != FIRE)) return false;
    w.setBlock(x, y, z, FIRE);
    sound(Sound::FlintSteel);
    return true;
}

void Session::tickLiquid(int d, int x, int y, int z) {
    World& w = worldAt(d);
    const ItemId b = w.getBlock(x, y, z);
    const uint64_t key = cellKey(x, y, z);
    auto it = flow_[d].find(key);
    if (b != WATER && b != LAVA) {
        if (it != flow_[d].end()) flow_[d].erase(it);
        return;
    }
    const int level = it == flow_[d].end() ? 0 : it->second;  // 0 = a source
    static const int kSide[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    // Lava that touches water hardens: a source into obsidian, flowing lava into cobblestone.
    const ItemId other = b == WATER ? (ItemId)LAVA : (ItemId)WATER;
    auto harden = [&](int lx, int ly, int lz) {
        const bool source = !flow_[d].count(cellKey(lx, ly, lz));
        flow_[d].erase(cellKey(lx, ly, lz));
        w.setBlock(lx, ly, lz, source ? OBSIDIAN : COBBLESTONE);
        sound(Sound::LavaCool);
    };
    for (int k = 0; k < 6; ++k) {
        const int nx = x + (k < 4 ? kSide[k][0] : 0), ny = y + (k == 4 ? 1 : k == 5 ? -1 : 0), nz = z + (k < 4 ? kSide[k][1] : 0);
        if (w.getBlock(nx, ny, nz) != other) continue;
        if (b == LAVA) {
            harden(x, y, z);
            return;
        }
        harden(nx, ny, nz);
    }
    // A flowing cell needs liquid above it or a neighbour nearer the source; otherwise it drains.
    if (level > 0) {
        bool fed = w.getBlock(x, y + 1, z) == b;
        for (const auto& o : kSide) {
            if (w.getBlock(x + o[0], y, z + o[1]) != b) continue;
            auto n = flow_[d].find(cellKey(x + o[0], y, z + o[1]));
            fed |= n == flow_[d].end() || n->second < level;
        }
        if (!fed) {
            flow_[d].erase(key);
            w.setBlock(x, y, z, AIR);
            return;
        }
    }
    // Down first; only on solid ground does it spread to the sides.
    const ItemId below = y > 0 ? w.getBlock(x, y - 1, z) : (ItemId)BEDROCK;
    if (washedAway(below)) {
        w.setBlock(x, y - 1, z, b);
        flow_[d][cellKey(x, y - 1, z)] = 1;
        return;
    }
    if (below == b) return;  // already falling into its own pool
    const int reach = b == WATER ? 4 : d == 1 ? 6 : 4;  // 1.9.5: lava runs like water (farther in the Nether)
    if (level >= reach) return;
    for (const auto& o : kSide) {
        const ItemId n = w.getBlock(x + o[0], y, z + o[1]);
        if (!washedAway(n)) continue;
        w.setBlock(x + o[0], y, z + o[1], b);
        flow_[d][cellKey(x + o[0], y, z + o[1])] = (uint8_t)(level + 1);
    }
}

void Session::saveFlow() const {
    std::ofstream out(info_.folder + "/flow.dat");
    out << "VCFLOW1\n";
    for (int d = 0; d < 4; ++d)
        for (const auto& [k, level] : flow_[d]) out << d << " " << k << " " << (int)level << "\n";
}

void Session::loadFlow() {
    std::ifstream in(info_.folder + "/flow.dat");
    std::string magic;
    if (!(in >> magic) || magic != "VCFLOW1") return;
    int d, level;
    uint64_t k;
    while (in >> d >> k >> level && d >= 0 && d < 4 && flow_[d].size() < 200000) flow_[d][k] = (uint8_t)level;
}

void Session::saveTimers() const {
    saveBlockTexts();
    std::ofstream out(info_.folder + "/saplings.dat");
    out << "VCSAP1\n";
    for (const Timed& s : saplings_) out << s.dim << " " << s.x << " " << s.y << " " << s.z << " " << s.time << "\n";
}

void Session::loadTimers() {
    loadBlockTexts();
    std::ifstream in(info_.folder + "/saplings.dat");
    std::string magic;
    if (!std::getline(in, magic) || magic != "VCSAP1") return;
    Timed s;
    while (in >> s.dim >> s.x >> s.y >> s.z >> s.time && saplings_.size() < 4096) {
        s.dim = std::clamp(s.dim, 0, 2);
        s.time = std::clamp(s.time, 0.0f, 60.0f);
        saplings_.push_back(s);
    }
}

void Session::teleport(const Vec3& pos) {
    exitBoat();
    world_->ensureChunksAround(floorDiv((int)std::floor(pos.x), kChunkSize), floorDiv((int)std::floor(pos.z), kChunkSize), 1, 9);
    player_.pos = pos;
    player_.vel = {};
    player_.fallDistance = 0;
}

void Session::kill() {
    if (!dead_) hurt(1e6f, true);
}

void Session::respawn() {
    riding_ = -1;
    if (survival() && !info_.keepInventory && dead_) {
        // The inventory scatters where the player died and can be collected again.
        for (int i = 0; i < kInventorySize; ++i) {
            ItemStack& st = state_.inventory.slots[i];
            if (st.empty()) continue;
            float a = i * 2.4f;
            drops_.spawn(dimIndex(), player_.pos + Vec3(0, 0.8f, 0), {std::cos(a) * 2.5f, 3, std::sin(a) * 2.5f}, st);
        }
        for (int i = 0; i < 4; ++i) {
            if (state_.armor[i].empty()) continue;
            drops_.spawn(dimIndex(), player_.pos + Vec3(0, 0.8f, 0), {std::sin(i * 1.6f) * 2, 3, std::cos(i * 1.6f) * 2},
                         state_.armor[i]);
            state_.armor[i] = {};
        }
        if (!state_.offhand.empty()) {
            drops_.spawn(dimIndex(), player_.pos + Vec3(0, 0.8f, 0), {0, 3.5f, 0}, state_.offhand);
            state_.offhand = {};
        }
    }
    world_ = &overworld_;
    state_.health = kMaxHealth;
    state_.food = kMaxFood;
    state_.saturation = 5;
    state_.air = kMaxAir;
    if (survival() && !info_.keepInventory) state_.inventory.clear();
    exhaustion_ = regenTimer_ = starveTimer_ = drownTimer_ = 0;
    overworld_.ensureChunksAround(floorDiv((int)state_.spawn.x, kChunkSize), floorDiv((int)state_.spawn.z, kChunkSize), 1, 9);
    player_.pos = state_.spawn;
    player_.vel = {};
    player_.fallDistance = 0;
    player_.flying = false;
    player_.sneaking = false;
    dead_ = false;
    camera_ = player_;
    hurtFlash_ = 0;
    cancelTouches();
}

// ---------------------------------------------------------------- input

Session::Layout Session::layout() const {
    Layout l;
    if (oldMode()) {
        // 1.8.0 Old World: the HUD of 0.1-0.3. Nine big slots at the bottom, one tall jump
        // button bottom right, the grass block (the menu) top right; no map, chat or D-pad.
        l.slot = 26;
        l.hotbarX = std::floor((guiW_ - l.slot * 9) / 2);
        l.hotbarY = guiH_ - l.slot - 2;
        l.buttonSize = 48;
        l.jumpX = guiW_ - l.buttonSize - 24;
        l.jumpY = guiH_ - l.buttonSize - 24;
        l.flyDownX = -1000;
        l.pauseSize = 26;
        l.pauseX = guiW_ - l.pauseSize - 6;
        l.pauseY = 6;
        l.chatX = -1000;
        l.mapSize = 0;
        l.mapX = l.mapY = -1000;
        l.padX = l.padY = -1000;
        l.padButton = 0;
        if (kClassicEdition) {
            // Classic 0.0.3: the creator's own D-pad bottom left instead of the floating joystick.
            l.padButton = std::round(30 * settings_.buttonScale);
            l.padX = 10;
            l.padY = guiH_ - 3 * l.padButton - 8;
        }
        return l;
    }
    l.slot = 20;
    // Nine hotbar slots plus the "..." inventory button, like Pocket Edition.
    l.hotbarX = std::floor((guiW_ - l.slot * 10) / 2);
    l.hotbarY = guiH_ - l.slot - 2;
    l.buttonSize = std::round(38 * settings_.buttonScale);  // 1.3.0: a little bigger
    l.jumpX = guiW_ - l.buttonSize - 8;
    l.jumpY = guiH_ - l.buttonSize - 10;
    l.flyDownX = l.jumpX - l.buttonSize - 6;
    l.pauseSize = 18;
    l.pauseX = std::floor((guiW_ - l.pauseSize) / 2);  // top centre
    l.mapSize = settings_.showMap ? (mapBig_ ? 104.0f : 64.0f) : 0.0f;
    l.mapX = guiW_ - l.mapSize - 4;
    l.mapY = 4;
    l.pauseY = 4;
    l.chatX = l.pauseX - l.pauseSize - 6;
    if (kFeatures220) l.cameraX = l.pauseX + l.pauseSize + 6;
    l.padButton = std::round(33 * settings_.buttonScale);
    l.padX = 10;
    l.padY = guiH_ - 3 * l.padButton - 10;
    if (settings_.lefty) {
        // Mirror: jump buttons on the left, D-pad on the right.
        l.jumpX = guiW_ - l.jumpX - l.buttonSize;
        l.flyDownX = guiW_ - l.flyDownX - l.buttonSize;
        l.padX = guiW_ - 3 * l.padButton - 10;
    }
    return l;
}

bool Session::dpadDirection(const Layout& l, float x, float y, int& dx, int& dz) const {
    // The pad is a 3x3 grid: edge cells are the four arrows, corners are diagonals,
    // the centre is "stand still". A finger may slide from one cell to another.
    int cx = (int)std::floor((x - l.padX) / l.padButton), cy = (int)std::floor((y - l.padY) / l.padButton);
    if (cx < 0 || cy < 0 || cx > 2 || cy > 2) {
        dx = dz = 0;
        return false;
    }
    dx = cx - 1;
    dz = 1 - cy;
    return true;
}

Session::Touch* Session::findTouch(int id) {
    for (Touch& t : touches_)
        if (t.id == id) return &t;
    return nullptr;
}

void Session::cancelTouches() {
    for (Touch& t : touches_) t = Touch{};
    breakProgress_ = 0;
    target_ = {};
}

bool Session::placeStructureAhead(const Structure& original, std::string& message) {
    if (!original.valid()) {
        message = "neplatná struktura";
        return false;
    }
    // The direction the player faces, snapped to one of the four sides.
    Vec3 f = player_.forward();
    int dx = 0, dz = 0;
    if (std::fabs(f.x) > std::fabs(f.z)) dx = f.x > 0 ? 1 : -1;
    else dz = f.z > 0 ? 1 : -1;
    // Turn the structure so its z = 0 side (the bottom edge of the preview) faces the player.
    const Structure s = original.rotated(dz > 0 ? 0 : dx < 0 ? 1 : dz < 0 ? 2 : 3);
    int ax = (int)std::floor(player_.pos.x), ay = (int)std::floor(player_.pos.y), az = (int)std::floor(player_.pos.z);
    RayHit hit = raycast(*world_, player_.eye(), f, 8);
    if (hit.hit) {
        ax = hit.x + hit.nx;
        ay = hit.y + hit.ny;
        az = hit.z + hit.nz;
    } else {
        ax += dx * 2;
        az += dz * 2;
    }
    // The near edge of the structure starts at the anchor, centred across the view.
    int ox = dx > 0 ? ax : dx < 0 ? ax - s.sx + 1 : ax - s.sx / 2;
    int oz = dz > 0 ? az : dz < 0 ? az - s.sz + 1 : az - s.sz / 2;
    int oy = std::max(1, ay);
    // Move it away until the player is not inside.
    int px = (int)std::floor(player_.pos.x), pz = (int)std::floor(player_.pos.z);
    int py0 = (int)std::floor(player_.pos.y), py1 = (int)std::floor(player_.pos.y + 1.7f);
    auto overlaps = [&] {
        return px >= ox && px < ox + s.sx && pz >= oz && pz < oz + s.sz && py1 >= oy && py0 < oy + s.sy;
    };
    for (int i = 0; i < 64 && overlaps(); ++i) {
        ox += dx;
        oz += dz;
    }
    std::vector<BlockChange> changes;
    if (!vc::placeStructure(*world_, s, ox, oy, oz, changes, message)) return false;
    structureUndo_.push_back({world_, std::move(changes)});
    if (world_ == &overworld_) {
        state_.structures.emplace_back(ox + s.sx / 2, oz + s.sz / 2);
        if (state_.structures.size() > 16) state_.structures.erase(state_.structures.begin());
    }
    if (structureUndo_.size() > kMaxUndo) structureUndo_.erase(structureUndo_.begin());
    message = trf("Umístěno na {0} {1} {2}", {std::to_string(ox), std::to_string(oy), std::to_string(oz)});
    return true;
}

bool Session::canUndoStructure() const {
    return !structureUndo_.empty() && structureUndo_.back().world == world_;
}

bool Session::undoStructure() {
    if (!canUndoStructure()) return false;
    undoPlacement(*world_, structureUndo_.back().changes);
    structureUndo_.pop_back();
    return true;
}

void Session::selectSlot(int slot) {
    if (slot != state_.selectedSlot) itemNameTimer_ = 2.0f;
    state_.selectedSlot = slot;
}

void Session::touchDown(int id, float x, float y, double time) {
    Touch* t = findTouch(-1);
    if (!t || dead_) return;
    *t = Touch{};
    t->id = id;
    t->startX = t->x = t->lastX = x;
    t->startY = t->y = t->lastY = y;
    t->startTime = time;

    Layout l = layout();
    auto in = [&](float rx, float ry, float rw, float rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; };
    if (oldMode()) {
        // 1.8.0 Old World: hotbar, menu block, jump; the left half is a joystick, the rest looks around.
        if (in(l.hotbarX, l.hotbarY, l.slot * 9, l.slot)) {
            t->kind = Touch::Hotbar;
            selectSlot((int)((x - l.hotbarX) / l.slot));
        } else if (in(l.pauseX, l.pauseY, l.pauseSize, l.pauseSize)) {
            t->kind = Touch::Pause;
        } else if (in(l.jumpX, l.jumpY, l.buttonSize, l.buttonSize)) {
            t->kind = Touch::Jump;
            if (kClassicEdition && !survival()) {
                // Classic 0.0.3: creative flies; two taps on jump take off or land.
                if (time - lastJumpTap_ < kDoubleTap) {
                    player_.flying = !player_.flying;
                    player_.vel = {};
                    lastJumpTap_ = -1;
                } else {
                    lastJumpTap_ = time;
                }
            }
        } else if (kClassicEdition && player_.flying && in(l.jumpX - l.buttonSize - 6, l.jumpY, l.buttonSize, l.buttonSize)) {
            t->kind = Touch::FlyDown;
        } else if (int dx, dz; kClassicEdition && dpadDirection(l, x, y, dx, dz)) {
            t->kind = Touch::DPad;
        } else if (!kClassicEdition && (settings_.lefty ? x > guiW_ * 0.6f : x < guiW_ * 0.4f) && y > guiH_ * 0.35f) {  // as in 0.3
            t->kind = Touch::Joystick;
        } else {
            t->kind = Touch::Look;
            target_ = pick(x, y);
            breakProgress_ = 0;
        }
        return;
    }
    if (trainButtonTap(x, y)) {
        t->kind = Touch::None;  // 2.1.0 preview: get in, throttle, brake, get out
    } else if (in(l.hotbarX, l.hotbarY, l.slot * 9, l.slot)) {
        t->kind = Touch::Hotbar;
        selectSlot((int)((x - l.hotbarX) / l.slot));
    } else if (paperShown_ && in(guiW_ / 2 - std::min(guiH_ * 0.62f, 170.0f) / 2, 26, std::min(guiH_ * 0.62f, 170.0f),
                                 std::min(guiH_ * 0.62f, 170.0f))) {
        // 1.5.7: tapping the map in hand opens the big world map.
        t->kind = Touch::Hotbar;
        wantsWorldMap = true;
    } else if (in(l.hotbarX - l.slot - 4, l.hotbarY, l.slot, l.slot)) {
        // 1.5.7: the off-hand slot: a tap swaps it with the selected hotbar item.
        t->kind = Touch::Hotbar;
        swapOffhand();
        sound(Sound::Click);
    } else if (in(l.hotbarX + l.slot * 9, l.hotbarY, l.slot, l.slot)) {
        t->kind = Touch::InventoryButton;
    } else if (in(l.pauseX, l.pauseY, l.pauseSize, l.pauseSize)) {
        t->kind = Touch::Pause;
    } else if (in(l.chatX, l.pauseY, l.pauseSize, l.pauseSize)) {
        t->kind = Touch::Chat;
    } else if (in(l.cameraX, l.pauseY, l.pauseSize, l.pauseSize)) {
        t->kind = Touch::Camera;
    } else if (l.mapSize > 0 && in(l.mapX, l.mapY, l.mapSize, l.mapSize)) {
        t->kind = Touch::Map;
    } else if (in(l.jumpX, l.jumpY, l.buttonSize, l.buttonSize)) {
        t->kind = Touch::Jump;
        if (trainSeatTrain_ >= 0) {
            leaveTrain();
        } else if (seated_) {
            seated_ = false;  // stand up from the cushion
            player_.pos.y = seatY_ + 6 / 16.0f + 0.05f;
        } else if (riding_ >= 0) {
            exitBoat();  // the jump button gets out of the boat
        } else if (!survival() || settings_.devFly) {
            if (time - lastJumpTap_ < kDoubleTap) {
                player_.flying = !player_.flying;
                player_.vel = {};
                lastJumpTap_ = -1;
            } else {
                lastJumpTap_ = time;
            }
        }
    } else if (player_.flying && in(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize)) {
        t->kind = Touch::FlyDown;
    } else if (!player_.flying && riding_ < 0 && in(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize)) {
        // 1.5.7: crouch on and off: walk slowly and raise a shield.
        t->kind = Touch::Sneak;
        player_.sneaking = !player_.sneaking;
    } else if (int dx, dz; dpadDirection(l, x, y, dx, dz)) {
        t->kind = Touch::DPad;
    } else {
        t->kind = Touch::Look;
        target_ = pick(x, y);
        breakProgress_ = 0;
    }
}

void Session::touchMove(int id, float x, float y) {
    Touch* t = findTouch(id);
    if (!t) return;
    t->x = x;
    t->y = y;
    if (std::hypot(x - t->startX, y - t->startY) > 4) t->moved = true;
    if (t->kind == Touch::Look) {
        float k = kLookPerGuiPixel * sensitivity_ * (fov_ / 70.0f);
        player_.yaw += (x - t->lastX) * k;
        float dy = (y - t->lastY) * (settings_.invertY ? -1 : 1);
        player_.pitch = std::clamp(player_.pitch - dy * k, -1.55f, 1.55f);
        if (t->moved) {
            breakProgress_ = 0;
            target_ = {};
        }
    }
    t->lastX = x;
    t->lastY = y;
}

void Session::touchUp(int id, float x, float y, double time) {
    Touch* t = findTouch(id);
    if (!t) return;
    Layout l = layout();
    auto in = [&](float rx, float ry, float rw, float rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; };
    switch (t->kind) {
        case Touch::Look:
            if (state_.inventory.slots[state_.selectedSlot].id == BOW) {
                // Hold to draw the bow, let go to shoot where the finger points.
                if (bowCharge_ >= 0.15f) shootArrow(rayThroughScreen(x, y), bowCharge_);
                bowCharge_ = 0;
                break;
            }
            // A short tap places a block or uses the item in hand; tapping a boat gets in.
            if (!t->moved && time - t->startTime < kHoldToBreak) {
                int boat = pickBoat(x, y), mob = pickMob(x, y);
                ItemStack& heldNow = state_.inventory.slots[state_.selectedSlot];
                const bool powerstone = heldNow.id == POWERSTONE_DUST || heldNow.id == POWERSTONE_CHIP || heldNow.id == POWERSTONE_BLOCK;
                // 2.1.0 preview: a tap on a car takes it off the track (back into the inventory in survival).
                int trainCar = -1;
                if (heldNow.id != LOCOMOTIVE && heldNow.id != PASSENGER_WAGON && heldNow.id != FREIGHT_WAGON && heldNow.id != DRILL_CAR && heldNow.id != SUBWAY_LOCOMOTIVE && heldNow.id != SUBWAY_CAR && trainSeatTrain_ < 0) {
                    float dist;
                    trainCar = trains_.pick(dimIndex(), camera_.eye(), rayThroughScreen(x, y), Player::kReach + cameraDistance_, dist);
                    if (trainCar >= 0) {
                        const RayHit block = pick(x, y);
                        if (block.hit && (Vec3(block.x + 0.5f, block.y + 0.5f, block.z + 0.5f) - camera_.eye()).length() < dist) trainCar = -1;
                    }
                }
                if (trainCar >= 0 && trains_.trains()[(size_t)(trainCar / 64)].cars[(size_t)(trainCar % 64)].kind == CarKind::Drill &&
                    heldNow.id == RAIL_NS) {
                    // 2.3.0: rails held against the drill car go into it; it lays them ahead as it bores on.
                    Train& tr = trains_.trains()[(size_t)(trainCar / 64)];
                    ItemStack& h = state_.inventory.slots[state_.selectedSlot];
                    tr.rails += h.count;
                    if (survival()) h = {};
                    if (onMessage) onMessage(trf("Vrtací vůz: {0} kolejí", {std::to_string(tr.rails)}));
                    hand_.use();
                    sound(Sound::Place);
                } else if (trainCar >= 0) {
                    const int ti = trainCar / 64, ci = trainCar % 64;
                    const CarKind kind = trains_.trains()[(size_t)ti].cars[(size_t)ci].kind;
                    trains_.removeCar(ti, ci);
                    if (survival())
                        giveItem({kind == CarKind::Locomotive ? LOCOMOTIVE : kind == CarKind::Passenger ? PASSENGER_WAGON : kind == CarKind::Drill ? DRILL_CAR : kind == CarKind::SubwayLocomotive ? SUBWAY_LOCOMOTIVE : kind == CarKind::SubwayCar ? SUBWAY_CAR : FREIGHT_WAGON, 1, 0});
                    hand_.use();
                    sound(Sound::DoorIron);
                } else if (mob >= 0 && mobs_.mobs()[mob].type == MobType::Villager) {
                    wantsTrade = mob;  // villagers trade instead of being hit
                } else if (mob >= 0 && mobs_.mobs()[mob].type == MobType::Hogman && powerstone && mobs_.mobs()[mob].barter <= 0) {
                    // 1.5.7: a hog man takes powerstone, calms down and pays for it after a look.
                    Mob& hog = mobs_.mobs()[mob];
                    hog.calm = 90;
                    hog.barter = 2.5f;
                    if (survival() && --heldNow.count == 0) heldNow = {};
                    hand_.use();
                    sound(Sound::Grunt);
                    achieve("hogman_trade");
                } else if (mob >= 0 && mobs_.mobs()[mob].type == MobType::Cow && heldNow.id == BUCKET) {
                    // Milking a cow.
                    if (heldNow.count > 1) {
                        --heldNow.count;
                        giveItem({MILK_BUCKET, 1, 0});
                    } else {
                        heldNow = {MILK_BUCKET, 1, 0};
                    }
                    hand_.use();
                    sound(Sound::Splash);
                    achieve("milk");
                } else if (mob >= 0 && info_.expBreeding && mobs_.mobs()[mob].love <= 0 &&
                           (((mobs_.mobs()[mob].type == MobType::Pig || mobs_.mobs()[mob].type == MobType::Sheep ||
                              mobs_.mobs()[mob].type == MobType::Cow) && heldNow.id == WHEAT) ||
                            (mobs_.mobs()[mob].type == MobType::Chicken && heldNow.id == WHEAT_SEEDS))) {
                    // 1.9.0 build 3 (2.0 preview): a fed animal looks for a mate; two of them have a young one.
                    Mob& fed = mobs_.mobs()[mob];
                    fed.love = 20;
                    if (survival() && --heldNow.count == 0) heldNow = {};
                    hand_.use();
                    sound(Sound::Eat);
                    const World& here = *world_;
                    for (int k = 0; k < 6; ++k) particles_.spawnAmbient(here, fed.pos + Vec3(0, 1.0f + k * 0.1f, 0), Vec3(0, 0.5f, 0), POPPY, 1.0f, 0.1f, -0.05f, 1.2f);
                    const MobType type = fed.type;
                    const Vec3 at = fed.pos;
                    const int fedDim = fed.dim;
                    for (size_t k = 0; k < mobs_.mobs().size(); ++k) {
                        Mob& o = mobs_.mobs()[k];
                        if ((int)k == mob || o.type != type || o.dim != fedDim || o.love <= 0 || (o.pos - at).length() > 5) continue;
                        o.love = mobs_.mobs()[mob].love = 0;
                        mobs_.spawn(type, fedDim, (o.pos + at) * 0.5f);
                        break;
                    }
                } else if (mob >= 0) {
                    attackMob(mob);
                } else if (int other = pickOtherPlayer(x, y); other >= 0) {
                    attackPlayer(other);
                } else if (boat >= 0 && boat != riding_) {
                    riding_ = boat;
                    player_.flying = false;
                    achieve("boat");
                } else if (state_.inventory.slots[state_.selectedSlot].id == BOAT && boat < 0) {
                    if (placeBoat()) {
                        if (survival()) state_.inventory.removeOne(state_.selectedSlot);
                        hand_.place();
                        sound(Sound::Place);
                    }
                } else if (const ItemId held = state_.inventory.slots[state_.selectedSlot].id;
                           held == LOCOMOTIVE || held == PASSENGER_WAGON || held == FREIGHT_WAGON || held == DRILL_CAR || held == SUBWAY_LOCOMOTIVE || held == SUBWAY_CAR) {
                    // 2.1.0 preview: a car goes on the rail the player taps.
                    const RayHit h = pick(x, y);
                    const Vec3 f = player_.forward();
                    const int look = std::fabs(f.x) > std::fabs(f.z) ? (f.x > 0 ? 1 : 3) : (f.z > 0 ? 2 : 0);
                    const CarKind kind = held == LOCOMOTIVE ? CarKind::Locomotive : held == PASSENGER_WAGON ? CarKind::Passenger : held == DRILL_CAR ? CarKind::Drill : held == SUBWAY_LOCOMOTIVE ? CarKind::SubwayLocomotive : held == SUBWAY_CAR ? CarKind::SubwayCar : CarKind::Freight;
                    if (h.hit && isRail(world_->getBlock(h.x, h.y, h.z)) && trains_.place(*world_, dimIndex(), kind, h.x, h.y, h.z, look)) {
                        if (survival()) state_.inventory.removeOne(state_.selectedSlot);
                        hand_.place();
                        sound(Sound::DoorIron);
                    } else if (onMessage) {
                        onMessage(tr("Vlak se dává na kolej (za poslední vagon, nebo na volnou kolej)."));
                    }
                } else {
                    useItem(pick(x, y));
                }
            }
            target_ = {};
            breakProgress_ = 0;
            break;
        case Touch::InventoryButton:
            if (in(l.hotbarX + l.slot * 9, l.hotbarY, l.slot, l.slot)) wantsInventory = true;
            break;
        case Touch::Pause:
            if (in(l.pauseX, l.pauseY, l.pauseSize, l.pauseSize)) (kClassicEdition && oldMode() ? wantsInventory : wantsPause) = true;
            break;
        case Touch::Chat:
            if (in(l.chatX, l.pauseY, l.pauseSize, l.pauseSize)) wantsChat = true;
            break;
        case Touch::Camera:
            if (in(l.cameraX, l.pauseY, l.pauseSize, l.pauseSize)) wantsCamera = true;
            break;
        case Touch::Map:
            if (in(l.mapX, l.mapY, l.mapSize, l.mapSize)) mapBig_ = !mapBig_;  // tap: bigger / smaller
            break;
        default: break;
    }
    *t = Touch{};
}

Vec3 Session::rayThroughScreen(float sx, float sy) const {
    // Like the original Pocket Edition, you aim at the block under your finger.
    Vec3 f = camera_.forward();
    Vec3 right = cross(f, {0, 1, 0}).normalized();
    Vec3 up = cross(right, f);
    float t = std::tan(fov_ * kPi / 360.0f), aspect = guiW_ / guiH_;
    float nx = (2 * sx / guiW_ - 1) * t * aspect, ny = (1 - 2 * sy / guiH_) * t;
    return (f + right * nx + up * ny).normalized();
}

int Session::pickMob(float sx, float sy) const {
    float dist;
    const Vec3 eye = camera_.eye(), dir = rayThroughScreen(sx, sy);
    int mob = mobs_.pick(dimIndex(), eye, dir, Player::kReach + cameraDistance_, &dist);
    if (mob < 0) return -1;
    RayHit block = pick(sx, sy);
    if (block.hit) {
        Vec3 c(block.x + 0.5f, block.y + 0.5f, block.z + 0.5f);
        if ((c - eye).length() < dist - 0.3f) return -1;  // a wall in between
    }
    return mob;
}

void Session::attackMob(int index) {
    if (attackCooldown_ > 0) return;
    attackCooldown_ = 0.35f;
    const ItemId held = state_.inventory.slots[state_.selectedSlot].id;
    const float damage = heldDamage();
    hand_.swing();
    sound(Sound::Hurt);
    if (survival() && toolKind(held) != ToolKind::None) damageHeldTool();
    if (mobsFromHost) {
        // The host owns the mobs: it is told, and the mob flashes here right away.
        Mob& m = mobs_.mobs()[(size_t)index];
        m.hurtTime = 0.35f;
        if (onHitRemoteMob) onHitRemoteMob(m.uid, damage);
        return;
    }
    mobs_.hurt(index, damage, player_.pos);
}

float Session::heldDamage() const {
    // The bare hand does 1; tools hit harder with better materials, axes the hardest.
    const ItemId held = state_.inventory.slots[state_.selectedSlot].id;
    switch (toolKind(held)) {
        case ToolKind::Axe: return 3.0f + toolTier(held);
        case ToolKind::Pickaxe: return 2.0f + toolTier(held);
        case ToolKind::Shovel: return 1.5f + toolTier(held) * 0.5f;
        case ToolKind::Sword: return 3.0f + toolTier(held) * 1.25f;
        default: return 1.0f;
    }
}

int Session::pickOtherPlayer(float sx, float sy) const {
    if (!pickRemotePlayer) return -1;
    float dist = 0;
    const Vec3 eye = camera_.eye(), dir = rayThroughScreen(sx, sy);
    const int who = pickRemotePlayer(eye, dir, Player::kReach + cameraDistance_, dist);
    if (who < 0) return -1;
    RayHit block = pick(sx, sy);
    if (block.hit && (Vec3(block.x + 0.5f, block.y + 0.5f, block.z + 0.5f) - eye).length() < dist - 0.3f) return -1;
    return who;
}

void Session::attackPlayer(int who) {
    if (attackCooldown_ > 0) return;
    attackCooldown_ = 0.35f;
    hand_.swing();
    if (!pvp) {
        if (onMessage) onMessage(tr("PvP je vypnuté."));
        return;
    }
    sound(Sound::Hurt);
    if (onHitPlayer) onHitPlayer(who, heldDamage());
}

bool Session::shieldBlocks(const Vec3& push) {
    // 1.5.7: a shield in either hand, raised by crouching, stops what comes at the player from the front.
    ItemStack* shield = state_.offhand.id == SHIELD ? &state_.offhand
                        : state_.inventory.slots[state_.selectedSlot].id == SHIELD ? &state_.inventory.slots[state_.selectedSlot] : nullptr;
    if (!shield || hurtCooldown_ > 0 || !player_.sneaking) return false;  // raised only while crouching
    const Vec3 f = player_.forward();
    const Vec3 from = Vec3(-push.x, 0, -push.z).normalized();  // towards the attacker
    if (Vec3(f.x, 0, f.z).normalized().x * from.x + Vec3(f.x, 0, f.z).normalized().z * from.z < 0.35f) return false;
    if (++shield->damage >= toolDurability(SHIELD)) *shield = {};
    hurtCooldown_ = 0.5f;
    sound(Sound::Place);
    achieve("shield_block");
    return true;
}

void Session::hurtByPlayer(float damage, const Vec3& knock) {
    if (!survival() || dead_) return;
    if (shieldBlocks(knock)) return;
    hurt(damage, false, true);
    player_.vel = player_.vel + knock;
}

void Session::protectedSpot() {
    if (clock_ - protectedToast_ < 2) return;
    protectedToast_ = clock_;
    if (onMessage) onMessage(tr("Okolí spawnu je chráněné."));
}

bool Session::tillOrHarvest(const RayHit& hit, ItemStack& held) {
    // The hoe turns grass or dirt with air above into farmland.
    World& w = *world_;
    const ItemId b = w.getBlock(hit.x, hit.y, hit.z), above = w.getBlock(hit.x, hit.y + 1, hit.z);
    if ((b != GRASS && b != DIRT) || (above != AIR && above != TALL_GRASS)) return false;
    if (above == TALL_GRASS) w.setBlock(hit.x, hit.y + 1, hit.z, AIR);
    w.setBlock(hit.x, hit.y, hit.z, FARMLAND);
    particles_.spawnChips(w, hit.x, hit.y, hit.z, 0, 1, 0, DIRT, 6);
    hand_.use();
    sound(Sound::DigDirt);
    if (survival()) damageHeldTool();
    (void)held;
    return true;
}

void Session::shootArrow(const Vec3& dir, float power) {
    // Survival needs an arrow in the inventory; creative shoots for free.
    if (survival()) {
        int slot = -1;
        for (int i = 0; i < (int)state_.inventory.slots.size() && slot < 0; ++i)
            if (state_.inventory.slots[i].id == ARROW) slot = i;
        if (slot < 0) {
            if (onMessage) onMessage(tr("Nemáš šípy"));
            return;
        }
        state_.inventory.removeOne(slot);
        damageHeldTool();
    }
    const Vec3 eye = camera_.eye() + dir * 0.4f;
    arrows_.push_back({dimIndex(), eye, dir * (12.0f + 26.0f * power), 30.0f, false});
    if (arrows_.size() > 64) arrows_.erase(arrows_.begin());
    hand_.use();
    sound(Sound::Click);
}

void Session::updateArrows(float dt) {
    for (size_t i = 0; i < arrows_.size(); ++i) {
        Arrow& a = arrows_[i];
        a.life -= dt;
        bool gone = a.life <= 0;
        if (a.dim == dimIndex() && !gone) {
            if (a.stuck) {
                // Walk over a stuck arrow to take it back.
                if ((a.pos - (player_.pos + Vec3(0, 0.5f, 0))).length() < 1.4f) {
                    if (survival()) giveItem({ARROW, 1, 0});
                    gone = true;
                }
            } else {
                World& w = *world_;
                const float speed = a.vel.length();
                const Vec3 dir = a.vel * (1 / std::max(0.001f, speed));
                const float step = speed * dt;
                float mobDist = 0;
                int mob = mobs_.pick(a.dim, a.pos, dir, step, &mobDist);
                // The first solid block on the way.
                float blockDist = step + 1;
                for (float s = 0; s <= step; s += 0.1f) {
                    Vec3 p = a.pos + dir * s;
                    if (isSolid(w.getBlock((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z)))) {
                        blockDist = s;
                        break;
                    }
                }
                if (mob >= 0 && mobDist < blockDist) {
                    if (mobsFromHost) {
                        mobs_.mobs()[(size_t)mob].hurtTime = 0.35f;
                        if (onHitRemoteMob) onHitRemoteMob(mobs_.mobs()[(size_t)mob].uid, 1.5f + speed * 0.22f);
                    } else {
                        mobs_.hurt(mob, 1.5f + speed * 0.22f, a.pos - dir);
                    }
                    sound(Sound::Hurt);
                    achieve("archer");
                    gone = true;
                } else if (blockDist <= step) {
                    a.pos = a.pos + dir * std::max(0.0f, blockDist - 0.05f);
                    a.stuck = true;
                    a.life = std::min(a.life, 20.0f);
                    sound(Sound::Dig);
                } else {
                    a.pos = a.pos + a.vel * dt;
                    a.vel.y -= 16 * dt;
                    a.vel = a.vel * (1 - 0.15f * dt);
                }
            }
        }
        if (gone) {
            arrows_.erase(arrows_.begin() + (long)i);
            --i;
        }
    }
}

void Session::updateFireballs(float dt) {
    // A Husking's fireball flies straight, burns the player it reaches and bursts on blocks.
    for (size_t i = 0; i < fireballs_.size(); ++i) {
        Fireball& f = fireballs_[i];
        f.life -= dt;
        bool gone = f.life <= 0;
        if (!gone && f.dim == dimIndex()) {
            const Vec3 next = f.pos + f.vel * dt;
            const Vec3 p = player_.pos;
            if (std::fabs(next.x - p.x) < 0.55f && std::fabs(next.z - p.z) < 0.55f && next.y > p.y - 0.2f && next.y < p.y + 2.0f) {
                if (survival() && !dead_ && !shieldBlocks(f.vel)) {
                    hurt(f.damage, false, true);
                    player_.vel = player_.vel + Vec3(f.vel.x, 0, f.vel.z).normalized() * 3.5f + Vec3(0, 2.5f, 0);
                }
                gone = true;
            } else if (isSolid(world_->getBlock((int)std::floor(next.x), (int)std::floor(next.y), (int)std::floor(next.z)))) {
                gone = true;
            } else {
                f.pos = next;
            }
            if (gone) {
                particles_.spawnChips(*world_, (int)std::floor(f.pos.x), (int)std::floor(f.pos.y), (int)std::floor(f.pos.z), 0, 1, 0, LAVA, 6);
                sound(Sound::Hurt);
            }
        }
        if (gone) {
            fireballs_.erase(fireballs_.begin() + (long)i);
            --i;
        }
    }
}

void Session::summon(MobType type) {
    Vec3 f = player_.forward();
    Vec3 at = player_.pos + Vec3(f.x, 0, f.z).normalized() * 3 + Vec3(0, 0.5f, 0);
    mobs_.spawn(type, dimIndex(), at);
}

float Session::spawnRandom() {
    spawnRng_ ^= spawnRng_ << 13;
    spawnRng_ ^= spawnRng_ >> 17;
    spawnRng_ ^= spawnRng_ << 5;
    return (spawnRng_ % 100000) / 100000.0f;
}

void Session::updateShowcase() {
    // The Anniversary modpack builds its showcase once per world, a little way from the player.
    if (state_.hasShowcase) {
        const float dx = player_.pos.x - (state_.showcaseX + kShowcaseW / 2.0f), dz = player_.pos.z - (state_.showcaseZ + kShowcaseD / 2.0f);
        if (std::fabs(dx) < kShowcaseW / 2.0f + 2 && std::fabs(dz) < kShowcaseD / 2.0f + 2) achieve("anniv_showcase");
        return;
    }
    World& w = overworld_;
    const int ox = (int)std::floor(player_.pos.x) + 10, oz = (int)std::floor(player_.pos.z) + 6;
    for (int x : {ox, ox + kShowcaseW - 1})
        for (int z : {oz, oz + kShowcaseD - 1})
            if (!w.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize))) return;  // wait for the land to load
    // On the ground in the middle of the site; columns of stone bricks hold it up where the land falls away.
    const int oy = std::clamp(w.skyHeight(ox + kShowcaseW / 2, oz + kShowcaseD / 2), 2, kChunkHeight - 12);
    const Structure plaza = anniversaryShowcase();
    for (int x = 0; x < kShowcaseW; ++x)
        for (int z = 0; z < kShowcaseD; ++z) {
            for (int y = oy - 1; y > 0 && !isSolid(w.getBlock(ox + x, y, oz + z)); --y) w.setBlock(ox + x, y, oz + z, STONE_BRICKS);
            for (int y = 0; y < 12; ++y) {
                const ItemId b = y < plaza.sy ? plaza.at(x, y, z) : (ItemId)AIR;
                w.setBlock(ox + x, oy + y, oz + z, b);
            }
        }
    const FurnaceKey chestKey{0, ox + kShowcaseChest[0], oy + kShowcaseChest[1], oz + kShowcaseChest[2]};
    ChestContents gifts{};
    gifts[0] = {ANNIV_TOKEN, 1, 0};
    gifts[1] = {PARTY_POPPER, 4, 0};
    gifts[2] = {CAKE, 1, 0};
    gifts[3] = {BALLOON, 2, 0};
    gifts[4] = {BALLOON + 1, 2, 0};
    gifts[5] = {BALLOON + 2, 2, 0};
    gifts[6] = {CONFETTI_BLOCK, 8, 0};
    chests_[chestKey] = gifts;
    state_.hasShowcase = true;
    state_.showcaseX = ox;
    state_.showcaseY = oy;
    state_.showcaseZ = oz;
    state_.structures.emplace_back(ox + kShowcaseW / 2, oz + kShowcaseD / 2);  // on the minimap
    if (state_.structures.size() > 16) state_.structures.erase(state_.structures.begin());
    if (onMessage) onMessage(tr("Nedaleko se otevřela výroční výstava!"));
}

void Session::spreadTar(const Vec3& where) {
    // Tar pools where the animal fell, and a new tar monster rises from them.
    World& w = *world_;
    const int x0 = (int)std::floor(where.x), y0 = (int)std::floor(where.y + 0.2f), z0 = (int)std::floor(where.z);
    int placed = 0;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz) {
            if (std::abs(dx) + std::abs(dz) == 2 && spawnRandom() < 0.5f) continue;
            const int x = x0 + dx, z = z0 + dz;
            for (int y = y0; y >= y0 - 2; --y) {
                const ItemId here = w.getBlock(x, y, z), below = w.getBlock(x, y - 1, z);
                if ((here == AIR || isPlant(here)) && isSolid(below) && !isChest(below)) {
                    w.setBlock(x, y, z, TAR);
                    ++placed;
                    break;
                }
            }
        }
    const int dim = dimIndex();
    if (mobs_.countType(dim, MobType::TarMonster, where, 32) < 6) {
        mobs_.spawn(MobType::TarMonster, dim, where + Vec3(0, 0.1f, 0));
        mobs_.mobs().back().cave = where.y < w.skyHeight((int)where.x, (int)where.z) - 2;
    }
    if (placed && onMessage && !tarWarned_) {
        tarWarned_ = true;
        onMessage(tr("Dehet se šíří!"));
    }
}

void Session::spawnInCave() {
    // A dark spot near the player, under the surface: air with solid ground and room above.
    World& w = *world_;
    const float a = spawnRandom() * 2 * kPi, r = 12 + spawnRandom() * 20;
    const int x = (int)std::floor(player_.pos.x + std::cos(a) * r), z = (int)std::floor(player_.pos.z + std::sin(a) * r);
    if (!w.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize))) return;
    const int top = w.skyHeight(x, z) - 6;
    const int py = (int)player_.pos.y;
    for (int tries = 0; tries < 6; ++tries) {
        const int y = std::clamp(py - 12 + (int)(spawnRandom() * 24), 6, kChunkHeight - 3);
        if (y >= top) continue;
        const ItemId ground = w.getBlock(x, y - 1, z);
        if (w.getBlock(x, y, z) != AIR || w.getBlock(x, y + 1, z) != AIR || !isSolid(ground) || isLiquid(ground)) continue;
        const Vec3 at(x + 0.5f, (float)y, z + 0.5f);
        // Next to tar the tar monster may wake (rare); bats in the open air of caves; otherwise monsters.
        bool tarNear = false;
        for (int dx = -3; dx <= 3 && !tarNear; ++dx)
            for (int dz = -3; dz <= 3 && !tarNear; ++dz)
                tarNear = w.getBlock(x + dx, y - 1, z + dz) == TAR || w.getBlock(x + dx, y, z + dz) == TAR ||
                          w.getBlock(x + dx, y - 1, z + dz) == TAR_BLOCK;
        const float roll = spawnRandom();
        MobType type;
        if (tarNear && roll < (overworld_.config().generation >= 4 ? 0.5f : 0.35f)) type = MobType::TarMonster;  // 1.6.0: tar lakes wake it more often
        else if (roll < 0.3f) type = MobType::Bat;
        else if (roll < 0.6f) type = MobType::Spider;
        else type = MobType::ForgottenMan;
        if (type == MobType::Bat ? mobs_.countType(0, MobType::Bat, player_.pos, 48) >= 4
                                 : mobs_.count(0, true, player_.pos, 48) >= 8)
            return;
        if (type == MobType::TarMonster && mobs_.countType(0, MobType::TarMonster, player_.pos, 64) >= 3) return;
        mobs_.spawn(type, 0, type == MobType::Bat ? at + Vec3(0, 1, 0) : at);
        mobs_.mobs().back().cave = true;
        return;
    }
}

void Session::tickSpawners() {
    // 1.6.0: spawners within 12 blocks of the player wake a monster now and then: at most three
    // around each one, and few in the whole area, so a dungeon is a fight but not a flood.
    World& w = *world_;
    if (mobs_.count(0, true, player_.pos, 48) >= 10) return;
    const int px = (int)std::floor(player_.pos.x), py = (int)std::floor(player_.pos.y), pz = (int)std::floor(player_.pos.z);
    for (int x = px - 12; x <= px + 12; ++x)
        for (int z = pz - 12; z <= pz + 12; ++z)
            for (int y = std::max(1, py - 12); y <= std::min(kChunkHeight - 2, py + 12); ++y) {
                if (w.getBlock(x, y, z) != MOB_SPAWNER) continue;
                const Vec3 at(x + 0.5f, (float)y, z + 0.5f);
                if (spawnRandom() > 0.25f || mobs_.count(0, true, at, 8) >= 3) continue;
                // A free spot beside the cage.
                const int sx = x + (int)(spawnRandom() * 5) - 2, sz = z + (int)(spawnRandom() * 5) - 2;
                int sy = y + 1;
                while (sy > y - 3 && !isSolid(w.getBlock(sx, sy - 1, sz))) --sy;
                if (w.getBlock(sx, sy, sz) != AIR || w.getBlock(sx, sy + 1, sz) != AIR || !isSolid(w.getBlock(sx, sy - 1, sz))) continue;
                static const MobType kinds[3] = {MobType::Spider, MobType::ForgottenMan, MobType::TntZombie};
                MobType type = kinds[(uint32_t)(x * 31 + y * 17 + z * 7) % 3];  // each spawner has its own kind
                if (w.config().devMap == 5) {
                    // 2.5.0: the mob arena: the pen's own kind (6 pens a row, 14 and 16 blocks apart), one at a time.
                    const int m = (floorDiv(x, 14)) + (floorDiv(z, 16)) * 6;
                    if (m < 0 || m >= (int)MobType::Count || mobs_.countType(0, (MobType)m, at, 6) >= 1) continue;
                    type = (MobType)m;
                }
                mobs_.spawn(type, 0, Vec3(sx + 0.5f, (float)sy, sz + 0.5f));
                mobs_.mobs().back().cave = true;
                particles_.spawnChips(w, x, y, z, 0, 1, 0, MOB_SPAWNER, 6);
                return;  // one a second at most
            }
}

void Session::updateMobs(float dt) {
    attackCooldown_ = std::max(0.0f, attackCooldown_ - dt);
    if (oldMode()) {
        // 1.8.0 Old World: 0.1-0.3 had no mobs at all; nothing spawns and nothing is left over.
        mobs_.mobs().clear();
        return;
    }
    const int dim = dimIndex();
    const bool halloween = settings_.addonHalloween;
    const bool calm = settings_.calmMode;  // no monsters at all
    if (calm) mobs_.clearHostiles();
    mobs_.zombieSkin = halloween;
    std::vector<MobEvent> events;
    if (mobsFromHost) {
        // A guest: the host's mobs glide to where the host last saw them; nothing spawns here.
        mobs_.followRemote(dt);
        return;
    }
    mobs_.update(*world_, dim, player_.pos, survival() && !dead_, dt, events);
    for (const MobEvent& e : events) {
        if (e.kind == MobEvent::HitPlayer) {
            if (!survival() || dead_) continue;
            if (shieldBlocks(e.knock)) {
                player_.vel = player_.vel + e.knock * 0.3f;  // pushed back a little
                continue;
            }
            hurt(e.damage, false, true);
            player_.vel = player_.vel + e.knock;
        } else if (e.kind == MobEvent::Barter) {
            // 1.5.7: what a hog man gives for powerstone.
            static const ItemStack kOffers[] = {{CRIMSON_LOG, 6, 0}, {ANCIENT_COIN, 2, 0}, {OBSIDIAN, 2, 0}, {TORCH, 8, 0},
                                                {IRON_INGOT, 3, 0}, {ARROW, 8, 0}, {RUBY, 1, 0}, {POWERSTONE_CHIP, 2, 0},
                                                {COOKED_PORK, 3, 0}, {AIR, 1, 0}};
            const float roll = spawnRandom();
            ItemStack offer = kOffers[(int)(roll * 10) % 10];
            if (offer.id == AIR) offer = explorationMaps() && roll > 0.95f ? ItemStack{EXPLORER_MAP, 1, 0} : ItemStack{SHIELD, 1, 0};
            drops_.spawn(dim, e.pos, {0, 3, 0}, offer);
            sound(Sound::Coin);
        } else if (e.kind == MobEvent::Died) {
            float roll = spawnRandom();
            std::vector<std::pair<ItemId, int>> loot = mobDrops(e.type, roll);
            if (dim == 3 && (e.type == MobType::Pig || e.type == MobType::Sheep) && roll > 0.6f) loot.emplace_back(PALE_CRYSTAL, 1);  // 1.9.0 build 3
            if (const ModMob* mod = ModRegistry::get().mob(e.variant)) {
                loot.clear();
                for (auto& [key, n] : mod->drops)
                    if (ItemId id = itemFromKey(key); id != AIR) loot.emplace_back(id, n);
            }
            if (e.killer >= 0 && onRemoteKill) {
                onRemoteKill(e.killer, loot);  // a guest's catch goes to the guest
                continue;
            }
            for (auto [id, n] : loot)
                drops_.spawn(dim, e.pos, {spawnRandom() * 2 - 1, 3, spawnRandom() * 2 - 1}, {id, (uint8_t)n, 0});
            if (mobInfo(e.type).hostile) achieve("hunter");
            if (e.type == MobType::TarMonster) achieve("tar_hunter");
            if (e.type == MobType::Husking) achieve("husking_hunter");
            if (mobInfo(e.type).boss) {
                ++state_.bossesDefeated;
                int ax, ay, az;
                if (world_->nearestStructure((int)e.pos.x, (int)e.pos.z, ax, ay, az)) state_.bossArenas.emplace_back(ax, az);
                if (e.type == MobType::SkyGuardian) {
                    if (onMessage) onMessage(tr("Strážce Nebe byl poražen! Získáváš Nebeská křídla."));
                } else {
                    achieve("crimson_man");
                    if (onMessage) onMessage(tr("Karmínový muž byl poražen!"));
                }
            }
        } else if (e.kind == MobEvent::BossRoar) {
            sound(Sound::Hurt);
        } else if (e.kind == MobEvent::BossWave) {
            // A ring of force around the Crimson Man: hurts and throws back whoever is near.
            sound(Sound::Grunt);
            for (int k = 0; k < 16; ++k) {
                const float a = k * kPi / 8;
                particles_.spawnChips(*world_, (int)std::floor(e.pos.x + std::cos(a) * 3), (int)e.pos.y, (int)std::floor(e.pos.z + std::sin(a) * 3),
                                      0, 1, 0, VOID_STONE_BRICKS, 2);
            }
            const Vec3 away = player_.pos - e.pos;
            const float d = away.length();
            if (d < 10 && survival() && !dead_) {
                const Vec3 push = Vec3(away.x, 0, away.z).normalized() * (14.0f - d) + Vec3(0, 7, 0);
                if (shieldBlocks(push)) player_.vel = player_.vel + push * 0.4f;
                else hurt(e.damage, false, true), player_.vel = player_.vel + push;
            }
        } else if (e.kind == MobEvent::BossSummon && e.type == MobType::SkyGuardian) {
            // 1.9.5: the Sky Guardian calls two Reach Stalkers out of the air.
            if (mobs_.countType(dim, MobType::ReachStalker, player_.pos, 24) < 3)
                for (int k = 0; k < 2; ++k) {
                    const float a = spawnRandom() * 2 * kPi;
                    mobs_.spawn(MobType::ReachStalker, dim, player_.pos + Vec3(std::cos(a) * 5, 1, std::sin(a) * 5));
                }
            if (onMessage) onMessage(tr("Strážce Nebe volá Reach Stalkery!"));
        } else if (e.kind == MobEvent::BossSummon) {
            // Two forgotten men rise near the player.
            if (mobs_.countType(dim, MobType::ForgottenMan, player_.pos, 24) < 4)
                for (int k = 0; k < 2; ++k) {
                    const float a = spawnRandom() * 2 * kPi;
                    const int x = (int)std::floor(player_.pos.x + std::cos(a) * 5), z = (int)std::floor(player_.pos.z + std::sin(a) * 5);
                    int y = (int)std::floor(player_.pos.y) + 3;
                    while (y > 1 && !isSolid(world_->getBlock(x, y - 1, z))) --y;
                    if (world_->getBlock(x, y, z) == AIR && world_->getBlock(x, y + 1, z) == AIR)
                        mobs_.spawn(MobType::ForgottenMan, dim, Vec3(x + 0.5f, (float)y, z + 0.5f));
                }
            if (onMessage) onMessage(tr("Karmínový muž volá zapomenuté na pomoc!"));
        } else if (e.kind == MobEvent::Explode) {
            explode(e.pos, 3.5f * (settings_.expBigBoom ? 2.0f : 1.0f), false);
        } else if (e.kind == MobEvent::Teleport) {
            // 1.9.5: a puff of pale sparks where the Reach Stalker vanishes and appears.
            for (int k = 0; k < 12; ++k)
                particles_.spawnAmbient(*world_, e.pos + Vec3(0, 0.3f + k * 0.15f, 0), Vec3((fireRandom() - 0.5f) * 2, 0.5f, (fireRandom() - 0.5f) * 2),
                                        PALE_CRYSTAL, 0.7f, 0.08f, 0, 1.5f);
            sound(Sound::Fuse);
        } else if (e.kind == MobEvent::TarSpread) {
            spreadTar(e.pos);
        } else if (e.kind == MobEvent::Fireball) {
            fireballs_.push_back({dim, e.pos, e.knock, 5.0f, e.damage});
            if (fireballs_.size() > 32) fireballs_.erase(fireballs_.begin());
            sound(Sound::Grunt);
        }
    }

    // Spawning, once a second.
    if ((spawnTimer_ -= dt) > 0) return;
    spawnTimer_ = 1;
    if (!calm && dim == 0) tickSpawners();
    const float t = (float)std::fmod(state_.dayTime, 24000.0);
    const bool night = isNight(t) || t >= 13000;
    mobs_.despawn(dim, player_.pos, 80, dim == 0 && !night && !halloween);
    World& w = *world_;
    // A free spot on the ground some distance from the player.
    auto spot = [&](float minR, float maxR, Vec3& out, ItemId& ground) {
        float a = spawnRandom() * 2 * kPi, r = minR + spawnRandom() * (maxR - minR);
        int x = (int)std::floor(player_.pos.x + std::cos(a) * r), z = (int)std::floor(player_.pos.z + std::sin(a) * r);
        if (!w.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize))) return false;
        int y = w.skyHeight(x, z);
        if (y <= 1 || y >= kChunkHeight - 3) return false;
        ground = w.getBlock(x, y - 1, z);
        if (!isSolid(ground) || isLeaves(ground) || w.getBlock(x, y, z) != AIR || w.getBlock(x, y + 1, z) != AIR) return false;
        out = Vec3(x + 0.5f, (float)y, z + 0.5f);
        return true;
    };
    Vec3 at;
    ItemId ground;
    if (dim == 0) {
        // Villagers live in villages: up to four near the well of a village close by.
        int vx, vy, vz;
        if (overworld_.nearestStructure((int)player_.pos.x, (int)player_.pos.z, vx, vy, vz) &&
            std::hypot(vx - player_.pos.x, vz - player_.pos.z) < 72 && w.getChunk(floorDiv(vx, kChunkSize), floorDiv(vz, kChunkSize))) {
            const Vec3 well(vx + 0.5f, (float)vy, vz + 0.5f);
            int have = mobs_.countType(0, MobType::Villager, well, 30);
            for (int k = have; k < 4; ++k) {
                const Vec3 spots[4] = {{4, 0, 0}, {-4, 0, 0}, {0, 0, 4}, {0, 0, -4}};
                mobs_.spawnVillager(0, well + spots[k], (Profession)(k % 3), well);
            }
        }
        if (mobs_.count(0, false, player_.pos, 80) - mobs_.countType(0, MobType::Bat, player_.pos, 80) < 8 && spawnRandom() < 0.25f && spot(24, 44, at, ground) && ground == GRASS) {
            const MobType kAnimals[4] = {MobType::Pig, MobType::Sheep, MobType::Chicken, MobType::Cow};
            MobType type = kAnimals[std::min(3, (int)(spawnRandom() * 4))];
            int herd = 2 + (int)(spawnRandom() * 2);
            for (int i = 0; i < herd; ++i) mobs_.spawn(type, 0, at + Vec3(spawnRandom() * 2 - 1, 0, spawnRandom() * 2 - 1));
        }
        // Mods' mobs: animals by day on grass, monsters at night.
        if (!ModRegistry::get().mobs().empty() && spawnRandom() < 0.2f && spot(24, 44, at, ground) && ground == GRASS) {
            const auto& modMobs = ModRegistry::get().mobs();
            const int pick = std::min((int)modMobs.size() - 1, (int)(spawnRandom() * modMobs.size()));
            const ModMob& mm = modMobs[(size_t)pick];
            const bool hostile = mobInfo(mm.base).hostile;
            if (spawnRandom() < mm.spawn && (hostile ? night && !calm : mobs_.count(0, false, player_.pos, 80) < 10)) {
                mobs_.spawn(mm.base, 0, at);
                mobs_.mobs().back().variant = (uint16_t)(pick + 1);
                mobs_.mobs().back().health = mm.health;
            }
        }
        if (!calm && night && mobs_.count(0, true, player_.pos, 64) < 7 && spawnRandom() < 0.5f && spot(18, 40, at, ground) &&
            ground != WATER && ground != LAVA) {
            const float r = spawnRandom();
            mobs_.spawn(r < 0.45f ? MobType::ForgottenMan : r < 0.65f ? MobType::PumpkinSlime : r < 0.88f ? MobType::Spider : MobType::TntZombie, 0, at);
        }
        // Caves are dark day and night: spiders, forgotten men, bats, and deep down the rare tar monster.
        if (!calm) spawnInCave();
        // Halloween: hordes of zombies, day and night.
        if (!calm && halloween && mobs_.count(0, true, player_.pos, 64) < 16 && spawnRandom() < 0.3f && spot(20, 40, at, ground) &&
            ground != WATER && ground != LAVA) {
            int horde = 3 + (int)(spawnRandom() * 3);
            for (int i = 0; i < horde; ++i)
                mobs_.spawn(MobType::ForgottenMan, 0, at + Vec3(spawnRandom() * 3 - 1.5f, 0, spawnRandom() * 3 - 1.5f));
        }
    } else if (dim == 1) {
        // 1.5.1: Huskings roam the Powerstone Wasteland of the Cornerstone Dimension. There is no
        // sky here, so look for a floor near the player's height.
        // 1.5.7: hog men roam the rest of the Cornerstone Dimension in small bands.
        if (!calm && mobs_.countType(1, MobType::Hogman, player_.pos, 64) < 3 && spawnRandom() < 0.2f) {
            const float a = spawnRandom() * 2 * kPi, r = 16 + spawnRandom() * 18;
            const int x = (int)std::floor(player_.pos.x + std::cos(a) * r), z = (int)std::floor(player_.pos.z + std::sin(a) * r);
            if (w.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize)) && !w.wastelandAt(x, z))
                for (int y = (int)player_.pos.y + 8; y > (int)player_.pos.y - 10 && y > 2; --y) {
                    const ItemId ground = w.getBlock(x, y - 1, z);
                    if (w.getBlock(x, y, z) == AIR && w.getBlock(x, y + 1, z) == AIR && isSolid(ground) && ground != LAVA) {
                        mobs_.spawn(MobType::Hogman, 1, Vec3(x + 0.5f, (float)y, z + 0.5f));
                        break;
                    }
                }
        }
        if (!calm && mobs_.countType(1, MobType::Husking, player_.pos, 64) < 4 && spawnRandom() < 0.3f) {
            const float a = spawnRandom() * 2 * kPi, r = 14 + spawnRandom() * 18;
            const int x = (int)std::floor(player_.pos.x + std::cos(a) * r), z = (int)std::floor(player_.pos.z + std::sin(a) * r);
            if (w.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize)) && w.wastelandAt(x, z))
                for (int y = (int)player_.pos.y + 8; y > (int)player_.pos.y - 10 && y > 2; --y) {
                    const ItemId ground = w.getBlock(x, y - 1, z);
                    if (w.getBlock(x, y, z) == AIR && w.getBlock(x, y + 1, z) == AIR && isSolid(ground) && ground != LAVA) {
                        mobs_.spawn(MobType::Husking, 1, Vec3(x + 0.5f, (float)y, z + 0.5f));
                        break;
                    }
                }
        }
    } else if (dim == 2) {
        World::VoidBiome biome = void_.voidBiomeAt((int)player_.pos.x, (int)player_.pos.z);
        if (!calm && biome != World::VoidBiome::Lava && mobs_.count(2, true, player_.pos, 64) < 6 && spawnRandom() < 0.45f &&
            spot(16, 36, at, ground))
            mobs_.spawn(biome == World::VoidBiome::Crimson || spawnRandom() < 0.3f ? MobType::PumpkinSlime : MobType::ForgottenMan, 2, at);
        // 1.6.0: while the eyes on the arena's obsidian pillars watch, they heal the Crimson Man;
        // break them to win.
        for (Mob& m : mobs_.mobs()) {
            if (m.dim != 2 || m.type != MobType::CrimsonMan || !m.alive()) continue;
            int eyes = 0;
            const int bx = (int)std::floor(m.pos.x), by = (int)std::floor(m.pos.y), bz = (int)std::floor(m.pos.z);
            for (int x = bx - 22; x <= bx + 22; ++x)
                for (int z = bz - 22; z <= bz + 22; ++z)
                    for (int y = by - 2; y <= by + 12; ++y)
                        if (void_.getBlock(x, y, z) == EYE_LOG && void_.getBlock(x, y - 1, z) == OBSIDIAN) {
                            ++eyes;
                            if (spawnRandom() < 0.3f) particles_.spawnChips(void_, x, y, z, 0, -1, 0, EYE_LOG, 2);
                        }
            const float maxHp = mobInfo(MobType::CrimsonMan).health;
            if (eyes > 0 && m.health < maxHp) {
                m.health = std::min(maxHp, m.health + eyes * 0.6f);
                if (!bossHealHint_ && onMessage) {
                    bossHealHint_ = true;
                    onMessage(tr("Oči na sloupech léčí Karmínového muže. Znič je!"));
                }
            }
        }
        // The Crimson Man wakes when the player steps into an arena he still guards.
        int ax, ay, az;
        if (!mobs_.boss(2) && void_.nearestStructure((int)player_.pos.x, (int)player_.pos.z, ax, ay, az)) {
            float d = std::hypot(player_.pos.x - ax, player_.pos.z - az);
            bool beaten = std::find(state_.bossArenas.begin(), state_.bossArenas.end(), std::make_pair(ax, az)) != state_.bossArenas.end();
            if (d < kArenaRadius + 2 && !beaten && survival() && !calm) {
                mobs_.spawn(MobType::CrimsonMan, 2, Vec3(ax + 0.5f, (float)ay, az - 6.5f));
                if (onMessage) onMessage(tr("Karmínový muž se probudil!"));
            }
        }
    } else if (dim == 3) {
        // 1.9.5: the Sky Guardian wakes when the player steps onto a temple he still guards.
        int tx, ty, tz;
        if (!mobs_.boss(3) && celestial_.nearestStructure((int)player_.pos.x, (int)player_.pos.z, tx, ty, tz)) {
            const float d = std::hypot(player_.pos.x - tx, player_.pos.z - tz);
            const bool beaten = std::find(state_.bossArenas.begin(), state_.bossArenas.end(), std::make_pair(tx, tz)) != state_.bossArenas.end();
            if (d < World::kTempleRadius - 1 && std::fabs(player_.pos.y - ty) < 6 && !beaten && survival() && !calm) {
                mobs_.spawn(MobType::SkyGuardian, 3, Vec3(tx + 0.5f, (float)ty + 1, tz + 0.5f));
                if (onMessage) onMessage(tr("Strážce Nebe se probudil!"));
            }
        }
        // 1.9.0 build 3: celestial pigs and sheep graze on the sky islands.
        // 1.9.5: and Celestlings; Sky Drifters glide over; at night Reach Stalkers hunt.
        if (!calm && night && mobs_.countType(3, MobType::ReachStalker, player_.pos, 64) < 2 && spawnRandom() < 0.15f && spot(18, 36, at, ground) &&
            ground == CELESTIAL_GRASS)
            mobs_.spawn(MobType::ReachStalker, 3, at);
        if (mobs_.countType(3, MobType::SkyDrifter, player_.pos, 80) < 3 && spawnRandom() < 0.15f && spot(20, 40, at, ground))
            mobs_.spawn(MobType::SkyDrifter, 3, at + Vec3(0, 6 + spawnRandom() * 6, 0));
        if (mobs_.count(3, false, player_.pos, 80) < 8 && spawnRandom() < 0.3f && spot(20, 40, at, ground) && ground == CELESTIAL_GRASS) {
            const float r = spawnRandom();
            const MobType type = r < 0.35f ? MobType::Pig : r < 0.7f ? MobType::Sheep : MobType::Celestling;
            const int herd = 2 + (int)(spawnRandom() * 2);
            for (int i = 0; i < herd; ++i) mobs_.spawn(type, 3, at + Vec3(spawnRandom() * 2 - 1, 0, spawnRandom() * 2 - 1));
        }
    }
}

int Session::pickBoat(float sx, float sy) const {
    float dist;
    const Vec3 eye = camera_.eye(), dir = rayThroughScreen(sx, sy);
    int boat = boats_.pick(dimIndex(), eye, dir, Player::kReach + cameraDistance_, dist);
    if (boat < 0) return -1;
    RayHit block = pick(sx, sy);
    if (block.hit) {
        Vec3 c(block.x + 0.5f, block.y + 0.5f, block.z + 0.5f);
        if ((c - eye).length() < dist) return -1;  // a block is in front of it
    }
    return boat;
}

bool Session::placeBoat() {
    // On the water surface the player looks at, otherwise on the ground in front.
    const Vec3 eye = player_.eye(), dir = player_.forward();
    for (float t = 0.5f; t < Player::kReach + 1; t += 0.25f) {
        Vec3 p = eye + dir * t;
        int x = (int)std::floor(p.x), y = (int)std::floor(p.y), z = (int)std::floor(p.z);
        ItemId b = world_->getBlock(x, y, z);
        if (b == WATER && world_->getBlock(x, y + 1, z) != WATER) {
            boats_.spawn(dimIndex(), Vec3(x + 0.5f, y + 0.55f, z + 0.5f), player_.yaw);
            return true;
        }
        if (isSolid(b)) {
            Vec3 back = eye + dir * (t - 0.3f);
            boats_.spawn(dimIndex(), Vec3(std::floor(back.x) + 0.5f, (float)std::floor(back.y), std::floor(back.z) + 0.5f), player_.yaw);
            return true;
        }
    }
    return false;
}

void Session::exitBoat() {
    if (riding_ < 0) return;
    const Boat& b = boats_.boats()[riding_];
    riding_ = -1;
    // Step out beside the boat, onto whatever is free.
    for (float side : {1.2f, -1.2f, 0.0f}) {
        Vec3 p = b.pos + Vec3(std::cos(b.yaw) * side, 0.6f, std::sin(b.yaw) * side);
        if (!player_.collides(*world_, p)) {
            player_.pos = p;
            break;
        }
    }
    player_.vel = {};
    player_.fallDistance = 0;
}

RayHit Session::pick(float sx, float sy) const {
    return raycast(*world_, camera_.eye(), rayThroughScreen(sx, sy), Player::kReach + cameraDistance_);
}

// ---------------------------------------------------------------- blocks

void Session::useItem(const RayHit& hit) {
    ItemStack& held = state_.inventory.slots[state_.selectedSlot];
    World& w = *world_;

    // Tapping a crafting table or a furnace opens it, like in Pocket Edition.
    if (hit.hit) {
        ItemId block = w.getBlock(hit.x, hit.y, hit.z);
        if (block == CRAFTING_TABLE) {
            wantsCrafting = true;
            return;
        }
        if (isFurnace(block)) {
            openFurnaceAt({dimIndex(), hit.x, hit.y, hit.z});
            return;
        }
        if ((block == COMMAND_BLOCK || block == STRUCTURE_BLOCK) && !survival()) {
            // 1.9.5 build 3: their editor (Game asks for the text).
            wantsBlockEditor = block == COMMAND_BLOCK ? 1 : 2;
            blockEditorKey = {dimIndex(), hit.x, hit.y, hit.z};
            sound(Sound::Click);
            return;
        }
        if (isChest(block) || block == BARREL || block == LOOT_BARREL_CAMP) {  // 1.7.2 build 2: the barrel keeps 27 stacks too
            openChestAt({dimIndex(), hit.x, hit.y, hit.z});
            return;
        }
        if (block == CUTTER || block == WOOD_CUTTER) {
            wantsCutter = true;
            cutterWood = block == WOOD_CUTTER;
            return;
        }
        // 2.2.0 preview: honey waxes bronze (it stops ageing); an axe scrapes the wax off, or the
        // rust down by one stage.
        if (kFeatures220 && bronzeAgingBase(block) >= 0 && (held.id == HONEY_BALL || toolKind(held.id) == ToolKind::Axe)) {
            const int base = bronzeAgingBase(block), stage = bronzeStage(block);
            ItemId to = block;
            if (held.id == HONEY_BALL && !bronzeWaxed(block)) to = bronzeVariant(base, stage, true);
            else if (held.id != HONEY_BALL && bronzeWaxed(block)) to = bronzeVariant(base, stage, false);
            else if (held.id != HONEY_BALL && stage > 0) to = bronzeVariant(base, stage - 1, false);
            if (to != block) {
                w.setBlock(hit.x, hit.y, hit.z, to);
                particles_.spawnBurst(w, hit.x, hit.y, hit.z, held.id == HONEY_BALL ? HONEY_BLOCK : block);
                if (held.id == HONEY_BALL && survival() && --held.count == 0) held = {};
                hand_.use();
                sound(held.id == HONEY_BALL ? Sound::Place : Sound::Dig);
                return;
            }
        }
        if (block == CELESTIAL_FRAME && held.id == CRIMSON_HEAD) {
            // 1.7.0 preview: a head on a frame block, like an eye on the End's frame. The ring of
            // twelve frames lies round a 3 x 3 hole; with every head in place the hole lights up.
            w.setBlock(hit.x, hit.y, hit.z, CELESTIAL_FRAME_FULL);
            if (survival() && --held.count == 0) held = {};
            hand_.use();
            sound(Sound::Place);
            int full = 0, x0 = hit.x, z0 = hit.z, x1 = hit.x, z1 = hit.z;
            for (int dx = -4; dx <= 4; ++dx)
                for (int dz = -4; dz <= 4; ++dz) {
                    const ItemId b = w.getBlock(hit.x + dx, hit.y, hit.z + dz);
                    if (b != CELESTIAL_FRAME && b != CELESTIAL_FRAME_FULL) continue;
                    x0 = std::min(x0, hit.x + dx), z0 = std::min(z0, hit.z + dz);
                    x1 = std::max(x1, hit.x + dx), z1 = std::max(z1, hit.z + dz);
                    full += b == CELESTIAL_FRAME_FULL;
                }
            if (full >= 12) {
                for (int x = x0 + 1; x < x1; ++x)
                    for (int z = z0 + 1; z < z1; ++z) w.setBlock(x, hit.y, z, CELESTIAL_PORTAL);
                sound(Sound::Explode);
                achieve("celestial_portal");
                if (onMessage) onMessage(tr("Nebeský portál se otevřel!"));
            } else if (onMessage) {
                onMessage(tr("Hlavy v rámu: ") + std::to_string(full) + " / 12");
            }
            return;
        }
        if (block == CELESTIAL_FRAME || block == CELESTIAL_FRAME_FULL) {
            // 1.6.0: the frame waits for the Celestial Reach.
            if (onMessage) onMessage(tr("Nebeská říše je zatím zavřená. Otevře se ve VoxelCraftu 1.7.0."));
            hand_.use();
            return;
        }
        if (isDoor(block) || isTrapdoor(block)) {
            toggleDoor(hit.x, hit.y, hit.z);
            hand_.use();
            return;
        }
        if (isBed(block) && held.id == CUSHION && w.getBlock(hit.x, hit.y + 1, hit.z) == AIR) {
            // 2.2.0: a cushion laid on the bed, as a pillow.
            w.setBlock(hit.x, hit.y + 1, hit.z, CUSHION);
            if (survival()) state_.inventory.removeOne(state_.selectedSlot);
            sound(Sound::Place);
            hand_.use();
            return;
        }
        if (isBed(block)) {
            sleepInBed(hit.x, hit.y, hit.z);
            return;
        }
        if (block == CUSHION && isBed(w.getBlock(hit.x, hit.y - 1, hit.z))) {
            sleepInBed(hit.x, hit.y - 1, hit.z);  // 2.2.0: a pillow: tapping it is tapping the bed
            return;
        }
        if (block == CUSHION && !player_.sneaking) {
            // 2.0.0: sit down on the cushion.
            seated_ = true;
            seatX_ = hit.x, seatY_ = hit.y, seatZ_ = hit.z;
            player_.flying = false;
            player_.vel = {};
            hand_.use();
            return;
        }
        if (block == STATION_SIGN) {
            // 2.5.0: a station's name board: a tap reads the name, a sneaking tap writes a new one.
            const FurnaceKey key{dimIndex(), hit.x, hit.y, hit.z};
            if (player_.sneaking) {
                wantsBlockEditor = 4;
                blockEditorKey = key;
            } else if (onMessage) {
                std::string name = blockText(key);
                int pages = 1, mark = -1;
                // 2.5.0: a board of a developer map: its own text (the HQ journal turns a page per tap; a
                // distance mark of the test track also tells how fast the nearest train goes).
                const std::string dev = name.empty() ? world_->devSignText(hit.x, hit.y, hit.z, devSignPage_, &pages, &mark) : "";
                if (!dev.empty()) {
                    std::string text = dev;
                    if (pages > 1) devSignPage_ = (devSignPage_ + 1) % pages;
                    if (mark >= 0) {
                        int ti, car;
                        if (trains_.nearest(dimIndex(), Vec3(hit.x + 0.5f, (float)hit.y, hit.z + 0.5f), 120.0f, ti, car)) {
                            const float v = std::fabs(trains_.trains()[(size_t)ti].speed);
                            text += "  ·  " + trf("vlak jede {0} m/s ({1} km/h)", {std::to_string((int)std::lround(v)), std::to_string((int)std::lround(v * 3.6f))});
                        } else {
                            text += "  ·  " + tr("rychlost = 10 m ÷ čas mezi dvěma značkami");
                        }
                    }
                    onMessage(text);
                } else {
                    if (name.empty()) name = world_->metroStationName(hit.x, hit.z);
                    onMessage(trf("Stanice: {0}", {name}));
                }
            }
            sound(Sound::Click);
            hand_.use();
            return;
        }
        if (block == RADIO) {
            // 2.2.0: Rozhlas. A tap writes its announcement; a sneaking tap reads it out now.
            const FurnaceKey key{dimIndex(), hit.x, hit.y, hit.z};
            if (player_.sneaking) {
                playRadio(key);
            } else {
                wantsBlockEditor = 3;
                blockEditorKey = key;
                sound(Sound::Click);
            }
            hand_.use();
            return;
        }
        if (block == SOUND_BLOCK) {
            // 2.0.0: a tap picks the next sound and plays it; powerstone plays the chosen one.
            const FurnaceKey key{dimIndex(), hit.x, hit.y, hit.z};
            const int next = (std::atoi(blockText(key).c_str()) + 1) % kSoundBlockSounds;
            blockTexts_[key] = std::to_string(next);
            playSoundBlock(key);
            if (onMessage) onMessage(trf("Zvukový blok: {0}", {tr(soundBlockName(next))}));
            hand_.use();
            return;
        }
        if (isTurntable(block)) {
            // Build 3: a tap turns the turntable's track a quarter turn.
            w.setBlock(hit.x, hit.y, hit.z, block == TURNTABLE ? (ItemId)(TURNTABLE + 1) : TURNTABLE);
            hand_.use();
            sound(Sound::DoorIron);
            return;
        }
        if (isRailSwitch8(block)) {
            // 2.3.0: a tap throws the flexible switch.
            w.setBlock(hit.x, hit.y, hit.z, railSwitch8Id(railSwitch8Entry(block), railSwitch8Left(block), 1 - railSwitch8State(block)));
            hand_.use();
            sound(Sound::DoorIron);
            return;
        }
        if (isRailSwitch(block)) {
            // 2.1.0 preview: a tap throws the switch.
            const int k = railSwitchIndex(block);
            w.setBlock(hit.x, hit.y, hit.z, railSwitchId(k / 2, 1 - k % 2, railSwitchLeft(block)));
            hand_.use();
            sound(Sound::Click);
            return;
        }
        if (isSwitch(block)) {
            w.setBlock(hit.x, hit.y, hit.z, toggledSwitch(block));
            powerChanged(hit.x, hit.y, hit.z);
            hand_.use();
            sound(Sound::Click);
            return;
        }
        if (block == CAKE) {
            state_.food = std::min(kMaxFood, state_.food + 7);
            state_.saturation = std::min(state_.food, state_.saturation + 3);
            w.setBlock(hit.x, hit.y, hit.z, AIR);
            hand_.use();
            sound(Sound::Eat);
            return;
        }
        if (block == GIFT_BOX) {
            // A present: sweets and something useful.
            w.setBlock(hit.x, hit.y, hit.z, AIR);
            static const ItemId kGifts[] = {RUBY, STEEL_INGOT, COOKED_PORK, BED_ITEM, IRON_SWORD, TNT, POWERSTONE_DUST, APPLE};
            giveItem({CANDY_CANE, (uint8_t)(2 + (rng_ >> 3) % 3), 0});
            rng_ = rng_ * 1664525u + 1013904223u;
            giveItem({kGifts[(rng_ >> 8) % 8], (uint8_t)(1 + (rng_ >> 16) % 3), 0});
            sound(Sound::Coin);
            if (onMessage) onMessage(tr("Veselé Vánoce!"));
            return;
        }
        if (isTnt(block) && (held.id == FLINT_AND_STEEL || held.id == VOID_FLINT_AND_STEEL)) {
            w.setBlock(hit.x, hit.y, hit.z, AIR);
            igniteTnt(hit.x, hit.y, hit.z, block == NUKE_TNT ? 8.0f : 4.0f);
            if (block == NUKE_TNT) tnt_.back().nuke = true;
            hand_.use();
            if (survival()) damageHeldTool();
            return;
        }
    }
    if (held.empty()) return;
    if (held.id == METRO_MAP) {  // 2.5.0: the subway map
        wantsMetroMap = true;
        hand_.use();
        sound(Sound::Page);
        return;
    }
    if (isJournal(held.id)) {  // 2.5.0: the dead explorer's journal
        wantsDocument = 100 + (held.id - JOURNAL);
        state_.journalsRead |= 1 << (held.id - JOURNAL);
        hand_.use();
        sound(Sound::Page);
        return;
    }
    if (isDocument(held.id)) {
        readDocument(held.id - OLD_DOCUMENT);
        return;
    }
    if (held.id == WRITABLE_BOOK || held.id == WRITTEN_BOOK) {
        ItemStack& slot = state_.inventory.slots[state_.selectedSlot];
        if (slot.damage == 0 || !books_.count(slot.damage)) {
            while (books_.count(nextBook_)) ++nextBook_;
            slot.damage = (uint16_t)nextBook_;
            books_[nextBook_];
        }
        wantsBook = slot.damage;
        hand_.use();
        return;
    }
    if (isArmor(held.id)) {
        equipArmor(state_.selectedSlot);
        return;
    }

    if (isFood(held.id)) {
        // Hunger points and saturation like the classic foods.
        float food = 4, saturation = 2.4f;
        if (isModId(held.id)) food = (float)modFood(held.id), saturation = food * 0.6f;
        if (held.id == RAW_PORK) food = 3, saturation = 1.8f;
        if (held.id == COOKED_PORK) food = 8, saturation = 12.8f;
        if (held.id == RAW_MUTTON) food = 2, saturation = 1.2f;
        if (held.id == COOKED_MUTTON) food = 6, saturation = 9.6f;
        if (held.id == BREAD) food = 5, saturation = 6;
        if (held.id == HONEY_BALL) food = 3, saturation = 1.5f;  // 1.9.5 build 3
        if (held.id == STARBERRIES) food = 2, saturation = 2, state_.health = std::min(20.0f, state_.health + 4);  // 2.2.0: they heal
        if (held.id == VOID_TRUFFLE) food = 6, saturation = 8;
        if (held.id == RAW_CHICKEN) food = 2, saturation = 1.2f;
        if (held.id == COOKED_CHICKEN) food = 6, saturation = 7.2f;
        if (held.id == RAW_BEEF) food = 3, saturation = 1.8f;
        if (held.id == STEAK) food = 8, saturation = 12.8f;
        if (held.id == MILK_BUCKET) food = 3, saturation = 2;
        const bool milk = held.id == MILK_BUCKET;
        if (state_.food < kMaxFood || !survival()) {
            state_.food = std::min(kMaxFood, state_.food + food);
            state_.saturation = std::min(state_.food, state_.saturation + saturation);
            if (survival()) state_.inventory.removeOne(state_.selectedSlot);
            if (survival() && milk) giveItem({BUCKET, 1, 0});  // the bucket stays
            hand_.use();
            sound(Sound::Eat);
        }
        return;
    }
    if (held.id == BUCKET || held.id == WATER_BUCKET || held.id == LAVA_BUCKET) {
        useBucket(held);
        return;
    }
    if (held.id == PARTY_POPPER) {
        particles_.spawnConfetti(*world_, player_.eye() + player_.forward() * 2.0f, 60);
        sound(Sound::Place);
        hand_.use();
        if (survival()) state_.inventory.removeOne(state_.selectedSlot);
        achieve("anniv_popper");
        return;
    }
    if (!hit.hit) return;
    int x = hit.x + hit.nx, y = hit.y + hit.ny, z = hit.z + hit.nz;
    if (canEdit && (!canEdit(x, y, z) || !canEdit(hit.x, hit.y, hit.z))) {
        protectedSpot();
        return;
    }
    if (isSpawnEgg(held.id)) {
        const Vec3 feet(x + 0.5f, (float)y, z + 0.5f);
        switch (held.id) {
            case EGG_PIG: mobs_.spawn(MobType::Pig, dimIndex(), feet); break;
            case EGG_SHEEP: mobs_.spawn(MobType::Sheep, dimIndex(), feet); break;
            case EGG_SLIME: mobs_.spawn(MobType::PumpkinSlime, dimIndex(), feet); break;
            case EGG_FORGOTTEN: mobs_.spawn(MobType::ForgottenMan, dimIndex(), feet); break;
            case EGG_CHICKEN: mobs_.spawn(MobType::Chicken, dimIndex(), feet); break;
            case EGG_COW: mobs_.spawn(MobType::Cow, dimIndex(), feet); break;
            case EGG_SPIDER: mobs_.spawn(MobType::Spider, dimIndex(), feet); break;
            case EGG_BAT: mobs_.spawn(MobType::Bat, dimIndex(), feet + Vec3(0, 1, 0)); break;
            case EGG_TNT_ZOMBIE: mobs_.spawn(MobType::TntZombie, dimIndex(), feet); break;
            case EGG_TAR_MONSTER: mobs_.spawn(MobType::TarMonster, dimIndex(), feet); break;
            case EGG_HUSKING: mobs_.spawn(MobType::Husking, dimIndex(), feet); break;
            case EGG_HOGMAN: mobs_.spawn(MobType::Hogman, dimIndex(), feet); break;
            default: mobs_.spawnVillager(dimIndex(), feet, (Profession)(rng_ % 3), feet); break;
        }
        rng_ = rng_ * 1664525u + 1013904223u;
        if (survival()) state_.inventory.removeOne(state_.selectedSlot);
        hand_.use();
        sound(Sound::Grunt);
        return;
    }
    if (held.id == HOE) {
        tillOrHarvest(hit, held);
        return;
    }
    if (held.id == FLINT_AND_STEEL || held.id == VOID_FLINT_AND_STEEL) {
        // 1.7.0: the way to the Crimson Void opens only from the Cornerstone Dimension.
        if (held.id == VOID_FLINT_AND_STEEL && world_ != &nether_) {
            if (onMessage) onMessage(tr("Portál do Karmínové prázdnoty jde zapálit jen v dimenzi úhelného kamene."));
            return;
        }
        if (tryLightPortal(w, x, y, z, held.id == VOID_FLINT_AND_STEEL) ||
            (held.id == FLINT_AND_STEEL && lightFire(w, x, y, z))) {
            hand_.use();
            if (survival()) damageHeldTool();
        }
        return;
    }
    if (held.id == OAK_DOOR_ITEM || held.id == IRON_DOOR_ITEM || held.id == BRONZE_DOOR_ITEM || held.id == CACTUS || isTrapdoor(held.id) || held.id == BED_ITEM ||
        isSkeletonHead(held.id) || held.id == TORCH) {
        ItemId existing = w.getBlock(x, y, z);
        if ((existing == AIR || existing == TALL_GRASS || isLiquid(existing)) && (held.id == TORCH || !player_.intersectsBlock(x, y, z)))
            placeSpecial(hit, held, x, y, z);
        return;
    }
    if (isStairs(held.id) || isSlab(held.id)) {
        placeShape(hit, held, x, y, z);
        return;
    }
    if (!isPlaceable(held.id)) return;
    // Tapping tall grass or a snow layer builds in its place, like in the classic games.
    if (w.getBlock(hit.x, hit.y, hit.z) == TALL_GRASS || w.getBlock(hit.x, hit.y, hit.z) == SNOW_LAYER) {
        x = hit.x;
        y = hit.y;
        z = hit.z;
    }
    ItemId existing = w.getBlock(x, y, z);
    if (existing != AIR && existing != TALL_GRASS && existing != SNOW_LAYER && !isLiquid(existing)) return;
    // Plants only grow on grass or dirt and not under water; saplings each on their own ground.
    if (held.id == DRIPSTONE_UP) {
        // Pointed dripstone hangs from a ceiling or stands on the floor.
        const bool hanging = hit.ny == -1;
        if (!isSolid(w.getBlock(x, hanging ? y + 1 : y - 1, z)) || isLiquid(existing)) return;
        if (!w.setBlock(x, y, z, hanging ? (ItemId)DRIPSTONE_DOWN : (ItemId)DRIPSTONE_UP)) return;
        if (survival()) state_.inventory.removeOne(state_.selectedSlot);
        hand_.place();
        sound(Sound::Place);
        return;
    }
    if (isPlant(held.id)) {
        ItemId below = w.getBlock(x, y - 1, z);
        ItemId above = w.getBlock(x, y + 1, z);
        bool ok = isSapling(held.id) ? saplingCanGrowOn(held.id, below)
                  : isCrop(held.id)    ? below == FARMLAND
                  : held.id == VINES   ? isSolid(above) || above == VINES || hit.ny == 0  // under a ceiling or on a wall
                                       : (below == GRASS || below == DIRT);
        if (!ok || isLiquid(existing)) return;
    }
    if (isSolid(held.id) && player_.intersectsBlock(x, y, z)) return;
    // Cables and switches go on the floor or, tapped on a side of a block, on that wall.
    ItemId place = held.id;
    if ((held.id == POWER_CABLE || held.id == SWITCH) && hit.ny == 0) {
        const int side = hit.nz == 1 ? 0 : hit.nx == -1 ? 1 : hit.nz == -1 ? 2 : 3;
        place = (ItemId)((held.id == POWER_CABLE ? WALL_CABLE : WALL_SWITCH) + side);
    }
    if (held.id == POWER_CABLE && hit.ny < 0) place = CEILING_CABLE;  // 1.5.7: under a ceiling
    if (isCrossingSignal(held.id) || isCrossingGate(held.id)) {
        // Build 3: the boards and the lamps face the player.
        const Vec3 f = player_.forward();
        const int look = std::fabs(f.x) > std::fabs(f.z) ? (f.x > 0 ? 1 : 3) : (f.z > 0 ? 2 : 0);
        const int facing = (look + 2) & 3;
        place = isCrossingSignal(held.id) ? (ItemId)(CROSSING_SIGNAL + facing * 3) : (ItemId)(CROSSING_GATE + facing * 2);
    }
    if (isBufferStop(held.id)) {
        // The plate faces the way the player looks: the track runs towards it.
        const Vec3 f = player_.forward();
        const int look = std::fabs(f.x) > std::fabs(f.z) ? (f.x > 0 ? 1 : 3) : (f.z > 0 ? 2 : 0);
        place = (ItemId)(BUFFER_STOP + look);
    }
    if (isRail(held.id)) {
        // 2.1.0 preview: rails need ground, and take their shape from the rails around them.
        if (!isSolid(w.getBlock(x, y - 1, z)) || isRail(w.getBlock(x, y - 1, z))) return;
        const Vec3 f = player_.forward();
        const int look = std::fabs(f.x) > std::fabs(f.z) ? (f.x > 0 ? 1 : 3) : (f.z > 0 ? 2 : 0);  // 0 north, 1 east...
        if (isTurntable(held.id)) {
            place = (ItemId)(TURNTABLE + (look & 1));
        } else if (isRailSlope(held.id)) {
            place = (ItemId)(RAIL_SLOPE + look);  // rising the way the player looks
        } else if (isRailGentle(held.id)) {
            // Build 12: a gentle slope, four pieces to a block of height, rising the way the player looks;
            // each piece follows the one behind it (after the fourth, the track goes on one block higher).
            static constexpr int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
            const ItemId behind = w.getBlock(x - dx[look], y, z - dz[look]);
            int piece = 0;
            if (isRailGentle(behind) && (behind - RAIL_GENTLE) / 4 == look && (behind - RAIL_GENTLE) % 4 < 3) piece = (behind - RAIL_GENTLE) % 4 + 1;
            place = (ItemId)(RAIL_GENTLE + look * 4 + piece);
        } else if (isRailCross(held.id)) {
            place = held.id;  // 2.4.0: a crossing stays a crossing
        } else if (isRailSwitch(held.id)) {
            place = shapeSwitch(x, y, z, look);
        } else {
            place = shapeRail(x, y, z, look);
            // 2.3.0: flexible track between heights. A rail one block higher beside this one: this becomes a
            // slope up to it. The end of a track one block lower beside it: that end turns into a slope up to here.
            static constexpr int sdx[4] = {0, 1, 0, -1}, sdz[4] = {-1, 0, 1, 0};
            auto plainRail = [&](ItemId id) { int a, b; return !isRailSlope(id) && !isRailGentle(id) && TrainSystem::railSides(id, a, b); };
            int up = -1;
            for (int k = 0; k < 4; ++k) {
                const int d = (look + k) & 3;  // the way the player looks first
                if (up < 0 && isRail(w.getBlock(x + sdx[d], y + 1, z + sdz[d])) && !isRail(w.getBlock(x + sdx[d], y, z + sdz[d]))) up = d;
            }
            if (up >= 0) {
                place = (ItemId)(RAIL_SLOPE + up);
            } else {
                for (int k = 0; k < 4; ++k) {
                    const int d = (look + k + 2) & 3;  // behind the player first
                    const int nx = x + sdx[d], ny = y - 1, nz = z + sdz[d];
                    const ItemId below = w.getBlock(nx, ny, nz);
                    int a, b;
                    if (!plainRail(below) || isRail(w.getBlock(nx, y, nz)) || !TrainSystem::railSides(below, a, b)) continue;
                    const int towards = ((d + 2) & 3) * 2;  // the port of that rail facing this cell
                    if (a != towards && b != towards) continue;
                    w.setBlock(nx, ny, nz, (ItemId)(RAIL_SLOPE + ((d + 2) & 3)));
                    place = (d & 1) ? RAIL_EW : RAIL_NS;
                    break;
                }
            }
        }
    }
    if (!w.setBlock(x, y, z, place)) return;
    // A sapling grows into a tree after 20 seconds (5 with the fast growth experiment).
    if (isSapling(held.id)) saplings_.push_back({dimIndex(), x, y, z, settings_.expFastGrowth ? 5.0f : 20.0f});
    if (isCrop(held.id)) saplings_.push_back({dimIndex(), x, y, z, settings_.expFastGrowth ? 3.0f : 12.0f});  // wheat grows too
    if (isPowerBlock(held.id)) powerChanged(x, y, z);
    if (isBalloon(held.id)) achieve("anniv_party");
    if (survival()) state_.inventory.removeOne(state_.selectedSlot);
    hand_.place();  // only after the game really placed it
    ++blocksPlaced;
    styledSound(soundMaterial(place) == SoundMat::Stone || soundMaterial(place) == SoundMat::Glass ? Sound::PlaceStone : Sound::Place, place);
}

void Session::dropAt(int x, int y, int z, const ItemStack& stack) {
    // Straight into the inventory like in Pocket Edition; only what does not fit stays
    // in the world as a dropped item where the block was.
    int left = state_.inventory.add(stack);
    if (left > 0) {
        ItemStack rest = stack;
        rest.count = (uint8_t)left;
        drops_.spawn(dimIndex(), Vec3(x + 0.5f, y + 0.5f, z + 0.5f), {0, 2, 0}, rest);
    }
}

void Session::breakBlock(const RayHit& hit) {
    World& w = *world_;
    if (!hit.hit) return;
    if (canEdit && !canEdit(hit.x, hit.y, hit.z)) {
        protectedSpot();
        return;
    }
    ItemId id = w.getBlock(hit.x, hit.y, hit.z);
    if (breakTime(id) < 0) return;
    particles_.spawnBurst(w, hit.x, hit.y, hit.z, id);
    hand_.swing();
    // Fill with water again if this block touches water from the side or above.
    bool nearWater = w.dimension() == Dimension::Overworld && hit.y <= w.seaLevel() &&
                     (w.getBlock(hit.x + 1, hit.y, hit.z) == WATER || w.getBlock(hit.x - 1, hit.y, hit.z) == WATER ||
                      w.getBlock(hit.x, hit.y, hit.z + 1) == WATER || w.getBlock(hit.x, hit.y, hit.z - 1) == WATER ||
                      w.getBlock(hit.x, hit.y + 1, hit.z) == WATER);
    w.setBlock(hit.x, hit.y, hit.z, nearWater ? WATER : AIR);
    if (isLog(id)) scheduleLeafDecay(hit.x, hit.y, hit.z);
    if (isPowerBlock(id)) powerChanged(hit.x, hit.y, hit.z);
    // A plant standing on the broken block loses its ground and goes with it.
    if (ItemId above = w.getBlock(hit.x, hit.y + 1, hit.z); isPlant(above)) {
        w.setBlock(hit.x, hit.y + 1, hit.z, AIR);
        ItemId gives = isCrop(above) ? dropFor(above, 0.5f) : above;
        if (survival() && above != TALL_GRASS && gives != AIR) dropAt(hit.x, hit.y + 1, hit.z, {gives, 1, 0});
    }
    // 1.5.7: torches standing on the block or hanging on its sides fall off.
    {
        const int around[5][4] = {{0, 1, 0, TORCH}, {0, 0, 1, TORCH_WALL + 0}, {-1, 0, 0, TORCH_WALL + 1}, {0, 0, -1, TORCH_WALL + 2},
                                  {1, 0, 0, TORCH_WALL + 3}};
        for (const auto& a : around)
            if (w.getBlock(hit.x + a[0], hit.y + a[1], hit.z + a[2]) == (ItemId)a[3]) {
                w.setBlock(hit.x + a[0], hit.y + a[1], hit.z + a[2], AIR);
                if (survival()) dropAt(hit.x + a[0], hit.y + a[1], hit.z + a[2], {TORCH, 1, 0});
            }
    }
    if (settings_.vibrate && onVibrate) onVibrate(25);
    styledSound(digSound(soundMaterial(id)), id);  // 1.9.5: by material and style
    // A broken frame makes the whole portal disappear.
    const int d[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (auto& o : d)
        if (isPortal(w.getBlock(hit.x + o[0], hit.y + o[1], hit.z + o[2])))
            collapsePortal(w, hit.x + o[0], hit.y + o[1], hit.z + o[2]);

    if (isBed(id)) {
        static const int kDir[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        const int facing = (id - BED) / 2, sign = ((id - BED) & 1) ? -1 : 1;  // the head looks back to its foot
        const int ox = hit.x + kDir[facing][0] * sign, oz = hit.z + kDir[facing][1] * sign;
        if (isBed(w.getBlock(ox, hit.y, oz))) w.setBlock(ox, hit.y, oz, AIR);
    }
    if (isDoor(id)) {
        // The other half goes too.
        int other = doorState(id).upper ? hit.y - 1 : hit.y + 1;
        if (isDoor(w.getBlock(hit.x, other, hit.z))) w.setBlock(hit.x, other, hit.z, AIR);
    }
    if (isChest(id) || id == BARREL || id == LOOT_BARREL_CAMP) {
        FurnaceKey key{dimIndex(), hit.x, hit.y, hit.z};
        if (id != CHEST && id != BARREL && !chests_.count(key)) {  // an unopened structure chest still spills its loot
            uint32_t seed = info_.seed ^ (uint32_t)(key.x * 73856093) ^ (uint32_t)(key.y * 19349663) ^ (uint32_t)(key.z * 83492791);
            chests_[key] = structureLoot(id, seed);
        }
        if (ChestContents* c = chest(key))
            for (const ItemStack& s : *c)
                if (!s.empty()) drops_.spawn(dimIndex(), Vec3(hit.x + 0.5f, hit.y + 0.6f, hit.z + 0.5f), {0, 2.5f, 0}, s);
        chests_.erase(key);
    }
    if (isFurnace(id)) {
        FurnaceKey key{dimIndex(), hit.x, hit.y, hit.z};
        if (Furnace* f = furnace(key)) {
            if (survival())
                for (const ItemStack* s : {&f->input, &f->fuel, &f->output})
                    if (!s->empty()) dropAt(hit.x, hit.y, hit.z, *s);
            furnaces_.erase(key);
        }
    }

    if (survival()) {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        ItemId tool = state_.inventory.slots[state_.selectedSlot].id;
        ItemId drop = dropFor(id, (rng_ % 10000) / 10000.0f, tool);
        if (drop != AIR) dropAt(hit.x, hit.y, hit.z, {drop, (uint8_t)(isDoubleSlab(id) ? 2 : 1), 0});
        if (id == WHEAT_RIPE) {
            dropAt(hit.x, hit.y, hit.z, {WHEAT_SEEDS, (uint8_t)(1 + rng_ % 2), 0});  // seeds to plant again
            achieve("farmer");
        }
        addExhaustion(0.005f);
        if (toolKind(tool) != ToolKind::None) damageHeldTool();
        if (onBlockMined) onBlockMined();
    }
}

void Session::updateMining(float dt, double time) {
    if (state_.inventory.slots[state_.selectedSlot].id == BOW) {
        // Drawing the bow while a finger rests on the world (it takes a second to draw fully).
        bool holding = false;
        for (const Touch& t : touches_) holding |= t.id >= 0 && t.kind == Touch::Look && time - t.startTime > 0.12;
        bowCharge_ = holding ? std::min(1.0f, bowCharge_ + dt) : 0;
        if (forcedBow_ >= 0) bowCharge_ = forcedBow_;
        if (holding) miningNow_ = false;
        return;
    }
    bowCharge_ = 0;
    for (Touch& t : touches_) {
        if (t.id < 0 || t.kind != Touch::Look || t.moved) continue;
        // 1.5.0 archaeology: the brusher slowly sweeps the debris gravel away from a find.
        if (state_.inventory.slots[state_.selectedSlot].id == BRUSHER && time - t.startTime >= kSurvivalDelay) {
            const RayHit hit = pick(t.x, t.y);
            if (hit.hit && world_->getBlock(hit.x, hit.y, hit.z) == DEBRIS_GRAVEL) {
                if (!target_.hit || hit.x != target_.x || hit.y != target_.y || hit.z != target_.z) brushProgress_ = 0;
                target_ = hit;
                miningNow_ = true;
                if ((chipTimer_ += dt) >= 0.15f) {
                    chipTimer_ = 0;
                    particles_.spawnChips(*world_, hit.x, hit.y, hit.z, hit.nx, hit.ny, hit.nz, GRAVEL, 2);
                    sound(Sound::Dig);
                }
                brushProgress_ += dt / (survival() ? kBrushTime : kBrushTime / 2);
                if (brushProgress_ >= 1) {
                    uncoverFind(hit.x, hit.y, hit.z);
                    brushProgress_ = 0;
                    target_ = {};
                }
                return;
            }
        }
        // Hitting a boat breaks it (the player inside gets out first).
        if (time - t.startTime >= kSurvivalDelay) {
            int boat = pickBoat(t.x, t.y);
            if (boat >= 0) {
                miningNow_ = true;
                Boat& b = boats_.boats()[boat];
                b.damage += dt * (survival() ? 1.0f : 4.0f);
                if (b.damage >= BoatSystem::kBreakTime) {
                    if (riding_ == boat) exitBoat();
                    if (riding_ > boat) --riding_;
                    Vec3 at = b.pos;
                    boats_.remove(boat);
                    if (survival()) dropAt((int)std::floor(at.x), (int)std::floor(at.y), (int)std::floor(at.z), {BOAT, 1, 0});
                    hand_.swing();
                    sound(Sound::Dig);
                }
                return;
            }
        }
        if (!survival()) {
            if (time - t.startTime >= kHoldToBreak * 0.5) miningNow_ = pick(t.x, t.y).hit;
            if (time - t.startTime >= kHoldToBreak * 0.5 && (chipTimer_ += dt) >= 0.08f) {
                // Chips while the finger rests, before the block pops.
                chipTimer_ = 0;
                RayHit h = pick(t.x, t.y);
                if (h.hit)
                    particles_.spawnChips(*world_, h.x, h.y, h.z, h.nx, h.ny, h.nz, world_->getBlock(h.x, h.y, h.z), 2);
            }
            if (time - t.startTime >= kHoldToBreak && time - t.lastBreak >= kBreakRepeat) {
                target_ = pick(t.x, t.y);
                breakBlock(target_);
                t.lastBreak = time;
            }
            return;
        }
        if (time - t.startTime < kSurvivalDelay) return;
        RayHit hit = pick(t.x, t.y);
        if (!hit.hit || !target_.hit || hit.x != target_.x || hit.y != target_.y || hit.z != target_.z) breakProgress_ = 0;
        target_ = hit;
        if (!hit.hit) return;
        ItemId block = world_->getBlock(hit.x, hit.y, hit.z);
        float bt = breakTime(block, state_.inventory.slots[state_.selectedSlot].id);
        if (bt < 0) return;
        if (settings_.devInstantMine) bt = 0.05f;
        miningNow_ = true;
        // Chips fly off the face while digging: a few right away, then steadily.
        if ((digHitTimer_ -= dt) <= 0) {
            // 2.0.0: each hit of the tool sounds like the block being dug.
            digHitTimer_ = 0.27f;
            styledSound(digSound(soundMaterial(block)), block);
        }
        if (breakProgress_ == 0) {
            particles_.spawnChips(*world_, hit.x, hit.y, hit.z, hit.nx, hit.ny, hit.nz, block, 4);
            chipTimer_ = 0;
        } else if ((chipTimer_ += dt) >= 0.08f) {
            chipTimer_ = 0;
            particles_.spawnChips(*world_, hit.x, hit.y, hit.z, hit.nx, hit.ny, hit.nz, block, 2);
        }
        breakProgress_ += dt / bt;
        if (breakProgress_ >= 1) {
            breakBlock(hit);
            breakProgress_ = 0;
            target_ = {};
        }
        return;
    }
}

// ---------------------------------------------------------------- survival

int Session::armorPoints() const {
    int points = 0;
    for (const ItemStack& a : state_.armor) points += a.empty() ? 0 : vc::armorPoints(a.id);
    return points;
}

bool Session::equipArmor(int slot) {
    ItemStack& held = state_.inventory.slots[slot];
    int piece = armorPiece(held.id);
    if (piece < 0) return false;
    std::swap(held, state_.armor[piece]);
    hand_.use();
    sound(Sound::Place);
    return true;
}

void Session::hurt(float amount, bool ignoreCooldown, bool armorHelps) {
    if (!survival() || dead_ || settings_.devGodMode) return;
    if (hurtCooldown_ > 0 && !ignoreCooldown) return;
    if (armorHelps && amount > 0) {
        amount *= 1 - armorReduction(state_.armor.data());
        // Every piece that took part wears a little and breaks at its durability.
        for (ItemStack& a : state_.armor)
            if (!a.empty() && ++a.damage >= toolDurability(a.id)) a = {};
    }
    state_.health = std::max(0.0f, state_.health - amount);
    sound(Sound::Hurt);
    hurtCooldown_ = 0.5f;
    hurtFlash_ = 0.35f;
    if (state_.health <= 0) {
        dead_ = true;
        if (world_ == &overworld_) {  // a skull on the map where it happened
            state_.hasDeath = true;
            state_.deathX = (int)std::floor(player_.pos.x);
            state_.deathZ = (int)std::floor(player_.pos.z);
        }
        cancelTouches();
    }
}

void Session::updateSurvival(float dt, bool jumped) {
    hurtCooldown_ = std::max(0.0f, hurtCooldown_ - dt);
    hurtFlash_ = std::max(0.0f, hurtFlash_ - dt);
    if (!survival()) {
        state_.air = kMaxAir;
        return;
    }

    if (world_ == &celestial_) {
        // Low gravity: falls hurt from three times higher, not at all with the celestium chestplate.
        player_.landedFall = state_.armor[1].id == CELESTIUM_CHESTPLATE ? 0 : player_.landedFall / 3;
    }
    if (player_.landedFall > 0 &&
        (world_->getBlock((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.y - 0.05f), (int)std::floor(player_.pos.z)) == HONEY_BLOCK ||
         world_->getBlock((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.y - 0.05f), (int)std::floor(player_.pos.z)) == AMBER_RESIN))
        player_.landedFall = 0;  // 1.9.5 build 3: honey catches every fall; 2.2.0: amber resin too
    if (player_.landedFall > 3) hurt(std::ceil(player_.landedFall - 3), true, true);
    {
        // 2.2.0: thin ice holds a moment, then gives way under whoever stands on it.
        const int ix = (int)std::floor(player_.pos.x), iy = (int)std::floor(player_.pos.y - 0.05f), iz = (int)std::floor(player_.pos.z);
        if (player_.onGround && world_->getBlock(ix, iy, iz) == THIN_ICE) {
            if ((thinIceTimer_ += dt) > 1.1f) {
                thinIceTimer_ = 0;
                particles_.spawnBurst(*world_, ix, iy, iz, THIN_ICE);
                const bool water = world_->getBlock(ix, iy - 1, iz) == WATER;
                world_->setBlock(ix, iy, iz, water ? (ItemId)WATER : (ItemId)AIR);
                sound(Sound::Dig);
            }
        } else {
            thinIceTimer_ = 0;
        }
    }
    if (player_.inLava) hurt(4, false, true);
    if (player_.touches(*world_, FIRE) || player_.touches(*world_, CAMPFIRE)) {
        hurt(1, false, true);  // 1.9.0: burns
        burning_ = 3;
    }
    if (burning_ > 0) {
        // Still on fire for a moment after stepping out, unless in water.
        burning_ = player_.eyeInWater(*world_) || player_.inWater ? 0 : burning_ - dt;
        if (burning_ > 0 && std::fmod(burning_, 1.0f) < dt) hurt(1, true, true);
    }
    // 2.5.0: the cactus is thinner than a block (its collision box is inset), so the player stopped
    // against it before reaching the block: look a little further out.
    if (player_.touches(*world_, CACTUS, 0.1f)) {
        // Prickly. 1.9.0 build 3: a real hit: it pushes the player back, so it is felt every time.
        if (hurtCooldown_ <= 0) {
            const int px = (int)std::floor(player_.pos.x), py = (int)std::floor(player_.pos.y), pz = (int)std::floor(player_.pos.z);
            Vec3 away;
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dz = -1; dz <= 1; ++dz)
                        if (world_->getBlock(px + dx, py + dy, pz + dz) == CACTUS) away = away - Vec3((float)dx, 0, (float)dz);
            if (away.length() > 0) player_.vel = player_.vel + away.normalized() * 4.0f + Vec3(0, 2.5f, 0);
        }
        hurt(1);
        achieve("cactus");
    }
    if (player_.pos.y < -20) hurt(4);
    updateTemperature(dt);

    // Breath
    if (player_.eyeInWater(*world_)) {
        state_.air = std::max(0.0f, state_.air - dt);
        if (state_.air <= 0 && (drownTimer_ += dt) >= 1) {
            drownTimer_ = 0;
            hurt(2, true);
        }
    } else {
        state_.air = std::min(kMaxAir, state_.air + dt * 5);
        drownTimer_ = 0;
    }

    // Hunger, following the classic exhaustion rules. Classic 0.0.3 had no food: always full.
    if (kClassicEdition) state_.food = kMaxFood, state_.saturation = 5, exhaustion_ = 0;
    if (player_.onGround) addExhaustion(player_.walked * 0.01f);
    if (jumped) addExhaustion(0.05f);
    while (exhaustion_ >= 4) {
        exhaustion_ -= 4;
        if (state_.saturation > 0) state_.saturation = std::max(0.0f, state_.saturation - 1);
        else state_.food = std::max(0.0f, state_.food - 1);
    }
    if (state_.food >= 18 && state_.health < kMaxHealth) {
        if ((regenTimer_ += dt) >= 4) {
            regenTimer_ = 0;
            state_.health = std::min(kMaxHealth, state_.health + 1);
            addExhaustion(6);
        }
    } else {
        regenTimer_ = 0;
    }
    if (state_.food <= 0) {
        if ((starveTimer_ += dt) >= 4) {
            starveTimer_ = 0;
            if (state_.health > 1) hurt(1, true);  // normal difficulty stops at half a heart
        }
    } else {
        starveTimer_ = 0;
    }
}

// ---------------------------------------------------------------- portals

void Session::updatePortal(float dt) {
    // 1.7.0: the Celestial portal, between the Crimson Void and the Celestial Reach.
    portalCelestial_ = player_.touchesBlock(*world_, CELESTIAL_PORTAL);
    if (portalCelestial_) {
        if (portalCooldown_) return;
        portalTime_ += dt;
        if (portalTime_ >= kPortalDelay) travelCelestial();
        return;
    }
    portalVoid_ = player_.touchesBlock(*world_, VOID_PORTAL);
    if (!portalVoid_ && !player_.touchesBlock(*world_, PORTAL)) {
        portalTime_ = 0;
        portalCooldown_ = false;
        return;
    }
    if (portalCooldown_) return;
    portalTime_ += dt;
    if (portalTime_ >= kPortalDelay) travelThroughPortal();
}

void Session::checkAchievements() {
    if (!survival() || !onAchievement) return;
    static const std::pair<ItemId, const char*> kItems[] = {
        {CRAFTING_TABLE, "crafting_table"}, {STONE_PICKAXE, "stone_pickaxe"}, {IRON_INGOT, "iron_ingot"},
        {BRONZE_INGOT, "bronze_ingot"}, {STEEL_INGOT, "steel_ingot"}, {RUBY, "ruby"}, {MAP, "cartographer"},
        {ANNIV_MEDAL, "anniv_collector"}};
    for (const ItemStack& s : state_.inventory.slots)
        for (auto [id, name] : kItems)
            if (s.id == id) achieve(name);
    bool full = true;
    for (const ItemStack& a : state_.armor) full = full && !a.empty();
    if (full) achieve("full_armor");
    if (world_ == &nether_ && nether_.wastelandAt((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.z))) achieve("wasteland");
    if (world_ == &overworld_) {
        const BiomeDef* b = world_->biomeAt((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.z));
        if (b && (b->id == "snowy_taiga" || b->id == "ice_plains")) achieve("snow");

    }
}

void Session::travelCelestial() {
    exitBoat();
    if (world_ != &celestial_) {
        // Up to the Celestial Reach: the big island round (0, 0), with the way back in its middle.
        celestialReturn_ = player_.pos;
        hasCelestialReturn_ = world_ == &void_;
        celestial_.ensureChunksAround(0, 0, 3, 64);
        const int top = std::max(10, celestial_.celestialTop(0, 4));
        for (int dx = -2; dx <= 2; ++dx)
            for (int dz = -2; dz <= 2; ++dz) {
                const bool corner = std::abs(dx) == 2 && std::abs(dz) == 2, inside = std::abs(dx) <= 1 && std::abs(dz) <= 1;
                celestial_.setBlock(dx, top, 4 + dz, corner ? CELESTIAL_STONE_BRICKS : inside ? CELESTIAL_PORTAL : CELESTIAL_FRAME_FULL);
                for (int dy = 1; dy <= 3; ++dy) celestial_.setBlock(dx, top + dy, 4 + dz, AIR);
            }
        const int sy = std::max(top, celestial_.celestialTop(0, 0));
        player_.pos = Vec3(0.5f, (float)sy + 1.01f, 0.5f);
        world_ = &celestial_;
        achieve("celestial_reach");
    } else {
        // Back down to the Crimson Void, beside the frame the player came through.
        Vec3 to = celestialReturn_;
        if (!hasCelestialReturn_) {
            int fx = 0, fz = 0;
            void_.nearestCelestialFrame(0, 0, fx, fz);
            to = Vec3(fx + 0.5f, 0, fz + 0.5f);
        }
        to.z += 3;
        const int tx = (int)std::floor(to.x), tz = (int)std::floor(to.z);
        void_.ensureChunksAround(floorDiv(tx, kChunkSize), floorDiv(tz, kChunkSize), 2, 64);
        player_.pos = Vec3(to.x, (float)void_.skyHeight(tx, tz) + 0.01f, to.z);
        world_ = &void_;
    }
    player_.vel = {};
    player_.fallDistance = 0;
    target_ = {};
    breakProgress_ = 0;
    portalTime_ = 0;
    portalCooldown_ = true;
    travelled = true;
}

void Session::travelThroughPortal() {
    exitBoat();
    // One block in the Nether equals eight in the Overworld.
    // Void portals join the Nether and the Crimson Void block for block.
    World* target = portalVoid_ ? (world_ == &void_ ? &nether_ : &void_) : (world_ == &overworld_ ? &nether_ : &overworld_);
    float factor = portalVoid_ ? 1.0f : target == &nether_ ? 1.0f / 8 : 8.0f;
    int tx = (int)std::floor(player_.pos.x * factor), tz = (int)std::floor(player_.pos.z * factor);
    target->ensureChunksAround(floorDiv(tx, kChunkSize), floorDiv(tz, kChunkSize), 3, 64);
    player_.pos = findOrBuildPortal(*target, tx, tz, portalVoid_);
    player_.vel = {};
    player_.fallDistance = 0;
    world_ = target;
    target_ = {};
    breakProgress_ = 0;
    portalTime_ = 0;
    portalCooldown_ = true;
    if (world_ == &nether_) achieve("nether");
    if (world_ == &void_) achieve("crimson_void");
    travelled = true;
}

// ---------------------------------------------------------------- frame

void Session::update(float dt, double time, float guiW, float guiH, const Settings& settings) {
    if (kFeatures250 && world_ == &overworld_ && (int)(time * 2) != (int)((time - dt) * 2)) {
        // 2.5.0: a subway station the player stands in (deep down near its entrance) counts as found.
        int ex, ez, kind;
        if (player_.pos.y < World::kMetroY + 12 && overworld_.nearestMetroStation((int)player_.pos.x, (int)player_.pos.z, ex, ez, kind) &&
            std::hypot(ex - player_.pos.x, ez - player_.pos.z) < 50) {
            bool known = false;
            for (const auto& v : state_.metroVisited) known = known || (v.first == ex && v.second == ez);
            if (!known) {
                state_.metroVisited.emplace_back(ex, ez);
                if (onMessage) onMessage(trf("Nová stanice metra: {0}", {overworld_.metroStationName(ex, ez)}));
            }
        }
    }
    guiW_ = guiW;
    guiH_ = guiH;
    fov_ = settings.fov;
    sensitivity_ = settings.sensitivity;
    settings_ = settings;
    player_.autoJump = settings.autoJump;
    // Developer modes (Game passes them only for the developer account).
    // 1.9.5 build 3: honey is sticky underfoot.
    const bool onHoney = world_->getBlock((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.y - 0.05f), (int)std::floor(player_.pos.z)) == HONEY_BLOCK;
    player_.speedMul = (onHoney ? 0.4f : 1.0f) * settings.devSpeed * (survival() && temperatureLevel() <= -1 ? (temperatureLevel() == -2 ? 0.78f : 0.9f) : 1.0f);
    player_.gravityMul = settings.expLowGravity ? 0.3f : 1.0f;
    gliding_ = false;
    if (state_.armor[1].id == CELESTIAL_WINGS && !player_.onGround && !player_.flying && !player_.inWater && player_.vel.y < -2.0f) {
        // 1.9.5: the celestial wings: falling turns into a glide along the look.
        gliding_ = true;
        const Vec3 look = player_.forward();
        const Vec3 flat = Vec3(look.x, 0, look.z).normalized();
        const float speed = 9.0f - look.y * 6.0f;  // diving is faster, looking up slows down
        player_.vel.x += (flat.x * speed - player_.vel.x) * std::min(1.0f, dt * 2.5f);
        player_.vel.z += (flat.z * speed - player_.vel.z) * std::min(1.0f, dt * 2.5f);
        player_.vel.y = std::max(player_.vel.y, -2.2f + std::min(0.0f, look.y) * 5.0f);
        player_.fallDistance = 0;
    }
    if (world_ == &celestial_) {
        // 1.9.0: low gravity in the Celestial Reach; the celestium chestplate steadies it further.
        const bool plate = state_.armor[1].id == CELESTIUM_CHESTPLATE;
        player_.gravityMul = std::min(player_.gravityMul, plate ? 0.38f : 0.45f);
    }
    player_.noclip = settings.devNoclip;
    if (survival() && player_.flying && !settings.devFly) player_.flying = false;
    if (oldMode()) {
        // 1.8.0 Old World: creative only, the nine blocks of 0.3 in the hotbar, no flying and
        // the old jump (no auto-jump).
        static const ItemId kOldBlocks[kHotbarSize] = {GRASS, COBBLESTONE, PLANKS, LOG, GLASS, BRICKS, NETHERRACK, OBSIDIAN, FLINT_AND_STEEL};
        // Classic 0.0.3: survival too (hearts only, no food); its hotbar holds what the player gathers.
        if (!kClassicEdition) info_.mode = GameMode::Creative;
        if (!survival() && !kClassicEdition)
            for (int i = 0; i < kHotbarSize; ++i) state_.inventory.slots[i] = ItemStack{kOldBlocks[i], 1};
        if (kClassicEdition && !survival() && state_.inventory.slots[0].empty() && !classicStarted_) {
            classicStarted_ = true;  // a new creative world starts with the nine blocks of 0.3; the inventory changes them
            for (int i = 0; i < kHotbarSize; ++i) state_.inventory.slots[i] = ItemStack{kOldBlocks[i], 1};
        }
        if (!kClassicEdition || survival()) player_.flying = false;
        player_.autoJump = false;
        player_.speedMul = 1;
        player_.noclip = false;
    }
    if (dt > 0) fps_ += (1 / dt - fps_) * std::min(1.0f, dt * 4);
    if (dead_) return;

    float moveX = 0, moveZ = 0;
    bool jump = false, descend = false;
    const Layout lay = layout();
    for (Touch& t : touches_) {
        if (t.id < 0) continue;
        if (t.kind == Touch::DPad) {
            // Every finger on the pad adds its direction (e.g. up + left = diagonal).
            int dx, dz;
            dpadDirection(lay, t.x, t.y, dx, dz);
            moveX += (float)dx;
            moveZ += (float)dz;
        } else if (t.kind == Touch::Joystick) {
            // 1.8.0 Old World: an invisible joystick where the finger first touched.
            const float r = 26;
            moveX += std::clamp((t.x - t.startX) / r, -1.0f, 1.0f);
            moveZ += std::clamp(-(t.y - t.startY) / r, -1.0f, 1.0f);
        } else if (t.kind == Touch::Jump) {
            jump = true;
        } else if (t.kind == Touch::FlyDown) {
            descend = true;
        }
    }
    // Scripted input (demos, the trailer tool) adds to the touch controls.
    moveX += autopilot.moveX;
    moveZ += autopilot.moveZ;
    jump = jump || autopilot.jump;
    moveX = std::clamp(moveX, -1.0f, 1.0f);
    moveZ = std::clamp(moveZ, -1.0f, 1.0f);
    if (moveX != 0 && moveZ != 0) {
        // Diagonals are not faster than straight lines.
        moveX *= 0.7071f;
        moveZ *= 0.7071f;
    }
    miningNow_ = false;
    updateMining(dt, time);

    int pcx = floorDiv((int)std::floor(player_.pos.x), kChunkSize);
    int pcz = floorDiv((int)std::floor(player_.pos.z), kChunkSize);
    bool jumped = false;
    // Only simulate the player once the ground below exists.
    // 2.1.0 preview: the trains move; a player standing in one rides along, a seated one sits still in it.
    if (world_->config().devMap == 3 && dimIndex() == 0 && trains_.trains().empty() && isRail(world_->getBlock(20, 5, 2))) {
        // 2.5.0: the metro lab starts with a subway of three cars on the loop, driving itself.
        if (trains_.place(*world_, 0, CarKind::SubwayLocomotive, 20, 5, 2, 1)) {
            trains_.place(*world_, 0, CarKind::SubwayCar, 20, 5, 2, 1);
            trains_.place(*world_, 0, CarKind::SubwayCar, 20, 5, 2, 1);
            if (kFeatures250) trains_.trains().back().autoMode = true;
        }
    }
    trains_.update(*world_, dimIndex(), dt, player_.pos, player_.yaw, trainSeatTrain_ >= 0);
    // Build 5: the drill car bores the tunnel: the next cell past the end of the track is cut
    // 3 wide and 4 high, a floor goes under it where there is none, and a straight rail goes on it.
    trains_.infiniteRails = !survival();
    for (const auto& r : trains_.drillRequests()) {
        static constexpr int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
        World& w = *world_;
        if (r[3] & 1) continue;  // build 12: the drill bores only straight along a side
        const int side = (r[3] >> 1) & 3, nx = r[0] + dx[side], ny = r[1], nz = r[2] + dz[side];
        const int px = dz[side] != 0 ? 1 : 0, pz = dx[side] != 0 ? 1 : 0;  // across the track
        bool cut = false;
        for (int k = -1; k <= 1; ++k)
            for (int h = 0; h < 4; ++h) {
                const int bx = nx + px * k, by = ny + h, bz = nz + pz * k;
                const ItemId b = w.getBlock(bx, by, bz);
                if (b == AIR || b == BEDROCK || isRail(b) || isBufferStop(b) || by >= kChunkHeight) continue;
                if (b == WATER || b == LAVA) {
                    w.setBlock(bx, by, bz, h == 0 && k == 0 ? COBBLESTONE : b);
                    continue;
                }
                if (cut == false || fireRandom() < 0.3f) particles_.spawnBurst(w, bx, by, bz, b);
                w.setBlock(bx, by, bz, AIR);
                cut = true;
            }
        if (w.getBlock(nx, ny, nz) == BEDROCK) continue;
        // 2.3.0: it walls up every hole round the tunnel: the floor under all three, both sides and the roof.
        auto seal = [&](int bx, int by, int bz) {
            const ItemId b = w.getBlock(bx, by, bz);
            if (b == AIR || isLiquid(b) || isPlant(b)) w.setBlock(bx, by, bz, COBBLESTONE);
        };
        for (int k = -1; k <= 1; ++k) seal(nx + px * k, ny - 1, nz + pz * k), seal(nx + px * k, ny + 4, nz + pz * k);
        for (int h = 0; h < 4; ++h) seal(nx - px * 2, ny + h, nz - pz * 2), seal(nx + px * 2, ny + h, nz + pz * 2);
        // The rail ahead, out of the train's own load.
        if (r[4] >= 0 && r[4] < (int)trains_.trains().size()) {
            Train& tr = trains_.trains()[(size_t)r[4]];
            if (!trains_.infiniteRails && tr.rails <= 0) continue;
            if (!trains_.infiniteRails) --tr.rails;
        }
        w.setBlock(nx, ny, nz, side == 0 || side == 2 ? RAIL_NS : RAIL_EW);
        if (cut) sound(Sound::Dig);
    }
    trains_.drillRequests().clear();
    // Build 3: crossing signals flash and gates come down while a train is near; chime and horn
    // makers sound as a train goes by.
    railwayTimer_ -= dt;
    if (railwayTimer_ <= 0) {
        railwayTimer_ = 0.25f;
        railwayFlash_ = !railwayFlash_;
        World& w = *world_;
        std::set<std::tuple<int, int, int>> near;
        for (const Train& t : trains_.trains()) {
            if (t.dim != dimIndex()) continue;
            for (const TrainCar& c : t.cars) {
                const int cx = (int)std::floor(c.pos.x), cy = (int)std::floor(c.pos.y), cz = (int)std::floor(c.pos.z);
                for (int dx = -10; dx <= 10; ++dx)
                    for (int dz = -10; dz <= 10; ++dz)
                        for (int dy = -2; dy <= 3; ++dy) {
                            const ItemId b = w.getBlock(cx + dx, cy + dy, cz + dz);
                            if (isCrossingSignal(b) || isCrossingGate(b)) near.insert({cx + dx, cy + dy, cz + dz});
                            if ((b == CHIME_MAKER || b == HORN_MAKER) && std::abs(dx) <= 2 && std::abs(dz) <= 2) {
                                const FurnaceKey key{dimIndex(), cx + dx, cy + dy, cz + dz};
                                auto it = railwaySounds_.find(key);
                                if (it == railwaySounds_.end() || clock_ - it->second > 4.0) {
                                    railwaySounds_[key] = clock_;
                                    sound(b == CHIME_MAKER ? Sound::CrossingChime : Sound::TrainHorn);
                                }
                            }
                        }
            }
        }
        for (const auto& [x, y, z] : near) {
            const ItemId b = w.getBlock(x, y, z);
            if (isCrossingSignal(b)) {
                const int f = (b - CROSSING_SIGNAL) / 3;
                w.setBlock(x, y, z, (ItemId)(CROSSING_SIGNAL + f * 3 + (railwayFlash_ ? 1 : 2)));
            } else {
                const int f = (b - CROSSING_GATE) / 2;
                w.setBlock(x, y, z, (ItemId)(CROSSING_GATE + f * 2 + 1));
            }
        }
        // Those no train is near any more go dark and open again.
        for (const auto& p : railwayActive_)
            if (!near.count(p)) {
                const auto& [x, y, z] = p;
                const ItemId b = w.getBlock(x, y, z);
                if (isCrossingSignal(b)) w.setBlock(x, y, z, (ItemId)(CROSSING_SIGNAL + (b - CROSSING_SIGNAL) / 3 * 3));
                else if (isCrossingGate(b)) w.setBlock(x, y, z, (ItemId)(CROSSING_GATE + (b - CROSSING_GATE) / 2 * 2));
            }
        railwayActive_ = near;
    }
    // Build 3: smoke from the locomotive's chimney, thicker the harder it pulls.
    smokeTimer_ -= dt;
    if (smokeTimer_ <= 0) {
        smokeTimer_ = 0.12f;
        for (const Train& t : trains_.trains()) {
            if (t.dim != dimIndex()) continue;
            for (const TrainCar& c : t.cars) {  // build 5: every locomotive smokes, wherever it is in the train
            if (c.kind != CarKind::Locomotive) continue;
            const Vec3 d = c.pos - player_.pos;
            if (d.x * d.x + d.z * d.z > 64 * 64) continue;
            const float cs = std::cos(c.yaw), sn = std::sin(c.yaw);
            const float lz = 0.95f * TrainSystem::kScale;  // the chimney, on the boiler in front of the cab
            const Vec3 top = c.pos + Vec3(-lz * sn, 2.55f * TrainSystem::kScale, lz * cs);
            const int puffs = 1 + (int)(std::fabs(t.throttle) * 2);
            for (int k = 0; k < puffs; ++k) {
                const Vec3 drift(-std::fabs(t.speed) * 0.25f * -sn * 0 + (hash3((int)(clock_ * 50), k, 1, 9) % 100 - 50) / 200.0f, 1.4f,
                                 (hash3((int)(clock_ * 50), k, 2, 9) % 100 - 50) / 200.0f);
                particles_.spawnAmbient(*world_, top, drift, GRAVEL, 2.2f, 0.32f, -0.35f, 1.0f);
            }
            }
        }
    }
    if (trainSeatTrain_ >= 0 && (trainSeatTrain_ >= (int)trains_.trains().size() ||
                                 trainSeatCar_ >= (int)trains_.trains()[(size_t)trainSeatTrain_].cars.size()))
        trainSeatTrain_ = -1;
    if (seated_ && world_->getBlock(seatX_, seatY_, seatZ_) != CUSHION) seated_ = false;  // the cushion is gone
    if (trainSeatTrain_ >= 0) {
        const TrainCar& car = trains_.trains()[(size_t)trainSeatTrain_].cars[(size_t)trainSeatCar_];
        boats_.update(*world_, dimIndex(), dt, -1, 0, 0, 0);
        player_.pos = trains_.seatPos(car, trainSeat_);
        player_.vel = {};
        player_.fallDistance = 0;
        player_.onGround = true;
        player_.walked = 0;
    } else if (seated_) {
        // On a cushion: the player sits still on it and can only look around.
        boats_.update(*world_, dimIndex(), dt, -1, 0, 0, 0);
        player_.pos = Vec3(seatX_ + 0.5f, seatY_ + 0.1f, seatZ_ + 0.5f);
        player_.vel = {};
        player_.fallDistance = 0;
        player_.onGround = true;
        player_.walked = 0;
    } else if (riding_ >= 0) {
        // In a boat: the D-pad rows the boat and the player sits in it.
        boats_.update(*world_, dimIndex(), dt, riding_, moveZ, moveX, player_.yaw);
        const Boat& b = boats_.boats()[riding_];
        Vec3 before = player_.pos;
        player_.pos = b.pos + Vec3(0, 0.15f, 0);
        player_.vel = {};
        player_.fallDistance = 0;
        player_.onGround = true;
        player_.walked = Vec3(player_.pos.x - before.x, 0, player_.pos.z - before.z).length();
    } else {
        boats_.update(*world_, dimIndex(), dt, -1, 0, 0, 0);
        if (world_->getChunk(pcx, pcz)) {
            jumped = jump && player_.onGround && !player_.flying;
            // Classic 0.0.3: the old water now and then held the player fast for a moment, as it did back then.
            if (kClassicEdition && player_.inWater) {
                if (waterStuck_ <= 0 && fireRandom() < dt * 0.12f) waterStuck_ = 0.6f + fireRandom() * 0.8f;
                if (waterStuck_ > 0) waterStuck_ -= dt, moveX = moveZ = 0, jump = false;
            } else {
                waterStuck_ = 0;
            }
            player_.update(*world_, dt, moveX, moveZ, jump, descend);
            // 2.1.0 preview: the cars are solid; walk in by their doors, stand on their floors.
            if (!player_.flying && trains_.collide(dimIndex(), player_.pos, player_.vel)) {
                player_.onGround = true;
                player_.fallDistance = 0;
            }
        }
    }
    if (!survival() && player_.pos.y < -20) {
        player_.pos = state_.spawn;
        player_.vel = {};
        world_ = &overworld_;
    }
    updateSurvival(dt, jumped);
    updatePortal(dt);
    bool walking = player_.onGround && player_.walked > 0.001f && !player_.flying;
    if (walking && (stepDistance_ += player_.walked) > 1.7f) {
        stepDistance_ = 0;
        // 1.9.5: the step sounds like what is underfoot.
        // The block the feet stand on (when the feet are a little above it, the one below that;
        // empty air used to count as stone, so steps on grass mixed in stone steps).
        const int fx = (int)std::floor(player_.pos.x), fz = (int)std::floor(player_.pos.z);
        ItemId under = world_->getBlock(fx, (int)std::floor(player_.pos.y - 0.05f), fz);
        if (!isSolid(under)) under = world_->getBlock(fx, (int)std::floor(player_.pos.y - 0.6f), fz);
        if (!isSolid(under)) under = GRASS;
        const ItemId on = world_->getBlock((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.y + 0.05f), (int)std::floor(player_.pos.z));
        const ItemId surface = on == SNOW_LAYER || on == LEAF_LITTER || isCarpet(on) ? on : under;
        styledSound(player_.inWater ? Sound::StepWet : stepSound(soundMaterial(surface)), surface);
    }
    updateCamera(dt, walking);
    {
        const ItemStack& held = state_.inventory.slots[state_.selectedSlot];
        hand_.update(dt, dt > 0 ? player_.walked / dt : 0, player_.onGround && !player_.flying, miningNow_,
                     held.empty() ? (ItemId)AIR : held.id, state_.selectedSlot);
    }
    updateFurnaces(dt);
    if ((achievementTimer_ += dt) >= 1) {
        achievementTimer_ = 0;
        checkAchievements();
    }
    updateAmbientParticles(dt);
    particles_.update(*world_, dt);
    clock_ += dt;
    updateWeather(dt);
    updateMobs(dt);
    updateArrows(dt);
    updateFireballs(dt);
    updateBlocksOverTime(dt);
    for (Mob& m : mobs_.mobs()) m.love = std::max(0.0f, m.love - dt);
    updateExperiments20(dt);
    if (kFeatures220 && !oldMode()) updateBronzeAndGrass(dt);  // 2.2.0 preview
    if (kClassicEdition) updateDoppel(dt);  // Classic 0.0.3: the legend of the Double
    if (!oldMode() && settings_.season == Season::NewYear && world_ == &overworld_ && dayLight_ < 0.6f && (fireworkTimer_ -= dt) <= 0) {
        // 1.9.0 New Year's Eve: fireworks burst over the player all night long.
        auto rnd = [&] { fireworkSeed_ = fireworkSeed_ * 1664525u + 1013904223u; return (fireworkSeed_ >> 8) / 16777216.0f; };
        fireworkTimer_ = 0.6f + rnd() * 1.2f;
        // In the sky ahead of the player, where they can be seen without looking straight up.
        const Vec3 ahead = Vec3(std::sin(player_.yaw), 0, -std::cos(player_.yaw)), side(ahead.z, 0, -ahead.x);
        const Vec3 at = player_.pos + ahead * (14 + rnd() * 10) + side * ((rnd() - 0.5f) * 24) + Vec3(0, 9 + rnd() * 9, 0);
        particles_.spawnFirework(*world_, at, 46, 1.4f / std::max(0.12f, dayLight_ * settings_.brightness));
        sound(Sound::Fuse);
    }
    if (settings_.addonAnniversary && world_ == &overworld_) updateShowcase();
    if (settings_.addonAnniversary && world_ == &overworld_ && (confettiTimer_ -= dt) <= 0) {
        confettiTimer_ = 0.4f;  // the first birthday: confetti all around
        particles_.spawnConfetti(*world_, player_.pos, 3);
    }
    if (sleepTimer_ > 0) {
        // Asleep: the screen goes dark, then it is morning and the weather has cleared.
        sleepTimer_ += dt;
        player_.vel = {};
        if (sleepTimer_ >= 2.5f) {
            state_.dayTime = (std::floor(state_.dayTime / 24000.0) + 1) * 24000.0 + 300;
            state_.raining = false;
            state_.weatherTimer = 600;
            sleepTimer_ = 0;
            if (onMessage) onMessage(tr("Dobré ráno!"));
        }
    }
    // The map item draws itself while the player carries one; holding it shows the sheet.
    const bool carryingMap = countInInventory(state_.inventory, MAP) > 0;
    const bool holdingMap = state_.inventory.slots[state_.selectedSlot].id == MAP;
    if (settings.showMap || carryingMap) {
        const bool rebuilt = minimap_.update(*world_, player_.pos.x, player_.pos.y, player_.pos.z, dt, settings.expOreMap);
        if (rebuilt && carryingMap) paperMaps_.explore(dimIndex(), minimap_, player_.pos.x, player_.pos.z, 26);
        mapChanged_ |= rebuilt;
    }
    if (holdingMap != paperShown_) mapChanged_ = true;
    drops_.update(*world_, dimIndex(), player_.pos, state_.inventory, dt);
    itemNameTimer_ = std::max(0.0f, itemNameTimer_ - dt);

    pcx = floorDiv((int)std::floor(player_.pos.x), kChunkSize);
    pcz = floorDiv((int)std::floor(player_.pos.z), kChunkSize);
    world_->ensureChunksAround(pcx, pcz, settings.renderDistance + 1, 2);
    world_->unloadFarChunks(pcx, pcz, settings.renderDistance + 3);

    if ((autosaveTimer_ += dt) >= kAutosave) {
        autosaveTimer_ = 0;
        save();
    }
}

void Session::updateCamera(float dt, bool walking) {
    // Walking animation for the player model and the view bobbing.
    if (walking) {
        walkPhase_ += player_.walked * 2.6f;
        swing_ = std::sin(walkPhase_) * 0.7f;
    } else {
        swing_ *= std::max(0.0f, 1 - dt * 8);
    }

    camera_ = player_;
    cameraDistance_ = 0;
    if (settings_.cameraMode == 0) {
        if (settings_.viewBobbing && walking) camera_.pos.y += std::fabs(std::sin(walkPhase_)) * 0.06f;
        return;
    }
    // Third person: back away from the head until a block is in the way.
    bool front = settings_.cameraMode == 2;
    Vec3 eye = player_.eye(), dir = player_.forward() * (front ? 1.0f : -1.0f);
    float dist = 4;
    for (float t = 0.2f; t <= 4; t += 0.1f) {
        Vec3 p = eye + dir * t;
        if (isSolid(world_->getBlock((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z)))) {
            dist = std::max(0.3f, t - 0.3f);
            break;
        }
    }
    camera_.pos = eye + dir * dist - Vec3(0, Player::kEyeHeight, 0);
    cameraDistance_ = dist;
    if (front) {
        camera_.yaw = player_.yaw + kPi;
        camera_.pitch = -player_.pitch;
    }
}

void Session::render(Renderer& renderer, Ui& ui, const Settings& settings, bool hud) {
    renderer.dropUnloadedMeshes(*world_);
    renderer.updateMeshes(*world_, 3);
    if (world_ == &overworld_ && oldMode()) {
        // 1.8.0 Old World: the sky of 0.3: always day, one pale blue that the distance fades into
        // (the same fog: from 60 % of the view distance), no sun, moon, stars, clouds or shadows.
        static const float kOldSky[3] = {0.53f, 0.74f, 1.0f};
        renderer.setFogColors(kOldSky, config_.nether.fog);
        renderer.setBrightness(settings.brightness);
        renderer.setSky({}, {}, 0, {});
        renderer.setSun(Vec3(0, 1, 0), 0);
        dayLight_ = 1;
    } else if (world_ == &overworld_) {
        // Morning, day, evening, night and midnight: the sky, the light, sun, moon, stars and clouds.
        SkyState sky = computeSky((float)std::fmod(state_.dayTime, kDayTicks), config_.overworld.sky, skyRain_);
        {
            // 1.9.5: rain clouds darken the sky and the land, a thunderstorm even more.
            const float dark = skyRain_ * (storm() ? 0.45f : 0.28f);
            for (int i = 0; i < 3; ++i) sky.sky[i] = sky.sky[i] * (1 - dark) + 0.18f * dark;
            sky.light *= 1 - dark * 0.8f;
        }
        if (settings.addonHalloween) {
            // Halloween: a blood-red sky and gloom.
            const float red[3] = {0.42f, 0.06f, 0.04f};
            for (int i = 0; i < 3; ++i) sky.sky[i] = sky.sky[i] * 0.35f + red[i] * 0.65f * std::max(0.35f, sky.light);
            sky.light *= 0.8f;
        } else if (settings.season == Season::Halloween) {
            // 1.9.0 autumn: a warm, golden-orange sky.
            const float amber[3] = {0.95f, 0.55f, 0.25f};
            for (int i = 0; i < 3; ++i) sky.sky[i] = sky.sky[i] * 0.78f + amber[i] * 0.22f * sky.light;
        }
        renderer.setFogColors(sky.sky, config_.nether.fog);
        renderer.setBrightness(settings.brightness * (sky.light + lightningFlash_ * 0.9f));
        dayLight_ = sky.light;
        buildSkyObjects(camera_.eye(), (float)std::fmod(state_.dayTime, kDayTicks), sunMoonVerts_, starVerts_);
        if (settings.clouds)  // 2.4.0: above the raised land of deep worlds (the highest peaks reach 182)
            buildClouds(camera_.eye(), clock_, sky.clouds, cloudVerts_, world_->yShift() > 0 ? 186.0f : kCloudHeight);
        else cloudVerts_.clear();
        renderer.setSky(sunMoonVerts_, starVerts_, sky.stars, cloudVerts_);
        // 1.5.5: shadows from the sun by day and, fainter, from the moon at night; none in heavy rain.
        const Vec3 sun = sunDirectionAt((float)std::fmod(state_.dayTime, kDayTicks));
        const bool moon = sun.y < 0;
        const Vec3 light = moon ? sun * -1.0f : sun;
        const float up = std::clamp((light.y - 0.04f) / 0.22f, 0.0f, 1.0f);
        renderer.setSun(light, up * up * (3 - 2 * up) * (moon ? 0.35f : 1.0f) * (1 - 0.85f * skyRain_));
    } else if (world_ == &celestial_) {
        // 1.7.0 the Celestial Reach: a white, endless sky all round, above and below the islands.
        static const float kWhite[3] = {0.93f, 0.95f, 1.0f};
        renderer.setSun(Vec3(0.3f, 1, 0.2f), 0.6f);
        renderer.setFogColors(kWhite, config_.nether.fog);
        renderer.setBrightness(settings.brightness * 1.05f);
        renderer.setSky({}, {}, 0, {});
        dayLight_ = 1;
    } else {
        renderer.setSun(Vec3(0, 1, 0), 0);
        renderer.setFogColors(config_.overworld.sky, config_.nether.fog);
        renderer.setSky({}, {}, 0, {});
        dayLight_ = 1;
    }
    if (world_ == &void_) {
        // Purple darkness in the eye-tree forest, red haze over the desert, glowing ash over the lava,
        // pale grey over the Forgotten Overworld (1.7.0: it had no entry, and the screen went green).
        // 1.7.2 build 3: deep red in the Crimsonium Forest, violet dust over the Shattered Void, dark
        // purple haze over the shard desert.
        static const float kFog[7][3] = {{0.16f, 0.05f, 0.24f}, {0.45f, 0.10f, 0.07f}, {0.42f, 0.13f, 0.04f}, {0.34f, 0.38f, 0.40f},
                                         {0.38f, 0.05f, 0.08f}, {0.22f, 0.16f, 0.30f}, {0.16f, 0.11f, 0.20f}};
        const float* want = kFog[(int)void_.voidBiomeAt((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.z))];
        for (int i = 0; i < 3; ++i) voidFog_[i] += (want[i] - voidFog_[i]) * 0.03f;
        renderer.setVoidFog(voidFog_);
        renderer.setBrightness(settings.brightness * 0.8f);
    }
    renderer.setFogScale(kClassicEdition ? (settings.classicFog == 0 ? 6.0f : settings.classicFog == 2 ? 0.55f : 1.0f) : settings.addonHalloween ? 0.3f : 1.0f);
    // 1.5.7: the player (also in first person), other players and mobs cast shadows.
    mobs_.buildVertices(dimIndex(), clock_, mobVerts_);
    shadowCasters_ = buildPlayerModel(this->pose());
    for (const RemoteAvatar& a : remotePlayers)
        if (a.dim == dimIndex()) {
            std::vector<Vertex> v = buildPlayerModel(a.pose);
            shadowCasters_.insert(shadowCasters_.end(), v.begin(), v.end());
        }
    shadowCasters_.insert(shadowCasters_.end(), mobVerts_.begin(), mobVerts_.end());
    renderer.setShadowCasters(shadowCasters_);
    // 1.5.7: a torch in either hand lights up the dark around the player.
    const bool torchInHand = state_.inventory.slots[state_.selectedSlot].id == TORCH || state_.offhand.id == TORCH;
    renderer.setHandLight(player_.eye() - Vec3(0, 0.4f, 0), torchInHand ? 0.95f : 0.0f);
    renderer.drawWorld(camera_, world_->dimension(), target_, breakProgress_, settings.renderDistance * (float)kChunkSize,
                       settings.fov);
    if (particles_.count()) {
        Vec3 f = camera_.forward();
        Vec3 right = cross(f, {0, 1, 0}).normalized();
        Vec3 up = cross(right, f).normalized();
        particles_.buildVertices(right, up, particleVerts_);
        renderer.drawParticles(particleVerts_);
    }
    renderer.setWind(world_ == &overworld_ ? wind_ : 0.1f);
    setPrecipitationWind(wind_);
    if (world_ == &overworld_ && rain_ > 0.01f) {
        buildPrecipitation(*world_, camera_.eye(), precipitation(), clock_, rain_, 0.45f + dayLight_ * 0.55f, weatherVerts_);
        if (!weatherVerts_.empty()) renderer.drawParticles(weatherVerts_);
    }
    tntVerts_.clear();
    for (const PrimedTnt& t : tnt_) {
        if (t.dim != dimIndex()) continue;
        // A flashing block that swells just before it goes off.
        const float flash = std::fmod(t.fuse, 0.5f) < 0.25f ? 1.8f : 1.0f, grow = t.fuse < 0.4f ? (0.4f - t.fuse) * 0.4f : 0;
        const float h = 0.5f + grow;
        Vec3 c = t.pos + Vec3(0, 0.5f, 0);
        for (int f = 0; f < 6; ++f) {
            int tile = tileFor(t.nuke ? NUKE_TNT : TNT, (Face)f);
            float u0 = (tile % kAtlasCols) * kTileUv, v0 = (tile / kAtlasCols) * kTileUv, u1 = u0 + kTileUv, v1 = v0 + kTileUv;
            static const int kQuad[6][4][3] = {
                {{-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}}, {{-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}},
                {{1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}}, {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}},
                {{1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}}, {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}}};
            Vertex q[4];
            const float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
            for (int k = 0; k < 4; ++k)
                q[k] = {c.x + kQuad[f][k][0] * h, c.y + kQuad[f][k][1] * h, c.z + kQuad[f][k][2] * h, uv[k][0], uv[k][1], flash};
            tntVerts_.insert(tntVerts_.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        }
    }
    if (!tntVerts_.empty()) renderer.drawParticles(tntVerts_);
    fallingVerts_.clear();
    for (const FallingBlock& fb : fallingBlocks_) {
        if (fb.dim != dimIndex()) continue;
        const Vec3 c = fb.pos + Vec3(0, 0.5f, 0);
        const float shade[6] = {1.0f, 0.5f, 0.8f, 0.8f, 0.65f, 0.65f}, light = std::max(0.35f, dayLight_);
        for (int f = 0; f < 6; ++f) {
            int tile = tileFor(fb.id, (Face)f);
            float u0 = (tile % kAtlasCols) * kTileUv, v0 = (tile / kAtlasCols) * kTileUv, u1 = u0 + kTileUv, v1 = v0 + kTileUv;
            static const int kQuad[6][4][3] = {
                {{-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}}, {{-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}},
                {{1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}}, {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}},
                {{1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}}, {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}}};
            Vertex q[4];
            const float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
            for (int k = 0; k < 4; ++k)
                q[k] = {c.x + kQuad[f][k][0] * 0.5f, c.y + kQuad[f][k][1] * 0.5f, c.z + kQuad[f][k][2] * 0.5f, uv[k][0], uv[k][1], shade[f] * light};
            fallingVerts_.insert(fallingVerts_.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        }
    }
    if (!fallingVerts_.empty()) renderer.drawParticles(fallingVerts_);
    // Arrows: two crossed squares along the flight, the arrow sprite on their diagonal.
    arrowVerts_.clear();
    {
        const int tile = T_ARROW;
        const float u0 = (tile % kAtlasCols) * kTileUv, v0 = (tile / kAtlasCols) * kTileUv, u1 = u0 + kTileUv, v1 = v0 + kTileUv;
        for (const Arrow& a : arrows_) {
            if (a.dim != dimIndex()) continue;
            Vec3 dir = a.stuck ? Vec3(0, -1, 0) : a.vel.normalized();
            if (a.stuck && a.vel.length() > 0.01f) dir = a.vel.normalized();
            const Vec3 head = a.pos, tail = a.pos - dir * 0.8f, mid = (head + tail) * 0.5f;
            Vec3 side1 = cross(dir, std::fabs(dir.y) > 0.9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0)).normalized() * 0.4f;
            Vec3 side2 = cross(dir, side1).normalized() * 0.4f;
            for (const Vec3& sd : {side1, side2}) {
                Vertex p0{tail.x, tail.y, tail.z, u0, v1, 1}, p1{mid.x + sd.x, mid.y + sd.y, mid.z + sd.z, u0, v0, 1};
                Vertex p2{head.x, head.y, head.z, u1, v0, 1}, p3{mid.x - sd.x, mid.y - sd.y, mid.z - sd.z, u1, v1, 1};
                arrowVerts_.insert(arrowVerts_.end(), {p0, p1, p2, p0, p2, p3, p0, p2, p1, p0, p3, p2});
            }
        }
    }
    if (!arrowVerts_.empty()) renderer.drawParticles(arrowVerts_);
    // Fireballs: small glowing cubes that spin as they fly.
    fireballVerts_.clear();
    for (const Fireball& f : fireballs_) {
        if (f.dim != dimIndex()) continue;
        const int tile = T_FIREBALL;
        const float u0 = (tile % kAtlasCols) * kTileUv, v0 = (tile / kAtlasCols) * kTileUv, u1 = u0 + kTileUv, v1 = v0 + kTileUv;
        const float h = 0.22f, a = clock_ * 7 + f.life, ca = std::cos(a), sa = std::sin(a);
        static const int kQuad[6][4][3] = {
            {{-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}}, {{-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}},
            {{1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}}, {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}},
            {{1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}}, {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}}};
        const float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
        for (int face = 0; face < 6; ++face) {
            Vertex q[4];
            for (int k = 0; k < 4; ++k) {
                const float x = kQuad[face][k][0] * h, z = kQuad[face][k][2] * h;
                q[k] = {f.pos.x + x * ca - z * sa, f.pos.y + kQuad[face][k][1] * h, f.pos.z + x * sa + z * ca, uv[k][0], uv[k][1], 2.0f};
            }
            fireballVerts_.insert(fireballVerts_.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        }
    }
    if (!fireballVerts_.empty()) renderer.drawParticles(fireballVerts_);
    if (!mobVerts_.empty()) renderer.drawParticles(mobVerts_);
    boats_.buildVertices(dimIndex(), boatVerts_);
    trains_.buildVertices(dimIndex(), boatVerts_);  // 2.1.0 preview: the trains go with the boats
    if (!boatVerts_.empty()) renderer.drawParticles(boatVerts_);
    drops_.buildVertices(dimIndex(), clock_, dropVerts_);
    if (!dropVerts_.empty()) renderer.drawParticles(dropVerts_);
    if (settings.cameraMode != 0) {
        PlayerPose pose = this->pose();
        renderer.drawPlayerModel(pose);
        playerExtras_ = buildPlayerExtras(pose);
        if (!playerExtras_.empty()) renderer.drawParticles(playerExtras_);
    }
    if (kClassicEdition && doppel_.active && world_ == &overworld_) renderer.drawPlayerModel(doppelPose(), kDoppelSkinSlot);
    for (const RemoteAvatar& a : remotePlayers) {
        if (a.dim != dimIndex()) continue;
        renderer.drawPlayerModel(a.pose, a.skinSlot);
        playerExtras_ = buildPlayerExtras(a.pose);
        if (!playerExtras_.empty()) renderer.drawParticles(playerExtras_);
    }
    if (settings.cameraMode == 0 && !settings.hideGui && (!oldMode() || kClassicEdition)) {  // 0.3 showed no hand; Classic does
        // The hand is lit like the place the player stands in.
        Vec3 e = player_.eye();
        float light = world_->dimension() != Dimension::Overworld ? 0.8f
                      : (int)std::floor(e.y) >= world_->skyHeight((int)std::floor(e.x), (int)std::floor(e.z)) ? 1.0f
                                                                                                             : 0.55f;
        buildFirstPerson(hand_, light, handSkin_, handAtlas_, kFeatures220 ? bowCharge_ : 0);
        buildOffhandFirstPerson(state_.offhand.id, player_.sneaking, hand_, light, handAtlas_);
        renderer.drawFirstPerson(handSkin_, handAtlas_);
    }
    if (mapChanged_) {
        paperShown_ = state_.inventory.slots[state_.selectedSlot].id == MAP;
        if (paperShown_) {
            std::vector<uint8_t> sheet;
            paperMaps_.view(dimIndex(), (int)std::floor(player_.pos.x), (int)std::floor(player_.pos.z), sheet, paperOriginX_,
                            paperOriginZ_);
            renderer.setMapTexture(sheet, PaperMaps::kSize, PaperMaps::kSize);
        } else {
            renderer.setMapTexture(minimap_.rgba(), Minimap::kSize, Minimap::kSize);
        }
        mapChanged_ = false;
    }
    if (hud) drawHud(ui);
}

PlayerPose Session::pose() const {
    PlayerPose pose;
    pose.feet = player_.pos;
    pose.yaw = player_.yaw;
    pose.headPitch = player_.pitch;
    pose.swing = riding_ >= 0 || seated_ ? 0 : swing_;
    // The same strokes as the first-person hand, with what is really held and worn.
    pose.attack = hand_.swingProgress();
    pose.place = hand_.placeProgress();
    pose.held = hand_.shown();
    for (int i = 0; i < 4; ++i) pose.armor[i] = state_.armor[i].empty() ? (ItemId)0 : state_.armor[i].id;
    // 1.5.7: only a shield or a torch shows in the off hand.
    if (state_.offhand.id == SHIELD || state_.offhand.id == TORCH) pose.offhand = state_.offhand.id;
    pose.sneaking = player_.sneaking && !player_.flying && riding_ < 0;
    pose.riding = riding_ >= 0 || seated_ || trainSeatTrain_ >= 0;
    if (pose.riding) pose.feet.y -= 0.45f;  // sitting in the boat
    if (kFeatures250 && !pose.riding) {
        // 2.5.0 preview build 5: crouching, swimming and waving show on the model.
        pose.crouch = pose.sneaking ? 1.0f : 0.0f;
        pose.swim = player_.inWater && !player_.flying && (!player_.onGround || player_.headInWater) ? 1.0f : 0.0f;
        if (pose.swim > 0) pose.crouch = 0, pose.feet.y += 0.5f;
        if (clock_ < waveUntil_) pose.wave = clock_ - (waveUntil_ - 2.4f);
    }
    if (kFeatures220) {
        // 2.2.0 preview: the arms breathe, the body dips as the legs stride apart, a bow is drawn.
        pose.lively = true;
        pose.time = clock_;
        pose.bow = pose.held == BOW ? bowCharge_ : 0;
        if (!pose.riding) pose.feet.y -= std::fabs(std::sin(pose.swing)) * 0.06f;
    }
    return pose;
}

Precipitation Session::precipitation() const {
    if (world_ != &overworld_) return Precipitation::None;
    if (settings_.addonWinter || snowySeason(settings_.season)) return Precipitation::Snow;  // a white winter everywhere
    const int x = (int)std::floor(player_.pos.x), z = (int)std::floor(player_.pos.z);
    const BiomeDef* b = overworld_.biomeAt(x, z);
    if (!b) return Precipitation::Rain;
    // Deserts stay dry; snowy lands and high mountain tops get snow instead of rain.
    if (b->cactusDensity > 0 || b->id == "desert") return Precipitation::None;
    // 1.7.2 build 3: the Dappled Forest gets snow most of the time; about one day in five it
    // rains there instead, and only lightly (see updateWeather).
    if (b->id == "dappled_forest")
        return hash3((int)(state_.dayTime / kDayTicks), 7, (int)info_.seed, 0xDA99u) % 5 == 0 ? Precipitation::Rain : Precipitation::Snow;
    if (b->freeze || b->id == "snowy_taiga" || b->id == "ice_plains" || (b->peakBlock && player_.pos.y > b->snowHeight - 12))
        return Precipitation::Snow;
    return Precipitation::Rain;
}

void Session::updateWeather(float dt) {
    // The day goes on in every dimension (the developer's fast time runs it 20 times faster).
    state_.dayTime += dt * kTicksPerSecond * (settings_.expDayNight ? 20.0f : 1.0f);
    if ((state_.weatherTimer -= dt) <= 0) {
        state_.raining = !state_.raining;
        uint32_t r = hash3((int)state_.dayTime, 5, (int)info_.seed, 0x3E7Au);
        state_.weatherTimer = state_.raining ? 120.0f + r % 240 : 300.0f + r % 900;
    }
    const Precipitation kind = precipitation();
    const bool falling = (state_.raining || settings_.addonWinter || snowySeason(settings_.season)) && kind != Precipitation::None;
    updateSnowCover(dt, falling && kind == Precipitation::Snow);
    if (falling && kind == Precipitation::Rain && world_ == &overworld_) {
        // 1.7.2 build 3: rain sets concrete powder that lies under the open sky.
        rainTimer_ += dt;
        while (rainTimer_ >= 0.05f) {
            rainTimer_ -= 0.05f;
            const int x = (int)std::floor(player_.pos.x) + (int)(spawnRandom() * 33) - 16;
            const int z = (int)std::floor(player_.pos.z) + (int)(spawnRandom() * 33) - 16;
            const int y = overworld_.skyHeight(x, z) - 1;
            const ItemId b = overworld_.getBlock(x, y, z);
            if (isConcretePowder(b)) overworld_.setBlock(x, y, z, hardenedConcrete(b));
        }
    }
    const BiomeDef* here = world_ == &overworld_ ? overworld_.biomeAt((int)std::floor(player_.pos.x), (int)std::floor(player_.pos.z)) : nullptr;
    const float most = here && here->id == "dappled_forest" && kind == Precipitation::Rain ? 0.3f : 1.0f;  // a light drizzle there
    rain_ += ((falling ? most : 0.0f) - rain_) * std::min(1.0f, dt * 0.4f);
    // The sky greys only where it rains or snows (not over a desert).
    skyRain_ += ((falling ? 1.0f : 0.0f) - skyRain_) * std::min(1.0f, dt * 0.3f);
    // 1.9.5 weather 2.0: wind. A breeze by day, a strong wind in rain and snow, gusting; it bends
    // the leaves, the grass and the rain.
    if (oldMode()) storm_ = false;  // 1.9.5: the Old World has plain rain only, no wind or storms
    const float want = oldMode() ? 0.0f : (falling ? (storm_ ? 1.0f : 0.7f) : 0.12f) * (0.75f + 0.25f * std::sin(clock_ * 0.37f) + 0.15f * std::sin(clock_ * 1.9f));
    wind_ += (want - wind_) * std::min(1.0f, dt * 0.25f);
    // Some rains are thunderstorms (chosen when the rain starts).
    if (state_.raining != wasRaining_) {
        wasRaining_ = state_.raining;
        storm_ = state_.raining && hash3((int)state_.dayTime, 9, (int)info_.seed, 0x570Au) % 100 < 45;
    }
}

void Session::updateSnowCover(float dt, bool snowing) {
    // While it snows, a thin layer settles on the ground and on the leaves around the player;
    // where it does not (warm lands, or the snow has stopped outside the cold biomes) it melts.
    if (world_ != &overworld_) return;
    snowTimer_ += dt;
    const float every = snowing ? 0.04f : 0.12f;
    World& w = overworld_;
    while (snowTimer_ >= every) {
        snowTimer_ -= every;
        const int x = (int)std::floor(player_.pos.x) + (int)(spawnRandom() * 49) - 24;
        const int z = (int)std::floor(player_.pos.z) + (int)(spawnRandom() * 49) - 24;
        if (!w.getChunk(floorDiv(x, kChunkSize), floorDiv(z, kChunkSize))) continue;
        const int y = w.skyHeight(x, z);
        if (y <= 1 || y >= kChunkHeight - 1) continue;
        const ItemId here = w.getBlock(x, y, z), below = w.getBlock(x, y - 1, z);
        if (snowing) {
            const bool ground = isOpaque(below) || isLeaves(below);
            if (ground && (here == AIR || here == TALL_GRASS) && !isLiquid(below)) w.setBlock(x, y, z, SNOW_LAYER);
        } else if (below == SNOW_LAYER || here == SNOW_LAYER) {
            const int sy = here == SNOW_LAYER ? y : y - 1;
            const BiomeDef* b = w.biomeAt(x, z);
            const bool cold = settings_.addonWinter || snowySeason(settings_.season) || (b && (b->freeze || b->id == "snowy_taiga" || b->id == "ice_plains" || b->id == "dappled_forest"));
            if (!cold) w.setBlock(x, sy, z, AIR);
        }
    }
}

void Session::setWeather(bool raining) {
    state_.raining = raining;
    state_.weatherTimer = raining ? 300.0f : 600.0f;
}

void Session::recordEdits(bool on) {
    overworld_.editLog = on ? &edits_[0] : nullptr;
    nether_.editLog = on ? &edits_[1] : nullptr;
    void_.editLog = on ? &edits_[2] : nullptr;
    celestial_.editLog = on ? &edits_[3] : nullptr;
}

std::vector<BlockEdit> Session::takeEdits(int dim) { return std::move(edits_[std::clamp(dim, 0, 3)]); }

void Session::applyRemoteBlock(int dim, int x, int y, int z, ItemId id) {
    World& w = worldAt(dim);
    std::vector<BlockEdit>* log = w.editLog;
    w.editLog = nullptr;
    w.setBlockLoading(x, y, z, id);
    w.editLog = log;
    if (target_.hit && target_.x == x && target_.y == y && target_.z == z && world_ == &w) breakProgress_ = 0;
}

void Session::saveWorlds() {
    overworld_.saveModifiedChunks();
    nether_.saveModifiedChunks();
    void_.saveModifiedChunks();
}

namespace {
// 2.5.0: the colour of a structure's mark on the map, by its kind.
Color foundColor(int type) {
    switch ((WorldStructure)type) {
        case WorldStructure::Village: return {0.45f, 0.9f, 0.35f, 1};
        case WorldStructure::RuinedVillage: case WorldStructure::Ruin: return {0.65f, 0.65f, 0.55f, 1};
        case WorldStructure::Watchtower: return {0.85f, 0.75f, 0.5f, 1};
        case WorldStructure::DesertTemple: return {1, 0.85f, 0.45f, 1};
        case WorldStructure::Igloo: return {0.8f, 0.95f, 1, 1};
        case WorldStructure::Shipwreck: case WorldStructure::DrownedRuins: return {0.35f, 0.6f, 1, 1};
        case WorldStructure::Mineshaft: case WorldStructure::Dungeon: case WorldStructure::CaveDungeon: return {0.6f, 0.45f, 0.35f, 1};
        case WorldStructure::BrokenPortal: return {0.75f, 0.4f, 1, 1};
        case WorldStructure::Camp: return {1, 0.55f, 0.3f, 1};
        case WorldStructure::RuinedStation: return {1, 0.6f, 0.25f, 1};
        case WorldStructure::TrainWreck: return {0.8f, 0.4f, 0.25f, 1};
        default: break;
    }
    if (type == 100) return {0.95f, 0.65f, 0.3f, 1};  // the Bronze Halls
    if (type == 101) return {0.4f, 1, 0.8f, 1};       // the lost city
    return {0.6f, 0.9f, 1, 1};
}
}  // namespace

std::vector<MapMarker> Session::mapMarkers() const {
    std::vector<MapMarker> m;
    if (world_ != &overworld_) return m;  // the places are all in the overworld
    m.push_back({MapMarker::Spawn, (int)std::floor(state_.spawn.x), (int)std::floor(state_.spawn.z)});
    if (state_.hasDeath) m.push_back({MapMarker::Death, state_.deathX, state_.deathZ});
    for (auto& [x, z] : state_.structures) m.push_back({MapMarker::Structure, x, z});
    if (world_ == &overworld_) {
        // 2.4.0: ruined stations nearby (within about 300 blocks) are shown on the map.
        const int px = (int)std::floor(player_.pos.x), pz = (int)std::floor(player_.pos.z), R = World::kStationRegion;
        for (int rx = floorDiv(px, R) - 1; rx <= floorDiv(px, R) + 1; ++rx)
            for (int rz = floorDiv(pz, R) - 1; rz <= floorDiv(pz, R) + 1; ++rz) {
                int cx, cz;
                if (world_->ruinedStationIn(rx, rz, cx, cz) && std::abs(cx - px) < 300 && std::abs(cz - pz) < 300) m.push_back({MapMarker::Station, cx, cz});
            }
        // 2.5.0: the entrances of the abandoned subway nearby.
        int mx, mz, kind;
        if (world_->nearestMetroStation(px, pz, mx, mz, kind) && std::abs(mx - px) < 300 && std::abs(mz - pz) < 300) m.push_back({MapMarker::Station, mx, mz});
        if (kFeatures250) {
            // 2.5.0: every structure around shows on the map (worked out again only when the player moves on).
            const int cx = floorDiv(px, 64), cz = floorDiv(pz, 64);
            if (cx != foundCX_ || cz != foundCZ_) {
                foundCX_ = cx, foundCZ_ = cz;
                foundMarkers_.clear();
                for (const World::FoundStructure& f : const_cast<World&>(overworld_).structuresNear(px, pz, 300))
                    foundMarkers_.push_back({MapMarker::Found, f.x, f.z, f.type});
                const int R = 400;  // the Bronze Halls' regions (kHallRegion)
                for (int rx = floorDiv(px, R) - 1; rx <= floorDiv(px, R) + 1; ++rx)
                    for (int rz = floorDiv(pz, R) - 1; rz <= floorDiv(pz, R) + 1; ++rz) {
                        int hx, hz;
                        if (overworld_.bronzeHallsIn(rx, rz, hx, hz)) foundMarkers_.push_back({MapMarker::Found, hx + 49, hz + 49, 100});
                    }
                const int M = World::kMetroRegion;
                for (int line = floorDiv(pz, M) - 2; line <= floorDiv(pz, M) + 2; ++line)
                    for (int region = floorDiv(px, M) - 2; region <= floorDiv(px, M) + 2; ++region) {
                        int rampX, ox, oz;
                        if (overworld_.lostCityOf(line, region, rampX, ox, oz)) foundMarkers_.push_back({MapMarker::Found, ox + 45, oz + 45, 101});
                    }
            }
            m.insert(m.end(), foundMarkers_.begin(), foundMarkers_.end());
        }
    }
    return m;
}

void Session::drawOldHud(Ui& ui) const {
    // 1.8.0 Old World: the HUD of VoxelCraft 0.1-0.3 (as in the creator's screenshots): nine
    // dark slots with a white frame round the chosen one, a grass block top right that opens
    // the menu, and a see-through jump button with a small step glyph. Nothing else.
    const Layout l = layout();
    bool jumpHeld = false, pauseHeld = false;
    for (const Touch& t : touches_) {
        if (t.id < 0) continue;
        jumpHeld |= t.kind == Touch::Jump;
        pauseHeld |= t.kind == Touch::Pause;
    }
    ui.fill(l.hotbarX - 2, l.hotbarY - 2, l.slot * 9 + 4, l.slot + 4, {0.12f, 0.12f, 0.12f, 0.85f});
    for (int i = 0; i < kHotbarSize; ++i) {
        const float x = l.hotbarX + i * l.slot;
        ui.fill(x + 1, l.hotbarY + 1, l.slot - 2, l.slot - 2, {0.22f, 0.22f, 0.22f, 0.9f});
        const ItemStack& st = state_.inventory.slots[i];
        if (!st.empty()) ui.tile(x + 3, l.hotbarY + 3, l.slot - 6, tileFor(st.id, F_NORTH));  // flat, as in 0.3
    }
    const float sx = l.hotbarX + state_.selectedSlot * l.slot;
    const Color white{0.95f, 0.95f, 0.95f, 1};
    ui.fill(sx - 1, l.hotbarY - 1, l.slot + 2, 2, white);
    ui.fill(sx - 1, l.hotbarY + l.slot - 1, l.slot + 2, 2, white);
    ui.fill(sx - 1, l.hotbarY - 1, 2, l.slot + 2, white);
    ui.fill(sx + l.slot - 1, l.hotbarY - 1, 2, l.slot + 2, white);
    // The menu: a grass block on a slate square.
    ui.fill(l.pauseX - 2, l.pauseY - 2, l.pauseSize + 4, l.pauseSize + 4, pauseHeld ? Color{0.5f, 0.6f, 0.75f, 0.9f} : Color{0.3f, 0.38f, 0.5f, 0.85f});
    ui.tile(l.pauseX + 2, l.pauseY + 2, l.pauseSize - 4, T_GRASS_SIDE);
    if (kClassicEdition && player_.flying) {
        // Classic 0.0.3: flying down, a second square beside jump with the step glyph upside down.
        const float fx = l.jumpX - l.buttonSize - 6;
        ui.fill(fx, l.jumpY, l.buttonSize, l.buttonSize, {1, 1, 1, 0.18f});
        const float cx = fx + l.buttonSize / 2, cy = l.jumpY + l.buttonSize / 2;
        ui.fill(cx - 2, cy - 1, 4, 9, {1, 1, 1, 0.85f});
        ui.fill(cx - 9, cy - 5, 18, 4, {1, 1, 1, 0.85f});
    }
    if (kClassicEdition && settings_.showFps) {
        const std::string info = "FPS: " + std::to_string((int)std::round(fps_)) + "  XYZ: " + std::to_string((int)std::floor(player_.pos.x)) + " " +
                                 std::to_string((int)std::floor(player_.pos.y)) + " " + std::to_string((int)std::floor(player_.pos.z));
        ui.fill(2, 2, ui.textWidth(info) + 4, 11, {0, 0, 0, 0.5f});
        ui.text(4, 4, info, {1, 1, 1, 1});
    }
    // Jump: a pale see-through square with the step glyph.
    ui.fill(l.jumpX, l.jumpY, l.buttonSize, l.buttonSize, {1, 1, 1, jumpHeld ? 0.35f : 0.18f});
    const float gx = l.jumpX + l.buttonSize / 2, gy = l.jumpY + l.buttonSize / 2;
    ui.fill(gx - 2, gy - 8, 4, 9, {1, 1, 1, 0.85f});
    ui.fill(gx - 9, gy + 1, 18, 4, {1, 1, 1, 0.85f});
    if (kClassicEdition) {
        // Classic 0.0.3: a D-pad in the old style.
        bool pad[3][3] = {};
        for (const Touch& t : touches_) {
            int dx, dz;
            if (t.id >= 0 && t.kind == Touch::DPad && dpadDirection(l, t.x, t.y, dx, dz)) pad[1 - dz][dx + 1] = true;
        }
        const float b = l.padButton;
        const int arrows[4][3] = {{1, 0, Font::HudUp}, {0, 1, Font::HudLeft}, {2, 1, Font::HudRight}, {1, 2, Font::HudDown}};
        for (const auto& a : arrows) {
            const int col = a[0], row = a[1];
            const bool on = pad[row][col] || (row == 0 && (pad[0][0] || pad[0][2])) || (row == 2 && (pad[2][0] || pad[2][2])) ||
                            (col == 0 && (pad[0][0] || pad[2][0])) || (col == 2 && (pad[0][2] || pad[2][2]));
            // The look of 0.3: a pale see-through square, a plain white arrow made of pixel rows.
            const float x = l.padX + col * b + 1, y = l.padY + row * b + 1, w = b - 2;
            ui.fill(x, y, w, w, {1, 1, 1, on ? 0.35f : 0.18f});
            const float cx = x + w / 2, cy = y + w / 2;
            const Color arrow{1, 1, 1, 0.85f};
            for (int k = 0; k < 5; ++k) {
                const float len = 2 + k * 2.5f, off = -5 + k * 2.2f;
                if (a[2] == Font::HudUp) ui.fill(cx - len / 2, cy + off, len, 2.2f, arrow);
                if (a[2] == Font::HudDown) ui.fill(cx - len / 2, cy - off - 2.2f, len, 2.2f, arrow);
                if (a[2] == Font::HudLeft) ui.fill(cx + off, cy - len / 2, 2.2f, len, arrow);
                if (a[2] == Font::HudRight) ui.fill(cx - off - 2.2f, cy - len / 2, 2.2f, len, arrow);
            }
        }
    }
    // The floating joystick of 0.3, wherever the left thumb went down.
    for (const Touch& t : touches_) {
        if (t.id < 0 || t.kind != Touch::Joystick) continue;
        const float r = 26;
        ui.fill(t.startX - r, t.startY - r, 2 * r, 2 * r, {1, 1, 1, 0.18f});
        const float kx = t.startX + std::clamp(t.x - t.startX, -r, r), ky = t.startY + std::clamp(t.y - t.startY, -r, r);
        ui.fill(kx - 9, ky - 9, 18, 18, {1, 1, 1, 0.5f});
    }
    // Classic 0.0.3: hearts above the hotbar in survival (there was no food yet), and counts on the slots.
    if (kClassicEdition && survival()) {
        for (int i = 0; i < 10; ++i) {
            const float hp = state_.health - i * 2;
            ui.icon(l.hotbarX + i * 9, l.hotbarY - 11, Font::HeartEmpty, {1, 1, 1, 1});
            if (hp >= 2) ui.icon(l.hotbarX + i * 9, l.hotbarY - 11, Font::HeartFull, {1, 0.3f, 0.3f, 1});
            else if (hp >= 1) ui.icon(l.hotbarX + i * 9, l.hotbarY - 11, Font::HeartHalf, {1, 0.3f, 0.3f, 1});
        }
        for (int i = 0; i < kHotbarSize; ++i) {
            const ItemStack& st = state_.inventory.slots[i];
            if (st.count > 1) ui.text(l.hotbarX + i * l.slot + l.slot - 3 - ui.textWidth(std::to_string(st.count)), l.hotbarY + l.slot - 10, std::to_string(st.count), {1, 1, 1, 1});
        }
    }
    // Crosshair.
    ui.fill(guiW_ / 2 - 4, guiH_ / 2, 9, 1, {1, 1, 1, 0.8f});
    ui.fill(guiW_ / 2, guiH_ / 2 - 4, 1, 9, {1, 1, 1, 0.8f});
}

Session::TrainButtons Session::trainButtons() const {
    TrainButtons b;
    if (dead_) return b;
    b.w = 66, b.h = 20, b.gap = 4;
    b.x = guiW_ - b.w - 8;
    b.y = std::max(guiH_ * 0.30f, 84.0f);  // below the minimap
    // 2.5.0: in a developer map, a button to wave hello (for filming).
    if (kFeatures250 && world_->config().devMap > 0 && trainSeatTrain_ < 0) b.labels[b.count++] = "Ahoj";
    if (trains_.trains().empty()) return b;  // trains exist only in worlds with the rails experiment
    if (trainSeatTrain_ >= 0) {
        const bool driver = trainSeat_ == 0 &&
                            isEngineCar(trains_.trains()[(size_t)trainSeatTrain_].cars[(size_t)trainSeatCar_].kind);
        if (driver) {
            b.labels[b.count++] = "Plyn +";
            b.labels[b.count++] = "Plyn -";
            b.labels[b.count++] = "Brzda";
            if (isSubwayCar(trains_.trains()[(size_t)trainSeatTrain_].cars[(size_t)trainSeatCar_].kind)) b.labels[b.count++] = "Dveře";
            if (kFeatures250 && isSubwayCar(trains_.trains()[(size_t)trainSeatTrain_].cars[(size_t)trainSeatCar_].kind)) b.labels[b.count++] = "Automat";
        }
        b.labels[b.count++] = "Vystoupit";
    } else {
        int tr, car;
        if (trains_.nearest(dimIndex(), player_.pos, 6.5f, tr, car)) b.labels[b.count++] = "Nastoupit";
    }
    return b;
}

bool Session::trainButtonTap(float x, float y) {
    const TrainButtons b = trainButtons();
    for (int i = 0; i < b.count; ++i) {
        const float by = b.y + i * (b.h + b.gap);
        if (x < b.x || x >= b.x + b.w || y < by || y >= by + b.h) continue;
        const std::string what = b.labels[i];
        if (what == "Ahoj") {
            waveHello();
            sound(Sound::Click);
            return true;
        }
        if (what == "Nastoupit") {
            // The driver's seat if the nearest car is a locomotive, else a free passenger seat.
            int tr, car;
            if (!trains_.nearest(dimIndex(), player_.pos, 6.5f, tr, car)) return true;
            const TrainCar& c = trains_.trains()[(size_t)tr].cars[(size_t)car];
            if (trains_.seatCount(c.kind) == 0) {
                // A goods wagon has no seats: climb up onto the load instead.
                player_.pos = c.pos + Vec3(0, 1.6f * TrainSystem::kScale, 0);
                return true;
            }
            trainSeatTrain_ = tr, trainSeatCar_ = car;
            trainSeat_ = (int)(hash3((int)clock_, tr, car, 7) % (unsigned)trains_.seatCount(c.kind));
            if (isEngineCar(c.kind)) trainSeat_ = 0;
            {
                // 2.4.0: sit facing the right way: the driver ahead, a passenger across the aisle.
                const float sn = std::sin(c.yaw), cs = std::cos(c.yaw);
                float fx = -sn, fz = cs;  // the car's front
                if (trainSeat_ != 0 || !isEngineCar(c.kind)) {
                    const Vec3 sp = trains_.seatPos(c, trainSeat_) - c.pos;
                    const float lx = sp.x * cs + sp.z * sn;  // across the car
                    if (isSubwayCar(c.kind) && std::fabs(lx) > 0.2f) fx = lx > 0 ? -cs : cs, fz = lx > 0 ? -sn : sn;
                }
                player_.yaw = std::atan2(fx, -fz);
            }
            player_.flying = false;
            seated_ = false;
            sound(Sound::Door);
        } else if (what == "Vystoupit") {
            leaveTrain();
        } else if (trainSeatTrain_ >= 0) {
            Train& t = trains_.trains()[(size_t)trainSeatTrain_];
            if (what == "Plyn +") t.throttle = std::min(1.0f, t.throttle + 0.25f), t.brake = false;
            if (what == "Plyn -") t.throttle = std::max(-0.5f, t.throttle - 0.25f), t.brake = false;
            if (what == "Brzda") t.brake = !t.brake, t.throttle = 0;
            if (what == "Automat") {  // 2.5.0: the automatic subway
                t.autoMode = !t.autoMode;
                t.autoDir = t.throttle < 0 ? -1.0f : 1.0f;
                t.dwell = 0, t.lastStopX = t.lastStopZ = 1e9f;
                if (!t.autoMode) t.throttle = 0;
                if (onMessage) onMessage(tr(t.autoMode ? "Automatické metro zapnuto: zastaví u každého nástupiště" : "Automatické metro vypnuto"));
            }
            if (what == "Dveře") {  // 2.4.0: the driver opens and shuts the doors by hand (when standing)
                t.doorsWanted = !t.doorsWanted && std::fabs(t.speed) < 0.05f;
                if (t.doorsWanted) t.throttle = 0, t.doors = 0;  // reopen on both sides
            }
            sound(Sound::Click);
        }
        return true;
    }
    return false;
}

void Session::leaveTrain() {
    if (trainSeatTrain_ < 0) return;
    const TrainCar& c = trains_.trains()[(size_t)trainSeatTrain_].cars[(size_t)trainSeatCar_];
    trainSeatTrain_ = -1;
    // Out by the door, beside the car on whatever is free.
    const float cs = std::cos(c.yaw), sn = std::sin(c.yaw);
    for (float side : {1.0f, -1.0f}) {
        const float lx = side * 1.6f * TrainSystem::kScale * 0.62f;
        Vec3 p = c.pos + Vec3(lx * cs, 0.2f, lx * sn);
        if (!player_.collides(*world_, p)) {
            player_.pos = p;
            return;
        }
    }
    player_.pos = c.pos + Vec3(0, 2.4f * TrainSystem::kScale, 0);  // on the roof, if both sides are blocked
}

void Session::drawHud(Ui& ui) const {
    {
        // 2.1.0 preview: the train buttons on the right, with the speed while driving.
        const TrainButtons b = trainButtons();
        for (int i = 0; i < b.count; ++i) {
            const float by = b.y + i * (b.h + b.gap);
            const bool on = std::string(b.labels[i]) == "Brzda" && trainSeatTrain_ >= 0 && trains_.trains()[(size_t)trainSeatTrain_].brake;
            ui.hudButton(b.x, by, b.w, b.h, on);
            ui.textCentered(b.x + b.w / 2, by + 6, b.labels[i], Color{1, 1, 1, 1});
        }
        if (trainSeatTrain_ >= 0 && b.count > 1) {
            const Train& t = trains_.trains()[(size_t)trainSeatTrain_];
            ui.textCentered(b.x + b.w / 2 - 20, b.y + b.count * (b.h + b.gap) + 2,
                            t.autoMode ? trf("{0} km/h  ·  automat", {std::to_string((int)std::lround(std::fabs(t.speed) * 3.6f))})
                                       : trf("{0} km/h  ·  plyn {1} %", {std::to_string((int)std::lround(std::fabs(t.speed) * 3.6f)),
                                                            std::to_string((int)std::lround(t.throttle * 100))}),
                            Color{1, 1, 0.8f, 1});
        }
    }
    if (oldMode()) {
        drawOldHud(ui);
        return;
    }
    Layout l = layout();
    const float W = guiW_, H = guiH_;

    if (portalTime_ > 0) ui.fill(0, 0, W, H, {0.45f, 0.1f, 0.75f, std::min(portalTime_ / kPortalDelay, 1.0f) * 0.75f});
    if (state_.inventory.slots[state_.selectedSlot].id == BOSS_COMPASS) {
        // The arena compass: an arrow towards the nearest arena whose boss still lives.
        const float cx = W / 2, cy = 52, r = 13;
        ui.fill(cx - r - 2, cy - r - 2, 2 * r + 4, 2 * r + 4, {0, 0, 0, 0.45f});
        int ax, ay, az;
        if (world_ == &void_ && void_.nearestStructure((int)player_.pos.x, (int)player_.pos.z, ax, ay, az)) {
            float want = std::atan2(ax + 0.5f - player_.pos.x, -(az + 0.5f - player_.pos.z));
            float a = want - player_.yaw;
            float sx = std::sin(a), sy = -std::cos(a);
            float px = -sy, py = sx;  // perpendicular, for the arrow's width
            const float pts[4][2] = {{cx + sx * r, cy + sy * r}, {cx + px * 4, cy + py * 4}, {cx - sx * 4, cy - sy * 4}, {cx - px * 4, cy - py * 4}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.9f, 0.15f, 0.25f, 1});
            int dist = (int)std::hypot(ax - player_.pos.x, az - player_.pos.z);
            ui.textCentered(cx, cy + r + 4, std::to_string(dist) + " m", {1, 0.8f, 0.8f, 1});
        } else {
            // Away from the Crimson Void the needle just spins.
            float a = clock_ * 6;
            const float pts[4][2] = {{cx + std::sin(a) * r, cy - std::cos(a) * r}, {cx + std::cos(a) * 3, cy + std::sin(a) * 3},
                                     {cx - std::sin(a) * 3, cy + std::cos(a) * 3}, {cx - std::cos(a) * 3, cy - std::sin(a) * 3}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.6f, 0.6f, 0.6f, 1});
        }
    }
    if (state_.inventory.slots[state_.selectedSlot].id == CELESTIAL_COMPASS) {
        // 1.7.0 preview: in the Crimson Void it points to the nearest Celestial portal frame.
        const float cx = W / 2, cy = 52, r = 13;
        ui.fill(cx - r - 2, cy - r - 2, 2 * r + 4, 2 * r + 4, {0, 0, 0, 0.45f});
        int fx, fz;
        if (world_ == &void_ && void_.nearestCelestialFrame((int)player_.pos.x, (int)player_.pos.z, fx, fz)) {
            const float a = std::atan2(fx + 0.5f - player_.pos.x, -(fz + 0.5f - player_.pos.z)) - player_.yaw;
            const float sx = std::sin(a), sy = -std::cos(a), px = -sy, py = sx;
            const float pts[4][2] = {{cx + sx * r, cy + sy * r}, {cx + px * 4, cy + py * 4}, {cx - sx * 4, cy - sy * 4}, {cx - px * 4, cy - py * 4}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.75f, 0.85f, 1.0f, 1});
            ui.textCentered(cx, cy + r + 4, std::to_string((int)std::hypot(fx - player_.pos.x, fz - player_.pos.z)) + " m", {0.85f, 0.9f, 1, 1});
        } else {
            const float a = clock_ * 6;
            const float pts[4][2] = {{cx + std::sin(a) * r, cy - std::cos(a) * r}, {cx + std::cos(a) * 3, cy + std::sin(a) * 3},
                                     {cx - std::sin(a) * 3, cy + std::cos(a) * 3}, {cx - std::cos(a) * 3, cy - std::sin(a) * 3}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.6f, 0.6f, 0.7f, 1});
        }
    }
    if (state_.inventory.slots[state_.selectedSlot].id == FORTRESS_COMPASS) {
        // 1.5.0: the fortress compass. In the Cornerstone Dimension it points at the nearest
        // fortress; in the Overworld at the place where a portal leads close to one.
        const float cx = W / 2, cy = 52, r = 13;
        ui.fill(cx - r - 2, cy - r - 2, 2 * r + 4, 2 * r + 4, {0, 0, 0, 0.45f});
        int fx, fy, fz;
        bool found = false;
        float tx = 0, tz = 0;
        if (world_ == &nether_ && nether_.nearestStructure((int)player_.pos.x, (int)player_.pos.z, fx, fy, fz)) {
            found = true, tx = fx + 0.5f, tz = fz + 0.5f;
        } else if (world_ == &overworld_ && nether_.nearestStructure((int)std::floor(player_.pos.x / 8), (int)std::floor(player_.pos.z / 8), fx, fy, fz)) {
            found = true, tx = fx * 8 + 4.0f, tz = fz * 8 + 4.0f;
        }
        if (found) {
            const float a = std::atan2(tx - player_.pos.x, -(tz - player_.pos.z)) - player_.yaw;
            const float sx = std::sin(a), sy = -std::cos(a), px = -sy, py = sx;
            const float pts[4][2] = {{cx + sx * r, cy + sy * r}, {cx + px * 4, cy + py * 4}, {cx - sx * 4, cy - sy * 4}, {cx - px * 4, cy - py * 4}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.2f, 0.85f, 0.95f, 1});
            const int dist = (int)std::hypot(tx - player_.pos.x, tz - player_.pos.z);
            ui.textCentered(cx, cy + r + 4, std::to_string(dist) + " m", {0.75f, 0.95f, 1, 1});
            if (world_ == &overworld_ && dist < 12) ui.textCentered(cx, cy + r + 14, "Tady postav portál", {0.75f, 0.95f, 1, 1});
        } else {
            const float a = clock_ * 6;
            const float pts[4][2] = {{cx + std::sin(a) * r, cy - std::cos(a) * r}, {cx + std::cos(a) * 3, cy + std::sin(a) * 3},
                                     {cx - std::sin(a) * 3, cy + std::cos(a) * 3}, {cx - std::cos(a) * 3, cy - std::sin(a) * 3}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.6f, 0.6f, 0.6f, 1});
        }
    }
    if (state_.inventory.slots[state_.selectedSlot].id == EXPLORER_MAP || state_.offhand.id == EXPLORER_MAP) {
        // 1.5.7: the explorer map: a paper with a red needle towards the nearest mineshaft.
        const float cx = W / 2, cy = 52, r = 13;
        ui.fill(cx - r - 3, cy - r - 3, 2 * r + 6, 2 * r + 6, {0.55f, 0.45f, 0.3f, 0.85f});
        ui.fill(cx - r - 1, cy - r - 1, 2 * r + 2, 2 * r + 2, {0.87f, 0.81f, 0.62f, 0.95f});
        int mx, my, mz;
        if ((world_ == &overworld_ || world_ == &nether_) && world_->nearestMineshaft((int)player_.pos.x, (int)player_.pos.z, mx, my, mz)) {
            const float a = std::atan2(mx + 0.5f - player_.pos.x, -(mz + 0.5f - player_.pos.z)) - player_.yaw;
            const float sx = std::sin(a), sy = -std::cos(a), px = -sy, py = sx;
            const float pts[4][2] = {{cx + sx * r, cy + sy * r}, {cx + px * 4, cy + py * 4}, {cx - sx * 4, cy - sy * 4}, {cx - px * 4, cy - py * 4}};
            const float uvs[4][2] = {};
            ui.quad(TexNone, pts, uvs, {0.8f, 0.12f, 0.1f, 1});
            const int dist = (int)std::hypot(mx - player_.pos.x, mz - player_.pos.z);
            ui.textCentered(cx, cy + r + 5, trf("Důl: {0} m", {std::to_string(dist)}), {1, 0.92f, 0.75f, 1});
            if (dist < 16) achieve("explorer");
        } else {
            ui.textCentered(cx, cy - 4, "?", {0.4f, 0.3f, 0.2f, 1});
        }
    }
    if (const Mob* boss = mobs_.boss(dimIndex()); boss && (boss->pos - player_.pos).length() < 60) {
        // Boss bar under the pause button.
        const float bw = std::min(220.0f, W - 120), bx = std::floor((W - bw) / 2), by = 27;
        float f = std::clamp(boss->health / mobInfo(boss->type).health, 0.0f, 1.0f);
        ui.textCentered(W / 2, by, mobInfo(boss->type).name, {1, 0.4f, 0.4f, 1});
        ui.fill(bx - 1, by + 10, bw + 2, 7, {0, 0, 0, 0.8f});
        ui.fill(bx, by + 11, bw * f, 5, {0.8f, 0.08f, 0.1f, 1});
    }
    if (sleepTimer_ > 0) {
        float a = std::min(1.0f, sleepTimer_ / 1.4f);
        ui.fill(0, 0, W, H, {0.02f, 0.02f, 0.06f, a * 0.92f});
        if (a > 0.6f) ui.textCentered(W / 2, H / 2 - 4, "Zzz...", {1, 1, 1, (a - 0.6f) * 2.5f});
    }

    // Name tags over the other players of a LAN game.
    if (!remotePlayers.empty()) {
        Mat4 mvp = Mat4::perspective(fov_ * kPi / 180.0f, W / H, 0.05f, 1000) *
                   Mat4::lookDir(camera_.eye(), camera_.forward(), {0, 1, 0});
        for (const RemoteAvatar& a : remotePlayers) {
            if (a.dim != dimIndex()) continue;
            Vec3 p = a.pose.feet + Vec3(0, 2.15f, 0);
            const float* m = mvp.m;
            float cx = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
            float cy = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
            float cw = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
            if (cw < 0.1f || (p - camera_.eye()).length() > 48) continue;
            float sx = (cx / cw * 0.5f + 0.5f) * W, sy = (0.5f - cy / cw * 0.5f) * H;
            float tw = ui.textWidth(a.name) + 6;
            if (sx + tw / 2 < 0 || sx - tw / 2 > W || sy < -10 || sy > H) continue;
            ui.fill(std::floor(sx - tw / 2), std::floor(sy - 6), tw, 11, {0, 0, 0, 0.45f});
            ui.text(std::floor(sx - tw / 2) + 3, std::floor(sy - 5), a.name, {1, 1, 1, 1}, 1, false);
        }
    }
    if (hurtFlash_ > 0 && !settings_.calmMode) ui.fill(0, 0, W, H, {0.8f, 0, 0, hurtFlash_});

    // Hotbar: nine slots and the "..." inventory button.
    if (Ui::preAlpha) ui.fill(l.hotbarX - 2, l.hotbarY - 2, l.slot * 9 + 4, l.slot + 4, {0.12f, 0.12f, 0.12f, 0.85f});  // 0.3 look
    else ui.frame(l.hotbarX - 3, l.hotbarY - 3, l.slot * 9 + 6, l.slot + 6, {0.08f, 0.07f, 0.06f, 0.55f}, {0.85f, 0.85f, 0.8f, 0.4f});
    for (int i = 0; i < kHotbarSize; ++i) {
        float x = l.hotbarX + i * l.slot;
        // Sunken slot: dark top-left edge, light bottom-right edge.
        ui.fill(x + 1, l.hotbarY + 1, l.slot - 2, l.slot - 2, {0.14f, 0.13f, 0.12f, 0.75f});
        ui.fill(x + 1, l.hotbarY + 1, l.slot - 2, 1, {0, 0, 0, 0.35f});
        ui.fill(x + 1, l.hotbarY + l.slot - 2, l.slot - 2, 1, {1, 1, 1, 0.08f});
        const ItemStack& s = state_.inventory.slots[i];
        if (!s.empty()) {
            ui.item(x + 2, l.hotbarY + 2, l.slot - 4, s.id);
            if (survival() && s.count > 1) {
                std::string n = std::to_string(s.count);
                ui.text(x + l.slot - 1 - ui.textWidth(n), l.hotbarY + l.slot - 9, n);
            }
        }
    }
    {
        // 1.5.7: the off hand, left of the hotbar.
        const float ox = l.hotbarX - l.slot - 4;
        ui.fill(ox - 1, l.hotbarY - 1, l.slot + 2, l.slot + 2, {0, 0, 0, 0.6f});
        ui.fill(ox + 1, l.hotbarY + 1, l.slot - 2, l.slot - 2, {0.14f, 0.13f, 0.12f, 0.75f});
        if (!state_.offhand.empty()) {
            ui.item(ox + 2, l.hotbarY + 2, l.slot - 4, state_.offhand.id);
            if (survival() && state_.offhand.count > 1) {
                std::string n = std::to_string(state_.offhand.count);
                ui.text(ox + l.slot - 1 - ui.textWidth(n), l.hotbarY + l.slot - 9, n);
            }
        } else {
            ui.tile(ox + 3, l.hotbarY + 3, l.slot - 6, T_SHIELD, {0, 0, 0, 0.3f});
        }
    }
    float sx = l.hotbarX + state_.selectedSlot * l.slot;
    // Selected slot: the theme's moss green frame.
    const Color sel = Ui::preAlpha ? Color{0.95f, 0.95f, 0.95f, 1} : theme::kGreenBorder;
    ui.fill(sx - 1, l.hotbarY - 1, l.slot + 2, 2, sel);
    ui.fill(sx - 1, l.hotbarY + l.slot - 1, l.slot + 2, 2, sel);
    ui.fill(sx - 1, l.hotbarY - 1, 2, l.slot + 2, sel);
    ui.fill(sx + l.slot - 1, l.hotbarY - 1, 2, l.slot + 2, sel);
    bool invHeld = false;
    for (const Touch& t : touches_) invHeld |= t.id >= 0 && t.kind == Touch::InventoryButton;
    ui.hudButton(l.hotbarX + l.slot * 9 + 1, l.hotbarY, l.slot, l.slot, invHeld);
    ui.textCentered(l.hotbarX + l.slot * 9.5f + 1, l.hotbarY + 6, "...", theme::kTextBright);

    // Name of the item just selected, fading out.
    const ItemStack& held = state_.inventory.slots[state_.selectedSlot];
    if (itemNameTimer_ > 0 && !held.empty())
        ui.textCentered(W / 2, l.hotbarY - (survival() ? 34 : 14), itemName(held.id), {1, 1, 1, std::min(1.0f, itemNameTimer_)});

    if (survival()) {
        // 1.7.0 UI: a health bar with a heart on the left, a food bar with a leaf on the right,
        // and the armour shield between them (brighter with more armour points).
        const float rowY = l.hotbarY - 13, total = l.slot * 9, gapW = 14, barW = std::floor((total - gapW) / 2);
        const bool blink = hurtFlash_ > 0;
        auto bar = [&](float x, float frac, Color fillC) {
            ui.frame(x, rowY, barW, 9, {0.08f, 0.07f, 0.06f, 0.75f}, {0.85f, 0.85f, 0.8f, 0.45f});
            const float fw = std::floor((barW - 4) * std::clamp(frac, 0.0f, 1.0f));
            if (fw > 0) {
                ui.fill(x + 2, rowY + 2, fw, 5, fillC);
                ui.fill(x + 2, rowY + 2, fw, 2, {1, 1, 1, 0.2f});
            }
        };
        bar(l.hotbarX, state_.health / 20.0f, blink ? Color{1, 1, 1, 1} : Color{0.86f, 0.14f, 0.12f, 1});
        ui.icon(l.hotbarX + 2, rowY + 1, Font::HeartFull, {1, 0.3f, 0.3f, 1});
        const float fx = l.hotbarX + total - barW;
        bar(fx, state_.food / 20.0f, Color{0.93f, 0.55f, 0.12f, 1});
        ui.icon(fx + barW - 10, rowY + 1, Font::FoodFull, {1, 0.75f, 0.3f, 1});
        const int points = armorPoints();
        ui.icon(l.hotbarX + barW + (gapW - 7) / 2, rowY + 1, points > 0 ? Font::ArmorFull : Font::ArmorEmpty,
                points > 0 ? Color{0.85f, 0.88f, 0.95f, 1} : Color{0.6f, 0.6f, 0.6f, 0.7f});
        {
            // 1.9.5: the temperature, a small labelled chip above the health bar.
            static const char* const kNames[4] = {"Velká zima", "Zima", "Teplo", "Horko"};
            static const Color kColors[4] = {{0.55f, 0.8f, 1, 1}, {0.75f, 0.9f, 1, 1}, {0.95f, 0.85f, 0.55f, 1}, {1, 0.45f, 0.2f, 1}};
            const int lv = temperatureLevel() + 2;
            const std::string name = tr(kNames[lv]);
            // Centred over the bars, above the armour shield in the middle.
            const float tw = ui.textWidth(name) + 8, tx = std::floor(l.hotbarX + total / 2 - tw / 2);
            ui.frame(tx, rowY - 11, tw, 10, {0.08f, 0.07f, 0.06f, 0.7f}, kColors[lv]);
            ui.text(tx + 4, rowY - 10, name, kColors[lv], 1, false);
        }
        float right = l.hotbarX + l.slot * 9;
        if (state_.air < kMaxAir) {
            int bubbles = (int)std::ceil(state_.air / kMaxAir * 10);
            // 1.9.5: centred above the temperature chip.
            const float bx = std::floor(l.hotbarX + total / 2 - bubbles * 4);
            for (int i = 0; i < bubbles; ++i) ui.icon(bx + i * 8, rowY - 22, Font::Bubble, {0.6f, 0.8f, 1, 1});
        }
    }

    // Jump (and fly down) buttons
    bool jumpHeld = false, downHeld = false, pauseHeld = false, chatHeld = false, cameraHeld = false;
    bool pad[3][3] = {};
    for (const Touch& t : touches_) {
        if (t.id < 0) continue;
        jumpHeld |= t.kind == Touch::Jump;
        downHeld |= t.kind == Touch::FlyDown;
        pauseHeld |= t.kind == Touch::Pause;
        chatHeld |= t.kind == Touch::Chat;
        cameraHeld |= t.kind == Touch::Camera;
        int dx, dz;
        if (t.kind == Touch::DPad && dpadDirection(l, t.x, t.y, dx, dz)) pad[1 - dz][dx + 1] = true;
    }
    if (kFeatures220) {
        // 2.2.0 preview: the project's own button pictures.
        ui.hudPicture(l.jumpX, l.jumpY, l.buttonSize, l.buttonSize, Font::HudJump, jumpHeld);
        if (player_.flying) ui.hudPicture(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize, Font::HudDown, downHeld);
        else if (riding_ < 0) ui.hudPicture(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize, Font::HudSneak, player_.sneaking);
    } else {
    ui.hudButton(l.jumpX, l.jumpY, l.buttonSize, l.buttonSize, jumpHeld);
    ui.hudGlyph(l.jumpX, l.jumpY, l.buttonSize, l.buttonSize, Font::ArrowUp);
    if (player_.flying) {
        ui.hudButton(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize, downHeld);
        ui.hudGlyph(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize, Font::ArrowDown);
    } else if (riding_ < 0) {
        // 1.5.7: the crouch button stays pressed while crouching.
        ui.hudButton(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize, player_.sneaking);
        ui.hudGlyph(l.flyDownX, l.jumpY, l.buttonSize, l.buttonSize, Font::ArrowDown);
    }
    }

    // D-pad: four arrow buttons around an empty centre; corner presses light up both arrows.
    const float b = l.padButton;
    const int arrows[4][3] = {{1, 0, Font::ArrowUp}, {0, 1, Font::ArrowLeft}, {2, 1, Font::ArrowRight}, {1, 2, Font::ArrowDown}};
    for (const auto& a : arrows) {
        int col = a[0], row = a[1];
        bool on = pad[row][col] || (row == 0 && (pad[0][0] || pad[0][2])) || (row == 2 && (pad[2][0] || pad[2][2])) ||
                  (col == 0 && (pad[0][0] || pad[2][0])) || (col == 2 && (pad[0][2] || pad[2][2]));
        float bx = l.padX + col * b, by = l.padY + row * b;
        if (kFeatures220) {
            const int pic = a[2] == Font::ArrowUp ? Font::HudUp : a[2] == Font::ArrowDown ? Font::HudDown : a[2] == Font::ArrowLeft ? Font::HudLeft : Font::HudRight;
            ui.hudPicture(bx + 1, by + 1, b - 2, b - 2, pic, on);
            continue;
        }
        ui.hudButton(bx + 1, by + 1, b - 2, b - 2, on);
        ui.hudGlyph(bx + 1, by + 1, b - 2, b - 2, a[2], b >= 26 ? 2 : 1);
    }

    // The map item in hand: the paper sheet, big, with the player on it.
    if (paperShown_) {
        const float ms = std::min(H * 0.62f, 170.0f), mx = W / 2 - ms / 2, my = 26, k = ms / PaperMaps::kSize;
        ui.fill(mx - 5, my - 5, ms + 10, ms + 10, {0.42f, 0.3f, 0.18f, 0.95f});
        ui.fill(mx - 3, my - 3, ms + 6, ms + 6, {0.89f, 0.83f, 0.66f, 1});
        ui.image(TexMap, mx, my, ms, ms, 0, 0, 1, 1);
        const float cxp = mx + std::clamp(player_.pos.x - paperOriginX_, 0.0f, (float)PaperMaps::kSize) * k;
        const float czp = my + std::clamp(player_.pos.z - paperOriginZ_, 0.0f, (float)PaperMaps::kSize) * k;
        const float fx = std::sin(player_.yaw), fz = -std::cos(player_.yaw), r = 5.0f;
        const float pts[4][2] = {{cxp + fx * r, czp + fz * r},
                                 {cxp - fx * r * 0.7f + fz * r * 0.7f, czp - fz * r * 0.7f - fx * r * 0.7f},
                                 {cxp - fx * r * 0.25f, czp - fz * r * 0.25f},
                                 {cxp - fx * r * 0.7f - fz * r * 0.7f, czp - fz * r * 0.7f + fx * r * 0.7f}};
        const float uv[4][2] = {};
        ui.quad(TexNone, pts, uv, {0.75f, 0.1f, 0.1f, 1});
        ui.text(mx + ms / 2 - 2, my + 2, "N", {0.3f, 0.2f, 0.1f, 1}, 1, false);
    }
    // Minimap, top right: terrain, places and the player as an arrow (north is up).
    if (l.mapSize > 0 && !paperShown_) {
        const float mx = l.mapX, my = l.mapY, ms = l.mapSize, k = ms / Minimap::kSize;
        ui.frame(mx - 3, my - 3, ms + 6, ms + 6, {0, 0, 0, 0.55f}, {0.85f, 0.85f, 0.8f, 0.55f});
        ui.image(TexMap, mx, my, ms, ms, 0, 0, 1, 1);
        for (const MapMarker& mk : mapMarkers()) {
            // Places outside the map sit on its edge, in their direction.
            float fx = std::clamp((mk.x - minimap_.originX() + 0.5f) * k, 3.0f, ms - 3);
            float fz = std::clamp((mk.z - minimap_.originZ() + 0.5f) * k, 3.0f, ms - 3);
            int glyph = mk.kind == MapMarker::Spawn ? Font::Home : mk.kind == MapMarker::Death ? Font::Skull : Font::House;
            Color c = mk.kind == MapMarker::Spawn ? Color{1, 0.9f, 0.3f, 1} : mk.kind == MapMarker::Death ? Color{1, 0.35f, 0.3f, 1}
                      : mk.kind == MapMarker::Station ? Color{1, 0.6f, 0.25f, 1}
                      : mk.kind == MapMarker::Found ? foundColor(mk.type) : Color{0.6f, 0.9f, 1, 1};
            ui.icon(mx + fx - 4, my + fz - 4, glyph, {0, 0, 0, 0.7f});
            ui.icon(mx + fx - 4.5f, my + fz - 4.5f, glyph, c);
        }
        // The player: an arrow pointing where they look.
        const float cxp = mx + (player_.pos.x - minimap_.originX()) * k, czp = my + (player_.pos.z - minimap_.originZ()) * k;
        const float fx = std::sin(player_.yaw), fz = -std::cos(player_.yaw), r = 4.5f;
        const float tip[2] = {cxp + fx * r, czp + fz * r};
        const float left[2] = {cxp - fx * r * 0.7f - fz * r * 0.7f, czp - fz * r * 0.7f + fx * r * 0.7f};
        const float right[2] = {cxp - fx * r * 0.7f + fz * r * 0.7f, czp - fz * r * 0.7f - fx * r * 0.7f};
        const float back[2] = {cxp - fx * r * 0.25f, czp - fz * r * 0.25f};
        const float pts[4][2] = {{tip[0], tip[1]}, {right[0], right[1]}, {back[0], back[1]}, {left[0], left[1]}};
        const float uv[4][2] = {};
        ui.quad(TexNone, pts, uv, {1, 1, 1, 1});
        ui.text(mx + ms / 2 - 2, my + 2, "N", {1, 1, 1, 0.9f}, 1, true);
    }

    // Pause button, top centre
    if (kFeatures220) {
        ui.hudPicture(l.pauseX, l.pauseY, l.pauseSize, l.pauseSize, Font::HudPause, pauseHeld);
    } else {
        ui.hudButton(l.pauseX, l.pauseY, l.pauseSize, l.pauseSize, pauseHeld);
        ui.fill(l.pauseX + 5, l.pauseY + 4, 3, l.pauseSize - 8, theme::kTextBright);
        ui.fill(l.pauseX + l.pauseSize - 8, l.pauseY + 4, 3, l.pauseSize - 8, theme::kTextBright);
    }
    // Chat button: a speech bubble.
    if (kFeatures220) {
        ui.hudPicture(l.chatX, l.pauseY, l.pauseSize, l.pauseSize, Font::HudChat, chatHeld);
        ui.hudPicture(l.cameraX, l.pauseY, l.pauseSize, l.pauseSize, Font::HudCamera, cameraHeld);
    } else {
    ui.hudButton(l.chatX, l.pauseY, l.pauseSize, l.pauseSize, chatHeld);
    ui.fill(l.chatX + 4, l.pauseY + 4, l.pauseSize - 8, 7, theme::kTextBright);
    ui.fill(l.chatX + 6, l.pauseY + 11, 3, 2, theme::kTextBright);
    ui.fill(l.chatX + 5, l.pauseY + 13, 2, 1, theme::kTextBright);
    ui.fill(l.chatX + 6, l.pauseY + 6, l.pauseSize - 12, 1, {0.3f, 0.3f, 0.3f, 1});
    ui.fill(l.chatX + 6, l.pauseY + 8, l.pauseSize - 15, 1, {0.3f, 0.3f, 0.3f, 1});
    }

    if (info_.showCoordinates) {
        // Position and biome, top left.
        char buf[96];
        std::snprintf(buf, sizeof buf, "X: %d  Y: %d  Z: %d", (int)std::floor(player_.pos.x), (int)std::floor(player_.pos.y),
                      (int)std::floor(player_.pos.z));
        ui.frame(2, 2, ui.textWidth(buf) + 6, 12, {0.08f, 0.07f, 0.06f, 0.5f}, {0.85f, 0.85f, 0.8f, 0.35f});
        ui.text(5, 4, buf);
        const int px = (int)std::floor(player_.pos.x), pz = (int)std::floor(player_.pos.z);
        std::string where = tr(world_->placeName(px, pz));
        if (world_ == &overworld_) {
            float t = (float)std::fmod(state_.dayTime, 24000.0);
            std::string day = trf("Den {0} · {1}", {std::to_string((long long)(state_.dayTime / 24000) + 1), tr(dayPhaseName(dayPhase(t)))});
            where = where.empty() ? day : where + "  ·  " + day;
        }
        if (!where.empty()) {
            ui.fill(2, 15, ui.textWidth(where) + 6, 12, {0, 0, 0, 0.45f});
            ui.text(5, 17, where);
        }
    }
    if (settings_.devDebug) {
        // Developer overlay, top left below the coordinates.
        const int bx = (int)std::floor(player_.pos.x), by = (int)std::floor(player_.pos.y), bz = (int)std::floor(player_.pos.z);
        char lines[6][96];
        std::snprintf(lines[0], 96, "FPS %.0f  dt %.1f ms", fps_, fps_ > 0 ? 1000 / fps_ : 0);
        std::snprintf(lines[1], 96, "XYZ %.2f %.2f %.2f", player_.pos.x, player_.pos.y, player_.pos.z);
        std::snprintf(lines[2], 96, "Chunk %d %d  in chunk %d %d", floorDiv(bx, kChunkSize), floorDiv(bz, kChunkSize),
                      floorMod(bx, kChunkSize), floorMod(bz, kChunkSize));
        std::snprintf(lines[3], 96, "Chunks %zu  drops %zu  particles %zu", world_->chunks().size(), drops_.drops().size(),
                      particles_.count());
        std::snprintf(lines[4], 96, "Block below %s  sky %d", itemKey(world_->getBlock(bx, by - 1, bz)), world_->skyHeight(bx, bz));
        std::snprintf(lines[5], 96, "Yaw %.0f  pitch %.0f  %s%s", player_.yaw * 57.3f, player_.pitch * 57.3f,
                      player_.flying ? "fly " : "", player_.noclip ? "noclip" : "");
        float y = info_.showCoordinates ? 30 : 4;
        for (const char* line : lines) {
            ui.fill(2, y - 2, ui.textWidth(line) + 6, 11, {0, 0, 0, 0.55f});
            ui.text(5, y, line, {0.6f, 1, 0.6f, 1});
            y += 11;
        }
    }
}

}  // namespace vc
