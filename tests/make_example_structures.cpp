// Writes the example files in examples/structures from the built-in structures.
// Build: g++ -std=c++17 -Iapp/src/main/cpp tests/make_example_structures.cpp app/src/main/cpp/engine/{Structure,Nbt,Items,World,Noise,GenConfig,Json}.cpp
#include <fstream>
#include <cstdio>
#include "engine/Structure.h"
using namespace vc;
static void save(const std::string& p, const std::vector<uint8_t>& d) { std::ofstream(p, std::ios::binary).write((const char*)d.data(), d.size()); }
int main() {
    auto b = builtinStructures();
    save("examples/structures/domek.mcstructure", exportMcStructure(b[0]));
    Structure v = b[1]; v.name = "Věž z kamene";
    save("examples/structures/vez.vcstructure", writeVcStructure(v));
    // read back
    for (const char* p : {"examples/structures/domek.mcstructure", "examples/structures/vez.vcstructure"}) {
        Structure s; ImportReport r; std::string e;
        bool ok = loadStructureFile(p, s, r, e);
        std::printf("%s: %s %dx%dx%d blocks %d unknown %zu %s\n", p, ok ? "ok" : "FAIL", s.sx, s.sy, s.sz, s.blockCount(), r.unknown.size(), e.c_str());
    }
}
