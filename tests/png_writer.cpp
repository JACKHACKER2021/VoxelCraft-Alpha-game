// The host-side tests use the engine's PNG writer.
#include "engine/Png.h"

void writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) { vc::writePng(path, w, h, rgba); }
