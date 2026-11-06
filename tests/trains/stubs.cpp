#include <string>
#include <initializer_list>
#include "Blocks.h"
#include "Lang.h"
#include "World.h"
namespace vc {
int modTileFor(ItemId, Face) { return 0; }
std::string tr(const std::string& s) { return s; }
std::string trf(const std::string& s, std::initializer_list<std::string>) { return s; }
ItemId World::getBlock(int, int, int) const { return 0; }
Chunk* World::getChunk(int, int) const { return nullptr; }
}
