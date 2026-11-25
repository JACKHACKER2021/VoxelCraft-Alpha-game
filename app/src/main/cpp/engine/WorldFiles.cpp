#include "WorldFiles.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <zlib.h>

#include "Save.h"
#include "Items.h"
#include "World.h"
#include <memory>
#include <sstream>

namespace vc {

namespace fs = std::filesystem;

namespace {

uint32_t crc32(const std::vector<uint8_t>& data) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : data) c = table[(c ^ b) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void put16(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back((uint8_t)v);
    o.push_back((uint8_t)(v >> 8));
}
void put32(std::vector<uint8_t>& o, uint32_t v) {
    put16(o, v & 0xFFFF);
    put16(o, v >> 16);
}
uint32_t get16(const std::vector<uint8_t>& d, size_t p) { return p + 2 <= d.size() ? d[p] | (uint32_t)d[p + 1] << 8 : 0; }
uint32_t get32(const std::vector<uint8_t>& d, size_t p) { return get16(d, p) | get16(d, p + 2) << 16; }

std::vector<uint8_t> readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string stamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d-%H%M%S", &tm);
    return buf;
}

}  // namespace

bool backupWorld(const std::string& worldFolder, const std::string& backupsRoot, int keep, std::string* made) {
    std::error_code ec;
    if (!fs::is_regular_file(fs::path(worldFolder) / "level.dat")) return false;
    const fs::path dir = fs::path(backupsRoot) / fs::path(worldFolder).filename();
    fs::path target = dir / stamp();
    for (int n = 2; fs::exists(target); ++n) target = dir / (stamp() + "-" + std::to_string(n));
    fs::create_directories(target, ec);
    fs::copy(worldFolder, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        fs::remove_all(target, ec);
        return false;
    }
    if (made) *made = target.string();
    // Only the newest few stay.
    std::vector<std::string> all = listBackups(worldFolder, backupsRoot);
    for (size_t i = (size_t)std::max(keep, 1); i < all.size(); ++i) fs::remove_all(all[i], ec);
    return true;
}

std::vector<std::string> listBackups(const std::string& worldFolder, const std::string& backupsRoot) {
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path dir = fs::path(backupsRoot) / fs::path(worldFolder).filename();
    if (!fs::is_directory(dir)) return out;
    for (auto& e : fs::directory_iterator(dir, ec))
        if (e.is_directory()) out.push_back(e.path().string());
    std::sort(out.rbegin(), out.rend());  // the names are timestamps: newest first
    return out;
}

bool restoreBackup(const std::string& worldFolder, const std::string& backupDir, const std::string& backupsRoot) {
    std::error_code ec;
    if (!fs::is_regular_file(fs::path(backupDir) / "level.dat")) return false;
    // Keep what is there now, in case the player changes their mind.
    backupWorld(worldFolder, backupsRoot + "/before_restore", 1);
    const fs::path tmp = fs::path(worldFolder).string() + ".restoring";
    fs::remove_all(tmp, ec);
    fs::copy(backupDir, tmp, fs::copy_options::recursive, ec);
    if (ec) return false;
    fs::remove_all(worldFolder, ec);
    fs::rename(tmp, worldFolder, ec);
    return !ec;
}

// 2.6.0: a region file in words. Each chunk is written layer by layer (y), each layer as runs of
// "block*count" over its 256 blocks (row by row: z = 0..15, in each row x = 0..15); layers that are
// the same are written once as a range. A block the names cannot bring back exactly is "#id".
std::string regionToText(const std::vector<uint8_t>& vcr, const std::string& name) {
    std::ostringstream o;
    o << "# VoxelCraft: a region of the world in readable form (" << name << ")\n"
      << "# Every chunk is 16 x 16 blocks, " << kChunkHeight << " high. Every line is one layer (y): block*count,\n"
      << "# read row by row (z = 0..15, in each row x = 0..15). The game turns this back into its .vcr file.\n"
      << "VCT1\n";
    if (vcr.size() < 4 + 1024 * 8 || vcr[0] != 'V' || vcr[1] != 'C' || vcr[2] != 'R' || vcr[3] != '1') return o.str();
    auto le = [&](size_t at) { return vcr[at] | vcr[at + 1] << 8 | vcr[at + 2] << 16 | (uint32_t)vcr[at + 3] << 24; };
    auto word = [](ItemId id) {
        const char* k = itemKey(id);
        if (k && *k && itemFromKey(k) == id) return std::string(k);
        return "#" + std::to_string(id);
    };
    auto c = std::make_unique<Chunk>(0, 0);
    for (int i = 0; i < 1024; ++i) {
        const uint32_t off = le(4 + i * 8), len = le(8 + i * 8);
        if (!len || (size_t)off + len > vcr.size()) continue;
        if (!decodeChunkData(std::vector<uint8_t>(vcr.begin() + off, vcr.begin() + off + len), *c)) continue;
        o << "chunk " << (i % 32) << " " << (i / 32) << "\n";
        std::string prev;
        int from = 0;
        auto flush = [&](int to) {
            if (prev.empty()) return;
            o << "y " << from;
            if (to > from) o << "-" << to;
            o << ":" << prev << "\n";
        };
        for (int y = 0; y < kChunkHeight; ++y) {
            std::string line;
            ItemId run = 0;
            int count = 0;
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    const ItemId b = c->blocks[Chunk::index(x, y, z)];
                    if (count && b == run) {
                        ++count;
                        continue;
                    }
                    if (count) line += " " + word(run) + "*" + std::to_string(count);
                    run = b, count = 1;
                }
            line += " " + word(run) + "*" + std::to_string(count);
            if (line != prev) {
                flush(y - 1);
                prev = line, from = y;
            }
        }
        flush(kChunkHeight - 1);
    }
    return o.str();
}

// The readable region back into a .vcr file; false when the text is broken.
bool textToRegion(const std::string& text, std::vector<uint8_t>& vcr) {
    std::istringstream in(text);
    std::string line;
    std::vector<std::vector<uint8_t>> blobs(1024);
    auto c = std::make_unique<Chunk>(0, 0);
    int index = -1;
    bool any = false;
    auto finish = [&] {
        if (index >= 0) blobs[(size_t)index] = encodeChunkData(*c);
    };
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line == "VCT1") continue;
        if (line.rfind("chunk ", 0) == 0) {
            finish();
            int lx, lz;
            if (std::sscanf(line.c_str(), "chunk %d %d", &lx, &lz) != 2 || lx < 0 || lx > 31 || lz < 0 || lz > 31) return false;
            index = lx + lz * 32;
            std::fill(std::begin(c->blocks), std::end(c->blocks), (ItemId)AIR);
            any = true;
            continue;
        }
        if (index < 0 || line.rfind("y ", 0) != 0) return false;
        const size_t colon = line.find(':');
        if (colon == std::string::npos) return false;
        int y0 = 0, y1 = 0;
        if (std::sscanf(line.c_str(), "y %d-%d", &y0, &y1) != 2) {
            if (std::sscanf(line.c_str(), "y %d", &y0) != 1) return false;
            y1 = y0;
        }
        if (y0 < 0 || y1 < y0 || y1 >= kChunkHeight) return false;
        std::istringstream runs(line.substr(colon + 1));
        std::vector<ItemId> layer;
        for (std::string tok; runs >> tok;) {
            const size_t star = tok.rfind('*');
            if (star == std::string::npos) return false;
            const std::string w = tok.substr(0, star);
            const int count = std::atoi(tok.c_str() + star + 1);
            if (count <= 0 || layer.size() + (size_t)count > 256) return false;
            const ItemId id = w[0] == '#' ? (ItemId)std::atoi(w.c_str() + 1) : (w == "air" ? (ItemId)AIR : itemFromKey(w));
            if (id == AIR && w != "air" && w != "#0") return false;  // an unknown name: refuse rather than lose blocks
            layer.insert(layer.end(), (size_t)count, id);
        }
        if (layer.size() != 256) return false;
        for (int y = y0; y <= y1; ++y)
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) c->blocks[Chunk::index(x, y, z)] = layer[(size_t)(z * 16 + x)];
    }
    finish();
    if (!any) return false;
    vcr = {'V', 'C', 'R', '1'};
    vcr.resize(4 + 1024 * 8, 0);
    for (int i = 0; i < 1024; ++i) {
        if (blobs[(size_t)i].empty()) continue;
        const uint32_t off = (uint32_t)vcr.size(), len = (uint32_t)blobs[(size_t)i].size();
        for (int k = 0; k < 4; ++k) vcr[4 + i * 8 + k] = (uint8_t)(off >> (8 * k)), vcr[8 + i * 8 + k] = (uint8_t)(len >> (8 * k));
        vcr.insert(vcr.end(), blobs[(size_t)i].begin(), blobs[(size_t)i].end());
    }
    return true;
}

bool exportWorld(const std::string& worldFolder, const std::string& file, std::string& error) {
    std::error_code ec;
    const fs::path root(worldFolder);
    if (!fs::is_regular_file(root / "level.dat")) {
        error = "Svět nemá level.dat";
        return false;
    }
    std::vector<uint8_t> out, central;
    uint32_t count = 0;
    for (auto& e : fs::recursive_directory_iterator(root, ec)) {
        if (!e.is_regular_file()) continue;
        std::string name = fs::relative(e.path(), root, ec).generic_string();
        std::vector<uint8_t> data = readAll(e.path());
        uint32_t method = 0;
        std::vector<uint8_t> stored = data;
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".vcr") == 0) {
            // 2.6.0: a region goes in readable (.vct), deflated so the file stays small.
            const std::string text = regionToText(data, name);
            data.assign(text.begin(), text.end());
            name.replace(name.size() - 4, 4, ".vct");
            z_stream zs{};
            stored.assign(compressBound((uLong)data.size()) + 64, 0);
            if (deflateInit2(&zs, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
                zs.next_in = data.data(), zs.avail_in = (uInt)data.size();
                zs.next_out = stored.data(), zs.avail_out = (uInt)stored.size();
                const bool done = deflate(&zs, Z_FINISH) == Z_STREAM_END;
                stored.resize(zs.total_out);
                deflateEnd(&zs);
                if (done) method = 8;
                else stored = data;
            } else {
                stored = data;
            }
        }
        const uint32_t crc = crc32(data), offset = (uint32_t)out.size();
        // Local header: stored (method 0), or deflated (8) for the readable regions.
        put32(out, 0x04034b50);
        put16(out, 20), put16(out, 0x0800), put16(out, method), put16(out, 0), put16(out, 0x21);
        put32(out, crc), put32(out, (uint32_t)stored.size()), put32(out, (uint32_t)data.size());
        put16(out, (uint32_t)name.size()), put16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), stored.begin(), stored.end());
        // Central directory entry.
        put32(central, 0x02014b50);
        put16(central, 20), put16(central, 20), put16(central, 0x0800), put16(central, method), put16(central, 0), put16(central, 0x21);
        put32(central, crc), put32(central, (uint32_t)stored.size()), put32(central, (uint32_t)data.size());
        put16(central, (uint32_t)name.size()), put16(central, 0), put16(central, 0), put16(central, 0), put16(central, 0);
        put32(central, 0), put32(central, offset);
        central.insert(central.end(), name.begin(), name.end());
        ++count;
    }
    const uint32_t centralAt = (uint32_t)out.size();
    out.insert(out.end(), central.begin(), central.end());
    put32(out, 0x06054b50);
    put16(out, 0), put16(out, 0), put16(out, count), put16(out, count);
    put32(out, (uint32_t)central.size()), put32(out, centralAt), put16(out, 0);
    // (2.6.0 preview build 6 sealed this into one packed file; it is a plain zip again, with readable
    // regions inside: the files of the world as they are, nothing hidden.)
    fs::create_directories(fs::path(file).parent_path(), ec);
    std::ofstream o(file, std::ios::binary);
    o.write((const char*)out.data(), (std::streamsize)out.size());
    if (!o) {
        error = "Soubor nejde zapsat";
        return false;
    }
    return true;
}

bool importWorld(const std::string& file, const std::string& worldsRoot, std::string& folder, std::string& error) {
    std::vector<uint8_t> d = readAll(file);
    // 2.6.0: a sealed world file (VCWORLD2): unpack the zip inside; older .vcworld files are the zip itself.
    if (d.size() > 8 && std::string(d.begin(), d.begin() + 8) == "VCWORLD2") {
        size_t p = 8;
        while (p + 1 < d.size() && !(d[p] == '\n' && d[p + 1] == '\n')) ++p;
        p += 2;
        if (p + 8 > d.size()) {
            error = "Soubor světa je poškozený";
            return false;
        }
        uint64_t size = 0;
        for (int k = 0; k < 8; ++k) size |= (uint64_t)d[p + k] << (8 * k);
        if (size > (1ull << 31)) {
            error = "Soubor světa je poškozený";
            return false;
        }
        std::vector<uint8_t> zip((size_t)size);
        uLongf got = (uLongf)size;
        if (uncompress(zip.data(), &got, d.data() + p + 8, (uLong)(d.size() - p - 8)) != Z_OK || got != size) {
            error = "Soubor světa je poškozený";
            return false;
        }
        d.swap(zip);
    }
    // The end of central directory record, searched from the end.
    size_t end = std::string::npos;
    for (size_t p = d.size() >= 22 ? d.size() - 22 : 0; p + 22 <= d.size(); --p) {
        if (get32(d, p) == 0x06054b50) {
            end = p;
            break;
        }
        if (p == 0) break;
    }
    if (end == std::string::npos) {
        error = "Není to soubor .vcworld";
        return false;
    }
    const uint32_t count = get16(d, end + 10);
    size_t at = get32(d, end + 16);
    struct Entry {
        std::string name;
        size_t data, size;
        uint32_t method, full;
    };
    std::vector<Entry> entries;
    bool hasLevel = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (get32(d, at) != 0x02014b50) break;
        const uint32_t method = get16(d, at + 10), size = get32(d, at + 20), full = get32(d, at + 24), nameLen = get16(d, at + 28);
        const uint32_t extra = get16(d, at + 30), comment = get16(d, at + 32), local = get32(d, at + 42);
        if (at + 46 + nameLen > d.size()) break;
        std::string name(d.begin() + (long)at + 46, d.begin() + (long)(at + 46 + nameLen));
        at += 46 + nameLen + extra + comment;
        if (method != 0 && method != 8) {
            error = "Komprimovaný soubor: použij export z VoxelCraftu";
            return false;
        }
        if (name.empty() || name.back() == '/' || name.find("..") != std::string::npos || name[0] == '/') continue;
        if (get32(d, local) != 0x04034b50) continue;
        const size_t dataAt = local + 30 + get16(d, local + 26) + get16(d, local + 28);
        if (dataAt + size > d.size()) continue;
        hasLevel |= name == "level.dat";
        entries.push_back({name, dataAt, size, method, full});
    }
    if (!hasLevel) {
        error = "V souboru není svět";
        return false;
    }
    folder = createWorldFolder(worldsRoot);
    for (const Entry& e : entries) {
        const fs::path target = fs::path(folder) / e.name;
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::vector<uint8_t> bytes(d.begin() + (long)e.data, d.begin() + (long)(e.data + e.size));
        if (e.method == 8) {
            // Deflated (the readable regions): inflate.
            if (e.full > (1u << 30)) continue;
            std::vector<uint8_t> raw(e.full);
            z_stream zs{};
            if (inflateInit2(&zs, -15) != Z_OK) continue;
            zs.next_in = bytes.data(), zs.avail_in = (uInt)bytes.size();
            zs.next_out = raw.data(), zs.avail_out = (uInt)raw.size();
            const int r = inflate(&zs, Z_FINISH);
            inflateEnd(&zs);
            if (r != Z_STREAM_END) {
                error = "Soubor světa je poškozený: " + e.name;
                return false;
            }
            bytes.swap(raw);
        }
        fs::path out = target;
        if (e.name.size() > 4 && e.name.compare(e.name.size() - 4, 4, ".vct") == 0) {
            // 2.6.0: a readable region becomes the game's .vcr again.
            std::vector<uint8_t> vcr;
            if (!textToRegion(std::string(bytes.begin(), bytes.end()), vcr)) {
                error = "Čitelný region se nedá přečíst: " + e.name;
                return false;
            }
            bytes.swap(vcr);
            out.replace_extension(".vcr");
        }
        std::ofstream(out, std::ios::binary).write((const char*)bytes.data(), (std::streamsize)bytes.size());
    }
    return true;
}

}  // namespace vc
