#include "Noise.h"
#include <cmath>

namespace vc {

namespace {
float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
float lerp(float a, float b, float t) { return a + t * (b - a); }

float grad2(int h, float x, float y) {
    switch (h & 7) {
        case 0: return x + y;  case 1: return -x + y;
        case 2: return x - y;  case 3: return -x - y;
        case 4: return x;      case 5: return -x;
        case 6: return y;      default: return -y;
    }
}

float grad3(int h, float x, float y, float z) {
    int b = h & 15;
    float u = b < 8 ? x : y;
    float v = b < 4 ? y : (b == 12 || b == 14) ? x : z;
    return ((b & 1) ? -u : u) + ((b & 2) ? -v : v);
}
}  // namespace

Noise::Noise(uint32_t seed) {
    uint8_t p[256];
    for (int i = 0; i < 256; ++i) p[i] = (uint8_t)i;
    uint32_t s = seed ? seed : 1;
    for (int i = 255; i > 0; --i) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;  // xorshift32
        int j = (int)(s % (uint32_t)(i + 1));
        uint8_t t = p[i]; p[i] = p[j]; p[j] = t;
    }
    for (int i = 0; i < 512; ++i) perm_[i] = p[i & 255];
}

float Noise::perlin2(float x, float y) const {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - xi, yf = y - yi;
    xi &= 255; yi &= 255;
    float u = fade(xf), v = fade(yf);
    int aa = perm_[perm_[xi] + yi], ab = perm_[perm_[xi] + yi + 1];
    int ba = perm_[perm_[xi + 1] + yi], bb = perm_[perm_[xi + 1] + yi + 1];
    return lerp(lerp(grad2(aa, xf, yf), grad2(ba, xf - 1, yf), u),
                lerp(grad2(ab, xf, yf - 1), grad2(bb, xf - 1, yf - 1), u), v) * 0.7071f;
}

float Noise::perlin3(float x, float y, float z) const {
    int xi = (int)std::floor(x), yi = (int)std::floor(y), zi = (int)std::floor(z);
    float xf = x - xi, yf = y - yi, zf = z - zi;
    xi &= 255; yi &= 255; zi &= 255;
    float u = fade(xf), v = fade(yf), w = fade(zf);
    int a = perm_[xi] + yi, aa = perm_[a] + zi, ab = perm_[a + 1] + zi;
    int b = perm_[xi + 1] + yi, ba = perm_[b] + zi, bb = perm_[b + 1] + zi;
    return lerp(
        lerp(lerp(grad3(perm_[aa], xf, yf, zf), grad3(perm_[ba], xf - 1, yf, zf), u),
             lerp(grad3(perm_[ab], xf, yf - 1, zf), grad3(perm_[bb], xf - 1, yf - 1, zf), u), v),
        lerp(lerp(grad3(perm_[aa + 1], xf, yf, zf - 1), grad3(perm_[ba + 1], xf - 1, yf, zf - 1), u),
             lerp(grad3(perm_[ab + 1], xf, yf - 1, zf - 1), grad3(perm_[bb + 1], xf - 1, yf - 1, zf - 1), u), v),
        w);
}

float Noise::fbm2(float x, float y, int octaves) const {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += perlin2(x, y) * amp;
        norm += amp;
        amp *= 0.5f; x *= 2; y *= 2;
    }
    return sum / norm;
}

}  // namespace vc
