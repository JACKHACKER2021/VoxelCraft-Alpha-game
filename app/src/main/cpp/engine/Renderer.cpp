#include "Renderer.h"

#include <cstring>

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Blocks.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VoxelCraft", __VA_ARGS__)

namespace vc {

namespace {

const char* kWorldVs = R"(#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_light;
layout(location = 3) in float a_block;
uniform mat4 u_mvp;
uniform vec3 u_cam;
out vec2 v_uv;
out float v_light;
out float v_dist;
out float v_block;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_uv = a_uv;
    v_light = a_light;
    v_block = a_block;
    v_dist = length(a_pos.xz - u_cam.xz);
})";

const char* kWorldFs = R"(#version 300 es
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

const char* kUiVs = R"(#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
uniform mat4 u_proj;
out vec2 v_uv;
out vec4 v_color;
void main() {
    gl_Position = u_proj * vec4(a_pos, 1.0);
    v_uv = a_uv;
    v_color = a_color;
})";

const char* kUiFs = R"(#version 300 es
precision mediump float;
uniform sampler2D u_atlas;
uniform bool u_useTex;
in vec2 v_uv;
in vec4 v_color;
out vec4 o_color;
void main() {
    o_color = u_useTex ? texture(u_atlas, v_uv) * v_color : v_color;
})";

struct UiVertex {
    float x, y, z, u, v, r, g, b, a;
};


GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        LOGE("shader compile failed: %s", log);
    }
    return s;
}

GLuint link(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        LOGE("program link failed: %s", log);
        return 0;
    }
    return p;
}

void setupUiVao(GLuint vao, GLuint vbo) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)offsetof(UiVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)offsetof(UiVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)offsetof(UiVertex, r));
}

}  // namespace

bool Renderer::init() {
    worldProg_ = link(kWorldVs, kWorldFs);
    uiProg_ = link(kUiVs, kUiFs);
    if (!worldProg_ || !uiProg_) return false;
    uMvp_ = glGetUniformLocation(worldProg_, "u_mvp");
    uCamPos_ = glGetUniformLocation(worldProg_, "u_cam");
    uFogColor_ = glGetUniformLocation(worldProg_, "u_fogColor");
    uFogRange_ = glGetUniformLocation(worldProg_, "u_fogRange");
    uAlpha_ = glGetUniformLocation(worldProg_, "u_alpha");
    uBrightness_ = glGetUniformLocation(worldProg_, "u_brightness");
    uUiProj_ = glGetUniformLocation(uiProg_, "u_proj");
    uUiUseTex_ = glGetUniformLocation(uiProg_, "u_useTex");

    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);
    setupUiVao(uiVao_, uiVbo_);
    glGenVertexArrays(1, &lineVao_);
    glGenBuffers(1, &lineVbo_);
    setupUiVao(lineVao_, lineVbo_);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    return true;
}

void Renderer::shutdown() {
    clearMeshes();
    if (atlas_) glDeleteTextures(1, &atlas_);
    if (gui_) glDeleteTextures(1, &gui_);
    if (skin_) glDeleteTextures(1, &skin_);
    if (skinAtlas_) glDeleteTextures(1, &skinAtlas_);
    if (map_) glDeleteTextures(1, &map_);
    for (GLuint& t : remoteSkins_) {
        if (t) glDeleteTextures(1, &t);
        t = 0;
    }
    if (modelVao_) glDeleteVertexArrays(1, &modelVao_);
    if (modelVbo_) glDeleteBuffers(1, &modelVbo_);
    skin_ = skinAtlas_ = map_ = modelVao_ = modelVbo_ = 0;
    if (worldProg_) glDeleteProgram(worldProg_);
    if (uiProg_) glDeleteProgram(uiProg_);
    glDeleteVertexArrays(1, &uiVao_);
    glDeleteBuffers(1, &uiVbo_);
    glDeleteVertexArrays(1, &lineVao_);
    glDeleteBuffers(1, &lineVbo_);
    atlas_ = gui_ = worldProg_ = uiProg_ = uiVao_ = uiVbo_ = lineVao_ = lineVbo_ = 0;
}

void Renderer::resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    glViewport(0, 0, width_, height_);
}

void Renderer::setAtlas(const std::vector<uint8_t>& rgba, int size) {
    if (!atlas_) glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    atlasSize_ = size;
    // Nearest filtering keeps the crisp pixel-art look.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Renderer::updateAtlasTile(int tile, const uint8_t* rgba16) {
    if (!atlas_ || atlasSize_ <= 0) return;
    const int t = atlasSize_ / kAtlasCols;
    std::vector<uint8_t> px((size_t)t * t * 4);
    for (int y = 0; y < t; ++y)
        for (int x = 0; x < t; ++x)
            std::memcpy(&px[((size_t)y * t + x) * 4], rgba16 + ((y * 16 / t) * 16 + x * 16 / t) * 4, 4);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, (tile % kAtlasCols) * t, (tile / kAtlasCols) * t, t, t, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
}

void Renderer::uploadPart(GpuMesh& m, int part, const std::vector<Vertex>& verts) {
    if (!m.vao[part]) {
        glGenVertexArrays(1, &m.vao[part]);
        glGenBuffers(1, &m.vbo[part]);
        glBindVertexArray(m.vao[part]);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo[part]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, light));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, block));
    }
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo[part]);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    m.count[part] = (GLsizei)verts.size();
}

void Renderer::freeMesh(GpuMesh& m) {
    for (int i = 0; i < 2; ++i) {
        if (m.vao[i]) glDeleteVertexArrays(1, &m.vao[i]);
        if (m.vbo[i]) glDeleteBuffers(1, &m.vbo[i]);
    }
    m = {};
}

int Renderer::updateMeshes(World& world, int budget) {
    int built = 0;
    for (auto& [key, chunk] : world.chunks()) {
        if (built >= budget) break;
        if (!chunk->dirty || !canMesh(world, chunk->cx, chunk->cz)) continue;
        buildChunkMesh(world, *chunk, scratch_);
        GpuMesh& m = meshes_[key];
        uploadPart(m, 0, scratch_.opaque);
        uploadPart(m, 1, scratch_.translucent);
        chunk->dirty = false;
        ++built;
    }
    glBindVertexArray(0);
    return built;
}

void Renderer::clearMeshes() {
    for (auto& [k, m] : meshes_) freeMesh(m);
    meshes_.clear();
}

void Renderer::dropUnloadedMeshes(const World& world) {
    for (auto it = meshes_.begin(); it != meshes_.end();) {
        if (!world.chunks().count(it->first)) {
            freeMesh(it->second);
            it = meshes_.erase(it);
        } else {
            ++it;
        }
    }
}

void Renderer::setGuiTexture(const std::vector<uint8_t>& rgba, int width, int height) {
    if (!gui_) glGenTextures(1, &gui_);
    glBindTexture(GL_TEXTURE_2D, gui_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

namespace {
void uploadNearest(GLuint& tex, const std::vector<uint8_t>& rgba, int w, int h) {
    if (!tex) glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
}  // namespace

void Renderer::setPlayerSkin(const std::vector<uint8_t>& rgba) { uploadNearest(skin_, rgba, 64, 64); }

void Renderer::setSkinAtlas(const std::vector<uint8_t>& rgba, int width, int height) {
    uploadNearest(skinAtlas_, rgba, width, height);
}

void Renderer::setMapTexture(const std::vector<uint8_t>& rgba, int width, int height) {
    uploadNearest(map_, rgba, width, height);
}

void Renderer::setRemoteSkin(int slot, const std::vector<uint8_t>& rgba) {
    if (slot >= 0 && slot < kRemoteSkins && rgba.size() == 64 * 64 * 4) uploadNearest(remoteSkins_[slot], rgba, 64, 64);
}

void Renderer::drawPlayerModel(const PlayerPose& pose, int remoteSkin) {
    GLuint tex = remoteSkin >= 0 && remoteSkin < kRemoteSkins && remoteSkins_[remoteSkin] ? remoteSkins_[remoteSkin] : skin_;
    if (tex) drawDynamic(buildPlayerModel(pose), tex);
}

void Renderer::drawParticles(const std::vector<Vertex>& verts) {
    if (!verts.empty()) drawDynamic(verts, atlas_);
}

void Renderer::drawFirstPerson(const std::vector<Vertex>& skinVerts, const std::vector<Vertex>& atlasVerts) {
    if (skinVerts.empty() && atlasVerts.empty()) return;
    glClear(GL_DEPTH_BUFFER_BIT);
    // A fixed field of view: the hand keeps its size whatever the FOV setting is.
    Mat4 proj = Mat4::perspective(70 * kPi / 180, (float)width_ / std::max(1, height_), 0.02f, 10.0f);
    glUseProgram(worldProg_);
    glUniform3f(uCamPos_, 0, 0, 0);
    glUniform2f(uFogRange_, 1000, 2000);  // no fog on the hand
    if (skin_ && !skinVerts.empty()) drawDynamic(skinVerts, skin_, &proj);
    if (!atlasVerts.empty()) drawDynamic(atlasVerts, atlas_, &proj);
}

void Renderer::drawDynamic(const std::vector<Vertex>& verts, GLuint texture, const Mat4* mvp) {
    if (!modelVao_) {
        glGenVertexArrays(1, &modelVao_);
        glGenBuffers(1, &modelVbo_);
        glBindVertexArray(modelVao_);
        glBindBuffer(GL_ARRAY_BUFFER, modelVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, light));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, block));
    }
    glUseProgram(worldProg_);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp ? mvp->m : lastMvp_.m);
    glUniform1f(uAlpha_, 1.0f);  // alpha-tested: transparent pixels are skipped
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    streamDraw(verts);
}

void Renderer::streamDraw(const std::vector<Vertex>& verts) {
    if (verts.empty()) return;
    if (!modelVao_) {
        glGenVertexArrays(1, &modelVao_);
        glGenBuffers(1, &modelVbo_);
        glBindVertexArray(modelVao_);
        glBindBuffer(GL_ARRAY_BUFFER, modelVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, light));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, block));
    }
    glBindVertexArray(modelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, modelVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

void Renderer::drawWorld(const Player& player, Dimension dim, const RayHit& selection, float breakProgress,
                         float renderDistance, float fovDegrees) {
    // Under water the fog turns blue and much denser; the Nether has thick red haze.
    const float waterFog[3] = {0.1f, 0.2f, 0.55f};
    const float lavaFog[3] = {0.8f, 0.25f, 0.0f};
    const float tarFog[3] = {0.04f, 0.03f, 0.06f};
    const float* netherFog = netherFog_;
    const float* fogColor = sky_;
    float fogStart = renderDistance * 0.6f, fogEnd = renderDistance;
    if (player.headInLava) {
        fogColor = lavaFog; fogStart = 0.0f; fogEnd = 3.0f;
    } else if (player.headInTar) {
        fogColor = tarFog; fogStart = 0.0f; fogEnd = 1.5f;
    } else if (player.headInWater) {
        fogColor = waterFog; fogStart = 2.0f; fogEnd = 16.0f;
    } else if (dim == Dimension::Nether) {
        fogColor = netherFog; fogStart = 8.0f; fogEnd = renderDistance * 0.75f;    } else if (dim == Dimension::Void) {
        fogColor = voidFog_; fogStart = 10.0f; fogEnd = renderDistance * 0.8f;
    } else {
        fogStart *= fogScale_ < 1 ? fogScale_ * 0.5f : 1.0f;
        fogEnd *= fogScale_;
    }
    glClearColor(fogColor[0], fogColor[1], fogColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Vec3 eye = player.eye(), fwd = player.forward();
    Mat4 proj = Mat4::perspective(fovDegrees * kPi / 180.0f, (float)width_ / height_, 0.05f, renderDistance + 32);
    Mat4 mvp = proj * Mat4::lookDir(eye, fwd, {0, 1, 0});
    lastMvp_ = mvp;

    glUseProgram(worldProg_);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
    glUniform3f(uCamPos_, eye.x, eye.y, eye.z);
    glUniform3f(uFogColor_, fogColor[0], fogColor[1], fogColor[2]);
    glUniform2f(uFogRange_, fogStart, fogEnd);
    glUniform1f(uBrightness_, brightness_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);

    const bool openSky = dim == Dimension::Overworld && !player.headInWater && !player.headInLava && !player.headInTar;
    if (openSky && (!skySunMoon_.empty() || !skyStars_.empty())) {
        // Behind everything: no fog, no depth, full brightness.
        glUniform2f(uFogRange_, 1e5f, 2e5f);
        glUniform1f(uBrightness_, 1.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (starAlpha_ > 0.01f) {
            glUniform1f(uAlpha_, starAlpha_ * 0.99f);
            streamDraw(skyStars_);
        }
        glUniform1f(uAlpha_, 0.99f);
        streamDraw(skySunMoon_);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glUniform2f(uFogRange_, fogStart, fogEnd);
        glUniform1f(uBrightness_, brightness_);
        glBindTexture(GL_TEXTURE_2D, atlas_);
    }

    std::vector<const GpuMesh*> visible;
    visible.reserve(meshes_.size());
    for (auto& [key, m] : meshes_) {
        int cx = (int)(key >> 32), cz = (int32_t)(uint32_t)key;
        Vec3 center(cx * kChunkSize + 8.0f, eye.y, cz * kChunkSize + 8.0f);
        Vec3 rel = center - eye;
        float horiz = std::sqrt(rel.x * rel.x + rel.z * rel.z);
        if (horiz > renderDistance + 12) continue;
        // Cheap culling: skip chunks clearly behind the camera.
        if (horiz > 24 && dot(rel, Vec3(fwd.x, 0, fwd.z)) < -12) continue;
        visible.push_back(&m);
    }

    glUniform1f(uAlpha_, 1.0f);
    for (const GpuMesh* m : visible)
        if (m->count[0]) {
            glBindVertexArray(m->vao[0]);
            glDrawArrays(GL_TRIANGLES, 0, m->count[0]);
        }

    if (openSky && !skyClouds_.empty()) {
        // Clouds fade with distance like the terrain, never with the sky behind them.
        glUniform2f(uFogRange_, fogStart * 1.6f, fogEnd * 1.9f);
        glUniform1f(uBrightness_, 1.0f);
        glUniform1f(uAlpha_, 1.0f);
        streamDraw(skyClouds_);
        glUniform2f(uFogRange_, fogStart, fogEnd);
        glUniform1f(uBrightness_, brightness_);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);  // water surface visible from below too
    glUniform1f(uAlpha_, 0.8f);
    for (const GpuMesh* m : visible)
        if (m->count[1]) {
            glBindVertexArray(m->vao[1]);
            glDrawArrays(GL_TRIANGLES, 0, m->count[1]);
        }
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    if (selection.hit) {
        // Black wireframe around the targeted block.
        const float e = 0.002f;
        float x0 = selection.x - e, y0 = selection.y - e, z0 = selection.z - e;
        float x1 = selection.x + 1 + e, y1 = selection.y + 1 + e, z1 = selection.z + 1 + e;
        const float c[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
                               {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}};
        const int edges[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
        UiVertex lines[24];
        for (int i = 0; i < 24; ++i) lines[i] = {c[edges[i]][0], c[edges[i]][1], c[edges[i]][2], 0, 0, 0, 0, 0, 0.6f};
        glUseProgram(uiProg_);
        glUniformMatrix4fv(uUiProj_, 1, GL_FALSE, mvp.m);
        glUniform1i(uUiUseTex_, 0);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof lines, lines, GL_STREAM_DRAW);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 24);

        if (breakProgress > 0) {
            // Mining feedback: the block gets darker as it cracks.
            const int faces[6][4] = {{4, 7, 6, 5}, {0, 1, 2, 3}, {0, 4, 5, 1}, {3, 2, 6, 7}, {1, 5, 6, 2}, {0, 3, 7, 4}};
            UiVertex tris[36];
            int n = 0;
            float a = 0.15f + 0.55f * breakProgress;
            for (auto& f : faces)
                for (int idx : {f[0], f[1], f[2], f[0], f[2], f[3]})
                    tris[n++] = {c[idx][0], c[idx][1], c[idx][2], 0, 0, 0, 0, 0, a};
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1, -1);
            glDisable(GL_CULL_FACE);
            glBufferData(GL_ARRAY_BUFFER, sizeof tris, tris, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glEnable(GL_CULL_FACE);
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Renderer::setFogColors(const float sky[3], const float nether[3]) {
    for (int i = 0; i < 3; ++i) {
        sky_[i] = sky[i];
        netherFog_[i] = nether[i];
    }
}

void Renderer::drawUi(const UiList& list, float guiWidth, float guiHeight) {
    if (list.verts.empty()) return;
    std::vector<UiVertex> verts;
    verts.reserve(list.verts.size());
    for (const UiVert& v : list.verts) verts.push_back({v.x, v.y, 0, v.u, v.v, v.r, v.g, v.b, v.a});

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProg_);
    // GUI pixels with the origin in the top-left corner.
    Mat4 proj = Mat4::ortho(0, guiWidth, guiHeight, 0);
    glUniformMatrix4fv(uUiProj_, 1, GL_FALSE, proj.m);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(UiVertex), verts.data(), GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    int first = 0;
    for (auto [tex, count] : list.runs) {
        glUniform1i(uUiUseTex_, tex != TexNone);
        if (tex != TexNone) glBindTexture(GL_TEXTURE_2D, tex == TexAtlas ? atlas_ : tex == TexSkins ? skinAtlas_ : tex == TexMap ? map_ : gui_);
        glDrawArrays(GL_TRIANGLES, first, count);
        first += count;
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace vc
