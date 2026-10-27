#include "Renderer.h"

#include <cstring>

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Blocks.h"
#include "Shaders.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VoxelCraft", __VA_ARGS__)

namespace vc {

namespace {

const char* kWorldVs = R"(#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_light;
layout(location = 3) in float a_block;
uniform mat4 u_mvp;
uniform mat4 u_lightMvp;
uniform vec3 u_cam;
uniform float u_wave;
uniform float u_waveTime;
out vec2 v_uv;
out float v_light;
out float v_dist;
out float v_block;
out vec4 v_shadow;
out float v_kind;
out vec3 v_world;
void main() {
    // 1.5.5: the mesher marks swaying vertices (leaves, plant tops) and water surfaces in a_block.
    float kind = floor(a_block * 0.5);
    vec3 p = a_pos;
    if (u_wave > 0.0 && kind > 0.5) {
        float ph = u_waveTime * 1.7 + p.x * 0.55 + p.z * 0.45 + p.y * 0.3;
        if (kind < 1.5) {
            p.x += sin(ph) * 0.045 * u_wave;
            p.z += cos(ph * 0.8 + 1.3) * 0.04 * u_wave;
        } else {
            p.y += (sin(ph * 0.9) * 0.5 - 0.5) * 0.06 * u_wave;
        }
    }
    gl_Position = u_mvp * vec4(p, 1.0);
    v_uv = a_uv;
    v_light = a_light;
    v_block = a_block - 2.0 * kind;
    v_dist = length(p.xz - u_cam.xz);
    v_shadow = u_lightMvp * vec4(p, 1.0);
    v_kind = kind;
    v_world = p;
})";

// The shadow pass: depth from the sun, leaves and glass cut out like in the world shader.
const char* kShadowVs = R"(#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 3) in float a_block;
uniform mat4 u_lightMvp;
uniform float u_wave;
uniform float u_waveTime;
out vec2 v_uv;
void main() {
    float kind = floor(a_block * 0.5);
    vec3 p = a_pos;
    if (u_wave > 0.0 && kind > 0.5 && kind < 1.5) {
        float ph = u_waveTime * 1.7 + p.x * 0.55 + p.z * 0.45 + p.y * 0.3;
        p.x += sin(ph) * 0.045 * u_wave;
        p.z += cos(ph * 0.8 + 1.3) * 0.04 * u_wave;
    }
    gl_Position = u_lightMvp * vec4(p, 1.0);
    v_uv = a_uv;
})";

const char* kShadowFs = R"(#version 300 es
precision mediump float;
uniform sampler2D u_atlas;
uniform float u_alphaTest;
in vec2 v_uv;
out vec4 o_color;
void main() {
    if (u_alphaTest > 0.5 && texture(u_atlas, v_uv).a < 0.5) discard;
    o_color = vec4(1.0);
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

// A box of the world seen from the sun: x/y across, depth along the sunlight.
Mat4 sunMatrix(const Vec3& centre, const Vec3& sunDir, float radius, float depth) {
    Mat4 o = Mat4::identity();
    o.m[0] = 1 / radius;
    o.m[5] = 1 / radius;
    o.m[10] = -2 / (2 * depth);
    o.m[14] = -1;  // near 0, far 2 * depth
    return o * Mat4::lookDir(centre + sunDir * depth, sunDir * -1.0f, Vec3(0, 0, 1));
}

}  // namespace

bool Renderer::setShader(GraphicsMode mode, const ShaderProfile& profile) {
    GLuint prog = 0;
    bool custom = false;
    if (mode == GraphicsMode::Enhanced && !profile.fragment.empty()) custom = (prog = link(kWorldVs, profile.fragment.c_str())) != 0;
    if (!prog) prog = link(kWorldVs, mode == GraphicsMode::Enhanced ? kVibrantWorldFs : kClassicWorldFs);
    if (!prog) prog = link(kWorldVs, kClassicWorldFs);  // never without a world shader
    if (!prog) return false;
    if (worldProg_) glDeleteProgram(worldProg_);
    worldProg_ = prog;
    uMvp_ = glGetUniformLocation(worldProg_, "u_mvp");
    uCamPos_ = glGetUniformLocation(worldProg_, "u_cam");
    uFogColor_ = glGetUniformLocation(worldProg_, "u_fogColor");
    uFogRange_ = glGetUniformLocation(worldProg_, "u_fogRange");
    uAlpha_ = glGetUniformLocation(worldProg_, "u_alpha");
    uBrightness_ = glGetUniformLocation(worldProg_, "u_brightness");
    uTime_ = glGetUniformLocation(worldProg_, "u_time");
    uScreen_ = glGetUniformLocation(worldProg_, "u_screen");
    glUseProgram(worldProg_);
    glUniform1f(glGetUniformLocation(worldProg_, "u_saturation"), profile.saturation);
    glUniform1f(glGetUniformLocation(worldProg_, "u_contrast"), profile.contrast);
    glUniform1f(glGetUniformLocation(worldProg_, "u_warmth"), profile.warmth);
    glUniform1f(glGetUniformLocation(worldProg_, "u_vignette"), profile.vignette);
    glUniform1f(glGetUniformLocation(worldProg_, "u_glow"), profile.glow);
    glUniform3f(glGetUniformLocation(worldProg_, "u_shade"), profile.shade[0], profile.shade[1], profile.shade[2]);
    glUniform1f(glGetUniformLocation(worldProg_, "u_fogStrength"), profile.fogStrength);
    glUniform1i(glGetUniformLocation(worldProg_, "u_atlas"), 0);
    glUniform1i(glGetUniformLocation(worldProg_, "u_shadowMap"), 1);
    uLightMvp_ = glGetUniformLocation(worldProg_, "u_lightMvp");
    uWave_ = glGetUniformLocation(worldProg_, "u_wave");
    uWaveTime_ = glGetUniformLocation(worldProg_, "u_waveTime");
    uShadowStrength_ = glGetUniformLocation(worldProg_, "u_shadowStrength");
    uHandLight_ = glGetUniformLocation(worldProg_, "u_handLight");
    glUniform4f(uHandLight_, 0, 0, 0, 0);
    glUniform1f(uWave_, 0);
    glUniform1f(uShadowStrength_, 0);
    glUniformMatrix4fv(uLightMvp_, 1, GL_FALSE, Mat4::identity().m);
    enhanced_ = mode == GraphicsMode::Enhanced;
    std::copy(profile.fogTint, profile.fogTint + 3, fogTint_);
    return custom || profile.fragment.empty() || mode == GraphicsMode::Classic;
}

bool Renderer::init() {
    uiProg_ = link(kUiVs, kUiFs);
    shadowProg_ = link(kShadowVs, kShadowFs);  // 0 on a phone without it: no shadows, nothing else changes
    if (shadowProg_) {
        uShadowLightMvp_ = glGetUniformLocation(shadowProg_, "u_lightMvp");
        uShadowWave_ = glGetUniformLocation(shadowProg_, "u_wave");
        uShadowWaveTime_ = glGetUniformLocation(shadowProg_, "u_waveTime");
        uShadowAlphaTest_ = glGetUniformLocation(shadowProg_, "u_alphaTest");
        glUseProgram(shadowProg_);
        glUniform1i(glGetUniformLocation(shadowProg_, "u_atlas"), 0);
    }
    if (!uiProg_ || !setShader(GraphicsMode::Enhanced, ShaderProfile{})) return false;
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
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    shadowTex_ = shadowFbo_ = 0;
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
    glUniform1f(uShadowStrength_, 0);  // the hand lives in its own space: no sun shadows, no swaying
    glUniform4f(uHandLight_, 0, 0, 0, 0);
    glUniform1f(uWave_, 0);
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

void Renderer::drawShadowMap(const Mat4& lightMvp, const Vec3& centre, float wave) {
    if (!shadowFbo_) {
        glGenTextures(1, &shadowTex_);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowSize, kShadowSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenFramebuffers(1, &shadowFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("shadow framebuffer incomplete: shadows off");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteProgram(shadowProg_);
            shadowProg_ = 0;
            return;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, kShadowSize, kShadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(shadowProg_);
    glUniformMatrix4fv(uShadowLightMvp_, 1, GL_FALSE, lightMvp.m);
    glUniform1f(uShadowWave_, wave);
    glUniform1f(uShadowWaveTime_, shaderTime_);
    glUniform1f(uShadowAlphaTest_, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glDisable(GL_CULL_FACE);  // thin leaves and plants cast shadows from both sides
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    const float reach = kShadowRadius * 1.5f + 16;
    for (auto& [key, m] : meshes_) {
        const int cx = (int)(key >> 32), cz = (int32_t)(uint32_t)key;
        const float dx = cx * kChunkSize + 8.0f - centre.x, dz = cz * kChunkSize + 8.0f - centre.z;
        if (m.count[0] && dx * dx + dz * dz < reach * reach) {
            glBindVertexArray(m.vao[0]);
            glDrawArrays(GL_TRIANGLES, 0, m.count[0]);
        }
    }
    if (!casters_.empty()) {
        // Players and mobs: their skins have see-through parts, so no alpha test.
        glUniform1f(uShadowAlphaTest_, 0);
        glUniform1f(uShadowWave_, 0);
        streamDraw(casters_);
        glDisable(GL_CULL_FACE);  // streamDraw turns it back on
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
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
    if (enhanced_) glClearColor(fogColor[0] * fogTint_[0], fogColor[1] * fogTint_[1], fogColor[2] * fogTint_[2], 1);
    else glClearColor(fogColor[0], fogColor[1], fogColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Vec3 eye = player.eye(), fwd = player.forward();
    Mat4 proj = Mat4::perspective(fovDegrees * kPi / 180.0f, (float)width_ / height_, 0.05f, renderDistance + 32);
    Mat4 mvp = proj * Mat4::lookDir(eye, fwd, {0, 1, 0});
    lastMvp_ = mvp;

    // 1.5.5 Enhanced: swaying leaves and rippling water, and the sun's shadows.
    shaderTime_ += 1.0f / 60;
    const float wave = enhanced_ && waves_ ? 1.0f : 0.0f;
    const bool shadows = enhanced_ && shadows_ && shadowProg_ && dim == Dimension::Overworld && sunStrength_ > 0.02f &&
                         !player.headInWater && !player.headInLava && !player.headInTar;
    Mat4 lightMvp = Mat4::identity();
    if (shadows) {
        const Vec3 centre(std::floor(eye.x), std::floor(eye.y), std::floor(eye.z));
        lightMvp = sunMatrix(centre, sunDir_, kShadowRadius, 96);
        drawShadowMap(lightMvp, centre, wave);
    }

    glUseProgram(worldProg_);
    glUniform1f(uWave_, wave);
    glUniform1f(uWaveTime_, shaderTime_);
    glUniform1f(uShadowStrength_, shadows ? sunStrength_ : 0.0f);
    glUniform4f(uHandLight_, handLight_.x, handLight_.y, handLight_.z, handLightStrength_);
    glUniformMatrix4fv(uLightMvp_, 1, GL_FALSE, lightMvp.m);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadows ? shadowTex_ : 0);
    glActiveTexture(GL_TEXTURE0);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
    glUniform3f(uCamPos_, eye.x, eye.y, eye.z);
    if (enhanced_) glUniform3f(uFogColor_, fogColor[0] * fogTint_[0], fogColor[1] * fogTint_[1], fogColor[2] * fogTint_[2]);
    else glUniform3f(uFogColor_, fogColor[0], fogColor[1], fogColor[2]);
    glUniform2f(uFogRange_, fogStart, fogEnd);
    glUniform1f(uTime_, shaderTime_);
    glUniform2f(uScreen_, (float)width_, (float)height_);
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
