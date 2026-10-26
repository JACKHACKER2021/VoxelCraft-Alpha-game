#pragma once
// What the game asked the renderer to draw in the last frame, kept in full so the
// trailer tool can paint it with the software renderer (tests/soft_render.h).
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/Shaders.h"
#include "engine/Mesher.h"
#include "engine/Player.h"
#include "engine/PlayerModel.h"
#include "engine/World.h"

namespace vc::film {

struct Frame {
    bool hasWorld = false;
    const World* world = nullptr;
    Player camera;
    Dimension dim = Dimension::Overworld;
    float renderDistance = 64, fov = 70;
    float sky[3] = {0.53f, 0.74f, 1.0f}, netherFog[3] = {0.22f, 0.04f, 0.03f}, voidFog[3] = {0.16f, 0.05f, 0.24f};
    float brightness = 1, fogScale = 1, starAlpha = 0;
    Vec3 sunDir{0, 1, 0};  // 1.5.5: the light that casts shadows, its strength, the player's effects
    float sunStrength = 0;
    bool shadows = true, waves = true;
    std::vector<Vertex> sunMoon, stars, clouds, particles, handSkin, handAtlas;
    std::vector<std::pair<std::vector<Vertex>, int>> models;  // player models and their skin slot (-1 = ours)
};
extern Frame frame;
extern std::vector<uint8_t> playerSkin;
extern std::vector<uint8_t> remoteSkins[8];
extern GraphicsMode graphics;  // 1.5.5: the shader the game chose
extern ShaderProfile shader;
extern int frameCount;

// Chunk meshes of the world being drawn, built like the real renderer does.
const World* meshedWorld();
const std::unordered_map<int64_t, ChunkMesh>& meshes();

}  // namespace vc::film
