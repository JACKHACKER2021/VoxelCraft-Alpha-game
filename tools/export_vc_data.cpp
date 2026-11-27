// 2.8.0: writes the game's data in the VC language from the code: assets/vc/blocks.json (every block and
// item: name, textures, hardness, stack) and assets/vc/loot.json (what structure chests hold).
// Examples of every structure go to assets/vc/structures_examples. The shaders go to assets/vc/shaders with tools/export_vc_shaders.py.
// Build: g++ -std=c++17 -DVC_PREVIEW -Iapp/src/main/cpp tools/export_vc_data.cpp <engine objects> -lz -o export_vc_data
// Run:   ./export_vc_data app/src/main/assets/vc
#include <cstdio>
#include <fstream>
#include <string>

#include "engine/VcData.h"
#include "engine/WorldStructures.h"
#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    const std::string out = argv[1];
    std::ofstream(out + "/blocks.json") << vc::exportVcBlocks();
    vc::vcExporting(true);
    std::ofstream(out + "/loot.json") << vc::exportVcLoot();
    vc::vcExporting(false);
    // Every structure of the world as an example to edit (seed 1): copy one into a pack's vc/structures/ to replace it.
    std::filesystem::create_directories(out + "/structures_examples");
    for (int k = 0; k < (int)vc::WorldStructure::Count; ++k) {
        const vc::Structure st = vc::buildWorldStructureCode((vc::WorldStructure)k, 1);
        const std::vector<uint8_t> bytes = vc::writeVcStructure(st);
        std::ofstream(out + "/structures_examples/" + vc::worldStructureName((vc::WorldStructure)k) + ".vcstructure", std::ios::binary)
            .write((const char*)bytes.data(), (std::streamsize)bytes.size());
    }
    std::printf("written\n");
    return 0;
}
