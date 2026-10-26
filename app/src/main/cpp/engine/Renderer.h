#pragma once
#include <GLES3/gl3.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Math.h"
#include "Mesher.h"
#include "Player.h"
#include "PlayerModel.h"
#include "Ui.h"
#include "World.h"

namespace vc {

class Renderer {
public:
    bool init();
    void shutdown();
    void resize(int width, int height);

    // Uploads the terrain atlas (256x256 RGBA, 16x16 tiles of 16px).
    void setAtlas(const std::vector<uint8_t>& rgba, int size);
    // Replaces one tile of the atlas with 16x16 RGBA pixels (scaled up for packs with larger tiles):
    // the animated eyes.
    void updateAtlasTile(int tile, const uint8_t* rgba16);
    void setGuiTexture(const std::vector<uint8_t>& rgba, int width, int height);
    // The skin worn by the player model (64x64 RGBA).
    void setPlayerSkin(const std::vector<uint8_t>& rgba);
    // All skins side by side for previews in menus (UiTex TexSkins).
    void setSkinAtlas(const std::vector<uint8_t>& rgba, int width, int height);
    // The minimap image (UiTex TexMap).
    void setMapTexture(const std::vector<uint8_t>& rgba, int width, int height);
    // Skins of other players in a LAN game, slots 0..kRemoteSkins-1.
    static constexpr int kRemoteSkins = 8;
    void setRemoteSkin(int slot, const std::vector<uint8_t>& rgba);
    // Draws a player model into the scene of the last drawWorld call: our skin, or a remote one.
    void drawPlayerModel(const PlayerPose& pose, int remoteSkin = -1);
    // Draws block-break particles (camera-facing quads textured from the terrain atlas).
    void drawParticles(const std::vector<Vertex>& verts);
    // Draws the first-person hand (view-space geometry from buildFirstPerson) over the
    // world with its own projection, so it never sinks into nearby blocks.
    void drawFirstPerson(const std::vector<Vertex>& skinVerts, const std::vector<Vertex>& atlasVerts);

    // Rebuilds up to `budget` dirty chunk meshes; returns how many were built.
    int updateMeshes(World& world, int budget);
    void dropUnloadedMeshes(const World& world);
    void clearMeshes();  // e.g. when switching dimensions

    // `camera` supplies the eye position, view direction and liquid state.
    // breakProgress in [0, 1] darkens the selected block while mining it.
    void drawWorld(const Player& camera, Dimension dim, const RayHit& selection, float breakProgress,
                   float renderDistance, float fovDegrees);
    // Draws GUI geometry laid out in a virtual guiWidth x guiHeight space.
    void drawUi(const UiList& list, float guiWidth, float guiHeight);
    // Sky colour of the overworld and fog colour of the Nether (from the world's generator.json).
    void setFogColors(const float sky[3], const float nether[3]);
    // The overworld sky for the next drawWorld calls: sun, moon and stars drawn behind
    // everything without fog, and the clouds after the terrain. Empty vectors = none.
    void setSky(const std::vector<Vertex>& sunMoon, const std::vector<Vertex>& stars, float starAlpha,
                const std::vector<Vertex>& clouds) {
        skySunMoon_ = sunMoon;
        skyStars_ = stars;
        starAlpha_ = starAlpha;
        skyClouds_ = clouds;
    }
    // Fog of the Crimson Void, which follows the biome the player stands in.
    void setVoidFog(const float fog[3]) { std::copy(fog, fog + 3, voidFog_); }
    // World brightness multiplier from the video settings.
    void setBrightness(float b) { brightness_ = b; }
    // Thicker fog above ground (the Halloween addon), 1 = normal.
    void setFogScale(float s) { fogScale_ = s; }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    struct GpuMesh {
        GLuint vao[2] = {}, vbo[2] = {};
        GLsizei count[2] = {};
    };
    void uploadPart(GpuMesh& m, int part, const std::vector<Vertex>& verts);
    void freeMesh(GpuMesh& m);
    // Streams triangles with the world shader into the last drawWorld scene.
    void drawDynamic(const std::vector<Vertex>& verts, GLuint texture, const Mat4* mvp = nullptr);
    void streamDraw(const std::vector<Vertex>& verts);  // with the current program and uniforms
    std::vector<Vertex> skySunMoon_, skyStars_, skyClouds_;
    float starAlpha_ = 0;

    int width_ = 1, height_ = 1;
    int atlasSize_ = 0;
    GLuint worldProg_ = 0, uiProg_ = 0, atlas_ = 0, gui_ = 0, skin_ = 0, skinAtlas_ = 0, map_ = 0;
    GLuint remoteSkins_[kRemoteSkins] = {};
    GLuint modelVao_ = 0, modelVbo_ = 0;
    Mat4 lastMvp_;
    GLint uMvp_ = -1, uFogColor_ = -1, uFogRange_ = -1, uCamPos_ = -1, uAlpha_ = -1;
    GLint uUiProj_ = -1, uUiUseTex_ = -1, uBrightness_ = -1;
    float sky_[3] = {0.53f, 0.74f, 1.0f}, netherFog_[3] = {0.22f, 0.04f, 0.03f}, voidFog_[3] = {0.16f, 0.05f, 0.24f};
    float brightness_ = 1.0f;
    float fogScale_ = 1.0f;
    GLuint uiVao_ = 0, uiVbo_ = 0, lineVao_ = 0, lineVbo_ = 0;
    std::unordered_map<int64_t, GpuMesh> meshes_;
    ChunkMesh scratch_;
};

}  // namespace vc
