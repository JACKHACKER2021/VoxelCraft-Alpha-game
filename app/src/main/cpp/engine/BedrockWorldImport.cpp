#include "BedrockWorldImport.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include <zlib.h>

#include "BlockShapes.h"
#include "GenConfig.h"
#include "Nbt.h"
#include "Save.h"
#include "World.h"

namespace vc {

namespace fs = std::filesystem;

namespace {

uint16_t get16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
uint32_t get32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

void put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back((uint8_t)v);
    b.push_back((uint8_t)(v >> 8));
}

void put32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)v);
    b.push_back((uint8_t)(v >> 8));
    b.push_back((uint8_t)(v >> 16));
    b.push_back((uint8_t)(v >> 24));
}

bool inflateRaw(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& dst) {
    z_stream strm{};
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;
    strm.next_in = const_cast<Bytef*>(src);
    strm.avail_in = (uInt)srcLen;
    uint8_t buf[16384];
    int ret;
    do {
        strm.next_out = buf;
        strm.avail_out = sizeof(buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            inflateEnd(&strm);
            return false;
        }
        size_t have = sizeof(buf) - strm.avail_out;
        dst.insert(dst.end(), buf, buf + have);
    } while (ret != Z_STREAM_END && strm.avail_in > 0);
    inflateEnd(&strm);
    return ret == Z_STREAM_END || ret == Z_OK;
}

struct ZipEntry {
    std::string name;
    const uint8_t* data = nullptr;
    size_t compSize = 0;
    size_t uncompSize = 0;
    uint16_t method = 0;
};

bool readZipEntries(const std::vector<uint8_t>& fileData, std::vector<ZipEntry>& out) {
    if (fileData.size() < 22) return false;
    size_t eocd = std::string::npos;
    for (size_t p = fileData.size() - 22; p + 22 <= fileData.size(); --p) {
        if (get32(&fileData[p]) == 0x06054b50) { eocd = p; break; }
        if (p == 0) break;
    }
    if (eocd == std::string::npos) return false;

    uint16_t count = get16(&fileData[eocd + 10]);
    size_t cdOffset = get32(&fileData[eocd + 16]);

    size_t pos = cdOffset;
    for (uint16_t i = 0; i < count; ++i) {
        if (pos + 46 > fileData.size() || get32(&fileData[pos]) != 0x02014b50) break;
        uint16_t method = get16(&fileData[pos + 10]);
        uint32_t compSz = get32(&fileData[pos + 20]);
        uint32_t uncompSz = get32(&fileData[pos + 24]);
        uint16_t nameLen = get16(&fileData[pos + 28]);
        uint16_t extraLen = get16(&fileData[pos + 30]);
        uint16_t commentLen = get16(&fileData[pos + 32]);
        uint32_t localOffset = get32(&fileData[pos + 42]);

        if (pos + 46 + nameLen > fileData.size()) break;
        std::string name(reinterpret_cast<const char*>(&fileData[pos + 46]), nameLen);
        pos += 46 + nameLen + extraLen + commentLen;

        if (localOffset + 30 > fileData.size() || get32(&fileData[localOffset]) != 0x04034b50) continue;
        uint16_t locNameLen = get16(&fileData[localOffset + 26]);
        uint16_t locExtraLen = get16(&fileData[localOffset + 28]);
        size_t dataOffset = localOffset + 30 + locNameLen + locExtraLen;

        if (dataOffset + compSz <= fileData.size()) {
            out.push_back({name, &fileData[dataOffset], compSz, uncompSz, method});
        }
    }
    return true;
}

bool extractZipFile(const ZipEntry& ze, std::vector<uint8_t>& out) {
    if (ze.method == 0) {
        out.assign(ze.data, ze.data + ze.compSize);
        return true;
    } else if (ze.method == 8) {
        out.reserve(ze.uncompSize > 0 ? ze.uncompSize : ze.compSize * 2);
        return inflateRaw(ze.data, ze.compSize, out);
    }
    return false;
}

uint64_t readVarint(const uint8_t* buf, size_t len, size_t& offset) {
    uint64_t res = 0;
    int shift = 0;
    while (offset < len) {
        uint8_t b = buf[offset++];
        res |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return res;
}

struct SubchunkRecord {
    int cx = 0, cy = 0, cz = 0;
    std::vector<uint8_t> data;
};

void parseLdbFile(const uint8_t* ldbData, size_t ldbLen, std::vector<SubchunkRecord>& subchunks) {
    if (ldbLen < 48) return;
    const uint8_t* footer = ldbData + (ldbLen - 48);
    size_t off = 0;
    readVarint(footer, 48, off); // meta offset
    readVarint(footer, 48, off); // meta size
    size_t indexOff = (size_t)readVarint(footer, 48, off);
    size_t indexSz = (size_t)readVarint(footer, 48, off);

    if (indexOff + indexSz > ldbLen) return;
    std::vector<uint8_t> indexData;
    if (!inflateRaw(ldbData + indexOff, indexSz, indexData)) return;
    if (indexData.size() < 4) return;

    uint32_t numRestarts = get32(&indexData[indexData.size() - 4]);
    if (indexData.size() < 4 + numRestarts * 4) return;
    size_t restartsOffset = indexData.size() - 4 - (numRestarts * 4);

    size_t pos = 0;
    std::string lastKey;
    std::vector<std::pair<size_t, size_t>> dataBlocks;

    while (pos < restartsOffset) {
        size_t shared = (size_t)readVarint(indexData.data(), indexData.size(), pos);
        size_t unshared = (size_t)readVarint(indexData.data(), indexData.size(), pos);
        size_t vlen = (size_t)readVarint(indexData.data(), indexData.size(), pos);
        if (pos + unshared + vlen > indexData.size()) break;

        std::string key = lastKey.substr(0, shared) + std::string((const char*)&indexData[pos], unshared);
        pos += unshared;
        const uint8_t* vdata = &indexData[pos];
        pos += vlen;

        size_t voff = 0;
        size_t boff = (size_t)readVarint(vdata, vlen, voff);
        size_t bsz = (size_t)readVarint(vdata, vlen, voff);
        dataBlocks.push_back({boff, bsz});
        lastKey = key;
    }

    for (const auto& [boff, bsz] : dataBlocks) {
        if (boff + bsz + 1 > ldbLen) continue;
        uint8_t ctype = ldbData[boff + bsz];
        if (ctype != 4 && ctype != 2) continue;

        std::vector<uint8_t> blockData;
        if (!inflateRaw(ldbData + boff, bsz, blockData)) continue;
        if (blockData.size() < 4) return;

        uint32_t bRestarts = get32(&blockData[blockData.size() - 4]);
        if (blockData.size() < 4 + bRestarts * 4) continue;
        size_t bRestartsOffset = blockData.size() - 4 - (bRestarts * 4);

        size_t bp = 0;
        std::string rLastKey;
        while (bp < bRestartsOffset) {
            size_t sh = (size_t)readVarint(blockData.data(), blockData.size(), bp);
            size_t un = (size_t)readVarint(blockData.data(), blockData.size(), bp);
            size_t vl = (size_t)readVarint(blockData.data(), blockData.size(), bp);
            if (bp + un + vl > blockData.size()) break;

            std::string k = rLastKey.substr(0, sh) + std::string((const char*)&blockData[bp], un);
            bp += un;
            const uint8_t* val = &blockData[bp];
            bp += vl;
            rLastKey = k;

            if (k.size() < 8) continue;
            std::string ukey = k.substr(0, k.size() - 8);

            // Overworld subchunk prefix tag is 0x2f (47)
            if ((ukey.size() == 9 || ukey.size() == 10) && (uint8_t)ukey[8] == 0x2f) {
                int cx = (int)get32((const uint8_t*)ukey.data());
                int cz = (int)get32((const uint8_t*)ukey.data() + 4);
                int cy = ukey.size() == 10 ? (int)(int8_t)ukey[9] : 0;
                subchunks.push_back({cx, cy, cz, std::vector<uint8_t>(val, val + vl)});
            }
        }
    }
}

std::vector<std::string> parseSubchunkPalette(const std::vector<uint8_t>& val) {
    std::vector<std::string> names;
    if (val.size() < 3) return names;
    uint8_t ver = val[0];
    size_t p = 2;
    if (ver == 9 && val.size() > p) p += 1;
    if (p >= val.size()) return names;

    uint8_t flag = val[p++];
    int bitsPerBlock = flag >> 1;
    if (bitsPerBlock > 0) {
        int bpw = 32 / bitsPerBlock;
        int words = (4096 + bpw - 1) / bpw;
        p += words * 4;
    }
    if (p + 4 > val.size()) return names;

    uint32_t palLen = get32(&val[p]);
    p += 4;
    if (palLen > 2000) return names;

    for (uint32_t i = 0; i < palLen && p < val.size(); ++i) {
        Nbt root;
        std::string err;
        std::vector<uint8_t> rest(val.begin() + (long)p, val.end());
        if (!Nbt::parse(rest, root, err)) break;

        const Nbt* target = &root;
        if (const Nbt* sub = root.get("")) target = sub;
        if (const Nbt* nameTag = target->get("name")) {
            names.push_back(nameTag->text);
        } else {
            names.push_back("minecraft:stone");
        }

        size_t nextComp = p + 1;
        while (nextComp + 2 < val.size()) {
            if (val[nextComp] == 0x0a && val[nextComp + 1] == 0x00 && val[nextComp + 2] == 0x00) break;
            nextComp++;
        }
        p = nextComp;
    }
    return names;
}

}  // namespace

ItemId defaultBedrockMapping(const std::string& name) {
    static const std::unordered_map<std::string, ItemId> kDefs = {
        {"minecraft:air", AIR},
        {"minecraft:cave_air", AIR},
        {"minecraft:barrier", BARRIER},
        {"minecraft:command_block", COMMAND_BLOCK},
        {"minecraft:structure_block", STRUCTURE_BLOCK},
        {"minecraft:stone", STONE},
        {"minecraft:granite", HARDENED_STONE},
        {"minecraft:diorite", PALE_STONE},
        {"minecraft:andesite", HARDENED_STONE},
        {"minecraft:grass_block", GRASS},
        {"minecraft:grass", GRASS},
        {"minecraft:dirt", DIRT},
        {"minecraft:coarse_dirt", DIRT},
        {"minecraft:podzol", DIRT},
        {"minecraft:cobblestone", COBBLESTONE},
        {"minecraft:mossy_cobblestone", MOSSY_COBBLESTONE},
        {"minecraft:planks", PLANKS},
        {"minecraft:oak_planks", PLANKS},
        {"minecraft:spruce_planks", SPRUCE_PLANKS},
        {"minecraft:birch_planks", PLANKS},
        {"minecraft:jungle_planks", PLANKS},
        {"minecraft:acacia_planks", PLANKS},
        {"minecraft:dark_oak_planks", SPRUCE_PLANKS},
        {"minecraft:crimson_planks", CRIMSON_PLANKS},
        {"minecraft:warped_planks", VOID_PLANKS},
        {"minecraft:bedrock", BEDROCK},
        {"minecraft:water", WATER},
        {"minecraft:flowing_water", WATER},
        {"minecraft:lava", LAVA},
        {"minecraft:flowing_lava", LAVA},
        {"minecraft:sand", SAND},
        {"minecraft:gravel", GRAVEL},
        {"minecraft:gold_ore", STEEL_ORE},
        {"minecraft:deepslate_gold_ore", STEEL_ORE},
        {"minecraft:iron_ore", IRON_ORE},
        {"minecraft:deepslate_iron_ore", IRON_ORE},
        {"minecraft:coal_ore", COAL_ORE},
        {"minecraft:deepslate_coal_ore", COAL_ORE},
        {"minecraft:log", LOG},
        {"minecraft:oak_log", LOG},
        {"minecraft:spruce_log", SPRUCE_LOG},
        {"minecraft:birch_log", LOG},
        {"minecraft:leaves", LEAVES},
        {"minecraft:oak_leaves", LEAVES},
        {"minecraft:spruce_leaves", SPRUCE_LEAVES},
        {"minecraft:birch_leaves", LEAVES},
        {"minecraft:glass", GLASS},
        {"minecraft:sandstone", SANDSTONE},
        {"minecraft:wool", WOOL},
        {"minecraft:bricks", BRICKS},
        {"minecraft:brick_block", BRICKS},
        {"minecraft:bookshelf", BOOKSHELF},
        {"minecraft:obsidian", OBSIDIAN},
        {"minecraft:torch", TORCH},
        {"minecraft:chest", CHEST},
        {"minecraft:crafting_table", CRAFTING_TABLE},
        {"minecraft:furnace", FURNACE},
        {"minecraft:lit_furnace", LIT_FURNACE},
        {"minecraft:diamond_ore", RUBY_ORE},
        {"minecraft:deepslate_diamond_ore", RUBY_ORE},
        {"minecraft:snow", SNOW_BLOCK},
        {"minecraft:ice", ICE},
        {"minecraft:cactus", CACTUS},
        {"minecraft:clay", CLAY},
        {"minecraft:netherrack", NETHERRACK},
        {"minecraft:stonebrick", STONE_BRICKS},
        {"minecraft:stone_bricks", STONE_BRICKS},
        {"minecraft:nether_brick", NETHER_BRICK},
        {"minecraft:quartz_block", PALE_STONE},
        {"minecraft:deepslate", DEEPSTONE},
        {"minecraft:cobbled_deepslate", DEEPSTONE},
        {"minecraft:deepslate_bricks", DEEPSTONE_BRICKS},
        {"minecraft:copper_block", BRONZE_BLOCK},
        {"minecraft:copper_ore", BRONZE_ORE},
        {"minecraft:waxed_exposed_copper", BRONZE_BLOCK},

        // Stairs
        {"minecraft:oak_stairs", stairsId(shapeMaterialOf(PLANKS), 0, false)},
        {"minecraft:wooden_stairs", stairsId(shapeMaterialOf(PLANKS), 0, false)},
        {"minecraft:spruce_stairs", stairsId(shapeMaterialOf(SPRUCE_PLANKS), 0, false)},
        {"minecraft:birch_stairs", stairsId(shapeMaterialOf(PLANKS), 0, false)},
        {"minecraft:jungle_stairs", stairsId(shapeMaterialOf(PLANKS), 0, false)},
        {"minecraft:acacia_stairs", stairsId(shapeMaterialOf(PLANKS), 0, false)},
        {"minecraft:dark_oak_stairs", stairsId(shapeMaterialOf(SPRUCE_PLANKS), 0, false)},
        {"minecraft:stone_stairs", stairsId(shapeMaterialOf(STONE), 0, false)},
        {"minecraft:cobblestone_stairs", stairsId(shapeMaterialOf(COBBLESTONE), 0, false)},
        {"minecraft:stone_brick_stairs", stairsId(shapeMaterialOf(STONE_BRICKS), 0, false)},
        {"minecraft:brick_stairs", stairsId(shapeMaterialOf(BRICKS), 0, false)},
        {"minecraft:sandstone_stairs", stairsId(shapeMaterialOf(SANDSTONE), 0, false)},
        {"minecraft:nether_brick_stairs", stairsId(shapeMaterialOf(NETHER_BRICK), 0, false)},
        {"minecraft:deepslate_stairs", stairsId(shapeMaterialOf(DEEPSTONE), 0, false)},
        {"minecraft:deepslate_brick_stairs", stairsId(shapeMaterialOf(DEEPSTONE_BRICKS), 0, false)},
        {"minecraft:polished_deepslate_stairs", stairsId(shapeMaterialOf(DEEPSTONE), 0, false)},
        {"minecraft:crimson_stairs", stairsId(shapeMaterialOf(CRIMSON_PLANKS), 0, false)},
        {"minecraft:warped_stairs", stairsId(shapeMaterialOf(VOID_PLANKS), 0, false)},

        // Slabs
        {"minecraft:wooden_slab", slabId(shapeMaterialOf(PLANKS), 0)},
        {"minecraft:oak_slab", slabId(shapeMaterialOf(PLANKS), 0)},
        {"minecraft:spruce_slab", slabId(shapeMaterialOf(SPRUCE_PLANKS), 0)},
        {"minecraft:birch_slab", slabId(shapeMaterialOf(PLANKS), 0)},
        {"minecraft:jungle_slab", slabId(shapeMaterialOf(PLANKS), 0)},
        {"minecraft:acacia_slab", slabId(shapeMaterialOf(PLANKS), 0)},
        {"minecraft:dark_oak_slab", slabId(shapeMaterialOf(SPRUCE_PLANKS), 0)},
        {"minecraft:stone_slab", slabId(shapeMaterialOf(STONE), 0)},
        {"minecraft:stone_block_slab", slabId(shapeMaterialOf(STONE), 0)},
        {"minecraft:stone_block_slab2", slabId(shapeMaterialOf(STONE_BRICKS), 0)},
        {"minecraft:stone_block_slab3", slabId(shapeMaterialOf(STONE_BRICKS), 0)},
        {"minecraft:stone_block_slab4", slabId(shapeMaterialOf(STONE_BRICKS), 0)},
        {"minecraft:cobblestone_slab", slabId(shapeMaterialOf(COBBLESTONE), 0)},
        {"minecraft:stone_brick_slab", slabId(shapeMaterialOf(STONE_BRICKS), 0)},
        {"minecraft:brick_slab", slabId(shapeMaterialOf(BRICKS), 0)},
        {"minecraft:sandstone_slab", slabId(shapeMaterialOf(SANDSTONE), 0)},
        {"minecraft:nether_brick_slab", slabId(shapeMaterialOf(NETHER_BRICK), 0)},
        {"minecraft:deepslate_slab", slabId(shapeMaterialOf(DEEPSTONE), 0)},
        {"minecraft:deepslate_brick_slab", slabId(shapeMaterialOf(DEEPSTONE_BRICKS), 0)},
        {"minecraft:polished_deepslate_slab", slabId(shapeMaterialOf(DEEPSTONE), 0)},
        {"minecraft:smooth_stone_slab", slabId(shapeMaterialOf(STONE), 0)},
        {"minecraft:crimson_slab", slabId(shapeMaterialOf(CRIMSON_PLANKS), 0)},
        {"minecraft:warped_slab", slabId(shapeMaterialOf(VOID_PLANKS), 0)},

        // Fences and Gates
        {"minecraft:fence", FENCE},
        {"minecraft:wooden_fence", FENCE},
        {"minecraft:oak_fence", FENCE},
        {"minecraft:spruce_fence", SPRUCE_FENCE},
        {"minecraft:birch_fence", SPRUCE_FENCE},
        {"minecraft:jungle_fence", SPRUCE_FENCE},
        {"minecraft:acacia_fence", SPRUCE_FENCE},
        {"minecraft:dark_oak_fence", SPRUCE_FENCE},
        {"minecraft:nether_brick_fence", NETHER_BRICK_FENCE},
        {"minecraft:crimson_fence", NETHERPERK_FENCE},
        {"minecraft:warped_fence", VOID_FENCE},
        {"minecraft:fence_gate", FENCE},
        {"minecraft:oak_fence_gate", FENCE},
        {"minecraft:spruce_fence_gate", SPRUCE_FENCE},
        {"minecraft:birch_fence_gate", SPRUCE_FENCE},
        {"minecraft:jungle_fence_gate", SPRUCE_FENCE},
        {"minecraft:acacia_fence_gate", SPRUCE_FENCE},
        {"minecraft:dark_oak_fence_gate", SPRUCE_FENCE},

        // Doors and Trapdoors
        {"minecraft:wooden_door", OAK_DOOR},
        {"minecraft:oak_door", OAK_DOOR},
        {"minecraft:spruce_door", OAK_DOOR},
        {"minecraft:birch_door", OAK_DOOR},
        {"minecraft:jungle_door", OAK_DOOR},
        {"minecraft:acacia_door", OAK_DOOR},
        {"minecraft:dark_oak_door", OAK_DOOR},
        {"minecraft:iron_door", IRON_DOOR},
        {"minecraft:trapdoor", OAK_TRAPDOOR},
        {"minecraft:wooden_trapdoor", OAK_TRAPDOOR},
        {"minecraft:oak_trapdoor", OAK_TRAPDOOR},
        {"minecraft:iron_trapdoor", OAK_TRAPDOOR},
        {"minecraft:spruce_trapdoor", OAK_TRAPDOOR},
        {"minecraft:birch_trapdoor", OAK_TRAPDOOR},
        {"minecraft:jungle_trapdoor", OAK_TRAPDOOR},
        {"minecraft:acacia_trapdoor", OAK_TRAPDOOR},
        {"minecraft:dark_oak_trapdoor", OAK_TRAPDOOR},

        // Powerstone (Redstone equivalents)
        {"minecraft:redstone_wire", POWER_CABLE},
        {"minecraft:redstone_block", POWERSTONE_BLOCK},
        {"minecraft:redstone_torch", TORCH},
        {"minecraft:unlit_redstone_torch", TORCH},
        {"minecraft:redstone_lamp", POWERSTONE_LAMP},
        {"minecraft:lit_redstone_lamp", POWERSTONE_LAMP_ON},
        {"minecraft:repeater", POWER_CABLE},
        {"minecraft:unpowered_repeater", POWER_CABLE},
        {"minecraft:powered_repeater", POWER_CABLE_ON},
        {"minecraft:comparator", POWER_CABLE},
        {"minecraft:unpowered_comparator", POWER_CABLE},
        {"minecraft:powered_comparator", POWER_CABLE_ON},
        {"minecraft:lever", SWITCH},
        {"minecraft:stone_button", SWITCH},
        {"minecraft:wooden_button", SWITCH},
        {"minecraft:stone_pressure_plate", SWITCH},
        {"minecraft:wooden_pressure_plate", SWITCH},
        {"minecraft:light_weighted_pressure_plate", SWITCH},
        {"minecraft:heavy_weighted_pressure_plate", SWITCH},
        {"minecraft:daylight_detector", POWERSTONE_BLOCK},
        {"minecraft:daylight_detector_inverted", POWERSTONE_BLOCK},
        {"minecraft:observer", POWERSTONE_BLOCK},
        {"minecraft:target", POWERSTONE_BLOCK},
        {"minecraft:dispenser", POWERSTONE_FURNACE},
        {"minecraft:dropper", POWERSTONE_FURNACE},
        {"minecraft:tripwire", POWER_CABLE},
        {"minecraft:tripwire_hook", POWER_CABLE},
        {"minecraft:tnt", TNT},

        {"minecraft:smooth_stone", HARDENED_CONCRETE},
        {"minecraft:polished_blackstone_bricks", DEEPSTONE_BRICKS},
        {"minecraft:light_gray_terracotta", CLAY},
        // Rails
        {"minecraft:rail", POWER_CABLE},
        {"minecraft:golden_rail", POWER_CABLE_ON},
        {"minecraft:detector_rail", POWER_CABLE},
        {"minecraft:activator_rail", POWER_CABLE}
    };

    auto it = kDefs.find(name);
    if (it != kDefs.end()) return it->second;

    // Substring heuristics
    if (name.find("stairs") != std::string::npos) return stairsId(shapeMaterialOf(COBBLESTONE), 0, false);
    if (name.find("slab") != std::string::npos) return slabId(shapeMaterialOf(STONE), 0);
    if (name.find("fence_gate") != std::string::npos || name.find("fence") != std::string::npos) return FENCE;
    if (name.find("door") != std::string::npos && name.find("iron") != std::string::npos) return IRON_DOOR;
    if (name.find("door") != std::string::npos) return OAK_DOOR;
    if (name.find("trapdoor") != std::string::npos) return OAK_TRAPDOOR;
    if (name.find("button") != std::string::npos || name.find("lever") != std::string::npos || name.find("pressure_plate") != std::string::npos) return SWITCH;
    if (name.find("redstone") != std::string::npos || name.find("wire") != std::string::npos) return POWER_CABLE;
    if (name.find("lamp") != std::string::npos) return POWERSTONE_LAMP;
    if (name.find("rail") != std::string::npos) return POWER_CABLE;
    if (name.find("brick") != std::string::npos) return BRICKS;
    if (name.find("wood") != std::string::npos || name.find("planks") != std::string::npos) return PLANKS;
    if (name.find("log") != std::string::npos) return LOG;
    if (name.find("ore") != std::string::npos) return IRON_ORE;
    if (name.find("stone") != std::string::npos) return STONE;
    if (name.find("deepslate") != std::string::npos) return DEEPSTONE;

    return STONE;
}

BedrockWorldScanResult scanBedrockWorld(const std::string& mcworldPath) {
    BedrockWorldScanResult res;
    std::ifstream in(mcworldPath, std::ios::binary);
    if (!in) {
        res.error = "Soubor světa nelze otevřít";
        return res;
    }
    std::vector<uint8_t> zipData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<ZipEntry> entries;
    if (!readZipEntries(zipData, entries)) {
        res.error = "Není to platný archiv .mcworld";
        return res;
    }

    res.name = "Bedrock svět";
    for (const auto& e : entries) {
        if (e.name == "levelname.txt" || (e.name.size() > 14 && e.name.rfind("/levelname.txt") == e.name.size() - 14)) {
            std::vector<uint8_t> content;
            if (extractZipFile(e, content) && !content.empty()) {
                res.name.assign((const char*)content.data(), content.size());
                while (!res.name.empty() && (res.name.back() == '\r' || res.name.back() == '\n' || res.name.back() == ' '))
                    res.name.pop_back();
            }
        }
    }

    std::map<std::string, int> counts;
    std::vector<SubchunkRecord> subchunks;

    for (const auto& e : entries) {
        if (e.name.find("db/") != std::string::npos && e.name.rfind(".ldb") == e.name.size() - 4) {
            std::vector<uint8_t> ldbBytes;
            if (extractZipFile(e, ldbBytes)) {
                parseLdbFile(ldbBytes.data(), ldbBytes.size(), subchunks);
            }
        }
    }

    res.subchunks = (int)subchunks.size();
    for (const auto& sc : subchunks) {
        auto pal = parseSubchunkPalette(sc.data);
        for (const auto& blockName : pal) {
            counts[blockName]++;
        }
    }

    // Sort by frequency descending
    std::vector<std::pair<std::string, int>> sortedBlocks(counts.begin(), counts.end());
    std::sort(sortedBlocks.begin(), sortedBlocks.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    res.blocks = std::move(sortedBlocks);
    res.ok = true;
    return res;
}

bool convertBedrockWorld(const std::string& mcworldPath,
                        const std::string& worldsDir,
                        const std::unordered_map<std::string, ItemId>& mapping,
                        bool flatGenerator,
                        std::string& outFolder,
                        std::string& error,
                        std::function<void(float progress, const std::string& status)> onProgress) {
    if (onProgress) onProgress(0.05f, "Načítám .mcworld soubor...");

    std::ifstream in(mcworldPath, std::ios::binary);
    if (!in) {
        error = "Nelze otevřít soubor světa";
        return false;
    }
    std::vector<uint8_t> zipData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<ZipEntry> entries;
    if (!readZipEntries(zipData, entries)) {
        error = "Chybný formát archivu .mcworld";
        return false;
    }

    // World name from levelname.txt
    std::string worldTitle = "Imported Bedrock World";
    for (const auto& e : entries) {
        if (e.name == "levelname.txt" || (e.name.size() > 14 && e.name.rfind("/levelname.txt") == e.name.size() - 14)) {
            std::vector<uint8_t> content;
            if (extractZipFile(e, content) && !content.empty()) {
                worldTitle.assign((const char*)content.data(), content.size());
                while (!worldTitle.empty() && (worldTitle.back() == '\r' || worldTitle.back() == '\n' || worldTitle.back() == ' '))
                    worldTitle.pop_back();
            }
        }
    }

    std::string safeName;
    for (char c : worldTitle) {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-') safeName += c;
        else if (c == ' ') safeName += '_';
    }
    if (safeName.empty()) safeName = "bedrock_world";

    std::string targetDir = worldsDir + "/" + safeName;
    int suffix = 1;
    while (fs::exists(targetDir)) {
        targetDir = worldsDir + "/" + safeName + "_" + std::to_string(suffix++);
    }
    outFolder = targetDir;

    std::error_code ec;
    fs::create_directories(outFolder, ec);
    std::string overworldDir = outFolder + "/overworld";
    fs::create_directories(overworldDir, ec);

    // Save world_icon.jpeg if present
    for (const auto& e : entries) {
        if (e.name == "world_icon.jpeg") {
            std::vector<uint8_t> iconData;
            if (extractZipFile(e, iconData)) {
                std::ofstream of(outFolder + "/world_icon.jpeg", std::ios::binary);
                of.write((const char*)iconData.data(), (std::streamsize)iconData.size());
            }
        }
    }

    if (onProgress) onProgress(0.12f, "Čtu bloky z LevelDB databáze...");

    std::vector<SubchunkRecord> subchunks;
    for (const auto& e : entries) {
        if (e.name.find("db/") != std::string::npos && e.name.rfind(".ldb") == e.name.size() - 4) {
            std::vector<uint8_t> ldbBytes;
            if (extractZipFile(e, ldbBytes)) {
                parseLdbFile(ldbBytes.data(), ldbBytes.size(), subchunks);
            }
        }
    }

    if (subchunks.empty()) {
        error = "V databázi světa nebyly nalezeny žádné chunky";
        return false;
    }

    if (onProgress) onProgress(0.20f, "Převádím subchunky na VoxelCraft formát...");

    // Group subchunks by (cx, cz)
    std::map<std::pair<int, int>, std::vector<const SubchunkRecord*>> chunkMap;
    for (const auto& sc : subchunks) {
        chunkMap[{sc.cx, sc.cz}].push_back(&sc);
    }

    // Group chunks into regions (rx, rz)
    struct RegionBuilder {
        std::vector<uint8_t> blobs[1024];
        bool hasChunks = false;
    };
    std::map<std::pair<int, int>, RegionBuilder> regionBuilders;

    size_t chunkIdx = 0;
    size_t totalChunks = chunkMap.size();

    for (const auto& [coords, scList] : chunkMap) {
        chunkIdx++;
        if (onProgress && (chunkIdx % 10 == 0 || chunkIdx == totalChunks)) {
            float p = 0.2f + 0.7f * ((float)chunkIdx / (float)totalChunks);
            onProgress(p, "Převádím chunky: " + std::to_string(chunkIdx) + " / " + std::to_string(totalChunks));
        }

        int cx = coords.first, cz = coords.second;
        // Region coordinates (floor(c / 32))
        int rx = cx >= 0 ? cx / 32 : (cx - 31) / 32;
        int rz = cz >= 0 ? cz / 32 : (cz - 31) / 32;
        int inRx = ((cx % 32) + 32) % 32;
        int inRz = ((cz % 32) + 32) % 32;
        int chunkSlot = inRx + inRz * 32;

        // VoxelCraft chunk: 16x16x128 blocks
        std::vector<ItemId> blocks(16 * 16 * 128, AIR);

        for (const SubchunkRecord* sc : scList) {
            int subY = sc->cy;
            // Bedrock Y: [-64, 319]. VoxelCraft Y: [0, 127]. Offset +64 -> subY from -4 to 3
            int yBase = (subY + 4) * 16;
            if (yBase < 0 || yBase >= 128) continue;

            const auto& val = sc->data;
            if (val.size() < 3) continue;
            uint8_t ver = val[0];
            size_t p = 2;
            if (ver == 9 && val.size() > p) p += 1;
            if (p >= val.size()) continue;

            uint8_t flag = val[p++];
            int bitsPerBlock = flag >> 1;
            const uint8_t* bitData = &val[p];
            if (bitsPerBlock > 0) {
                int bpw = 32 / bitsPerBlock;
                int words = (4096 + bpw - 1) / bpw;
                p += words * 4;
            }
            if (p + 4 > val.size()) continue;

            uint32_t palLen = get32(&val[p]);
            p += 4;
            std::vector<ItemId> paletteVcIds;

            for (uint32_t i = 0; i < palLen && p < val.size(); ++i) {
                Nbt root;
                std::string err;
                std::vector<uint8_t> rest(val.begin() + (long)p, val.end());
                if (!Nbt::parse(rest, root, err)) break;
                const Nbt* target = &root;
                if (const Nbt* sub = root.get("")) target = sub;
                std::string bname = "minecraft:stone";
                if (const Nbt* nt = target->get("name")) bname = nt->text;

                auto it = mapping.find(bname);
                paletteVcIds.push_back(it != mapping.end() ? it->second : defaultBedrockMapping(bname));

                size_t nextComp = p + 1;
                while (nextComp + 2 < val.size()) {
                    if (val[nextComp] == 0x0a && val[nextComp + 1] == 0x00 && val[nextComp + 2] == 0x00) break;
                    nextComp++;
                }
                p = nextComp;
            }

            if (paletteVcIds.empty()) paletteVcIds.push_back(AIR);

            // Decode bitpacked subchunk blocks into chunk
            if (bitsPerBlock == 0) {
                ItemId single = paletteVcIds[0];
                for (int bx = 0; bx < 16; ++bx)
                    for (int bz = 0; bz < 16; ++bz)
                        for (int sy = 0; sy < 16; ++sy)
                            blocks[(bx << 11) | (bz << 7) | (yBase + sy)] = single;
            } else {
                int bpw = 32 / bitsPerBlock;
                uint32_t mask = (1u << bitsPerBlock) - 1;
                for (int bx = 0; bx < 16; ++bx) {
                    for (int bz = 0; bz < 16; ++bz) {
                        for (int sy = 0; sy < 16; ++sy) {
                            int blkIdx = (bx << 8) | (bz << 4) | sy;
                            int wordIdx = blkIdx / bpw;
                            int bitOff = (blkIdx % bpw) * bitsPerBlock;
                            uint32_t word = get32(bitData + wordIdx * 4);
                            uint32_t palIdx = (word >> bitOff) & mask;
                            ItemId id = palIdx < paletteVcIds.size() ? paletteVcIds[palIdx] : AIR;
                            blocks[(bx << 11) | (bz << 7) | (yBase + sy)] = id;
                        }
                    }
                }
            }
        }

        // RLE encode the 32768 blocks
        std::vector<uint8_t> runs;
        size_t bpos = 0;
        while (bpos < blocks.size()) {
            ItemId id = blocks[bpos];
            size_t count = 1;
            while (bpos + count < blocks.size() && blocks[bpos + count] == id && count < 256) count++;
            runs.push_back((uint8_t)(count - 1));
            runs.push_back((uint8_t)id);
            runs.push_back((uint8_t)(id >> 8));
            bpos += count;
        }

        // Compress runs into VCC4
        uLongf packedSize = compressBound((uLong)runs.size());
        std::vector<uint8_t> blob(8 + packedSize);
        blob[0] = 'V'; blob[1] = 'C'; blob[2] = 'C'; blob[3] = '4';
        for (int k = 0; k < 4; ++k) blob[4 + k] = (uint8_t)(runs.size() >> (8 * k));
        if (compress2(blob.data() + 8, &packedSize, runs.data(), (uLong)runs.size(), 6) == Z_OK) {
            blob.resize(8 + packedSize);
            regionBuilders[{rx, rz}].blobs[chunkSlot] = std::move(blob);
            regionBuilders[{rx, rz}].hasChunks = true;
        }
    }

    if (onProgress) onProgress(0.92f, "Ukládám regiony...");

    // Write region files
    for (const auto& [rcoords, rb] : regionBuilders) {
        if (!rb.hasChunks) continue;
        int rx = rcoords.first, rz = rcoords.second;
        std::string rpath = overworldDir + "/r." + std::to_string(rx) + "." + std::to_string(rz) + ".vcr";

        std::vector<uint8_t> out = {'V', 'C', 'R', '1'};
        out.resize(4 + 1024 * 8, 0);

        for (int i = 0; i < 1024; ++i) {
            if (rb.blobs[i].empty()) continue;
            uint32_t off = (uint32_t)out.size();
            uint32_t len = (uint32_t)rb.blobs[i].size();
            for (int k = 0; k < 4; ++k) {
                out[4 + i * 8 + k] = (uint8_t)(off >> (8 * k));
                out[8 + i * 8 + k] = (uint8_t)(len >> (8 * k));
            }
            out.insert(out.end(), rb.blobs[i].begin(), rb.blobs[i].end());
        }

        std::ofstream rf(rpath, std::ios::binary);
        rf.write((const char*)out.data(), (std::streamsize)out.size());
    }

    if (onProgress) onProgress(0.96f, "Nastavuji generátor světa...");

    // Save generator configuration (Flat or Overworld)
    GenConfig cfg = GenConfig::defaults();
    cfg.flat = flatGenerator;
    if (flatGenerator) {
        cfg.flatPreset = 0;
    }
    saveGenerator(outFolder, cfg);

    if (onProgress) onProgress(0.98f, "Vytvářím level.dat...");

    // Write level.dat
    std::string levelDat = "version=1\nname=" + worldTitle +
                           "\nseed=1870714464\nmode=1\nlastPlayed=0\nshowCoordinates=1\nkeepInventory=1\nallowCommands=1"
                           "\nplayer.x=0.5\nplayer.y=65\nplayer.z=0.5\nspawn.x=0.5\nspawn.y=65\nspawn.z=0.5"
                           "\nyaw=0\npitch=0\ndimension=0\nhealth=20\nfood=20\nsaturation=5\nair=15\nflying=1\nselected=0\n";
    std::ofstream lf(outFolder + "/level.dat", std::ios::binary);
    lf.write(levelDat.data(), (std::streamsize)levelDat.size());

    if (onProgress) onProgress(1.0f, "Dokončeno!");
    return true;
}

bool exportBedrockWorld(const std::string& worldFolder, const std::string& outFile, std::string& error) {
    fs::path root(worldFolder);
    if (!fs::is_regular_file(root / "level.dat")) {
        error = "Svět nemá level.dat";
        return false;
    }

    // Read world title
    std::string worldTitle = root.filename().string();
    std::ifstream lf(root / "level.dat");
    std::string line;
    while (std::getline(lf, line)) {
        if (line.rfind("name=", 0) == 0) {
            worldTitle = line.substr(5);
            break;
        }
    }

    struct ZipFileItem {
        std::string name;
        std::vector<uint8_t> data;
    };
    std::vector<ZipFileItem> files;

    // 1. levelname.txt
    files.push_back({"levelname.txt", std::vector<uint8_t>(worldTitle.begin(), worldTitle.end())});

    // 2. world_behavior_packs.json & world_resource_packs.json
    std::string emptyJson = "[]\n";
    files.push_back({"world_behavior_packs.json", std::vector<uint8_t>(emptyJson.begin(), emptyJson.end())});
    files.push_back({"world_resource_packs.json", std::vector<uint8_t>(emptyJson.begin(), emptyJson.end())});

    // 3. Bedrock level.dat
    Nbt rootNbt = Nbt::makeCompound();
    rootNbt.fields["LevelName"] = Nbt::makeString(worldTitle);
    rootNbt.fields["StorageVersion"] = Nbt::makeInt(9);
    rootNbt.fields["NetworkVersion"] = Nbt::makeInt(582);
    rootNbt.fields["GameType"] = Nbt::makeInt(1);
    rootNbt.fields["Generator"] = Nbt::makeInt(1);
    std::vector<uint8_t> nbtBytes = rootNbt.write();

    std::vector<uint8_t> levelDatBedrock;
    uint32_t bVer = 10;
    uint32_t bSz = (uint32_t)nbtBytes.size();
    levelDatBedrock.insert(levelDatBedrock.end(), (const uint8_t*)&bVer, ((const uint8_t*)&bVer) + 4);
    levelDatBedrock.insert(levelDatBedrock.end(), (const uint8_t*)&bSz, ((const uint8_t*)&bSz) + 4);
    levelDatBedrock.insert(levelDatBedrock.end(), nbtBytes.begin(), nbtBytes.end());
    files.push_back({"level.dat", std::move(levelDatBedrock)});

    // 4. world_icon.jpeg if exists
    if (fs::is_regular_file(root / "world_icon.jpeg")) {
        std::ifstream img(root / "world_icon.jpeg", std::ios::binary);
        std::vector<uint8_t> imgBytes((std::istreambuf_iterator<char>(img)), std::istreambuf_iterator<char>());
        if (!imgBytes.empty()) files.push_back({"world_icon.jpeg", std::move(imgBytes)});
    }

    // 5. db/ minimal manifest
    std::string currentDb = "MANIFEST-000001\n";
    files.push_back({"db/CURRENT", std::vector<uint8_t>(currentDb.begin(), currentDb.end())});
    std::string manifestDb = "\x00\x00\x00\x00\x04\x01\x00\x00\x00";
    files.push_back({"db/MANIFEST-000001", std::vector<uint8_t>(manifestDb.begin(), manifestDb.end())});

    // Write standard ZIP format (method 0 stored)
    std::vector<uint8_t> out, central;
    uint32_t count = 0;
    for (const auto& item : files) {
        const std::string& name = item.name;
        const auto& data = item.data;
        uint32_t crc = (uint32_t)::crc32(0L, (const Bytef*)data.data(), (uInt)data.size());
        uint32_t offset = (uint32_t)out.size();

        // Local file header
        put32(out, 0x04034b50);
        put16(out, 20); put16(out, 0x0800); put16(out, 0); put16(out, 0); put16(out, 0x21);
        put32(out, crc); put32(out, (uint32_t)data.size()); put32(out, (uint32_t)data.size());
        put16(out, (uint16_t)name.size()); put16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());

        // Central directory entry
        put32(central, 0x02014b50);
        put16(central, 20); put16(central, 20); put16(central, 0x0800); put16(central, 0); put16(central, 0); put16(central, 0x21);
        put32(central, crc); put32(central, (uint32_t)data.size()); put32(central, (uint32_t)data.size());
        put16(central, (uint16_t)name.size()); put16(central, 0); put16(central, 0); put16(central, 0); put16(central, 0);
        put32(central, 0); put32(central, offset);
        central.insert(central.end(), name.begin(), name.end());
        ++count;
    }

    uint32_t centralAt = (uint32_t)out.size();
    out.insert(out.end(), central.begin(), central.end());
    put32(out, 0x06054b50);
    put16(out, 0); put16(out, 0); put16(out, (uint16_t)count); put16(out, (uint16_t)count);
    put32(out, (uint32_t)central.size()); put32(out, centralAt); put16(out, 0);

    std::error_code ec;
    fs::create_directories(fs::path(outFile).parent_path(), ec);
    std::ofstream o(outFile, std::ios::binary);
    o.write((const char*)out.data(), (std::streamsize)out.size());
    if (!o) {
        error = "Soubor nejde zapsat";
        return false;
    }
    return true;
}

}  // namespace vc
