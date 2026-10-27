#include "Audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace vc {

namespace {

constexpr int kRate = 22050;
constexpr float kTau = 6.2831853f;
const char* kVersionTag = "synth-v3";  // 1.6.0: the cave tune

std::vector<uint8_t> toWav(const std::vector<float>& samples) {
    std::vector<uint8_t> out;
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back((uint8_t)(v >> (8 * i))); };
    auto u16 = [&](uint16_t v) { out.push_back((uint8_t)v); out.push_back((uint8_t)(v >> 8)); };
    uint32_t dataBytes = (uint32_t)samples.size() * 2;
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    u32(36 + dataBytes);
    out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    u32(16);
    u16(1);  // PCM
    u16(1);  // mono
    u32(kRate);
    u32(kRate * 2);
    u16(2);
    u16(16);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    u32(dataBytes);
    for (float s : samples) u16((uint16_t)(int16_t)std::lround(std::clamp(s, -1.0f, 1.0f) * 32000));
    return out;
}

struct Noise {
    uint32_t state = 0x1234567;
    float next() {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (state & 0xFFFF) / 32768.0f - 1.0f;
    }
};

// Low-passed noise burst with an exponential decay: digging, steps, crunching.
void noiseBurst(std::vector<float>& out, int start, float seconds, float cutoff, float gain, float decay, uint32_t seed) {
    Noise n{seed};
    float lp = 0, a = std::min(1.0f, cutoff / kRate * kTau);
    int len = (int)(seconds * kRate);
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    for (int i = 0; i < len; ++i) {
        lp += a * (n.next() - lp);
        out[start + i] += lp * gain * std::exp(-decay * i / (float)kRate);
    }
}

void tone(std::vector<float>& out, int start, float seconds, float f0, float f1, float gain, float decay, float attack = 0.003f) {
    int len = (int)(seconds * kRate);
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    float phase = 0;
    for (int i = 0; i < len; ++i) {
        float t = i / (float)kRate;
        float f = f0 + (f1 - f0) * (t / seconds);
        phase += kTau * f / kRate;
        float env = std::min(1.0f, t / attack) * std::exp(-decay * t);
        // A soft "bell": fundamental plus a quiet octave.
        out[start + i] += (std::sin(phase) + 0.25f * std::sin(2 * phase)) * gain * env;
    }
}

}  // namespace

const char* soundName(Sound s) {
    switch (s) {
        case Sound::Click: return "click";
        case Sound::Dig: return "dig";
        case Sound::Place: return "place";
        case Sound::Step: return "step";
        case Sound::Hurt: return "hurt";
        case Sound::Eat: return "eat";
        case Sound::Coin: return "coin";
        case Sound::Explode: return "explode";
        case Sound::Fuse: return "fuse";
        case Sound::Splash: return "splash";
        case Sound::Grunt: return "grunt";
        default: return "";
    }
}

std::vector<uint8_t> synthesizeSound(Sound s) {
    std::vector<float> o;
    switch (s) {
        case Sound::Click: tone(o, 0, 0.05f, 1100, 900, 0.35f, 60); break;
        case Sound::Dig: noiseBurst(o, 0, 0.16f, 1800, 0.9f, 22, 7); break;
        case Sound::Place:
            noiseBurst(o, 0, 0.1f, 900, 0.7f, 35, 11);
            tone(o, 0, 0.1f, 170, 120, 0.4f, 30);
            break;
        case Sound::Step: noiseBurst(o, 0, 0.07f, 700, 0.5f, 45, 23); break;
        case Sound::Hurt: tone(o, 0, 0.25f, 420, 180, 0.5f, 9); break;
        case Sound::Eat:
            for (int k = 0; k < 3; ++k) noiseBurst(o, k * kRate / 8, 0.09f, 2500, 0.6f, 40, 31 + k);
            break;
        case Sound::Coin:
            tone(o, 0, 0.12f, 988, 988, 0.35f, 12);
            tone(o, kRate / 12, 0.35f, 1318, 1318, 0.35f, 8);
            break;
        case Sound::Explode:
            // A deep boom with a long rumble.
            noiseBurst(o, 0, 1.4f, 500, 1.0f, 3.2f, 91);
            tone(o, 0, 0.9f, 90, 35, 0.8f, 4);
            break;
        case Sound::Fuse: noiseBurst(o, 0, 0.8f, 6000, 0.25f, 1.5f, 93); break;
        case Sound::Splash:
            noiseBurst(o, 0, 0.35f, 2500, 0.6f, 9, 95);
            tone(o, 0, 0.2f, 600, 300, 0.2f, 14);
            break;
        case Sound::Grunt: tone(o, 0, 0.3f, 160, 110, 0.5f, 8); break;
        default: break;
    }
    return toWav(o);
}

std::vector<uint8_t> synthesizeMusic() {
    // Slow arpeggios over C - Am - F - G, pentatonic melody, soft pad. 48 s at 80 bpm-ish.
    const float noteLen = 0.75f;
    const int bars = 16, notesPerBar = 4;
    const float chords[4][3] = {{261.6f, 329.6f, 392.0f}, {220.0f, 261.6f, 329.6f}, {174.6f, 220.0f, 261.6f}, {196.0f, 246.9f, 293.7f}};
    const float melody[] = {523.3f, 587.3f, 659.3f, 784.0f, 880.0f};
    std::vector<float> o((size_t)(bars * notesPerBar * noteLen * kRate) + kRate, 0);
    uint32_t r = 0xBEEF;
    for (int bar = 0; bar < bars; ++bar) {
        const float* c = chords[bar % 4];
        int barStart = (int)(bar * notesPerBar * noteLen * kRate);
        // Pad: the whole chord, very quiet and slow.
        for (int k = 0; k < 3; ++k) tone(o, barStart, notesPerBar * noteLen, c[k] / 2, c[k] / 2, 0.05f, 0.4f, 0.8f);
        for (int n = 0; n < notesPerBar; ++n) {
            int start = barStart + (int)(n * noteLen * kRate);
            tone(o, start, noteLen * 2, c[n % 3], c[n % 3], 0.12f, 2.2f, 0.01f);
            r = r * 1103515245u + 12345u;
            if ((r >> 16) % 3 == 0) {
                float m = melody[(r >> 8) % 5];
                tone(o, start, noteLen * 2.5f, m, m, 0.07f, 1.6f, 0.02f);
            }
        }
    }
    // Fade the tail into the start so the loop is seamless.
    size_t loop = (size_t)(bars * notesPerBar * noteLen * kRate);
    for (size_t i = loop; i < o.size(); ++i) o[i - loop] += o[i];
    o.resize(loop);
    // Tiny echo for space.
    for (size_t i = kRate / 3; i < o.size(); ++i) o[i] += o[i - kRate / 3] * 0.25f;
    float peak = 0;
    for (float s : o) peak = std::max(peak, std::fabs(s));
    if (peak > 0) for (float& s : o) s *= 0.8f / peak;
    return toWav(o);
}

namespace {

// Karplus-Strong plucked string: a short noise burst through a delay line with a
// gentle low-pass, which rings like a nylon guitar or a harp.
void pluck(std::vector<float>& out, int start, float freq, float gain, float seconds, float brightness, uint32_t seed) {
    int period = std::max(2, (int)std::lround(kRate / freq));
    std::vector<float> line(period);
    Noise n{seed};
    for (float& v : line) v = n.next();
    int len = (int)(seconds * kRate);
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    float prev = 0;
    for (int i = 0; i < len; ++i) {
        int k = i % period;
        float v = line[k];
        float next = line[(k + 1) % period];
        line[k] = (v * brightness + next * (1 - brightness)) * 0.996f;
        float env = std::min(1.0f, i / 40.0f) * (i > len - kRate / 10 ? (len - i) / (kRate / 10.0f) : 1.0f);
        prev = v;
        out[start + i] += prev * gain * env;
    }
}

// Warm pad: a few detuned sines with harmonics, slow attack and release.
void pad(std::vector<float>& out, int start, float seconds, float freq, float gain) {
    int len = (int)(seconds * kRate);
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    const float attack = std::min(1.2f, seconds / 3), release = std::min(1.5f, seconds / 2);
    float p1 = 0, p2 = 0, p3 = 0;
    for (int i = 0; i < len; ++i) {
        float t = i / (float)kRate;
        float env = std::min(1.0f, t / attack) * std::min(1.0f, (seconds - t) / release);
        p1 += kTau * freq * 0.998f / kRate;
        p2 += kTau * freq * 1.003f / kRate;
        p3 += kTau * freq * 2.001f / kRate;
        out[start + i] += (std::sin(p1) + std::sin(p2) + 0.3f * std::sin(p3) + 0.12f * std::sin(3 * p1)) * gain * env;
    }
}

// Bell / flute-like lead with a little vibrato.
void bell(std::vector<float>& out, int start, float seconds, float freq, float gain, float vibrato) {
    int len = (int)(seconds * kRate);
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    float phase = 0;
    for (int i = 0; i < len; ++i) {
        float t = i / (float)kRate;
        float f = freq * (1 + vibrato * std::sin(kTau * 5.2f * t) * std::min(1.0f, t * 2));
        phase += kTau * f / kRate;
        float env = std::min(1.0f, t / 0.04f) * std::exp(-1.6f * t) * std::min(1.0f, (seconds - t) * 8);
        out[start + i] += (std::sin(phase) + 0.18f * std::sin(2 * phase) + 0.06f * std::sin(3 * phase)) * gain * env;
    }
}

void kick(std::vector<float>& out, int start, float gain) {
    int len = kRate / 4;
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    float phase = 0;
    for (int i = 0; i < len; ++i) {
        float t = i / (float)kRate;
        phase += kTau * (48 + 90 * std::exp(-t * 30)) / kRate;
        out[start + i] += std::sin(phase) * gain * std::exp(-t * 14);
    }
}

void hat(std::vector<float>& out, int start, float gain, uint32_t seed) {
    Noise n{seed};
    int len = kRate / 20;
    if ((int)out.size() < start + len) out.resize(start + len, 0);
    float prev = 0;
    for (int i = 0; i < len; ++i) {
        float v = n.next();
        out[start + i] += (v - prev) * gain * std::exp(-i / (float)kRate * 70);  // high-passed noise tick
        prev = v;
    }
}

float midiFreq(int note) { return 440.0f * std::pow(2.0f, (note - 69) / 12.0f); }

struct Style {
    int root;               // MIDI note of the tonic (bass register)
    int scale[7];           // semitones of the mode
    int chords[4];          // scale degrees (0-based) of the four bars of the progression
    float bpm;
    bool drums;
    float pluckBright;      // 0.5 bright .. 0.9 dark
    float leadChance;       // how busy the melody is
    float echo;             // feedback of the delay
};

Style styleOf(MusicStyle s) {
    // All but the Nether use the notes of C major, like the theme songs, in different modes.
    switch (s) {
        case MusicStyle::Plains:    return {48, {0, 2, 4, 5, 7, 9, 11}, {0, 4, 5, 3}, 66, true, 0.55f, 0.55f, 0.28f};  // C ionian: C G Am F
        case MusicStyle::Forest:    return {45, {0, 2, 3, 5, 7, 8, 10}, {0, 5, 2, 6}, 64, false, 0.65f, 0.45f, 0.35f}; // A aeolian: Am F C G
        case MusicStyle::Desert:    return {40, {0, 1, 3, 5, 7, 8, 10}, {0, 1, 0, 6}, 72, true, 0.5f, 0.6f, 0.3f};    // E phrygian: Em F Em D
        case MusicStyle::Mountains: return {41, {0, 2, 4, 6, 7, 9, 11}, {0, 1, 2, 1}, 60, false, 0.7f, 0.35f, 0.42f}; // F lydian: F G Am G
        case MusicStyle::Ocean:     return {43, {0, 2, 4, 5, 7, 9, 10}, {0, 6, 3, 0}, 58, false, 0.75f, 0.3f, 0.45f}; // G mixolydian: G F C G
        // 1.6.0 Caves and Tunes: D dorian, slow, dark plucks and a long echo, like drops in a cave.
        case MusicStyle::Caves:     return {38, {0, 2, 3, 5, 7, 9, 10}, {0, 3, 5, 4}, 52, false, 0.8f, 0.3f, 0.6f};   // Dm G Bb A
        default:                    return {36, {0, 1, 3, 5, 7, 8, 10}, {0, 1, 0, 6}, 56, false, 0.85f, 0.25f, 0.5f}; // C phrygian, dark
    }
}

}  // namespace

const char* musicStyleName(MusicStyle s) {
    static const char* kNames[] = {"biome_plains", "biome_forest", "biome_desert", "biome_mountains", "biome_ocean", "biome_nether", "biome_caves"};
    return kNames[(int)s];
}

std::vector<float> synthesizeBiomeSamples(MusicStyle ms) {
    const Style st = styleOf(ms);
    const float beat = 60.0f / st.bpm, bar = beat * 4;
    const int bars = 16;  // four times the progression, about a minute
    std::vector<float> o((size_t)(bars * bar * kRate) + kRate * 4, 0);
    auto note = [&](int degree, int octave) {  // scale degree (may exceed 6) to MIDI
        int d = ((degree % 7) + 7) % 7, o8 = (degree - d) / 7;
        return st.root + 12 * (octave + o8) + st.scale[d];
    };
    uint32_t r = 0x51ED + (uint32_t)ms * 7919;
    auto rnd = [&](int n) { r = r * 1103515245u + 12345u; return (int)((r >> 16) % (uint32_t)n); };
    // A four-bar motif (scale steps per eighth note, -1 = rest) that the melody repeats
    // and varies, so the piece has a recognisable tune.
    int motif[32];
    int pos = 4;
    for (int i = 0; i < 32; ++i) {
        if (rnd(100) < (int)(st.leadChance * 100) && i % 2 == 0) {
            pos = std::clamp(pos + (rnd(5) - 2), 0, 9);
            motif[i] = pos;
        } else {
            motif[i] = -1;
        }
    }
    for (int b = 0; b < bars; ++b) {
        const int start = (int)(b * bar * kRate);
        const int chord = st.chords[b % 4];
        const int section = b / 4;  // 0 intro, 1 theme, 2 variation, 3 outro
        // Pad: the triad, two octaves up from the bass.
        for (int k = 0; k < 3; ++k) pad(o, start, bar * 1.02f, midiFreq(note(chord + 2 * k, 1)), 0.035f);
        // Bass: root on beat 1 and 3.
        if (section > 0 || ms == MusicStyle::Nether)
            for (int q : {0, 2}) bell(o, start + (int)(q * beat * kRate), beat * 1.8f, midiFreq(note(chord, 0)), 0.11f, 0);
        // Plucked arpeggio in eighths: up and down the chord.
        static const int kArp[8] = {0, 2, 4, 7, 4, 2, 0, 2};
        for (int e = 0; e < 8; ++e) {
            if (section == 0 && e % 2) continue;  // sparser intro
            int n = note(chord + kArp[e], 2);
            pluck(o, start + (int)(e * beat / 2 * kRate), midiFreq(n), 0.16f, beat * 2.5f, st.pluckBright, r + b * 8 + e);
        }
        // Melody from the motif in the middle sections, one octave higher in the variation.
        if (section == 1 || section == 2)
            for (int e = 0; e < 8; ++e) {
                int m = motif[(b % 4) * 8 + e];
                if (m < 0) continue;
                if (section == 2 && rnd(4) == 0) m += rnd(3) - 1;  // small variations
                bell(o, start + (int)(e * beat / 2 * kRate), beat * 1.6f, midiFreq(note(m, section == 2 ? 4 : 3)), 0.07f, 0.004f);
            }
        // Soft drums for the brighter biomes.
        if (st.drums && section > 0 && section < 3)
            for (int q = 0; q < 4; ++q) {
                if (q % 2 == 0) kick(o, start + (int)(q * beat * kRate), 0.22f);
                hat(o, start + (int)((q + 0.5f) * beat * kRate), 0.05f, r + q);
            }
    }
    // Echo (dotted eighth) with a darker repeat, then a gentle fade-out at the end.
    const int delay = (int)(beat * 0.75f * kRate);
    float lp = 0;
    for (size_t i = delay; i < o.size(); ++i) {
        lp += 0.35f * (o[i - delay] - lp);
        o[i] += lp * st.echo;
    }
    const size_t fade = (size_t)(kRate * 4);
    for (size_t i = 0; i < fade && i < o.size(); ++i) o[o.size() - 1 - i] *= i / (float)fade;
    float peak = 0;
    for (float v : o) peak = std::max(peak, std::fabs(v));
    if (peak > 0)
        for (float& v : o) v *= 0.85f / peak;
    return o;
}

std::vector<uint8_t> synthesizeBiomeMusic(MusicStyle s) { return toWav(synthesizeBiomeSamples(s)); }

bool writeSoundFiles(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path tag = fs::path(dir) / "version.txt";
    {
        std::ifstream in(tag);
        std::string v;
        if (in >> v && v == kVersionTag) return true;
    }
    auto write = [&](const std::string& name, const std::vector<uint8_t>& data) {
        std::ofstream out(fs::path(dir) / (name + ".wav"), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        return (bool)out;
    };
    bool ok = true;
    for (int i = 0; i < (int)Sound::Count; ++i) ok = write(soundName((Sound)i), synthesizeSound((Sound)i)) && ok;
    ok = write("music", synthesizeMusic()) && ok;
    for (int i = 0; i < (int)MusicStyle::Count; ++i)
        ok = write(musicStyleName((MusicStyle)i), synthesizeBiomeMusic((MusicStyle)i)) && ok;
    if (ok) std::ofstream(tag) << kVersionTag;
    return ok;
}

}  // namespace vc
