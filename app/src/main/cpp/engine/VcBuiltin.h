#pragma once
// 2.8.0 preview (the VC language in the engine): the built-in creatures whose bodies now come from
// GLB models (assets_src/models_redesign, packed into VcBuiltinModels.inc) instead of boxes written
// in C++. Their textures sit in the atlas from row kVcBuiltinRow; mods get the tiles after them.
#include <cstdint>
#include <vector>

#include "Blocks.h"
#include "Mobs.h"
#include "VcModel.h"

namespace vc {

constexpr int kVcBuiltinRow = 22, kVcBuiltinRows = 6;
static_assert(T_COUNT <= kVcBuiltinRow * kAtlasCols, "the built-in model textures go after the tiles");
// The first tile mods may use.
constexpr int kFirstModTile = kFeatures280 ? (kVcBuiltinRow + kVcBuiltinRows) * kAtlasCols : T_COUNT;

// The model of a built-in creature and where its texture lies in the atlas; null when it keeps the
// old body (the normal edition, or a creature not redesigned yet).
const VcModel* builtinMobModel(MobType type, VcTexRect& tex);
// Copies the models' textures into an atlas of `size` pixels (any tile size).
void blitBuiltinModelTextures(std::vector<uint8_t>& rgba, int size);

}  // namespace vc
