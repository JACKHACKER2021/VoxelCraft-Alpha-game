#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vc {

// Little-endian NBT as used by Minecraft Bedrock files such as .mcstructure.
// The reader checks every length against the remaining bytes, so a broken or
// hostile file fails cleanly instead of crashing the game.
struct Nbt {
    enum Type : uint8_t { End = 0, Byte, Short, Int, Long, Float, Double, ByteArray, String, List, Compound, IntArray, LongArray };
    Type type = End;
    int64_t integer = 0;  // Byte, Short, Int, Long
    double real = 0;      // Float, Double
    std::string text;     // String
    Type listType = End;
    std::vector<Nbt> items;              // List, and the arrays (as Int/Long/Byte items)
    std::map<std::string, Nbt> fields;   // Compound

    const Nbt* get(const std::string& name) const {
        auto it = fields.find(name);
        return it == fields.end() ? nullptr : &it->second;
    }

    // Parses a whole file: one named root tag (normally a compound with an empty name).
    static bool parse(const std::vector<uint8_t>& data, Nbt& root, std::string& error);
    // Writes a root compound (for tests and tools).
    std::vector<uint8_t> write() const;

    static Nbt makeInt(int32_t v) { Nbt n; n.type = Int; n.integer = v; return n; }
    static Nbt makeString(std::string s) { Nbt n; n.type = String; n.text = std::move(s); return n; }
    static Nbt makeCompound() { Nbt n; n.type = Compound; return n; }
    static Nbt makeList(Type of) { Nbt n; n.type = List; n.listType = of; return n; }
};

}  // namespace vc
