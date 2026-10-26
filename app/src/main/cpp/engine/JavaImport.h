#pragma once
#include <string>
#include <vector>

namespace vc {

// What the Java mod / datapack importer made of a file, shown after the import.
struct JavaImportReport {
    std::string name;                 // the mod's or datapack's name
    int blocks = 0, items = 0, recipes = 0, textures = 0, drops = 0, names = 0;
    int skippedMinecraft = 0;         // files of Minecraft itself (assets/minecraft, data/minecraft): never used
    int skippedShapes = 0;            // blocks that are not simple cubes (stairs, plants, custom models)
    int skippedRecipes = 0;           // recipes with tags or unknown items
    int classes = 0;                  // compiled Java code: VoxelCraft cannot run it
    std::vector<std::string> notes;   // details, e.g. "skipped: mymod:pipe (shape)"
    bool ok() const { return blocks + items + recipes > 0; }
};

// Turns the files of a Java Edition mod (.jar) or datapack/resource pack (.zip), unpacked into
// `javaDir` by the Java side (assets/, data/, fabric.mod.json, META-INF/mods.toml, pack.mcmeta,
// classes.txt with the number of .class files), into VoxelCraft content in `packDir`:
// content.json, textures/mod/*.png, recipes/java_<namespace>.json and manifest.json.
// Simple cube blocks (cube_all, cube_column, cube_bottom_top), items with a flat texture,
// English and Czech names, shaped/shapeless/smelting recipes and block drops come across;
// behaviour written in Java code does not. The report is also saved as import_report.txt.
JavaImportReport convertJavaMod(const std::string& javaDir, const std::string& packDir);

// The saved report of a pack ("" if it was not imported from Java).
std::vector<std::string> readImportReport(const std::string& packDir);

}  // namespace vc
