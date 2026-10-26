#include "Shaders.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Json.h"

namespace vc {

namespace fs = std::filesystem;

const char* kClassicWorldFs = R"(#version 300 es
precision mediump float;
uniform sampler2D u_atlas;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform float u_alpha;
uniform float u_brightness;
in vec2 v_uv;
in float v_light;
in float v_dist;
in float v_block;
out vec4 o_color;
void main() {
    vec4 c = texture(u_atlas, v_uv);
    if (u_alpha >= 1.0 && c.a < 0.5) discard;
    float fog = clamp((v_dist - u_fogRange.x) / (u_fogRange.y - u_fogRange.x), 0.0, 1.0);
    // The sky's light dims at night; lamps and lava keep their warm light.
    vec3 lit = max(vec3(v_light * u_brightness), vec3(v_block, v_block * 0.9, v_block * 0.72));
    o_color = vec4(mix(min(c.rgb * lit, vec3(1.0)), u_fogColor, fog), c.a * u_alpha);
})";

const char* kVibrantWorldFs = R"(#version 300 es
precision mediump float;
uniform sampler2D u_atlas;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform float u_alpha;
uniform float u_brightness;
uniform float u_time;
uniform vec2 u_screen;
uniform float u_saturation;
uniform float u_contrast;
uniform float u_warmth;
uniform float u_vignette;
uniform float u_glow;
uniform vec3 u_shade;
uniform float u_fogStrength;
uniform highp sampler2DShadow u_shadowMap;
uniform float u_shadowStrength;
in vec2 v_uv;
in float v_light;
in float v_dist;
in float v_block;
in vec4 v_shadow;
in float v_kind;
out vec4 o_color;
// 1.5.5: how much sun reaches this pixel (1 lit, 0 in the shadow of a block), soft-edged.
float sunVisible() {
    if (u_shadowStrength <= 0.0) return 1.0;
    vec3 sp = v_shadow.xyz / v_shadow.w * 0.5 + 0.5;
    float edge = max(abs(sp.x - 0.5), abs(sp.y - 0.5)) * 2.0;
    if (edge >= 1.0 || sp.z >= 1.0) return 1.0;
    const float t = 1.0 / 1024.0;
    float z = sp.z - 0.0012;
    float s = texture(u_shadowMap, vec3(sp.xy + vec2(-0.6, -0.6) * t, z)) + texture(u_shadowMap, vec3(sp.xy + vec2(0.6, -0.6) * t, z))
            + texture(u_shadowMap, vec3(sp.xy + vec2(-0.6, 0.6) * t, z)) + texture(u_shadowMap, vec3(sp.xy + vec2(0.6, 0.6) * t, z));
    return mix(s * 0.25, 1.0, smoothstep(0.8, 1.0, edge));
}
void main() {
    vec4 c = texture(u_atlas, v_uv);
    if (u_alpha >= 1.0 && c.a < 0.5) discard;
    // Shadows only matter where the sky reaches: caves are dark already.
    // Leaves let some light through: they are shaded less by the leaves above them.
    float leaf = v_kind > 0.5 && v_kind < 1.5 ? 0.45 : 1.0;
    float sunVis = mix(1.0, sunVisible(), u_shadowStrength * leaf * smoothstep(0.35, 0.75, v_light));
    float sun = v_light * u_brightness * (0.58 + 0.42 * sunVis);
    // Out of the sun the light turns cool; in full sunlight it warms up.
    vec3 sky = vec3(sun) * mix(u_shade, vec3(1.0), smoothstep(0.25, 0.9, v_light) * sunVis);
    sky *= mix(vec3(1.0), vec3(1.0 + u_warmth, 1.0 + u_warmth * 0.35, 1.0 - u_warmth * 0.45), smoothstep(0.7, 1.0, v_light));
    // Lamps, lava and torches: a warm light that softly flickers.
    float flicker = 1.0 + 0.04 * sin(u_time * 7.0 + v_dist * 0.7);
    vec3 lamp = vec3(v_block, v_block * 0.86, v_block * 0.62) * flicker;
    vec3 col = c.rgb * max(sky, lamp) + c.rgb * lamp * u_glow * (1.0 - v_light);
    float g = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(g), col, u_saturation);
    col = (col - 0.5) * u_contrast + 0.5;
    col = clamp(col, 0.0, 1.0);
    float fog = clamp((v_dist - u_fogRange.x) / (u_fogRange.y - u_fogRange.x) * u_fogStrength, 0.0, 1.0);
    fog = fog * fog * (3.0 - 2.0 * fog);
    col = mix(col, u_fogColor, fog);
    vec2 q = gl_FragCoord.xy / u_screen - 0.5;
    col *= 1.0 - u_vignette * dot(q, q) * 1.6;
    o_color = vec4(col, c.a * u_alpha);
})";

const char* kShaderPackGuide = R"(VoxelCraft shader packs (1.5.5)

A shader pack is a normal pack (Settings > Packs) with a "shaders" folder:

  shaders/shader.json   numbers for the Vibrant shader:
      "name"          shown in the video settings
      "saturation"    0.5 .. 2     colour strength (1 = unchanged)
      "contrast"      0.5 .. 1.6
      "warmth"        -0.3 .. 0.4  warm (positive) or cold sunlight
      "vignette"      0 .. 0.8     darker corners
      "glow"          0 .. 1       extra light around lamps and lava
      "shade"         [r, g, b]    tint of places out of the sun
      "fog_tint"      [r, g, b]    multiplies the fog colour
      "fog_strength"  0.3 .. 2

  shaders/world.fsh     optional, a fragment shader of your own in GLSL ES 3.00
      (#version 300 es). It receives:
        in vec2 v_uv; in float v_light (sky light 0..1); in float v_dist (distance);
        in float v_block (lamp light 0..1)
        uniform sampler2D u_atlas; vec3 u_fogColor; vec2 u_fogRange; float u_alpha;
        float u_brightness; float u_time; vec2 u_screen; in vec4 v_shadow and
        highp sampler2DShadow u_shadowMap with float u_shadowStrength (sun shadows, 0 = off);
        and every number above as
        u_saturation, u_contrast, u_warmth, u_vignette, u_glow, vec3 u_shade, u_fogStrength
      and writes "out vec4 o_color". Discard pixels with alpha < 0.5 when u_alpha >= 1.

Shader packs of other games (OptiFine/Iris "shaders/*.fsh" for desktop OpenGL) cannot run on
a phone's OpenGL ES. When such a zip is imported, its files are kept in shaders/original/ for
reference and the pack starts from the Vibrant numbers, ready to be tuned.
)";

namespace {

float clampNum(const Json& j, const char* key, float def, float lo, float hi) {
    return j.has(key) ? std::clamp((float)j[key].num(def), lo, hi) : def;
}

void readColor(const Json& j, const char* key, float out[3]) {
    if (!j.has(key) || !j[key].isArray()) return;
    for (int k = 0; k < 3; ++k) out[k] = std::clamp((float)j[key][k].num(out[k]), 0.0f, 2.0f);
}

}  // namespace

ShaderProfile loadShaderPack(const std::string& dir, std::vector<std::string>* warnings) {
    ShaderProfile p;
    const fs::path base = fs::path(dir) / "shaders";
    std::ifstream in((base / "shader.json").string());
    if (in) {
        std::stringstream ss;
        ss << in.rdbuf();
        Json j;
        std::string err;
        if (!Json::parse(ss.str(), j, err) || !j.isObject()) {
            if (warnings) warnings->push_back(fs::path(dir).filename().string() + "/shaders/shader.json: " + err);
        } else {
            p.name = j["name"].str(fs::path(dir).filename().string());
            p.saturation = clampNum(j, "saturation", p.saturation, 0.5f, 2.0f);
            p.contrast = clampNum(j, "contrast", p.contrast, 0.5f, 1.6f);
            p.warmth = clampNum(j, "warmth", p.warmth, -0.3f, 0.4f);
            p.vignette = clampNum(j, "vignette", p.vignette, 0.0f, 0.8f);
            p.glow = clampNum(j, "glow", p.glow, 0.0f, 1.0f);
            p.fogStrength = clampNum(j, "fog_strength", p.fogStrength, 0.3f, 2.0f);
            readColor(j, "shade", p.shade);
            readColor(j, "fog_tint", p.fogTint);
        }
    } else {
        p.name = fs::path(dir).filename().string();
    }
    std::ifstream fsh((base / "world.fsh").string());
    if (fsh) {
        std::stringstream ss;
        ss << fsh.rdbuf();
        p.fragment = ss.str();
        if (p.fragment.find("#version 300 es") == std::string::npos) {
            if (warnings) warnings->push_back(fs::path(dir).filename().string() + "/shaders/world.fsh: needs #version 300 es");
            p.fragment.clear();
        }
    }
    return p;
}

std::string shaderProfileJson(const ShaderProfile& p) {
    Json j = Json::object();
    j.set("name", Json::string(p.name));
    j.set("saturation", Json::number(p.saturation));
    j.set("contrast", Json::number(p.contrast));
    j.set("warmth", Json::number(p.warmth));
    j.set("vignette", Json::number(p.vignette));
    j.set("glow", Json::number(p.glow));
    Json shade = Json::array(), fog = Json::array();
    for (int k = 0; k < 3; ++k) shade.push(Json::number(p.shade[k])), fog.push(Json::number(p.fogTint[k]));
    j.set("shade", shade);
    j.set("fog_tint", fog);
    j.set("fog_strength", Json::number(p.fogStrength));
    return j.dump();
}

namespace {
void writeText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path.string(), std::ios::binary) << text;
}
}  // namespace

bool convertImportedShaderPack(const std::string& packDir, const std::string& name) {
    const fs::path base = fs::path(packDir) / "shaders";
    if (!fs::is_directory(base) || fs::exists(base / "shader.json")) return false;
    const fs::path original = base / "original";
    std::error_code ec;
    fs::create_directories(original, ec);
    for (const auto& e : fs::directory_iterator(base, ec)) {
        if (e.path() == original) continue;
        fs::rename(e.path(), original / e.path().filename(), ec);
    }
    ShaderProfile p;
    p.name = name;
    writeText(base / "shader.json", shaderProfileJson(p));
    writeText(base / "README.txt", kShaderPackGuide);
    return true;
}

void writeShaderPackFiles(const std::string& packDir, const std::string& name) {
    const fs::path base = fs::path(packDir) / "shaders";
    ShaderProfile p;
    p.name = name;
    writeText(base / "shader.json", shaderProfileJson(p));
    writeText(base / "README.txt", kShaderPackGuide);
    // Rename to world.fsh to start from the Vibrant shader's own code.
    writeText(base / "world.fsh.example", kVibrantWorldFs);
}

}  // namespace vc
