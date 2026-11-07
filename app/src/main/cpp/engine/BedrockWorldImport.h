#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Blocks.h"

namespace vc {

struct BedrockWorldScanResult {
    bool ok = false;
    std::string name;
    std::vector<std::pair<std::string, int>> blocks;  // (minecraft block identifier, count)
    int subchunks = 0;
    std::string error;
};

// Scans a Bedrock world (.mcworld zip or folder), reads levelname.txt and unique blocks.
BedrockWorldScanResult scanBedrockWorld(const std::string& mcworldPath);

// Returns default recommended VoxelCraft ItemId for a given Minecraft block identifier.
ItemId defaultBedrockMapping(const std::string& mcName);

// Converts a Bedrock world (.mcworld) into VoxelCraft format in worldsDir.
bool convertBedrockWorld(const std::string& mcworldPath,
                        const std::string& worldsDir,
                        const std::unordered_map<std::string, ItemId>& mapping,
                        bool flatGenerator,
                        std::string& outFolder,
                        std::string& error,
                        std::function<void(float progress, const std::string& status)> onProgress = nullptr);

// Exports a VoxelCraft world into a Minecraft Bedrock .mcworld file.
bool exportBedrockWorld(const std::string& worldFolder, const std::string& outFile, std::string& error);

}  // namespace vc
