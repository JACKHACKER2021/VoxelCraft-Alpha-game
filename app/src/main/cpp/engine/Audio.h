#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

// Sound effects and background music synthesised in code (our own, no
// samples from anywhere), written as WAV files the Android side can play.
enum class Sound {
    Click, Dig, Place, Step, Hurt, Eat, Coin, Explode, Fuse, Splash, Grunt,
    // 1.9.5: kinds with their own recordings (the Old World plays the nearest synthesized one).
    StepWood, StepWet, DigWood, Glass, Door, Chest, Thunder, Page, Confirm,
    // 1.9.5 (Minetest Game sounds): every surface and material its own.
    StepSand, StepGravel, StepStone, StepSnow, StepIce, DigDirt, DigGravel, DigLeaves, DigIce,
    PlaceStone, ToolBreak, FlintSteel, Extinguish, DoorIron, Fire, LavaCool,
    TrainHorn, CrossingChime,  // 2.1.0 preview: synthesized (our own), for the horn and chime makers
    Count
};
// The synthesized sound a kind falls back to (the Old World keeps the sounds of 0.1-0.3).
Sound synthesizedKind(Sound s);

const char* soundName(Sound s);
std::vector<uint8_t> synthesizeSound(Sound s);
std::vector<uint8_t> synthesizeMusic();  // a calm loop of about 48 seconds

// Biome music in the VoxelCraft style (C major family like the theme songs):
// plucked strings, soft pads, bass, a bell melody and echo, about a minute each.
enum class MusicStyle { Plains, Forest, Desert, Mountains, Ocean, Nether, Caves, Count };  // 1.6.0: Caves
const char* musicStyleName(MusicStyle s);  // file name: "biome_plains", ...
std::vector<float> synthesizeBiomeSamples(MusicStyle s);  // mono, kSampleRate
std::vector<uint8_t> synthesizeBiomeMusic(MusicStyle s);  // WAV
constexpr int kSampleRate = 22050;

// Writes all sounds, music.wav and the biome music into `dir` unless they already exist for
// this version of the synthesiser.
bool writeSoundFiles(const std::string& dir);

}  // namespace vc
