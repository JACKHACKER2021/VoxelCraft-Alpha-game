#pragma once
// The trailer camera: runs the real Game (menus, HUD, sessions) at 24 frames a second and
// paints each frame with the software renderer, the world first and the UI on top, so the
// footage shows exactly what the phone would draw.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "engine/Blocks.h"
#include "engine/Game.h"
#include "engine/Items.h"
#include "engine/Lang.h"
#include "engine/Minimap.h"
#include "engine/Save.h"
#include "engine/Session.h"
#include "../soft_render.h"
#include "../stub_capture.h"
#include "film_capture.h"

namespace fs = std::filesystem;

namespace {

constexpr float kFps = 24;
constexpr int kGuiW = 600, kGuiH = 270, kScale = 2;  // a 2400 x 1080 phone, painted at 1200 x 540

// The world as Renderer::drawWorld would draw it.
std::vector<uint8_t> paintWorld(const film::Frame& f) {
    gImageW = kGuiW * kScale;
    gImageH = kGuiH * kScale;
    Scene s;
    s.world = f.world;
    const float rd = f.renderDistance + 16;
    for (const auto& [k, m] : film::meshes()) {
        int cx = (int)(k >> 32), cz = (int)(uint32_t)k;
        float dx = cx * 16 + 8 - f.camera.pos.x, dz = cz * 16 + 8 - f.camera.pos.z;
        if (dx * dx + dz * dz > rd * rd) continue;
        s.opaque.insert(s.opaque.end(), m.opaque.begin(), m.opaque.end());
        s.translucent.insert(s.translucent.end(), m.translucent.begin(), m.translucent.end());
    }
    s.particles = f.particles;
    s.particles.insert(s.particles.end(), f.clouds.begin(), f.clouds.end());
    s.sky = f.sunMoon;
    if (f.starAlpha > 0.3f) s.sky.insert(s.sky.end(), f.stars.begin(), f.stars.end());
    for (const auto& [verts, slot] : f.models) {
        const std::vector<uint8_t>& skin = slot >= 0 && slot < 8 && film::remoteSkins[slot].size() == 64 * 64 * 4
                                               ? film::remoteSkins[slot] : film::playerSkin;
        s.extraModels.emplace_back(verts, skin);
    }
    s.modelSkin = film::playerSkin;
    s.handSkin = f.handSkin;
    s.handAtlas = f.handAtlas;
    // Fog, as in Renderer::drawWorld.
    const float waterFog[3] = {0.1f, 0.2f, 0.55f}, lavaFog[3] = {0.8f, 0.25f, 0.0f};
    const float* fog = f.sky;
    float start = f.renderDistance * 0.6f, end = f.renderDistance;
    if (f.camera.headInLava) {
        fog = lavaFog, start = 0, end = 3;
    } else if (f.camera.headInWater) {
        fog = waterFog, start = 2, end = 16;
    } else if (f.dim == Dimension::Nether) {
        fog = f.netherFog, start = 8, end = f.renderDistance * 0.75f;
    } else if (f.dim == Dimension::Void) {
        fog = f.voidFog, start = 10, end = f.renderDistance * 0.8f;
    } else {
        start *= f.fogScale < 1 ? f.fogScale * 0.5f : 1.0f;
        end *= f.fogScale;
    }
    std::copy(fog, fog + 3, gSky);
    gFogStart = start;
    gFogEnd = end;
    gBrightness = f.brightness;
    return renderRgba(s, f.camera, capture::atlas, capture::atlasSize, f.fov);
}

// The UI list on top of `base` (W x H rgba), like the UI shader.
void paintUi(std::vector<uint8_t>& base, int W, int H, float scale) {
    const auto& L = capture::ui;
    auto sample = [&](int tex, float u, float v, float out[4]) {
        const std::vector<uint8_t>& t = tex == TexAtlas ? capture::atlas : tex == TexSkins ? capture::skins
                                        : tex == TexMap ? capture::map : capture::gui;
        int tw = tex == TexAtlas ? capture::atlasSize : tex == TexSkins ? capture::skinsW : tex == TexMap ? capture::mapW : capture::guiW_px;
        int th = tex == TexAtlas ? capture::atlasSize : tex == TexSkins ? capture::skinsH : tex == TexMap ? capture::mapH : capture::guiH_px;
        if (t.size() < (size_t)tw * th * 4) {
            out[0] = out[1] = out[2] = out[3] = 0;
            return;
        }
        int x = std::clamp((int)(u * tw), 0, tw - 1), y = std::clamp((int)(v * th), 0, th - 1);
        for (int i = 0; i < 4; ++i) out[i] = t[(y * tw + x) * 4 + i] / 255.0f;
    };
    size_t first = 0;
    for (auto [tex, count] : L.runs) {
        for (size_t i = first; i + 2 < first + count; i += 3) {
            const UiVert* v[3] = {&L.verts[i], &L.verts[i + 1], &L.verts[i + 2]};
            float x0 = std::min({v[0]->x, v[1]->x, v[2]->x}) * scale, x1 = std::max({v[0]->x, v[1]->x, v[2]->x}) * scale;
            float y0 = std::min({v[0]->y, v[1]->y, v[2]->y}) * scale, y1 = std::max({v[0]->y, v[1]->y, v[2]->y}) * scale;
            float ax = v[0]->x * scale, ay = v[0]->y * scale, bx = v[1]->x * scale, by = v[1]->y * scale;
            float cx = v[2]->x * scale, cy = v[2]->y * scale;
            float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (std::fabs(area) < 1e-6f) continue;
            for (int py = std::max(0, (int)y0); py < std::min(H, (int)std::ceil(y1)); ++py)
                for (int px = std::max(0, (int)x0); px < std::min(W, (int)std::ceil(x1)); ++px) {
                    float sx = px + 0.5f, sy = py + 0.5f;
                    float w0 = ((bx - sx) * (cy - sy) - (by - sy) * (cx - sx)) / area;
                    float w1 = ((cx - sx) * (ay - sy) - (cy - sy) * (ax - sx)) / area;
                    float w2 = 1 - w0 - w1;
                    if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                    float c[4] = {1, 1, 1, 1};
                    if (tex != TexNone) {
                        float u = w0 * v[0]->u + w1 * v[1]->u + w2 * v[2]->u, vv = w0 * v[0]->v + w1 * v[1]->v + w2 * v[2]->v;
                        sample(tex, u, vv, c);
                    }
                    float r = c[0] * v[0]->r, g = c[1] * v[0]->g, b = c[2] * v[0]->b, a = c[3] * v[0]->a;
                    uint8_t* p = &base[(py * W + px) * 4];
                    p[0] = (uint8_t)std::clamp(p[0] * (1 - a) + r * a * 255, 0.0f, 255.0f);
                    p[1] = (uint8_t)std::clamp(p[1] * (1 - a) + g * a * 255, 0.0f, 255.0f);
                    p[2] = (uint8_t)std::clamp(p[2] * (1 - a) + b * a * 255, 0.0f, 255.0f);
                }
        }
        first += count;
    }
}

// Looks at a point: yaw and pitch like Player uses them.
float yawTo(const Vec3& from, const Vec3& to) { return std::atan2(to.x - from.x, -(to.z - from.z)); }
float pitchTo(const Vec3& from, const Vec3& to) {
    Vec3 d = to - from;
    return std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z));
}
float smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
float angleMix(float a, float b, float t) { return a + std::remainder(b - a, 2 * kPi) * t; }

// Drives the game and films it.
struct Director {
    fs::path out, data;
    std::unique_ptr<Game> game;
    double t = 1;
    fs::path shotDir;
    int shotFrame = 0;
    bool recording = false;
    int stepsPerFrame = 1;  // > 1 = time-lapse

    Director(const fs::path& outDir, const std::string& name, bool keepSplash = false) : out(outDir) {
        data = fs::temp_directory_path() / ("vc_film_" + name);
        fs::remove_all(data);
        fs::create_directories(data);
        game = std::make_unique<Game>(data.string());
        if (!keepSplash) game->skipSplash();
        game->toolSettings().language = "en";
        game->toolSettings().tutorialStep = 99;  // no first-step hints in the footage
        setLanguage(Language::English);
        game->onSurfaceCreated();
        game->onResize(kGuiW * 4, kGuiH * 4);
        idle(2);
    }

    ~Director() {
        game.reset();
        fs::remove_all(data);
    }

    Session& s() { return *game->session(); }
    Settings& settings() { return game->toolSettings(); }

    void shot(const std::string& name) {
        shotDir = out / name;
        fs::remove_all(shotDir);
        fs::create_directories(shotDir);
        shotFrame = 0;
        recording = true;
        std::printf("shot %s\n", name.c_str());
        std::fflush(stdout);
    }
    void cut() { recording = false; }

    void paint() {
        std::vector<uint8_t> img;
        const int W = kGuiW * kScale, H = kGuiH * kScale;
        if (film::frame.hasWorld) img = paintWorld(film::frame);
        else img.assign((size_t)W * H * 4, 0);
        for (size_t i = 3; i < img.size(); i += 4) img[i] = 255;
        paintUi(img, W, H, (float)W / capture::guiW);
        char file[32];
        std::snprintf(file, sizeof file, "f_%05d.png", shotFrame++);
        writePng((shotDir / file).string(), W, H, img);
    }

    // One frame of film (several game steps in a time-lapse); `each` runs before every step.
    void frame(const std::function<void()>& each = {}) {
        for (int k = 0; k < stepsPerFrame; ++k) {
            if (each) each();
            film::frame.hasWorld = false;
            t += 1 / kFps;
            game->frame(t);
        }
        if (recording) paint();
    }
    void frames(int n, const std::function<void(int)>& each = {}) {
        for (int i = 0; i < n; ++i) frame([&] {
                if (each) each(i);
            });
    }
    void seconds(float s, const std::function<void(float)>& each = {}) {
        int n = (int)std::round(s * kFps);
        frames(n, [&](int i) {
            if (each) each(n > 1 ? i / (float)(n - 1) : 1.0f);
        });
    }
    // Runs the game without filming (loading, setting up).
    void idle(int n) {
        bool r = recording;
        recording = false;
        frames(n);
        recording = r;
    }

    // Touches in GUI coordinates (the game gets phone pixels).
    void down(int id, float gx, float gy) { game->onTouch(id, TouchAction::Down, gx * 4, gy * 4, t); }
    void move(int id, float gx, float gy) { game->onTouch(id, TouchAction::Move, gx * 4, gy * 4, t); }
    void up(int id, float gx, float gy) { game->onTouch(id, TouchAction::Up, gx * 4, gy * 4, t); }
    void tap(float gx, float gy, int after = 6) {
        down(0, gx, gy);
        frame();
        up(0, gx, gy);
        frames(after);
    }
    void say(const std::string& chat) { game->onTextResult(14, chat); }

    // HUD buttons (see the survival HUD snapshot).
    static constexpr float kUpX = 59.5f, kUpY = 177.5f, kJumpX = 573, kJumpY = 241, kCenterX = 300, kCenterY = 135;
    static constexpr float kMenuX = 391, kMenuY = 258;

    void walk(bool on) {
        if (on) down(7, kUpX, kUpY);
        else up(7, kUpX, kUpY);
    }
    // Turns the view smoothly towards a point over the next frames (call every frame).
    void look(const Vec3& at, float speed = 0.15f) {
        Player& p = s().player();
        p.yaw = angleMix(p.yaw, yawTo(p.eye(), at), speed);
        p.pitch = p.pitch + (pitchTo(p.eye(), at) - p.pitch) * speed;
    }
    void lookNow(const Vec3& at) { look(at, 1); }
    // Holds a finger in the middle of the screen (mining) until the block is gone.
    void mine(int x, int y, int z, int maxFrames = 96) {
        const Vec3 c(x + 0.5f, y + 0.5f, z + 0.5f);
        for (int i = 0; i < 6; ++i) frame([&] { look(c, 0.35f); });
        down(1, kCenterX, kCenterY);
        for (int i = 0; i < maxFrames && s().currentWorld().getBlock(x, y, z) != AIR; ++i) frame([&] { lookNow(c); });
        up(1, kCenterX, kCenterY);
        frames(4);
    }
    // Taps the screen centre while looking at a block face: places the held block next to it, or uses it.
    void use(const Vec3& at) {
        for (int i = 0; i < 4; ++i) frame([&] { look(at, 0.45f); });
        lookNow(at);
        tap(kCenterX, kCenterY, 3);
    }

    // A ready world (no menus filmed): survival or creative, with an optional generator tweak.
    void openWorld(const std::string& name, uint32_t seed, GameMode mode, const std::function<void(GenConfig&)>& tweak = {}) {
        WorldInfo info;
        info.folder = createWorldFolder((data / "worlds").string());
        info.name = name;
        info.seed = seed;
        info.mode = mode;
        info.allowCommands = true;
        GenConfig g = GenConfig::defaults();
        if (tweak) tweak(g);
        saveGenerator(info.folder, g);
        game->openWorld(info, PlayerState{});
        for (int i = 0; i < 400 && !(game->session() && film::frame.hasWorld); ++i) idle(1);
        idle(24);
    }
    void give(int slot, ItemId id, int count = 1) { s().inventory().slots[slot] = {id, (uint8_t)count, 0}; }
};

}  // namespace
