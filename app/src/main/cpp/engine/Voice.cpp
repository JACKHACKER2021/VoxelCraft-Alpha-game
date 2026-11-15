#include "Voice.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#ifdef VC_ESPEAK
#include <espeak-ng/speak_lib.h>
#endif

namespace vc {
namespace {
constexpr int kRate = 22050;
constexpr float kPi = 3.14159265f;

// A two-pole resonator: one formant of the voice.
struct Resonator {
    float a1 = 0, a2 = 0, g = 0, y1 = 0, y2 = 0;
    void set(float freq, float bw) {
        const float r = std::exp(-kPi * bw / kRate);
        a1 = 2 * r * std::cos(2 * kPi * freq / kRate), a2 = -r * r, g = 1 - r;
    }
    float run(float x) {
        const float y = g * x + a1 * y1 + a2 * y2;
        y2 = y1, y1 = y;
        return y;
    }
};

enum class Kind { Vowel, Voiced, Nasal, Fricative, Stop, Pause };
struct Phone {
    Kind kind;
    float f1, f2, f3, len;  // formants (Hz) and length (s); fricatives: f1 = noise centre
    bool voiced = true;
    bool trill = false;
};

// Czech letters (after lower-casing) to sounds. Long vowels (á, é, í, ó, ú, ů, ý) last longer.
bool phoneOf(uint32_t c, Phone& p) {
    auto V = [&](float f1, float f2, float f3, float len) { p = {Kind::Vowel, f1, f2, f3, len}; return true; };
    auto F = [&](float centre, bool voiced, float len) { p = {Kind::Fricative, centre, 0, 0, len, voiced}; return true; };
    auto S = [&](float burst, bool voiced) { p = {Kind::Stop, burst, 0, 0, 0.075f, voiced}; return true; };
    switch (c) {
        case 'a': return V(760, 1300, 2500, 0.085f);
        case 0xE1: return V(760, 1300, 2500, 0.16f);
        case 'e': return V(520, 1850, 2550, 0.08f);
        case 0xE9: case 0x11B: return V(520, 1850, 2550, 0.15f);
        case 'i': case 'y': return V(320, 2200, 2900, 0.075f);
        case 0xED: case 0xFD: return V(320, 2200, 2900, 0.14f);
        case 'o': return V(520, 950, 2450, 0.085f);
        case 0xF3: return V(520, 950, 2450, 0.15f);
        case 'u': return V(340, 820, 2350, 0.08f);
        case 0xFA: case 0x16F: return V(340, 820, 2350, 0.15f);
        case 'm': p = {Kind::Nasal, 260, 1100, 2300, 0.07f}; return true;
        case 'n': case 0x148: p = {Kind::Nasal, 260, 1500, 2500, 0.065f}; return true;
        case 'l': p = {Kind::Voiced, 380, 1100, 2500, 0.06f}; return true;
        case 'r': p = {Kind::Voiced, 450, 1300, 1700, 0.07f}; p.trill = true; return true;
        case 0x159: p = {Kind::Voiced, 420, 1500, 2600, 0.08f}; p.trill = true; return true;  // ř
        case 'j': p = {Kind::Voiced, 300, 2100, 2900, 0.05f}; return true;
        case 'v': case 'w': p = {Kind::Voiced, 320, 1100, 2300, 0.055f}; return true;
        case 's': return F(5200, false, 0.09f);
        case 'z': return F(5000, true, 0.08f);
        case 0x161: return F(2700, false, 0.09f);  // š
        case 0x17E: return F(2600, true, 0.08f);   // ž
        case 'f': return F(6500, false, 0.08f);
        case 'h': return F(1300, true, 0.07f);
        case 'x': return F(4800, false, 0.11f);
        case 'c': return F(4600, false, 0.08f);
        case 0x10D: return F(3000, false, 0.08f);  // č
        case 'p': return S(900, false);
        case 'b': return S(700, true);
        case 't': case 0x165: return S(3800, false);
        case 'd': case 0x10F: return S(3000, true);
        case 'k': case 'q': return S(1800, false);
        case 'g': return S(1500, true);
        case ' ': case '-': p = {Kind::Pause, 0, 0, 0, 0.06f}; return true;
        case ',': case ';': case ':': p = {Kind::Pause, 0, 0, 0, 0.18f}; return true;
        case '.': case '!': case '?': p = {Kind::Pause, 0, 0, 0, 0.32f}; return true;
        default:
            if (c >= '0' && c <= '9') return V(600, 1500, 2500, 0.07f);
            return false;
    }
}

std::vector<uint32_t> codepoints(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        const uint8_t b = (uint8_t)s[i];
        uint32_t c = b;
        int n = 1;
        if (b >= 0xF0) c = b & 7, n = 4;
        else if (b >= 0xE0) c = b & 15, n = 3;
        else if (b >= 0xC0) c = b & 31, n = 2;
        for (int k = 1; k < n && i + k < s.size(); ++k) c = c << 6 | ((uint8_t)s[i + k] & 63);
        i += n;
        // Lower case: ASCII, Latin-1 and the Czech letters of Latin Extended-A (upper case is even).
        if (c >= 'A' && c <= 'Z') c += 32;
        else if (c >= 0xC0 && c <= 0xDE && c != 0xD7) c += 32;
        else if (c >= 0x100 && c <= 0x17F && (c & 1) == 0 && c != 0x138) c += 1;
        out.push_back(c);
    }
    return out;
}

// The chime of a village loudspeaker: two soft bell notes going up.
void chime(std::vector<float>& out) {
    const float notes[2] = {659.3f, 880.0f};
    for (int n = 0; n < 2; ++n) {
        const int start = (int)(n * 0.42f * kRate);
        const int len = (int)(1.1f * kRate);
        if ((int)out.size() < start + len) out.resize(start + len, 0.0f);
        for (int i = 0; i < len; ++i) {
            const float t = (float)i / kRate;
            const float env = std::min(1.0f, t / 0.01f) * std::exp(-t * 3.2f);
            out[start + i] += 0.32f * env * (std::sin(2 * kPi * notes[n] * t) + 0.35f * std::sin(2 * kPi * notes[n] * 2.01f * t));
        }
    }
}

#ifdef VC_ESPEAK
std::vector<float>* gSpeech = nullptr;
int speechChunk(short* wav, int n, espeak_EVENT*) {
    if (wav && gSpeech)
        for (int i = 0; i < n; ++i) gSpeech->push_back(wav[i] / 32768.0f);
    return 0;
}
#endif
}  // namespace

std::vector<float> synthesizeVoiceSamples(const std::string& utf8, const std::string& dataDir, const std::string& lang) {
    std::vector<float> out;
    chime(out);
    out.resize(out.size() + kRate / 5, 0.0f);
#ifdef VC_ESPEAK
    // eSpeak NG reads the text: the Czech voice (or English for the other languages), a little slow.
    static std::mutex lock;
    static bool ready = false;
    std::lock_guard<std::mutex> guard(lock);
    if (!ready) ready = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, dataDir.c_str(), 0) > 0;
    if (ready) {
        espeak_SetSynthCallback(speechChunk);
        espeak_SetVoiceByName(lang == "cs" ? "cs" : "en");
        espeak_SetParameter(espeakRATE, 145, 0);
        espeak_SetParameter(espeakPITCH, 45, 0);
        std::vector<float> speech;
        gSpeech = &speech;
        const std::string text = utf8.substr(0, 400);
        espeak_Synth(text.c_str(), text.size() + 1, 0, POS_CHARACTER, 0, espeakCHARS_UTF8, nullptr, nullptr);
        gSpeech = nullptr;
        out.insert(out.end(), speech.begin(), speech.end());
    }
    (void)lang;
#else
    (void)dataDir, (void)lang;
    formantSpeech(utf8, out);
#endif
    loudspeaker(out);
    return out;
}

void formantSpeech(const std::string& utf8, std::vector<float>& out) {
    const std::vector<uint32_t> text = codepoints(utf8.substr(0, 400));
    std::vector<Phone> phones;
    for (size_t i = 0; i < text.size(); ++i) {
        Phone p;
        if (text[i] == 'c' && i + 1 < text.size() && text[i + 1] == 'h') {  // "ch"
            p = {Kind::Fricative, 1600, 0, 0, 0.09f, false};
            ++i;
        } else if (!phoneOf(text[i], p)) {
            continue;
        }
        phones.push_back(p);
    }
    const bool question = !text.empty() && text.back() == '?';
    Resonator r1, r2, r3, nf;
    float phase = 0, prevAmp = 0;
    uint32_t seed = 0x9E3779B9u;
    auto noise = [&] {
        seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
        return (seed & 0xFFFF) / 32768.0f - 1.0f;
    };
    // Formants glide from one sound to the next instead of jumping.
    float c1 = 500, c2 = 1500, c3 = 2500;
    const size_t total = phones.size();
    for (size_t k = 0; k < total; ++k) {
        const Phone& p = phones[k];
        const int n = (int)(p.len * kRate);
        const float progress = total > 1 ? (float)k / (total - 1) : 0;
        // Intonation: a slow fall through the sentence, a rise at the end of a question.
        float f0 = 128 - 22 * progress + (question && progress > 0.8f ? (progress - 0.8f) * 260 : 0);
        for (int i = 0; i < n; ++i) {
            const float t = (float)i / std::max(1, n - 1);
            float sample = 0, amp = 0;
            if (p.kind == Kind::Vowel || p.kind == Kind::Voiced || p.kind == Kind::Nasal) {
                c1 += (p.f1 - c1) * 0.004f, c2 += (p.f2 - c2) * 0.004f, c3 += (p.f3 - c3) * 0.004f;
                r1.set(c1, 90), r2.set(c2, 120), r3.set(c3, 170);
                phase += (f0 + 3 * std::sin(out.size() * 0.0009f)) / kRate;
                if (phase >= 1) phase -= 1;
                const float pulse = (phase < 0.4f ? std::sin(phase / 0.4f * kPi) : 0.0f) + 0.02f * noise();  // glottal pulse
                sample = r1.run(pulse) * 1.0f + r2.run(pulse) * 0.6f + r3.run(pulse) * 0.25f;
                amp = p.kind == Kind::Vowel ? 1.0f : p.kind == Kind::Nasal ? 0.45f : 0.6f;
                if (p.trill) amp *= 0.6f + 0.4f * std::sin(t * p.len * 2 * kPi * 28);
            } else if (p.kind == Kind::Fricative) {
                nf.set(p.f1, p.f1 * 0.35f);
                sample = nf.run(noise()) * 1.6f;
                amp = 0.35f;
                if (p.voiced) {
                    phase += f0 / kRate;
                    if (phase >= 1) phase -= 1;
                    sample += 0.25f * std::sin(phase * 2 * kPi);
                }
            } else if (p.kind == Kind::Stop) {
                // Closure, then a short burst.
                if (t > 0.6f) {
                    nf.set(p.f1, p.f1 * 0.6f);
                    sample = nf.run(noise()) * 2.2f * std::exp(-(t - 0.6f) * 30);
                    amp = 0.5f;
                } else if (p.voiced) {
                    phase += f0 / kRate;
                    if (phase >= 1) phase -= 1;
                    sample = 0.08f * std::sin(phase * 2 * kPi);
                    amp = 1;
                }
            }
            prevAmp += (amp - prevAmp) * 0.01f;  // smooth joins: no clicks
            out.push_back(sample * prevAmp);
        }
    }
}

void loudspeaker(std::vector<float>& out) {
    uint32_t seed = 0x2545F491u;
    auto noise = [&] {
        seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
        return (seed & 0xFFFF) / 32768.0f - 1.0f;
    };
    out.resize(out.size() + kRate / 4, 0.0f);
    // The loudspeaker: telephone band (about 300-3400 Hz), a little overdrive, a soft crackle and hum.
    float hp = 0, lpA = 0, lpB = 0, last = 0, peak = 1e-4f;
    for (float& s : out) peak = std::max(peak, std::fabs(s));
    for (size_t i = 0; i < out.size(); ++i) {
        float s = out[i] / peak;
        hp = 0.92f * (hp + s - last);
        last = s;
        lpA += (hp - lpA) * 0.55f;
        lpB += (lpA - lpB) * 0.55f;
        float v = std::tanh(lpB * 2.2f) * 0.75f;
        v += 0.012f * noise() + 0.008f * std::sin(i * 2 * kPi * 50.0f / kRate);
        if ((seed & 0x7FF) == 3) v += 0.15f * noise();  // a crackle now and then
        out[i] = v;
    }
}

std::vector<uint8_t> synthesizeVoice(const std::string& utf8, const std::string& dataDir, const std::string& lang) {
    const std::vector<float> s = synthesizeVoiceSamples(utf8, dataDir, lang);
    std::vector<uint8_t> out;
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back((uint8_t)(v >> (8 * i))); };
    auto u16 = [&](uint16_t v) { out.push_back((uint8_t)v), out.push_back((uint8_t)(v >> 8)); };
    const uint32_t bytes = (uint32_t)s.size() * 2;
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    u32(36 + bytes);
    out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    u32(16), u16(1), u16(1), u32(kRate), u32(kRate * 2), u16(2), u16(16);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    u32(bytes);
    for (float v : s) u16((uint16_t)(int16_t)std::lround(std::clamp(v, -1.0f, 1.0f) * 30000));
    return out;
}
}  // namespace vc
