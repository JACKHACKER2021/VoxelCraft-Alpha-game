#include "McWorld.h"

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "Lang.h"
#include "Nbt.h"
#include "Save.h"
#include "Structure.h"
#include "World.h"

namespace vc {

namespace fs = std::filesystem;

namespace {

uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

// zlib (raw = false) or raw deflate (raw = true), into `out`; false when broken.
bool inflateAll(const uint8_t* in, size_t n, bool raw, std::vector<uint8_t>& out, size_t limit = 512u << 20) {
    z_stream z{};
    if (inflateInit2(&z, raw ? -15 : 15) != Z_OK) return false;
    z.next_in = const_cast<Bytef*>(in);
    z.avail_in = (uInt)n;
    out.clear();
    uint8_t buf[65536];
    int r = Z_OK;
    while (r != Z_STREAM_END) {
        z.next_out = buf;
        z.avail_out = sizeof(buf);
        r = inflate(&z, Z_NO_FLUSH);
        if (r != Z_OK && r != Z_STREAM_END) break;
        out.insert(out.end(), buf, buf + (sizeof(buf) - z.avail_out));
        if (out.size() > limit || (r == Z_OK && z.avail_in == 0 && z.avail_out != 0)) break;
    }
    inflateEnd(&z);
    return r == Z_STREAM_END;
}

// ------------------------------------------------------------------------ the zip

bool readZip(const std::vector<uint8_t>& d, std::map<std::string, std::vector<uint8_t>>& files, std::string& error) {
    size_t end = std::string::npos;
    for (size_t p = d.size() >= 22 ? d.size() - 22 : 0;; --p) {
        if (le32(&d[p]) == 0x06054b50) {
            end = p;
            break;
        }
        if (p == 0 || d.size() - p > 70000) break;
    }
    if (d.size() < 22 || end == std::string::npos) {
        error = "Není to soubor .mcworld (zip)";
        return false;
    }
    const uint32_t count = le16(&d[end + 10]);
    size_t at = le32(&d[end + 16]);
    for (uint32_t i = 0; i < count; ++i) {
        if (at + 46 > d.size() || le32(&d[at]) != 0x02014b50) break;
        const uint32_t method = le16(&d[at + 10]), csize = le32(&d[at + 20]), usize = le32(&d[at + 24]);
        const uint32_t nameLen = le16(&d[at + 28]), extra = le16(&d[at + 30]), comment = le16(&d[at + 32]);
        const uint32_t local = le32(&d[at + 42]);
        if (at + 46 + nameLen > d.size()) break;
        std::string name(d.begin() + (long)at + 46, d.begin() + (long)(at + 46 + nameLen));
        at += 46 + nameLen + extra + comment;
        if (local + 30 > d.size() || le32(&d[local]) != 0x04034b50) continue;
        const size_t data = local + 30 + le16(&d[local + 26]) + le16(&d[local + 28]);
        if (data + csize > d.size()) continue;
        // Only what we need: the database, level.dat and the name.
        std::string base = name.substr(name.find_last_of('/') == std::string::npos ? 0 : name.find_last_of('/') + 1);
        const bool db = name.find("db/") != std::string::npos;
        if (!(db || base == "level.dat" || base == "levelname.txt") || base.empty()) continue;
        std::vector<uint8_t> out;
        if (method == 0) out.assign(d.begin() + (long)data, d.begin() + (long)(data + csize));
        else if (method != 8 || !inflateAll(&d[data], csize, true, out) || out.size() != usize) continue;
        files[db ? "db/" + base : base] = std::move(out);
    }
    return true;
}

// ------------------------------------------------------------------------ LevelDB

bool varint(const std::vector<uint8_t>& d, size_t& p, size_t end, uint64_t& v) {
    v = 0;
    for (int shift = 0; shift < 64 && p < end; shift += 7) {
        const uint8_t b = d[p++];
        v |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

struct Db {
    // user key -> (sequence, value; empty optional = deleted)
    struct Entry {
        uint64_t seq = 0;
        bool deleted = false;
        std::vector<uint8_t> value;
    };
    std::map<std::string, Entry> entries;

    void put(std::string key, uint64_t seq, bool deleted, const uint8_t* v, size_t n) {
        auto it = entries.find(key);
        if (it != entries.end() && it->second.seq > seq) return;
        Entry& e = entries[std::move(key)];
        e.seq = seq;
        e.deleted = deleted;
        if (deleted) e.value.clear();
        else e.value.assign(v, v + n);
    }
};

// One table block, decompressed as its trailer says.
bool tableBlock(const std::vector<uint8_t>& f, uint64_t off, uint64_t size, std::vector<uint8_t>& out) {
    if (off + size + 5 > f.size()) return false;
    const uint8_t type = f[off + size];
    if (type == 0) {
        out.assign(f.begin() + (long)off, f.begin() + (long)(off + size));
        return true;
    }
    if (type == 2) return inflateAll(&f[off], size, false, out);  // zlib
    if (type == 4) return inflateAll(&f[off], size, true, out);   // raw deflate (Bedrock)
    return false;                                                  // snappy: not used by Bedrock
}

// Calls fn(key, value) for every entry of a block (prefix-compressed, restart array at the end).
template <typename Fn>
bool forEachInBlock(const std::vector<uint8_t>& b, Fn fn) {
    if (b.size() < 4) return false;
    const uint32_t restarts = le32(&b[b.size() - 4]);
    if ((uint64_t)restarts * 4 + 4 > b.size()) return false;
    const size_t end = b.size() - 4 - (size_t)restarts * 4;
    size_t p = 0;
    std::string key;
    while (p < end) {
        uint64_t shared, unshared, vlen;
        if (!varint(b, p, end, shared) || !varint(b, p, end, unshared) || !varint(b, p, end, vlen)) return false;
        if (shared > key.size() || p + unshared + vlen > end) return false;
        key.resize(shared);
        key.append((const char*)&b[p], unshared);
        p += unshared;
        fn(key, &b[p], (size_t)vlen);
        p += vlen;
    }
    return true;
}

bool readTable(const std::vector<uint8_t>& f, Db& db) {
    if (f.size() < 48) return false;
    const size_t foot = f.size() - 48;
    if (le32(&f[f.size() - 8]) != 0x8b80fb57u || le32(&f[f.size() - 4]) != 0xdb477524u) return false;
    size_t p = foot;
    uint64_t moff, msize, ioff, isize;
    if (!varint(f, p, f.size(), moff) || !varint(f, p, f.size(), msize) || !varint(f, p, f.size(), ioff) ||
        !varint(f, p, f.size(), isize))
        return false;
    std::vector<uint8_t> index;
    if (!tableBlock(f, ioff, isize, index)) return false;
    std::vector<std::pair<uint64_t, uint64_t>> handles;
    forEachInBlock(index, [&](const std::string&, const uint8_t* v, size_t n) {
        std::vector<uint8_t> h(v, v + n);
        size_t q = 0;
        uint64_t o, s;
        if (varint(h, q, h.size(), o) && varint(h, q, h.size(), s)) handles.push_back({o, s});
    });
    std::vector<uint8_t> block;
    for (auto [o, s] : handles) {
        if (!tableBlock(f, o, s, block)) continue;
        forEachInBlock(block, [&](const std::string& ikey, const uint8_t* v, size_t n) {
            if (ikey.size() < 8) return;
            uint64_t tag = 0;
            for (int i = 0; i < 8; ++i) tag |= (uint64_t)(uint8_t)ikey[ikey.size() - 8 + i] << (8 * i);
            db.put(ikey.substr(0, ikey.size() - 8), tag >> 8, (tag & 0xff) == 0, v, n);
        });
    }
    return true;
}

void readLog(const std::vector<uint8_t>& f, Db& db) {
    // 32 KiB blocks of records: crc(4) length(2) type(1) data; FULL=1 FIRST=2 MIDDLE=3 LAST=4.
    std::vector<uint8_t> rec;
    auto batch = [&](const std::vector<uint8_t>& b) {
        if (b.size() < 12) return;
        uint64_t seq = 0;
        for (int i = 0; i < 8; ++i) seq |= (uint64_t)b[i] << (8 * i);
        const uint32_t count = le32(&b[8]);
        size_t p = 12;
        for (uint32_t i = 0; i < count && p < b.size(); ++i) {
            const uint8_t type = b[p++];
            uint64_t klen, vlen = 0;
            if (!varint(b, p, b.size(), klen) || p + klen > b.size()) return;
            std::string key((const char*)&b[p], klen);
            p += klen;
            if (type == 1) {
                if (!varint(b, p, b.size(), vlen) || p + vlen > b.size()) return;
                db.put(std::move(key), seq + i, false, &b[p], vlen);
                p += vlen;
            } else {
                db.put(std::move(key), seq + i, true, nullptr, 0);
            }
        }
    };
    size_t p = 0;
    while (p + 7 <= f.size()) {
        const size_t left = 32768 - p % 32768;
        if (left < 7) {
            p += left;
            continue;
        }
        const uint16_t len = le16(&f[p + 4]);
        const uint8_t type = f[p + 6];
        if (p + 7 + len > f.size() || type == 0) {
            p += left;  // padding or a torn record
            continue;
        }
        const uint8_t* data = &f[p + 7];
        if (type == 1) {
            rec.assign(data, data + len);
            batch(rec);
        } else if (type == 2) {
            rec.assign(data, data + len);
        } else {
            rec.insert(rec.end(), data, data + len);
            if (type == 4) batch(rec);
        }
        p += 7 + len;
    }
}

// ------------------------------------------------------------------------ sub-chunks

struct Palette {
    std::vector<ItemId> ids;
};

// Layer 0 of a sub-chunk into blocks[4096] (index (x << 8) | (z << 4) | y). False when unreadable.
bool decodeSubChunk(const std::vector<uint8_t>& v, std::vector<ItemId>& blocks, McWorldReport& report,
                    std::map<std::string, std::pair<ItemId, bool>>& cache) {
    if (v.size() < 2) return false;
    const uint8_t version = v[0];
    size_t p = 1;
    int storages = 1;
    if (version == 8 || version == 9) {
        storages = v[p++];
        if (version == 9) ++p;  // the sub-chunk's own y index
    } else if (version != 1) {
        return false;  // the very old formats (0, 2-7) are not supported
    }
    if (storages < 1) return false;
    if (p >= v.size()) return false;
    const uint8_t head = v[p++];
    if (head & 1) return false;  // runtime ids: not used in saved worlds
    const int bits = head >> 1;
    std::vector<uint32_t> idx(4096, 0);
    if (bits > 0) {
        if (bits > 16) return false;
        const int perWord = 32 / bits, words = (4096 + perWord - 1) / perWord;
        if (p + (size_t)words * 4 > v.size()) return false;
        const uint32_t mask = (1u << bits) - 1;
        for (int i = 0; i < 4096; ++i) {
            const uint32_t w = le32(&v[p + (size_t)(i / perWord) * 4]);
            idx[i] = (w >> ((i % perWord) * bits)) & mask;
        }
        p += (size_t)words * 4;
    }
    uint32_t count = 1;
    if (bits > 0) {
        if (p + 4 > v.size()) return false;
        count = le32(&v[p]);
        p += 4;
    }
    if (count == 0 || count > 4096) return false;
    std::vector<ItemId> pal(count, AIR);
    std::vector<const std::string*> inexact(count, nullptr);
    std::string err;
    for (uint32_t i = 0; i < count; ++i) {
        Nbt entry;
        if (!Nbt::parseAt(v, p, entry, err)) return false;
        const Nbt* name = entry.get("name");
        std::string n = name ? name->text : "minecraft:air";
        if (n.rfind("minecraft:", 0) == 0) n = n.substr(10);
        // Bedrock's older names carry the colour or kind in the states.
        if (const Nbt* states = entry.get("states")) {
            if (const Nbt* c = states->get("color"); c && c->type == Nbt::String && (n == "concrete" || n == "wool" || n == "concrete_powder"))
                n = (c->text == "silver" ? "light_gray" : c->text) + "_" + n;
        }
        auto it = cache.find(n);
        if (it == cache.end()) {
            bool exact = true;
            const ItemId id = mapMcWorldBlock(n, exact);
            it = cache.emplace(n, std::make_pair(id, exact)).first;
        }
        ItemId id = it->second.first;
        // 2.0.0: stairs and slabs keep their direction and half, so buildings line up as they did.
        if (const Nbt* states = entry.get("states")) {
            auto stateInt = [&](const char* k, int def) {
                const Nbt* v = states->get(k);
                return v && v->type != Nbt::String ? (int)v->integer : def;
            };
            auto stateStr = [&](const char* k) {
                const Nbt* v = states->get(k);
                return v && v->type == Nbt::String ? v->text : std::string();
            };
            if (isStairs(id)) {
                // Bedrock: 0 east, 1 west, 2 south, 3 north (the high side); ours: 0 north, 1 east, 2 south, 3 west.
                static const int kFacing[4] = {1, 3, 2, 0};
                const int dir = stateInt("weirdo_direction", -1);
                if (dir >= 0 && dir < 4) id = stairsId(stairsMaterial(id), kFacing[dir], stateInt("upside_down_bit", 0) != 0);
            } else if (isSlab(id) && slabHalf(id) != 2) {
                const bool top = stateStr("minecraft:vertical_half") == "top" || stateInt("top_slot_bit", 0) != 0;
                id = slabId(slabMaterial(id), top ? 1 : 0);
            }
        }
        pal[i] = id;
        if (!it->second.second) inexact[i] = &it->first;
    }
    for (int i = 0; i < 4096; ++i) {
        const uint32_t k = idx[i] < count ? idx[i] : 0;
        blocks[i] = idx[i] < count ? pal[k] : (ItemId)AIR;
        if (inexact[k]) ++report.replaced[*inexact[k]];
    }
    return true;
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

ItemId mapMcWorldBlock(const std::string& raw, bool& exact) {
    exact = true;
    std::string n = raw;
    auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
    auto key = [](const char* k) { return itemFromKey(k); };
    auto ends = [&](const char* s) {
        const size_t l = std::strlen(s);
        return n.size() >= l && n.compare(n.size() - l, l, s) == 0;
    };
    if (n == "air" || n == "cave_air" || n == "void_air" || n == "light_block" || n == "structure_void") return AIR;
    // The player's own choices (2.0.0 converter).
    if (has("deepslate")) {
        if (ends("_ore")) {
            std::string ore = n.substr(n.find("deepslate_") + 10);
            n = ore;  // deepslate_iron_ore -> iron_ore, below
        } else if (has("brick") || has("tile")) {
            return DEEPSTONE_BRICKS;
        } else {
            return DEEPSTONE;
        }
    }
    if (n == "redstone_wire" || n == "unpowered_repeater" || n == "powered_repeater" || n == "unpowered_comparator" ||
        n == "powered_comparator")
        return POWER_CABLE;
    if (n == "redstone_block") return POWERSTONE_BLOCK;
    if (n == "redstone_ore" || n == "lit_redstone_ore") return POWERSTONE_ORE;
    if (n == "redstone_lamp") return POWERSTONE_LAMP;
    if (n == "lit_redstone_lamp") return POWERSTONE_LAMP_ON;
    if (n == "redstone_torch" || n == "unlit_redstone_torch") return TORCH;
    if (n == "lever") return SWITCH;
    if (n == "copper_ore") return BRONZE_ORE;
    if (has("copper") && !has("torch") && !has("lantern") && !has("bulb") && !has("door") && !has("grate") &&
        !has("chain") && !has("bars") && !ends("slab") && !ends("stairs"))
        return BRONZE_BLOCK;
    if (n == "soul_sand" || n == "soul_soil") return NETHERRACK;  // the Cornerstone block
    if (n == "magma") return CRIMSON_STONE;                       // solid, warm, does not flow
    if (n == "iron_bars") return key("oak_slab");
    if (n == "crimson_stem" || n == "crimson_hyphae" || n == "stripped_crimson_stem" || n == "stripped_crimson_hyphae")
        return NETHERPERK_LOG;
    if (n == "crimson_planks") return NETHERPERK_PLANKS;
    if (n == "warped_stem" || n == "warped_hyphae" || n == "stripped_warped_stem" || n == "stripped_warped_hyphae")
        return CRIMSON_LOG;
    if (n == "warped_planks") return CRIMSON_PLANKS;
    if (n == "crimson_nylium" || n == "warped_nylium") return NETHER_GRASS;
    if (n == "nether_wart_block" || n == "warped_wart_block") return NETHERPERK_CAP;
    if (n == "coarse_dirt" || n == "rooted_dirt" || n == "dirt_with_roots" || n == "mud" || n == "packed_mud") return DIRT;
    if (n == "podzol" || n == "mycelium" || n == "dirt_path" || n == "grass_path") return GRASS;
    if (n == "brick_block" || n == "bricks") return BRICKS;
    if (n == "water" || n == "flowing_water") return WATER;
    if (n == "lava" || n == "flowing_lava") return LAVA;
    if (n == "command_block" || n == "repeating_command_block" || n == "chain_command_block") return COMMAND_BLOCK;
    if (has("quartz") && !ends("_ore")) return key("white_concrete");
    if (n == "glowstone" || n == "shroomlight" || n == "sea_lantern") return POWERSTONE_LIGHT;
    if (n == "end_stone" || n == "end_bricks") return key("void_stone");
    if (n == "tuff" || n == "calcite" || n == "andesite" || n == "diorite" || n == "granite" || n == "polished_andesite" ||
        n == "polished_diorite" || n == "polished_granite" || n == "stone" || n == "smooth_stone")
        return STONE;
    if (n == "basalt" || n == "polished_basalt" || n == "smooth_basalt" || n == "blackstone" || n == "polished_blackstone")
        return key("void_stone");
    if (n == "terracotta" || n == "hardened_clay" || ends("_terracotta") || n == "stained_hardened_clay") return HARDENED_STONE;
    if (n == "packed_ice" || n == "blue_ice" || n == "frosted_ice") return ICE;
    if (n == "powder_snow") return SNOW_BLOCK;
    if (n == "kelp" || n == "seagrass" || n == "bubble_column") return WATER;
    if (n == "chest" || n == "trapped_chest" || n == "barrel" || ends("shulker_box")) return CHEST;
    if (n == "iron_block") return STEEL_BLOCK;
    if (n == "lapis_block") return key("blue_concrete");
    if (n == "gold_block" || n == "raw_gold_block") return key("gold_block") ? key("gold_block") : key("yellow_concrete");
    if (n == "ancient_debris" || n == "nether_gold_ore") return NETHERRACK;
    if (has("blackstone") && (has("brick") || has("polished"))) return key("nether_brick");
    if (n == "dried_kelp_block") return key("black_concrete");
    if (n == "noteblock" || n == "jukebox") return PLANKS;
    if (has("pale_oak") && (has("planks"))) return PLANKS;
    if (has("pale_oak") && (has("log") || has("wood"))) return LOG;
    // Doors are two blocks with a direction in VoxelCraft: an open doorway is closer than a wall.
    if (ends("door") && !ends("trapdoor")) return AIR;
    if (ends("trapdoor")) return key("oak_trapdoor");
    if (n == "tinted_glass") return GLASS;
    if (n == "portal") return AIR;
    // Stairs, slabs and fences of materials we lack: the nearest of ours.
    if (ends("_stairs") || ends("_slab") || ends("double_slab") || ends("_fence") || ends("_wall")) {
        const bool wood = has("oak") || has("spruce") || has("birch") || has("jungle") || has("acacia") || has("mangrove") ||
                          has("cherry") || has("bamboo") || has("wooden");
        const bool warped = has("warped"), crimson = has("crimson");
        const bool nether = has("nether") || has("blackstone");
        const bool brick = has("brick");
        const bool copper = has("copper");
        std::string base = warped ? "crimson" : crimson ? "netherperk" : wood ? "oak" : nether ? "nether_brick" : copper ? "brick" :
                           brick ? "brick" : has("deepslate") ? "deepstone" : has("sandstone") ? "sandstone" : "stone";
        if (ends("double_slab")) {
            const ItemId full = base == "oak" ? PLANKS : base == "crimson" ? CRIMSON_PLANKS : base == "netherperk" ? NETHERPERK_PLANKS :
                                base == "nether_brick" ? key("nether_brick") : base == "brick" ? BRICKS : base == "deepstone" ? DEEPSTONE :
                                base == "sandstone" ? SANDSTONE : STONE;
            if (full != AIR) return full;
        }
        if (ends("_stairs")) {
            if (const ItemId st = key((base + "_stairs").c_str()); st != AIR) return st;
        } else if (ends("_slab")) {
            if (const ItemId sl = key((base + "_slab").c_str()); sl != AIR) return sl;
        } else if (ends("_fence")) {
            if (const ItemId f = key((base + "_fence").c_str()); f != AIR) return f;
            return key("oak_fence");
        } else {
            return base == "nether_brick" ? key("nether_brick") : COBBLESTONE;
        }
    }
    if (n == "crafting_table") return CRAFTING_TABLE;
    if (n == "furnace" || n == "lit_furnace" || n == "blast_furnace" || n == "smoker" || n == "lit_blast_furnace" || n == "lit_smoker")
        return FURNACE;
    // A block VoxelCraft has under the same name (spruce planks, concrete colours...).
    if (const ItemId same = itemFromKey(n); same != AIR && isPlaceable(same) && same != PORTAL) return same;
    // Everything else: the mapping of the .mcstructure importer (oak for the other woods...).
    bool known = false;
    const ItemId id = mapBedrockBlock("minecraft:" + n, known);
    if (known && id != Structure::kVoid) return id;
    exact = false;
    // Not in VoxelCraft: plants, flowers and small things go away, solid blocks become stone.
    for (const char* soft : {"flower", "tulip", "orchid", "daisy", "allium", "bluet", "lily", "fern", "bush", "grass",
                             "vine", "sapling", "mushroom", "roots", "sprouts", "carpet", "button", "pressure_plate",
                             "sign", "banner", "rail", "torch", "lantern", "candle", "coral", "pickle", "berry",
                             "dripleaf", "azalea", "moss_carpet", "lichen", "web", "string", "head", "skull", "pot",
                             "frame", "chain", "rod", "ladder", "scaffolding", "pane", "fire", "petal", "egg", "amethyst_bud",
                             "spore", "tripwire", "hook", "bell", "sculk_vein"})
        if (has(soft)) return AIR;
    if (has("glass")) return GLASS;
    if (has("wool")) return WOOL;
    if (has("leaves")) return LEAVES;
    if (has("log") || has("wood") || has("stem")) return LOG;
    if (has("planks") || has("fence") || has("door") || has("trapdoor")) return PLANKS;
    if (has("sand")) return SAND;
    if (has("ice")) return ICE;
    return STONE;
}

bool readLevelDb(const std::map<std::string, std::vector<uint8_t>>& files, std::map<std::string, std::vector<uint8_t>>& out,
                 std::string& error) {
    Db db;
    // Tables first (older), in file number order, then the logs (newest).
    std::vector<std::string> tables, logs;
    for (const auto& [name, data] : files) {
        if (name.size() > 4 && (name.compare(name.size() - 4, 4, ".ldb") == 0 || name.compare(name.size() - 4, 4, ".sst") == 0))
            tables.push_back(name);
        else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".log") == 0)
            logs.push_back(name);
    }
    if (tables.empty() && logs.empty()) {
        error = "Ve světě chybí databáze (db/*.ldb)";
        return false;
    }
    for (const std::string& t : tables) readTable(files.at(t), db);
    for (const std::string& l : logs) readLog(files.at(l), db);
    for (auto& [k, e] : db.entries)
        if (!e.deleted) out[k] = std::move(e.value);
    return true;
}

bool importMcWorld(const std::string& file, const std::string& worldsRoot, std::string& folder, McWorldReport& report,
                   std::string& error) {
    std::error_code ec;
    const auto fileSize = fs::file_size(file, ec);
    if (ec || fileSize == 0) {
        error = "Soubor nejde přečíst";
        return false;
    }
    if (fileSize > (600ull << 20)) {
        error = "Svět je příliš velký (víc než 600 MB)";
        return false;
    }
    std::map<std::string, std::vector<uint8_t>> files;
    {
        const std::vector<uint8_t> zip = readFile(file);
        if (!readZip(zip, files, error)) return false;
    }
    std::map<std::string, std::vector<uint8_t>> dbFiles;
    for (auto it = files.begin(); it != files.end();) {
        if (it->first.rfind("db/", 0) == 0) {
            dbFiles[it->first.substr(3)] = std::move(it->second);
            it = files.erase(it);
        } else {
            ++it;
        }
    }
    std::map<std::string, std::vector<uint8_t>> kv;
    if (!readLevelDb(dbFiles, kv, error)) return false;
    dbFiles.clear();

    // The name and the spawn from level.dat (an 8-byte header, then little-endian NBT).
    report.name = "Bedrock svět";
    if (auto it = files.find("levelname.txt"); it != files.end() && !it->second.empty())
        report.name = std::string(it->second.begin(), it->second.end()).substr(0, 40);
    int spawnX = 0, spawnY = 32767, spawnZ = 0;
    uint32_t seed = 1;
    bool flatWorld = false;
    if (auto it = files.find("level.dat"); it != files.end() && it->second.size() > 8) {
        std::vector<uint8_t> nbt(it->second.begin() + 8, it->second.end());
        Nbt root;
        std::string e2;
        if (Nbt::parse(nbt, root, e2)) {
            if (const Nbt* v = root.get("SpawnX")) spawnX = (int)v->integer;
            if (const Nbt* v = root.get("SpawnY")) spawnY = (int)v->integer;
            if (const Nbt* v = root.get("SpawnZ")) spawnZ = (int)v->integer;
            if (const Nbt* v = root.get("RandomSeed")) seed = (uint32_t)v->integer;
            if (const Nbt* v = root.get("Generator")) flatWorld = v->integer == 2;
            if (const Nbt* v = root.get("LevelName"); v && !v->text.empty() && files.find("levelname.txt") == files.end())
                report.name = v->text.substr(0, 40);
        }
    }
    files.clear();

    // Sub-chunk keys: x(4) z(4) [dimension(4)] tag 0x2f, y index (signed byte).
    std::map<std::tuple<int, int, int>, std::vector<std::pair<int, const std::vector<uint8_t>*>>> columns;
    bool negative = false;
    for (const auto& [k, v] : kv) {
        if (k.size() != 10 && k.size() != 14) continue;
        const uint8_t* p = (const uint8_t*)k.data();
        const bool withDim = k.size() == 14;
        if (p[withDim ? 12 : 8] != 0x2f) continue;
        const int x = (int)le32(p), z = (int)le32(p + 4), dim = withDim ? (int)le32(p + 8) : 0;
        const int y = (int8_t)p[withDim ? 13 : 9];
        if (dim < 0 || dim > 2) continue;
        if (dim == 0 && y < 0) negative = true;
        columns[{dim, x, z}].push_back({y, &v});
    }
    if (columns.empty()) {
        error = "Ve světě nejsou žádné chunky, které umíme přečíst (příliš stará verze?)";
        return false;
    }
    // A 1.18+ world reaches y -64: shifted up so y -56..71 fits our 128 blocks.
    // A flat world keeps its ground at the bottom: shifted by 64, its bedrock, dirt and grass
    // (y -64..-61) land on y 0..3, exactly where VoxelCraft's own flat world has them.
    report.yOffset = negative ? (flatWorld ? 64 : 56) : 0;

    folder = createWorldFolder(worldsRoot);
    if (folder.empty()) {
        error = "Nepodařilo se vytvořit složku světa";
        return false;
    }
    GenConfig config = GenConfig::defaults();
    if (flatWorld) {
        // Around the imported chunks: VoxelCraft's classic flat world, at the same height.
        config.flat = true;
        config.flatPreset = 0;
    }
    saveGenerator(folder, config);
    std::map<std::string, std::pair<ItemId, bool>> cache;
    std::vector<ItemId> sub(4096);
    for (int dim = 0; dim < 3; ++dim) {
        const Dimension vd = dim == 0 ? Dimension::Overworld : dim == 1 ? Dimension::Nether : Dimension::Void;
        World world(seed, vd, config);
        world.setStorageDir(folder + (dim == 0 ? "/overworld" : dim == 1 ? "/nether" : "/void"));
        const int offset = dim == 0 ? report.yOffset : 0;
        int done = 0;
        for (const auto& [col, subs] : columns) {
            if (std::get<0>(col) != dim) continue;
            const int cx = std::get<1>(col), cz = std::get<2>(col);
            Chunk* c = world.getChunk(cx, cz);
            if (!c) c = world.generateChunk(cx, cz);
            if (!c) continue;
            std::fill(std::begin(c->blocks), std::end(c->blocks), AIR);
            for (const auto& [sy, value] : subs) {
                if (!decodeSubChunk(*value, sub, report, cache)) continue;
                ++report.subchunks;
                for (int i = 0; i < 4096; ++i) {
                    const ItemId id = sub[i];
                    if (id == AIR) continue;
                    const int x = (i >> 8) & 15, z = (i >> 4) & 15, y = sy * 16 + (i & 15) + offset;
                    if (y < 0 || y >= kChunkHeight) {
                        ++report.cutOff;
                        continue;
                    }
                    c->set(x, y, z, id);
                    ++report.blocks;
                }
            }
            // The floor: bedrock, so nobody falls out where the world was cut.
            for (int x = 0; x < kChunkSize; ++x)
                for (int z = 0; z < kChunkSize; ++z)
                    if (!flatWorld || c->get(x, 0, z) == AIR) c->set(x, 0, z, BEDROCK);
            c->recalcHeightmap();
            c->modified = true;
            c->dirty = true;
            ++report.chunks;
            if (++done % 64 == 0) world.unloadFarChunks(cx + 100000, cz, 1);  // saves them and frees memory
        }
        world.saveModifiedChunks();
    }

    WorldInfo info;
    info.folder = folder;
    info.name = report.name;
    info.seed = seed;
    info.mode = GameMode::Creative;
    info.lastPlayed = 0;
    PlayerState player;
    player.valid = true;
    // The spawn: the top of the column at the world spawn (or 0, 0).
    int sy = spawnY == 32767 ? -1 : spawnY + report.yOffset;
    {
        World probe(seed, Dimension::Overworld, config);
        probe.setStorageDir(folder + "/overworld");
        const int scx = (int)std::floor(spawnX / 16.0), scz = (int)std::floor(spawnZ / 16.0);
        Chunk* c = probe.generateChunk(scx, scz);
        if (c && (sy < 1 || sy >= kChunkHeight - 2)) sy = c->heightmap[((spawnX & 15) << 4) | (spawnZ & 15)];
        if (c && sy < 1) sy = 70;
    }
    player.pos = player.spawn = Vec3{spawnX + 0.5f, (float)std::min(sy + 1, kChunkHeight - 2), spawnZ + 0.5f};
    player.flying = true;
    if (!saveLevel(info, player)) {
        error = "Nepodařilo se uložit level.dat";
        return false;
    }
    return true;
}

}  // namespace vc
