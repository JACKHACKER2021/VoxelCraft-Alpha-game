#include "Sky.h"

#include <algorithm>
#include <cmath>

#include "Blocks.h"
#include "Noise.h"
#include "World.h"

namespace vc {

namespace {

constexpr float kTile = kTileUv;

float smoothstep(float a, float b, float v) {
    float t = std::clamp((v - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

// A quad facing `normal` around `center`, textured with a whole atlas tile.
void quad(std::vector<Vertex>& out, const Vec3& center, const Vec3& right, const Vec3& up, int tile, float light) {
    const float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile, u1 = u0 + kTile, v1 = v0 + kTile;
    Vertex a{center.x - right.x - up.x, center.y - right.y - up.y, center.z - right.z - up.z, u0, v1, light};
    Vertex b{center.x + right.x - up.x, center.y + right.y - up.y, center.z + right.z - up.z, u1, v1, light};
    Vertex c{center.x + right.x + up.x, center.y + right.y + up.y, center.z + right.z + up.z, u1, v0, light};
    Vertex d{center.x - right.x + up.x, center.y - right.y + up.y, center.z - right.z + up.z, u0, v0, light};
    out.insert(out.end(), {a, b, c, a, c, d});
}

Vec3 sunDirection(float ticks) {
    // Rises in the east (+x), noon overhead, sets in the west; tilted a little to the south.
    float a = ticks / kDayTicks * 2 * kPi;
    return Vec3(std::cos(a), std::sin(a), 0.18f).normalized();
}

}  // namespace

DayPhase dayPhase(float ticks) {
    float t = std::fmod(std::fmod(ticks, kDayTicks) + kDayTicks, kDayTicks);
    if (t < 3000 || t >= 23000) return DayPhase::Morning;
    if (t < 11000) return DayPhase::Day;
    if (t < 13500) return DayPhase::Evening;
    if (t >= 17000 && t < 19000) return DayPhase::Midnight;
    return DayPhase::Night;
}

const char* dayPhaseName(DayPhase phase) {
    switch (phase) {
        case DayPhase::Morning: return "Ráno";
        case DayPhase::Day: return "Den";
        case DayPhase::Evening: return "Večer";
        case DayPhase::Night: return "Noc";
        default: return "Půlnoc";
    }
}

SkyState computeSky(float ticks, const float daySky[3], float rain) {
    SkyState s;
    const float elev = sunDirection(ticks).y;
    const float day = smoothstep(-0.18f, 0.22f, elev);
    const float night[3] = {0.02f, 0.03f, 0.08f}, glow[3] = {0.98f, 0.52f, 0.28f}, grey[3] = {0.45f, 0.48f, 0.52f};
    // Orange around sunrise and sunset.
    const float dusk = std::exp(-(elev / 0.16f) * (elev / 0.16f)) * 0.55f;
    for (int i = 0; i < 3; ++i) {
        float c = night[i] + (daySky[i] - night[i]) * day;
        c = c + (glow[i] - c) * dusk;
        c = c + (grey[i] * (0.25f + 0.75f * day) - c) * rain * 0.7f;
        s.sky[i] = c;
    }
    s.light = (0.22f + 0.78f * day) * (1 - 0.3f * rain);
    s.stars = (1 - smoothstep(-0.3f, 0.05f, elev)) * (1 - rain);
    s.clouds = (0.25f + 0.75f * day) * (1 - 0.35f * rain);
    return s;
}

void buildSkyObjects(const Vec3& eye, float ticks, std::vector<Vertex>& sunMoon, std::vector<Vertex>& stars) {
    sunMoon.clear();
    stars.clear();
    const Vec3 sun = sunDirection(ticks);
    auto billboard = [&](std::vector<Vertex>& out, const Vec3& dir, float dist, float size, int tile, float light) {
        Vec3 right = cross(dir, Vec3(0, 0, 1)).normalized() * size;
        Vec3 up = cross(right, dir).normalized() * size;
        quad(out, eye + dir * dist, right, up, tile, light);
    };
    billboard(sunMoon, sun, 50, 7, T_SUN, 1.4f);
    billboard(sunMoon, sun * -1, 50, 5, T_MOON, 1.2f);
    // Stars turn with the sky.
    const float a = ticks / kDayTicks * 2 * kPi;
    for (int i = 0; i < 160; ++i) {
        uint32_t h = hash3(i, 7, 11, 0x57A2u);
        float az = (h % 3600) / 3600.0f * 2 * kPi, el = 0.08f + ((h >> 12) % 1000) / 1000.0f * 1.45f;
        Vec3 d(std::cos(az) * std::cos(el), std::sin(el), std::sin(az) * std::cos(el));
        // Rotate around the sun's axis (z) with the time of day.
        Vec3 r(d.x * std::cos(a) - d.y * std::sin(a), d.x * std::sin(a) + d.y * std::cos(a), d.z);
        if (r.y < -0.05f) continue;
        billboard(stars, r, 52, 0.18f + ((h >> 22) % 3) * 0.06f, T_STAR, 1.3f);
    }
}

void buildClouds(const Vec3& eye, float seconds, float brightness, std::vector<Vertex>& out) {
    out.clear();
    const float cell = 12, shift = seconds * 1.2f, y0 = kCloudHeight, y1 = kCloudHeight + 4;
    const int span = 9;
    const int cx0 = (int)std::floor((eye.x + shift) / cell), cz0 = (int)std::floor(eye.z / cell);
    auto cloudy = [](int i, int k) {
        // Lumps: a cell is cloudy where a few hashed neighbours agree.
        int n = 0;
        for (int dx = 0; dx <= 1; ++dx)
            for (int dz = 0; dz <= 1; ++dz) n += hash3((i + dx) >> 1, 0, (k + dz) >> 1, 0xC10Du) % 100 < 45;
        return n >= 2 && hash3(i, 1, k, 0xC10Du) % 100 < 80;
    };
    const float u0 = (T_CLOUD % kAtlasCols) * kTile + 0.002f, v0 = (T_CLOUD / kAtlasCols) * kTile + 0.002f;
    auto face = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, float shade) {
        float l = brightness * shade;
        Vertex va{a.x, a.y, a.z, u0, v0, l}, vb{b.x, b.y, b.z, u0, v0, l}, vc{c.x, c.y, c.z, u0, v0, l}, vd{d.x, d.y, d.z, u0, v0, l};
        out.insert(out.end(), {va, vb, vc, va, vc, vd});
    };
    for (int i = cx0 - span; i <= cx0 + span; ++i)
        for (int k = cz0 - span; k <= cz0 + span; ++k) {
            if (!cloudy(i, k)) continue;
            float x0 = i * cell - shift, z0 = k * cell, x1 = x0 + cell, z1 = z0 + cell;
            face({x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}, 1.0f);   // top
            face({x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, 0.72f);  // bottom
            if (!cloudy(i - 1, k)) face({x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}, 0.85f);
            if (!cloudy(i + 1, k)) face({x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x1, y1, z0}, 0.85f);
            if (!cloudy(i, k - 1)) face({x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, 0.9f);
            if (!cloudy(i, k + 1)) face({x0, y0, z1}, {x0, y1, z1}, {x1, y1, z1}, {x1, y0, z1}, 0.9f);
        }
}

void buildPrecipitation(const World& world, const Vec3& eye, Precipitation kind, float seconds, float intensity,
                        float light, std::vector<Vertex>& out) {
    out.clear();
    if (kind == Precipitation::None || intensity <= 0.01f) return;
    const int radius = kind == Precipitation::Rain ? 7 : 9;
    const int ex = (int)std::floor(eye.x), ez = (int)std::floor(eye.z);
    const int tile = kind == Precipitation::Rain ? T_RAIN : T_SNOWFLAKE;
    for (int dx = -radius; dx <= radius; ++dx)
        for (int dz = -radius; dz <= radius; ++dz) {
            if (dx * dx + dz * dz > radius * radius) continue;
            const int x = ex + dx, z = ez + dz;
            const uint32_t h = hash3(x, 3, z, 0x4A1Du);
            if ((h % 1000) / 1000.0f > intensity * 0.85f) continue;
            const float ground = (float)world.skyHeight(x, z);
            const float top = eye.y + 14, bottom = std::max(ground, eye.y - 10);
            if (bottom >= top) continue;  // under a roof
            const float offset = (h >> 10) % 1000 / 1000.0f;
            if (kind == Precipitation::Rain) {
                // Two crossed planes of streaks per column, falling fast.
                const float fall = std::fmod(seconds * 9 + offset * 4, 4.0f);
                for (float y = std::floor(bottom) - 4 + (4 - fall); y < top; y += 4) {  // moving down
                    float ya = std::max(y, bottom), yb = std::min(y + 4, top);
                    if (yb <= ya) continue;
                    const float u0 = (tile % kAtlasCols) * kTile, v0 = (tile / kAtlasCols) * kTile;
                    float va = v0 + kTile * (1 - (ya - y) / 4), vb = v0 + kTile * (1 - (yb - y) / 4);
                    float cxp = x + 0.5f, czp = z + 0.5f;
                    for (int p = 0; p < 2; ++p) {
                        float ox = p ? 0.5f : 0, oz = p ? 0 : 0.5f;
                        Vertex a{cxp - ox, ya, czp - oz, u0, va, light}, b{cxp + ox, ya, czp + oz, u0 + kTile, va, light};
                        Vertex c{cxp + ox, yb, czp + oz, u0 + kTile, vb, light}, d{cxp - ox, yb, czp - oz, u0, vb, light};
                        out.insert(out.end(), {a, b, c, a, c, d});
                    }
                }
            } else {
                // A few flakes per column drifting down slowly.
                for (int f = 0; f < 3; ++f) {
                    uint32_t q = hash3(x, f, z, 0x5A0Fu);
                    float span = top - bottom;
                    float y = top - std::fmod(seconds * (1.2f + (q % 7) * 0.1f) + (q >> 8) % 1000 / 1000.0f * span, span);
                    float fx = x + 0.2f + (q >> 3) % 60 / 100.0f + std::sin(seconds + f) * 0.15f;
                    float fz = z + 0.2f + (q >> 9) % 60 / 100.0f;
                    Vec3 to = Vec3(fx, y, fz) - eye;
                    Vec3 right = cross(to, Vec3(0, 1, 0)).normalized() * 0.09f;
                    quad(out, Vec3(fx, y, fz), right, Vec3(0, 0.09f, 0), tile, light * 1.2f);
                }
            }
        }
}

}  // namespace vc
