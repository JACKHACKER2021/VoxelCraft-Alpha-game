// A Renderer for the trailer: it draws nothing itself but keeps everything the game
// hands it (see film_capture.h), plus the UI capture used by the snapshot tools.
#include "engine/Renderer.h"
#include "film_capture.h"
#include "../stub_capture.h"

namespace vc {
namespace capture {
UiList ui;
float guiW = 0, guiH = 0;
std::vector<uint8_t> atlas, gui;
int atlasSize = 0, guiW_px = 0, guiH_px = 0;
std::vector<uint8_t> skins, playerSkin;
int skinsW = 0, skinsH = 0, modelVertices = 0, particleVertices = 0, remoteModels = 0;
std::vector<Vertex> handSkin, handAtlas;
std::vector<uint8_t> map;
int mapW = 64, mapH = 64;
}  // namespace capture

namespace film {
Frame frame;
std::vector<uint8_t> playerSkin;
std::vector<uint8_t> remoteSkins[8];
namespace {
const World* gWorld = nullptr;
std::unordered_map<int64_t, ChunkMesh> gMeshes;
int64_t key(int cx, int cz) { return (int64_t)(((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz); }
}  // namespace
const World* meshedWorld() { return gWorld; }
const std::unordered_map<int64_t, ChunkMesh>& meshes() { return gMeshes; }
}  // namespace film

bool Renderer::init() { return true; }
void Renderer::shutdown() {}
void Renderer::resize(int width, int height) { width_ = width; height_ = height; }
void Renderer::setAtlas(const std::vector<uint8_t>& rgba, int size) { capture::atlas = rgba; capture::atlasSize = size; }
void Renderer::updateAtlasTile(int tile, const uint8_t* rgba16) {
    if (capture::atlasSize <= 0) return;
    const int t = capture::atlasSize / kAtlasCols;
    for (int y = 0; y < t; ++y)
        for (int x = 0; x < t; ++x) {
            uint8_t* d = &capture::atlas[((size_t)((tile / kAtlasCols) * t + y) * capture::atlasSize + (tile % kAtlasCols) * t + x) * 4];
            const uint8_t* s = rgba16 + ((y * 16 / t) * 16 + x * 16 / t) * 4;
            for (int k = 0; k < 4; ++k) d[k] = s[k];
        }
}
void Renderer::setGuiTexture(const std::vector<uint8_t>& rgba, int w, int h) {
    capture::gui = rgba;
    capture::guiW_px = w;
    capture::guiH_px = h;
}
int Renderer::updateMeshes(World& world, int) {
    // Unlike the phone, every dirty chunk is meshed at once: no holes in the footage.
    if (&world != film::gWorld) {
        film::gMeshes.clear();
        film::gWorld = &world;
    }
    int built = 0;
    for (auto& [k, c] : world.chunks())
        if (c->dirty && canMesh(world, c->cx, c->cz)) {
            ChunkMesh m;
            buildChunkMesh(world, *c, m);
            film::gMeshes[film::key(c->cx, c->cz)] = std::move(m);
            c->dirty = false;
            ++built;
        }
    return built;
}
void Renderer::dropUnloadedMeshes(const World& world) {
    if (&world != film::gWorld) return;
    for (auto it = film::gMeshes.begin(); it != film::gMeshes.end();) {
        int cx = (int)(it->first >> 32), cz = (int)(uint32_t)it->first;
        if (!world.getChunk(cx, cz)) it = film::gMeshes.erase(it);
        else ++it;
    }
}
void Renderer::clearMeshes() {
    film::gMeshes.clear();
    film::gWorld = nullptr;
}
void Renderer::setFogColors(const float* sky, const float* nether) {
    std::copy(sky, sky + 3, sky_);
    std::copy(nether, nether + 3, netherFog_);
}
void Renderer::setPlayerSkin(const std::vector<uint8_t>& rgba) { capture::playerSkin = film::playerSkin = rgba; }
void Renderer::setSkinAtlas(const std::vector<uint8_t>& rgba, int w, int h) {
    capture::skins = rgba;
    capture::skinsW = w;
    capture::skinsH = h;
}
void Renderer::setRemoteSkin(int slot, const std::vector<uint8_t>& rgba) {
    if (slot >= 0 && slot < 8) film::remoteSkins[slot] = rgba;
}
void Renderer::drawPlayerModel(const PlayerPose& pose, int remoteSkin) {
    film::frame.models.emplace_back(buildPlayerModel(pose), remoteSkin);
}
void Renderer::setMapTexture(const std::vector<uint8_t>& rgba, int width, int height) {
    capture::map = rgba;
    capture::mapW = width;
    capture::mapH = height;
}
void Renderer::drawParticles(const std::vector<Vertex>& verts) {
    film::frame.particles.insert(film::frame.particles.end(), verts.begin(), verts.end());
}
void Renderer::drawFirstPerson(const std::vector<Vertex>& skinVerts, const std::vector<Vertex>& atlasVerts) {
    film::frame.handSkin = skinVerts;
    film::frame.handAtlas = atlasVerts;
}
void Renderer::drawWorld(const Player& camera, Dimension dim, const RayHit&, float, float renderDistance, float fov) {
    film::Frame& f = film::frame;
    f.hasWorld = true;
    f.world = film::gWorld;
    f.camera = camera;
    f.dim = dim;
    f.renderDistance = renderDistance;
    f.fov = fov;
    std::copy(sky_, sky_ + 3, f.sky);
    std::copy(netherFog_, netherFog_ + 3, f.netherFog);
    std::copy(voidFog_, voidFog_ + 3, f.voidFog);
    f.brightness = brightness_;
    f.fogScale = fogScale_;
    f.sunMoon = skySunMoon_;
    f.stars = skyStars_;
    f.clouds = skyClouds_;
    f.starAlpha = starAlpha_;
    f.particles.clear();
    f.models.clear();
    f.handSkin.clear();
    f.handAtlas.clear();
}
void Renderer::drawUi(const UiList& list, float w, float h) {
    capture::ui = list;
    capture::guiW = w;
    capture::guiH = h;
}
}  // namespace vc
