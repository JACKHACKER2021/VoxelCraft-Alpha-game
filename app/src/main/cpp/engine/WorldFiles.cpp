#include "WorldFiles.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "Save.h"

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
        const std::string name = fs::relative(e.path(), root, ec).generic_string();
        const std::vector<uint8_t> data = readAll(e.path());
        const uint32_t crc = crc32(data), offset = (uint32_t)out.size();
        // Local header, stored (method 0).
        put32(out, 0x04034b50);
        put16(out, 20), put16(out, 0x0800), put16(out, 0), put16(out, 0), put16(out, 0x21);
        put32(out, crc), put32(out, (uint32_t)data.size()), put32(out, (uint32_t)data.size());
        put16(out, (uint32_t)name.size()), put16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());
        // Central directory entry.
        put32(central, 0x02014b50);
        put16(central, 20), put16(central, 20), put16(central, 0x0800), put16(central, 0), put16(central, 0), put16(central, 0x21);
        put32(central, crc), put32(central, (uint32_t)data.size()), put32(central, (uint32_t)data.size());
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
    const std::vector<uint8_t> d = readAll(file);
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
    };
    std::vector<Entry> entries;
    bool hasLevel = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (get32(d, at) != 0x02014b50) break;
        const uint32_t method = get16(d, at + 10), size = get32(d, at + 20), nameLen = get16(d, at + 28);
        const uint32_t extra = get16(d, at + 30), comment = get16(d, at + 32), local = get32(d, at + 42);
        if (at + 46 + nameLen > d.size()) break;
        std::string name(d.begin() + (long)at + 46, d.begin() + (long)(at + 46 + nameLen));
        at += 46 + nameLen + extra + comment;
        if (method != 0) {
            error = "Komprimovaný soubor: použij export z VoxelCraftu";
            return false;
        }
        if (name.empty() || name.back() == '/' || name.find("..") != std::string::npos || name[0] == '/') continue;
        if (get32(d, local) != 0x04034b50) continue;
        const size_t dataAt = local + 30 + get16(d, local + 26) + get16(d, local + 28);
        if (dataAt + size > d.size()) continue;
        hasLevel |= name == "level.dat";
        entries.push_back({name, dataAt, size});
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
        std::ofstream(target, std::ios::binary).write((const char*)d.data() + e.data, (std::streamsize)e.size);
    }
    return true;
}

}  // namespace vc
