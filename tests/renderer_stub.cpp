// No-op Renderer so the game logic (menus, sessions) can be tested without a GPU.
#include "engine/Renderer.h"
#include "stub_capture.h"

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

bool Renderer::init() { return true; }
bool Renderer::setShader(GraphicsMode, const ShaderProfile&) { return true; }
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
int Renderer::updateMeshes(World& world, int budget) {
    int built = 0;
    for (auto& [k, c] : world.chunks())
        if (c->dirty && built < budget && canMesh(world, c->cx, c->cz)) { c->dirty = false; ++built; }
    return built;
}
void Renderer::dropUnloadedMeshes(const World&) {}
void Renderer::clearMeshes() {}
void Renderer::setFogColors(const float*, const float*) {}
void Renderer::setPlayerSkin(const std::vector<uint8_t>& rgba) { capture::playerSkin = rgba; }
void Renderer::setSkinAtlas(const std::vector<uint8_t>& rgba, int w, int h) {
    capture::skins = rgba;
    capture::skinsW = w;
    capture::skinsH = h;
}
void Renderer::setRemoteSkin(int, const std::vector<uint8_t>&) {}
void Renderer::drawPlayerModel(const PlayerPose& pose, int remoteSkin) {
    capture::modelVertices = (int)buildPlayerModel(pose).size();
    if (remoteSkin >= 0) ++capture::remoteModels;
}
void Renderer::setMapTexture(const std::vector<uint8_t>& rgba, int width, int height) {
    capture::map = rgba;
    capture::mapW = width;
    capture::mapH = height;
}
void Renderer::drawParticles(const std::vector<Vertex>& verts) { capture::particleVertices = (int)verts.size(); }
void Renderer::drawFirstPerson(const std::vector<Vertex>& skinVerts, const std::vector<Vertex>& atlasVerts) {
    capture::handSkin = skinVerts;
    capture::handAtlas = atlasVerts;
}
void Renderer::drawWorld(const Player&, Dimension, const RayHit&, float, float, float) {}
void Renderer::drawUi(const UiList& list, float w, float h) {
    capture::ui = list;
    capture::guiW = w;
    capture::guiH = h;
}
}  // namespace vc
