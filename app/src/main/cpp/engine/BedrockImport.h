#pragma once
#include <string>
#include <vector>

namespace vc {

// Report of converting a Minecraft Bedrock addon / mod into a VoxelCraft pack.
struct BedrockImportReport {
    std::string name;
    int blocks = 0, items = 0, recipes = 0, textures = 0;
    std::vector<std::string> notes;
    bool ok() const { return blocks + items + recipes > 0; }
};

// Converts an unpacked Bedrock addon / mod directory (containing manifest.json,
// blocks/*.json, items/*.json, textures/, recipes/, texts/) into VoxelCraft pack format
// inside `packDir`: manifest.json, content.json, textures/mod/*.png, recipes/bedrock_*.json.
BedrockImportReport convertBedrockAddon(const std::string& bedrockDir, const std::string& packDir);

// Checks if a directory contains a Bedrock addon manifest or structure.
bool isBedrockAddon(const std::string& dir);

}  // namespace vc
