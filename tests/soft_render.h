#pragma once
// Software renderer for the 3D world (shared by world_snapshots and trailer_scenes): it
// draws exactly the triangles the game uploads (chunk meshes, particles, models) with the
// same alpha test, light and fog rules as the GLSL shader in Renderer.cpp.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/Mesher.h"
#include "engine/Player.h"
#include "engine/World.h"

void writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

namespace {
using namespace vc;

float gSky[3] = {0.53f, 0.74f, 1.0f};  // clear colour and fog colour
float gFogStart = 48, gFogEnd = 80;
float gBrightness = 1;  // like the shader's u_brightness (night, rain)

struct Image {
    int w, h;
    std::vector<float> color;  // rgb
    std::vector<float> depth;
};

struct Texture {
    const std::vector<uint8_t>* rgba;
    int w, h;
};

struct Camera {
    Mat4 mvp;
    Vec3 eye;
    float fogStart, fogEnd, fog[3];
};

void drawTriangles(Image& img, const Camera& cam, const std::vector<Vertex>& v, const Texture& tex, float alpha) {
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
        float sx[3], sy[3], sz[3], sw[3];
        bool behind = false;
        for (int k = 0; k < 3; ++k) {
            const Vertex& p = v[i + k];
            const float* m = cam.mvp.m;
            float cx = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
            float cy = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
            float cz = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
            float cw = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
            if (cw < 0.05f) behind = true;
            sw[k] = cw;
            sx[k] = (cx / cw * 0.5f + 0.5f) * img.w;
            sy[k] = (1 - (cy / cw * 0.5f + 0.5f)) * img.h;
            sz[k] = cz / cw;
        }
        if (behind) continue;
        float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sy[1] - sy[0]) * (sx[2] - sx[0]);
        if (std::fabs(area) < 1e-6f) continue;
        int x0 = std::max(0, (int)std::floor(std::min({sx[0], sx[1], sx[2]})));
        int x1 = std::min(img.w - 1, (int)std::ceil(std::max({sx[0], sx[1], sx[2]})));
        int y0 = std::max(0, (int)std::floor(std::min({sy[0], sy[1], sy[2]})));
        int y1 = std::min(img.h - 1, (int)std::ceil(std::max({sy[0], sy[1], sy[2]})));
        for (int py = y0; py <= y1; ++py)
            for (int px = x0; px <= x1; ++px) {
                float qx = px + 0.5f, qy = py + 0.5f;
                float w0 = ((sx[1] - qx) * (sy[2] - qy) - (sy[1] - qy) * (sx[2] - qx)) / area;
                float w1 = ((sx[2] - qx) * (sy[0] - qy) - (sy[2] - qy) * (sx[0] - qx)) / area;
                float w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                float& dz = img.depth[py * img.w + px];
                if (z >= dz) continue;
                // Perspective-correct texture coordinates.
                float iw = w0 / sw[0] + w1 / sw[1] + w2 / sw[2];
                const Vertex &a = v[i], &b = v[i + 1], &c = v[i + 2];
                float u = (w0 * a.u / sw[0] + w1 * b.u / sw[1] + w2 * c.u / sw[2]) / iw;
                float t = (w0 * a.v / sw[0] + w1 * b.v / sw[1] + w2 * c.v / sw[2]) / iw;
                float wx = (w0 * a.x / sw[0] + w1 * b.x / sw[1] + w2 * c.x / sw[2]) / iw;
                float wz = (w0 * a.z / sw[0] + w1 * b.z / sw[1] + w2 * c.z / sw[2]) / iw;
                int tx = std::clamp((int)(u * tex.w), 0, tex.w - 1), ty = std::clamp((int)(t * tex.h), 0, tex.h - 1);
                const uint8_t* tc = &(*tex.rgba)[(ty * tex.w + tx) * 4];
                float ta = tc[3] / 255.0f;
                if (alpha >= 1 && ta < 0.5f) continue;  // the shader's alpha test
                float light = a.light;
                const float glow = w0 * a.block + w1 * b.block + w2 * c.block;  // lamps, as in the shader
                const float warm[3] = {glow, glow * 0.9f, glow * 0.72f};
                float dist = std::hypot(wx - cam.eye.x, wz - cam.eye.z);
                float fog = std::clamp((dist - cam.fogStart) / (cam.fogEnd - cam.fogStart), 0.0f, 1.0f);
                float* out = &img.color[(py * img.w + px) * 3];
                float blend = alpha >= 1 ? 1 : ta * alpha;
                for (int k = 0; k < 3; ++k) {
                    float col = std::min(1.0f, tc[k] / 255.0f * std::max(light * gBrightness, warm[k])) * (1 - fog) + cam.fog[k] * fog;
                    out[k] = out[k] * (1 - blend) + col * blend;
                }
                if (alpha >= 1) dz = z;
            }
    }
}

struct Scene {
    const World* world;
    std::vector<Vertex> opaque, translucent, particles, model;
    std::vector<uint8_t> modelSkin;
    std::vector<Vertex> handSkin, handAtlas;  // first-person hand in view space
    // More player models, each with its own skin (other players in the trailer).
    std::vector<std::pair<std::vector<Vertex>, std::vector<uint8_t>>> extraModels;
    std::vector<Vertex> sky;  // sun, moon, stars
};

int gImageW = 800, gImageH = 450;  // smaller for video frames
bool gQuiet = false;

std::vector<uint8_t> renderRgba(const Scene& s, const Player& camPlayer, const std::vector<uint8_t>& atlas, int atlasSize,
                                float fovDegrees = 70) {
    const int IW = gImageW, IH = gImageH;
    Image img{IW, IH, std::vector<float>(IW * IH * 3), std::vector<float>(IW * IH, 1e9f)};
    const float* sky = gSky;
    for (int i = 0; i < img.w * img.h; ++i)
        for (int k = 0; k < 3; ++k) img.color[i * 3 + k] = sky[k];
    Camera cam;
    cam.eye = camPlayer.eye();
    cam.mvp = Mat4::perspective(fovDegrees * kPi / 180, (float)IW / IH, 0.05f, 200) * Mat4::lookDir(cam.eye, camPlayer.forward(), {0, 1, 0});
    cam.fogStart = gFogStart;
    cam.fogEnd = gFogEnd;
    std::copy(sky, sky + 3, cam.fog);
    Texture tex{&atlas, atlasSize, atlasSize};
    if (!s.sky.empty()) {
        // Sun, moon and stars: behind everything, no fog, full brightness, no depth.
        Camera skyCam = cam;
        skyCam.fogStart = 1e5f;
        skyCam.fogEnd = 2e5f;
        float b = gBrightness;
        gBrightness = 1;
        drawTriangles(img, skyCam, s.sky, tex, 1);
        gBrightness = b;
        std::fill(img.depth.begin(), img.depth.end(), 1e9f);
    }
    drawTriangles(img, cam, s.opaque, tex, 1);
    drawTriangles(img, cam, s.particles, tex, 1);
    if (!s.model.empty()) drawTriangles(img, cam, s.model, Texture{&s.modelSkin, 64, 64}, 1);
    for (const auto& [verts, skin] : s.extraModels) drawTriangles(img, cam, verts, Texture{&skin, 64, 64}, 1);
    drawTriangles(img, cam, s.translucent, tex, 0.8f);
    if (!s.handSkin.empty() || !s.handAtlas.empty()) {
        // Like Renderer::drawFirstPerson: fresh depth, own projection, no fog.
        std::fill(img.depth.begin(), img.depth.end(), 1e9f);
        Camera hc;
        hc.eye = {0, 0, 0};
        hc.mvp = Mat4::perspective(70 * kPi / 180, (float)IW / IH, 0.02f, 10);
        hc.fogStart = 1000;
        hc.fogEnd = 2000;
        if (!s.handSkin.empty()) drawTriangles(img, hc, s.handSkin, Texture{&s.modelSkin, 64, 64}, 1);
        drawTriangles(img, hc, s.handAtlas, tex, 1);
    }
    std::vector<uint8_t> out(img.w * img.h * 4);
    for (int i = 0; i < img.w * img.h; ++i) {
        for (int k = 0; k < 3; ++k) out[i * 4 + k] = (uint8_t)std::clamp(img.color[i * 3 + k] * 255, 0.0f, 255.0f);
        out[i * 4 + 3] = 255;
    }
    return out;
}

void render(const Scene& s, const Player& camPlayer, const std::string& name, const std::vector<uint8_t>& atlas, int atlasSize) {
    writePng(name, gImageW, gImageH, renderRgba(s, camPlayer, atlas, atlasSize));
    if (!gQuiet) std::printf("wrote %s\n", name.c_str());
}

Scene meshWorld(World& w, int cx, int cz, int radius) {
    Scene s;
    s.world = &w;
    w.ensureChunksAround(cx, cz, radius + 1, 1000);
    ChunkMesh m;
    for (auto& [k, c] : w.chunks()) {
        int dx = c->cx - cx, dz = c->cz - cz;
        if (dx * dx + dz * dz > radius * radius || !canMesh(w, c->cx, c->cz)) continue;
        buildChunkMesh(w, *c, m);
        s.opaque.insert(s.opaque.end(), m.opaque.begin(), m.opaque.end());
        s.translucent.insert(s.translucent.end(), m.translucent.begin(), m.translucent.end());
    }
    return s;
}


}  // namespace
