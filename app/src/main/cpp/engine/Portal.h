#pragma once
#include "Math.h"
#include "World.h"

namespace vc {

// Tries to light a Nether portal whose interior contains (x, y, z): an
// obsidian frame with an empty inside of 2x3 up to 21x21 blocks, standing
// along the X or the Z axis. Returns true if portal blocks were placed.
// `voidPortal`: a void stone frame filled with void portal blocks (lit with the
// void flint and steel), leading between the Nether and the Crimson Void.
bool tryLightPortal(World& world, int x, int y, int z, bool voidPortal = false);

// Removes every portal block connected to (x, y, z), e.g. after the frame broke.
void collapsePortal(World& world, int x, int y, int z);

// Looks for an existing portal near (x, z); if there is none, builds a new
// one at a safe spot. Returns the feet position for a player standing in it.
// Chunks around (x, z) must already be generated.
Vec3 findOrBuildPortal(World& world, int x, int z, bool voidPortal = false);

}  // namespace vc
