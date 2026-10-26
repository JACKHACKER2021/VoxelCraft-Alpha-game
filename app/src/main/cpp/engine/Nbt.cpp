#include "Nbt.h"

#include <cstring>

namespace vc {

namespace {

constexpr int kMaxDepth = 64;
constexpr size_t kMaxElements = 64u << 20;  // a 64M element list is surely not a real structure

struct Reader {
    const std::vector<uint8_t>& d;
    size_t pos = 0;
    std::string error;

    bool need(size_t n) {
        if (pos + n <= d.size()) return true;
        if (error.empty()) error = "soubor je zkrácený nebo poškozený (pozice " + std::to_string(pos) + ")";
        return false;
    }
    template <typename T>
    bool read(T& out) {
        if (!need(sizeof(T))) return false;
        uint64_t v = 0;
        for (size_t i = 0; i < sizeof(T); ++i) v |= (uint64_t)d[pos + i] << (8 * i);
        pos += sizeof(T);
        std::memcpy(&out, &v, sizeof(T));
        return true;
    }
    bool readString(std::string& s) {
        uint16_t len;
        if (!read(len) || !need(len)) return false;
        s.assign(reinterpret_cast<const char*>(&d[pos]), len);
        pos += len;
        return true;
    }
    bool payload(Nbt::Type type, Nbt& out, int depth) {
        if (depth > kMaxDepth) {
            error = "příliš hluboké vnoření";
            return false;
        }
        out.type = type;
        switch (type) {
            case Nbt::Byte: { int8_t v; if (!read(v)) return false; out.integer = v; return true; }
            case Nbt::Short: { int16_t v; if (!read(v)) return false; out.integer = v; return true; }
            case Nbt::Int: { int32_t v; if (!read(v)) return false; out.integer = v; return true; }
            case Nbt::Long: { int64_t v; if (!read(v)) return false; out.integer = v; return true; }
            case Nbt::Float: { float v; if (!read(v)) return false; out.real = v; return true; }
            case Nbt::Double: { double v; if (!read(v)) return false; out.real = v; return true; }
            case Nbt::String: return readString(out.text);
            case Nbt::ByteArray:
            case Nbt::IntArray:
            case Nbt::LongArray: {
                int32_t n;
                if (!read(n)) return false;
                size_t each = type == Nbt::ByteArray ? 1 : type == Nbt::IntArray ? 4 : 8;
                if (n < 0 || (size_t)n > kMaxElements || !need((size_t)n * each)) {
                    if (error.empty()) error = "neplatná délka pole";
                    return false;
                }
                Nbt::Type item = type == Nbt::ByteArray ? Nbt::Byte : type == Nbt::IntArray ? Nbt::Int : Nbt::Long;
                out.items.resize((size_t)n);
                for (Nbt& it : out.items)
                    if (!payload(item, it, depth + 1)) return false;
                return true;
            }
            case Nbt::List: {
                uint8_t t;
                int32_t n;
                if (!read(t) || !read(n)) return false;
                if (t > Nbt::LongArray || n < 0 || (size_t)n > kMaxElements || (n > 0 && t == Nbt::End)) {
                    error = "neplatný seznam";
                    return false;
                }
                // Every element needs at least one byte (empty compounds still have their End tag).
                if (n > 0 && t != Nbt::List && !need((size_t)n)) return false;
                out.listType = (Nbt::Type)t;
                out.items.resize((size_t)n);
                for (Nbt& it : out.items)
                    if (!payload((Nbt::Type)t, it, depth + 1)) return false;
                return true;
            }
            case Nbt::Compound:
                for (;;) {
                    uint8_t t;
                    if (!read(t)) return false;
                    if (t == Nbt::End) return true;
                    if (t > Nbt::LongArray) {
                        error = "neznámý typ tagu " + std::to_string(t);
                        return false;
                    }
                    std::string name;
                    if (!readString(name)) return false;
                    Nbt child;
                    if (!payload((Nbt::Type)t, child, depth + 1)) return false;
                    out.fields[name] = std::move(child);
                }
            default: error = "neznámý typ tagu"; return false;
        }
    }
};

struct Writer {
    std::vector<uint8_t> out;
    template <typename T>
    void put(T v) {
        uint64_t u = 0;
        std::memcpy(&u, &v, sizeof(T));
        for (size_t i = 0; i < sizeof(T); ++i) out.push_back((uint8_t)(u >> (8 * i)));
    }
    void putString(const std::string& s) {
        put<uint16_t>((uint16_t)s.size());
        out.insert(out.end(), s.begin(), s.end());
    }
    void payload(const Nbt& n) {
        switch (n.type) {
            case Nbt::Byte: put<int8_t>((int8_t)n.integer); break;
            case Nbt::Short: put<int16_t>((int16_t)n.integer); break;
            case Nbt::Int: put<int32_t>((int32_t)n.integer); break;
            case Nbt::Long: put<int64_t>(n.integer); break;
            case Nbt::Float: put<float>((float)n.real); break;
            case Nbt::Double: put<double>(n.real); break;
            case Nbt::String: putString(n.text); break;
            case Nbt::ByteArray:
            case Nbt::IntArray:
            case Nbt::LongArray:
                put<int32_t>((int32_t)n.items.size());
                for (const Nbt& i : n.items) payload(i);
                break;
            case Nbt::List:
                put<uint8_t>(n.listType);
                put<int32_t>((int32_t)n.items.size());
                for (const Nbt& i : n.items) payload(i);
                break;
            case Nbt::Compound:
                for (const auto& [name, child] : n.fields) {
                    put<uint8_t>(child.type);
                    putString(name);
                    payload(child);
                }
                put<uint8_t>(Nbt::End);
                break;
            default: break;
        }
    }
};

}  // namespace

bool Nbt::parse(const std::vector<uint8_t>& data, Nbt& root, std::string& error) {
    Reader r{data, 0, {}};
    uint8_t type;
    std::string name;
    if (!r.read(type) || type != Compound || !r.readString(name) || !r.payload(Compound, root, 0)) {
        error = r.error.empty() ? "soubor nezačíná NBT složkou (není to .mcstructure?)" : r.error;
        return false;
    }
    return true;
}

std::vector<uint8_t> Nbt::write() const {
    Writer w;
    w.put<uint8_t>(Compound);
    w.putString("");
    w.payload(*this);
    return w.out;
}

}  // namespace vc
