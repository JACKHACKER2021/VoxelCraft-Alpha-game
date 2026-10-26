#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

// Writes an RGBA image as an (uncompressed) PNG. Returns false on I/O errors.
bool writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

}  // namespace vc
