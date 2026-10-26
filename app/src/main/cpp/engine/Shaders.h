#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

// 1.5.5: how the world is shaded. Classic is the original shader of VoxelCraft; Enhanced is
// the Vibrant shader (richer colours, warm sunlight, cool shade, glowing lamps, soft vignette),
// tuned by the active shader pack.
enum class GraphicsMode : uint8_t { Classic = 0, Enhanced = 1 };

// The look of the Enhanced mode. A shader pack is a pack with
//   shaders/shader.json  {"name", "saturation", "contrast", "warmth", "vignette", "glow",
//                         "shade": [r, g, b], "fog_tint": [r, g, b], "fog_strength"}
//   shaders/world.fsh    optional: a whole GLSL ES 3.00 fragment shader of its own (see
//                        kShaderPackGuide for what it receives); if it fails to compile the
//                        game falls back to the Vibrant shader with these numbers.
struct ShaderProfile {
    std::string name = "Vibrant";
    float saturation = 1.22f;  // 1 = unchanged
    float contrast = 1.08f;
    float warmth = 0.10f;      // warm tint in full sunlight
    float vignette = 0.30f;    // darker corners
    float glow = 0.35f;        // extra warm light around lamps, lava and torches
    float shade[3] = {0.84f, 0.90f, 1.06f};    // tint of places out of the sun
    float fogTint[3] = {1.0f, 1.0f, 1.0f};     // multiplies the fog colour
    float fogStrength = 1.0f;                  // < 1 clearer air, > 1 hazier
    std::string fragment;      // world.fsh, empty = the Vibrant shader
};

// Reads the shader pack in `dir` (the pack folder); problems are reported as warnings and
// the numbers fall back to the Vibrant defaults.
ShaderProfile loadShaderPack(const std::string& dir, std::vector<std::string>* warnings = nullptr);
std::string shaderProfileJson(const ShaderProfile& p);  // the shader.json of a new pack

// After a zip with a shaders/ folder was unpacked into `packDir`: a VoxelCraft shader pack is
// left as it is; files of another game's shader pack (desktop GLSL that a phone cannot run)
// are moved to shaders/original/ and the pack gets a shader.json with the Vibrant numbers and a
// README. Returns true when it was such a foreign pack.
bool convertImportedShaderPack(const std::string& packDir, const std::string& name);
// Writes the files of a new shader pack (shader.json, README.txt, world.fsh.example) into
// <packDir>/shaders.
void writeShaderPackFiles(const std::string& packDir, const std::string& name);

// The fragment shaders of both modes; world.fsh files are checked against kShaderPackGuide.
extern const char* kClassicWorldFs;
extern const char* kVibrantWorldFs;
extern const char* kShaderPackGuide;

}  // namespace vc
