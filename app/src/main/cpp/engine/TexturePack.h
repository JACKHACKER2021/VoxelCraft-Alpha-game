#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

struct Atlas {
    std::vector<uint8_t> rgba;
    int size = 0;  // square, 16x16 tiles
    int tilesFromPack = 0;
};

// Where a tile lives inside a texture pack, e.g. "blocks/stone.png".
std::string tileFilePath(int tile);

// Our own procedural placeholder textures (256x256, 16px tiles).
std::vector<uint8_t> buildFallbackAtlas(int& sizeOut);

// 1.5.1: the animated eyes (16x16 RGBA): the Eye Log (kind 0) looks around and blinks; the Void
// Brick Eye (kind 1) pulses and narrows its slit. `time` in seconds; the game redraws them a few
// times a second (Renderer::updateAtlasTile).
void drawEyeTile(uint8_t out[16 * 16 * 4], int kind, float time);
// 1.5.1 Experimental Features, Ore Redesign (1.6.0 Preview): the six ore tiles it replaces
// and their new 16x16 pictures.
constexpr int kOreRedesignCount = 6;
extern const int kOreRedesignTiles[kOreRedesignCount];
void oreRedesignTile(int index, uint8_t out[16 * 16 * 4]);

// Builds the terrain atlas from a texture pack that the player installed
// themselves. Our code never ships any of these textures.
//
// The directory is filled by the Java side (VoxelActivity) from a picked
// .zip/.mcpack/.png and may contain:
//   terrain.png      classic 16x16 atlas (Classic, Java Beta, old Pocket Edition)
//   blocks/<name>    single block textures (Bedrock 1.x or Java 1.5+ packs, .png or .tga)
//   items/<name>     single item textures
// Any tile the pack does not provide keeps its placeholder from `fallback`
// (a 16px-per-tile atlas).
Atlas loadTexturePack(const std::string& dir, const std::vector<uint8_t>& fallback, int fallbackSize);

// Puts one PNG into a tile of the atlas (mods' textures); animated strips give their first frame.
bool blitTextureFile(Atlas& atlas, int tile, const std::string& path);
// Size of a PNG/TGA file, or false if it cannot be read.
bool imageSize(const std::string& path, int& w, int& h);

}  // namespace vc
