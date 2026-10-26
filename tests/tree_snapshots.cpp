// Close-ups of tree trunks, leaves and branches for checking the bark and the joints.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "engine/Blocks.h"
#include "engine/Mesher.h"
#include "engine/Player.h"
#include "engine/TexturePack.h"
#include "engine/World.h"
#include "engine/Trees.h"
#include "engine/GenConfig.h"
#include "engine/Particles.h"
#include "engine/PlayerModel.h"
#include "engine/Skin.h"
using namespace vc;
void writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);
#include "soft_render.h"
int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : "build/snapshots";
    int atlasSize = 0;
    std::vector<uint8_t> atlas = buildFallbackAtlas(atlasSize);
    GenConfig flat = GenConfig::defaults();
    flat.flat = true;
    World w(5, Dimension::Overworld, flat);
    w.ensureChunksAround(0, 0, 3, 1000);
    const TreeKind kinds[4] = {TreeKind::Oak, TreeKind::Pine, TreeKind::Void, TreeKind::Netherperk};
    for (int i = 0; i < 4; ++i) growTree(w, 2 + i * 9, 4, 4, kinds[i], 1234 + i);
    for (int k = 0; k < 6; ++k) w.setBlock(38, 4 + k, 4, NETHERPERK_LOG), w.setBlock(40, 4 + k, 4, VOID_LOG), w.setBlock(42, 4 + k, 4, LOG), w.setBlock(44, 4 + k, 4, SPRUCE_LOG);
    w.setBlock(40, 6, 4, EYE_LOG);
    Scene s = meshWorld(w, 1, 0, 3);
    Player cam;
    cam.pos = Vec3(16, 7.5f, 22);
    cam.yaw = 0; cam.pitch = -0.15f;
    render(s, cam, out + "/trees_all.png", atlas, atlasSize);
    cam.pos = Vec3(41.5f, 6.0f, 9.5f);
    cam.pitch = 0.05f;
    render(s, cam, out + "/trees_trunks.png", atlas, atlasSize);
    return 0;
}
