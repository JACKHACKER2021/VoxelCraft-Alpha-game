#include "Png.h"

#include <algorithm>
#include <cstdio>

namespace vc {

// Stored (uncompressed) deflate blocks keep this tiny; the files are small anyway.
bool writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
    auto crc = [](const uint8_t* d, size_t n, uint32_t c = 0xffffffffu) {
        for (size_t i = 0; i < n; ++i) {
            c ^= d[i];
            for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
        }
        return c;
    };
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), &rgba[y * w * 4], &rgba[(y + 1) * w * 4]);
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size(); off += 65535) {
        size_t len = std::min<size_t>(65535, raw.size() - off);
        z.push_back(off + len == raw.size());
        z.push_back(len & 255); z.push_back(len >> 8);
        z.push_back(~len & 255); z.push_back((~len >> 8) & 255);
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + len);
    }
    uint32_t adler = (b << 16) | a;
    for (int i = 3; i >= 0; --i) z.push_back((adler >> (i * 8)) & 255);

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto be32 = [&](uint32_t v) { uint8_t o[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; std::fwrite(o, 1, 4, f); };
    auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        be32((uint32_t)data.size());
        std::vector<uint8_t> td(type, type + 4);
        td.insert(td.end(), data.begin(), data.end());
        std::fwrite(td.data(), 1, td.size(), f);
        be32(~crc(td.data(), td.size()));
    };
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
                                 uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h), 8, 6, 0, 0, 0};
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    return std::fclose(f) == 0;
}


}  // namespace vc
