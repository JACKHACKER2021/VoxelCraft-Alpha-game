#include "Structure.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

#include "Blocks.h"
#include "Items.h"
#include "Lang.h"
#include "Nbt.h"

namespace vc {

int Structure::blockCount() const {
    return (int)std::count_if(cells.begin(), cells.end(), [](ItemId c) { return c != kVoid; });
}

Structure Structure::rotated(int quarterTurns) const {
    quarterTurns = ((quarterTurns % 4) + 4) % 4;
    Structure r = *this;
    for (int turn = 0; turn < quarterTurns; ++turn) {
        Structure next;
        next.name = r.name;
        next.sx = r.sz;
        next.sy = r.sy;
        next.sz = r.sx;
        next.cells.assign(r.cells.size(), kVoid);
        // Clockwise from above: (x, z) -> (sz - 1 - z, x)
        for (int y = 0; y < r.sy; ++y)
            for (int z = 0; z < r.sz; ++z)
                for (int x = 0; x < r.sx; ++x) next.at(r.sz - 1 - z, y, x) = r.at(x, y, z);
        r = std::move(next);
    }
    return r;
}

Structure Structure::mirroredX() const {
    Structure m = *this;
    for (int y = 0; y < sy; ++y)
        for (int z = 0; z < sz; ++z)
            for (int x = 0; x < sx; ++x) m.at(sx - 1 - x, y, z) = at(x, y, z);
    return m;
}

// ------------------------------------------------------------------ .vcstructure

namespace {

void put16(std::vector<uint8_t>& o, uint16_t v) {
    o.push_back((uint8_t)v);
    o.push_back((uint8_t)(v >> 8));
}

struct ByteReader {
    const std::vector<uint8_t>& d;
    size_t pos = 0;
    bool u16(uint16_t& v) {
        if (pos + 2 > d.size()) return false;
        v = (uint16_t)(d[pos] | d[pos + 1] << 8);
        pos += 2;
        return true;
    }
    bool bytes(size_t n, std::string& out) {
        if (pos + n > d.size()) return false;
        out.assign(reinterpret_cast<const char*>(&d[pos]), n);
        pos += n;
        return true;
    }
};

constexpr int kMaxSide = 256;  // 256 x 128 x 256 at most

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

}  // namespace

std::vector<uint8_t> writeVcStructure(const Structure& s) {
    std::vector<uint8_t> o = {'V', 'C', 'S', 'T'};
    put16(o, 1);
    put16(o, (uint16_t)s.sx);
    put16(o, (uint16_t)s.sy);
    put16(o, (uint16_t)s.sz);
    put16(o, (uint16_t)s.name.size());
    o.insert(o.end(), s.name.begin(), s.name.end());
    // Palette of the ids actually used.
    std::vector<ItemId> palette;
    for (ItemId c : s.cells)
        if (std::find(palette.begin(), palette.end(), c) == palette.end()) palette.push_back(c);
    put16(o, (uint16_t)palette.size());
    for (ItemId id : palette) {
        std::string key = id == Structure::kVoid ? "void" : itemKey(id);
        o.push_back((uint8_t)key.size());
        o.insert(o.end(), key.begin(), key.end());
    }
    for (size_t i = 0; i < s.cells.size();) {
        size_t run = 1;
        while (i + run < s.cells.size() && run < 65535 && s.cells[i + run] == s.cells[i]) ++run;
        put16(o, (uint16_t)run);
        put16(o, (uint16_t)(std::find(palette.begin(), palette.end(), s.cells[i]) - palette.begin()));
        i += run;
    }
    return o;
}

bool readVcStructure(const std::vector<uint8_t>& data, Structure& out, std::string& error) {
    ByteReader r{data, 0};
    std::string magic;
    uint16_t version, sx, sy, sz, nameLen, paletteSize;
    if (!r.bytes(4, magic) || magic != "VCST") {
        error = "není to soubor .vcstructure";
        return false;
    }
    if (!r.u16(version) || version != 1) {
        error = "neznámá verze .vcstructure";
        return false;
    }
    if (!r.u16(sx) || !r.u16(sy) || !r.u16(sz) || !r.u16(nameLen) || !r.bytes(nameLen, out.name) || !r.u16(paletteSize)) {
        error = "poškozená hlavička";
        return false;
    }
    if (!sx || !sy || !sz || sx > kMaxSide || sz > kMaxSide || sy > kChunkHeight) {
        error = "nepodporovaná velikost struktury";
        return false;
    }
    std::vector<ItemId> palette;
    for (int i = 0; i < paletteSize; ++i) {
        if (r.pos >= data.size()) {
            error = "poškozená paleta";
            return false;
        }
        uint8_t len = data[r.pos++];
        std::string key;
        if (!r.bytes(len, key)) {
            error = "poškozená paleta";
            return false;
        }
        palette.push_back(key == "void" ? Structure::kVoid : itemFromKey(key));
    }
    out.sx = sx;
    out.sy = sy;
    out.sz = sz;
    out.cells.clear();
    out.cells.reserve((size_t)sx * sy * sz);
    const size_t total = (size_t)sx * sy * sz;
    while (out.cells.size() < total) {
        uint16_t run, index;
        if (!r.u16(run) || !r.u16(index) || index >= palette.size() || run == 0 || out.cells.size() + run > total) {
            error = "poškozená data bloků";
            return false;
        }
        out.cells.insert(out.cells.end(), run, palette[index]);
    }
    return true;
}

// ------------------------------------------------------------------ .mcstructure

ItemId mapBedrockBlock(const std::string& fullName, bool& known) {
    known = true;
    std::string n = fullName;
    if (n.rfind("minecraft:", 0) == 0) n = n.substr(10);
    if (n == "air" || n == "cave_air" || n == "void_air") return AIR;
    if (n == "structure_void") return Structure::kVoid;
    // Bedrock names that differ from VoxelCraft keys.
    static const std::map<std::string, std::string> aliases = {
        {"brick_block", "bricks"},      {"flowing_water", "water"},     {"flowing_lava", "lava"},
        {"grass_block", "grass"},       {"oak_log", "log"},             {"log2", "log"},
        {"oak_planks", "planks"},       {"oak_leaves", "leaves"},       {"leaves2", "leaves"},
        {"red_flower", "poppy"},        {"yellow_flower", "dandelion"}, {"tallgrass", "tall_grass"},
        {"short_grass", "tall_grass"},  {"lit_furnace", "furnace"},     {"quartz_ore", "quartz_ore"},
        {"nether_quartz_ore", "quartz_ore"}, {"stonebrick", "stone_bricks"},
        {"smooth_stone", "stone"},      {"coarse_dirt", "dirt"},     {"snow_layer", "snow"},
        {"stained_glass", "glass"},     {"glass_pane", "glass"},        {"nether_bricks", "nether_brick"}, {"bricks", "bricks"},
    };
    if (auto it = aliases.find(n); it != aliases.end()) n = it->second;
    // Wood types we do not have yet are built from oak.
    for (const char* wood : {"spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry"}) {
        std::string w = wood;
        if (n == w + "_log" || n == w + "_wood") return LOG;
        if (n == w + "_planks") return PLANKS;
        if (n == w + "_leaves") return LEAVES;
    }
    ItemId id = itemFromKey(n);
    if (id == AIR || isItemOnly(id) || id == PORTAL) {
        known = false;
        return Structure::kVoid;
    }
    return id;
}

bool importMcStructure(const std::vector<uint8_t>& data, Structure& out, ImportReport& report, std::string& error) {
    Nbt root;
    if (!Nbt::parse(data, root, error)) return false;
    const Nbt* size = root.get("size");
    const Nbt* structure = root.get("structure");
    if (!size || size->type != Nbt::List || size->items.size() != 3 || !structure) {
        error = "chybí \"size\" nebo \"structure\" (není to .mcstructure)";
        return false;
    }
    int sx = (int)size->items[0].integer, sy = (int)size->items[1].integer, sz = (int)size->items[2].integer;
    if (sx <= 0 || sy <= 0 || sz <= 0 || sx > kMaxSide || sz > kMaxSide || sy > kChunkHeight) {
        error = trf("struktura {0}×{1}×{2} je mimo podporovaný rozsah (max 256×128×256)",
                    {std::to_string(sx), std::to_string(sy), std::to_string(sz)});
        return false;
    }
    const Nbt* indices = structure->get("block_indices");
    const Nbt* palette = structure->get("palette");
    const Nbt* def = palette ? palette->get("default") : nullptr;
    const Nbt* blocks = def ? def->get("block_palette") : nullptr;
    if (!indices || indices->type != Nbt::List || indices->items.empty() || !blocks || blocks->type != Nbt::List) {
        error = "chybí block_indices nebo palette.default.block_palette";
        return false;
    }
    // Map the palette once.
    std::vector<ItemId> map;
    std::vector<std::string> names;
    for (const Nbt& entry : blocks->items) {
        const Nbt* name = entry.get("name");
        std::string n = name ? name->text : "?";
        bool known = true;
        map.push_back(mapBedrockBlock(n, known));
        names.push_back(known ? "" : n);
        if (const Nbt* st = entry.get("states"); st && !st->fields.empty()) ++report.ignoredStates;
    }
    const size_t total = (size_t)sx * sy * sz;
    const Nbt& layer = indices->items[0];
    if (layer.items.size() != total) {
        error = trf("block_indices nemá {0} položek", {std::to_string(total)});
        return false;
    }
    out.name = "mcstructure";
    out.sx = sx;
    out.sy = sy;
    out.sz = sz;
    out.cells.assign(total, Structure::kVoid);
    // Bedrock order: z fastest, then y, then x.
    size_t i = 0;
    for (int x = 0; x < sx; ++x)
        for (int y = 0; y < sy; ++y)
            for (int z = 0; z < sz; ++z, ++i) {
                int64_t pi = layer.items[i].integer;
                ++report.cells;
                if (pi < 0 || pi >= (int64_t)map.size()) {
                    ++report.voidCells;
                    continue;
                }
                ItemId id = map[(size_t)pi];
                if (!names[(size_t)pi].empty()) ++report.unknown[names[(size_t)pi]];
                if (id == Structure::kVoid) ++report.voidCells;
                else ++report.placedBlocks;
                out.at(x, y, z) = id;
            }
    if (indices->items.size() > 1)
        for (const Nbt& v : indices->items[1].items) report.secondLayer += v.integer >= 0;
    if (const Nbt* ents = structure->get("entities")) report.entities = (int)ents->items.size();
    return true;
}

std::vector<uint8_t> exportMcStructure(const Structure& s) {
    static const std::map<std::string, std::string> names = {
        {"grass", "grass_block"}, {"log", "oak_log"}, {"planks", "oak_planks"}, {"leaves", "oak_leaves"},
        {"tall_grass", "short_grass"}, {"nether_brick", "nether_bricks"}, {"quartz_ore", "quartz_ore"},
    };
    Nbt root = Nbt::makeCompound();
    root.fields["format_version"] = Nbt::makeInt(1);
    Nbt size = Nbt::makeList(Nbt::Int);
    for (int v : {s.sx, s.sy, s.sz}) size.items.push_back(Nbt::makeInt(v));
    root.fields["size"] = size;
    Nbt origin = Nbt::makeList(Nbt::Int);
    for (int i = 0; i < 3; ++i) origin.items.push_back(Nbt::makeInt(0));
    root.fields["structure_world_origin"] = origin;

    std::vector<ItemId> used;
    Nbt palette = Nbt::makeList(Nbt::Compound);
    Nbt layer = Nbt::makeList(Nbt::Int), water = Nbt::makeList(Nbt::Int);
    for (int x = 0; x < s.sx; ++x)
        for (int y = 0; y < s.sy; ++y)
            for (int z = 0; z < s.sz; ++z) {
                ItemId id = s.at(x, y, z);
                water.items.push_back(Nbt::makeInt(-1));
                if (id == Structure::kVoid) {
                    layer.items.push_back(Nbt::makeInt(-1));
                    continue;
                }
                auto it = std::find(used.begin(), used.end(), id);
                if (it == used.end()) {
                    used.push_back(id);
                    std::string key = id == AIR ? "air" : itemKey(id);
                    if (auto n = names.find(key); n != names.end()) key = n->second;
                    Nbt entry = Nbt::makeCompound();
                    entry.fields["name"] = Nbt::makeString("minecraft:" + key);
                    entry.fields["states"] = Nbt::makeCompound();
                    entry.fields["version"] = Nbt::makeInt(18168865);  // 1.21 block format
                    palette.items.push_back(entry);
                    it = used.end() - 1;
                }
                layer.items.push_back(Nbt::makeInt((int32_t)(it - used.begin())));
            }
    Nbt indices = Nbt::makeList(Nbt::List);
    indices.items.push_back(layer);
    indices.items.push_back(water);
    Nbt def = Nbt::makeCompound();
    def.fields["block_palette"] = palette;
    def.fields["block_position_data"] = Nbt::makeCompound();
    Nbt pal = Nbt::makeCompound();
    pal.fields["default"] = def;
    Nbt structure = Nbt::makeCompound();
    structure.fields["block_indices"] = indices;
    structure.fields["entities"] = Nbt::makeList(Nbt::Compound);
    structure.fields["palette"] = pal;
    root.fields["structure"] = structure;
    return root.write();
}

bool captureStructure(const World& world, int x0, int y0, int z0, int sx, int sy, int sz, Structure& out,
                      std::string& error) {
    if (sx <= 0 || sy <= 0 || sz <= 0 || sx > kMaxSide || sz > kMaxSide || y0 < 0 || y0 + sy > kChunkHeight) {
        error = "neplatný výběr";
        return false;
    }
    for (int cz = floorDiv(z0, kChunkSize); cz <= floorDiv(z0 + sz - 1, kChunkSize); ++cz)
        for (int cx = floorDiv(x0, kChunkSize); cx <= floorDiv(x0 + sx - 1, kChunkSize); ++cx)
            if (!world.getChunk(cx, cz)) {
                error = "část výběru není načtená";
                return false;
            }
    out.sx = sx;
    out.sy = sy;
    out.sz = sz;
    out.cells.assign((size_t)sx * sy * sz, AIR);
    for (int y = 0; y < sy; ++y)
        for (int z = 0; z < sz; ++z)
            for (int x = 0; x < sx; ++x) out.at(x, y, z) = world.getBlock(x0 + x, y0 + y, z0 + z);
    return true;
}

// ------------------------------------------------------------------ placing

bool placeStructure(World& world, const Structure& s, int ox, int oy, int oz, std::vector<BlockChange>& undo, std::string& error) {
    undo.clear();
    if (!s.valid()) {
        error = "neplatná struktura";
        return false;
    }
    if (oy < 1 || oy + s.sy > kChunkHeight) {
        error = "struktura se nevejde do výšky světa";
        return false;
    }
    // Check first, change afterwards: never leave half a structure behind.
    for (int cz = floorDiv(oz, kChunkSize); cz <= floorDiv(oz + s.sz - 1, kChunkSize); ++cz)
        for (int cx = floorDiv(ox, kChunkSize); cx <= floorDiv(ox + s.sx - 1, kChunkSize); ++cx)
            if (!world.getChunk(cx, cz)) {
                error = "část místa ještě není načtená, přibliž se";
                return false;
            }
    for (int y = 0; y < s.sy; ++y)
        for (int z = 0; z < s.sz; ++z)
            for (int x = 0; x < s.sx; ++x) {
                ItemId id = s.at(x, y, z);
                if (id == Structure::kVoid) continue;
                int wx = ox + x, wy = oy + y, wz = oz + z;
                ItemId before = world.getBlock(wx, wy, wz);
                if (before == BEDROCK || before == WORLD_END || before == id) continue;
                world.setBlock(wx, wy, wz, id);
                undo.push_back({wx, wy, wz, before, id});
            }
    return true;
}

void undoPlacement(World& world, const std::vector<BlockChange>& undo) {
    for (auto it = undo.rbegin(); it != undo.rend(); ++it)
        if (world.getBlock(it->x, it->y, it->z) == it->after) world.setBlock(it->x, it->y, it->z, it->before);
}

namespace {

Structure box(const std::string& name, int sx, int sy, int sz) {
    Structure s;
    s.name = name;
    s.sx = sx;
    s.sy = sy;
    s.sz = sz;
    s.cells.assign((size_t)sx * sy * sz, Structure::kVoid);
    return s;
}

}  // namespace

std::vector<Structure> builtinStructures() {
    std::vector<Structure> out;
    {
        // A small wooden house: cobblestone floor, log corners, plank walls, glass windows and a door gap.
        Structure s = box("Dřevěný domek", 7, 7, 7);
        for (int y = 0; y < 7; ++y)
            for (int z = 0; z < 7; ++z)
                for (int x = 0; x < 7; ++x) {
                    bool edgeX = x == 0 || x == 6, edgeZ = z == 0 || z == 6;
                    ItemId& c = s.at(x, y, z);
                    if (y == 0) c = COBBLESTONE;
                    else if (y <= 3) {
                        if (edgeX && edgeZ) c = LOG;
                        else if (edgeX || edgeZ) c = (y == 2 && (x == 3 || z == 3)) ? GLASS : PLANKS;
                        else c = AIR;
                    } else {
                        // Stepped roof: each layer one block smaller.
                        int inset = y - 4;
                        bool inside = x >= inset && x <= 6 - inset && z >= inset && z <= 6 - inset;
                        if (inside && (y == 6 || x == inset || x == 6 - inset || z == inset || z == 6 - inset)) c = BRICKS;
                        else if (inside) c = AIR;
                    }
                }
        s.at(3, 1, 0) = s.at(3, 2, 0) = AIR;  // door on the near side
        s.at(5, 1, 5) = CRAFTING_TABLE;
        s.at(1, 1, 5) = FURNACE;
        out.push_back(std::move(s));
    }
    {
        // Watchtower: hollow cobblestone shaft with a crenellated top.
        Structure s = box("Strážní věž", 5, 12, 5);
        for (int y = 0; y < 12; ++y)
            for (int z = 0; z < 5; ++z)
                for (int x = 0; x < 5; ++x) {
                    bool wall = x == 0 || x == 4 || z == 0 || z == 4;
                    ItemId& c = s.at(x, y, z);
                    if (y == 0 || y == 9) c = y == 9 && x == 2 && z == 2 ? AIR : COBBLESTONE;
                    else if (y < 9) c = wall ? ((y % 4 == 2 && (x == 2 || z == 2)) ? GLASS : COBBLESTONE) : AIR;
                    else if (y == 10) c = wall && (x + z) % 2 == 0 ? COBBLESTONE : AIR;
                    else c = AIR;
                }
        s.at(2, 1, 0) = s.at(2, 2, 0) = AIR;
        out.push_back(std::move(s));
    }
    {
        // Village well.
        Structure s = box("Studna", 5, 5, 5);
        for (int y = 0; y < 5; ++y)
            for (int z = 0; z < 5; ++z)
                for (int x = 0; x < 5; ++x) {
                    bool rim = x == 0 || x == 4 || z == 0 || z == 4;
                    bool corner = (x == 0 || x == 4) && (z == 0 || z == 4);
                    ItemId& c = s.at(x, y, z);
                    if (y == 0) c = rim ? COBBLESTONE : WATER;
                    else if (y == 1) c = rim ? COBBLESTONE : AIR;
                    else if (y < 4) c = corner ? LOG : AIR;
                    else c = PLANKS;
                }
        out.push_back(std::move(s));
    }
    return out;
}

bool loadStructureFile(const std::string& path, Structure& out, ImportReport& report, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "soubor nejde otevřít";
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool mc = path.size() > 12 && path.compare(path.size() - 12, 12, ".mcstructure") == 0;
    bool ok = mc ? importMcStructure(data, out, report, error) : readVcStructure(data, out, error);
    if (ok) {
        std::string file = path.substr(path.find_last_of('/') + 1);
        if (mc || out.name.empty()) out.name = file.substr(0, file.find_last_of('.'));
    }
    return ok;
}

}  // namespace vc
