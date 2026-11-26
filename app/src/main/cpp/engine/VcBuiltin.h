#pragma once
// 2.8.0 preview (the VC language in the engine): every creature's body is a GLB file
// (models/mobs/<id>.glb, <id>_sit.glb for the sitting pose), read from the game's data
// (assets/vc, unpacked to <data>/vc) or from a pack that brings its own: nothing is built into the code,
// so everything can be edited. How a part moves is in the GLB node's "extras": {"vc": {"swing", "flap",
// "pivot"}}; a model without it gets its legs found by shape (guessVcLegs).
// The textures go into the atlas: tiles the atlas already has are shared (so texture packs still apply),
// new ones get room from row kVcBuiltinRow; mods get the tiles after that.
#include <cstdint>
#include <string>
#include <vector>

#include "Blocks.h"
#include "Mobs.h"
#include "VcModel.h"

namespace vc {

constexpr int kVcBuiltinRow = 22, kVcBuiltinRows = 6;
static_assert(T_COUNT <= kVcBuiltinRow * kAtlasCols, "the creature textures go after the tiles");
// The first tile mods may use.
constexpr int kFirstModTile = kFeatures280 ? (kVcBuiltinRow + kVcBuiltinRows) * kAtlasCols : T_COUNT;

// The file name of each creature's model (without .glb).
const char* vcMobModelId(MobType type);
// Folders that hold models/mobs/, the first one wins (packs first, the game's data last).
void setVcModelDirs(const std::vector<std::string>& dirs);
// Reads every creature's model and fits its texture into `atlas` (the built-in tiles, 16 pixels each);
// call it when the textures are built, before blitVcMobTextures. Returns how many models were read.
int loadVcMobModels(const std::vector<uint8_t>& atlas, int size, std::vector<std::string>* warnings = nullptr);
// Copies the new texture pieces into the finished atlas (any tile size).
void blitVcMobTextures(std::vector<uint8_t>& rgba, int size);
// The model of a creature (null: it keeps the old body, as in the normal edition). Its UVs are in the atlas.
const VcModel* builtinMobModel(MobType type, bool sitting = false);
// Tools that export the old bodies (tools/export_models.cpp) switch the GLB bodies off.
extern bool gVcLegacyMobs;

}  // namespace vc
