#pragma once
// 2.2.0: the voice of the village radio (Rozhlas): a two-note chime like a village loudspeaker, then the
// text read by eSpeak NG built into the game (no speech service of the phone), through a small
// loudspeaker with a little crackle.
#include <cstdint>
#include <string>
#include <vector>

namespace vc {
// dataDir holds espeak-ng-data (eSpeak NG reads the text; without it, VC_ESPEAK unset, a rough formant
// voice of our own); lang "cs" picks the Czech voice, anything else English.
std::vector<float> synthesizeVoiceSamples(const std::string& utf8, const std::string& dataDir, const std::string& lang);  // mono, 22050 Hz
std::vector<uint8_t> synthesizeVoice(const std::string& utf8, const std::string& dataDir, const std::string& lang);       // WAV
void formantSpeech(const std::string& utf8, std::vector<float>& out);
void loudspeaker(std::vector<float>& out);
}  // namespace vc
