#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "Math.h"
#include "Mesher.h"

namespace vc {

// 3D models of flat items (tools, ingots, flowers): the 16x16 sprite is extruded
// one pixel deep, with side walls along every edge of the opaque pixels, like
// held items in the classic games. Built from the atlas the game really uses, so
// texture packs change the shape too.
void setItemModelAtlas(const std::vector<uint8_t>& rgba, int size);

// Appends the model of atlas `tile`, 1 unit wide (x and y in -0.5..0.5, depth
// `depth` centred on z = 0), each corner passed through `map`.
void appendItemModel(int tile, float light, float depth, const std::function<Vec3(const Vec3&)>& map,
                     std::vector<Vertex>& out);

}  // namespace vc
