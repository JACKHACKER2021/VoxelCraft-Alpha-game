#pragma once
#include <string>
#include <vector>

#include "Crafting.h"
#include "GenConfig.h"

namespace vc {

struct PackInfo {
    std::string id;    // folder name
    std::string dir;   // absolute path
    std::string name, description, version;
    bool enabled = false;
    bool builtin = false;  // the "default" pack
};

// Packs (mod packs / add-ons) live in <dataDir>/packs/<id>/ and may contain:
//   manifest.json             {"name", "description", "version"}
//   biomes/*.json             biomes, merged by "id" ("enabled": false removes one)
//   dimensions/overworld.json keys override earlier packs
//   dimensions/nether.json    keys override earlier packs
//   recipes/*.json            recipes, smelting and fuels ("replace": true starts from scratch)
//   textures/                 terrain.png, blocks/, items/ like a texture pack
// Enabled packs are applied in order; the order is stored in packs.json.
class PackManager {
public:
    explicit PackManager(const std::string& dataDir);

    void ensureDefaultPack();
    void resetDefaultPack();
    std::vector<PackInfo> list() const;
    void setEnabled(const std::string& id, bool enabled);
    void move(const std::string& id, int delta);  // change the order of enabled packs
    std::string createPack(const std::string& name);  // a copy of the default content, returns its id
    void deletePack(const std::string& id);

    std::vector<std::string> files(const std::string& id) const;  // relative paths of .json files
    std::string readFile(const std::string& id, const std::string& path) const;
    bool writeFile(const std::string& id, const std::string& path, const std::string& text);
    std::string packDir(const std::string& id) const { return root_ + "/" + id; }

    // Merges all enabled packs. Problems in files are reported as warnings.
    GenConfig buildConfig(std::vector<std::string>* warnings = nullptr) const;
    RecipeBook buildRecipes(std::vector<std::string>* warnings = nullptr) const;
    std::vector<std::string> textureDirs() const;  // enabled packs that ship textures, in order

private:
    std::vector<std::string> enabledOrder() const;
    void saveOrder(const std::vector<std::string>& order);
    std::string root_, statePath_;
};

}  // namespace vc
