#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "Crafting.h"
#include "GenConfig.h"
#include "Items.h"
#include "Shaders.h"
#include "Math.h"

namespace vc {

enum class GameMode { Creative = 0, Survival = 1 };

struct WorldInfo {
    std::string folder;  // absolute path of the world directory
    std::string name;
    uint32_t seed = 0;
    GameMode mode = GameMode::Creative;
    int64_t lastPlayed = 0;  // unix seconds
    bool showCoordinates = false;
    bool keepInventory = false;
    bool allowCommands = false;  // cheat commands (/gamemode, /tp, /give...) in Survival
    // 1.5.1: Experimental Features chosen when the world was created (off by default).
    bool expOreRedesign = false;  // 1.6.0 Preview: the redesigned ore textures
};

// Everything about the player that survives quitting the game.
struct PlayerState {
    bool valid = false;  // false for a world that was never played
    Vec3 pos, spawn;
    float yaw = 0, pitch = 0;
    int dimension = 0;  // 0 overworld, 1 nether, 2 crimson void
    float health = 20, food = 20, saturation = 5, air = 15;
    bool flying = false;
    int selectedSlot = 0;
    Inventory inventory;
    std::array<ItemStack, 4> armor{};  // helmet, chestplate, leggings, boots
    // Places on the minimap
    bool hasDeath = false;
    int deathX = 0, deathZ = 0;
    std::vector<std::pair<int, int>> structures;  // x, z of placed structures (newest last, at most 16)
    // 1.1.0: the sky and the weather of the world, beaten bosses.
    double dayTime = 1000;      // ticks since the world began (24000 a day, see Sky.h)
    bool raining = false;
    float weatherTimer = 600;   // seconds until the weather changes
    int bossesDefeated = 0;
    std::vector<std::pair<int, int>> bossArenas;  // centres of arenas whose boss is beaten
    // 1.4.0: where the Anniversary showcase was built (once per world).
    bool hasShowcase = false;
    int showcaseX = 0, showcaseY = 0, showcaseZ = 0;
    // 1.5.0: which pages of the old documents the player has read (bit per page).
    int documentsRead = 0;
};

struct Settings {
    // Touch
    float sensitivity = 1.0f;
    bool invertY = false;
    bool autoJump = true;
    bool vibrate = true;      // short buzz when a block breaks
    bool lefty = false;       // joystick on the right, buttons on the left
    float buttonScale = 1.0f;
    // Video
    int guiScale = 0;         // 0 = automatic
    float brightness = 1.0f;
    float fov = 70.0f;
    int renderDistance = 6;   // chunks
    bool hideGui = false;
    bool clouds = true;
    int cameraMode = 0;       // 0 first person, 1 from behind, 2 from the front
    bool viewBobbing = true;
    bool showMap = true;      // minimap in the top right corner
    GraphicsMode graphics = GraphicsMode::Enhanced;  // 1.5.5: Classic or Enhanced (Vibrant shaders)
    bool shadows = true;      // 1.5.5 Enhanced: shadows of the sun and moon
    bool waves = true;        // 1.5.5 Enhanced: leaves and plants sway, water ripples
    // Audio
    float musicVolume = 0.6f;
    float soundVolume = 0.8f;
    bool uiSounds = true;
    // Language code: en (main), cs, es, ru, de
    std::string language = "en";
    // Developer tools (only applied while the developer account is signed in)
    bool devGodMode = false;      // no damage
    bool devFly = false;          // flying in survival (double tap jump)
    bool devInstantMine = false;  // blocks break at once in survival
    bool devDebug = false;        // FPS, position, chunks and more in the corner
    bool devNoclip = false;       // fly through blocks
    float devSpeed = 1.0f;        // walking and flying speed
    // Experiments
    bool expDayNight = false;     // day and night cycle (10 minutes)
    bool expFastFurnace = false;  // furnaces work four times faster
    bool expOreMap = false;       // the minimap shows ores below the player
    bool expFastGrowth = false;   // saplings grow in 5 seconds
    bool expLowGravity = false;   // jumps like on the moon
    bool expBigBoom = false;      // TNT blasts twice as big
    bool calmMode = false;        // no monsters, softer sounds, no red flash (accessibility)
    int tutorialStep = 0;         // 1.4.0 first steps; kTutorialSteps when done or skipped
    // 1.4.0 multiplayer rules of a hosted world
    bool mpPvp = false;
    bool mpProtectSpawn = true;
    // Store addons in effect (filled in from the profile every frame, never saved).
    bool addonWinter = false, addonHalloween = false, addonAnniversary = false;
};

// Seeds typed by the player: numbers are used as they are, any other text
// is hashed like Java's String.hashCode, empty means random.
uint32_t seedFromText(const std::string& text);

std::vector<WorldInfo> listWorlds(const std::string& root);  // newest first
std::string createWorldFolder(const std::string& root);
bool saveLevel(const WorldInfo& info, const PlayerState& player);
bool loadLevel(const std::string& folder, WorldInfo& info, PlayerState& player);
void deleteWorld(const std::string& folder);

// Block entities: furnaces keep their contents and burn while you are away.
struct FurnaceKey {
    int dim = 0, x = 0, y = 0, z = 0;
    bool operator<(const FurnaceKey& o) const { return std::tie(dim, x, y, z) < std::tie(o.dim, o.x, o.y, o.z); }
    bool operator==(const FurnaceKey& o) const { return std::tie(dim, x, y, z) == std::tie(o.dim, o.x, o.y, o.z); }
};
// The generator settings a world was created with (generator.json).
bool saveGenerator(const std::string& folder, const GenConfig& config);
GenConfig loadGenerator(const std::string& folder);  // defaults for worlds made before 0.5

bool saveFurnaces(const std::string& folder, const std::map<FurnaceKey, Furnace>& furnaces);
// Chests use the same key (dimension and position) and 27 slots each.
bool saveChests(const std::string& folder, const std::map<FurnaceKey, std::array<ItemStack, 27>>& chests);
std::map<FurnaceKey, std::array<ItemStack, 27>> loadChests(const std::string& folder);
std::map<FurnaceKey, Furnace> loadFurnaces(const std::string& folder);

Settings loadSettings(const std::string& path);
void saveSettings(const std::string& path, const Settings& s);

int64_t unixNow();

}  // namespace vc
