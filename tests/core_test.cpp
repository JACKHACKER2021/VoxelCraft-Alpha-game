// Desktop sanity test for the platform-independent engine core.
// Build & run: tests/run_tests.sh
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "engine/Blocks.h"
#include "engine/Crafting.h"
#include "engine/GenConfig.h"
#include "engine/Json.h"
#include "engine/Lang.h"
#include "engine/Developer.h"
#include "stub_capture.h"
#include "engine/BlockShapes.h"
#include "engine/Loot.h"
#include "engine/WorldStructures.h"
#include "engine/Particles.h"
#include "engine/Audio.h"
#include "engine/PlayerModel.h"
#include "engine/Profile.h"
#include "engine/Achievements.h"
#include "engine/Multiplayer.h"
#include "engine/Commands.h"
#include "engine/Mobs.h"
#include "engine/Sky.h"
#include "engine/Portal.h"
#include "engine/Loot.h"
#include "engine/Net.h"
#include "engine/Skin.h"
#include "engine/Store.h"
#include "engine/TexturePack.h"
#include "engine/Packs.h"
#include "engine/Font.h"
#include "engine/Game.h"
#include "engine/Items.h"
#include "engine/Save.h"
#include "engine/Shaders.h"
#include "engine/Session.h"
#include "engine/Mesher.h"
#include "engine/Player.h"
#include "engine/Portal.h"
#include "engine/TexturePack.h"
#include "engine/World.h"
#include "engine/Nbt.h"
#include "engine/Structure.h"
#include "engine/Rewards.h"
#include "engine/Trees.h"
#include "engine/Trading.h"
#include "engine/Mods.h"
#include "engine/JavaImport.h"
#include "engine/WorldFiles.h"
#include "engine/BlockShapes.h"

using namespace vc;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                   \
        }                                                               \
    } while (0)

static int testOverworld() {
    World w(42, Dimension::Overworld);
    w.ensureChunksAround(0, 0, 3, 100);
    CHECK(w.chunks().size() >= 29);
    CHECK(w.getBlock(5, 0, 5) == WORLD_END);
    int h = w.surfaceHeight(8, 8);
    CHECK(h > 4 && h < kChunkHeight - 10);

    ChunkMesh mesh;
    CHECK(canMesh(w, 0, 0));
    buildChunkMesh(w, *w.getChunk(0, 0), mesh);
    CHECK(!mesh.opaque.empty());
    CHECK(mesh.opaque.size() % 6 == 0);
    std::printf("overworld: surface %d, chunk(0,0) %zu opaque / %zu translucent verts\n", h, mesh.opaque.size(),
                mesh.translucent.size());

    // Dropping a player above the surface must land on solid ground.
    int x = 8, z = 8;
    for (int i = 0; i < 64 && w.surfaceHeight(x, z) <= kSeaLevel; ++i) x += 16;
    w.ensureChunksAround(x / 16, 0, 2, 100);
    Player p;
    p.pos = Vec3(x + 0.5f, (float)w.surfaceHeight(x, z) + 5, z + 0.5f);
    for (int i = 0; i < 200; ++i) p.update(w, 1 / 60.0f, 0, 0, false);
    CHECK(p.onGround);
    CHECK(isSolid(w.getBlock(x, (int)std::floor(p.pos.y - 0.01f), z)));

    // Looking straight down hits the block under the feet.
    p.pitch = -1.55f;
    RayHit hit = raycast(w, p.eye(), p.forward(), Player::kReach);
    CHECK(hit.hit && hit.ny == 1);
    return 0;
}

static int testNether() {
    World w(42, Dimension::Nether);
    w.ensureChunksAround(0, 0, 3, 100);
    int netherrack = 0, lava = 0, air = 0, glow = 0;
    for (auto& [k, c] : w.chunks())
        for (ItemId b : c->blocks) {
            netherrack += b == NETHERRACK;
            lava += b == LAVA;
            air += b == AIR;
            glow += b == POWERSTONE_LIGHT;
        }
    std::printf("nether: netherrack %d, lava %d, air %d, glowstone %d\n", netherrack, lava, air, glow);
    CHECK(netherrack > 0 && lava > 0 && air > 0);
    CHECK(w.getBlock(3, 0, 3) == WORLD_END && w.getBlock(3, kChunkHeight - 1, 3) == BEDROCK);
    for (int y = 1; y <= kNetherLavaLevel; ++y) CHECK(w.getBlock(0, y, 0) != AIR);  // no air below lava level
    return 0;
}

static int testPortal() {
    World w(7, Dimension::Overworld);
    w.ensureChunksAround(0, 0, 2, 100);
    // Hand-build a 4x5 frame in the air at y=100 along the Z axis.
    const int x = 4, y = 100, z = 4;
    for (int dz = -1; dz <= 2; ++dz) { w.setBlock(x, y - 1, z + dz, OBSIDIAN); w.setBlock(x, y + 3, z + dz, OBSIDIAN); }
    for (int dy = 0; dy < 3; ++dy) { w.setBlock(x, y + dy, z - 1, OBSIDIAN); w.setBlock(x, y + dy, z + 2, OBSIDIAN); }
    CHECK(!tryLightPortal(w, x + 1, y, z));     // outside the frame
    CHECK(tryLightPortal(w, x, y + 1, z + 1));  // anywhere inside works
    for (int dz = 0; dz < 2; ++dz)
        for (int dy = 0; dy < 3; ++dy) CHECK(w.getBlock(x, y + dy, z + dz) == PORTAL);

    // Breaking a frame block collapses the portal.
    w.setBlock(x, y + 3, z, AIR);
    collapsePortal(w, x, y + 2, z);
    CHECK(w.getBlock(x, y, z) == AIR && w.getBlock(x, y + 2, z + 1) == AIR);

    // Incomplete frame does not light.
    CHECK(!tryLightPortal(w, x, y + 1, z + 1));

    // Travelling: the Nether gets a new portal, a second trip reuses it.
    World nether(7, Dimension::Nether);
    nether.ensureChunksAround(0, 0, 3, 100);
    Vec3 a = findOrBuildPortal(nether, 10, 10);
    CHECK(nether.getBlock((int)std::floor(a.x), (int)a.y, (int)std::floor(a.z)) == PORTAL);
    CHECK(isSolid(nether.getBlock((int)std::floor(a.x), (int)a.y - 1, (int)std::floor(a.z))));
    Vec3 b = findOrBuildPortal(nether, 12, 9);
    CHECK(std::fabs(a.y - b.y) < 0.01f && std::fabs(a.x - b.x) <= 1.5f && std::fabs(a.z - b.z) <= 1.0f);
    std::printf("portal: nether portal at %.1f %.1f %.1f\n", a.x, a.y, a.z);

    Player p;
    p.pos = a;
    CHECK(p.touchesBlock(nether, PORTAL));
    return 0;
}

void writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

static const uint8_t* atlasPixel(const Atlas& a, int tile, int px, int py) {
    int res = a.size / kAtlasCols;
    return &a.rgba[(((tile / kAtlasCols) * res + py) * a.size + (tile % kAtlasCols) * res + px) * 4];
}

static int testTexturePacks() {
    namespace fs = std::filesystem;
    int fbSize = 0;
    auto fallback = buildFallbackAtlas(fbSize);
    fs::path root = fs::temp_directory_path() / "vc_pack_test";
    fs::remove_all(root);

    // 1) Classic terrain.png at 32px per tile: stone slot solid red, grass top slot gray.
    fs::create_directories(root / "classic");
    std::vector<uint8_t> terrain(512 * 512 * 4, 0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            uint8_t* stone = &terrain[(y * 512 + 32 + x) * 4];  // slot 1
            stone[0] = 200; stone[3] = 255;
            uint8_t* grass = &terrain[(y * 512 + x) * 4];       // slot 0
            grass[0] = grass[1] = grass[2] = 255; grass[3] = 255;
        }
    writePng((root / "classic/terrain.png").string(), 512, 512, terrain);
    Atlas a = loadTexturePack((root / "classic").string(), fallback, fbSize);
    CHECK(a.size == 32 * kAtlasCols);
    CHECK(atlasPixel(a, T_STONE, 5, 5)[0] == 200 && atlasPixel(a, T_STONE, 5, 5)[1] == 0);
    const uint8_t* g = atlasPixel(a, T_GRASS_TOP, 3, 3);
    CHECK(g[1] > g[0] && g[1] > g[2]);  // gray grass got tinted green
    // Empty slots (e.g. dirt here) keep the placeholder.
    CHECK(atlasPixel(a, T_DIRT, 0, 0)[3] == 255);
    std::printf("texture pack: classic terrain.png -> %d tiles at %dpx\n", a.tilesFromPack, a.size / kAtlasCols);

    // 2) Bedrock-style single textures, animated water strip uses the first frame.
    fs::create_directories(root / "bedrock/blocks");
    std::vector<uint8_t> blue(16 * 16 * 4);
    for (size_t i = 0; i < blue.size(); i += 4) { blue[i + 2] = 255; blue[i + 3] = 255; }
    writePng((root / "bedrock/blocks/stone.png").string(), 16, 16, blue);
    std::vector<uint8_t> strip(16 * 64 * 4, 255);
    writePng((root / "bedrock/blocks/water_still.png").string(), 16, 64, strip);
    a = loadTexturePack((root / "bedrock").string(), fallback, fbSize);
    CHECK(a.size == 16 * kAtlasCols && a.tilesFromPack == 2);
    CHECK(atlasPixel(a, T_STONE, 0, 0)[2] == 255 && atlasPixel(a, T_STONE, 0, 0)[0] == 0);
    const uint8_t* w = atlasPixel(a, T_WATER, 0, 0);
    CHECK(w[2] > w[0]);  // white water tinted blue

    // 3) Missing directory: plain fallback.
    a = loadTexturePack((root / "nothing").string(), fallback, fbSize);
    CHECK(a.tilesFromPack == 0 && a.size == 16 * kAtlasCols && std::memcmp(a.rgba.data(), fallback.data(), fallback.size()) == 0);
    fs::remove_all(root);
    return 0;
}


static int testFontAndItems() {
    Font font;
    int a = font.glyphFor('a'), aa = font.glyphFor(0xE1), zz = font.glyphFor(0x17D);
    CHECK(a == 'a' && aa != a && aa != '?' && zz != '?');
    CHECK(font.glyphFor(0x4E00) == '?');
    CHECK(font.glyphFor(0xB7) != '?' && font.glyphFor(0xB0) != '?' && font.glyphFor(0x201E) != '?');
    std::string word = "Přežití";
    CHECK(font.measure(word) > font.measure("P") * 4);
    size_t i = 0;
    CHECK(Font::nextCodepoint(std::string("ř"), i) == 0x159 && i == 2);

    // Accents land above the letter: some pixel in the headroom rows of 'á' and 'Ř'.
    auto rowsWithInk = [&](int glyph, int fromRow, int toRow) {
        float u0, v0, u1, v1;
        font.uv(glyph, u0, v0, u1, v1);
        int ox = (int)(u0 * Font::kTexW), oy = (int)(v0 * Font::kTexH), ink = 0;
        for (int r = fromRow; r <= toRow; ++r)
            for (int c = 0; c < Font::kCellW; ++c) ink += font.rgba()[((oy + r) * Font::kTexW + ox + c) * 4 + 3] > 0;
        return ink;
    };
    CHECK(rowsWithInk(aa, 2, 3) > 0 && rowsWithInk(a, 2, 3) == 0);
    CHECK(rowsWithInk(font.glyphFor(0x158), 0, 1) > 0);

    Inventory inv;
    CHECK(inv.add(DIRT, 70) == 0);
    CHECK(inv.slots[0].id == DIRT && inv.slots[0].count == 64 && inv.slots[1].count == 6);
    inv.moveOrSwap(1, 0);  // full stack: nothing moves
    CHECK(inv.slots[0].count == 64 && inv.slots[1].count == 6);
    inv.slots[0].count = 60;
    inv.moveOrSwap(1, 0);  // merge as much as fits
    CHECK(inv.slots[0].count == 64 && inv.slots[1].count == 2);
    inv.slots[5] = {STONE, 1};
    inv.moveOrSwap(5, 1);  // different items swap
    CHECK(inv.slots[1].id == STONE && inv.slots[5].id == DIRT);
    inv.removeOne(1);
    CHECK(inv.slots[1].empty());
    CHECK(dropFor(GRASS, 0.5f) == DIRT && dropFor(STONE, 0.5f, WOOD_PICKAXE) == COBBLESTONE && dropFor(LEAVES, 0.01f) == APPLE);
    CHECK(breakTime(BEDROCK) < 0 && breakTime(WORLD_END) < 0 && breakTime(DIRT) < breakTime(STONE));
    for (int k = 0; k < kCreativeItemCount; ++k) CHECK(itemName(kCreativeItems[k])[0] != '\0');
    return 0;
}

static int testSaves() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_save_test";
    fs::remove_all(root);

    CHECK(seedFromText("12345") == 12345u);
    CHECK(seedFromText("ahoj") == seedFromText("ahoj") && seedFromText("ahoj") != seedFromText("ahoj!"));
    CHECK(seedFromText("  ") != 0);

    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.name = "Můj = svět";
    info.seed = 99;
    info.mode = GameMode::Survival;
    info.lastPlayed = 1000;
    PlayerState p;
    p.valid = true;
    p.pos = Vec3(1.5f, 70, -3.25f);
    p.health = 13;
    p.dimension = 1;
    p.inventory.slots[3] = {POWERSTONE_LIGHT, 17};
    CHECK(saveLevel(info, p));

    WorldInfo second = info;
    second.folder = createWorldFolder(root.string());
    second.name = "Novější";
    second.lastPlayed = 2000;
    CHECK(saveLevel(second, PlayerState{}));

    auto list = listWorlds(root.string());
    CHECK(list.size() == 2 && list[0].name == "Novější" && list[1].name == "Můj = svět");
    WorldInfo li;
    PlayerState lp;
    CHECK(loadLevel(info.folder, li, lp));
    CHECK(li.mode == GameMode::Survival && li.seed == 99 && lp.valid && lp.health == 13 && lp.dimension == 1);
    CHECK(std::fabs(lp.pos.z + 3.25f) < 1e-4f && lp.inventory.slots[3].id == POWERSTONE_LIGHT && lp.inventory.slots[3].count == 17);
    CHECK(loadLevel(second.folder, li, lp) && !lp.valid);

    // Modified chunks survive a reload; untouched ones are regenerated, not saved.
    {
        World w(5, Dimension::Overworld);
        w.setStorageDir((root / "chunks").string());
        w.ensureChunksAround(0, 0, 1, 9);
        CHECK(w.setBlock(3, 100, 4, POWERSTONE_LIGHT));
        w.saveModifiedChunks();
    }
    int files = 0;
    for (auto& e : fs::directory_iterator(root / "chunks")) files += e.path().extension() == ".bin";
    CHECK(files == 1);
    World w2(5, Dimension::Overworld);
    w2.setStorageDir((root / "chunks").string());
    w2.ensureChunksAround(0, 0, 1, 9);
    CHECK(w2.getBlock(3, 100, 4) == POWERSTONE_LIGHT);

    Settings st;
    st.renderDistance = 9;
    st.fov = 90;
    saveSettings((root / "options.txt").string(), st);
    Settings st2 = loadSettings((root / "options.txt").string());
    CHECK(st2.renderDistance == 9 && st2.fov == 90);
    CHECK(loadSettings((root / "missing.txt").string()).renderDistance == 6);

    deleteWorld(info.folder);
    CHECK(listWorlds(root.string()).size() == 1);
    fs::remove_all(root);
    return 0;
}

static int testSurvivalSession() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_session_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;

    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.name = "test";
    info.seed = 42;
    info.mode = GameMode::Survival;
    Session s(info, PlayerState{});
    CHECK(s.survival() && s.stats().health == 20);
    for (int k = 0; k < kInventorySize; ++k) CHECK(s.inventory().slots[k].empty());

    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s.update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    run(1);
    CHECK(s.player().onGround);

    // Fall damage: drop from 10 blocks above the ground.
    Vec3 ground = s.player().pos;
    s.player().pos = ground + Vec3(0, 10, 0);
    run(3);
    float hp = s.stats().health;
    std::printf("session: health after a 10 block fall: %.0f\n", hp);
    CHECK(hp <= 14 && hp >= 11);

    // Mining by holding a finger on the block below: needs time in survival.
    s.player().pitch = -1.5f;
    s.player().yaw = 0;
    ItemId below = s.currentWorld().getBlock((int)std::floor(s.player().pos.x), (int)std::floor(s.player().pos.y - 1),
                                              (int)std::floor(s.player().pos.z));
    s.touchDown(7, 400, 135, t);
    run(0.4f);
    CHECK(s.inventory().slots[0].empty());  // not broken yet
    run(3);
    s.touchUp(7, 400, 135, t);
    std::printf("session: mined block %d -> got item %d x%d\n", below, s.inventory().slots[0].id, s.inventory().slots[0].count);
    CHECK(!s.inventory().slots[0].empty() && s.inventory().slots[0].id == dropFor(below, 0.5f));

    // Placing it back with a short tap uses up the item.
    run(1);
    int before = s.inventory().slots[0].count;
    s.touchDown(8, 400, 135, t);
    run(0.1f);
    s.touchUp(8, 400, 135, t);
    CHECK(s.inventory().slots[0].count == before - 1 || (before == 1 && s.inventory().slots[0].empty()));

    // Lava kills, respawn resets stats and clears the inventory.
    s.inventory().slots[4] = {STONE, 3};
    for (int i = 0; i < 200 && !s.dead(); ++i) {
        Vec3 p = s.player().pos;
        s.currentWorld().setBlock((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z), LAVA);
        run(0.1f);
    }
    CHECK(s.dead());
    s.respawn();
    CHECK(!s.dead() && s.stats().health == 20 && s.inventory().slots[4].empty());

    s.save();
    WorldInfo li;
    PlayerState lp;
    CHECK(loadLevel(info.folder, li, lp) && lp.valid && lp.health == 20);
    fs::remove_all(root);
    return 0;
}

static int testMenuFlow() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_menu_test";
    fs::remove_all(root);
    fs::create_directories(root);
    Game game(root.string());
    game.skipSplash();
    int prompts = 0;
    game.onTextPrompt = [&](int, const std::string&, const std::string&) { ++prompts; };
    CHECK(game.onSurfaceCreated());
    game.onResize(2400, 1080);  // GUI scale 4 -> 600 x 270 GUI pixels
    double t = 1;
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    frames(3);
    tap(222, 121);   // "Hrát"
    tap(164, 245);   // Nový svět
    tap(100, 56);    // name field -> text prompt
    CHECK(prompts == 1);
    game.onTextResult(1, "Testovací svět");
    frames(1);
    tap(216, 245);   // "Vytvořit a hrát" (survival is the default)
    frames(30);
    auto worlds = listWorlds((root / "worlds").string());
    CHECK(worlds.size() == 1 && worlds[0].name == "Testovací svět" && worlds[0].mode == GameMode::Survival);
    CHECK(fs::exists(fs::path(worlds[0].folder) / "generator.json"));
    CHECK(fs::exists(root / "packs" / "default" / "biomes" / "plains.json"));

    CHECK(game.onBack());  // playing -> pause
    frames(2);
    tap(300, 184);   // "Uložit a odejít"
    CHECK(!game.onBack());            // on the title screen back closes the app
    fs::remove_all(root);
    return 0;
}

static int testCrafting() {
    auto grid = [](std::initializer_list<uint8_t> ids) {
        std::array<ItemStack, 9> g{};
        int i = 0;
        for (uint8_t id : ids) { if (id != AIR) g[i] = {id, 1}; ++i; }
        return g;
    };
    // 2x2 inventory grid
    auto g = grid({LOG, AIR, AIR, AIR});
    CHECK(matchRecipe(g.data(), 2).id == PLANKS && matchRecipe(g.data(), 2).count == 4);
    g = grid({AIR, AIR, AIR, LOG});  // anywhere in the grid
    CHECK(matchRecipe(g.data(), 2).id == PLANKS);
    g = grid({PLANKS, PLANKS, PLANKS, PLANKS});
    CHECK(matchRecipe(g.data(), 2).id == CRAFTING_TABLE);
    g = grid({AIR, PLANKS, AIR, PLANKS});
    CHECK(matchRecipe(g.data(), 2).id == STICK && matchRecipe(g.data(), 2).count == 4);
    g = grid({PLANKS, PLANKS, AIR, PLANKS});
    CHECK(matchRecipe(g.data(), 2).empty());
    // 3x3 crafting table
    g = grid({COBBLESTONE, COBBLESTONE, COBBLESTONE, AIR, STICK, AIR, AIR, STICK, AIR});
    CHECK(matchRecipe(g.data(), 3).id == STONE_PICKAXE);
    g = grid({PLANKS, PLANKS, AIR, PLANKS, STICK, AIR, AIR, STICK, AIR});
    CHECK(matchRecipe(g.data(), 3).id == WOOD_AXE);
    g = grid({AIR, IRON_INGOT, IRON_INGOT, AIR, STICK, IRON_INGOT, AIR, STICK, AIR});  // mirrored axe
    CHECK(matchRecipe(g.data(), 3).id == IRON_AXE);
    g = grid({AIR, AIR, PLANKS, AIR, AIR, STICK, AIR, AIR, STICK});
    CHECK(matchRecipe(g.data(), 3).id == WOOD_SHOVEL);
    g = grid({COBBLESTONE, COBBLESTONE, COBBLESTONE, COBBLESTONE, AIR, COBBLESTONE, COBBLESTONE, COBBLESTONE, COBBLESTONE});
    CHECK(matchRecipe(g.data(), 3).id == FURNACE);
    g = grid({FLINT, AIR, AIR, AIR, AIR, AIR, AIR, AIR, IRON_INGOT});
    CHECK(matchRecipe(g.data(), 3).id == FLINT_AND_STEEL);
    g = grid({COBBLESTONE, COBBLESTONE, COBBLESTONE, AIR, STICK, AIR, AIR, AIR, AIR});  // pickaxe without second stick
    CHECK(matchRecipe(g.data(), 3).empty());

    // Furnace: one coal smelts eight items, 10 s each.
    Furnace f;
    f.input = {IRON_ORE, 3};
    f.fuel = {COAL, 1};
    CHECK(f.update(0.1f) && f.burning() && f.fuel.empty());
    for (int i = 0; i < 400; ++i) f.update(0.1f);
    CHECK(f.output.id == IRON_INGOT && f.output.count == 3 && f.input.empty());
    for (int i = 0; i < 500; ++i) f.update(0.1f);
    CHECK(!f.burning());
    Furnace nofuel;
    nofuel.input = {SAND, 1};
    for (int i = 0; i < 200; ++i) nofuel.update(0.1f);
    CHECK(nofuel.output.empty());

    // Tools: harvesting rules and speed.
    CHECK(!canHarvest(STONE, AIR) && canHarvest(STONE, WOOD_PICKAXE));
    CHECK(!canHarvest(IRON_ORE, WOOD_PICKAXE) && canHarvest(IRON_ORE, STONE_PICKAXE));
    CHECK(dropFor(IRON_ORE, 0.5f, WOOD_PICKAXE) == AIR && dropFor(COAL_ORE, 0.5f, WOOD_PICKAXE) == COAL);
    CHECK(breakTime(STONE, IRON_PICKAXE) < breakTime(STONE, WOOD_PICKAXE) && breakTime(STONE, WOOD_PICKAXE) < breakTime(STONE));
    CHECK(breakTime(LOG, STONE_AXE) < breakTime(LOG) && breakTime(DIRT, WOOD_SHOVEL) < breakTime(DIRT));
    CHECK(breakTime(STONE, WOOD_AXE) == breakTime(STONE));  // wrong tool does not help
    CHECK(maxStack(WOOD_PICKAXE) == 1 && maxStack(STICK) == 64);
    Inventory inv;
    inv.add(WOOD_PICKAXE, 2);
    CHECK(inv.slots[0].count == 1 && inv.slots[1].id == WOOD_PICKAXE);
    inv.moveOrSwap(0, 1);  // tools never merge
    CHECK(inv.slots[0].count == 1 && inv.slots[1].count == 1);

    // Ores show up in generated stone.
    World w(77, Dimension::Overworld);
    w.ensureChunksAround(0, 0, 2, 100);
    int coal = 0, iron = 0;
    for (auto& [k, c] : w.chunks())
        for (ItemId b : c->blocks) { coal += b == COAL_ORE; iron += b == IRON_ORE; }
    std::printf("crafting: ores in 13 chunks: coal %d, iron %d\n", coal, iron);
    CHECK(coal > 50 && iron > 20 && coal > iron);

    // Furnaces are saved with the world.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_furnace_test";
    fs::remove_all(root);
    fs::create_directories(root);
    std::map<FurnaceKey, Furnace> fm;
    Furnace sf;
    sf.input = {SAND, 5};
    sf.output = {GLASS, 2};
    sf.burnLeft = 12;
    sf.burnTotal = 15;
    fm[{1, -5, 40, 9}] = sf;
    fm[{0, 1, 2, 3}] = Furnace{};  // empty and cold: not worth saving
    CHECK(saveFurnaces(root.string(), fm));
    auto loaded = loadFurnaces(root.string());
    CHECK(loaded.size() == 1);
    const Furnace& lf = loaded.at({1, -5, 40, 9});
    CHECK(lf.input.id == SAND && lf.input.count == 5 && lf.output.count == 2 && lf.burnLeft == 12);
    fs::remove_all(root);
    return 0;
}

static int testWorkbenchInSession() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_bench_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 3;
    info.mode = GameMode::Survival;
    Session s(info, PlayerState{});
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s.update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    run(1);
    // Put a furnace below the player's feet and tap it: it opens instead of placing.
    Vec3 p = s.player().pos;
    int bx = (int)std::floor(p.x), by = (int)std::floor(p.y) - 1, bz = (int)std::floor(p.z);
    s.currentWorld().setBlock(bx, by, bz, FURNACE);
    s.player().pitch = -1.5f;
    s.inventory().slots[0] = {COBBLESTONE, 5};
    s.touchDown(1, 300, 135, t);
    run(0.05f);
    s.touchUp(1, 300, 135, t);
    CHECK(s.wantsFurnace && s.inventory().slots[0].count == 5);
    CHECK(s.openFurnace.x == bx && s.openFurnace.y == by && s.openFurnace.z == bz);
    Furnace* f = s.furnace(s.openFurnace);
    CHECK(f != nullptr);
    f->input = {COBBLESTONE, 1};
    f->fuel = {PLANKS, 1};
    run(0.2f);
    CHECK(s.currentWorld().getBlock(bx, by, bz) == LIT_FURNACE);  // lights up
    run(11);
    CHECK(s.furnace(s.openFurnace)->output.id == STONE);
    run(6);
    CHECK(s.currentWorld().getBlock(bx, by, bz) == FURNACE);  // fuel burnt out

    // Mining with a wooden pickaxe wears it down and gives cobblestone from stone.
    s.wantsFurnace = false;
    s.currentWorld().setBlock(bx, by, bz, STONE);
    s.inventory().slots[0] = {WOOD_PICKAXE, 1};
    s.selectedSlot() = 0;
    s.touchDown(2, 300, 135, t);
    run(1.5f);
    s.touchUp(2, 300, 135, t);
    bool gotCobble = false;
    for (auto& st : s.inventory().slots) gotCobble |= st.id == COBBLESTONE;
    CHECK(gotCobble && s.inventory().slots[0].id == WOOD_PICKAXE && s.inventory().slots[0].damage >= 1);
    fs::remove_all(root);
    return 0;
}

static int testJsonAndPacks() {
    // Parser: values, comments, trailing commas, UTF-8 and escapes.
    Json j;
    std::string err;
    CHECK(Json::parse("{\"a\": [1, 2.5, true, null], \"b\": {\"c\": \"Přežití \\u00e1\"}, // poznámka\n \"d\": -3,}", j, err));
    CHECK(j["a"].size() == 4 && j["a"][1].num() == 2.5 && j["a"][2].boolean() && j["a"][3].isNull());
    CHECK(j["b"]["c"].str() == "Přežití á" && j["d"].num() == -3 && j["missing"]["x"].num(7) == 7);
    CHECK(j.keys().size() == 3 && j.keys()[0] == "a");
    Json back;
    CHECK(Json::parse(j.dump(), back, err) && back["b"]["c"].str() == "Přežití á");
    CHECK(!Json::parse("{\"a\": 1\n \"b\": 2}", j, err));
    std::printf("json: error message: %s\n", err.c_str());
    CHECK(err.find("line 2, column 2: expected comma or }") != std::string::npos);  // English is the default
    setLanguage(Language::Czech);
    CHECK(!Json::parse("{\"a\": 1\n \"b\": 2}", j, err) && err.find("řádek 2, sloupec 2") != std::string::npos);
    setLanguage(Language::Russian);
    CHECK(!Json::parse("{\"a\": 1\n \"b\": 2}", j, err) && err.find("строка 2") != std::string::npos);
    setLanguage(Language::English);
    CHECK(!Json::parse("[1, 2", j, err) && !Json::parse("{\"x\": tru}", j, err) && !Json::parse("\"open", j, err));

    // Default pack files are valid and give the built-in generator.
    for (const auto& [path, text] : defaultPackFiles()) {
        CHECK(Json::parse(text, j, err));
        if (!err.empty()) std::printf("%s: %s\n", path.c_str(), err.c_str());
    }
    const GenConfig& def = GenConfig::defaults();
    CHECK(def.biomes.size() == 7 && def.overworld.seaLevel == 62 && def.overworld.ores.size() == 5);
    CHECK(def.nether.fill == NETHERRACK && def.nether.lavaLevel == 31);
    std::vector<std::string> warn;
    GenConfig round = GenConfig::fromJson(def.toJson(), &warn);
    CHECK(warn.empty() && round.biomes.size() == 7 && round.biomes[2].surface == def.biomes[2].surface);
    CHECK(RecipeBook::defaults().shaped.size() == 12 + 35 + 6 + 2 + 16 + 6 + 24 + 16 + 3 + 10 + 12 && RecipeBook::defaults().shapeless.size() == 2 + 4 + 1 + 1 + 6 + 1 + 1 + 3 + 1);  // 1.5.1: no planks from Eye Logs

    // Biomes really show up in the world: deserts are sandy, forests have trees.
    World w(1234, Dimension::Overworld);
    std::map<std::string, int> seen;
    for (int x = -2000; x <= 2000; x += 40)
        for (int z = -2000; z <= 2000; z += 40) seen[w.biomeAt(x, z)->id]++;
    std::printf("packs: biome samples:");
    for (auto& [id, n] : seen) std::printf(" %s=%d", id.c_str(), n);
    std::printf("\n");
    CHECK(seen.size() >= 4);

    // Packs on disk: default pack, an overriding mod pack, ordering.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_packs_test";
    fs::remove_all(root);
    PackManager pm(root.string());
    pm.ensureDefaultPack();
    auto list = pm.list();
    CHECK(list.size() == 1 && list[0].builtin && list[0].enabled && list[0].name == "VoxelCraft Default");
    CHECK(pm.files("default").size() == 19 && pm.files("default")[0] == "manifest.json");
    warn.clear();
    GenConfig cfg = pm.buildConfig(&warn);
    CHECK(warn.empty() && cfg.biomes.size() == 7);

    // A mod pack: glowstone Nether with a higher lava sea, a new biome, one removed biome and a recipe.
    std::string id = pm.createPack("Můj mod");
    CHECK(id == "m_j_mod" || !id.empty());
    CHECK(pm.writeFile(id, "dimensions/nether.json", "{\"lava_level\": 50, \"blocks\": {\"fill\": \"glowstone\"}}"));
    CHECK(pm.writeFile(id, "biomes/ice.json",
                       "{\"id\": \"ice\", \"name\": \"Led\", \"climate\": {\"temperature\": 0, \"humidity\": 0},"
                       " \"blocks\": {\"surface\": \"glass\", \"filler\": \"obsidian\"}}"));
    CHECK(pm.writeFile(id, "biomes/desert.json", "{\"id\": \"desert\", \"enabled\": false}"));
    CHECK(pm.writeFile(id, "recipes/extra.json",
                       "{\"recipes\": [{\"type\": \"shapeless\", \"ingredients\": [\"dirt\", \"dirt\"], \"result\": \"grass\"}]}"));
    pm.setEnabled(id, true);
    warn.clear();
    cfg = pm.buildConfig(&warn);
    CHECK(warn.empty());
    CHECK(cfg.nether.lavaLevel == 50 && cfg.nether.fill == POWERSTONE_LIGHT && cfg.nether.floor == NETHER_GRASS);
    bool hasIce = false, hasDesert = false;
    for (const BiomeDef& b : cfg.biomes) { hasIce |= b.id == "ice" && b.surface == GLASS; hasDesert |= b.id == "desert"; }
    CHECK(hasIce && !hasDesert);
    RecipeBook book = pm.buildRecipes(&warn);
    setRecipes(book);
    std::array<ItemStack, 9> g{};
    g[0] = {DIRT, 1};
    g[1] = {DIRT, 1};
    CHECK(matchRecipe(g.data(), 2).id == GRASS);
    g = {};
    g[0] = {LOG, 1};
    CHECK(matchRecipe(g.data(), 2).id == PLANKS);  // the copied default recipes still work

    // The mod pack's Nether: the lava sea is higher and the walls glow.
    World nether(5, Dimension::Nether, cfg);
    nether.ensureChunksAround(0, 0, 1, 9);
    int glow = 0, lavaHigh = 0;
    for (auto& [k, c] : nether.chunks())
        for (int x = 0; x < 16; ++x)
            for (int z = 0; z < 16; ++z) {
                glow += c->get(x, 60, z) == POWERSTONE_LIGHT;
                lavaHigh += c->get(x, 45, z) == LAVA;
            }
    CHECK(glow > 100 && lavaHigh > 0);

    // A broken file is reported, not fatal.
    CHECK(pm.writeFile(id, "biomes/broken.json", "{\"id\": \"x\", \"climate\": {"));
    warn.clear();
    cfg = pm.buildConfig(&warn);
    CHECK(warn.size() == 1 && warn[0].find("broken.json") != std::string::npos);
    CHECK(pm.writeFile(id, "biomes/bad_block.json", "{\"id\": \"y\", \"blocks\": {\"surface\": \"diamond_block\"}}"));
    warn.clear();
    pm.buildConfig(&warn);
    CHECK(warn.size() == 2);

    // Ordering and removal.
    CHECK(pm.list()[1].id == id);
    pm.move(id, -1);
    CHECK(pm.list()[0].id == id);
    pm.deletePack(id);
    CHECK(pm.list().size() == 1);
    pm.deletePack("default");  // the built-in pack cannot be deleted
    CHECK(pm.list().size() == 1);
    setRecipes(RecipeBook::defaults());

    // The shipped example mod pack loads without warnings.
    fs::copy("examples/ohnivy_nether", root / "packs" / "ohnivy_nether", fs::copy_options::recursive);
    pm.setEnabled("ohnivy_nether", true);
    warn.clear();
    cfg = pm.buildConfig(&warn);
    pm.buildRecipes(&warn);
    for (auto& m : warn) std::printf("example warning: %s\n", m.c_str());
    CHECK(warn.empty() && cfg.nether.fill == NETHER_BRICK && cfg.nether.lavaLevel == 48 && cfg.biomes.size() == 8);
    pm.deletePack("ohnivy_nether");

    // Superflat
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World fw(9, Dimension::Overworld, flat);
    fw.ensureChunksAround(0, 0, 1, 9);
    CHECK(fw.getBlock(5, 3, 5) == GRASS && fw.getBlock(5, 2, 5) == DIRT && fw.getBlock(5, 0, 5) == WORLD_END &&
          fw.getBlock(5, 4, 5) == AIR);
    fs::remove_all(root);
    return 0;
}

static int testSkinsStoreAudio() {
    namespace fs = std::filesystem;
    // Built-in skins are complete 64x64 images with an opaque face.
    auto skins = builtinSkins();
    CHECK(skins.size() == 9 && skins[7].vipOnly && !skins[0].vipOnly);
    for (const Skin& sk : skins) {
        CHECK(sk.valid() && !sk.name.empty());
        CHECK(sk.rgba[(12 * 64 + 12) * 4 + 3] == 255);  // head front centre
    }
    // Legacy 64x32 skins get their left arm and leg from the right ones.
    std::vector<uint8_t> legacy(64 * 32 * 4, 0);
    for (int y = 16; y < 32; ++y)
        for (int x = 0; x < 16; ++x) legacy[(y * 64 + x) * 4 + 3] = 255, legacy[(y * 64 + x) * 4] = 200;
    Skin conv;
    CHECK(Skin::fromRgba(legacy.data(), 64, 32, conv) && conv.rgba[(52 * 64 + 20) * 4] == 200);
    std::vector<uint8_t> wrong(32 * 32 * 4, 0);
    CHECK(!Skin::fromRgba(wrong.data(), 32, 32, conv));
    fs::path root = fs::temp_directory_path() / "vc_skin_test";
    fs::remove_all(root);
    fs::create_directories(root);
    writePng((root / "mine.png").string(), 64, 64, skins[4].rgba);
    Skin loaded;
    std::string err;
    CHECK(Skin::loadPng((root / "mine.png").string(), loaded, err) && loaded.rgba == skins[4].rgba);
    writePng((root / "bad.png").string(), 32, 32, wrong);
    CHECK(!Skin::loadPng((root / "bad.png").string(), loaded, err) && err.find("64") != std::string::npos);

    // Player model: 6 parts x 2 layers x 6 faces x 2 triangles; standing upright at the feet position.
    PlayerPose pose;
    pose.feet = Vec3(10, 64, -3);
    auto verts = buildPlayerModel(pose);
    CHECK(verts.size() == 6 * 2 * 36);
    float minY = 1e9f, maxY = -1e9f;
    for (const Vertex& v : verts) { minY = std::min(minY, v.y); maxY = std::max(maxY, v.y); CHECK(v.u >= 0 && v.u <= 1 && v.v >= 0 && v.v <= 1); }
    CHECK(std::fabs(minY - 64) < 0.05f && maxY > 65.8f && maxY < 66.0f);

    // Profile: daily bonus once per day, a coin every 10 blocks, saved and loaded.
    Profile prof;
    int start = prof.coins;
    CHECK(prof.claimDailyBonus(20261024) == 50 && prof.claimDailyBonus(20261024) == 0 && prof.claimDailyBonus(20261025) == 50);
    int earned = 0;
    for (int i = 0; i < 25; ++i) earned += prof.countBlock();
    CHECK(earned == 2 && prof.coins == start + 102);
    prof.name = "Michal";
    prof.owned.insert("skin:robot");
    CHECK(prof.save((root / "profile.json").string()));
    Profile back = Profile::load((root / "profile.json").string());
    CHECK(back.name == "Michal" && back.coins == prof.coins && back.owns("skin:robot") && back.owns("skin:explorer"));

    // Store: every item installs; coins are checked and taken once.
    PackManager pm(root.string());
    pm.ensureDefaultPack();
    Profile buyer;
    buyer.coins = 10000;
    for (const StoreItem& it : storeCatalog()) {
        CHECK(buyStoreItem(it, buyer, pm, err) == BuyResult::Bought);
        CHECK(buyStoreItem(it, buyer, pm, err) == BuyResult::AlreadyOwned);
    }
    int spent = 0;
    for (const StoreItem& it : storeCatalog()) spent += it.price;
    CHECK(buyer.coins == 10000 - spent);
    std::vector<std::string> warn;
    pm.buildConfig(&warn);
    pm.buildRecipes(&warn);
    for (auto& m : warn) std::printf("store warning: %s\n", m.c_str());
    CHECK(warn.empty());
    CHECK(pm.list().size() == 1 + 7);  // default + 4 mod packs + 3 texture packs
    CHECK(fs::exists(root / "packs" / "store_night" / "textures" / "blocks" / "stone.png"));
    // The bought texture pack really changes the textures (darker stone).
    int fbSize = 0;
    auto fb = buildFallbackAtlas(fbSize);
    Atlas night = loadTexturePack((root / "packs" / "store_night" / "textures").string(), fb, fbSize);
    CHECK(night.tilesFromPack == T_COUNT && night.rgba[(8 * 256 + 8) * 4] < fb[(8 * 256 + 8) * 4]);
    Profile poor;
    poor.coins = 10;
    CHECK(buyStoreItem(storeCatalog()[0], poor, pm, err) == BuyResult::NotEnoughCoins && poor.coins == 10);

    // Audio: valid WAV files.
    for (int i = 0; i < (int)Sound::Count; ++i) {
        auto wav = synthesizeSound((Sound)i);
        CHECK(wav.size() > 1000 && std::memcmp(wav.data(), "RIFF", 4) == 0 && std::memcmp(wav.data() + 8, "WAVE", 4) == 0);
    }
    auto music = synthesizeMusic();
    CHECK(music.size() > 22050 * 2 * 40);
    CHECK(writeSoundFiles((root / "sounds").string()) && fs::exists(root / "sounds" / "music.wav") &&
          fs::exists(root / "sounds" / "coin.wav"));
    fs::remove_all(root);
    return 0;
}

static int testThirdPersonCamera() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_camera_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 77;
    info.mode = GameMode::Creative;
    Session s(info, PlayerState{});
    std::vector<Sound> sounds;
    s.onSound = [&](Sound snd) { sounds.push_back(snd); };
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s.update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    run(1);
    // Walking makes step sounds.
    s.player().flying = false;
    s.touchDown(3, 52, 190, t);  // D-pad: up
    run(2);
    s.touchUp(3, 52, 190, t);
    CHECK(std::count(sounds.begin(), sounds.end(), Sound::Step) >= 1);
    // Breaking a block makes the dig sound.
    s.player().pitch = -1.5f;
    s.touchDown(4, 300, 135, t);
    run(0.6f);
    s.touchUp(4, 300, 135, t);
    CHECK(std::count(sounds.begin(), sounds.end(), Sound::Dig) >= 1);
    fs::remove_all(root);
    return 0;
}

// A pool cut into flat ground: water blocks y = yBottom..2, ground (grass) top at y = 4.
static World makePool(int depth) {
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World w(3, Dimension::Overworld, flat);
    w.ensureChunksAround(0, 0, 3, 64);
    for (int x = 2; x <= 9; ++x)
        for (int z = 2; z <= 9; ++z)
            for (int y = 3 - depth; y <= 3; ++y) w.setBlock(x, y, z, y == 3 ? AIR : WATER);
    // depth 3 also replaces the bedrock floor at y = 0 (a bottomless pool is fine for the shore test).
    return w;
}

static int testWaterEscape() {
    for (int depth : {1, 2}) {
        World w = makePool(depth);
        Player p;
        p.pos = Vec3(5.5f, (float)(3 - depth), 5.5f);  // standing on the pool floor
        p.yaw = -kPi / 2;                                // facing -X, towards the shore at x = 1
        bool out = false;
        for (int i = 0; i < 60 * 8 && !out; ++i) {
            p.update(w, 1 / 60.0f, 0, 1, true);  // walk forward, hold jump
            out = p.pos.x < 2.0f && p.pos.y >= 3.99f && p.onGround;
        }
        std::printf("water: depth %d -> ended at %.2f %.2f %.2f (%s)\n", depth, p.pos.x, p.pos.y, p.pos.z,
                    out ? "on the shore" : "STUCK");
        CHECK(out);
    }
    {
        // Deep water: a 6 deep basin built in the air, rim one block above the surface.
        World w = makePool(1);
        for (int x = 20; x <= 28; ++x)
            for (int z = 20; z <= 28; ++z)
                for (int y = 10; y <= 17; ++y) {
                    bool wall = x == 20 || x == 28 || z == 20 || z == 28;
                    w.setBlock(x, y, z, y == 10 || wall ? STONE : y <= 16 ? WATER : AIR);
                }
        Player p;
        p.pos = Vec3(24.5f, 11, 24.5f);
        p.yaw = -kPi / 2;
        bool out = false;
        for (int i = 0; i < 60 * 12 && !out; ++i) {
            p.update(w, 1 / 60.0f, 0, 1, true);
            out = p.pos.y >= 17.99f && p.pos.x < 20.7f;  // above the rim, out of the basin
        }
        std::printf("water: deep basin -> ended at %.2f %.2f %.2f (%s)\n", p.pos.x, p.pos.y, p.pos.z, out ? "out" : "STUCK");
        CHECK(out);
    }
    // Swimming up: from the bottom of a 2 deep pool, holding jump brings the head out of the water.
    World w = makePool(2);
    Player p;
    p.pos = Vec3(5.5f, 1.0f, 5.5f);
    for (int i = 0; i < 60 * 3; ++i) p.update(w, 1 / 60.0f, 0, 0, true);
    CHECK(!p.eyeInWater(w));
    // Without input the player slowly sinks back to the floor and can walk on it.
    for (int i = 0; i < 60 * 4; ++i) p.update(w, 1 / 60.0f, 0, 0, false);
    CHECK(p.onGround && p.pos.y < 1.1f);
    // Normal ground physics are unchanged: a jump on land is about 1.25 blocks high.
    World g = makePool(1);
    Player q;
    q.pos = Vec3(12.5f, 4, 12.5f);
    for (int i = 0; i < 30; ++i) q.update(g, 1 / 60.0f, 0, 0, false);
    float top = 0;
    for (int i = 0; i < 60; ++i) { q.update(g, 1 / 60.0f, 0, 0, i == 0); top = std::max(top, q.pos.y); }
    CHECK(top > 5.1f && top < 5.4f);
    return 0;
}

static int testLeavesMesh() {
    // A 3x3x3 block of leaves surrounded by air: every one of the 27 blocks shows all 6 faces,
    // so the inside of a crown is never seen as empty through the transparent leaf pixels.
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World w(1, Dimension::Overworld, flat);
    w.ensureChunksAround(0, 0, 2, 25);
    for (int x = 5; x < 8; ++x)
        for (int y = 20; y < 23; ++y)
            for (int z = 5; z < 8; ++z) w.setBlock(x, y, z, LEAVES);
    // The same with glass for comparison: only the outer shell is drawn.
    for (int x = 9; x < 12; ++x)
        for (int y = 20; y < 23; ++y)
            for (int z = 5; z < 8; ++z) w.setBlock(x, y, z, GLASS);
    ChunkMesh mesh;
    buildChunkMesh(w, *w.getChunk(0, 0), mesh);
    int leafFaces = 0, glassFaces = 0;
    for (size_t i = 0; i < mesh.opaque.size(); i += 6) {
        const Vertex& v = mesh.opaque[i];
        if (v.y < 19.9f) continue;
        if (v.x < 8.5f) ++leafFaces; else ++glassFaces;
    }
    std::printf("leaves: %d leaf faces, %d glass faces in a 3x3x3 cube\n", leafFaces, glassFaces);
    CHECK(leafFaces == 27 * 6 && glassFaces == 9 * 6);
    // Trees: every leaf block in a generated forest is meshed with its outward faces.
    World f(4242, Dimension::Overworld);
    f.ensureChunksAround(0, 0, 4, 100);
    int leaves = 0;
    for (auto& [k, c] : f.chunks())
        for (ItemId b : c->blocks) leaves += b == LEAVES;
    CHECK(leaves > 0);
    // Every generated tree has its whole crown: the leaves at the trunk's top are on all four
    // sides (filling later terrain columns used to wipe the +x / +z half away).
    int trees = 0, halfCrowns = 0;
    for (int x = -56; x < 72; ++x)
        for (int z = -56; z < 72; ++z) {
            int top = -1;
            for (int y = 120; y > 1; --y)
                if (f.getBlock(x, y, z) == LOG) {
                    top = y;
                    break;
                }
            if (top < 0 || f.getBlock(x, top + 1, z) != LEAVES) continue;
            ++trees;
            int sides = 0;
            for (auto [dx, dz] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
                sides += (f.getBlock(x + dx, top, z + dz) == LEAVES || f.getBlock(x + dx, top, z + dz) == LOG) &&
                         (f.getBlock(x + 2 * dx, top - 1, z + 2 * dz) == LEAVES || f.getBlock(x + 2 * dx, top - 1, z + 2 * dz) == LOG);
                // (a neighbouring tree's trunk may stand where leaves would be)
            halfCrowns += sides < 4;
        }
    std::printf("trees: %d checked, %d with a broken crown\n", trees, halfCrowns);
    CHECK(trees > 5 && halfCrowns == 0);

    // Shade: grass under a crown is darker than grass in the open, and inner corners get
    // ambient occlusion.
    World g(1, Dimension::Overworld, flat);
    g.ensureChunksAround(0, 0, 2, 25);
    for (int x = 3; x <= 7; ++x)
        for (int z = 3; z <= 7; ++z) g.setBlock(x, 8, z, LEAVES);
    g.setBlock(12, 4, 12, STONE);  // a block on open ground: occlusion around its foot
    ChunkMesh m;
    buildChunkMesh(g, *g.getChunk(0, 0), m);
    auto topLight = [&](float x, float z) {
        float best = -1;
        for (size_t i = 0; i < m.opaque.size(); ++i) {
            const Vertex& v = m.opaque[i];
            if (std::fabs(v.y - 4) < 1e-3f && std::fabs(v.x - x) < 1e-3f && std::fabs(v.z - z) < 1e-3f) best = std::max(best, v.light);
        }
        return best;
    };
    float shaded = topLight(5, 5), open = topLight(1, 14), corner = topLight(12, 12);
    std::printf("light: open grass %.2f, under leaves %.2f, next to a block %.2f\n", open, shaded, corner);
    CHECK(open > 0.95f && shaded < open * 0.9f && corner < open);
    return 0;
}

static int testPlants() {
    // Generation: grassy biomes grow tall grass and flowers, always on grass with nothing solid inside.
    World w(4242, Dimension::Overworld);
    w.ensureChunksAround(0, 0, 3, 64);
    int grass = 0, flowers = 0, wrongGround = 0;
    for (auto& [k, c] : w.chunks())
        for (int x = 0; x < 16; ++x)
            for (int z = 0; z < 16; ++z)
                for (int y = 1; y < kChunkHeight; ++y) {
                    ItemId b = c->get(x, y, z);
                    if (!isPlant(b) || isDripstoneSpike(b)) continue;
                    (b == TALL_GRASS || b == DRY_GRASS || b == FERN ? grass : flowers)++;
                    ItemId below = c->get(x, y - 1, z);
                    if (below != GRASS && below != DIRT && below != WASTELAND_GRASS) {
                        if (wrongGround < 5) std::printf("plants: %d on %d\n", (int)b, (int)below);
                        ++wrongGround;
                    }
                }
    std::printf("plants: %d tall grass, %d flowers\n", grass, flowers);
    CHECK(grass > 100 && flowers > 5 && wrongGround == 0);
    CHECK(!isSolid(TALL_GRASS) && isTargetable(POPPY) && !isOpaque(DANDELION));

    // Meshing: a plant is two crossed planes, drawn from both sides (4 quads = 24 vertices).
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World f(1, Dimension::Overworld, flat);
    f.ensureChunksAround(0, 0, 2, 25);
    ChunkMesh before, after;
    buildChunkMesh(f, *f.getChunk(0, 0), before);
    f.setBlock(5, 4, 5, POPPY);
    buildChunkMesh(f, *f.getChunk(0, 0), after);
    CHECK(after.opaque.size() - before.opaque.size() == 24);  // the grass top below stays visible

    // Older packs without "plants" still get plants in grassy biomes.
    Json b;
    std::string err;
    CHECK(Json::parse("{\"biomes\": [{\"id\": \"x\", \"blocks\": {\"surface\": \"grass\"}}]}", b, err));
    GenConfig old = GenConfig::fromJson(b);
    CHECK(old.biomes[0].grassDensity > 0 && old.biomes[0].flowerDensity > 0);

    // Raycast hits a plant even though you can walk through it.
    Player p;
    p.pos = Vec3(5.5f, 4, 3.5f);
    p.pitch = -0.6f;
    p.yaw = kPi;  // looking towards +Z
    RayHit hit = raycast(f, p.eye(), p.forward(), Player::kReach);
    CHECK(hit.hit && hit.x == 5 && hit.y == 4 && hit.z == 5);
    CHECK(!p.touchesBlock(f, STONE));
    return 0;
}

static int testDigParticles() {
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World w(1, Dimension::Overworld, flat);
    w.ensureChunksAround(0, 0, 2, 25);
    ParticleSystem ps;
    ps.spawnChips(w, 5, 3, 5, 0, 1, 0, GRASS, 4);
    CHECK(ps.count() == 4);
    ps.spawnBurst(w, 5, 3, 5, STONE);
    size_t burst = ps.count() - 4;
    std::printf("particles: burst of %zu pieces\n", burst);
    CHECK(burst > 15 && burst < 64);
    // Particles fall, land on the ground and vanish after a while.
    std::vector<Vertex> verts;
    ps.buildVertices({1, 0, 0}, {0, 1, 0}, verts);
    CHECK(verts.size() == ps.count() * 6);
    // Their texture piece lies inside the block's atlas tile.
    int stoneTile = tileFor(STONE, F_SOUTH);
    bool insideTile = false;
    for (const Vertex& v : verts) {
        int tx = (int)(v.u * kAtlasCols + 1e-4f), ty = (int)(v.v * kAtlasCols + 1e-4f);
        insideTile |= ty * kAtlasCols + tx == stoneTile;
    }
    CHECK(insideTile);
    w.setBlock(5, 3, 5, AIR);  // the game removes the block right after spawning the burst
    float minY = 1e9f;
    for (int i = 0; i < 20; ++i) ps.update(w, 1 / 60.0f);
    ps.buildVertices({1, 0, 0}, {0, 1, 0}, verts);
    for (const Vertex& v : verts) minY = std::min(minY, v.y);
    CHECK(minY > 2.8f);  // nothing fell through the dirt below (top at y = 3)
    for (int i = 0; i < 120; ++i) ps.update(w, 1 / 60.0f);
    CHECK(ps.count() == 0);
    // The cap keeps phones smooth.
    for (int i = 0; i < 40; ++i) ps.spawnBurst(w, 5, 3, 5, DIRT);
    CHECK(ps.count() <= (size_t)ParticleSystem::kMaxParticles);

    // In a session: mining spawns chips while digging and a burst when the block breaks.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_particles_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 5;
    info.mode = GameMode::Survival;
    Session s(info, PlayerState{});
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s.update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    run(1);
    s.player().pitch = -1.5f;
    s.touchDown(1, 300, 135, t);
    run(0.3f);
    size_t whileDigging = s.particles().count();
    run(1.5f);
    s.touchUp(1, 300, 135, t);
    std::printf("particles: %zu while digging\n", whileDigging);
    CHECK(whileDigging >= 4);
    fs::remove_all(root);
    return 0;
}

static int testDPad() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_dpad_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;
    settings.autoJump = false;
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 11;
    info.mode = GameMode::Creative;
    Session s(info, PlayerState{});
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s.update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    run(1);
    // Flat ground for clean measurements: a stone floor high in the air.
    Vec3 base = s.player().pos;
    int bx = (int)std::floor(base.x), bz = (int)std::floor(base.z), fy = 100;
    for (int x = bx - 12; x <= bx + 12; ++x)
        for (int z = bz - 12; z <= bz + 12; ++z) {
            s.currentWorld().setBlock(x, fy, z, STONE);
            for (int y = fy + 1; y < fy + 4; ++y) s.currentWorld().setBlock(x, y, z, AIR);
        }
    s.player().pos = Vec3(bx + 0.5f, fy + 1.0f, bz + 0.5f);
    s.player().yaw = 0;  // forward is -Z
    run(0.3f);
    // Layout at 600x270: 28 px cells, pad from (10, 176). Up, left, right, down, centre:
    const float up[2] = {52, 190}, left[2] = {24, 218}, right[2] = {80, 218}, centre[2] = {52, 218};
    Vec3 p0 = s.player().pos;
    s.touchDown(1, up[0], up[1], t);
    run(0.5f);
    Vec3 p1 = s.player().pos;
    CHECK(p1.z < p0.z - 1.5f && std::fabs(p1.x - p0.x) < 0.01f);  // walked forward
    s.touchUp(1, up[0], up[1], t);
    run(0.1f);
    Vec3 p2 = s.player().pos;
    run(0.5f);
    CHECK((s.player().pos - p2).length() < 0.01f);  // released: stops right away, nothing stuck

    // Sliding the finger from "up" to "right" changes the direction.
    s.touchDown(2, up[0], up[1], t);
    run(0.1f);
    s.touchMove(2, right[0], right[1]);
    Vec3 p3 = s.player().pos;
    run(0.4f);
    Vec3 p4 = s.player().pos;
    CHECK(p4.x > p3.x + 1 && std::fabs(p4.z - p3.z) < 0.01f);
    // Sliding onto the centre stops without lifting the finger.
    s.touchMove(2, centre[0], centre[1]);
    run(0.1f);
    Vec3 p5 = s.player().pos;
    run(0.3f);
    CHECK((s.player().pos - p5).length() < 0.01f);
    s.touchUp(2, centre[0], centre[1], t);

    // Two fingers (up + left) make a diagonal at normal speed.
    s.touchDown(3, up[0], up[1], t);
    s.touchDown(4, left[0], left[1], t);
    Vec3 p6 = s.player().pos;
    run(0.5f);
    Vec3 d = s.player().pos - p6;
    CHECK(d.x < -0.5f && d.z < -0.5f && std::hypot(d.x, d.z) < 2.3f);
    // An interrupted touch (system gesture, dialog) releases every direction.
    s.cancelTouches();
    run(0.1f);
    Vec3 p7 = s.player().pos;
    run(0.4f);
    CHECK((s.player().pos - p7).length() < 0.01f);
    fs::remove_all(root);
    return 0;
}


// A Bedrock-style .mcstructure written by hand: 3 x 2 x 2 with a palette that
// includes states, an unknown block and structure_void.
static std::vector<uint8_t> sampleMcStructure() {
    Nbt root = Nbt::makeCompound();
    root.fields["format_version"] = Nbt::makeInt(1);
    Nbt size = Nbt::makeList(Nbt::Int);
    for (int v : {3, 2, 2}) size.items.push_back(Nbt::makeInt(v));
    root.fields["size"] = size;
    Nbt palette = Nbt::makeList(Nbt::Compound);
    for (const char* name : {"minecraft:cobblestone", "minecraft:oak_planks", "minecraft:air", "minecraft:redstone_lamp",
                             "minecraft:structure_void", "minecraft:spruce_log"}) {
        Nbt e = Nbt::makeCompound();
        e.fields["name"] = Nbt::makeString(name);
        Nbt states = Nbt::makeCompound();
        if (std::string(name) == "minecraft:spruce_log") states.fields["pillar_axis"] = Nbt::makeString("y");
        e.fields["states"] = states;
        e.fields["version"] = Nbt::makeInt(18168865);
        palette.items.push_back(e);
    }
    // Order: z fastest, then y, then x. Bottom row cobblestone, top row:
    // x=0 planks/air, x=1 lamp/void, x=2 spruce log/-1.
    int top[3][2] = {{1, 2}, {3, 4}, {5, -1}};
    Nbt layer = Nbt::makeList(Nbt::Int), water = Nbt::makeList(Nbt::Int);
    for (int x = 0; x < 3; ++x)
        for (int y = 0; y < 2; ++y)
            for (int z = 0; z < 2; ++z) {
                layer.items.push_back(Nbt::makeInt(y == 0 ? 0 : top[x][z]));
                water.items.push_back(Nbt::makeInt(x == 0 && y == 0 && z == 0 ? 0 : -1));
            }
    Nbt indices = Nbt::makeList(Nbt::List);
    indices.items.push_back(layer);
    indices.items.push_back(water);
    Nbt def = Nbt::makeCompound();
    def.fields["block_palette"] = palette;
    Nbt pal = Nbt::makeCompound();
    pal.fields["default"] = def;
    Nbt structure = Nbt::makeCompound();
    structure.fields["block_indices"] = indices;
    structure.fields["palette"] = pal;
    structure.fields["entities"] = Nbt::makeList(Nbt::Compound);
    root.fields["structure"] = structure;
    return root.write();
}

static int testStructures() {
    // NBT round trip.
    std::vector<uint8_t> file = sampleMcStructure();
    Nbt parsed;
    std::string err;
    CHECK(Nbt::parse(file, parsed, err));
    CHECK(parsed.get("size") && parsed.get("size")->items[2].integer == 2);

    // Import maps names, keeps void cells untouched and reports what is missing.
    Structure s;
    ImportReport report;
    CHECK(importMcStructure(file, s, report, err));
    CHECK(s.sx == 3 && s.sy == 2 && s.sz == 2);
    CHECK(s.at(0, 0, 0) == COBBLESTONE && s.at(2, 0, 1) == COBBLESTONE);
    CHECK(s.at(0, 1, 0) == PLANKS && s.at(0, 1, 1) == AIR);
    CHECK(s.at(1, 1, 0) == Structure::kVoid && s.at(1, 1, 1) == Structure::kVoid);  // lamp unknown, void
    CHECK(s.at(2, 1, 0) == LOG && s.at(2, 1, 1) == Structure::kVoid);              // -1 is void
    CHECK(report.unknown.size() == 1 && report.unknown["minecraft:redstone_lamp"] == 1);
    CHECK(report.ignoredStates == 1 && report.secondLayer == 1);
    std::printf("mcstructure: %d cells, %d blocks, %d void, %zu unknown names\n", report.cells, report.placedBlocks,
                report.voidCells, report.unknown.size());

    // Broken files are rejected without crashing: every truncation and some random bytes.
    for (size_t cut = 0; cut < file.size(); cut += 7) {
        std::vector<uint8_t> part(file.begin(), file.begin() + cut);
        Structure t;
        ImportReport r;
        CHECK(!importMcStructure(part, t, r, err));
    }
    std::vector<uint8_t> noise(file);
    uint32_t seed = 99;
    for (int round = 0; round < 300; ++round) {
        std::vector<uint8_t> bad = noise;
        for (int k = 0; k < 4; ++k) {
            seed = seed * 1664525 + 1013904223;
            bad[seed % bad.size()] = (uint8_t)(seed >> 24);
        }
        Structure t;
        ImportReport r;
        importMcStructure(bad, t, r, err);  // must simply return
        if (t.sx) CHECK(t.cells.size() == (size_t)t.sx * t.sy * t.sz);
    }
    CHECK(!importMcStructure({'V', 'C', 'S', 'T'}, s, report, err) && !err.empty());

    // .vcstructure round trip.
    Structure back;
    s.name = "domek";
    CHECK(readVcStructure(writeVcStructure(s), back, err));
    CHECK(back.name == "domek" && back.sx == 3 && back.cells == s.cells);
    std::vector<uint8_t> vc = writeVcStructure(s);
    vc.resize(vc.size() - 2);
    CHECK(!readVcStructure(vc, back, err));

    // Rotation and mirroring.
    Structure r1 = s.rotated(1);
    CHECK(r1.sx == 2 && r1.sz == 3);
    // (x, z) -> (sz - 1 - z, x): planks at (0, 1, 0) go to (1, 1, 0); the log at (2, 1, 0) to (1, 1, 2).
    CHECK(r1.at(1, 1, 0) == PLANKS && r1.at(1, 1, 2) == LOG && r1.at(0, 1, 0) == AIR);
    CHECK(s.rotated(4).cells == s.cells && s.rotated(2).rotated(2).cells == s.cells);
    CHECK(s.rotated(1).rotated(3).cells == s.cells);
    CHECK(s.mirroredX().at(2, 1, 0) == PLANKS && s.mirroredX().mirroredX().cells == s.cells);

    // Placing into a real world, then undo.
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World w(4, Dimension::Overworld, flat);
    w.ensureChunksAround(0, 0, 2, 25);
    w.setBlock(14, 4, 14, GLASS);  // what the void cell at (1, 1, 1) covers stays
    std::vector<BlockChange> undo;
    CHECK(placeStructure(w, s, 13, 3, 13, undo, err));  // crosses into chunk (0,0) only... and from 13+2 = 15 still chunk 0
    CHECK(w.getBlock(13, 3, 13) == COBBLESTONE && w.getBlock(13, 4, 13) == PLANKS && w.getBlock(15, 4, 13) == LOG);
    CHECK(w.getBlock(14, 4, 14) == GLASS);
    // Across a chunk border and back.
    std::vector<BlockChange> undo2;
    CHECK(placeStructure(w, r1, 15, 10, -1, undo2, err));
    CHECK(w.getBlock(16, 11, -1) == PLANKS);
    undoPlacement(w, undo2);
    CHECK(w.getBlock(16, 11, -1) == AIR);
    undoPlacement(w, undo);
    CHECK(w.getBlock(13, 3, 13) == DIRT || w.getBlock(13, 3, 13) == GRASS);
    CHECK(w.getBlock(13, 4, 13) == AIR && w.getBlock(14, 4, 14) == GLASS);
    // Bedrock survives, unloaded areas and the sky limit are refused before anything changes.
    CHECK(placeStructure(w, s, 0, 0, 0, undo, err) == false);  // y = 0 is not allowed
    w.setBlock(1, 2, 0, BEDROCK);
    CHECK(placeStructure(w, s, 0, 2, 0, undo, err));
    CHECK(w.getBlock(1, 2, 0) == BEDROCK && w.getBlock(0, 2, 0) == COBBLESTONE);
    undoPlacement(w, undo);
    CHECK(w.getBlock(1, 2, 0) == BEDROCK);
    CHECK(!placeStructure(w, s, 500, 10, 0, undo, err) && undo.empty());
    CHECK(!placeStructure(w, s, 0, 127, 0, undo, err));

    // Capture from the world and the .mcstructure export read back by our importer.
    Structure cap;
    CHECK(captureStructure(w, 12, 2, 12, 4, 3, 4, cap, err));
    Structure again;
    ImportReport rr;
    CHECK(importMcStructure(exportMcStructure(cap), again, rr, err));
    CHECK(again.cells == cap.cells && rr.unknown.empty());

    // In the game: a creative world, Pause -> Struktury -> Umístit -> Vrátit zpět, and an imported file.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_structures_test";
    fs::remove_all(root);
    fs::create_directories(root / "structures");
    {
        std::ofstream(root / "structures" / "ukazka.mcstructure", std::ios::binary)
            .write(reinterpret_cast<const char*>(file.data()), (std::streamsize)file.size());
        std::ofstream(root / "structures" / "rozbita.mcstructure") << "nesmysl";
    }
    Game game(root.string());
    game.skipSplash();
    int imports = 0;
    game.onImportStructure = [&] { ++imports; };
    game.onTextPrompt = [](int, const std::string&, const std::string&) {};
    CHECK(game.onSurfaceCreated());
    game.onResize(2400, 1080);
    double t = 1;
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    frames(3);
    tap(222, 121);  // Hrát
    tap(164, 245);  // Nový svět
    tap(230, 92);   // Kreativní
    tap(216, 245);  // Vytvořit a hrát
    frames(30);
    CHECK(game.session() && !game.session()->survival());
    World& gw = game.session()->currentWorld();
    auto countBlocks = [&](uint8_t id) {
        Vec3 p = game.session()->player().pos;
        int n = 0;
        for (int x = (int)p.x - 16; x <= (int)p.x + 16; ++x)
            for (int z = (int)p.z - 16; z <= (int)p.z + 16; ++z)
                for (int y = 1; y < 100; ++y) n += gw.getBlock(x, y, z) == id;
        return n;
    };
    int bricksBefore = countBlocks(BRICKS);
    game.onBack();  // pause
    frames(2);
    tap(300, 142);  // Struktury
    tap(436, 222);  // Umístit před sebe (the first entry: the wooden house)
    int bricksAfter = countBlocks(BRICKS);
    std::printf("structures: house placed, bricks %d -> %d\n", bricksBefore, bricksAfter);
    CHECK(bricksAfter > bricksBefore + 30);
    game.onBack();
    frames(2);
    tap(300, 142);  // Struktury
    tap(164, 246);  // Vrátit zpět
    CHECK(countBlocks(BRICKS) == bricksBefore);
    tap(300, 246);  // Importovat soubor -> the platform picker
    CHECK(imports == 1);
    // The picker copied a file: the game keeps it only if it loads.
    fs::copy_file(root / "structures" / "ukazka.mcstructure", root / "structures" / "nova.mcstructure");
    game.onTextResult(9, "nova.mcstructure");
    frames(2);
    {
        std::ofstream(root / "structures" / "zla.vcstructure") << "VCST";
    }
    game.onTextResult(9, "zla.vcstructure");
    frames(2);
    CHECK(fs::exists(root / "structures" / "nova.mcstructure") && !fs::exists(root / "structures" / "zla.vcstructure"));
    game.onBack();
    frames(2);
    tap(300, 198);  // Uložit a odejít
    fs::remove_all(root);
    return 0;
}

static int countItems(const Inventory& inv, uint8_t id) {
    int n = 0;
    for (const ItemStack& s : inv.slots)
        if (s.id == id) n += s.count;
    return n;
}

static bool inventoryValid(const Inventory& inv) {
    for (const ItemStack& s : inv.slots) {
        if ((s.id == AIR) != (s.count == 0)) return false;
        if (s.count > maxStack(s.id)) return false;
    }
    return true;
}

static int testInventoryAndHand() {
    // 1-4: splitting, moving part of a stack, merging and the stack limit.
    ItemStack a{DIRT, 64}, b{}, c{};
    CHECK(transferItems(a, b, 32) == 32 && a.count == 32 && b.count == 32 && b.id == DIRT);
    CHECK(transferItems(a, c, 5) == 5 && a.count == 27 && c.count == 5);
    CHECK(transferItems(c, b, 5) == 5 && c.empty() && c.id == AIR && b.count == 37);
    ItemStack d{DIRT, 40}, e{DIRT, 40};
    CHECK(transferItems(d, e, 40) == 24 && e.count == 64 && d.count == 16);  // only up to 64
    CHECK(transferItems(d, e, 16) == 0 && d.count == 16 && e.count == 64);
    ItemStack f{STONE, 10};
    CHECK(transferItems(d, f, 5) == 0 && d.count == 16 && f.count == 10);  // a part cannot swap
    CHECK(transferItems(d, f, 16) == 16 && d.id == STONE && d.count == 10 && f.id == DIRT && f.count == 16);
    ItemStack pick{WOOD_PICKAXE, 1, 7}, slotP{};
    CHECK(transferItems(pick, slotP, 1) == 1 && slotP.damage == 7 && pick.empty());
    ItemStack p2{WOOD_PICKAXE, 1, 0};
    CHECK(transferItems(p2, slotP, 1) == 1 && slotP.damage == 0 && p2.damage == 7);  // tools never stack: swap

    // 12: thousands of random operations never create, lose or overfill anything.
    {
        Inventory inv;
        uint32_t r = 7;
        auto rnd = [&](int n) { r = r * 1664525u + 1013904223u; return (int)((r >> 8) % (uint32_t)n); };
        const uint8_t kinds[] = {DIRT, STONE, PLANKS, WOOD_PICKAXE};
        for (int i = 0; i < 20; ++i) {
            uint8_t id = kinds[rnd(4)];
            inv.slots[rnd(kInventorySize)] = {id, (uint8_t)(1 + rnd(maxStack(id)))};
        }
        int before[4];
        for (int k = 0; k < 4; ++k) before[k] = countItems(inv, kinds[k]);
        for (int op = 0; op < 20000; ++op) {
            int from = rnd(kInventorySize), to = rnd(kInventorySize);
            switch (rnd(3)) {
                case 0: transferItems(inv.slots[from], inv.slots[to], 1 + rnd(64)); break;
                case 1: moveIntoRange(inv.slots[from], 1 + rnd(64), inv.slots.data(), inv.slots.data() + kHotbarSize); break;
                default: inv.moveOrSwap(from, to); break;
            }
            CHECK(inventoryValid(inv));
        }
        for (int k = 0; k < 4; ++k) CHECK(countItems(inv, kinds[k]) == before[k]);
    }

    // In a survival world.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_inventory_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 5;
    info.mode = GameMode::Survival;
    info.name = "Inventář";
    auto s = std::make_unique<Session>(info, PlayerState{});
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    auto tapCentre = [&] {
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
    };
    run(1);
    Inventory& inv = s->inventory();
    inv.clear();
    inv.slots[0] = {PLANKS, 2};
    s->selectedSlot() = 0;
    run(0.5f);
    CHECK(s->hand().shown() == PLANKS && s->hand().equip() < 0.01f);

    // 6: a refused placement (into the player's own body, looking straight down) changes
    // nothing and does not play the placing animation.
    s->player().pitch = -1.57f;
    tapCentre();
    CHECK(inv.slots[0].count == 2 && s->hand().placeProgress() >= 1);
    // A real placement takes exactly one block and pushes the hand forward.
    s->player().pitch = -0.75f;
    tapCentre();
    std::printf("hand: after placing %d planks left, place animation at %.2f\n", inv.slots[0].count, s->hand().placeProgress());
    CHECK(inv.slots[0].count == 1 && s->hand().placeProgress() < 1);
    // 7: using up the stack empties the slot and the hand.
    s->player().yaw += 0.6f;
    tapCentre();
    CHECK(inv.slots[0].empty());
    run(0.3f);
    CHECK(s->hand().shown() == AIR);

    // 8: switching the hotbar slot lowers the hand and raises it with the new item.
    inv.slots[1] = {COBBLESTONE, 5};
    s->selectedSlot() = 1;
    run(1 / 60.0f);
    run(1 / 30.0f);
    float dip = s->hand().equip();
    run(0.5f);
    std::printf("hand: equip dip %.2f, now shows %d\n", dip, s->hand().shown());
    CHECK(dip > 0.05f && s->hand().shown() == COBBLESTONE && s->hand().equip() < 0.01f);

    // Walking bobs the hand, standing still calms it.
    s->player().yaw += kPi;  // away from the planks placed above
    s->touchDown(2, 52, 190, t);  // D-pad up
    float walkingBob = 0;
    for (int i = 0; i < 40; ++i) {
        run(1 / 60.0f);
        walkingBob = std::max(walkingBob, s->hand().bob());
    }
    s->touchUp(2, 52, 190, t);
    run(1.0f);
    std::printf("hand: bob %.2f walking, %.2f standing\n", walkingBob, s->hand().bob());
    CHECK(walkingBob > 0.3f && s->hand().bob() < 0.05f);

    // 5 and 9: mining adds the drop and the hand keeps striking while digging.
    s->selectedSlot() = 2;  // empty hand
    run(0.5f);
    int dirtBefore = countItems(inv, DIRT);
    s->player().pitch = -1.5f;
    s->touchDown(1, 300, 135, t);
    float minSwing = 1;
    for (int i = 0; i < 150; ++i) {
        run(1 / 60.0f);
        minSwing = std::min(minSwing, s->hand().swingProgress());
    }
    s->touchUp(1, 300, 135, t);
    run(0.1f);
    std::printf("hand: mining swing reached %.2f, dirt %d -> %d\n", minSwing, dirtBefore, countItems(inv, DIRT));
    CHECK(minSwing < 0.5f && countItems(inv, DIRT) > dirtBefore);
    // A full inventory: the mined block drops into the world instead of vanishing,
    // and is picked up once there is room again.
    for (int i = 0; i < kInventorySize; ++i) inv.slots[i] = {COBBLESTONE, 64};
    s->selectedSlot() = 0;
    s->player().pitch = -1.5f;
    s->touchDown(1, 300, 135, t);
    run(2.5f);
    s->touchUp(1, 300, 135, t);
    run(0.1f);
    int lying = s->drops().itemCount(0);
    std::printf("drops: %d item(s) lying next to a full inventory\n", lying);
    CHECK(lying >= 1);
    inv.slots[20] = {};  // make room for one stack
    run(1.5f);
    CHECK(s->drops().itemCount(0) == lying);  // out of reach at the bottom of the hole: still lying there
    const DroppedItem& d0 = s->drops().drops()[0];
    s->player().pos = Vec3(d0.pos.x, d0.pos.y - 0.12f, d0.pos.z);  // climb down to it
    run(0.5f);
    int inside = -35 * 64;
    for (const ItemStack& st : inv.slots) inside += st.count;
    std::printf("drops: after making room %d picked up, %d still lying\n", inside, s->drops().itemCount(0));
    CHECK(inside >= 1 && inside + s->drops().itemCount(0) == lying && inventoryValid(inv));

    // 10: save and load keep split stacks, the selected slot and items lying around.
    inv.clear();
    inv.slots[3] = {DIRT, 32};
    inv.slots[4] = {DIRT, 32};
    inv.slots[12] = {WOOD_PICKAXE, 1, 9};
    s->selectedSlot() = 4;
    s->throwItem({STONE, 3, 0});
    run(0.1f);
    s->save();
    std::string folder = info.folder;
    s.reset();
    WorldInfo info2;
    PlayerState st2;
    CHECK(loadLevel(folder, info2, st2));
    Session again(info2, st2);
    CHECK(again.inventory().slots[3].count == 32 && again.inventory().slots[4].count == 32 &&
          again.inventory().slots[12].damage == 9 && again.selectedSlot() == 4);
    CHECK(again.drops().itemCount(0) == 3);
    fs::remove_all(root);

    // 11: the inventory screen by touch only: pick, halve, move 5, merge, quick move, throw.
    fs::create_directories(root);
    Game game(root.string());
    game.skipSplash();
    game.onTextPrompt = [](int, const std::string&, const std::string&) {};
    CHECK(game.onSurfaceCreated());
    game.onResize(2400, 1080);
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    frames(3);
    tap(222, 121);  // Hrát
    tap(164, 245);  // Nový svět
    tap(216, 245);  // Vytvořit a hrát (survival)
    frames(30);
    Inventory& gi = game.session()->inventory();
    gi.clear();
    gi.slots[9] = {DIRT, 64};
    tap(390, 258);  // "..." inventory button
    auto mainSlot = [](int j) { return std::pair<float, float>{151 + (j % 9) * 20 + 10, 141 + (j / 9) * 20 + 10}; };
    auto hotbarSlot = [](int i) { return std::pair<float, float>{151 + i * 20 + 10, 215}; };
    auto tapAt = [&](std::pair<float, float> p) { tap(p.first, p.second); };
    tapAt(mainSlot(0));  // pick the 64 dirt
    tap(374, 105);       // Půlka
    tapAt(mainSlot(1));  // put 32 into the next slot
    CHECK(gi.slots[9].count == 32 && gi.slots[10].count == 32);
    tap(400, 87);        // "1" (the rest stays picked after a split)
    for (int i = 0; i < 4; ++i) tap(434, 87);  // "+" four times: 5
    tapAt(mainSlot(2));
    CHECK(gi.slots[9].count == 27 && gi.slots[11].count == 5);
    tap(425, 105);       // Vše: 27
    tapAt(mainSlot(1));  // merge onto the 32: 59
    CHECK(gi.slots[9].empty() && gi.slots[10].count == 59);
    tapAt(mainSlot(2));  // pick the 5
    tap(400, 127);       // Do hotbaru
    CHECK(gi.slots[11].empty() && gi.slots[0].count == 5);
    tapAt(hotbarSlot(0));
    tap(400, 87);        // 1
    tap(400, 145);       // Vyhodit: one block thrown into the world
    CHECK(gi.slots[0].count == 4 && game.session()->drops().itemCount(0) == 1);
    // Fast random tapping on slots and panel buttons never duplicates or loses dirt.
    uint32_t r = 3;
    for (int i = 0; i < 400; ++i) {
        r = r * 1664525u + 1013904223u;
        int what = (r >> 8) % 10;
        if (what < 6) tapAt(mainSlot((r >> 12) % 27));
        else if (what < 8) tapAt(hotbarSlot((r >> 12) % 9));
        else if (what == 8) tap(374, 105);
        else tap(434, 87);
    }
    const int thrown = game.session()->drops().itemCount(0);
    int inHand = 0;
    for (const ItemStack& st : gi.slots) inHand += st.id == DIRT ? st.count : 0;
    std::printf("inventory: after 400 random taps %d dirt in the inventory, %d thrown\n", inHand, thrown);
    CHECK(inventoryValid(gi) && inHand + game.session()->drops().itemCount(0) == 64);
    game.onBack();
    frames(2);
    game.onBack();
    frames(2);
    fs::remove_all(root);
    return 0;
}

static int testMetalsAndArmor() {
    // Ores are generated at their depths.
    World w(4242, Dimension::Overworld);
    w.ensureChunksAround(0, 0, 4, 64);
    int bronze = 0, steel = 0, ruby = 0, rubyTooHigh = 0;
    for (auto& [k, c] : w.chunks())
        for (int y = 1; y < kChunkHeight; ++y)
            for (int x = 0; x < 16; ++x)
                for (int z = 0; z < 16; ++z) {
                    ItemId b = c->get(x, y, z);
                    bronze += b == BRONZE_ORE;
                    steel += b == STEEL_ORE;
                    ruby += b == RUBY_ORE;
                    rubyTooHigh += b == RUBY_ORE && y > 20;
                }
    std::printf("metals: ores in %zu chunks: bronze %d, steel %d, ruby %d\n", w.chunks().size(), bronze, steel, ruby);
    CHECK(bronze > steel && steel > ruby && ruby > 0 && rubyTooHigh == 0);
    // Tool tiers: each ore needs the tier below it, ruby ore drops a ruby.
    CHECK(!canHarvest(BRONZE_ORE, WOOD_PICKAXE) && canHarvest(BRONZE_ORE, STONE_PICKAXE));
    CHECK(!canHarvest(STEEL_ORE, STONE_PICKAXE) && canHarvest(STEEL_ORE, BRONZE_PICKAXE) && canHarvest(STEEL_ORE, IRON_PICKAXE));
    CHECK(!canHarvest(RUBY_ORE, IRON_PICKAXE) && canHarvest(RUBY_ORE, STEEL_PICKAXE) && dropFor(RUBY_ORE, 0.5f, STEEL_PICKAXE) == RUBY);
    CHECK(breakTime(STONE, RUBY_PICKAXE) < breakTime(STONE, STEEL_PICKAXE) && breakTime(STONE, STEEL_PICKAXE) < breakTime(STONE, IRON_PICKAXE));
    CHECK(toolDurability(RUBY_PICKAXE) > toolDurability(STEEL_PICKAXE) && toolDurability(STEEL_PICKAXE) > toolDurability(BRONZE_PICKAXE));
    CHECK(maxStack(STEEL_AXE) == 1 && maxStack(armorItem(4, 1)) == 1 && maxStack(RUBY) == 64);
    // Recipes and smelting from the default pack.
    auto grid = [](std::initializer_list<uint8_t> ids) {
        std::array<ItemStack, 9> g{};
        int i = 0;
        for (uint8_t id : ids) { if (id != AIR) g[i] = {id, 1}; ++i; }
        return g;
    };
    auto g = grid({RUBY, RUBY, RUBY, AIR, STICK, AIR, AIR, STICK, AIR});
    CHECK(matchRecipe(g.data(), 3).id == RUBY_PICKAXE);
    g = grid({STEEL_INGOT, AIR, STEEL_INGOT, STEEL_INGOT, STEEL_INGOT, STEEL_INGOT, STEEL_INGOT, STEEL_INGOT, STEEL_INGOT});
    CHECK(matchRecipe(g.data(), 3).id == armorItem(3, 1));
    g = grid({PLANKS, AIR, PLANKS, PLANKS, AIR, PLANKS, AIR, AIR, AIR});
    CHECK(matchRecipe(g.data(), 3).id == armorItem(0, 3));
    CHECK(smeltResult(BRONZE_ORE) == BRONZE_INGOT && smeltResult(STEEL_ORE) == STEEL_INGOT);
    // Every new item has a name, a key that maps back, and a texture tile of its own.
    for (int id = 1; id < 256; ++id) {
        if (std::string(itemKey((ItemId)id)) == "air") continue;
        CHECK(itemFromKey(itemKey((ItemId)id)) == id && (itemName((ItemId)id)[0] != '\0' || id == PORTAL));
    }
    CHECK(tileFor(RUBY_SHOVEL, F_SOUTH) == T_FIRST_NEW_TOOL + 8 && tileFor(armorItem(4, 3), F_SOUTH) == T_FIRST_ARMOR + 19);
    CHECK(tileFor(WOOL_BLACK, F_TOP) == T_WOOL + 15 && T_COUNT <= kAtlasCols * kAtlasCols);

    // Armour: protection, wear, equipping by tap, saving.
    ItemStack full[4] = {{armorItem(4, 0), 1}, {armorItem(4, 1), 1}, {armorItem(4, 2), 1}, {armorItem(4, 3), 1}};
    CHECK(std::fabs(armorReduction(full) - 0.8f) < 1e-4f);
    ItemStack wood[4] = {{armorItem(0, 0), 1}, {}, {}, {}};
    CHECK(std::fabs(armorReduction(wood) - 0.04f) < 1e-4f);
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_armor_test";
    fs::remove_all(root);
    Settings settings;
    settings.renderDistance = 2;
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 5;
    info.mode = GameMode::Survival;
    auto s = std::make_unique<Session>(info, PlayerState{});
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    run(1);
    s->inventory().clear();
    s->inventory().slots[0] = {armorItem(3, 1), 1};  // steel chestplate in the hand
    s->selectedSlot() = 0;
    s->player().pitch = -0.5f;
    s->touchDown(1, 300, 135, t);
    run(1 / 60.0f);
    s->touchUp(1, 300, 135, t);
    run(1 / 60.0f);
    CHECK(s->armor()[1].id == armorItem(3, 1) && s->inventory().slots[0].empty() && s->armorPoints() == 6);
    // A fall: 10 blocks = 7 damage, steel chestplate takes 24 %.
    float before = s->stats().health;
    Vec3 p = s->player().pos;
    s->player().pos = p + Vec3(0, 10, 0);
    s->player().vel = {};
    run(2.5f);
    float lost = before - s->stats().health;
    std::printf("armor: a 10 block fall took %.1f health with 6 armour points (7 without)\n", lost);
    CHECK(lost > 4.5f && lost < 6.0f && s->armor()[1].damage == 1);
    s->save();
    std::string folder = info.folder;
    s.reset();
    WorldInfo info2;
    PlayerState st2;
    CHECK(loadLevel(folder, info2, st2) && st2.armor[1].id == armorItem(3, 1) && st2.armor[1].damage == 1 && st2.armor[0].empty());
    fs::remove_all(root);
    return 0;
}

static int testDeveloperAccess() {
    CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex(std::string(1000, 'a')).size() == 64);
    CHECK(checkDeveloperLogin("Hoppy_Hopsy21", "Michal2006"));
    CHECK(!checkDeveloperLogin("Hoppy_Hopsy21", "michal2006") && !checkDeveloperLogin("Hoppy", "Michal2006") &&
          !checkDeveloperLogin("Hoppy_Hopsy21", ""));

    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_dev_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        // Developer options in options.txt do nothing for a normal player.
        Settings cheat;
        cheat.devGodMode = true;
        saveSettings((root / "options.txt").string(), cheat);
    }
    Game game(root.string());
    game.skipSplash();
    int prompts = 0, passwordPrompts = 0;
    game.onTextPrompt = [&](int, const std::string&, const std::string&) { ++prompts; };
    game.onPasswordPrompt = [&](int, const std::string&) { ++passwordPrompts; };
    CHECK(game.onSurfaceCreated());
    game.onResize(2400, 1080);
    double t = 1;
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    frames(3);
    tap(222, 121);  // Hrát
    tap(164, 245);  // Nový svět
    tap(216, 245);  // Vytvořit a hrát (survival)
    frames(30);
    Session* s = game.session();
    CHECK(s && s->survival());
    float before = s->stats().health;
    s->player().pos = s->player().pos + Vec3(0, 12, 0);
    frames(150);
    CHECK(s->stats().health < before);  // a normal player is hurt despite devGodMode in the file
    s->healAndFeed();

    // Wrong password, then the right one: name prompt, masked password prompt.
    game.onTextResult(10, "Hoppy_Hopsy21");
    frames(1);
    CHECK(passwordPrompts == 1);
    game.onTextResult(11, std::getenv("VC_DEV_PASSWORD") ? std::getenv("VC_DEV_PASSWORD") : "");
    frames(1);
    CHECK(!game.profile().isDeveloper());
    game.onTextResult(10, "Hoppy_Hopsy21");
    frames(1);
    game.onTextResult(11, std::getenv("VC_DEV_PASSWORD") ? std::getenv("VC_DEV_PASSWORD") : "");
    frames(1);
    CHECK(game.profile().isDeveloper() && game.profile().name == "Hoppy_Hopsy21" && game.profile().skin == "developer");
    CHECK(developerSkin().valid() && developerSkin().developerOnly && capture::playerSkin == developerSkin().rgba);
    // Now the same option works: no fall damage.
    before = s->stats().health;
    s->player().pos = s->player().pos + Vec3(0, 12, 0);
    frames(150);
    CHECK(s->stats().health == before);
    // Lockout after five wrong passwords, even with the right one afterwards.
    const_cast<Profile&>(game.profile()).developer = false;  // signed out
    for (int i = 0; i < 5; ++i) {
        game.onTextResult(10, "Hoppy_Hopsy21");
        game.onTextResult(11, std::getenv("VC_DEV_PASSWORD") ? std::getenv("VC_DEV_PASSWORD") : "");
        frames(1);
    }
    game.onTextResult(10, "Hoppy_Hopsy21");
    game.onTextResult(11, std::getenv("VC_DEV_PASSWORD") ? std::getenv("VC_DEV_PASSWORD") : "");
    frames(1);
    CHECK(!game.profile().isDeveloper());
    fs::remove_all(root);
    return 0;
}

// Walks through the menus and the game in English and collects every drawn text:
// none may still contain Czech letters (a missing translation).
static int testEnglishEverywhere() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_lang_test";
    fs::remove_all(root);
    fs::create_directories(root);
    std::vector<std::string> log;
    Ui::textLog = &log;
    Game game(root.string());
    game.skipSplash();
    game.onTextPrompt = [](int, const std::string&, const std::string&) {};
    CHECK(game.onSurfaceCreated());
    game.onResize(2400, 1080);
    setLanguage(Language::English);
    double t = 1;
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    frames(3);
    tap(378, 121);  // Settings
    for (float y : {65.0f, 91.0f, 117.0f, 143.0f, 169.0f}) tap(103, y);  // every tab
    game.onBack();
    frames(2);
    tap(222, 149);  // Store
    game.onBack();
    frames(2);
    tap(378, 149);  // Profile & skins
    game.onBack();
    frames(2);
    tap(300, 177);  // Packs
    tap(426, 168);  // edit JSON files
    game.onBack();
    frames(2);
    game.onBack();
    frames(2);
    tap(222, 121);  // Play
    tap(216, 245);  // New world
    tap(230, 92);   // Creative
    tap(216, 245);  // Create
    frames(30);
    tap(390, 258);  // creative inventory
    for (float y : {36.0f, 60.0f, 84.0f, 108.0f, 132.0f}) tap(114, y);  // every tab
    game.onBack();
    frames(2);
    game.onBack();  // pause
    frames(2);
    tap(300, 142);  // Structures
    game.onBack();
    frames(2);
    game.onTextResult(10, "Hoppy_Hopsy21");
    game.onTextResult(11, std::getenv("VC_DEV_PASSWORD") ? std::getenv("VC_DEV_PASSWORD") : "");
    frames(2);
    tap(300, 79 + 3 * 28 - 14);  // Developer
    frames(2);
    game.onBack();
    frames(2);
    game.session()->setMode(GameMode::Survival);
    tap(300, 51 + 24 + 11);  // back to the game
    frames(2);
    tap(390, 258);  // survival inventory
    game.onBack();
    frames(2);
    Ui::textLog = nullptr;
    std::set<std::string> bad;
    const std::string czech = "ěščřžýáíéůúďťňĚŠČŘŽÝÁÍÉŮÚĎŤŇ";
    for (const std::string& line : log)
        for (size_t i = 0; i + 1 < line.size(); ++i)
            if ((unsigned char)line[i] >= 0xC3 && czech.find(line.substr(i, 2)) != std::string::npos) {
                bad.insert(line);
                break;
            }
    for (const std::string& b : bad) std::printf("  untranslated: %s\n", b.c_str());
    std::printf("lang: %zu texts drawn in English, %zu untranslated, %d table entries\n", log.size(), bad.size(), translationCount());
    CHECK(log.size() > 500 && bad.empty());
    fs::remove_all(root);
    return 0;
}

static int testMusic() {
    // The biome pieces: about a minute, never silent for long, no clipping.
    for (int i = 0; i < (int)MusicStyle::Count; ++i) {
        std::vector<float> m = synthesizeBiomeSamples((MusicStyle)i);
        float seconds = m.size() / (float)kSampleRate, peak = 0;
        for (float v : m) peak = std::max(peak, std::fabs(v));
        int quietSeconds = 0;
        for (size_t s0 = 0; s0 + kSampleRate < m.size() - kSampleRate * 5; s0 += kSampleRate) {
            float e = 0;
            for (size_t k = s0; k < s0 + kSampleRate; ++k) e += m[k] * m[k];
            quietSeconds += std::sqrt(e / kSampleRate) < 0.01f;
        }
        CHECK(seconds > 50 && seconds < 80 && peak <= 0.86f && peak > 0.5f && quietSeconds == 0);
    }
    // The music director: menu theme, then in the world the Overworld song and the biome
    // piece take turns with pauses; a song of the player's own replaces the built-in one.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_music_test";
    fs::remove_all(root);
    fs::create_directories(root);
    Game game(root.string());
    game.skipSplash();
    std::vector<std::pair<std::string, bool>> played;
    game.onPlayMusic = [&](const std::string& src, bool loop) { played.emplace_back(src, loop); };
    game.onTextPrompt = [](int, const std::string&, const std::string&) {};
    CHECK(game.onSurfaceCreated());
    game.onResize(2400, 1080);
    double t = 1;
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    frames(3);
    CHECK(!played.empty() && played.back().first == "asset:music/menu_theme.mp3" && played.back().second);
    tap(222, 121);
    tap(216, 245);
    tap(216, 245);
    frames(30);
    CHECK(played.back().first.empty());  // a short silence when the world opens
    frames(150);
    CHECK(game.musicSource() == "asset:music/overworld.mp3" && !played.back().second);
    game.onTextResult(12, "asset:music/overworld.mp3");  // the song ended
    frames(5);
    CHECK(game.musicSource().empty());
    frames(60 * 50);  // the pause is 20-45 s
    std::printf("music: after the Overworld song comes %s\n", game.musicSource().c_str());
    CHECK(game.musicSource().find("/sounds/biome_") != std::string::npos);
    // The player's own Overworld song.
    fs::create_directories(root / "music");
    std::ofstream(root / "music" / "overworld.mp3") << "x";
    game.onTextResult(12, game.musicSource());
    frames(60 * 50);
    CHECK(game.musicSource() == (root / "music" / "overworld.mp3").string());
    // Back in the menu: the theme again.
    game.onBack();
    frames(2);
    tap(300, 184);  // save and quit
    frames(2);
    CHECK(game.musicSource() == "asset:music/menu_theme.mp3");
    fs::remove_all(root);
    return 0;
}

// A flat creative-or-survival world for block interaction tests.
static std::unique_ptr<Session> flatSession(const std::filesystem::path& root, GameMode mode) {
    std::filesystem::remove_all(root);
    WorldInfo info;
    info.folder = createWorldFolder(root.string());
    info.seed = 9;
    info.mode = mode;
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    saveGenerator(info.folder, flat);
    return std::make_unique<Session>(info, PlayerState{});
}

static int testDoorsChests() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_doors_test";
    auto s = flatSession(root, GameMode::Survival);
    Settings settings;
    settings.renderDistance = 2;
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    auto tap = [&] {
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
    };
    run(1);
    World& w = s->currentWorld();
    Player& p = s->player();
    p.pos = Vec3(8.5f, 4, 8.5f);
    p.yaw = 0;  // looking north (-z)
    p.pitch = -0.6f;
    run(0.2f);
    // A door: two halves on the ground in front of the player.
    s->inventory().clear();
    s->inventory().slots[0] = {OAK_DOOR_ITEM, 2};
    s->selectedSlot() = 0;
    tap();
    int dz = -1;
    while (dz > -4 && !isDoor(w.getBlock(8, 4, 8 + dz))) --dz;
    const int doorZ = 8 + dz;
    std::printf("doors: placed at z %d: %d / %d\n", doorZ, w.getBlock(8, 4, doorZ), w.getBlock(8, 5, doorZ));
    CHECK(isDoor(w.getBlock(8, 4, doorZ)) && isDoor(w.getBlock(8, 5, doorZ)) && doorState(w.getBlock(8, 5, doorZ)).upper);
    CHECK(s->inventory().slots[0].count == 1 && !doorState(w.getBlock(8, 4, doorZ)).open);
    // Closed, it blocks the way; open, the player walks through.
    Player probe;
    probe.pos = Vec3(8.5f, 4, doorZ + 0.1f);
    CHECK(probe.collides(w, probe.pos));
    s->toggleDoor(8, 5, doorZ);  // tapping the upper half opens both
    CHECK(doorState(w.getBlock(8, 4, doorZ)).open && doorState(w.getBlock(8, 5, doorZ)).open);
    probe.pos = Vec3(8.5f, 4, doorZ + 0.5f);
    CHECK(!probe.collides(w, probe.pos));
    // The crosshair passes an open door's empty part and hits what is behind it.
    w.setBlock(8, 4, doorZ - 1, STONE);
    RayHit through = raycast(w, Vec3(8.5f, 4.5f, doorZ + 1.5f), Vec3(0, 0, -1), 5);
    CHECK(through.hit && through.z == doorZ - 1);
    // Breaking the lower half removes the door and gives it back.
    int doorsBefore = countItems(s->inventory(), OAK_DOOR_ITEM);
    s->setMode(GameMode::Creative);
    s->setMode(GameMode::Survival);
    RayHit h{true, 8, 4, doorZ, 0, 0, 1};
    w.setBlock(8, 4, doorZ, doorId(OAK_DOOR, {0, false, false}));
    w.setBlock(8, 5, doorZ, doorId(OAK_DOOR, {0, false, true}));
    // (Session::breakBlock is private; dig it like a player instead.)
    p.pos = Vec3(8.5f, 4, doorZ + 1.6f);
    p.yaw = 0;
    p.pitch = -0.35f;
    s->inventory().slots[1] = {WOOD_AXE, 1};
    s->selectedSlot() = 1;
    s->touchDown(1, 300, 135, t);
    run(6);
    s->touchUp(1, 300, 135, t);
    run(0.2f);
    std::printf("doors: after digging %d %d, doors in inventory %d -> %d\n", w.getBlock(8, 4, doorZ), w.getBlock(8, 5, doorZ),
                doorsBefore, countItems(s->inventory(), OAK_DOOR_ITEM));
    CHECK(!isDoor(w.getBlock(8, 4, doorZ)) && !isDoor(w.getBlock(8, 5, doorZ)));
    CHECK(countItems(s->inventory(), OAK_DOOR_ITEM) == doorsBefore + 1);
    (void)h;

    // Trapdoor: closed it is a floor, open it is a wall.
    w.setBlock(3, 4, 3, doorId(OAK_TRAPDOOR, {0, false, false}));
    probe.pos = Vec3(3.5f, 4.05f, 3.5f);
    CHECK(probe.collides(w, Vec3(3.5f, 4.1f, 3.5f)));
    s->toggleDoor(3, 4, 3);
    CHECK(doorState(w.getBlock(3, 4, 3)).open && !probe.collides(w, Vec3(3.5f, 4.1f, 3.5f)));
    // Fence: 1.5 blocks high, nobody jumps over it.
    w.setBlock(12, 4, 12, FENCE);
    CHECK(probe.collides(w, Vec3(12.5f, 5.3f, 12.5f)) && !probe.collides(w, Vec3(12.5f, 5.55f, 12.5f)));
    // Cactus hurts.
    w.setBlock(4, 4, 12, CACTUS);
    p.pos = Vec3(4.5f, 4, 13.25f);
    float hp = s->stats().health;
    run(1);
    CHECK(s->stats().health < hp);
    s->healAndFeed();

    // Chests: keep their items, save them, spill them when broken.
    w.setBlock(6, 4, 2, CHEST);
    s->openChestAt({0, 6, 4, 2});
    CHECK(s->wantsChest && s->chest({0, 6, 4, 2}));
    (*s->chest({0, 6, 4, 2}))[5] = {RUBY, 7, 0};
    // Structure chests: loot the first time, always the same for the same place.
    w.setBlock(6, 4, 4, LOOT_CHEST_SHIP);
    s->openChestAt({0, 6, 4, 4});
    ChestContents loot = *s->chest({0, 6, 4, 4});
    int lootItems = 0;
    for (const ItemStack& st : loot) lootItems += st.empty() ? 0 : 1;
    std::printf("chests: shipwreck chest has %d stacks\n", lootItems);
    CHECK(lootItems >= 4 && w.getBlock(6, 4, 4) == CHEST);
    CHECK(generateLoot(LOOT_CHEST_SHIP, 1234) == generateLoot(LOOT_CHEST_SHIP, 1234));
    CHECK(!(generateLoot(LOOT_CHEST_DUNGEON, 1) == generateLoot(LOOT_CHEST_DUNGEON, 2)));
    s->save();
    std::string folder = s->info().folder;
    auto chestsBack = loadChests(folder);
    const FurnaceKey key{0, 6, 4, 2};
    CHECK(chestsBack.count(key) && chestsBack[key][5].id == RUBY && chestsBack[key][5].count == 7);
    fs::remove_all(root);
    return 0;
}

static int testNewBiomes() {
    // A large area of the default world has every new feature somewhere.
    World w(4242, Dimension::Overworld);
    int spruce = 0, ice = 0, cactus = 0, hardened = 0, snow = 0, floatingCactus = 0;
    std::set<std::string> biomes;
    for (int cx = -24; cx <= 24; cx += 3)
        for (int cz = -24; cz <= 24; cz += 3) {
            w.ensureChunksAround(cx, cz, 0, 1);
            const Chunk* c = w.getChunk(cx, cz);
            if (!c) continue;
            if (const BiomeDef* b = w.biomeAt(cx * 16 + 8, cz * 16 + 8)) biomes.insert(b->id);
            for (int x = 0; x < 16; ++x)
                for (int z = 0; z < 16; ++z)
                    for (int y = 1; y < kChunkHeight; ++y) {
                        ItemId b = c->get(x, y, z);
                        spruce += b == SPRUCE_LOG;
                        ice += b == ICE;
                        hardened += b == HARDENED_STONE;
                        snow += b == SNOW_BLOCK;
                        if (b == CACTUS) {
                            ++cactus;
                            ItemId below = c->get(x, y - 1, z);
                            floatingCactus += below != SAND && below != CACTUS;
                        }
                    }
        }
    std::printf("biomes: %zu kinds seen, spruce %d, ice %d, cactus %d, hardened stone %d, snow %d\n", biomes.size(), spruce,
                ice, cactus, hardened, snow);
    CHECK(biomes.count("snowy_taiga") && biomes.count("ice_plains") && biomes.count("mountains") && biomes.count("desert"));
    CHECK(spruce > 50 && ice > 50 && cactus > 5 && floatingCactus == 0 && hardened > 50 && snow > 200);

    // Upgrading the default pack: untouched old files get the new content, edited ones stay.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_upgrade_test";
    fs::remove_all(root);
    fs::create_directories(root / "packs" / "default" / "biomes");
    std::ofstream(root / "packs" / "default" / "manifest.json") << "{\"name\": \"VoxelCraft Default\"}\n";
    for (const auto& [path, old] : upgradableDefaults()) std::ofstream(root / "packs" / "default" / path) << old << "\n";
    std::ofstream(root / "packs" / "default" / "biomes" / "plains.json") << "{\"id\": \"plains\", \"name\": \"Moje pláně\"}\n";
    PackManager pm(root.string());
    pm.ensureDefaultPack();
    std::string desert = pm.readFile("default", "biomes/desert.json"), plains = pm.readFile("default", "biomes/plains.json");
    CHECK(desert.find("cactus") != std::string::npos && plains.find("Moje pláně") != std::string::npos);
    CHECK(fs::exists(root / "packs" / "default" / "biomes" / "snowy_taiga.json"));
    fs::remove_all(root);
    return 0;
}

static int testWorldStructures() {
    World w(4242, Dimension::Overworld);
    std::map<ItemId, int> chests;
    int fences = 0, wetShipChests = 0, deepChests = 0;
    for (int cx = -20; cx <= 20; ++cx)
        for (int cz = -20; cz <= 20; ++cz) {
            w.ensureChunksAround(cx, cz, 0, 1);
            const Chunk* c = w.getChunk(cx, cz);
            for (int x = 0; x < 16; ++x)
                for (int z = 0; z < 16; ++z)
                    for (int y = 1; y < kChunkHeight; ++y) {
                        ItemId b = c->get(x, y, z);
                        if (b >= LOOT_CHEST_RUIN && b <= LOOT_CHEST_MINE) {
                            ++chests[b];
                            if (b == LOOT_CHEST_SHIP && y < 62) ++wetShipChests;
                            if ((b == LOOT_CHEST_DUNGEON || b == LOOT_CHEST_MINE) && y < 45) ++deepChests;
                        }
                        fences += b == FENCE;
                    }
        }
    std::printf("structures: chests ruin %d, ship %d, dungeon %d, mine %d; fences %d\n", chests[LOOT_CHEST_RUIN],
                chests[LOOT_CHEST_SHIP], chests[LOOT_CHEST_DUNGEON], chests[LOOT_CHEST_MINE], fences);
    // Shipwrecks need open sea: find an ocean and look around there.
    {
        int ox = 0, oz = 0;
        bool found = false;
        for (int x = -2000; x < 2000 && !found; x += 32)
            for (int z = -2000; z < 2000 && !found; z += 32)
                if (w.surfaceHeight(x, z) <= 48 && w.surfaceHeight(x + 64, z) <= 52 && w.surfaceHeight(x, z + 64) <= 52) {
                    ox = x;
                    oz = z;
                    found = true;
                }
        CHECK(found);
        for (int cx = floorDiv(ox, 16) - 12; cx <= floorDiv(ox, 16) + 12; ++cx)
            for (int cz = floorDiv(oz, 16) - 12; cz <= floorDiv(oz, 16) + 12; ++cz) {
                w.ensureChunksAround(cx, cz, 0, 1);
                const Chunk* c = w.getChunk(cx, cz);
                for (int i = 0; i < 16 * 16 * kChunkHeight; ++i)
                    if (c->blocks[i] == LOOT_CHEST_SHIP) {
                        ++chests[LOOT_CHEST_SHIP];
                        wetShipChests += (i & 127) < 62;
                    }
            }
        std::printf("structures: %d shipwreck chests in the ocean near %d %d\n", chests[LOOT_CHEST_SHIP], ox, oz);
    }
    CHECK(chests[LOOT_CHEST_RUIN] > 0 && chests[LOOT_CHEST_SHIP] > 0 && chests[LOOT_CHEST_DUNGEON] > 0 && chests[LOOT_CHEST_MINE] > 0);
    CHECK(wetShipChests == chests[LOOT_CHEST_SHIP] && deepChests == chests[LOOT_CHEST_DUNGEON] + chests[LOOT_CHEST_MINE]);
    CHECK(fences > 50);
    // The same chunk comes out the same whatever order the chunks were generated in.
    World a(4242, Dimension::Overworld), b(4242, Dimension::Overworld);
    for (int cx = -6; cx <= 6; ++cx)
        for (int cz = -6; cz <= 6; ++cz) a.ensureChunksAround(cx, cz, 0, 1);
    for (int cx = 6; cx >= -6; --cx)
        for (int cz = 6; cz >= -6; --cz) b.ensureChunksAround(cx, cz, 0, 1);
    int differences = 0;
    for (int cx = -6; cx <= 6; ++cx)
        for (int cz = -6; cz <= 6; ++cz)
            differences += std::memcmp(a.getChunk(cx, cz)->blocks, b.getChunk(cx, cz)->blocks, sizeof a.getChunk(cx, cz)->blocks) != 0;
    CHECK(differences == 0);
    // Every blueprint has a chest and fits its size bound.
    for (int k = 0; k < (int)WorldStructure::Count; ++k)
        for (uint32_t seed : {1u, 77u, 12345u}) {
            Structure s = buildWorldStructure((WorldStructure)k, seed);
            int loot = 0;
            for (ItemId c : s.cells) loot += isChest(c) && c != CHEST;
            CHECK(s.valid() && (loot >= 1 || k == (int)WorldStructure::BossArena || k == (int)WorldStructure::Village) && s.sx <= 97 && s.sz <= 97);
        }
    return 0;
}

static int testBoat() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "vc_boat_test";
    auto s = flatSession(root, GameMode::Survival);
    Settings settings;
    settings.renderDistance = 2;
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    auto tap = [&] {
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
    };
    run(1);
    World& w = s->currentWorld();
    // A lake 24 blocks long, 2 deep.
    for (int x = 0; x < 24; ++x)
        for (int z = 0; z < 6; ++z)
            for (int y = 2; y <= 3; ++y) w.setBlock(x, y, z, WATER);
    Player& p = s->player();
    p.pos = Vec3(2.5f, 4, 7.5f);  // on the shore
    p.yaw = 0;                     // looking north over the water
    p.pitch = -0.45f;
    s->inventory().clear();
    s->inventory().slots[0] = {BOAT, 1};
    s->selectedSlot() = 0;
    run(0.1f);
    tap();
    CHECK(s->boats().boats().size() == 1 && s->inventory().slots[0].empty());
    const Boat placed = s->boats().boats()[0];
    std::printf("boat: placed at %.1f %.2f %.1f\n", placed.pos.x, placed.pos.y, placed.pos.z);
    CHECK(w.getBlock((int)std::floor(placed.pos.x), 3, (int)std::floor(placed.pos.z)) == WATER);
    // Tap on it: in. The D-pad rows it along the lake, the shore stops it.
    run(0.5f);
    tap();
    CHECK(s->riding());
    p.yaw = kPi / 2;  // look east, along the lake
    s->touchDown(2, 52, 190, t);  // D-pad up
    run(4);
    s->touchUp(2, 52, 190, t);
    run(0.5f);
    const Boat& b = s->boats().boats()[0];
    std::printf("boat: rowed to %.1f %.2f %.1f\n", b.pos.x, b.pos.y, b.pos.z);
    CHECK(b.pos.x > placed.pos.x + 8 && b.pos.x < 24 && b.pos.y > 3.2f && b.pos.y < 4.2f);
    CHECK(std::fabs(p.pos.x - b.pos.x) < 0.01f && s->stats().health == 20);
    // Jump: out of the boat, onto the shore or into the water next to it.
    s->touchDown(3, 560, 245, t);
    s->touchUp(3, 560, 245, t);
    run(0.1f);
    CHECK(!s->riding());
    // Saved with the world.
    s->save();
    BoatSystem loaded;
    loaded.load(s->info().folder);
    CHECK(loaded.boats().size() == 1 && std::fabs(loaded.boats()[0].pos.x - b.pos.x) < 0.01f);
    // Hitting it gives the boat back.
    p.pos = Vec3(b.pos.x - 2.2f, 4, 6.5f);
    p.yaw = std::atan2(b.pos.x - p.pos.x, -(b.pos.z - p.pos.z));
    p.pitch = -0.35f;
    run(0.1f);
    s->touchDown(1, 300, 135, t);
    run(2);
    s->touchUp(1, 300, 135, t);
    run(0.2f);
    std::printf("boat: after hitting it %zu boats, %d in the inventory\n", s->boats().boats().size(),
                countItems(s->inventory(), BOAT));
    CHECK(s->boats().boats().empty() && countItems(s->inventory(), BOAT) == 1);
    fs::remove_all(root);
    return 0;
}

static int testAchievements() {
    namespace fs = std::filesystem;
    // Every achievement has a unique id, a name, a description and a reward.
    std::set<std::string> ids;
    for (int i = 0; i < kAchievementCount; ++i) {
        CHECK(ids.insert(kAchievements[i].id).second);
        CHECK(kAchievements[i].coins > 0 && *kAchievements[i].name && *kAchievements[i].description);
    }
    CHECK(findAchievement("ruby") && findAchievement("ruby")->coins == 60 && !findAchievement("nope"));
    // The profile pays each achievement exactly once and remembers it.
    Profile p;
    int before = p.coins;
    CHECK(p.unlock("first_block") == 10 && p.coins == before + 10);
    CHECK(p.unlock("first_block") == 0 && p.coins == before + 10);
    CHECK(p.unlock("unknown") == 0);
    fs::path file = fs::temp_directory_path() / "vc_ach_profile.json";
    p.blocksMined = 123;
    p.save(file.string());
    Profile q = Profile::load(file.string());
    CHECK(q.achievements.count("first_block") && q.blocksMined == 123);
    // A survival session reports items in the inventory; creative never does.
    for (GameMode mode : {GameMode::Survival, GameMode::Creative}) {
        auto s = flatSession(fs::temp_directory_path() / "vc_ach_test", mode);
        std::set<std::string> got;
        s->onAchievement = [&](const std::string& id) { got.insert(id); };
        Settings settings;
        settings.renderDistance = 2;
        double t = 0;
        s->inventory().clear();
        s->inventory().slots[3] = {RUBY, 1};
        s->inventory().slots[4] = {CRAFTING_TABLE, 1};
        for (int i = 0; i < 90; ++i) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
        if (mode == GameMode::Survival) CHECK(got.count("ruby") && got.count("crafting_table") && !got.count("steel_ingot"));
        else CHECK(got.empty());
    }
    std::printf("achievements: %d goals\n", kAchievementCount);
    return 0;
}

static int testLanGame() {
    namespace fs = std::filesystem;
    fs::path rootA = fs::temp_directory_path() / "vc_lan_host", rootB = fs::temp_directory_path() / "vc_lan_client";
    auto a = flatSession(rootA, GameMode::Survival);
    Settings settings;
    settings.renderDistance = 2;
    double t = 0;
    for (int i = 0; i < 30; ++i) a->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    // A change made before the world is opened travels with the chunk files.
    CHECK(a->currentWorld().setBlock(5, 4, 5, RUBY_BLOCK));
    std::vector<uint8_t> skin(64 * 64 * 4, 200);
    Multiplayer host, client;
    std::string err;
    CHECK(host.host(*a, "LAN svět", "Hostitel", skin, false, err));
    CHECK(host.port() >= net::kGamePort);
    host.setRules(false, false);  // the spawn is not protected here: building near it is tested below
    net::Discovery discovery;
    bool discovering = discovery.start();
    CHECK(client.join("127.0.0.1", host.port(), "Host2", skin, true, err));
    std::unique_ptr<Session> b;
    auto pump = [&](float seconds, const std::function<bool()>& done) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) {
            t += 1 / 60.0;
            a->update(1 / 60.0f, t, 600, 270, settings);
            host.update(a.get(), 1 / 60.0f, t);
            if (b) b->update(1 / 60.0f, t, 600, 270, settings);
            client.update(b.get(), 1 / 60.0f, t);
            discovery.poll(t);
            if (done && done()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    };
    CHECK(pump(5, [&] { return client.joinState() == Multiplayer::JoinState::Ready; }));
    CHECK(client.welcome().world == "LAN svět" && client.welcome().host == "Hostitel");
    CHECK(client.welcome().seed == a->info().seed && client.welcome().mode == GameMode::Survival);
    CHECK(client.chunksReceived() > 0 && !client.welcome().generator.empty());
    if (discovering) {
        pump(3, [&] { return !discovery.games().empty() && discovery.games()[0].players == 2; });
        CHECK(!discovery.games().empty() && discovery.games()[0].world == "LAN svět" && discovery.games()[0].players == 2);
    } else {
        std::printf("lan: discovery port busy, skipped\n");
    }
    // The client builds its copy of the world from what it downloaded.
    fs::remove_all(rootB);
    WorldInfo info;
    info.folder = createWorldFolder(rootB.string());
    info.seed = client.welcome().seed;
    info.mode = client.welcome().mode;
    client.writeWorld(info.folder);
    b = std::make_unique<Session>(info, PlayerState{});
    pump(0.5f, nullptr);
    CHECK(b->currentWorld().getBlock(5, 4, 5) == RUBY_BLOCK);
    CHECK(b->currentWorld().getBlock(9, 2, 9) == a->currentWorld().getBlock(9, 2, 9));
    // Blocks both ways, also into chunks the other side has not loaded.
    CHECK(a->currentWorld().setBlock(6, 4, 6, STONE_BRICKS));
    CHECK(b->currentWorld().setBlock(7, 4, 7, BOOKSHELF));
    a->currentWorld().setBlock(8, 4, 8, AIR);
    CHECK(pump(2, [&] {
        return b->currentWorld().getBlock(6, 4, 6) == STONE_BRICKS && a->currentWorld().getBlock(7, 4, 7) == BOOKSHELF;
    }));
    b->applyRemoteBlock(0, 900, 10, 900, GLASS);  // far away: the chunk is loaded for it
    CHECK(b->currentWorld().getBlock(900, 10, 900) == GLASS);
    // Each sees the other player where they stand.
    b->player().pos = Vec3(3.5f, 4, 12.5f);
    CHECK(pump(2, [&] {
        return a->remotePlayers.size() == 1 && b->remotePlayers.size() == 1 &&
               (a->remotePlayers[0].pose.feet - Vec3(3.5f, 4, 12.5f)).length() < 0.3f;
    }));
    CHECK(a->remotePlayers[0].name == "Host2" && a->remotePlayers[0].pose.slim && a->remotePlayers[0].skinSlot == 0);
    CHECK(b->remotePlayers[0].name == "Hostitel" && host.playerCount() == 2 && client.playerCount() == 2);
    std::vector<std::string> joined = host.takeMessages();
    CHECK(joined.size() == 1 && joined[0] == "+Host2");
    // Chat both ways.
    client.takeMessages();
    client.sendChat("ahoj");
    host.sendChat("vítej");
    std::vector<std::string> hostGot, clientGot;
    CHECK(pump(2, [&] {
        for (auto& m : host.takeMessages()) hostGot.push_back(m);
        for (auto& m : client.takeMessages()) clientGot.push_back(m);
        return !hostGot.empty() && !clientGot.empty();
    }));
    CHECK(hostGot[0] == "#<Host2> ahoj" && clientGot[0] == "#<Hostitel> vítej");
    // 1.3.0: everyone sees the host's sky.
    a->setTimeOfDay(13000);
    a->setWeather(true);
    CHECK(pump(3, [&] {
        double d = std::fmod(b->timeOfDay(), 24000.0);
        return std::fabs(d - 13000) < 200 && b->raining();
    }));
    // 1.4.0: the host's rules, spawn protection, shared mobs, chests and PvP.
    host.setRules(true, true);
    CHECK(pump(2, [&] { return client.rules().pvp && client.rules().protectSpawn; }));
    CHECK(b->canEdit && !b->canEdit(8, 4, 9) && b->canEdit(300, 4, 300) && !a->canEdit);
    const ItemId underSpawn = a->currentWorld().getBlock(9, 3, 9);
    b->currentWorld().setBlock(9, 3, 9, GLASS);  // a guest's edit by the spawn is put back
    CHECK(pump(2, [&] { return b->currentWorld().getBlock(9, 3, 9) == underSpawn; }));
    CHECK(a->currentWorld().getBlock(9, 3, 9) == underSpawn);
    a->mobs().mobs().clear();
    a->mobs().spawn(MobType::Pig, 0, Vec3(14.5f, 4, 14.5f));
    const uint32_t pigUid = a->mobs().mobs().back().uid;
    CHECK(pump(2, [&] { return b->mobs().findUid(pigUid) >= 0; }));
    CHECK(b->mobsFromHost && b->onHitRemoteMob);
    b->inventory().clear();
    b->onHitRemoteMob(pigUid, 100);
    CHECK(pump(4, [&] { return a->mobs().findUid(pigUid) < 0 && countInInventory(b->inventory(), RAW_PORK) > 0; }));
    CHECK(pump(1, [&] { return b->mobs().findUid(pigUid) < 0; }));
    const FurnaceKey box{0, 14, 4, 10};
    a->currentWorld().setBlock(14, 4, 10, CHEST);
    ChestContents gifts{};
    gifts[0] = {RUBY, 3, 0};
    a->setChest(box, gifts);
    client.setOpenContainer(0, box);
    CHECK(pump(2, [&] { return b->chest(box) && (*b->chest(box))[0].id == RUBY; }));
    (*b->chest(box))[1] = {STICK, 5, 0};  // the guest puts sticks in
    CHECK(pump(2, [&] { return (*a->chest(box))[1].id == STICK && (*a->chest(box))[1].count == 5; }));
    client.setOpenContainer(-1, box);
    const float hostHealth = a->stats().health;
    CHECK(b->onHitPlayer);
    b->onHitPlayer(0, 4);
    CHECK(pump(2, [&] { return a->stats().health < hostHealth; }));
    host.setRules(false, true);
    CHECK(pump(2, [&] { return !client.rules().pvp && !b->pvp; }));
    // Leaving.
    client.stop();
    CHECK(pump(2, [&] { return host.playerCount() == 1; }));
    CHECK(a->remotePlayers.empty());
    // The host closing the world tells the players.
    Multiplayer again;
    CHECK(again.join("127.0.0.1", host.port(), "Host3", skin, false, err));
    for (int i = 0; i < 300 && again.joinState() != Multiplayer::JoinState::Ready; ++i) {
        host.update(a.get(), 1 / 60.0f, t += 1 / 60.0);
        again.update(nullptr, 1 / 60.0f, t);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(again.joinState() == Multiplayer::JoinState::Ready);
    host.stop();
    for (int i = 0; i < 200 && again.joinState() != Multiplayer::JoinState::Failed; ++i) {
        again.update(nullptr, 1 / 60.0f, t += 1 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(again.joinState() == Multiplayer::JoinState::Failed && again.error() == "Hostitel zavřel svět");
    // A wrong address fails cleanly.
    Multiplayer nobody;
    CHECK(!nobody.join("neexistuje.invalid", 1, "x", skin, false, err) || true);
    std::printf("lan: %d chunk files, port %d\n", client.chunksReceived(), host.port());
    return 0;
}

static int countItem(const Inventory& inv, uint8_t id) {
    int n = 0;
    for (const ItemStack& st : inv.slots) n += st.id == id ? st.count : 0;
    return n;
}

static int testCommands() {
    namespace fs = std::filesystem;
    auto s = flatSession(fs::temp_directory_path() / "vc_cmd_test", GameMode::Survival);
    Settings settings;
    settings.renderDistance = 2;
    double t = 0;
    for (int i = 0; i < 30; ++i) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    auto run = [&](const char* line, bool cheats) { return runCommand(*s, line, cheats); };
    // Without cheats only the harmless ones work.
    CHECK(run("/help", false).size() == 6);
    CHECK(run("/seed", false)[0] == "Seed: 9");
    CHECK(run("/gamemode creative", false)[0].find("not allowed") != std::string::npos);
    CHECK(s->survival());
    CHECK(run("/nothing", true)[0] == "Unknown command: /nothing. Type /help");
    CHECK(run("hello", true)[0] == "Unknown command. Type /help");
    // With cheats.
    CHECK(run("/help", true).size() == 1 + (size_t)kCommandCount);
    CHECK(run("/gm c", true)[0] == "Game mode: Creative" && !s->survival());
    CHECK(run("/gamemode survival", true)[0] == "Game mode: Survival" && s->survival());
    CHECK(run("/gamemode banana", true)[0].rfind("Usage:", 0) == 0);
    CHECK(run("/tp 100 20 -40", true)[0] == "Teleported to 100.5 20.0 -39.5");
    CHECK(std::fabs(s->player().pos.x - 100.5f) < 0.01f && std::fabs(s->player().pos.z + 39.5f) < 0.01f);
    run("/tp ~5 ~ ~-2", true);
    CHECK(std::fabs(s->player().pos.x - 105.5f) < 0.01f && std::fabs(s->player().pos.z + 41.5f) < 0.01f);
    CHECK(run("/tp a b", true)[0].rfind("Usage:", 0) == 0);
    CHECK(run("/tp Nikdo", true)[0] == "Player Nikdo is not here");
    s->inventory().clear();
    CHECK(run("/give ruby 5", true)[0] == "You got 5x Ruby");
    CHECK(countItem(s->inventory(), RUBY) == 5);
    run("/give minecraft:stone 100", true);
    CHECK(countItem(s->inventory(), STONE) == 100);
    CHECK(run("/give unobtainium", true)[0] == "Unknown item: unobtainium");
    CHECK(run("/clear", true)[0] == "Inventory cleared" && countItem(s->inventory(), RUBY) == 0);
    run("/time noon", true);
    CHECK(std::fabs(std::fmod(s->timeOfDay(), 24000.0) - 6000) < 1);
    run("/time set 18000", true);
    CHECK(std::fabs(std::fmod(s->timeOfDay(), 24000.0) - 18000) < 1);
    CHECK(run("/time", true)[0].find("Midnight") != std::string::npos);
    CHECK(run("/weather rain", true).size() == 1 && s->raining());
    CHECK(run("/weather clear", true).size() == 1 && !s->raining());
    run("/spawn", true);
    CHECK((s->player().pos - s->stats().spawn).length() < 0.01f);
    CHECK(run("/heal", true)[0] == "Health and food restored");
    CHECK(run("/pos", true)[0].rfind("Position:", 0) == 0);
    CHECK(run("/kill", true).empty() && s->dead());
    std::printf("commands: %d\n", kCommandCount);
    return 0;
}

static int testUpdate11() {
    namespace fs = std::filesystem;
    GenConfig config = GenConfig::defaults();
    // The Nether: grassy meadows, and a fortress whose void portal frame lights with the void flint and steel.
    {
        World nether(4242, Dimension::Nether, config);
        int fx, fy, fz;
        CHECK(nether.nearestStructure(0, 0, fx, fy, fz));
        nether.ensureChunksAround(floorDiv(fx, kChunkSize), floorDiv(fz, kChunkSize), 3, 1000);
        int bricks = 0, chests = 0, frame = 0, grass = 0;
        for (int x = fx - 26; x <= fx + 26; ++x)
            for (int z = fz - 26; z <= fz + 26; ++z)
                for (int y = fy - 2; y < fy + 18; ++y) {
                    ItemId b = nether.getBlock(x, y, z);
                    bricks += b == NETHER_BRICK;
                    chests += b == LOOT_CHEST_FORTRESS;
                    frame += b == VOID_STONE;
                }
        for (auto& [k, c] : nether.chunks())
            for (ItemId b : c->blocks) grass += b == NETHER_GRASS;
        std::printf("fortress at %d %d %d: %d bricks, %d chests, %d frame blocks; nether grass %d\n", fx, fy, fz, bricks, chests,
                    frame, grass);
        CHECK(bricks > 2000 && chests >= 10 && frame >= 30 && grass > 0);
        bool lit = false;
        for (int dy = 0; dy < 6 && !lit; ++dy) lit = tryLightPortal(nether, fx, fy + dy, fz, true);
        int portal = 0;
        for (int x = fx - 5; x <= fx + 5; ++x)
            for (int y = fy; y < fy + 14; ++y) portal += nether.getBlock(x, y, fz) == VOID_PORTAL;
        CHECK(lit && portal == 7 * 9);
        // Fortress chests are rich and often hold the key to the Crimson Void.
        int keys = 0, items = 0;
        for (uint32_t seed = 1; seed <= 20; ++seed) {
            ChestContents c = generateLoot(LOOT_CHEST_FORTRESS, seed * 7919);
            for (const ItemStack& st : c) {
                items += !st.empty();
                keys += st.id == VOID_FLINT_AND_STEEL;
            }
        }
        std::printf("fortress loot: %d stacks in 20 chests, %d void flint and steel\n", items, keys);
        CHECK(items >= 20 * 6 && keys >= 8);
    }
    // The Crimson Void: three lands, eye trees, and the boss arena.
    {
        World v(4242, Dimension::Void, config);
        int seen[3] = {};
        for (int x = -2000; x <= 2000; x += 50)
            for (int z = -2000; z <= 2000; z += 50) ++seen[(int)v.voidBiomeAt(x, z)];
        std::printf("void biomes: purple %d crimson %d lava %d\n", seen[0], seen[1], seen[2]);
        CHECK(seen[0] > 100 && seen[1] > 100 && seen[2] > 50);
        int ax, ay, az;
        CHECK(v.nearestStructure(0, 0, ax, ay, az));
        v.ensureChunksAround(floorDiv(ax, kChunkSize), floorDiv(az, kChunkSize), 2, 1000);
        CHECK(v.getBlock(ax + 3, ay - 1, az + 3) == VOID_STONE_BRICKS && v.getBlock(ax, ay + 3, az + 5) == AIR);
        CHECK(v.placeName(ax, az) == "Lávová pustina");
        int px = 0, pz = 0;
        for (int r = 0; r < 3000 && v.voidBiomeAt(px, pz) != World::VoidBiome::Purple; r += 40) px = r;
        v.ensureChunksAround(floorDiv(px, kChunkSize), 0, 3, 1000);
        int eyes = 0, purple = 0;
        for (auto& [k, c] : v.chunks())
            for (ItemId b : c->blocks) eyes += b == EYE_LOG, purple += b == VOID_GRASS;
        CHECK(eyes > 0 && purple > 1000);
    }
    // Animals, monsters, the boss, beds, food and weather in a flat survival world.
    auto s = flatSession(fs::temp_directory_path() / "vc_update11_test", GameMode::Survival);
    Settings settings;
    settings.renderDistance = 2;
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    auto tap = [&] {
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
    };
    run(0.5f);
    World& w = s->currentWorld();
    Player& p = s->player();
    p.pos = Vec3(8.5f, 4, 8.5f);
    p.yaw = 0;
    p.pitch = 0;
    s->mobs().mobs().clear();
    s->inventory().clear();
    s->inventory().slots[0] = {STEEL_AXE, 1};
    s->selectedSlot() = 0;
    // A pig in front: a few axe hits and it drops pork.
    s->mobs().spawn(MobType::Pig, 0, Vec3(8.5f, 4, 6.5f));
    p.pitch = -0.55f;  // at the pig's back
    for (int i = 0; i < 6 && !s->mobs().mobs().empty() && s->mobs().mobs()[0].alive(); ++i) {
        s->mobs().mobs()[0].pos = Vec3(8.5f, 4, 6.5f);
        s->mobs().mobs()[0].vel = {};
        tap();
        run(0.4f);
    }
    run(1.5f);
    for (const auto& d : s->drops().drops()) p.pos = Vec3(d.pos.x, 4, d.pos.z);  // walk over to the meat
    run(1.5f);
    int pork = 0;
    for (const ItemStack& st : s->inventory().slots) pork += st.id == RAW_PORK ? st.count : 0;
    std::printf("mobs: pig killed, %d raw pork picked up (mobs left %zu, health %.1f, drops %zu)\n", pork, s->mobs().mobs().size(),
                s->mobs().mobs().empty() ? 0.0f : s->mobs().mobs()[0].health, s->drops().drops().size());
    CHECK(s->mobs().count(0, false, Vec3(8.5f, 4, 6.5f), 3) == 0 && pork >= 1);
    // Eating it fills the hunger bar.
    s->inventory().slots[1] = {COOKED_PORK, 2};
    s->selectedSlot() = 1;
    float before = s->stats().food;
    p.pitch = 1.2f;  // at the sky: nothing to place on
    p.pitch = -1.5f;
    const_cast<PlayerState&>(s->stats()).food = 6;
    tap();
    CHECK(s->stats().food >= 13 && s->inventory().slots[1].count == 1);
    (void)before;
    // A forgotten man at night hurts the player; killing one counts as a hunt.
    s->setTimeOfDay(18000);
    s->mobs().spawn(MobType::ForgottenMan, 0, Vec3(10.5f, 4, 8.5f));
    float health = s->stats().health;
    run(3);
    std::printf("mobs: forgotten man, health %.0f -> %.0f\n", health, s->stats().health);
    CHECK(s->stats().health < health);
    s->mobs().mobs().clear();
    // A bed: placed as two halves; sleeping at night skips to the morning and sets the spawn.
    s->inventory().slots[2] = {BED_ITEM, 1};
    s->selectedSlot() = 2;
    p.pos = Vec3(8.5f, 4, 8.5f);
    p.yaw = 0;
    p.pitch = -0.9f;
    run(0.1f);
    tap();
    int bedZ = 0;
    for (int z = 3; z < 9 && !bedZ; ++z)
        if (isBed(w.getBlock(8, 4, z))) bedZ = z;
    CHECK(bedZ && isBed(w.getBlock(8, 4, bedZ + 1)) && s->inventory().slots[2].empty());
    double night = s->timeOfDay();
    tap();  // tapping the bed
    CHECK(s->sleeping());
    run(3);
    CHECK(!s->sleeping() && s->timeOfDay() > night && dayPhase((float)std::fmod(s->timeOfDay(), 24000.0)) == DayPhase::Morning);
    CHECK(std::fabs(s->stats().spawn.z - (bedZ + 0.5f)) < 1.6f);
    // Weather: rain over the meadow, then a clear sky again.
    s->setWeather(true);
    run(4);
    std::vector<Vertex> rain;
    buildPrecipitation(w, p.eye(), s->precipitation(), 1.0f, 1.0f, 1.0f, rain);
    CHECK(s->precipitation() == Precipitation::Rain && !rain.empty());
    SkyState noon = computeSky(6000, config.overworld.sky, 0), midnight = computeSky(18000, config.overworld.sky, 0);
    CHECK(noon.light > 0.9f && midnight.light < 0.3f && midnight.stars > 0.9f && noon.stars < 0.1f);
    CHECK(dayPhase(500) == DayPhase::Morning && dayPhase(6000) == DayPhase::Day && dayPhase(12000) == DayPhase::Evening &&
          dayPhase(15000) == DayPhase::Night && dayPhase(18000) == DayPhase::Midnight);
    // The boss: strong, hits hard, and leaves a treasure behind.
    {
        MobSystem ms;
        World& ground = s->currentWorld();
        ms.spawn(MobType::CrimsonMan, 0, Vec3(8.5f, 4, 4.5f));
        std::vector<MobEvent> ev;
        float dmg = 0;
        for (int i = 0; i < 400; ++i) {
            ms.update(ground, 0, Vec3(8.5f, 4, 8.5f), true, 1 / 60.0f, ev);
            for (const MobEvent& e : ev) dmg += e.kind == MobEvent::HitPlayer ? e.damage : 0;
            ev.clear();
        }
        CHECK(dmg >= 9);
        int hits = 0;
        while (!ms.hurt(0, 7, Vec3(8.5f, 4, 8.5f))) ++hits;
        for (int i = 0; i < 80; ++i) ms.update(ground, 0, Vec3(8.5f, 4, 8.5f), true, 1 / 60.0f, ev);
        bool died = false;
        for (const MobEvent& e : ev) died |= e.kind == MobEvent::Died && e.type == MobType::CrimsonMan;
        std::printf("boss: took %d hits, dealt %.0f damage\n", hits + 1, dmg);
        CHECK(hits >= 40 && died && ms.mobs().empty() && !mobDrops(MobType::CrimsonMan, 0.5f).empty());
    }
    // Animals are kept with the world, monsters are not.
    s->mobs().mobs().clear();
    s->mobs().spawn(MobType::Sheep, 0, Vec3(3.5f, 4, 3.5f));
    s->mobs().spawn(MobType::PumpkinSlime, 0, Vec3(5.5f, 4, 3.5f));
    s->save();
    MobSystem loaded;
    loaded.load(s->info().folder);
    CHECK(loaded.mobs().size() == 1 && loaded.mobs()[0].type == MobType::Sheep);
    std::vector<Vertex> verts;
    s->mobs().buildVertices(0, 1.0f, verts);
    CHECK(verts.size() >= 36 * 3);
    return 0;
}

static int testUpdate12() {
    // The VoxelSoftworks intro comes before the title, and a tap skips it.
    {
        namespace fs = std::filesystem;
        fs::path root = fs::temp_directory_path() / "vc_splash_test";
        fs::remove_all(root);
        fs::create_directories(root);
        Game game(root.string());
        game.onSurfaceCreated();
        game.onResize(2400, 1080);
        std::vector<std::string> log;
        Ui::textLog = &log;
        double t = 1;
        auto drawn = [&](const std::string& s) { return std::find(log.begin(), log.end(), s) != log.end(); };
        for (int i = 0; i < 40; ++i) game.frame(t += 1 / 30.0);
        CHECK(drawn("VOXELSOFTWORKS") && !drawn("Play"));
        game.onTouch(0, TouchAction::Down, 1200, 540, t);
        game.onTouch(0, TouchAction::Up, 1200, 540, t);
        for (int i = 0; i < 30; ++i) game.frame(t += 1 / 30.0);
        log.clear();
        game.frame(t += 1 / 30.0);
        CHECK(drawn("Play") && !drawn("VOXELSOFTWORKS"));
        Ui::textLog = nullptr;
    }

    // Powerstone: a block feeds a cable that lights a lamp; without it the lamp goes out.
    {
        World w(77, Dimension::Overworld);
        w.ensureChunksAround(0, 0, 1, 1000);
        const int y = 110;
        std::vector<std::array<int, 3>> ignite;
        for (int x = 1; x <= 3; ++x) w.setBlock(x, y, 0, POWER_CABLE);
        w.setBlock(4, y, 0, POWERSTONE_LAMP);
        w.setBlock(0, y, 0, POWERSTONE_BLOCK);
        updatePower(w, 0, y, 0, ignite);
        CHECK(w.getBlock(2, y, 0) == POWER_CABLE_ON && w.getBlock(4, y, 0) == POWERSTONE_LAMP_ON);
        w.setBlock(0, y, 0, AIR);
        updatePower(w, 0, y, 0, ignite);
        CHECK(w.getBlock(2, y, 0) == POWER_CABLE && w.getBlock(4, y, 0) == POWERSTONE_LAMP);
        // TNT next to a powered cable is listed to be lit.
        w.setBlock(4, y, 0, TNT);
        w.setBlock(0, y, 0, POWERSTONE_BLOCK);
        updatePower(w, 0, y, 0, ignite);
        CHECK(ignite.size() == 1 && ignite[0] == (std::array<int, 3>{4, y, 0}));
        // A sapling grows into a tree; leaves cut off from the trunk are not held.
        w.setBlock(8, y - 1, 8, GRASS);
        w.setBlock(8, y, 8, OAK_SAPLING);
        CHECK(saplingCanGrowOn(OAK_SAPLING, GRASS) && !saplingCanGrowOn(OAK_SAPLING, SAND));
        CHECK(growTree(w, 8, y, 8, treeForSapling(OAK_SAPLING), 5));
        CHECK(isLog(w.getBlock(8, y, 8)) && isLog(w.getBlock(8, y + 5, 8)) && isLeaves(w.getBlock(8, y + 7, 8)));
        CHECK(leafSupported(w, 9, y + 4, 8));
        for (int k = 0; k < 8; ++k)
            if (isLog(w.getBlock(8, y + k, 8))) w.setBlock(8, y + k, 8, AIR);
        CHECK(!leafSupported(w, 9, y + 4, 8));
        CHECK(treeForSapling(NETHERPERK_SAPLING) == TreeKind::Netherperk && saplingCanGrowOn(NETHERPERK_SAPLING, NETHER_GRASS));
    }
    // The bottom of every world cannot be broken.
    {
        World nether(5, Dimension::Nether), v(5, Dimension::Void);
        nether.ensureChunksAround(0, 0, 0, 1000);
        v.ensureChunksAround(0, 0, 0, 1000);
        CHECK(nether.getBlock(3, 0, 3) == WORLD_END && v.getBlock(3, 0, 3) == WORLD_END && breakTime(WORLD_END, RUBY_PICKAXE) < 0);
    }
    // Void chests often hold the compass to the arena.
    {
        int compasses = 0;
        for (uint32_t seed = 1; seed <= 40; ++seed)
            for (const ItemStack& st : generateLoot(LOOT_CHEST_VOID, seed * 104729)) compasses += st.id == BOSS_COMPASS;
        std::printf("void loot: %d arena compasses in 40 chests\n", compasses);
        CHECK(compasses >= 8);
    }
    // TNT blows a hole.
    {
        auto s = flatSession(std::filesystem::temp_directory_path() / "vc_update12_test", GameMode::Creative);
        World& w = s->currentWorld();
        w.ensureChunksAround(0, 0, 2, 1000);
        for (int x = 20; x < 27; ++x)
            for (int y = 60; y < 67; ++y)
                for (int z = 0; z < 7; ++z) w.setBlock(x, y, z, STONE);
        s->explodeAt(Vec3(23.5f, 63.5f, 3.5f), 3);
        int left = 0;
        for (int x = 20; x < 27; ++x)
            for (int y = 60; y < 67; ++y)
                for (int z = 0; z < 7; ++z) left += w.getBlock(x, y, z) == STONE;
        std::printf("explosion left %d of 343 stone blocks\n", left);
        CHECK(left < 300 && w.getBlock(23, 63, 3) == AIR);
    }
    // Store items, daily rewards, the wheel and codes.
    {
        bool vip = false, winter = false, halloween = false;
        for (const StoreItem& it : storeCatalog()) {
            vip |= it.id == "vip" && it.price == 3000;
            winter |= it.id == "addon:winter" && it.price == 850;
            halloween |= it.id == "addon:halloween" && it.price == 950;
        }
        CHECK(vip && winter && halloween);
        Profile p;
        p.coins = 0;
        const long long now = 1800000000;
        CHECK(nextRewardDay(p, 20261025) == 1);
        claimDailyReward(p, 20261025, now, 1);
        CHECK(p.coins == 50 && nextRewardDay(p, 20261025) == 0 && claimDailyReward(p, 20261025, now, 1).empty());
        CHECK(nextRewardDay(p, 20261026) == 2);
        claimDailyReward(p, 20261026, now, 1);
        CHECK(p.coins == 125 && p.pendingItems.size() == 2);
        CHECK(nextRewardDay(p, 20261028) == 1);  // a missed day starts the track again
        // A week in a row (across the end of the month): day 7 gives an addon for 8 hours.
        Profile q;
        for (int d = 0; d < 7; ++d) claimDailyReward(q, d < 6 ? 20261026 + d : 20261101, now, 3);
        CHECK(q.rewardStreak == 7 && q.bonusSpins == 2 && activeAddons(q, now).size() == 1 && activeAddons(q, now + 9 * 3600).empty());
        CHECK(nextRewardDay(q, 20261102) == 1);
        // VIP doubles the coins and adds a spin.
        Profile v;
        v.coins = 0;
        v.owned.insert("vip");
        claimDailyReward(v, 20261025, now, 1);
        CHECK(v.coins == 100 && spinsAvailable(v, 20261025, now) == 2 && isVip(v, now));
        // Spins: one a day, then none until tomorrow.
        Profile w;
        std::string msg, code;
        CHECK(spinsAvailable(w, 20261025, now) == 1);
        int seg = spinWheel(w, 20261025, now, 99, msg, code);
        CHECK(seg >= 0 && seg < (int)wheelSegments().size() && !msg.empty());
        CHECK(spinsAvailable(w, 20261025, now) == 0 && spinWheel(w, 20261025, now, 5, msg, code) == -1);
        CHECK(spinsAvailable(w, 20261026, now) == 1);
        int prizes[(int)WheelPrize::Count] = {}, codes = 0;
        for (uint32_t i = 0; i < 2000; ++i) {
            Profile t;
            int k = spinWheel(t, 20261025, now, i * 2654435761u, msg, code);
            ++prizes[(int)wheelSegments()[k].prize];
            if (!code.empty()) codes += validCode(code);
        }
        std::printf("wheel: 10c %d, 500c %d, vip %d, codes %d\n", prizes[0], prizes[4], prizes[5], codes);
        CHECK(prizes[0] > prizes[4] && prizes[5] > 0 && codes == prizes[(int)WheelPrize::Code] && codes > 0);
        // Anniversary codes: checked, case and spaces do not matter, used once.
        std::string c = makeAnniversaryCode(1234);
        CHECK(validCode(c) && validCode(" " + std::string("vc1y") + c.substr(4) + " ") && !validCode("VC1Y-AAAA-AA"));
        Profile a;
        CHECK(!redeemCode(a, "nonsense", msg) && redeemCode(a, c, msg) && !redeemCode(a, c, msg));
        CHECK(activeAddons(a, now).count("anniversary"));
        a.addonsOff.insert("addon:anniversary");
        CHECK(activeAddons(a, now).empty());
        // Saved and loaded with the profile.
        std::string path = (std::filesystem::temp_directory_path() / "vc_rewards_profile.json").string();
        q.codes.push_back(c);
        CHECK(q.save(path));
        Profile r = Profile::load(path);
        CHECK(r.rewardStreak == 7 && r.lastRewardDay == 20261101 && r.bonusSpins == 2 && r.until == q.until &&
              r.codes == q.codes && r.pendingItems == q.pendingItems);
    }
    return 0;
}

static int testUpdate13() {
    namespace fs = std::filesystem;
    // Farming, animals, the bow, villagers and calm mode in a flat survival world.
    auto s = flatSession(fs::temp_directory_path() / "vc_update13_test", GameMode::Survival);
    Settings settings;
    settings.renderDistance = 2;
    double t = 0;
    auto run = [&](float seconds) {
        for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
    };
    auto tap = [&] {
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
    };
    run(0.5f);
    World& w = s->currentWorld();
    Player& p = s->player();
    p.pos = Vec3(8.5f, 4, 8.5f);
    p.yaw = 0;
    p.pitch = -0.9f;
    s->mobs().mobs().clear();
    s->inventory().clear();
    // The hoe makes farmland, seeds grow into ripe wheat.
    s->inventory().slots[0] = {HOE, 1};
    s->inventory().slots[1] = {WHEAT_SEEDS, 4};
    s->selectedSlot() = 0;
    run(0.1f);
    tap();
    int fz = 0;
    for (int z = 2; z < 9 && !fz; ++z)
        if (w.getBlock(8, 3, z) == FARMLAND) fz = z;
    CHECK(fz != 0 && s->inventory().slots[0].damage == 1);
    s->selectedSlot() = 1;
    tap();
    CHECK(w.getBlock(8, 4, fz) == WHEAT_SEEDS && s->inventory().slots[1].count == 3);
    w.setBlock(9, 4, 8, WHEAT_SEEDS);  // seeds never grow on grass: they cannot even be placed there
    run(40);
    CHECK(w.getBlock(8, 4, fz) == WHEAT_RIPE);
    CHECK(dropFor(WHEAT_RIPE, 0.5f) == WHEAT && dropFor(TALL_GRASS, 0.05f) == WHEAT_SEEDS && dropFor(FARMLAND, 0.5f) == DIRT);
    // Milking a cow, and a villager offers trades.
    for (int z = 2; z < 9; ++z) w.setBlock(8, 4, z, AIR);  // nothing in the way
    p.pitch = -0.3f;  // the cow is lower than the eyes
    s->mobs().spawn(MobType::Cow, 0, Vec3(8.5f, 4, 6.2f));
    s->mobs().mobs().back().yaw = 0;
    s->inventory().slots[2] = {BUCKET, 1};
    s->selectedSlot() = 2;
    tap();
    CHECK(s->inventory().slots[2].id == MILK_BUCKET);
    s->mobs().mobs().clear();
    s->mobs().spawnVillager(0, Vec3(8.5f, 4, 6.2f), Profession::Smith, Vec3(8.5f, 4, 6.2f));
    tap();
    CHECK(s->wantsTrade == 0);
    s->wantsTrade = -1;
    // The bow: hold to draw, let go to shoot; the arrow hurts a pig.
    s->mobs().mobs().clear();
    s->mobs().spawn(MobType::Pig, 0, Vec3(8.5f, 4, 3.5f));
    s->inventory().slots[3] = {BOW, 1};
    s->inventory().slots[4] = {ARROW, 5};
    s->selectedSlot() = 3;
    p.pitch = -0.25f;
    s->touchDown(1, 300, 135, t);
    run(1.0f);
    CHECK(s->bowCharge() > 0.8f);
    s->touchUp(1, 300, 135, t);
    CHECK(s->arrows().size() == 1 && s->inventory().slots[4].count == 4);
    run(1.0f);
    CHECK(s->mobs().mobs().empty() || s->mobs().mobs()[0].health < mobInfo(MobType::Pig).health);
    // Calm mode: no monsters.
    settings.calmMode = true;
    s->mobs().spawn(MobType::ForgottenMan, 0, Vec3(8.5f, 4, 2.5f));
    run(0.2f);
    for (const Mob& m : s->mobs().mobs()) CHECK(!mobInfo(m.type).hostile);
    settings.calmMode = false;
    // Animals and villagers are kept with the world, professions and homes too.
    s->mobs().spawn(MobType::Chicken, 0, Vec3(5.5f, 4, 5.5f));
    s->mobs().spawnVillager(0, Vec3(6.5f, 4, 6.5f), Profession::Trader, Vec3(6, 4, 7));
    std::vector<Vertex> verts;
    s->mobs().buildVertices(0, 1, verts);
    CHECK(!verts.empty());
    s->mobs().save(s->info().folder);
    MobSystem loaded;
    loaded.load(s->info().folder);
    bool trader = false, chicken = false;
    for (const Mob& m : loaded.mobs()) {
        trader |= m.type == MobType::Villager && m.profession == Profession::Trader && m.hasHome && std::fabs(m.home.z - 7) < 0.01f;
        chicken |= m.type == MobType::Chicken;
    }
    CHECK(trader && chicken);
    // Trades: paid only when there is enough.
    Inventory inv;
    inv.add(RUBY, 2);
    const Trade& sword = tradesFor(Profession::Smith)[2];
    CHECK(sword.result.id == STEEL_SWORD && !makeTrade(inv, sword) && countInInventory(inv, RUBY) == 2);
    inv.add(RUBY, 5);
    CHECK(makeTrade(inv, sword) && countInInventory(inv, RUBY) == 4 && countInInventory(inv, STEEL_SWORD) == 1);
    // Power cables follow their neighbours: a straight line, and a lone piece.
    {
        World cw(3, Dimension::Overworld);
        cw.ensureChunksAround(0, 0, 0, 10);
        Box boxes[10];
        for (int x = 2; x <= 4; ++x) cw.setBlock(x, 110, 5, POWER_CABLE);
        CHECK(blockBoxes(cw, 3, 110, 5, POWER_CABLE, false, boxes) == 3);  // centre, west, east
        CHECK(boxes[1].x0 == 0 && boxes[2].x1 == 1);
        cw.setBlock(8, 110, 8, POWER_CABLE);
        CHECK(blockBoxes(cw, 8, 110, 8, POWER_CABLE, false, boxes) == 2);
        cw.setBlock(3, 110, 6, POWERSTONE_LAMP);  // a T towards the lamp
        CHECK(blockBoxes(cw, 3, 110, 5, POWER_CABLE, false, boxes) == 4);
    }
    // Villages: houses, fields and a well near the found centre.
    {
        World vw(4242, Dimension::Overworld);
        int vx, vy, vz;
        CHECK(vw.nearestStructure(0, 0, vx, vy, vz));
        vw.ensureChunksAround(floorDiv(vx, 16), floorDiv(vz, 16), 2, 1000);
        int farmland = 0, doors = 0, water = 0, wheat = 0;
        for (int x = vx - 22; x <= vx + 22; ++x)
            for (int z = vz - 22; z <= vz + 22; ++z)
                for (int y = vy - 3; y < vy + 8; ++y) {
                    ItemId b = vw.getBlock(x, y, z);
                    farmland += b == FARMLAND;
                    doors += isDoor(b);
                    water += b == WATER;
                    wheat += isCrop(b);
                }
        std::printf("village at %d %d %d: farmland %d, doors %d, wheat %d, water %d\n", vx, vy, vz, farmland, doors, wheat, water);
        CHECK(farmland >= 60 && doors >= 6 && wheat >= 60 && water >= 9);
    }
    // Old worlds: chunks saved before 1.3.0 have soul sand, glowstone and quartz in those ids.
    {
        fs::path dir = fs::temp_directory_path() / "vc_legacy_chunks";
        fs::remove_all(dir);
        fs::create_directories(dir);
        std::vector<uint8_t> file = {'V', 'C', 'C', '1'};
        for (int i = 0; i < 128; ++i) file.insert(file.end(), {255, (uint8_t)(i % 2 ? 88 : 153)});
        std::ofstream((dir / "c.0.0.bin").string(), std::ios::binary).write((const char*)file.data(), (std::streamsize)file.size());
        World lw(1, Dimension::Nether);
        lw.setStorageDir(dir.string());
        lw.ensureChunksAround(0, 0, 0, 1);
        int cornerstone = 0, ore = 0;
        for (ItemId b : lw.getChunk(0, 0)->blocks) cornerstone += b == NETHERRACK, ore += b == POWERSTONE_ORE;
        CHECK(cornerstone == 16384 && ore == 16384);
    }
    return 0;
}

static int testUpdate14() {
    namespace fs = std::filesystem;
    // 16-bit ids: a block above 255 survives a save, and 1.3.0 (VCC2) chunks still load.
    {
        fs::path dir = fs::temp_directory_path() / "vc_wide_chunks";
        fs::remove_all(dir);
        fs::create_directories(dir);
        {
            World w(3, Dimension::Overworld);
            w.setStorageDir(dir.string());
            w.ensureChunksAround(0, 0, 0, 1);
            w.setBlock(2, 100, 3, (ItemId)4321);
            w.setBlock(3, 100, 3, STONE);
            w.saveModifiedChunks();
        }
        World w(3, Dimension::Overworld);
        w.setStorageDir(dir.string());
        w.ensureChunksAround(0, 0, 0, 1);
        CHECK(w.getBlock(2, 100, 3) == 4321 && w.getBlock(3, 100, 3) == STONE);

        fs::remove_all(dir);
        fs::create_directories(dir);
        std::vector<uint8_t> file = {'V', 'C', 'C', '2'};
        for (int i = 0; i < 128; ++i) file.insert(file.end(), {255, (uint8_t)(i % 2 ? BRICKS : 88)});
        std::ofstream((dir / "c.0.0.bin").string(), std::ios::binary).write((const char*)file.data(), (std::streamsize)file.size());
        World old(1, Dimension::Overworld);
        old.setStorageDir(dir.string());
        old.ensureChunksAround(0, 0, 0, 1);
        int bricks = 0, crop = 0;
        for (ItemId b : old.getChunk(0, 0)->blocks) bricks += b == BRICKS, crop += b == WHEAT_CROP2;
        std::printf("wide ids: 4321 kept, VCC2 bricks %d crops %d\n", bricks, crop);
        CHECK(bricks == 16384 && crop == 16384);
    }
    // Stairs and slabs: items, recipes, placing, a double slab and walking up a step.
    {
        CHECK(isPlaceable(stairsId(0, 0, false)) && isPlaceable(slabId(5, 0)) && !isPlaceable((ItemId)1096));
        CHECK(itemFromKey("minecraft:oak_stairs") == STAIRS && itemFromKey("stone_slab") == slabId(5, 0));
        CHECK(std::string(itemKey(stairsId(8, 3, true))) == "brick_stairs" && tileFor(slabId(8, 1), F_TOP) == T_BRICKS);
        CHECK(dropFor(stairsId(0, 2, true), 0.5f, WOOD_AXE) == STAIRS && dropFor(slabId(5, 2), 0.5f, WOOD_PICKAXE) == slabId(5, 0));
        CHECK(isOpaque(slabId(5, 2)) && !isOpaque(slabId(5, 0)) && isShaped(stairsId(1, 1, false)));
        ItemStack grid[9] = {};
        for (int i : {0, 3, 4, 6, 7, 8}) grid[i] = {PLANKS, 1};
        CraftResult stairs = matchRecipe(grid, 3);
        ItemStack row[9] = {};
        for (int i : {6, 7, 8}) row[i] = {COBBLESTONE, 1};
        CraftResult slabs = matchRecipe(row, 3);
        CHECK(stairs.id == STAIRS && stairs.count == 4 && slabs.id == slabId(4, 0) && slabs.count == 6);
        const Language before = language();
        setLanguage(Language::English);
        CHECK(std::string(tr(itemName(slabId(6, 0)))) == "Stone Brick Slab" && std::string(tr(itemName(STAIRS))) == "Oak Stairs");
        setLanguage(before);

        auto s = flatSession(fs::temp_directory_path() / "vc_update14_shapes", GameMode::Survival);
        Settings settings;
        settings.renderDistance = 2;
        double t = 0;
        auto run = [&](float seconds) {
            for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
        };
        auto tap = [&] {
            s->touchDown(1, 300, 135, t);
            run(1 / 60.0f);
            s->touchUp(1, 300, 135, t);
            run(1 / 60.0f);
        };
        run(0.5f);
        World& w = s->currentWorld();
        Player& p = s->player();
        s->mobs().mobs().clear();
        s->inventory().clear();
        p.pos = Vec3(8.5f, 4, 8.5f);
        p.yaw = 0;  // looking north
        p.pitch = -0.9f;
        s->inventory().slots[0] = {slabId(5, 0), 4};
        s->inventory().slots[1] = {STAIRS + 8 * 6, 4};  // stone brick stairs
        s->selectedSlot() = 0;
        run(0.1f);
        tap();
        int sz = 0;
        for (int z = 2; z < 9 && !sz; ++z)
            if (w.getBlock(8, 4, z) == slabId(5, 0)) sz = z;
        CHECK(sz != 0 && s->inventory().slots[0].count == 3);
        tap();  // a second slab on top of the first makes a double slab
        CHECK(w.getBlock(8, 4, sz) == slabId(5, 2) && s->inventory().slots[0].count == 2);
        w.setBlock(8, 4, sz, AIR);
        s->selectedSlot() = 1;
        run(0.1f);
        tap();
        ItemId placed = w.getBlock(8, 4, sz);
        std::printf("stairs: placed %d at z %d (facing %d)\n", (int)placed, sz, isStairs(placed) ? (placed - STAIRS) & 3 : -1);
        CHECK(placed == stairsId(6, 0, false));
        // Walking north into a bottom slab steps up onto it without jumping.
        for (int z = 1; z < 9; ++z) w.setBlock(8, 4, z, AIR);
        w.setBlock(8, 4, 6, slabId(5, 0));
        p.pos = Vec3(8.5f, 4, 8.5f);
        p.pitch = 0;
        float highest = 0;
        for (int i = 0; i < 90; ++i) {
            p.update(w, 1 / 60.0f, 0, 1, false);
            highest = std::max(highest, p.pos.y);
        }
        std::printf("step: highest y %.2f, ended at z %.2f\n", highest, p.pos.z);
        CHECK(highest > 4.45f && highest < 4.7f && p.pos.z < 6.0f);
    }
    // Content: items, recipes, the new mobs, tar, the map and Caves 2.0.
    {
        CHECK(isPlaceable(STAINED_GLASS + 3) && isTranslucent(STAINED_GLASS) && !isOpaque(STAINED_GLASS + 15));
        CHECK(isLiquid(TAR) && isPlaceable(TAR) && !isPlaceable(STRING) && !isPlaceable(MAP) && isSpawnEgg(EGG_TAR_MONSTER));
        CHECK(itemFromKey("minecraft:light_blue_stained_glass") == STAINED_GLASS + 3 && itemFromKey("string") == STRING);
        CHECK(dropFor(DRIPSTONE_DOWN, 0.5f, WOOD_PICKAXE) == DRIPSTONE_UP && tileFor(STAINED_GLASS + 14, F_TOP) == T_STAINED_GLASS + 14);
        ItemStack bow[9] = {};
        bow[1] = bow[3] = bow[7] = {STICK, 1};
        bow[2] = bow[5] = bow[8] = {STRING, 1};
        ItemStack glass[9];
        for (auto& g : glass) g = {GLASS, 1};
        glass[4] = {177, 1};  // red wool
        CHECK(matchRecipe(bow, 3).id == BOW && matchRecipe(glass, 3).id == STAINED_GLASS + 14 && matchRecipe(glass, 3).count == 8);
        bool stringDrop = false;
        for (auto [id, n] : mobDrops(MobType::Spider, 0.5f)) stringDrop |= id == STRING && n > 0;
        CHECK(stringDrop && mobInfo(MobType::TarMonster).hostile && !mobInfo(MobType::Bat).hostile);

        auto s = flatSession(fs::temp_directory_path() / "vc_update14_mobs", GameMode::Survival);
        Settings settings;
        settings.renderDistance = 2;
        double t = 0;
        auto run = [&](float seconds) {
            for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
        };
        run(0.5f);
        World& w = s->currentWorld();
        Player& p = s->player();
        s->mobs().mobs().clear();
        p.pos = Vec3(8.5f, 4, 8.5f);
        // The TNT zombie walks up, stops, hisses and blows a hole into the ground.
        s->mobs().spawn(MobType::TntZombie, 0, Vec3(8.5f, 4, 13.5f));
        float fuseSeen = 0;
        auto zombies = [&] { return s->mobs().countType(0, MobType::TntZombie, Vec3(8, 4, 8), 40); };
        for (int i = 0; i < 60 * 6 && zombies() > 0; ++i) {
            run(1 / 60.0f);
            p.pos = Vec3(8.5f, 4, 8.5f);
            for (const Mob& m : s->mobs().mobs()) fuseSeen = std::max(fuseSeen, m.fuse);
        }
        int holes = 0;
        for (int x = 4; x < 14; ++x)
            for (int z = 5; z < 16; ++z) holes += w.getBlock(x, 3, z) == AIR;
        std::printf("tnt zombie: fuse %.2f, %d grass blocks blown away, health %.1f\n", fuseSeen, holes, s->stats().health);
        CHECK(zombies() == 0 && fuseSeen > 1.0f && holes > 5 && s->stats().health < 20);
        // The tar monster swallows a pig: tar spreads there and a second monster rises.
        for (int x = 0; x < 24; ++x)
            for (int z = 0; z < 24; ++z) {
                w.setBlock(x, 3, z, GRASS);
                for (int y = 4; y < 8; ++y) w.setBlock(x, y, z, AIR);
            }
        s->mobs().mobs().clear();
        p.pos = Vec3(2.5f, 4, 2.5f);
        s->setMode(GameMode::Creative);  // the player is not the prey here
        s->mobs().spawn(MobType::TarMonster, 0, Vec3(16.5f, 4, 16.5f));
        s->mobs().spawn(MobType::Pig, 0, Vec3(16.5f, 4, 19.5f));
        for (int i = 0; i < 60 * 12 && s->mobs().countType(0, MobType::TarMonster, Vec3(16, 4, 16), 20) < 2; ++i) {
            run(1 / 60.0f);
            p.pos = Vec3(2.5f, 4, 2.5f);
        }
        int tar = 0;
        for (int x = -16; x < 48; ++x)
            for (int z = -16; z < 48; ++z)
                for (int y = 3; y < 7; ++y) tar += w.getBlock(x, y, z) == TAR;
        const int monsters = s->mobs().countType(0, MobType::TarMonster, Vec3(16, 4, 16), 20);
        std::printf("tar monster: %d monsters, %d tar blocks\n", monsters, tar);
        CHECK(monsters == 2 && tar >= 3);
        // Tar is thick: walking through it is slow.
        Player probe;
        probe.pos = Vec3(17.5f, 4, 17.5f);
        for (int x = 14; x < 22; ++x)
            for (int z = 14; z < 22; ++z) w.setBlock(x, 4, z, TAR);
        probe.update(w, 1 / 60.0f, 0, 1, false);
        CHECK(probe.inTar && std::hypot(probe.vel.x, probe.vel.z) < 1.0f);
        // Bats fly and do not fall to the ground.
        s->mobs().mobs().clear();
        s->mobs().spawn(MobType::Bat, 0, Vec3(4.5f, 7, 4.5f));
        run(3);
        bool flying = false;
        for (const Mob& m : s->mobs().mobs()) flying |= m.type == MobType::Bat && m.pos.y > 4.6f;
        CHECK(flying);
        // The map draws where the player carries it, and shows when held.
        s->inventory().clear();
        s->inventory().slots[0] = {MAP, 1};
        s->selectedSlot() = 0;
        settings.showMap = false;
        run(1.5f);
        const int drawn = s->paperMaps().explored(0, 2, 2);
        std::printf("map: %d pixels drawn\n", drawn);
        CHECK(drawn > 500);
        s->save();
        PaperMaps reread;
        reread.load(s->info().folder);
        CHECK(reread.explored(0, 2, 2) == drawn);
    }
    // Caves 2.0: big caverns hold lakes of water and tar, and dripstone.
    {
        World cw(1234, Dimension::Overworld);
        cw.ensureChunksAround(0, 0, 8, 1000);
        int tarBlocks = 0, lakes = 0, spikes = 0;
        for (auto& [k, c] : cw.chunks())
            for (int x = 0; x < 16; ++x)
                for (int z = 0; z < 16; ++z)
                    for (int y = 1; y < 60; ++y) {
                        const ItemId b = c->get(x, y, z);
                        tarBlocks += b == TAR;
                        lakes += b == WATER && y <= kCaveLakeLevel;
                        spikes += isDripstoneSpike(b);
                    }
        std::printf("caves 2.0: tar %d, cave water %d, dripstone %d\n", tarBlocks, lakes, spikes);
        CHECK(tarBlocks > 50 && lakes > 50 && spikes > 50);
    }
    // VoxelCraft Mods from JSON and PNG, and a Java mod converted to one.
    {
        const fs::path root = fs::temp_directory_path() / "vc_mods_test";
        fs::remove_all(root);
        fs::create_directories(root / "ruby" / "textures" / "mod");
        fs::create_directories(root / "ruby" / "recipes");
        std::vector<uint8_t> red(16 * 16 * 4), green(16 * 16 * 4);
        for (size_t i = 0; i < red.size(); i += 4) {
            red[i] = 220; red[i + 3] = 255;
            green[i + 1] = 200; green[i + 3] = 255;
        }
        writePng((root / "ruby/textures/mod/ruby_bricks.png").string(), 16, 16, red);
        writePng((root / "ruby/textures/mod/ruby_apple.png").string(), 16, 16, green);
        std::ofstream(root / "ruby/content.json") << R"({"namespace": "ruby",
          "blocks": [{"id": "ruby_bricks", "name": "Ruby Bricks", "name_cs": "Rubínové cihly", "texture": "ruby_bricks.png",
                      "hardness": 2, "tool": "pickaxe", "light": true},
                     {"id": "glow_glass", "name": "Glow Glass", "texture": "ruby_bricks.png", "transparent": true, "drop": "ruby:ruby_apple"}],
          "items": [{"id": "ruby_apple", "name": "Ruby Apple", "texture": "ruby_apple.png", "food": 6, "stack": 16}],
          "mobs": [{"id": "ruby_pig", "name": "Ruby Pig", "base": "pig", "texture": "ruby_bricks.png", "health": 15,
                    "drops": [{"item": "ruby:ruby_apple", "count": 2}]}]})";
        std::ofstream(root / "ruby/recipes/ruby.json") << R"({"recipes": [
          {"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "ruby:ruby_apple"}, "result": "ruby:ruby_bricks"}]})";
        ModRegistry& mods = ModRegistry::get();
        std::vector<std::string> warnings;
        mods.load({(root / "ruby").string()}, (root / "mod_ids.txt").string(), &warnings);
        const ItemId bricks = itemFromKey("ruby:ruby_bricks"), apple = itemFromKey("ruby:ruby_apple"), glass = itemFromKey("ruby:glow_glass");
        std::printf("mods: bricks %d, glass %d, apple %d, %zu warnings\n", (int)bricks, (int)glass, (int)apple, warnings.size());
        CHECK(bricks >= kModIdBase && apple > bricks && warnings.empty() && mods.creative().size() == 3 && mods.mobs().size() == 1);
        CHECK(isPlaceable(bricks) && !isPlaceable(apple) && isOpaque(bricks) && !isOpaque(glass) && isEmissive(bricks));
        CHECK(std::string(itemKey(apple)) == "ruby:ruby_apple" && std::string(itemName(bricks)) == "Ruby Bricks");
        CHECK(isFood(apple) && maxStack(apple) == 16 && tileFor(bricks, F_TOP) >= T_COUNT && tileFor(apple, F_SOUTH) >= T_COUNT);
        CHECK(dropFor(glass, 0.5f) == apple && dropFor(bricks, 0.5f, WOOD_PICKAXE) == bricks && dropFor(bricks, 0.5f) == AIR);
        ItemStack grid[4] = {{apple, 1}, {apple, 1}, {apple, 1}, {apple, 1}};
        RecipeBook book;
        Json recipeDoc;
        std::string jerr;
        CHECK(Json::parse(R"({"recipes": [{"type": "shaped", "pattern": ["AA", "AA"], "key": {"A": "ruby:ruby_apple"}, "result": "ruby:ruby_bricks"}]})", recipeDoc, jerr));
        book.addJson(recipeDoc);
        setRecipes(book);
        CHECK(matchRecipe(grid, 2).id == bricks);
        setRecipes(RecipeBook::defaults());
        // The same ids again after a reload, even with another mod loaded first.
        fs::create_directories(root / "other");
        std::ofstream(root / "other/content.json") << R"({"namespace": "other", "items": [{"id": "thing"}]})";
        mods.load({(root / "other").string(), (root / "ruby").string()}, (root / "mod_ids.txt").string());
        CHECK(itemFromKey("ruby:ruby_bricks") == bricks && itemFromKey("ruby:ruby_apple") == apple && itemFromKey("other:thing") > apple);
        // A mod mob: a pig body in the mod's skin, with the mod's drops.
        auto ms = flatSession(root / "world", GameMode::Survival);
        ms->mobs().mobs().clear();
        ms->mobs().spawn(MobType::Pig, 0, Vec3(8.5f, 4, 8.5f));
        ms->mobs().mobs().back().variant = 1;
        ms->inventory().clear();
        ms->mobs().hurt(0, 100, Vec3(8.5f, 4, 6));
        Settings st;
        st.renderDistance = 2;
        double mt = 0;
        for (int i = 0; i < 120; ++i) ms->update(1 / 60.0f, mt += 1 / 60.0, 600, 270, st);
        ms->player().pos = Vec3(8.5f, 4, 8.5f);
        for (int i = 0; i < 120; ++i) ms->update(1 / 60.0f, mt += 1 / 60.0, 600, 270, st);
        std::printf("mod mob: %d ruby apples picked up\n", countInInventory(ms->inventory(), apple));
        CHECK(countInInventory(ms->inventory(), apple) == 2 && countInInventory(ms->inventory(), RAW_PORK) == 0);

        // A Java mod: a cube block, a column block, a flat item, a fence (skipped), lang names,
        // recipes (one with a tag), a block drop, Minecraft's own files and some classes.
        const fs::path java = root / "jmod" / "java";
        for (const char* d : {"assets/gems/blockstates", "assets/gems/models/block", "assets/gems/models/item", "assets/gems/textures/block",
                              "assets/gems/textures/item", "assets/gems/lang", "data/gems/recipe", "data/gems/loot_table/blocks",
                              "assets/minecraft/textures/block"})
            fs::create_directories(java / d);
        std::ofstream(java / "fabric.mod.json") << R"({"id": "gems", "name": "Gems Mod", "version": "2.1"})";
        std::ofstream(java / "classes.txt") << 42;
        std::ofstream(java / "assets/minecraft/textures/block/stone.png") << "not ours";
        writePng((java / "assets/gems/textures/block/topaz_block.png").string(), 16, 16, red);
        writePng((java / "assets/gems/textures/block/topaz_log.png").string(), 16, 16, green);
        writePng((java / "assets/gems/textures/block/topaz_log_top.png").string(), 16, 16, red);
        writePng((java / "assets/gems/textures/item/topaz.png").string(), 16, 16, green);
        std::ofstream(java / "assets/gems/blockstates/topaz_block.json") << R"({"variants": {"": {"model": "gems:block/topaz_block"}}})";
        std::ofstream(java / "assets/gems/blockstates/topaz_log.json") << R"({"variants": {"axis=y": {"model": "gems:block/topaz_log"}}})";
        std::ofstream(java / "assets/gems/blockstates/topaz_fence.json") << R"({"multipart": [{"apply": {"model": "gems:block/fence_post"}}]})";
        std::ofstream(java / "assets/gems/models/block/topaz_block.json") << R"({"parent": "minecraft:block/cube_all", "textures": {"all": "gems:block/topaz_block"}})";
        std::ofstream(java / "assets/gems/models/block/topaz_log.json") << R"({"parent": "block/cube_column", "textures": {"end": "gems:block/topaz_log_top", "side": "gems:block/topaz_log"}})";
        std::ofstream(java / "assets/gems/models/item/topaz.json") << R"({"parent": "minecraft:item/generated", "textures": {"layer0": "gems:item/topaz"}})";
        std::ofstream(java / "assets/gems/models/item/topaz_block.json") << R"({"parent": "gems:block/topaz_block"})";
        std::ofstream(java / "assets/gems/lang/en_us.json") << R"({"block.gems.topaz_block": "Block of Topaz", "item.gems.topaz": "Topaz"})";
        std::ofstream(java / "assets/gems/lang/cs_cz.json") << R"({"item.gems.topaz": "Topaz CZ"})";
        std::ofstream(java / "data/gems/recipe/topaz_block.json") << R"({"type": "minecraft:crafting_shaped", "pattern": ["TTT", "TTT", "TTT"],
            "key": {"T": {"item": "gems:topaz"}}, "result": {"id": "gems:topaz_block", "count": 1}})";
        std::ofstream(java / "data/gems/recipe/topaz.json") << R"({"type": "minecraft:crafting_shapeless", "ingredients": ["gems:topaz_block"],
            "result": {"item": "gems:topaz", "count": 9}})";
        std::ofstream(java / "data/gems/recipe/stick_from_logs.json") << R"({"type": "crafting_shapeless", "ingredients": [{"tag": "minecraft:logs"}],
            "result": {"item": "minecraft:stick", "count": 4}})";
        std::ofstream(java / "data/gems/recipe/smelt.json") << R"({"type": "minecraft:smelting", "ingredient": {"item": "gems:topaz_log"}, "result": "minecraft:coal"})";
        std::ofstream(java / "data/gems/loot_table/blocks/topaz_log.json") << R"({"pools": [{"entries": [{"type": "item", "name": "gems:topaz"}]}]})";
        JavaImportReport rep = convertJavaMod(java.string(), (root / "jmod").string());
        std::printf("java import '%s': %d blocks, %d items, %d recipes, %d textures, %d drops; skipped %d minecraft files, %d shapes, %d recipes, %d classes\n",
                    rep.name.c_str(), rep.blocks, rep.items, rep.recipes, rep.textures, rep.drops, rep.skippedMinecraft, rep.skippedShapes,
                    rep.skippedRecipes, rep.classes);
        CHECK(rep.name == "Gems Mod" && rep.blocks == 2 && rep.items == 1 && rep.recipes == 3 && rep.textures == 4 && rep.drops == 1);
        CHECK(rep.skippedMinecraft == 1 && rep.skippedShapes == 1 && rep.skippedRecipes == 1 && rep.classes == 42);
        CHECK(!fs::exists(root / "jmod/textures/mod/minecraft_block_stone.png") && readImportReport((root / "jmod").string()).size() >= 10);
        mods.load({(root / "jmod").string()}, (root / "mod_ids.txt").string());
        const ItemId topaz = itemFromKey("gems:topaz"), topazBlock = itemFromKey("gems:topaz_block"), log = itemFromKey("gems:topaz_log");
        CHECK(topaz != AIR && topazBlock != AIR && log != AIR && dropFor(log, 0.5f) == topaz);
        CHECK(std::string(itemName(topazBlock)) == "Block of Topaz" && tileFor(log, F_TOP) != tileFor(log, F_NORTH));
        RecipeBook jb;
        Json jr;
        CHECK(Json::parse([&] { std::ifstream f(root / "jmod/recipes/java_gems.json"); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }(), jr, jerr));
        jb.addJson(jr);
        setRecipes(jb);
        ItemStack nine[9];
        for (auto& g : nine) g = {topaz, 1};
        CHECK(matchRecipe(nine, 3).id == topazBlock && smeltResult(log) == COAL);
        setRecipes(RecipeBook::defaults());
        mods.clear();
    }
    // Switches and cables on walls: placed by tapping a side of a block, and they carry power.
    {
        auto ps = flatSession(fs::temp_directory_path() / "vc_wall_power", GameMode::Survival);
        Settings st;
        st.renderDistance = 2;
        double pt = 0;
        auto run = [&](float seconds) {
            for (float e = 0; e < seconds; e += 1 / 60.0f) ps->update(1 / 60.0f, pt += 1 / 60.0, 600, 270, st);
        };
        run(0.3f);
        World& w = ps->currentWorld();
        ps->mobs().mobs().clear();
        for (int x = 4; x < 13; ++x)
            for (int y = 4; y < 8; ++y) w.setBlock(x, y, 5, STONE);  // a wall to the north
        Player& p = ps->player();
        p.pos = Vec3(8.5f, 4, 8.5f);
        p.yaw = 0;
        p.pitch = 0.05f;
        ps->inventory().clear();
        ps->inventory().slots[0] = {SWITCH, 2};
        ps->inventory().slots[1] = {POWER_CABLE, 8};
        ps->selectedSlot() = 0;
        run(0.1f);
        ps->touchDown(1, 300, 135, pt);
        run(1 / 60.0f);
        ps->touchUp(1, 300, 135, pt);
        run(0.1f);
        const ItemId onWall = w.getBlock(8, 5, 6);
        std::printf("wall switch: %d (side %d)\n", (int)onWall, wallSide(onWall));
        CHECK(isWallSwitch(onWall) && wallSide(onWall) == 0 && !switchOn(onWall));
        // A cable up the wall from the switch to a lamp; flipping the switch lights it.
        w.setBlock(8, 6, 6, WALL_CABLE);
        w.setBlock(8, 7, 6, WALL_CABLE);
        w.setBlock(8, 8, 6, POWERSTONE_LAMP);
        w.setBlock(8, 5, 6, toggledSwitch(onWall));
        std::vector<std::array<int, 3>> ignite;
        updatePower(w, 8, 5, 6, ignite);
        CHECK(switchOn(w.getBlock(8, 5, 6)) && cableOn(w.getBlock(8, 7, 6)) && w.getBlock(8, 8, 6) == POWERSTONE_LAMP_ON);
        CHECK(dropFor(WALL_CABLE + 5, 0.5f) == POWER_CABLE && dropFor(WALL_SWITCH + 2, 0.5f) == SWITCH && lightLevel(WALL_CABLE + 4) > 0);
        Box boxes[10];
        CHECK(blockBoxes(w, 8, 7, 6, w.getBlock(8, 7, 6), false, boxes) >= 3 && boxes[0].z1 < 0.1f);
    }
    // Backups and .vcworld files.
    {
        const fs::path root = fs::temp_directory_path() / "vc_worldfiles_test";
        fs::remove_all(root);
        auto ws = flatSession(root / "worlds", GameMode::Survival);
        ws->currentWorld().setBlock(3, 4, 3, RUBY_BLOCK);
        ws->save();
        const std::string folder = ws->info().folder, backups = (root / "backups").string();
        std::string made;
        CHECK(backupWorld(folder, backups, 2, &made) && fs::exists(fs::path(made) / "level.dat"));
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        CHECK(backupWorld(folder, backups, 2));
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        CHECK(backupWorld(folder, backups, 2));
        CHECK(listBackups(folder, backups).size() == 2);
        // Something goes wrong in the world; the newest backup brings the ruby block back.
        ws->currentWorld().setBlock(3, 4, 3, TNT);
        ws->save();
        const std::string newest = listBackups(folder, backups)[0];
        ws.reset();
        CHECK(restoreBackup(folder, newest, backups));
        {
            WorldInfo info;
            PlayerState player;
            CHECK(loadLevel(folder, info, player));
            Session again(info, player);
            again.currentWorld().ensureChunksAround(0, 0, 0, 1);
            CHECK(again.currentWorld().getBlock(3, 4, 3) == RUBY_BLOCK);
        }
        std::string err, imported;
        const std::string file = (root / "export" / "svet.vcworld").string();
        CHECK(exportWorld(folder, file, err) && fs::file_size(file) > 100);
        CHECK(importWorld(file, (root / "worlds").string(), imported, err) && imported != folder);
        WorldInfo info;
        PlayerState player;
        CHECK(loadLevel(imported, info, player));
        Session copy(info, player);
        copy.currentWorld().ensureChunksAround(0, 0, 0, 1);
        CHECK(copy.currentWorld().getBlock(3, 4, 3) == RUBY_BLOCK);
        std::ofstream(root / "junk.vcworld") << "not a zip";
        CHECK(!importWorld((root / "junk.vcworld").string(), (root / "worlds").string(), imported, err) && !err.empty());
        std::printf("world files: export %d bytes, import ok\n", (int)fs::file_size(file));
    }
    // Room codes: an address and a port in ten letters, read back from any spelling.
    {
        const std::string code = net::roomCode("192.168.43.1", 19133);
        std::string ip;
        int port = 0;
        CHECK(code.size() == 14 && code.rfind("VC-", 0) == 0);
        CHECK(net::parseJoinAddress(code, ip, port) && ip == "192.168.43.1" && port == 19133);
        std::string lower = code;
        for (char& ch : lower) ch = (char)std::tolower((unsigned char)ch);
        CHECK(net::parseJoinAddress(" " + lower.substr(3) + " ", ip, port) && ip == "192.168.43.1" && port == 19133);
        CHECK(net::parseJoinAddress("10.0.0.7:2000", ip, port) && ip == "10.0.0.7" && port == 2000);
        CHECK(net::parseJoinAddress("10.0.0.7", ip, port) && port == net::kGamePort);
        CHECK(net::roomCode("1.2.3.4", 5) != net::roomCode("1.2.3.5", 5) && !net::parseJoinAddress("VC-AAAAA-AAAA0", ip, port));
        CHECK(net::classifyAddress("192.168.43.1", "wlan0") == net::AddressKind::Hotspot &&
              net::classifyAddress("192.168.1.20", "wlan0") == net::AddressKind::Wifi &&
              net::classifyAddress("10.120.4.9", "ap0") == net::AddressKind::Hotspot &&
              net::classifyAddress("85.70.1.2", "rmnet0") == net::AddressKind::Internet);
        std::printf("room code: %s\n", code.c_str());
    }
    // The Anniversary modpack: only with the addon on; the showcase builds once, with gifts.
    {
        CHECK(isAnniversaryItem(BALLOON + 2) && isAnniversaryItem(PARTY_POPPER) && !isAnniversaryItem(CAKE));
        CHECK(isPlaceable(ANNIV_LOGO) && isPlaceable(PAINTED_PLANKS + 3) && !isPlaceable(ANNIV_TOKEN) && isShaped(BALLOON));
        CHECK(isEmissive(PARTY_LANTERN) && itemFromKey("red_balloon") == BALLOON && std::string(itemKey(FESTIVE_BRICKS)) == "festive_bricks");
        auto s = flatSession(fs::temp_directory_path() / "vc_update14_anniversary", GameMode::Survival);
        Settings settings;
        settings.renderDistance = 3;
        double t = 0;
        std::vector<std::string> got;
        s->onAchievement = [&](const std::string& id) { got.push_back(id); };
        auto run = [&](float seconds) {
            for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
        };
        run(1);
        CHECK(!s->stats().hasShowcase);  // not without the addon
        settings.addonAnniversary = true;
        run(1);
        const PlayerState& st = s->stats();
        CHECK(st.hasShowcase);
        World& w = s->currentWorld();
        const int ox = st.showcaseX, oy = st.showcaseY, oz = st.showcaseZ;
        int letters = 0, balloons = 0;
        for (int x = ox; x < ox + kShowcaseW; ++x)
            for (int z = oz; z < oz + kShowcaseD; ++z)
                for (int y = oy; y < oy + 9; ++y) {
                    letters += isPaintedPlanks(w.getBlock(x, y, z)) && z == oz + 2;
                    balloons += isBalloon(w.getBlock(x, y, z));
                }
        std::printf("showcase at %d %d %d: %d letter blocks, %d balloons\n", ox, oy, oz, letters, balloons);
        CHECK(w.getBlock(ox + 21, oy + 2, oz + 8) == ANNIV_LOGO && w.getBlock(ox + 15, oy + 2, oz + 9) == CAKE && letters > 80 && balloons >= 10);
        const ChestContents* gifts = s->chest({0, ox + kShowcaseChest[0], oy + kShowcaseChest[1], oz + kShowcaseChest[2]});
        CHECK(gifts && (*gifts)[0].id == ANNIV_TOKEN && (*gifts)[1].id == PARTY_POPPER);
        // Walking onto the plaza, firing a popper.
        s->player().pos = Vec3(ox + 21.5f, (float)oy + 1, oz + 11.5f);
        s->inventory().clear();
        s->inventory().slots[0] = {PARTY_POPPER, 2};
        s->selectedSlot() = 0;
        s->player().pitch = 0.5f;
        run(0.2f);
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(0.5f);
        auto has = [&](const char* id) { return std::find(got.begin(), got.end(), id) != got.end(); };
        CHECK(has("anniv_showcase") && has("anniv_popper") && s->inventory().slots[0].count == 1);
        // A second visit does not build it again.
        const int before = st.showcaseX;
        s->player().pos = Vec3(ox - 40.5f, (float)oy + 1, oz);
        run(1);
        CHECK(s->stats().showcaseX == before);
    }
    return 0;
}

static int testUpdate15() {
    namespace fs = std::filesystem;
    const Language before = language();
    setLanguage(Language::English);
    // Ids: the new blocks are blocks, the new items are items; powerstone stairs have their own range.
    {
        for (ItemId b : std::initializer_list<ItemId>{POWERSTONE_BRICKS, CRACKED_POWERSTONE_BRICKS, CHISELED_POWERSTONE_BRICKS, POWERSTONE_GLASS, DEBRIS_GRAVEL,
                         (ItemId)SKELETON_HEAD, WASTELAND_GRASS, DRY_GRASS, GRASSER_FLOWER, FERN, CRACKED_NETHER_BRICKS,
                         CHISELED_NETHER_BRICKS, SPRUCE_FENCE, NETHER_BRICK_FENCE, POWERSTONE_FENCE, LOOT_CHEST_FORGOTTEN})
            CHECK(isNewBlock(b) && isPlaceable(b));
        for (ItemId i : std::initializer_list<ItemId>{BRUSHER, FORTRESS_COMPASS, (ItemId)OLD_DOCUMENT, (ItemId)(OLD_DOCUMENT + 7), ANCIENT_COIN, POWERSTONE_RELIC,
                         POTTERY_SHARD, EGG_HUSKING})
            CHECK(isItemOnly(i) && !isPlaceable(i));
        CHECK(stairsId(12, 2, true) == STAIRS_EXTRA + 6 && isStairs(STAIRS_EXTRA) && !isStairs((ItemId)(STAIRS_EXTRA + 8)));
        CHECK(stairsMaterial(stairsId(12, 1, false)) == 12 && stairsVariant(stairsId(12, 1, true)) == 5);
        CHECK(stairsMaterial(stairsId(3, 1, false)) == 3 && shapeMaterial(slabId(12, 0)) == POWERSTONE_BRICKS);
        CHECK(isSlab(slabId(12, 2)) && slabId(12, 2) < TAR && !isSlab(STAIRS_EXTRA));
        CHECK(std::string(itemKey(stairsId(12, 3, false))) == "powerstone_stairs" && itemFromKey("powerstone_slab") == slabId(12, 0));
        CHECK(itemFromKey("minecraft:suspicious_gravel") == DEBRIS_GRAVEL && itemFromKey("brush") == BRUSHER);
        CHECK(itemFromKey("old_document_3") == OLD_DOCUMENT + 2 && std::string(itemKey(OLD_DOCUMENT + 5)) == "old_document_6");
        // Light: only lamps and other glowing blocks shine, never the bricks, glass, stairs or slabs.
        for (ItemId b : std::initializer_list<ItemId>{POWERSTONE_BRICKS, CRACKED_POWERSTONE_BRICKS, CHISELED_POWERSTONE_BRICKS, POWERSTONE_GLASS, stairsId(12, 0, false), slabId(12, 0)})
            CHECK(lightLevel(b) == 0 && !isEmissive(b));
        CHECK(lightLevel(POWERSTONE_LAMP_ON) == 15 && !isOpaque(POWERSTONE_GLASS) && isOpaque(POWERSTONE_BRICKS));
        // Every catalogue item has an English name, every new tile a texture source.
        int unnamed = 0;
        for (int i = 0; i < kCreativeItemCount; ++i) {
            const char* cz = itemName(kCreativeItems[i]);
            const std::string en = tr(cz);
            for (size_t k = 0; k + 1 < en.size(); ++k)
                if ((unsigned char)en[k] == 0xC3 || (unsigned char)en[k] == 0xC5 || (unsigned char)en[k] == 0xC4) {
                    std::printf("untranslated item: %s\n", cz);
                    ++unnamed;
                    break;
                }
        }
        CHECK(unnamed == 0 && std::string(tr(itemName(SKELETON_HEAD + 3))) == "Skeleton Head" && std::string(tr(itemName(BRUSHER))) == "Brusher");
        // Fences look like fences in the inventory now (posts and rails, not a plank cube).
        Box boxes[kItemBoxes];
        CHECK(itemBoxes(FENCE, boxes) == 4 && boxes[0].x1 - boxes[0].x0 < 0.3f && itemBoxes(POWERSTONE_FENCE, boxes) == 4);
        CHECK(fenceConnects(NETHER_BRICK_FENCE) && fenceConnects(FENCE) && isShaped(SPRUCE_FENCE) && isFence(POWERSTONE_FENCE));
        CHECK(tileFor(POWERSTONE_FENCE, F_TOP) == T_POWERSTONE_BRICKS && tileFor(SKELETON_HEAD, F_SOUTH) == T_SKULL_FACE &&
              tileFor(SKELETON_HEAD + 2, F_NORTH) == T_SKULL_FACE && tileFor(SKELETON_HEAD + 2, F_SOUTH) == T_SKULL);
        // Recipes: powerstone bricks from stone bricks and dust, stairs, glass, fences, the brusher; cracked bricks in the furnace.
        ItemStack g[9] = {};
        g[0] = g[3] = {STONE_BRICKS, 1};
        g[1] = g[2] = {POWERSTONE_DUST, 1};
        CHECK(matchRecipe(g, 2).id == POWERSTONE_BRICKS && matchRecipe(g, 2).count == 2);
        ItemStack st[9] = {};
        for (int i : {0, 3, 4, 6, 7, 8}) st[i] = {POWERSTONE_BRICKS, 1};
        CHECK(matchRecipe(st, 3).id == stairsId(12, 0, false));
        ItemStack fence[9] = {};
        for (int i : {0, 2, 3, 5}) fence[i] = {SPRUCE_PLANKS, 1};
        fence[1] = fence[4] = {STICK, 1};
        CHECK(matchRecipe(fence, 3).id == SPRUCE_FENCE && matchRecipe(fence, 3).count == 3);
        ItemStack brush[9] = {};
        brush[1] = {FEATHER, 1};
        brush[4] = {IRON_INGOT, 1};
        brush[7] = {STICK, 1};
        CHECK(matchRecipe(brush, 3).id == BRUSHER);
        CHECK(recipes().smelting.count(POWERSTONE_BRICKS) && recipes().smelting.at(POWERSTONE_BRICKS) == CRACKED_POWERSTONE_BRICKS);
        CHECK(toolDurability(BRUSHER) == 64 && maxStack(BRUSHER) == 1 && maxStack(FORTRESS_COMPASS) == 1 && maxStack(OLD_DOCUMENT) == 16);
        CHECK(dropFor(DEBRIS_GRAVEL, 0.5f) == GRAVEL && dropFor(SKELETON_HEAD + 2, 0.5f) == SKELETON_HEAD && dropFor(POWERSTONE_GLASS, 0.5f) == AIR);
        CHECK(dropFor(POWERSTONE_BRICKS, 0.5f, WOOD_PICKAXE) == POWERSTONE_BRICKS && dropFor(POWERSTONE_BRICKS, 0.5f) == AIR);
    }
    // Loot and archaeology: portal ruins always give the fortress compass, every ruin chest a page.
    {
        int compass = 0, pages = 0, finds[5] = {};
        for (uint32_t seed = 1; seed <= 40; ++seed) {
            for (const ItemStack& it : generateLoot(LOOT_CHEST_PORTAL, seed)) compass += it.id == FORTRESS_COMPASS, pages += isDocument(it.id);
            for (const ItemStack& it : generateLoot(LOOT_CHEST_FORGOTTEN, seed)) pages += isDocument(it.id);
            for (const ItemStack& it : generateLoot(LOOT_CHEST_DROWNED, seed)) pages += isDocument(it.id);
        }
        for (uint32_t seed = 1; seed <= 400; ++seed) {
            const ItemStack f = archaeologyFind(seed);
            finds[0] += isSkeletonHead(f.id), finds[1] += f.id == ANCIENT_COIN, finds[2] += f.id == POTTERY_SHARD;
            finds[3] += isDocument(f.id), finds[4] += f.id == POWERSTONE_RELIC;
            CHECK(f.count >= 1);
        }
        std::printf("loot 1.5: %d compasses in 40 portal chests, %d pages; finds: heads %d coins %d shards %d pages %d relics %d\n", compass,
                    pages, finds[0], finds[1], finds[2], finds[3], finds[4]);
        CHECK(compass == 40 && pages >= 120 && finds[0] > 10 && finds[1] > 50 && finds[3] > 20 && finds[4] > 3);
    }
    // Structures: blueprints hold their chests, lamps that glow and the things to brush.
    {
        auto count = [](const Structure& st, auto pred) {
            int n = 0;
            for (ItemId b : st.cells) n += pred(b) ? 1 : 0;
            return n;
        };
        const Structure village = buildWorldStructure(WorldStructure::ForgottenVillage, 77);
        const Structure drowned = buildWorldStructure(WorldStructure::DrownedRuins, 77);
        const Structure portal = buildWorldStructure(WorldStructure::BrokenPortal, 77);
        const int vChests = count(village, [](ItemId b) { return b == LOOT_CHEST_FORGOTTEN; });
        const int vLamps = count(village, [](ItemId b) { return b == POWERSTONE_LAMP_ON; });
        const int vBricks = count(village, [](ItemId b) { return b == POWERSTONE_BRICKS || b == CRACKED_POWERSTONE_BRICKS || b == CHISELED_POWERSTONE_BRICKS; });
        const int vDebris = count(village, [](ItemId b) { return b == DEBRIS_GRAVEL; });
        std::printf("forgotten village: %d chests, %d lit lamps, %d bricks, %d debris\n", vChests, vLamps, vBricks, vDebris);
        CHECK(village.sx == kForgottenVillageSize && vChests >= 4 && vLamps >= 1 && vBricks > 200 && vDebris > 5);
        // Every lit lamp sits under a powerstone block, so it stays lit.
        for (int y = 0; y + 1 < village.sy; ++y)
            for (int z = 0; z < village.sz; ++z)
                for (int x = 0; x < village.sx; ++x)
                    if (village.at(x, y, z) == POWERSTONE_LAMP_ON) CHECK(village.at(x, y + 1, z) == POWERSTONE_BLOCK);
        CHECK(count(drowned, [](ItemId b) { return b == LOOT_CHEST_DROWNED; }) >= 1 && count(drowned, [](ItemId b) { return isSkeletonHead(b); }) >= 1);
        CHECK(count(portal, [](ItemId b) { return b == LOOT_CHEST_PORTAL; }) == 1 && count(portal, [](ItemId b) { return b == OBSIDIAN; }) >= 6);
        // The fortress: cracked and carved nether bricks, nether brick fences, the kingdom's carved stone.
        const Structure fort = buildWorldStructure(WorldStructure::Fortress, 5);
        CHECK(count(fort, [](ItemId b) { return b == CRACKED_NETHER_BRICKS; }) > 100 && count(fort, [](ItemId b) { return b == NETHER_BRICK_FENCE; }) > 10 &&
              count(fort, [](ItemId b) { return b == FENCE; }) == 0 && count(fort, [](ItemId b) { return b == CHISELED_POWERSTONE_BRICKS; }) >= 7);
    }
    // 1.5.1: the wasteland, its Grasser flowers and the forgotten villages lie in the Cornerstone
    // Dimension; the Overworld has none of them.
    {
        World over(4242, Dimension::Overworld);
        over.ensureChunksAround(0, 0, 6, 1000);
        int overWaste = 0, overGrasser = 0;
        for (auto& [k, c] : over.chunks())
            for (ItemId b : c->blocks) overWaste += b == WASTELAND_GRASS, overGrasser += b == GRASSER_FLOWER;
        for (const BiomeDef& b : GenConfig::defaults().biomes) CHECK(b.id != "powerstone_wasteland");
        GenConfig old = GenConfig::defaults();
        old.biomes.push_back(old.biomes[0]);
        old.biomes.back().id = "powerstone_wasteland";
        retireBiomes(old);
        CHECK(old.biomes.size() == GenConfig::defaults().biomes.size());
        World w(4242, Dimension::Nether);
        int bx = 1 << 30, bz = 0;
        for (int r = 0; r < 1500 && bx == (1 << 30); r += 24)
            for (int a = 0; a < 16 && bx == (1 << 30); ++a) {
                const int x = (int)(std::cos(a * 0.3927f) * r), z = (int)(std::sin(a * 0.3927f) * r);
                if (w.wastelandAt(x, z) && w.wastelandAt(x + 24, z) && w.wastelandAt(x, z + 24)) bx = x, bz = z;
            }
        CHECK(bx != (1 << 30));
        w.ensureChunksAround(floorDiv(bx, 16), floorDiv(bz, 16), 3, 64);
        int ground = 0, dry = 0, grasser = 0, debris = 0, pillars = 0;
        for (auto& [k, c] : w.chunks())
            for (ItemId b : c->blocks)
                ground += b == WASTELAND_GRASS, dry += b == DRY_GRASS, grasser += b == GRASSER_FLOWER, debris += b == DEBRIS_GRAVEL,
                    pillars += b == CHISELED_POWERSTONE_BRICKS;
        std::printf("cornerstone wasteland at %d %d: grass %d, dry grass %d, grasser %d, debris %d, pillar tops %d; overworld: %d %d\n", bx, bz,
                    ground, dry, grasser, debris, pillars, overWaste, overGrasser);
        CHECK(overWaste == 0 && overGrasser == 0);
        CHECK(ground > 500 && dry > 50 && grasser > 0 && debris > 0);
        CHECK(w.placeName(bx, bz) == "Powerstonová pustina" && std::string(tr("Powerstonová pustina")) == "Powerstone Wasteland");
        CHECK(tileFor(WASTELAND_GRASS, F_BOTTOM) == T_NETHERRACK && dropFor(WASTELAND_GRASS, 0.5f, WOOD_PICKAXE) == NETHERRACK);
        // A forgotten village: found by its region, generated with its chests and lamps, on solid ground.
        int vx = 0, vy = 0, vz = 0;
        bool found = false;
        for (int r = 0; r < 12 && !found; ++r)
            for (int rx = -r; rx <= r && !found; ++rx)
                for (int rz = -r; rz <= r && !found; ++rz)
                    if (std::max(std::abs(rx), std::abs(rz)) == r && w.forgottenVillageIn(rx, rz, vx, vy, vz)) found = true;
        CHECK(found);
        World vw(4242, Dimension::Nether);
        vw.ensureChunksAround(floorDiv(vx, 16), floorDiv(vz, 16), 2, 64);
        int chests = 0, lamps = 0;
        for (int x = vx - 20; x <= vx + 20; ++x)
            for (int z = vz - 20; z <= vz + 20; ++z)
                for (int y = vy - 2; y < vy + 10; ++y) {
                    const ItemId b = vw.getBlock(x, y, z);
                    chests += b == LOOT_CHEST_FORGOTTEN, lamps += b == POWERSTONE_LAMP_ON;
                }
        std::printf("cornerstone village at %d %d %d: %d chests, %d lamps, plaza %d, under it %d\n", vx, vy, vz, chests, lamps,
                    (int)vw.getBlock(vx, vy - 1, vz), (int)vw.getBlock(vx + 2, vy - 2, vz + 2));
        CHECK(chests >= 4 && lamps >= 1 && vw.getBlock(vx, vy + 4, vz) == POWERSTONE_LAMP_ON && isSolid(vw.getBlock(vx + 2, vy - 2, vz + 2)));
        CHECK(std::string(tr("Dimenze úhelného kamene")) == "Cornerstone Dimension");
    }
    // Playing: brushing debris gravel, reading a page, a Husking's fireball, the fortress compass.
    {
        auto s = flatSession(fs::temp_directory_path() / "vc_update15", GameMode::Survival);
        Settings settings;
        settings.renderDistance = 2;
        double t = 0;
        auto run = [&](float seconds) {
            for (float e = 0; e < seconds; e += 1 / 60.0f) s->update(1 / 60.0f, t += 1 / 60.0, 600, 270, settings);
        };
        std::vector<std::string> earned;
        s->onAchievement = [&](const std::string& id) { earned.push_back(id); };
        auto has = [&](const char* id) { return std::find(earned.begin(), earned.end(), id) != earned.end(); };
        run(0.5f);
        World& w = s->currentWorld();
        Player& p = s->player();
        s->mobs().mobs().clear();
        s->inventory().clear();
        p.pos = Vec3(8.5f, 4, 8.5f);
        p.yaw = 0;
        p.pitch = -0.9f;
        run(0.1f);
        // The ground in front turns to debris gravel; hold the brusher on it.
        int gz = 0;
        s->inventory().slots[0] = {BRUSHER, 1};
        s->selectedSlot() = 0;
        for (int z = 1; z < 9; ++z) w.setBlock(8, 3, z, DEBRIS_GRAVEL);
        s->touchDown(1, 300, 135, t);
        run(0.5f);
        CHECK(s->inventory().slots[0].id == BRUSHER);  // still brushing, nothing found yet
        run(2.0f);
        s->touchUp(1, 300, 135, t);
        run(0.1f);
        for (int z = 1; z < 9; ++z) gz += w.getBlock(8, 3, z) == GRAVEL;
        std::printf("brushing: %d block(s) uncovered, brusher damage %d, drops %zu\n", gz, (int)s->inventory().slots[0].damage,
                    s->drops().drops().size());
        CHECK(gz == 1 && s->inventory().slots[0].damage == 1 && has("archaeologist"));
        // Reading a page opens the document screen and remembers the page.
        s->inventory().slots[1] = {(ItemId)(OLD_DOCUMENT + 4), 1};
        s->selectedSlot() = 1;
        p.pitch = 1.2f;  // up at the sky, nothing to tap on
        run(0.1f);
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
        CHECK(s->wantsDocument == 4 && (s->stats().documentsRead & (1 << 4)) && has("historian"));
        s->wantsDocument = -1;
        for (int page = 0; page < kDocumentPages; ++page) s->readDocument(page);
        CHECK(s->stats().documentsRead == 255 && has("lore_master"));
        // A Husking keeps its distance and spits fire; the fireball hurts.
        p.pitch = 0;
        s->mobs().mobs().clear();
        s->mobs().spawn(MobType::Husking, 0, Vec3(8.5f, 4, 0.5f));
        auto huskIndex = [&] {
            for (size_t i = 0; i < s->mobs().mobs().size(); ++i)
                if (s->mobs().mobs()[i].type == MobType::Husking) return (int)i;
            return -1;
        };
        const float health = s->stats().health;
        bool fired = false;
        for (int i = 0; i < 6 * 60 && s->stats().health >= health; ++i) {
            run(1 / 60.0f);
            fired |= s->fireballCount() > 0;
        }
        CHECK(huskIndex() >= 0);
        const Mob& husk = s->mobs().mobs()[(size_t)huskIndex()];
        std::printf("husking: fired %d, health %.1f -> %.1f, distance %.1f\n", fired ? 1 : 0, health, s->stats().health, (husk.pos - p.pos).length());
        CHECK(fired && s->stats().health < health && (husk.pos - p.pos).length() > 4.5f);
        // Close in: it backs away.
        p.pos = husk.pos + Vec3(0, 0, 3);
        const float near = (husk.pos - p.pos).length();
        run(1.0f);
        CHECK((s->mobs().mobs()[(size_t)huskIndex()].pos - p.pos).length() > near + 0.5f);
        // Defeat it: its drops, the achievement.
        const int hi = huskIndex();
        while (s->mobs().mobs()[(size_t)hi].alive()) s->mobs().hurt(hi, 50, p.pos);
        run(1.5f);
        CHECK(has("husking_hunter"));
        // Placing a skeleton head: it looks back at the player.
        s->inventory().slots[2] = {SKELETON_HEAD, 2};
        s->selectedSlot() = 2;
        p.pos = Vec3(8.5f, 4, 8.5f);
        p.yaw = 0;
        p.pitch = -0.9f;
        for (int z = 1; z < 9; ++z) w.setBlock(8, 4, z, AIR), w.setBlock(8, 3, z, STONE);
        run(0.1f);
        s->touchDown(1, 300, 135, t);
        run(1 / 60.0f);
        s->touchUp(1, 300, 135, t);
        run(1 / 60.0f);
        int head = 0;
        for (int z = 1; z < 9; ++z)
            if (isSkeletonHead(w.getBlock(8, 4, z))) head = w.getBlock(8, 4, z);
        CHECK(head == SKELETON_HEAD && s->inventory().slots[2].count == 1);  // looking north: the face looks south
        // The documents read are saved with the world.
        s->save();
        WorldInfo info;
        PlayerState ps;
        CHECK(loadLevel(s->info().folder, info, ps) && ps.documentsRead == 255);
    }
    setLanguage(before);
    return 0;
}

static int testExperiments151() {
    namespace fs = std::filesystem;
    // Experiments are per world: saved in level.dat, off for worlds made before 1.5.1.
    fs::path dir = fs::temp_directory_path() / "vc_exp_world";
    fs::remove_all(dir);
    fs::create_directories(dir);
    WorldInfo w;
    w.folder = dir.string();
    w.name = "exp";
    w.seed = 7;
    w.expOreRedesign = true;
    CHECK(saveLevel(w, PlayerState{}));
    WorldInfo r;
    PlayerState p;
    CHECK(loadLevel(dir.string(), r, p) && r.expOreRedesign);
    std::ofstream((dir / "level.dat").string()) << "version=1\nname=old\nseed=5\nmode=1\n";
    CHECK(loadLevel(dir.string(), r, p) && !r.expOreRedesign);
    fs::remove_all(dir);
    // The redesigned ores are opaque 16x16 pictures that differ from the current ones.
    int size = 0;
    const std::vector<uint8_t> atlas = buildFallbackAtlas(size);
    for (int i = 0; i < kOreRedesignCount; ++i) {
        uint8_t px[16 * 16 * 4];
        oreRedesignTile(i, px);
        int diff = 0, opaque = 0;
        const int tile = kOreRedesignTiles[i];
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const uint8_t* a = &atlas[((size_t)((tile / kAtlasCols) * 16 + y) * size + (tile % kAtlasCols) * 16 + x) * 4];
                for (int k = 0; k < 3; ++k) diff += std::abs(a[k] - px[(y * 16 + x) * 4 + k]);
                opaque += px[(y * 16 + x) * 4 + 3] == 255;
            }
        CHECK(opaque == 256 && diff > 3000);
    }
    return 0;
}

static int testShaders155() {
    namespace fs = std::filesystem;
    // Graphics mode is a setting: Enhanced by default, Classic survives a save.
    fs::path root = fs::temp_directory_path() / "vc_shaders";
    fs::remove_all(root);
    fs::create_directories(root);
    Settings st;
    CHECK(st.graphics == GraphicsMode::Enhanced);
    st.graphics = GraphicsMode::Classic;
    saveSettings((root / "settings.txt").string(), st);
    CHECK(loadSettings((root / "settings.txt").string()).graphics == GraphicsMode::Classic);
    // A shader pack: numbers are read and clamped, a bad world.fsh is refused.
    PackManager packs(root.string());
    packs.ensureDefaultPack();
    CHECK(packs.shaderPackDir().empty());
    const std::string id = packs.createPack("Sunny");
    writeShaderPackFiles(packs.packDir(id), "Sunny");
    packs.setEnabled(id, true);
    CHECK(packs.shaderPackDir() == packs.packDir(id));
    CHECK(fs::exists(fs::path(packs.packDir(id)) / "shaders/README.txt") && fs::exists(fs::path(packs.packDir(id)) / "shaders/world.fsh.example"));
    ShaderProfile p = loadShaderPack(packs.packDir(id));
    CHECK(p.name == "Sunny" && std::fabs(p.saturation - ShaderProfile{}.saturation) < 1e-4f && p.fragment.empty());
    std::ofstream((fs::path(packs.packDir(id)) / "shaders/shader.json").string())
        << R"({"name": "Loud", "saturation": 9, "warmth": 0.2, "shade": [0.5, 0.6, 3]})";
    std::ofstream((fs::path(packs.packDir(id)) / "shaders/world.fsh").string()) << "void main() {}";
    std::vector<std::string> warn;
    p = loadShaderPack(packs.packDir(id), &warn);
    CHECK(p.name == "Loud" && p.saturation == 2.0f && std::fabs(p.warmth - 0.2f) < 1e-4f && p.shade[2] == 2.0f && p.fragment.empty() && warn.size() == 1);
    std::ofstream((fs::path(packs.packDir(id)) / "shaders/world.fsh").string()) << kVibrantWorldFs;
    CHECK(loadShaderPack(packs.packDir(id)).fragment == kVibrantWorldFs);
    // An old shader pack of another game: its files are kept aside, the pack gets our numbers.
    const std::string old = packs.createPack("Old Shaders");
    fs::create_directories(fs::path(packs.packDir(old)) / "shaders/program");
    std::ofstream((fs::path(packs.packDir(old)) / "shaders/gbuffers_terrain.fsh").string()) << "#version 120\nvoid main(){}";
    std::ofstream((fs::path(packs.packDir(old)) / "shaders/program/final.fsh").string()) << "#version 120";
    CHECK(convertImportedShaderPack(packs.packDir(old), "Old Shaders"));
    CHECK(fs::exists(fs::path(packs.packDir(old)) / "shaders/original/gbuffers_terrain.fsh") &&
          fs::exists(fs::path(packs.packDir(old)) / "shaders/original/program/final.fsh"));
    CHECK(loadShaderPack(packs.packDir(old)).name == "Old Shaders" && loadShaderPack(packs.packDir(old)).fragment.empty());
    CHECK(!convertImportedShaderPack(packs.packDir(id), "x"));  // ours stays as it is
    // Both shaders read the same inputs; the Vibrant one has every pack number.
    for (const char* u : {"u_saturation", "u_contrast", "u_warmth", "u_vignette", "u_glow", "u_shade", "u_fogStrength", "u_time", "u_screen"})
        CHECK(std::string(kVibrantWorldFs).find(u) != std::string::npos && std::string(kShaderPackGuide).find(u) != std::string::npos);
    fs::remove_all(root);
    return 0;
}

int main() {
    if (testOverworld() || testNether() || testPortal() || testTexturePacks() || testFontAndItems() || testSaves() ||
        testSurvivalSession() || testMenuFlow() || testCrafting() || testWorkbenchInSession() ||
        testJsonAndPacks() || testSkinsStoreAudio() || testThirdPersonCamera() || testWaterEscape() || testLeavesMesh() || testPlants() || testDigParticles() || testDPad() || testStructures() || testInventoryAndHand() || testMetalsAndArmor() || testDeveloperAccess() || testEnglishEverywhere() || testMusic() || testDoorsChests() || testNewBiomes() || testWorldStructures() || testBoat() || testAchievements() || testLanGame() || testCommands() || testUpdate11() || testUpdate12() || testUpdate13() || testUpdate14() || testUpdate15() || testExperiments151() || testShaders155())
        return 1;
    std::printf("all tests passed\n");
    return 0;
}
