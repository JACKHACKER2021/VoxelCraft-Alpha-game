#pragma once
#include <string>
#include <vector>

namespace vc {

// 2.7.0: one line of an import report: what a file became, or why it could not.
struct ImportLine {
    char status = '+';     // '+' converted, '!' partly, 'x' not possible
    std::string file;      // the source file inside the add-on
    std::string text;      // what happened: a Czech template ({0}, {1}...), shown through trf() with `args`
    std::vector<std::string> args;
};

// What the Bedrock importer made of an add-on.
struct BedrockImportReport {
    std::string name;
    int blocks = 0, items = 0, mobs = 0, textures = 0, models = 0, scripts = 0;
    std::vector<ImportLine> lines;
    int errors() const;
    int warnings() const;
    bool ok() const { return blocks + items + mobs > 0; }
};

// Turns a Bedrock add-on (behaviour and resource packs, unpacked into `bedrockDir` by the Java side)
// into VoxelCraft content in `packDir`: content.json (the Behavior Voxel: blocks, items, mobs),
// textures/mod/*.png (the Resource Voxel) and manifest.json. Simple blocks, items with icons, entities
// with health, texture, drops and a body close to their geometry come across; scripts and UI do not
// (the add-on is marked un-UI). Every step is written to import_report.txt, so nothing fails silently.
BedrockImportReport convertBedrockAddon(const std::string& bedrockDir, const std::string& packDir);

// The report lines of a pack ("+|file|text" per line), empty if it has none.
std::vector<ImportLine> readImportLines(const std::string& packDir);

}  // namespace vc
