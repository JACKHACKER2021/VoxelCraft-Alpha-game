#include "Game.h"

#include <algorithm>
#include <sstream>
#include <cmath>
#include <cctype>
#include <ctime>
#include <random>

#include "Blocks.h"
#include "Crafting.h"
#include "Items.h"
#include "WorldFiles.h"
#include "JavaImport.h"
#include "Mods.h"
#include "Achievements.h"
#include "Commands.h"
#include "Developer.h"
#include "ItemModel.h"
#include "Lang.h"
#include "Rewards.h"
#include "Trading.h"
#include "TexturePack.h"

#include <filesystem>

namespace vc {

namespace {
const char* kVersion = "VoxelCraft 1.6.0 Caves and Tunes";

// Splash texts under the logo; a different one every time the title screen opens.
const char* const kSplashes[] = {
    "Teď se skiny!", "Servery se blíží!", "Bronz, ocel a rubín!", "Teď s brněním!", "Mluví pěti jazyky!",
    "Postav si strukturu!", "Nástroje ve 3D!", "Mapa v rohu!", "Hudba z VoxelCraftu!", "Upravuj biomy v JSON!",
    "Navštiv dimenzi úhelného kamene!", "Vlastní balíčky textur!", "Hledej rubíny hluboko!", "Ručně vyrobené pixely!",
    "Od VoxelSoftworks!",
};
constexpr int kSplashCount = sizeof(kSplashes) / sizeof(kSplashes[0]);
constexpr uint32_t kMenuSeed = 20261023;

std::string formatDate(int64_t t) {
    std::time_t tt = (std::time_t)t;
    std::tm tm{};
    localtime_r(&tt, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%d.%m.%Y %H:%M", &tm);
    return buf;
}

std::string modeName(GameMode m) { return m == GameMode::Survival ? "Přežití" : "Kreativní"; }
}  // namespace

Game::Game(const std::string& dataDir)
    : dataDir_(dataDir),
      worldsDir_(dataDir + "/worlds"),
      settingsPath_(dataDir + "/options.txt"),
      texturePackDir_(dataDir + "/texturepack"),
      ui_(font_),
      packs_(dataDir),
      profilePath_(dataDir + "/profile.json"),
      skinsDir_(dataDir + "/skins"),
      soundsDir_(dataDir + "/sounds"),
      structuresDir_(dataDir + "/structures") {
    settings_ = loadSettings(settingsPath_);
    splash_ = (int)(std::time(nullptr) % kSplashCount);  // the first splash differs between launches too
    setLanguage(languageFromCode(settings_.language));
    packs_.ensureDefaultPack();
    rebuildFromPacks();
    profile_ = Profile::load(profilePath_);
    if (nextRewardDay(profile_, todayYmd())) {
        // The daily reward waits in the store (days 1 to 7).
        toast_ = tr("Denní odměna čeká v obchodě!");
        toastUntil_ = 1e9;  // shown until the first frames set a real deadline
    }
    writeSoundFiles(soundsDir_);
    loadSkins();
    ui_.onClick = [this] {
        if (settings_.uiSounds) playSound(Sound::Click);
    };
    menuWorld_ = std::make_unique<World>(kMenuSeed, Dimension::Overworld);
    menuCamera_.pos = Vec3(8.5f, (float)menuWorld_->surfaceHeight(8, 8) + 12, 8.5f);
    menuCamera_.pitch = -0.3f;
}

Game::~Game() { endSession(); }

// ---------------------------------------------------------------- platform

bool Game::onSurfaceCreated() {
    if (!renderer_.init()) return false;
    shadersDirty_ = true;  // a new GL context: build the chosen shader again
    hasSurface_ = true;
    loadTextures();
    renderer_.setGuiTexture(font_.rgba(), Font::kTexW, Font::kTexH);
    loadSkins();  // uploads the skin atlas and the worn skin
    meshedWorld_ = nullptr;  // GPU meshes were lost together with the old context
    for (Multiplayer::Player& p : lan_.players()) p.skinDirty = true;
    hasSurface_ = true;
    return true;
}

void Game::onSurfaceDestroyed() {
    if (hasSurface_) renderer_.shutdown();
    hasSurface_ = false;
    meshedWorld_ = nullptr;
}

void Game::onResize(int width, int height) {
    width_ = width;
    height_ = height;
    applyGuiScale();
    renderer_.resize(width, height);
}

void Game::applyGuiScale() {
    // Integer GUI scale keeps the pixel font crisp; automatic aims for ~250 GUI pixels of height.
    float automatic = std::max(1.0f, std::round(height_ / 250.0f));
    float limit = std::max(1.0f, std::floor(width_ / 400.0f));
    guiScale_ = std::min(settings_.guiScale > 0 ? (float)settings_.guiScale : automatic, limit);
}

void Game::onTouch(int id, TouchAction action, float x, float y, double time) {
    float gx = x / guiScale_, gy = y / guiScale_;
    if (screen_ == Screen::Playing && session_) {
        if (action == TouchAction::Down) session_->touchDown(id, gx, gy, time);
        else if (action == TouchAction::Move) session_->touchMove(id, gx, gy);
        else session_->touchUp(id, gx, gy, time);
        return;
    }
    if (action == TouchAction::Down) ui_.touchDown(id, gx, gy);
    else if (action == TouchAction::Move) ui_.touchMove(id, gx, gy);
    else ui_.touchUp(id, gx, gy);
}

void Game::onAppPause() {
    saveSettings(settingsPath_, settings_);
    profile_.save(profilePath_);
    if (session_) {
        session_->save();
        if (screen_ != Screen::Pause && screen_ != Screen::Death && screen_ != Screen::Settings) setScreen(Screen::Pause);
    }
}

bool Game::onBack() {
    switch (screen_) {
        case Screen::Title: return false;
        case Screen::WorldList: setScreen(Screen::Title); break;
        case Screen::CreateWorld: setScreen(Screen::WorldList); break;
        case Screen::ConfirmDelete: setScreen(Screen::WorldList); break;
        case Screen::Settings:
            saveSettings(settingsPath_, settings_);
            setScreen(settingsReturn_);
            break;
        case Screen::Store:
            if (pendingBuy_ >= 0) pendingBuy_ = -1;  // back closes the purchase dialog first
            else setScreen(Screen::Title);
            break;
        case Screen::Packs:
        case Screen::Skins: setScreen(Screen::Title); break;
        case Screen::EditWorld: setScreen(Screen::WorldList); break;
        case Screen::PackFiles: setScreen(Screen::Packs); break;
        case Screen::JsonEditor: setScreen(Screen::PackFiles); break;
        case Screen::Playing: setScreen(Screen::Pause); break;
        case Screen::Invite: setScreen(Screen::Pause); break;
        case Screen::OfflineGuide: setScreen(Screen::WorldList); break;
        case Screen::Structures: setScreen(Screen::Pause); break;
        case Screen::Developer: setScreen(devReturn_); break;
        case Screen::Achievements: setScreen(achievementsReturn_); break;
        case Screen::Joining:
            lan_.stop();
            setScreen(Screen::WorldList);
            break;
        case Screen::Loading: break;  // wait for the terrain
        case Screen::Pause:
        case Screen::Inventory:
        case Screen::Crafting:
        case Screen::Furnace:
        case Screen::Chest:
        case Screen::Trade: setScreen(Screen::Playing); break;
        case Screen::Document: setScreen(Screen::Playing); break;
        case Screen::Credits:
        case Screen::Trailers: setScreen(Screen::Title); break;
        case Screen::WorldMap:
            session_->refreshMapTexture();
            setScreen(Screen::Playing);
            break;
        case Screen::Chat:
        case Screen::Book: setScreen(Screen::Playing); break;
        case Screen::Death: break;
    }
    return true;
}

void Game::onTextResult(int requestId, const std::string& text) {
    std::lock_guard<std::mutex> lock(textMutex_);
    textResults_.emplace_back(requestId, text);
}

void Game::loadTextures() {
    // Layers: our placeholders, then the picked texture pack, then textures from enabled packs.
    int size = 0;
    auto rgba = buildFallbackAtlas(size);
    Atlas atlas = loadTexturePack(texturePackDir_, rgba, size);
    for (const std::string& dir : packs_.textureDirs()) atlas = loadTexturePack(dir, atlas.rgba, atlas.size);
    for (const auto& [tile, path] : ModRegistry::get().textures()) blitTextureFile(atlas, tile, path);  // mods
    texturesWinter_ = playSettings().addonWinter;
    if (texturesWinter_) {
        // The Winter addon: snow on the grass and frost on the leaves.
        const int t = atlas.size / kAtlasCols;
        auto frost = [&](int tile, int rows, float amount) {
            for (int py = 0; py < rows * t / 16; ++py)
                for (int px = 0; px < t; ++px) {
                    uint8_t* c = &atlas.rgba[((size_t)((tile / kAtlasCols) * t + py) * atlas.size + (tile % kAtlasCols) * t + px) * 4];
                    float a = ((px * 7 + py * 13) % 5 == 0) ? amount * 0.8f : amount;
                    for (int k = 0; k < 3; ++k) c[k] = (uint8_t)(c[k] + (245 - c[k]) * a);
                }
        };
        frost(T_GRASS_TOP, 16, 0.85f);
        frost(T_GRASS_SIDE, 4, 0.9f);
        frost(T_LEAVES, 16, 0.45f);
        frost(T_SPRUCE_LEAVES, 16, 0.45f);
    }
    renderer_.setAtlas(atlas.rgba, atlas.size);
    setItemModelAtlas(atlas.rgba, atlas.size);  // 3D items follow the texture pack
    Minimap::setPalette(atlas.rgba, atlas.size);  // and so do the map colours
    const int t = atlas.size / kAtlasCols;
    oreBase_.assign(kOreRedesignCount * 16 * 16 * 4, 0);
    for (int i = 0; i < kOreRedesignCount; ++i) {
        const int tile = kOreRedesignTiles[i];
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const size_t sx = (tile % kAtlasCols) * t + (x * t + t / 2) / 16, sy = (tile / kAtlasCols) * t + (y * t + t / 2) / 16;
                std::copy_n(&atlas.rgba[(sy * atlas.size + sx) * 4], 4, &oreBase_[((size_t)i * 256 + y * 16 + x) * 4]);
            }
    }
}

void Game::applyShaders() {
    shadersDirty_ = false;
    appliedGraphics_ = playSettings().graphics;
    const std::string dir = packs_.shaderPackDir();
    shaderProfile_ = dir.empty() ? ShaderProfile{} : loadShaderPack(dir, &packWarnings_);
    if (!renderer_.setShader(appliedGraphics_, shaderProfile_))
        packWarnings_.push_back(shaderProfile_.name + ": world.fsh did not compile, the Vibrant shader is used");
}

void Game::applyOreRedesign(bool on) {
    if (oreBase_.empty()) return;
    for (int i = 0; i < kOreRedesignCount; ++i) {
        uint8_t px[16 * 16 * 4];
        if (on) oreRedesignTile(i, px);
        else std::copy_n(&oreBase_[(size_t)i * 1024], 1024, px);
        renderer_.updateAtlasTile(kOreRedesignTiles[i], px);
    }
}

void Game::rebuildFromPacks() {
    packList_ = packs_.list();
    packWarnings_.clear();
    // Mods first: recipes name their blocks and items.
    std::vector<std::string> modDirs;
    for (const PackInfo& p : packList_)
        if (p.enabled) modDirs.push_back(p.dir);
    ModRegistry::get().load(modDirs, dataDir_ + "/mod_ids.txt", &packWarnings_);
    setRecipes(packs_.buildRecipes(&packWarnings_));
    packs_.buildConfig(&packWarnings_);  // only to report problems; worlds snapshot it when created
    packSelected_ = std::clamp(packSelected_, 0, std::max(0, (int)packList_.size() - 1));
    if (hasSurface_) loadTextures();
    shadersDirty_ = true;
}

// ---------------------------------------------------------------- flow

std::string Game::findMusic(const std::string& name) const {
    // The player's own songs: <data>/music/<name>.mp3|.ogg|.wav, then music/ of enabled packs.
    std::vector<std::string> dirs = {dataDir_ + "/music"};
    for (const PackInfo& p : packs_.list())
        if (p.enabled) dirs.push_back(packs_.packDir(p.id) + "/music");
    for (const std::string& dir : dirs)
        for (const char* ext : {".mp3", ".ogg", ".wav", ".m4a"}) {
            std::error_code ec;
            std::string path = dir + "/" + name + ext;
            if (std::filesystem::is_regular_file(path, ec)) return path;
        }
    return "";
}

void Game::updateMusic(float dt) {
    if (!onPlayMusic) return;
    // The context: menus, the overworld or the Nether.
    std::string context = "menu";
    if (session_ && screen_ != Screen::Title) {
        Dimension d = session_->currentWorld().dimension();
        context = d == Dimension::Nether ? "nether" : d == Dimension::Void ? "void" : "overworld";
    }
    auto play = [&](const std::string& source, bool loop) {
        musicSource_ = source;
        musicEnded_ = false;
        onPlayMusic(source, loop);
    };
    if (context != musicContext_) {
        musicContext_ = context;
        musicBiomeNext_ = false;
        if (context == "menu") {
            // The VoxelCraft theme (VoxelSoftworks), looping.
            std::string own = findMusic("menu");
            play(own.empty() ? "asset:music/menu_theme.mp3" : own, true);
        } else {
            musicGap_ = 2;  // a short breath before the first song in the world
            play("", false);
        }
        return;
    }
    if (context == "menu") return;
    if (!musicSource_.empty() && !musicEnded_) return;  // still playing
    if (!musicSource_.empty()) {
        // A song finished: silence for a while, like the classic game.
        musicSource_.clear();
        musicGap_ = 20 + (float)(std::fmod(now_ * 7.3, 25.0));
    }
    if ((musicGap_ -= dt) > 0) return;
    if (context == "void") {
        // 1.6.0: the Crimson Void has its own song (the VoxelCraft theme remix), unless the
        // player adds a "void" track.
        std::string own = findMusic("void");
        play(own.empty() ? "asset:music/void_theme.mp3" : own, false);
        return;
    }
    if (context == "nether") {
        // 1.6.0: the Cornerstone Dimension's own song, unless the player adds a "nether" track.
        std::string own = findMusic(context);
        play(own.empty() ? "asset:music/cornerstone_theme.mp3" : own, false);
        return;
    }
    // 1.6.0 Caves and Tunes: deep under the ground the cave tune plays.
    {
        const Vec3 p = session_->player().pos;
        const int px = (int)std::floor(p.x), pz = (int)std::floor(p.z);
        const World& w = session_->currentWorld();
        int roof = 0;
        for (int y = (int)p.y + 2; y < kChunkHeight && roof < 8; ++y) roof += isOpaque(w.getBlock(px, y, pz));
        if (roof >= 8 && p.y < w.config().overworld.seaLevel - 6) {
            std::string own = findMusic("caves");
            play(own.empty() ? soundsDir_ + "/biome_caves.wav" : own, false);
            return;
        }
    }
    // Overworld: the Overworld song and the music of the biome take turns.
    if (!musicBiomeNext_) {
        std::string own = findMusic("overworld");
        play(own.empty() ? "asset:music/overworld.mp3" : own, false);
    } else {
        Vec3 p = session_->player().pos;
        const BiomeDef* b = session_->currentWorld().biomeAt((int)std::floor(p.x), (int)std::floor(p.z));
        std::string id = b ? b->id : "plains";
        std::string own = findMusic(id);
        static const char* kStyles[] = {"plains", "forest", "desert", "mountains", "ocean"};
        std::string style = "plains";
        for (const char* st : kStyles)
            if (id == st) style = st;
        play(own.empty() ? soundsDir_ + "/biome_" + style + ".wav" : own, false);
    }
    musicBiomeNext_ = !musicBiomeNext_;
}

bool Game::saverActive() const {
    const int level = batteryLevel_;
    return settings_.batterySaver == 2 || (settings_.batterySaver == 1 && level >= 0 && level <= 35 && !charging_);
}

Settings Game::playSettings() const {
    Settings s = settings_;
    if (saverActive()) {
        // Battery saver: a shorter view, no clouds and the plain shader (no shadow pass).
        s.renderDistance = std::min(s.renderDistance, 4);
        s.clouds = false;
        s.shadows = s.waves = false;
        s.graphics = GraphicsMode::Classic;
    }
    if (!profile_.isDeveloper()) {
        // Developer modes and experiments exist only for the developer account.
        s.devGodMode = s.devFly = s.devInstantMine = s.devDebug = s.devNoclip = false;
        s.devSpeed = 1;
        s.expDayNight = s.expFastFurnace = s.expOreMap = s.expFastGrowth = s.expLowGravity = s.expBigBoom = false;
    }
    std::set<std::string> addons = activeAddons(profile_, nowSeconds());
    s.addonWinter = addons.count("winter") > 0;
    s.addonHalloween = addons.count("halloween") > 0;
    s.addonAnniversary = addons.count("anniversary") > 0;
    return s;
}

void Game::setScreen(Screen s) {
    ui_.cancelTouches();
    if (session_) session_->cancelTouches();
    // Items left in a crafting grid go back to the inventory when it closes.
    if ((screen_ == Screen::Inventory || screen_ == Screen::Crafting) && s != screen_) returnCraftingGrid();
    if (s == Screen::WorldList) refreshWorldList();
    if (s == Screen::WorldList) discovery_.start();
    else discovery_.stop();
    if (s == Screen::Title && screen_ != Screen::Title) {
        // A new splash on every visit, never the same twice in a row.
        splashSeed_ = splashSeed_ * 1664525u + 1013904223u + (uint32_t)(now_ * 1000);
        splash_ = (splash_ + 1 + (int)((splashSeed_ >> 8) % (kSplashCount - 1))) % kSplashCount;
    }
    if (s == Screen::Packs) packList_ = packs_.list();
    if (s == Screen::Skins) {
        skinSelected_ = std::max(0, skinIndex(profile_.skin));
        skinBack_ = false;
    }
    if (s == Screen::Inventory) gridSize_ = 2;
    if (s == Screen::Structures && screen_ != Screen::Structures) refreshStructures();
    if (s == Screen::Crafting) gridSize_ = 3;
    if (s != screen_) listScroll_ = 0;
    held_ = {};
    screen_ = s;
}

void Game::refreshWorldList() {
    worlds_ = listWorlds(worldsDir_);
    worldScroll_ = 0;
}

void Game::startSession(const WorldInfo& info, const PlayerState& player) {
    endSession();
    session_ = std::make_unique<Session>(info, player);
    chat_.clear();  // a new world starts with an empty chat
    session_->onVibrate = [this](int ms) {
        if (onVibrate) onVibrate(ms);
    };
    session_->onSound = [this](Sound s) { playSound(s); };
    session_->onBlockMined = [this] {
        ++tutorialMined_;
        if (profile_.countBlock()) playSound(Sound::Coin);
        ++profile_.blocksMined;
        unlockAchievement("first_block");
        if (profile_.blocksMined >= 100) unlockAchievement("miner_100");
        if (profile_.blocksMined >= 1000) unlockAchievement("miner_1000");
    };
    session_->onAchievement = [this](const std::string& id) { unlockAchievement(id); };
    session_->onMessage = [this](const std::string& text) { addChat(text); };
    session_->save();  // writes level.dat right away so the world shows up in the list
    // Items from the daily rewards, and a present every day while the Winter addon is on.
    for (auto [id, n] : profile_.pendingItems)
        if (id > 0 && id < kItemIdLimit && n > 0) session_->giveItem({(ItemId)id, (uint8_t)std::min(n, 64), 0});
    profile_.pendingItems.clear();
    if (playSettings().addonWinter && profile_.lastGiftDay != todayYmd()) {
        profile_.lastGiftDay = todayYmd();
        session_->giveItem({GIFT_BOX, 1, 0});
        addChat(tr("Veselé Vánoce!"));
    }
    profile_.save(profilePath_);
    startLoading("Načítání světa", false);
}

void Game::endSession() {
    if (!session_) return;
    lan_.stop();
    returnCraftingGrid();
    session_->save();
    backupWorld(session_->info().folder, dataDir_ + "/backups", 3);  // a copy of the last three plays
    session_.reset();
    profile_.save(profilePath_);
    meshedWorld_ = nullptr;
}

void Game::drawWorldOnce(World& world) {
    // The renderer keeps meshes for one world at a time.
    if (meshedWorld_ != &world) {
        renderer_.clearMeshes();
        for (auto& [k, c] : world.chunks()) c->dirty = true;
        meshedWorld_ = &world;
    }
}

void Game::updateMenuBackground(float dt) {
    World& w = *menuWorld_;
    drawWorldOnce(w);
    w.ensureChunksAround(0, 0, 5, 2);
    menuCamera_.yaw += dt * 0.06f;
    renderer_.dropUnloadedMeshes(w);
    renderer_.updateMeshes(w, 3);
    renderer_.setFogColors(w.config().overworld.sky, w.config().nether.fog);
    renderer_.drawWorld(menuCamera_, Dimension::Overworld, RayHit{}, 0, 5 * kChunkSize, 70);
}

void Game::frame(double time) {
    if (!hasSurface_) return;
    float dt = lastTime_ > 0 ? (float)std::min(time - lastTime_, 0.1) : 0.016f;
    lastTime_ = now_ = time;
    {
        const Settings eff = playSettings();
        if (shadersDirty_ || eff.graphics != appliedGraphics_) applyShaders();
        renderer_.setEffects(eff.shadows, eff.waves);
        const bool saver = saverActive();
        if (saver && !saverWasOn_ && settings_.batterySaver == 1) {
            toast_ = trf("Úsporný režim: baterie {0} %", {std::to_string((int)batteryLevel_)});
            toastUntil_ = time + 5;
        }
        saverWasOn_ = saver;
    }
    if (textureReload_.exchange(false) || texturesWinter_ != playSettings().addonWinter) loadTextures();
    if (packsReload_.exchange(false)) rebuildFromPacks();
    if (!languageSent_ && onLanguage) {
        onLanguage(languageCode(language()));
        languageSent_ = true;
    }
    updateMusic(dt);
    // 1.5.1: the eyes of the Eye Log and the Void Brick Eye move about eight times a second.
    if (std::floor(time * 8) != std::floor((time - dt) * 8)) {
        uint8_t eye[16 * 16 * 4];
        drawEyeTile(eye, 0, (float)time);
        renderer_.updateAtlasTile(T_EYE_LOG, eye);
        drawEyeTile(eye, 1, (float)time);
        renderer_.updateAtlasTile(T_VOID_BRICK_EYE, eye);
    }
    if (!musicStarted_ && onMusicVolume) {
        onMusicVolume(settings_.musicVolume);
        musicStarted_ = true;
    }
    if (toastUntil_ > 1e8) toastUntil_ = time + 5;
    {
        std::vector<std::pair<int, std::string>> results;
        {
            std::lock_guard<std::mutex> lock(textMutex_);
            results.swap(textResults_);
        }
        for (auto& [id, text] : results) {
            if (id == PromptWorldName) newWorldName_ = text.substr(0, 40);
            else if (id == PromptSeed) newWorldSeed_ = text.substr(0, 40);
            else if (id == PromptJson) editorApply(text, true);
            else if (id == PromptPackImported) {
                std::string packId = text;
                if (packId.rfind("java:", 0) == 0) {
                    // A Java Edition mod or datapack: convert what can be converted, report the rest.
                    packId = packId.substr(5);
                    const std::string dir = packs_.packDir(packId);
                    JavaImportReport rep = convertJavaMod(dir + "/java", dir);
                    toast_ = rep.ok() ? trf("Import Java: {0} bloků, {1} předmětů, {2} receptů",
                                            {std::to_string(rep.blocks), std::to_string(rep.items), std::to_string(rep.recipes)})
                                      : tr("Z tohoto souboru nešlo nic převést");
                    toastUntil_ = time + 8;
                }
                if (packId.rfind("shaders:", 0) == 0) {
                    // 1.5.5: a shader pack; old ones of other games keep their files for reference.
                    packId = packId.substr(8);
                    const bool foreign = convertImportedShaderPack(packs_.packDir(packId), packId);
                    toast_ = foreign ? tr("Shader pack z jiné hry: soubory schovány v shaders/original, vzhled nastavíš v shader.json")
                                     : tr("Shader pack nainstalován");
                    toastUntil_ = time + 8;
                    if (settings_.graphics != GraphicsMode::Enhanced) settings_.graphics = GraphicsMode::Enhanced;
                    shadersDirty_ = true;
                }
                packs_.setEnabled(packId, true);  // an imported pack is switched on right away
                rebuildFromPacks();
                for (size_t i = 0; i < packList_.size(); ++i)
                    if (packList_[i].id == packId) packSelected_ = (int)i;
            } else if (id == PromptPlayerName && !text.empty()) {
                profile_.name = text.substr(0, 24);
                profile_.save(profilePath_);
            } else if (id == PromptEditWorldName && !text.empty()) {
                editInfo_.name = text.substr(0, 40);
            } else if (id == PromptSkinImported) {
                // Java copied the picked PNG into skins/<text>; check it really is a skin.
                Skin test;
                std::string err;
                std::string path = skinsDir_ + "/" + text;
                if (Skin::loadPng(path, test, err)) {
                    loadSkins();
                    profile_.skin = "custom:" + text;
                    profile_.save(profilePath_);
                    applySkin();
                    skinSelected_ = skinIndex(profile_.skin);
                    toast_ = "Skin nahrán";
                } else {
                    std::error_code ec;
                    std::filesystem::remove(path, ec);
                    toast_ = trf("Nejde použít: {0}", {tr(err)});
                }
                toastUntil_ = time + 5;
            } else if (id == PromptDevName && !text.empty()) {
                devLoginName_ = text.substr(0, 40);
                if (onPasswordPrompt) onPasswordPrompt(PromptDevPassword, tr("Heslo vývojáře"));
            } else if (id == PromptDevPassword) {
                if (time < devLockedUntil_) {
                    toast_ = tr("Příliš mnoho pokusů, zkus to za minutu");
                } else if (checkDeveloperLogin(devLoginName_, text)) {
                    profile_.name = devLoginName_;
                    profile_.developer = true;
                    profile_.skin = "developer";  // the developer's own skin
                    applySkin();
                    profile_.save(profilePath_);
                    devFailures_ = 0;
                    toast_ = tr("Vývojářský přístup odemčen");
                } else {
                    toast_ = tr("Špatné jméno nebo heslo");
                    if (++devFailures_ >= 5) {
                        devLockedUntil_ = time + 60;
                        devFailures_ = 0;
                    }
                }
                toastUntil_ = time + 5;
            } else if (id == PromptJoinAddress && !text.empty()) {
                // A room code ("VC-7K3PA-QX2MD"), "address" or "address:port".
                std::string address;
                int port = 0;
                if (net::parseJoinAddress(text, address, port)) joinLan(address, port, address);
                else {
                    toast_ = tr("Neplatný kód nebo adresa");
                    toastUntil_ = time + 5;
                }
            } else if (id == PromptWorldImported && !text.empty()) {
                std::string folder, err;
                if (importWorld(text, worldsDir_, folder, err)) {
                    worlds_ = listWorlds(worldsDir_);
                    toast_ = tr("Svět byl naimportován");
                } else {
                    toast_ = tr(err);
                }
                toastUntil_ = time + 5;
            } else if (id == PromptPublicIp && !text.empty()) {
                std::string address;
                int port = 0;
                internetCode_ = net::parseJoinAddress(text, address, port) ? net::roomCode(address, lan_.port()) : "";
            } else if (id == PromptWaypoint && session_ && !text.empty()) {
                session_->waypoints().push_back({session_->dimIndex(), (int)std::floor(mapCx_), (int)std::floor(mapCz_), text.substr(0, 32)});
            } else if (id == PromptBookPage && session_ && bookId_ > 0 && !session_->bookSigned(bookId_)) {
                auto& pages = session_->book(bookId_).pages;
                if (bookPage_ >= 0 && bookPage_ < (int)pages.size()) pages[bookPage_] = text.substr(0, 512);
            } else if (id == PromptBookTitle && session_ && bookId_ > 0 && !text.empty()) {
                session_->signBook(bookId_, text, profile_.name);
            } else if (id == PromptChat) {
                submitChat(text);
            } else if (id == PromptCode) {
                std::string message;
                if (redeemCode(profile_, text, message)) {
                    profile_.save(profilePath_);
                    playSound(Sound::Coin);
                }
                toast_ = message;
                toastUntil_ = now_ + 5;
            } else if (id == PromptMusicEnded) {
                if (text == musicSource_) musicEnded_ = true;
            } else if (id == PromptStructureImported) {
                // Java copied the picked file into structures/<text>; keep it only if it loads.
                Structure test;
                ImportReport report;
                std::string err, path = structuresDir_ + "/" + text;
                if (loadStructureFile(path, test, report, err)) {
                    refreshStructures();
                    for (size_t i = 0; i < structures_.size(); ++i)
                        if (structures_[i].file == path) structureSelected_ = (int)i;
                    structureStatus_ = trf("Nahráno: {0}", {test.name});
                } else {
                    std::error_code ec;
                    std::filesystem::remove(path, ec);
                    structureStatus_ = trf("Nejde nahrát: {0}", {tr(err)});
                }
            } else if (id == PromptPackName && !text.empty()) {
                packs_.createPack(text.substr(0, 40));
                packList_ = packs_.list();
            } else if (id == PromptShaderPackName && !text.empty()) {
                // 1.5.5: an empty pack with the Vibrant numbers in shaders/shader.json, switched on.
                const std::string packId = packs_.createPack(text.substr(0, 40));
                writeShaderPackFiles(packs_.packDir(packId), text.substr(0, 40));
                packs_.setEnabled(packId, true);
                settings_.graphics = GraphicsMode::Enhanced;
                rebuildFromPacks();
            }
        }
    }

    updateLan(dt);

    ui_.begin(width_ / guiScale_, height_ / guiScale_);
    const float W = ui_.width(), H = ui_.height();
    renderer_.setBrightness(settings_.brightness);

    bool inGame = session_ && (screen_ == Screen::Playing || screen_ == Screen::Pause || screen_ == Screen::Inventory ||
                               screen_ == Screen::Structures || (screen_ == Screen::Developer && devReturn_ == Screen::Pause) ||
                               screen_ == Screen::Crafting || screen_ == Screen::Furnace || screen_ == Screen::Chest ||
                               screen_ == Screen::Death || screen_ == Screen::Trade || screen_ == Screen::Invite ||
                               screen_ == Screen::Document || screen_ == Screen::WorldMap || screen_ == Screen::Chat || screen_ == Screen::Book ||
                               (screen_ == Screen::Settings && settingsReturn_ == Screen::Pause));
    if (inGame) {
        if (screen_ == Screen::Playing) {
            session_->update(dt, time, W, H, playSettings());
            if (session_->dead()) setScreen(Screen::Death);
            else if (session_->wantsPause) setScreen(Screen::Pause);
            else if (session_->wantsInventory) setScreen(Screen::Inventory);
            else if (session_->wantsCrafting) setScreen(Screen::Crafting);
            else if (session_->wantsFurnace) setScreen(Screen::Furnace);
            else if (session_->wantsChest) setScreen(Screen::Chest);
            else if (session_->wantsTrade >= 0 && session_->wantsTrade < (int)session_->mobs().mobs().size()) {
                tradeMob_ = session_->wantsTrade;
                tradeProfession_ = (int)session_->mobs().mobs()[tradeMob_].profession;
                setScreen(Screen::Trade);
            }
            else if (session_->wantsDocument >= 0) {
                documentPage_ = session_->wantsDocument;
                setScreen(Screen::Document);
            }
            else if (session_->wantsBook > 0) {
                bookId_ = session_->wantsBook, bookPage_ = 0;
                setScreen(Screen::Book);
            }
            session_->wantsBook = 0;
            session_->wantsTrade = -1;
            session_->wantsDocument = -1;
            session_->wantsPause = session_->wantsInventory = session_->wantsCrafting = session_->wantsFurnace = false;
            session_->wantsChest = false;
            if (session_->wantsChat) {
                session_->wantsChat = false;
                chatScroll_ = 0;
                setScreen(Screen::Chat);  // 1.5.7: a chat window instead of a bare line
            }
            if (session_->wantsWorldMap) {
                session_->wantsWorldMap = false;
                mapCx_ = session_->player().pos.x, mapCz_ = session_->player().pos.z, mapScale_ = 1, mapSelected_ = -1, mapDirty_ = true;
                setScreen(Screen::WorldMap);
            }
            if (session_->travelled) {
                session_->travelled = false;
                bool nether = session_->currentWorld().dimension() == Dimension::Nether;
                startLoading(nether ? "Vstup do dimenze úhelného kamene" : "Návrat do Overworldu", true);
            }
        } else if (screen_ == Screen::Inventory || screen_ == Screen::Crafting || screen_ == Screen::Furnace || screen_ == Screen::Chest) {
            session_->updateFurnaces(dt);
        }
        drawWorldOnce(session_->currentWorld());
        bool hud = screen_ != Screen::Death && screen_ != Screen::Settings && !(settings_.hideGui && screen_ == Screen::Playing);
        session_->render(renderer_, ui_, playSettings(), hud);
        if (now_ < achievementUntil_ && !achievementBanner_.empty()) {
            // "Achievement!" banner below the pause button, like the classic toasts.
            std::string line = trf("Úspěch: {0}", {tr(achievementBanner_)});
            std::string coins = "+" + std::to_string(achievementCoins_);
            float bw = std::max(ui_.textWidth(line), ui_.textWidth(coins) + 12) + 20, bx = std::floor((W - bw) / 2), by = 26;
            ui_.fill(bx + 2, by + 2, bw, 26, {0, 0, 0, 0.5f});
            ui_.frame(bx, by, bw, 26, theme::kPanel, theme::kGreenBorder);
            ui_.icon(bx + 5, by + 5, Font::Star, {1, 0.85f, 0.2f, 1});
            ui_.text(bx + 16, by + 4, line, theme::kTextBright, 1, false);
            ui_.icon(bx + 16, by + 15, Font::Coin, theme::kGold);
            ui_.text(bx + 26, by + 15, coins, theme::kGold, 1, false);
        }
        if (screen_ == Screen::Playing || screen_ == Screen::Pause) drawChat();
        if (screen_ == Screen::Playing || screen_ == Screen::Inventory) tutorial(dt);
    } else if (screen_ == Screen::Loading && session_) {
        loadingScreen(dt);
    } else {
        updateMenuBackground(dt);
    }

    const bool splash = splashTime_ < kSplashLength;
    if (splash && splashTime_ < kSplashLength - 0.6f) {
        splashScreen(dt);  // nothing under it takes taps yet
        renderer_.drawUi(ui_.list(), W, H);
        ui_.endFrame();
        return;
    }
    switch (screen_) {
        case Screen::Title: titleScreen(); break;
        case Screen::WorldList: worldListScreen(); break;
        case Screen::CreateWorld: createWorldScreen(); break;
        case Screen::EditWorld: editWorldScreen(); break;
        case Screen::Store: storeScreen(); break;
        case Screen::Skins: skinsScreen(); break;
        case Screen::ConfirmDelete: confirmDeleteScreen(); break;
        case Screen::Settings: settingsScreen(); break;
        case Screen::Packs: packsScreen(); break;
        case Screen::PackFiles: packFilesScreen(); break;
        case Screen::JsonEditor: jsonEditorScreen(); break;
        case Screen::Pause: pauseScreen(); break;
        case Screen::Structures: structuresScreen(); break;
        case Screen::Developer: developerScreen(); break;
        case Screen::Achievements: achievementsScreen(); break;
        case Screen::Joining: joiningScreen(); break;
        case Screen::Loading: if (!session_) setScreen(Screen::Title); break;
        case Screen::Inventory: inventoryScreen(); break;
        case Screen::Crafting: craftingScreen(); break;
        case Screen::Furnace: furnaceScreen(); break;
        case Screen::Chest: chestScreen(); break;
        case Screen::Trade: tradeScreen(); break;
        case Screen::Death: deathScreen(); break;
        case Screen::Invite: inviteScreen(); break;
        case Screen::OfflineGuide: offlineGuideScreen(); break;
        case Screen::Document: documentScreen(); break;
        case Screen::Credits: creditsScreen(); break;
        case Screen::Trailers: trailersScreen(); break;
        case Screen::WorldMap: worldMapScreen(); break;
        case Screen::Chat: chatScreen(); break;
        case Screen::Book: bookScreen(); break;
        case Screen::Playing: break;
    }

    if (splash) splashScreen(dt);  // fading out over the title
    renderer_.drawUi(ui_.list(), W, H);
    ui_.endFrame();
}

void Game::splashScreen(float dt) {
    // VoxelSoftworks: three blocks drop into place, the name fades in, then the menu.
    const float W = ui_.width(), H = ui_.height();
    if (ui_.tapped(0, 0, W, H) && splashTime_ < kSplashLength - 0.6f) splashTime_ = kSplashLength - 0.6f;  // tap to skip
    splashTime_ += std::min(dt, 0.05f);
    const float t = splashTime_;
    auto ease = [](float k) {
        k = std::clamp(k, 0.0f, 1.0f);
        return k * k * (3 - 2 * k);
    };
    const float fade = 1 - ease((t - (kSplashLength - 0.6f)) / 0.6f);
    ui_.fill(0, 0, W, H, {0, 0, 0, fade});
    const float size = std::floor(std::min(W, H) / 9), gap = std::floor(size / 6);
    const float x0 = std::floor(W / 2 - (3 * size + 2 * gap) / 2), y = std::floor(H / 2 - size * 0.9f);
    const int tiles[3] = {T_GRASS_SIDE, T_STONE, T_PLANKS};
    for (int k = 0; k < 3; ++k) {
        const float start = 0.25f + k * 0.25f, k01 = (t - start) / 0.45f;
        if (k01 <= 0) continue;
        // Falls, lands with a little bounce.
        const float fall = k01 < 1 ? (1 - k01 * k01) : 0;
        const float bounce = k01 >= 1 && k01 < 1.4f ? std::sin((k01 - 1) / 0.4f * kPi) * size * 0.08f : 0;
        if (k01 >= 1 && splashLanded_ <= k) {
            splashLanded_ = k + 1;
            if (settings_.uiSounds) playSound(Sound::Place);
        }
        const float a = std::min(1.0f, k01 * 3) * fade;
        ui_.tile(x0 + k * (size + gap), y - fall * H * 0.45f - bounce, size, tiles[k], {1, 1, 1, a});
    }
    const float scale = ui_.textWidth("VOXELSOFTWORKS", 3) < W - 20 ? 3 : 2;
    const float ta = ease((t - 1.15f) / 0.6f) * fade;
    if (ta > 0) ui_.textCentered(W / 2, y + size + gap * 2 + (1 - ta) * 6, "VOXELSOFTWORKS", {1, 1, 1, ta}, scale);
}

// ---------------------------------------------------------------- menu screens

using namespace theme;

void Game::unlockAchievement(const std::string& id) {
    int coins = profile_.unlock(id);
    if (coins <= 0) return;
    profile_.save(profilePath_);
    achievementBanner_ = findAchievement(id)->name;
    achievementCoins_ = coins;
    achievementUntil_ = now_ + 4;
    playSound(Sound::Coin);
}

void Game::achievementsScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Úspěchy", kTitle, 1, false);
    ui_.text(x + 2 + ui_.textWidth("Úspěchy") + 10, y + 2,
             std::to_string(profile_.achievements.size()) + " / " + std::to_string(kAchievementCount), kGold, 1, false);
    ui_.text(x + 2, y + 14, "Plň cíle v Přežití a získej bonusové mince.", kTextDim, 1, false, w);
    const float listY = y + 28, listH = h - 28 - 30, rowH = 24, colW = std::floor((w - 6) / 2);
    ui_.frame(x, listY, w, listH, kPanelInner, kSeparator);
    const int rows = (kAchievementCount + 1) / 2;
    listScroll_ -= ui_.dragY(x, listY, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, rows * rowH - (listH - 4)));
    for (int i = 0; i < kAchievementCount; ++i) {
        const Achievement& a = kAchievements[i];
        float rx = x + 3 + (i % 2) * (colW + 2), ry = listY + 2 + (i / 2) * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        bool done = profile_.achievements.count(a.id) > 0;
        ui_.frame(rx, ry, colW - 2, rowH - 2, done ? kGreen : kButton, done ? kGreenBorder : kSeparator);
        ui_.icon(rx + 4, ry + 7, Font::Star, done ? Color{1, 0.85f, 0.2f, 1} : Color{0.3f, 0.3f, 0.3f, 1});
        ui_.text(rx + 16, ry + 3, a.name, done ? kTextBright : kText, 1, false, colW - 60);
        ui_.text(rx + 16, ry + 12, a.description, kTextDim, 1, false, colW - 60);
        std::string reward = "+" + std::to_string(a.coins);
        ui_.icon(rx + colW - 14 - ui_.textWidth(reward), ry + 7, Font::Coin, kGold);
        ui_.text(rx + colW - 6 - ui_.textWidth(reward), ry + 7, reward, kGold, 1, false);
    }
    if (ui_.button(x + w - 80, y + h - 24, 80, 20, "Zpět")) setScreen(achievementsReturn_);
}


// ---------------------------------------------------------------- loading screen

namespace {
const char* const kLoadingTips[] = {
    "Tip: Truhly ve zříceninách a kobkách skrývají poklady.",
    "Tip: Rubín najdeš hluboko pod zemí.",
    "Tip: Úspěchy v Přežití dávají bonusové mince.",
    "Tip: Otevři svět pro LAN a hraj s kamarády.",
    "Tip: Loďka je nejrychlejší cesta přes oceán.",
    "Tip: Pod zasněženými horami bývá tvrdý kámen.",
};
constexpr int kLoadingTipCount = sizeof(kLoadingTips) / sizeof(kLoadingTips[0]);
}  // namespace

void Game::startLoading(const std::string& title, bool dimension) {
    loadingTitle_ = title;
    loadingDimension_ = dimension;
    loadingTime_ = 0;
    loadingProgress_ = 0;
    loadingTip_ = (loadingTip_ + 1 + (int)(std::fmod(now_ * 13.7, 1000.0))) % kLoadingTipCount;
    setScreen(Screen::Loading);
}

void Game::loadingScreen(float dt) {
    // The work: generate the chunks around the player and build their meshes, a few per frame.
    World& world = session_->currentWorld();
    drawWorldOnce(world);
    const Vec3 p = session_->player().pos;
    const int pcx = floorDiv((int)std::floor(p.x), kChunkSize), pcz = floorDiv((int)std::floor(p.z), kChunkSize);
    const int radius = std::clamp(settings_.renderDistance, 2, 3);
    world.ensureChunksAround(pcx, pcz, radius + 1, 6);
    renderer_.updateMeshes(world, 6);
    int total = 0, loaded = 0, meshed = 0;
    for (int dx = -radius; dx <= radius; ++dx)
        for (int dz = -radius; dz <= radius; ++dz) {
            if (dx * dx + dz * dz > radius * radius) continue;
            ++total;
            Chunk* c = world.getChunk(pcx + dx, pcz + dz);
            if (c) ++loaded;
            if (c && !c->dirty) ++meshed;
        }
    float target = total ? 0.5f * loaded / total + 0.5f * meshed / total : 1;
    loadingProgress_ = std::max(loadingProgress_, target);
    loadingTime_ += dt;
    const float minTime = loadingDimension_ ? 1.0f : 0.35f;
    const bool done = (loadingProgress_ >= 1 && loadingTime_ >= minTime) || loadingTime_ > 12;

    // The look: a dark tiled background like the classic loading screens.
    const float W = ui_.width(), H = ui_.height(), ts = 32;
    const bool nether = world.dimension() == Dimension::Nether;
    const int tile = loadingDimension_ ? (nether ? T_NETHERRACK : T_DIRT) : T_DIRT;
    for (float y = 0; y < H; y += ts)
        for (float x = 0; x < W; x += ts) ui_.tile(x, y, ts, tile, {0.25f, 0.25f, 0.25f, 1});
    if (loadingDimension_) {
        // Portal swirl: purple waves sweeping over the background.
        for (int i = 0; i < 12; ++i) {
            float phase = loadingTime_ * 1.7f + i * 0.55f;
            float a = 0.10f + 0.08f * std::sin(phase);
            float band = H / 12;
            ui_.fill(0, i * band + std::sin(phase * 0.7f) * 4, W, band, {0.45f, 0.1f, 0.75f, a});
        }
        float fade = std::clamp(1 - loadingTime_ * 1.5f, 0.0f, 1.0f);  // fades in from the portal
        ui_.fill(0, 0, W, H, {0.55f, 0.2f, 0.85f, 0.6f * fade});
    }
    ui_.textCentered(W / 2, H / 2 - 30, loadingTitle_, {1, 1, 1, 1});
    ui_.textCentered(W / 2, H / 2 - 14, loadingProgress_ < 0.5f ? "Generování terénu" : "Stavění terénu", kTextBright);
    // Progress bar.
    const float bw = std::min(200.0f, W - 40), bx = std::floor((W - bw) / 2), by = std::floor(H / 2 + 4);
    ui_.fill(bx - 1, by - 1, bw + 2, 6, {0.05f, 0.05f, 0.05f, 1});
    ui_.fill(bx, by, bw, 4, {0.35f, 0.35f, 0.35f, 1});
    ui_.fill(bx, by, std::floor(bw * std::min(loadingProgress_, 1.0f)), 4,
             loadingDimension_ ? Color{0.65f, 0.35f, 0.95f, 1} : Color{0.45f, 0.85f, 0.3f, 1});
    ui_.textCentered(W / 2, by + 9, std::to_string((int)std::round(std::min(loadingProgress_, 1.0f) * 100)) + " %", kTextDim);
    ui_.textCentered(W / 2, H - 22, kLoadingTips[loadingTip_], kTextDim);
    if (done) {
        setScreen(Screen::Playing);
        if (hostAfterLoad_) openToLan();  // chosen from the Multiplayer list
        hostAfterLoad_ = false;
    }
}

// ---------------------------------------------------------------- chat

std::string Game::playerName() const {
    // The default name follows the language ("Hráč", "Player", "Jugador"...); a chosen one stays as typed.
    std::string name = profile_.name.empty() || profile_.name == "Hráč" ? tr("Hráč") : profile_.name;
    return isVip(profile_, nowSeconds()) ? "[VIP] " + name : name;  // VIP players are seen as such
}

void Game::addChat(const std::string& line) {
    chat_.push_back({line, now_});
    if (chat_.size() > 50) chat_.erase(chat_.begin());
}

void Game::submitChat(const std::string& raw) {
    std::string text = raw.substr(0, 200);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n')) text.pop_back();
    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
    if (text.empty() || !session_) return;
    if (text[0] == '/') {
        addChat(text);
        std::string cmd = text.substr(1, text.find(' ') == std::string::npos ? std::string::npos : text.find(' ') - 1);
        if (cmd == "list") {
            std::string names = playerName();
            for (const RemoteAvatar& a : session_->remotePlayers) names += ", " + a.name;
            addChat(trf("Hráči ({0}): {1}", {std::to_string(1 + session_->remotePlayers.size()), names}));
            return;
        }
        const bool cheats = !session_->survival() || session_->info().allowCommands || profile_.isDeveloper();
        for (const std::string& reply : runCommand(*session_, text, cheats)) addChat(reply);
        return;
    }
    addChat("<" + playerName() + "> " + text);
    lan_.sendChat(text);
}

void Game::drawChat() {
    // The newest lines, top left under the coordinates, wrapped; each fades out after 10 seconds.
    const float maxW = std::floor(ui_.width() * 0.55f);
    struct Row {
        std::string text;
        Color color;
    };
    std::vector<Row> rows;
    for (int i = (int)chat_.size() - 1; i >= 0 && rows.size() < 8; --i) {
        float age = (float)(now_ - chat_[i].time);
        if (age > 10) break;
        float a = std::clamp(10 - age, 0.0f, 1.0f);
        const std::string& t = chat_[i].text;
        Color c = t[0] == '<' ? Color{1, 1, 1, a} : t[0] == '/' ? Color{0.7f, 0.7f, 0.7f, a} : Color{1, 1, 0.6f, a};
        std::vector<std::string> lines = wrapText(t, maxW - 8);
        for (int k = (int)lines.size() - 1; k >= 0; --k) rows.push_back({lines[k], c});
    }
    float y = 30;
    for (int i = std::min((int)rows.size(), 8) - 1; i >= 0; --i) {
        const Row& r = rows[i];
        ui_.fill(4, y, ui_.textWidth(r.text) + 8, 11, {0, 0, 0, 0.45f * r.color.a});
        ui_.text(8, y + 1, r.text, r.color, 1, false);
        y += 11;
    }
}

// ---------------------------------------------------------------- LAN games

const Skin* Game::currentSkin() const {
    int i = skinIndex(profile_.skin);
    if (i < 0) i = 0;
    return skins_.empty() ? nullptr : &skins_[i];
}

void Game::setLanActive(bool active) {
    if (active == lanActive_) return;
    lanActive_ = active;
    if (onLanActive) onLanActive(active);
}

void Game::openToLan() {
    if (!session_ || lan_.active()) return;
    const Skin* skin = currentSkin();
    std::string err;
    if (lan_.host(*session_, session_->info().name, playerName(), skin ? skin->rgba : std::vector<uint8_t>{},
                  skin && skin->slim, err)) {
        addChat(trf("Svět je otevřený v síti LAN (port {0})", {std::to_string(lan_.port())}));
    } else {
        addChat(tr(err));
    }
}

void Game::joinLan(const std::string& address, int port, const std::string& label) {
    const Skin* skin = currentSkin();
    std::string err;
    if (!lan_.join(address, port, playerName(), skin ? skin->rgba : std::vector<uint8_t>{}, skin && skin->slim, err)) {
        toast_ = trf("Nepodařilo se připojit: {0}", {tr(err)});
        toastUntil_ = now_ + 6;
        return;
    }
    joinLabel_ = label;
    setScreen(Screen::Joining);
}

void Game::updateLan(float dt) {
    if (discovery_.running()) discovery_.poll(now_);
    setLanActive(discovery_.running() || lan_.active());
    if (!lan_.active()) return;
    if (lan_.role() == Multiplayer::Role::Host) lan_.setRules(settings_.mpPvp, settings_.mpProtectSpawn);
    if (session_)
        lan_.setOpenContainer(screen_ == Screen::Chest ? 0 : screen_ == Screen::Furnace ? 1 : -1,
                              screen_ == Screen::Furnace ? session_->openFurnace : session_->openChest);
    lan_.update(session_.get(), dt, now_);
    if (lan_.role() == Multiplayer::Role::Client) {
        if (lan_.joinState() == Multiplayer::JoinState::Failed) {
            std::string err = tr(lan_.error());
            bool playing = session_ != nullptr;
            if (playing) endSession();
            lan_.stop();
            toast_ = trf(playing ? "Spojení ztraceno: {0}" : "Nepodařilo se připojit: {0}", {err});
            toastUntil_ = now_ + 6;
            setScreen(Screen::WorldList);
            return;
        }
        if (!session_ && lan_.joinState() == Multiplayer::JoinState::Ready) {
            // Our copy of the host's world lives in <data>/lan/<host>_<world>; level.dat keeps
            // where we were and what we carried the last time.
            const Multiplayer::Welcome& w = lan_.welcome();
            std::string key;
            for (char c : w.host + "_" + w.world)
                key += std::isalnum((unsigned char)c) || c == '_' || c == '-' ? c : '_';
            std::string folder = dataDir_ + "/lan/" + key.substr(0, 60);
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            lan_.writeWorld(folder);
            WorldInfo info;
            PlayerState player;
            if (!loadLevel(folder, info, player) || info.seed != w.seed) player = PlayerState{};
            info.folder = folder;
            info.name = w.world;
            info.seed = w.seed;
            info.mode = w.mode;
            startSession(info, player);
            addChat(trf("Připojeno ke hře hráče {0}", {w.host}));
            lan_.update(session_.get(), 0, now_);
        }
    }
    if (hasSurface_)
        for (Multiplayer::Player& p : lan_.players())
            if (p.skinDirty && !p.skin.empty()) {
                renderer_.setRemoteSkin(p.skinSlot, p.skin);
                p.skinDirty = false;
            }
    for (const std::string& m : lan_.takeMessages()) {
        if (m[0] == '#') addChat(m.substr(1));
        else addChat(trf(m[0] == '+' ? "{0} se připojil(a) ke hře" : "{0} opustil(a) hru", {m.substr(1)}));
    }
    if (session_ && lan_.playerCount() >= 2) unlockAchievement("multiplayer");
}

void Game::joiningScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Připojování ke hře", kTitle, 1, false);
    std::string status = lan_.joinState() == Multiplayer::JoinState::Downloading
                             ? trf("Stahuji svět... {0} chunků", {std::to_string(lan_.chunksReceived())})
                             : trf("Připojování k {0}...", {joinLabel_});
    ui_.textCentered(x + w / 2, y + h / 2 - 10, status, kTextBright);
    // A little busy indicator.
    for (int i = 0; i < 3; ++i) {
        float a = 0.3f + 0.7f * (0.5f + 0.5f * std::sin((float)now_ * 5 - i * 0.8f));
        ui_.fill(x + w / 2 - 14 + i * 10, y + h / 2 + 6, 6, 6, {0.4f, 0.8f, 0.3f, a});
    }
    float bw = std::min(160.0f, w - 8);
    if (ui_.button(x + w / 2 - bw / 2, y + h - 26, bw, 22, "Zrušit")) {
        lan_.stop();
        setScreen(Screen::WorldList);
    }
}

std::vector<std::string> Game::wrapText(const std::string& source, float width) const {
    const std::string text = tr(source);  // wrap the translated text, not the key
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&] {
        if (word.empty()) return;
        std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && ui_.textWidth(candidate) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ' || c == '\n') {
            flush();
            if (c == '\n') { lines.push_back(line); line.clear(); }
        } else {
            word += c;
        }
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void Game::menuWindow(float& x, float& y, float& w, float& h, bool logo) {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.35f});
    // Side strips show the world, the window is centred like in the mock-ups.
    float ww = std::min(W - 16, 560.0f), wx = std::floor((W - ww) / 2);
    ui_.fill(wx + 2, 6, ww, H - 10, {0, 0, 0, 0.5f});  // shadow
    ui_.frame(wx, 4, ww, H - 10, kPanel, kBorder);
    y = 10;
    if (logo) {
        ui_.textCentered(W / 2, 10, "VOXELCRAFT", kTitle, 2);
        ui_.textCentered(W / 2, 30, "VOXEL SURVIVAL", kTextDim, 1);
        y = 44;
    }
    x = wx + 8;
    w = ww - 16;
    h = H - 10 - y;
}

void Game::coinsPill(float right, float y) {
    std::string text = trf("{0} MINCÍ", {std::to_string(profile_.coins)});
    float w = ui_.textWidth(text) + 22;
    ui_.frame(right - w, y, w, 16, kPanel, kBorder);
    ui_.icon(right - w + 4, y + 4, Font::Coin, kGold);
    ui_.text(right - w + 16, y + 4, text, kGold, 1, false);
}

void Game::titleScreen() {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.2f});
    float logoY = std::floor(H * 0.1f);
    ui_.textCentered(W / 2 + 2, logoY + 2, "VOXELCRAFT", {0, 0, 0, 0.5f}, 4);
    ui_.textCentered(W / 2, logoY, "VOXELCRAFT", kTitle, 4);
    ui_.textCentered(W / 2, logoY + 38, "VOXEL SURVIVAL", kTextBright, 1);
    // Splash text, gently pulsing.
    float pulse = 1 + 0.06f * (float)std::sin(now_ * 6);
    std::string splash = kSplashes[splash_ % kSplashCount];
    float logoRight = W / 2 + ui_.textWidth("VOXELCRAFT", 4) / 2;
    ui_.text(logoRight - ui_.textWidth(splash, pulse) * 0.6f, logoY + 30, splash, {1, 1, 0.33f, 1}, pulse);
    coinsPill(W - 4, 4);

    // Button panel
    float pw = std::min(320.0f, W - 20), px = std::floor((W - pw) / 2), py = std::floor(H * 0.38f);
    float bw = (pw - 24) / 2, bh = 22, gap = 6;
    ui_.fill(px + 2, py + 2, pw, 4 * (bh + gap) + 10, {0, 0, 0, 0.45f});
    ui_.frame(px, py, pw, 4 * (bh + gap) + 10, kPanel, kBorder);
    float x = px + 8, y = py + 8;
    if (ui_.button(x, y, bw, bh, "Hrát", true, true)) setScreen(Screen::WorldList);
    if (ui_.button(x + bw + 8, y, bw, bh, "Nastavení")) {
        settingsReturn_ = Screen::Title;
        setScreen(Screen::Settings);
    }
    y += bh + gap;
    if (ui_.button(x, y, bw, bh, "Obchod")) setScreen(Screen::Store);
    if (ui_.button(x + bw + 8, y, bw, bh, "Profil a skiny")) setScreen(Screen::Skins);
    y += bh + gap;
    if (ui_.button(x, y, pw - 16, bh, "Módy a balíčky")) setScreen(Screen::Packs);
    y += bh + gap;
    if (ui_.button(x, y, pw - 16, bh, "Ukončit hru") && onQuit) onQuit();

    if (now_ < toastUntil_ && !toast_.empty()) {
        float tw = ui_.textWidth(toast_) + 16;
        ui_.frame((W - tw) / 2, py - 22, tw, 16, kGreen, kGreenBorder);
        ui_.textCentered(W / 2, py - 18, toast_, kTextBright);
    }
    ui_.text(3, H - 10, kVersion, kText);
    std::string note = "VoxelSoftworks";
    ui_.text(W - ui_.textWidth(note) - 3, H - 10, note, kAccent);
    // 1.5.7: the studio name opens the credits.
    if (ui_.tapped(W - ui_.textWidth(note) - 8, H - 16, ui_.textWidth(note) + 8, 16)) {
        creditsStart_ = now_;
        setScreen(Screen::Credits);
    }
}

void Game::creditsScreen() {
    const float W = ui_.width(), H = ui_.height();
    // Rolling credits over the dimmed menu, in the style of the game's pixel font.
    ui_.fill(0, 0, W, H, {0.03f, 0.03f, 0.06f, 0.93f});
    struct Line {
        const char* text;
        float scale;
        Color color;
        bool translate;
    };
    const Color gold{1.0f, 0.82f, 0.43f, 1}, white{1, 1, 1, 1}, dim{0.75f, 0.75f, 0.8f, 1}, green{0.63f, 0.92f, 0.5f, 1};
    static const Line lines[] = {
        {"VOXELSOFTWORKS", 2, white, false},
        {"", 1, white, false},
        {"VOXELCRAFT ALPHA", 3, gold, false},
        {"- GAME CREDITS -", 2, gold, false},
        {"", 1, white, false},
        {"+ LEAD DEVELOPER +", 2, gold, false},
        {"Hoppy_Hopsy21", 2, white, false},
        {"", 1, white, false},
        {"+ AI COLLABORATION +", 2, gold, false},
        {"Claude, by Anthropic", 2, white, false},
        {"", 1, white, false},
        {"TECHNICAL STACK", 2, gold, false},
        {"Custom C++ engine, OpenGL ES 3.0", 1, dim, false},
        {"GLSL shader pipeline: Classic and Enhanced", 1, dim, false},
        {"Native Android runtime (NDK)", 1, dim, false},
        {"", 1, white, false},
        {"GAME WORLD", 2, gold, false},
        {"Overworld and Crimson Void", 1, dim, false},
        {"Cornerstone Dimension", 1, dim, false},
        {"Powerstone Wastelands", 1, dim, false},
        {"", 1, white, false},
        {"MUSIC, TEXTURES AND TRAILERS", 2, gold, false},
        {"Made for VoxelCraft, our own", 1, dim, false},
        {"", 1, white, false},
        {"Made with love by and for people with autism", 1, green, false},
        {"Special thanks to our community and modders!", 1, green, false},
        {"", 1, white, false},
        {"(c) 2026 VoxelSoftworks", 1, dim, false},
    };
    float y = H - (float)(now_ - creditsStart_) * 18;  // rolls up
    float total = 0;
    for (const Line& l : lines) total += (l.text[0] ? 10 : 6) * l.scale + 4;
    for (const Line& l : lines) {
        if (l.text[0] && y > -30 && y < H + 10) ui_.textCentered(W / 2, y, l.text, l.color, l.scale);
        y += (l.text[0] ? 10 : 6) * l.scale + 4;
    }
    // Little blocks along the sides, like the corner art of the poster.
    for (int k = 0; k < 3; ++k) {
        static const int kTiles[3] = {T_GRASS_SIDE, T_COBBLE, T_PLANKS};
        ui_.tile(6 + k * 14, 6, 12, kTiles[k], {1, 1, 1, 0.9f});
        ui_.tile(W - 18 - k * 14, 6, 12, kTiles[k], {1, 1, 1, 0.9f});
    }
    // When the roll is over the special access opens by itself; Skip goes there at once.
    if (now_ - creditsStart_ > (H + total) / 18) setScreen(Screen::Trailers);
    if (ui_.button(W - 70, H - 26, 64, 20, "Přeskočit", true, true)) setScreen(Screen::Trailers);
    if (ui_.button(6, H - 26, 64, 20, "Zpět")) setScreen(Screen::Title);
}

void Game::worldMapScreen() {
    // 1.5.7: everything the map item has drawn, zoomed and panned with a finger, with the
    // places the player named.
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.55f});
    constexpr int kImg = 192;
    const float ms = std::floor(std::min(H - 50.0f, W - 150.0f)), mx = std::floor(12 + (W - 150 - ms) / 2), my = 24;
    // Dragging pans (the picture follows the finger).
    const float k = ms / kImg;
    const float dx = ui_.dragX(mx, my, ms, ms), dz = ui_.dragY(mx, my, ms, ms);
    if (dx != 0 || dz != 0) mapCx_ -= dx / k * mapScale_, mapCz_ -= dz / k * mapScale_, mapDirty_ = true;
    if (mapDirty_) {
        session_->paperMaps().render(session_->dimIndex(), mapCx_, mapCz_, mapScale_, kImg, mapImage_);
        renderer_.setMapTexture(mapImage_, kImg, kImg);
        mapDirty_ = false;
    }
    ui_.fill(mx - 5, my - 5, ms + 10, ms + 10, {0.42f, 0.3f, 0.18f, 0.95f});
    ui_.image(TexMap, mx, my, ms, ms, 0, 0, 1, 1);
    auto toScreen = [&](float wx, float wz, float& sx, float& sy) {
        sx = mx + ms / 2 + (wx - mapCx_) / mapScale_ * k;
        sy = my + ms / 2 + (wz - mapCz_) / mapScale_ * k;
        return sx >= mx && sy >= my && sx < mx + ms && sy < my + ms;
    };
    // The player.
    float px, py;
    if (toScreen(session_->player().pos.x, session_->player().pos.z, px, py)) {
        const float fx = std::sin(session_->player().yaw), fz = -std::cos(session_->player().yaw), r = 5.0f;
        const float pts[4][2] = {{px + fx * r, py + fz * r},
                                 {px - fx * r * 0.7f + fz * r * 0.7f, py - fz * r * 0.7f - fx * r * 0.7f},
                                 {px - fx * r * 0.25f, py - fz * r * 0.25f},
                                 {px - fx * r * 0.7f - fz * r * 0.7f, py - fz * r * 0.7f + fx * r * 0.7f}};
        const float uv[4][2] = {};
        ui_.quad(TexNone, pts, uv, {0.75f, 0.1f, 0.1f, 1});
    }
    // Named places: a pin and the name; tap one to select it.
    auto& marks = session_->waypoints();
    for (int i = 0; i < (int)marks.size(); ++i) {
        if (marks[i].dim != session_->dimIndex()) continue;
        float sx, sy;
        if (!toScreen(marks[i].x + 0.5f, marks[i].z + 0.5f, sx, sy)) continue;
        const Color c = i == mapSelected_ ? Color{1, 0.85f, 0.2f, 1} : Color{0.15f, 0.35f, 0.85f, 1};
        ui_.fill(sx - 3, sy - 3, 6, 6, {0, 0, 0, 0.8f});
        ui_.fill(sx - 2, sy - 2, 4, 4, c);
        ui_.text(sx + 5, sy - 4, marks[i].name, {0.15f, 0.1f, 0.05f, 1}, 1, false);
        if (ui_.tapped(sx - 8, sy - 8, 16 + ui_.textWidth(marks[i].name), 16)) mapSelected_ = i;
    }
    // The middle of the view (where a new place is marked).
    ui_.fill(mx + ms / 2 - 4, my + ms / 2, 9, 1, {0, 0, 0, 0.6f});
    ui_.fill(mx + ms / 2, my + ms / 2 - 4, 1, 9, {0, 0, 0, 0.6f});
    ui_.text(mx, my - 14, trf("Mapa světa  {0}, {1}  (1:{2})", {std::to_string((int)mapCx_), std::to_string((int)mapCz_), std::to_string((int)mapScale_)}),
             kTextBright, 1, false);
    // Controls on the right.
    const float bx = mx + ms + 14, bw = W - bx - 8;
    float by = my;
    if (ui_.button(bx, by, bw / 2 - 2, 20, "+", mapScale_ > 1)) mapScale_ /= 2, mapDirty_ = true;
    if (ui_.button(bx + bw / 2 + 2, by, bw / 2 - 2, 20, "-", mapScale_ < 16)) mapScale_ *= 2, mapDirty_ = true;
    by += 26;
    if (ui_.button(bx, by, bw, 20, "Na mě")) mapCx_ = session_->player().pos.x, mapCz_ = session_->player().pos.z, mapDirty_ = true;
    by += 26;
    if (ui_.button(bx, by, bw, 20, "Označit místo", (bool)onTextPrompt, true) && onTextPrompt)
        onTextPrompt(PromptWaypoint, tr("Název místa"), "");
    by += 26;
    if (mapSelected_ >= 0 && mapSelected_ < (int)marks.size()) {
        ui_.text(bx, by, marks[mapSelected_].name, kAccent, 1, false, bw);
        ui_.text(bx, by + 10, std::to_string(marks[mapSelected_].x) + ", " + std::to_string(marks[mapSelected_].z), kTextDim, 1, false);
        by += 24;
        if (ui_.button(bx, by, bw, 20, "Ukázat")) mapCx_ = (float)marks[mapSelected_].x, mapCz_ = (float)marks[mapSelected_].z, mapDirty_ = true;
        by += 26;
        if (ui_.button(bx, by, bw, 20, "Smazat značku")) {
            marks.erase(marks.begin() + mapSelected_);
            mapSelected_ = -1;
        }
    }
    if (ui_.button(bx, H - 28, bw, 22, "Zpět")) {
        session_->refreshMapTexture();
        setScreen(Screen::Playing);
    }
}

void Game::chatScreen() {
    // 1.5.7: a chat window: the conversation, a line to write in and quick commands.
    const float W = ui_.width(), H = ui_.height();
    const float pw = std::min(W - 20, 420.0f), ph = H - 30, px = std::floor((W - pw) / 2), py = 10;
    ui_.frame(px, py, pw, ph, kPanel, kSeparator);
    ui_.text(px + 8, py + 6, "Chat", kTitle, 1, false);
    const float listY = py + 20, listH = ph - 20 - 58;
    ui_.frame(px + 6, listY, pw - 12, listH, kPanelInner, kSeparator);
    // Newest at the bottom; dragging scrolls back through older messages.
    chatScroll_ = std::max(0.0f, chatScroll_ + ui_.dragY(px + 6, listY, pw - 12, listH));
    std::vector<std::string> lines;
    for (const ChatLine& c : chat_)
        for (const std::string& l : wrapText(c.text, pw - 24)) lines.push_back(l);
    const int visible = (int)((listH - 6) / 10), skip = std::min((int)(chatScroll_ / 10), std::max(0, (int)lines.size() - visible));
    const int last = (int)lines.size() - skip, first = std::max(0, last - visible);
    float ly = listY + listH - 4 - (last - first) * 10;
    for (int i = first; i < last; ++i, ly += 10) ui_.text(px + 12, ly, lines[i], kText, 1, false, pw - 24);
    if (lines.empty()) ui_.textCentered(px + pw / 2, listY + listH / 2 - 4, "Zatím tu nic není. Napiš zprávu nebo příkaz.", kTextDim);
    // The line to write in (the phone's keyboard opens on a tap).
    const float iy = listY + listH + 6;
    if (ui_.textBox(px + 6, iy, pw - 80, 18, "", "Napiš zprávu nebo /příkaz...") && onTextPrompt)
        onTextPrompt(PromptChat, tr("Chat (příkazy začínají /)"), "");
    if (ui_.button(px + pw - 70, iy, 64, 18, "Odeslat", (bool)onTextPrompt, true) && onTextPrompt)
        onTextPrompt(PromptChat, tr("Chat (příkazy začínají /)"), "");
    // Quick commands.
    const char* quick[] = {"/help", "/time set day", "/weather clear", "/gamemode creative", "/gamemode survival"};
    float qx = px + 6;
    for (const char* q : quick) {
        const float qw = ui_.textWidth(q) + 10;
        if (qx + qw > px + pw - 6) break;
        if (ui_.button(qx, iy + 22, qw, 16, q)) onTextResult(PromptChat, q);
        qx += qw + 4;
    }
    if (ui_.button(px + pw - 50, py + 4, 44, 14, "Zpět")) setScreen(Screen::Playing);
}

void Game::bookScreen() {
    // 1.5.7 book and quill: one page at a time; while unsigned a page can be written (the
    // phone's keyboard), pages added, and the book signed with a title, after which it is read only.
    if (!session_ || bookId_ <= 0) return setScreen(Screen::Playing);
    Session::Book& b = session_->book(bookId_);
    const bool locked = session_->bookSigned(bookId_);
    bookPage_ = std::clamp(bookPage_, 0, (int)b.pages.size() - 1);
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.5f});
    const float pw = std::min(W - 120, 260.0f), ph = std::min(H - 16, 240.0f);
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    const Color ink{0.2f, 0.14f, 0.08f, 1}, paper{0.93f, 0.89f, 0.76f, 1};
    ui_.fill(px + 3, py + 3, pw, ph, {0, 0, 0, 0.4f});
    ui_.fill(px, py, pw, ph, locked ? Color{0.24f, 0.45f, 0.28f, 1} : Color{0.47f, 0.28f, 0.16f, 1});
    ui_.fill(px + 4, py + 4, pw - 8, ph - 8, paper);
    float ty = py + 10;
    if (locked && bookPage_ == 0) {
        ui_.textCentered(px + pw / 2, ty, b.title, {0.5f, 0.3f, 0.12f, 1});
        ui_.textCentered(px + pw / 2, ty + 11, trf("od {0}", {b.author}), {0.45f, 0.4f, 0.32f, 1});
        ty += 28;
    }
    ui_.textCentered(px + pw / 2, py + ph - 16, trf("Strana {0} z {1}", {std::to_string(bookPage_ + 1), std::to_string(b.pages.size())}),
                     {0.45f, 0.4f, 0.32f, 1});
    std::vector<std::string> lines;
    std::string para;
    std::istringstream text(b.pages[bookPage_]);
    while (std::getline(text, para))
        for (const std::string& l : wrapText(para, pw - 24)) lines.push_back(l);
    for (const std::string& l : lines) {
        if (ty > py + ph - 30) break;
        ui_.text(px + 12, ty, l, ink, 1, false);
        ty += 11;
    }
    if (!locked && b.pages[bookPage_].empty()) ui_.textCentered(px + pw / 2, py + ph / 2 - 4, "Klepni na Psát a napiš stránku.", {0.55f, 0.5f, 0.4f, 1});
    // Turning pages.
    if (ui_.button(px - 44, py + ph / 2 - 12, 36, 24, "<", bookPage_ > 0)) --bookPage_;
    if (ui_.button(px + pw + 8, py + ph / 2 - 12, 36, 24, ">", bookPage_ + 1 < (int)b.pages.size())) ++bookPage_;
    // Writing.
    float bx = 8, by = 8;
    const float bw = std::max(50.0f, px - 16);
    if (!locked) {
        if (ui_.button(bx, by, bw, 20, "Psát", (bool)onTextPrompt, true) && onTextPrompt)
            onTextPrompt(PromptBookPage, tr("Text stránky"), b.pages[bookPage_]);
        by += 26;
        if (ui_.button(bx, by, bw, 20, "+ Strana", b.pages.size() < 50)) {
            b.pages.insert(b.pages.begin() + bookPage_ + 1, "");
            ++bookPage_;
        }
        by += 26;
        if (ui_.button(bx, by, bw, 20, "- Strana", b.pages.size() > 1)) b.pages.erase(b.pages.begin() + bookPage_);
        by += 26;
        if (ui_.button(bx, by, bw, 20, "Podepsat", (bool)onTextPrompt) && onTextPrompt) onTextPrompt(PromptBookTitle, tr("Název knihy"), "");
    }
    if (ui_.button(bx, H - 28, bw, 22, "Hotovo")) setScreen(Screen::Playing);
}

void Game::trailersScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Speciální přístup: trailery", kTitle, 1, false);
    ui_.text(x + 2, y + 14, "Každá verze VoxelCraftu ve videu, od 0.1 po 1.6.0.", kTextDim, 1, false);
    struct Trailer {
        const char* title;
        const char* asset;
    };
    static const Trailer trailers[] = {
        {"0.1 - 1.2.0: šestiminutový příběh", "trailers/t01_0.1-1.2.0.mp4"},
        {"1.3.0: vesnice a farmaření", "trailers/t02_1.3.0.mp4"},
        {"1.4.0: módy, jeskyně a online", "trailers/t03_1.4.0.mp4"},
        {"1.5.0: Forgotten Powerstone", "trailers/t04_1.5.0.mp4"},
        {"1.5.1: opravená edice", "trailers/t05_1.5.1.mp4"},
        {"1.5.5: The Shaders Update", "trailers/t06_1.5.5.mp4"},
        {"1.5.7: louče, štít a Prasák", "trailers/t07_1.5.7.mp4"},
        {"1.6.0: Caves and Tunes", "trailers/t08_1.6.0.mp4"},
    };
    const float rowH = 22, top = y + 28, colW = (w - 8) / 2;
    for (int i = 0; i < (int)(sizeof(trailers) / sizeof(trailers[0])); ++i) {
        const float bx = x + (i % 2) * (colW + 8), by = top + (i / 2) * (rowH + 6);
        if (ui_.button(bx, by, colW, rowH, trailers[i].title, (bool)onPlayVideo) && onPlayVideo) onPlayVideo(trailers[i].asset);
    }
    if (ui_.button(x + w / 2 - 60, y + h - 26, 120, 22, "Zpět")) setScreen(Screen::Title);
}

void Game::worldListScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    // Two parts: your own worlds (Singleplayer) and games on the Wi-Fi (Multiplayer).
    const char* tabs[2] = {"Singleplayer", "Multiplayer"};
    float tx = x;
    for (int t = 0; t < 2; ++t) {
        float tw = ui_.textWidth(tabs[t]) + 18;
        if (ui_.button(tx, y - 1, tw, 15, tabs[t], true, playTab_ == t)) {
            playTab_ = t;
            worldScroll_ = 0;
        }
        tx += tw + 4;
    }
    if (now_ < toastUntil_ && !toast_.empty()) ui_.text(x + w - ui_.textWidth(toast_), y + 2, toast_, kAccent, 1, false);
    const bool multi = playTab_ == 1;
    const float listY = y + 16, listH = h - 16 - 32, rowH = 34;
    ui_.frame(x, listY, w, listH, kPanelInner, kSeparator);
    worldScroll_ -= ui_.dragY(x, listY, w, listH);
    // Multiplayer: games opened to the LAN, then your worlds to host. Singleplayer: your worlds.
    const std::vector<net::LanGame> lanGames = multi ? discovery_.games() : std::vector<net::LanGame>{};
    const size_t lanRows = lanGames.size(), headerRows = multi ? 1 : 0;
    float maxScroll = std::max(0.0f, (lanRows + headerRows + worlds_.size()) * rowH - (listH - 4));
    worldScroll_ = std::clamp(worldScroll_, 0.0f, std::max(0.0f, maxScroll));

    if (!multi && worlds_.empty()) ui_.textCentered(x + w / 2, listY + listH / 2 - 4, "Zatím nemáš žádný svět", kTextDim);
    for (size_t i = 0; i < lanRows; ++i) {
        float ry = listY + 2 + i * rowH - worldScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        const net::LanGame& g = lanGames[i];
        float rx = x + 2, rw = w - 4;
        bool down = ui_.held(rx, ry, rw, rowH - 2);
        ui_.frame(rx, ry, rw, rowH - 2, down ? kButtonDown : kButton, kGreenBorder);
        ui_.item(rx + 4, ry + 3, 26, GRASS);
        ui_.fill(rx + 4, ry + 21, 20, 9, {0.1f, 0.45f, 0.15f, 0.9f});
        ui_.text(rx + 6, ry + 22, "LAN", kTextBright, 1, false);
        ui_.text(rx + 36, ry + 6, g.world.empty() ? "Svět" : g.world, kTextBright, 1, false, rw - 80);
        std::string sub = trf("Hostitel: {0} · hráčů: {1}", {g.host, std::to_string(g.players)}) + " · " +
                          tr(modeName(g.mode == (int)GameMode::Creative ? GameMode::Creative : GameMode::Survival));
        ui_.text(rx + 36, ry + 18, sub, kTextDim, 1, false, rw - 80);
        float jw = ui_.textWidth("Připojit") + 14;
        ui_.frame(rx + rw - 6 - jw, ry + 6, jw, 20, down ? kButtonDown : kGreen, kGreenBorder);
        ui_.textCentered(rx + rw - 6 - jw / 2, ry + 12, "Připojit", kTextBright);
        if (ui_.tapped(rx, ry, rw, rowH - 2)) {
            joinLan(g.address, g.port, g.world);
            return;
        }
    }
    if (multi) {
        // The header between the games and your worlds.
        float ry = listY + 2 + lanRows * rowH - worldScroll_;
        if (ry >= listY && ry + rowH <= listY + listH + 1) {
            if (lanGames.empty()) {
                const int dots = (int)(now_ * 2) % 4;
                ui_.text(x + 8, ry + 4, tr("Hledám hry na tvé Wi-Fi") + std::string(dots, '.'), kTextDim, 1, false, w - 132);
            }
            // No Wi-Fi around: a phone's hotspot does the job, without using data.
            if (ui_.button(x + w - 118, ry + 1, 110, 15, "Bez internetu?")) {
                setScreen(Screen::OfflineGuide);
                return;
            }
            ui_.text(x + 8, ry + 18, "Tvoje světy: ťukni a otevři ho kamarádům v LAN", kAccent, 1, false, w - 16);
        }
    }
    for (size_t i = 0; i < worlds_.size(); ++i) {
        float ry = listY + 2 + (i + lanRows + headerRows) * rowH - worldScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;  // rows outside the list are skipped
        const WorldInfo& wi = worlds_[i];
        float rx = x + 2, rw = w - 4;
        const float actions = multi ? 0 : 54;
        bool down = ui_.held(rx, ry, rw - actions, rowH - 2);
        ui_.frame(rx, ry, rw, rowH - 2, down ? kButtonDown : kButton, kSeparator);
        // World "icon": a grass block like in the mock-up.
        ui_.item(rx + 4, ry + 3, 26, GRASS);
        ui_.text(rx + 36, ry + 6, wi.name.empty() ? "Svět" : wi.name, kTextBright, 1, false, rw - 80);
        ui_.text(rx + 36, ry + 18, tr(modeName(wi.mode)) + " · " + formatDate(wi.lastPlayed), kTextDim, 1, false, rw - 80);
        if (multi) {
            float hw = ui_.textWidth("Hostovat") + 14;
            ui_.frame(rx + rw - 6 - hw, ry + 6, hw, 20, down ? kButtonDown : kGreen, kGreenBorder);
            ui_.textCentered(rx + rw - 6 - hw / 2, ry + 12, "Hostovat", kTextBright);
        } else {
            if (ui_.button(rx + rw - 50, ry + (rowH - 2 - 20) / 2, 20, 20, "E")) {
                if (loadLevel(wi.folder, editInfo_, editPlayer_)) {
                    editGen_ = loadGenerator(wi.folder);
                    setScreen(Screen::EditWorld);
                }
                return;
            }
            if (ui_.button(rx + rw - 26, ry + (rowH - 2 - 20) / 2, 20, 20, "X")) {
                deleteIndex_ = (int)i;
                setScreen(Screen::ConfirmDelete);
                return;
            }
        }
        if (ui_.tapped(rx, ry, rw - actions, rowH - 2)) {
            WorldInfo info;
            PlayerState player;
            if (loadLevel(wi.folder, info, player)) {
                hostAfterLoad_ = multi;  // opened to the LAN as soon as the terrain is ready
                startSession(info, player);
            }
            return;
        }
    }

    float by = y + h - 26, bw = std::min(130.0f, (w - 16) / 3);
    if (!multi) {
        if (ui_.button(x + w / 2 - bw * 1.5f - 6, by, bw, 22, "Nový svět", true, true)) {
            newWorldName_ = "Můj svět";
            newWorldSeed_.clear();
            newWorldMode_ = GameMode::Survival;
            newWorldFlat_ = newWorldCoords_ = newWorldKeepInventory_ = newWorldCommands_ = newWorldExpLiquids_ = newWorldExpMaps_ = newWorldExpOpen_ = false;
            setScreen(Screen::CreateWorld);
        }
        // The Multiplayer tab is at the top; this place now imports a world (.vcworld).
        if (ui_.button(x + w / 2 - bw / 2, by, bw, 22, "Importovat svět", (bool)onImportWorld)) onImportWorld();
    } else {
        if (ui_.button(x + w / 2 - bw * 1.5f - 6, by, bw, 22, "Připojit kódem", true, true) && onTextPrompt)
            onTextPrompt(PromptJoinAddress, tr("Kód místnosti nebo IP adresa"), "");
        if (ui_.button(x + w / 2 - bw / 2, by, bw, 22, trf("Jméno: {0}", {playerName()})) && onTextPrompt)
            onTextPrompt(PromptPlayerName, "Jméno hráče", profile_.name);
    }
    if (ui_.button(x + w / 2 + bw / 2 + 6, by, bw, 22, "Zpět")) setScreen(Screen::Title);
}

void Game::createWorldScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Nastavení nového světa", kTitle, 1, false);
    ui_.text(x + 2, y + 14, "Název, herní režim, typ světa a seed.", kTextDim, 1, false);
    float cy = y + 28;
    const float col = std::floor(w / 2) - 4;

    // Left column: name, mode, type
    ui_.text(x, cy, "Název světa:", kText, 1, false);
    if (ui_.textBox(x, cy + 10, col, 18, newWorldName_, "Svět") && onTextPrompt)
        onTextPrompt(PromptWorldName, "Název světa", newWorldName_);
    ui_.text(x, cy + 34, "Výchozí herní režim:", kText, 1, false);
    float mw = (col - 4) / 2;
    if (ui_.button(x, cy + 44, mw, 20, "Přežití", true, newWorldMode_ == GameMode::Survival)) newWorldMode_ = GameMode::Survival;
    if (ui_.button(x + mw + 4, cy + 44, mw, 20, "Kreativní", true, newWorldMode_ == GameMode::Creative))
        newWorldMode_ = GameMode::Creative;
    ui_.text(x, cy + 72, "Typ světa:", kText, 1, false);
    if (ui_.dropdown(x, cy + 82, col, 18, newWorldFlat_ ? "Plochý" : "Nekonečný (standardní)")) newWorldFlat_ = !newWorldFlat_;

    // Right column: seed and options
    float rx = x + col + 8;
    ui_.text(rx, cy, "Seed:", kText, 1, false);
    if (ui_.textBox(rx, cy + 10, col - 22, 18, newWorldSeed_, "náhodný") && onTextPrompt)
        onTextPrompt(PromptSeed, "Seed světa", newWorldSeed_);
    if (ui_.button(rx + col - 18, cy + 10, 18, 18, "?")) {
        std::random_device rd;
        newWorldSeed_ = std::to_string((int32_t)rd());
    }
    float oy = cy + 34;
    ui_.frame(rx, oy, col, 92, kPanelInner, kSeparator);
    ui_.text(rx + 6, oy + 6, "Zobrazit souřadnice", kText, 1, false);
    ui_.text(rx + 6, oy + 16, "XYZ a biom vlevo nahoře", kTextDim, 1, false, col - 30);
    ui_.checkbox(rx + col - 18, oy + 8, newWorldCoords_);
    ui_.separator(rx + 4, oy + 30, col - 8);
    ui_.text(rx + 6, oy + 36, "Ponechat inventář", kText, 1, false);
    ui_.text(rx + 6, oy + 46, "Věci zůstanou po smrti", kTextDim, 1, false, col - 30);
    ui_.checkbox(rx + col - 18, oy + 38, newWorldKeepInventory_);
    ui_.separator(rx + 4, oy + 60, col - 8);
    ui_.text(rx + 6, oy + 66, "Povolit příkazy", kText, 1, false);
    ui_.text(rx + 6, oy + 76, "/gamemode, /tp, /give a další", kTextDim, 1, false, col - 30);
    ui_.checkbox(rx + col - 18, oy + 68, newWorldCommands_);

    // Which packs will shape the world.
    int enabled = 0;
    for (const PackInfo& p : packList_) enabled += p.enabled;
    std::string info = trf("Biomy a Nether z {0} aktivních balíčků", {std::to_string(enabled)});
    ui_.text(x, cy + 108, info, kTextDim, 1, false, w);
    // 1.5.1: Experimental Features, for everyone, saved with the world.
    if (ui_.button(x, cy + 120, col, 18, newWorldExpLiquids_ ? trf("Experimentální funkce ({0})", {std::to_string((int)newWorldExpLiquids_)}) : tr("Experimentální funkce")))
        newWorldExpOpen_ = !newWorldExpOpen_;
    if (newWorldExpOpen_) {
        const float ex = x + 8, ey = y + 24, ew = w - 16;
        ui_.frame(ex, ey, ew, 112, kPanel, kSeparator);
        ui_.text(ex + 6, ey + 6, "Experimentální funkce", kTitle, 1, false);
        ui_.text(ex + 6, ey + 18, "Rozpracovaný obsah dalších verzí. Platí jen pro tento svět a po vytvoření je nelze změnit.",
                 kTextDim, 1, false, ew - 12);
        // 1.5.7: the redesigned ores are standard now; these two are the new experiments.
        auto experiment = [&](float ry, const char* name, const char* about, bool& on) {
            ui_.frame(ex + 6, ry, ew - 12, 22, kPanelInner, kSeparator);
            ui_.text(ex + 12, ry + 3, name, kText, 1, false);
            ui_.text(ex + 12, ry + 12, about, kTextDim, 1, false, ew - 50);
            ui_.checkbox(ex + ew - 30, ry + 3, on);
        };
        experiment(ey + 36, "Tekoucí voda a láva", "Kapaliny tečou dolů a do stran; láva s vodou ztvrdne.", newWorldExpLiquids_);
        // 1.5.9: explorer maps are a standard feature now (no longer listed here).
        ui_.text(ex + 6, ey + 86, "Upozornění: experimenty se mohou změnit nebo zmizet.", Color{0.9f, 0.7f, 0.35f, 1}, 1, false, ew - 70);
        if (ui_.button(ex + ew - 66, ey + 92, 60, 16, "Hotovo")) newWorldExpOpen_ = false;
        return;  // the panel covers the screen's other buttons
    }

    float by = y + h - 26, bw = std::min(160.0f, (w - 8) / 2);
    if (ui_.button(x + w / 2 - bw - 4, by, bw, 22, "Vytvořit a hrát", true, true)) {
        WorldInfo wi;
        wi.folder = createWorldFolder(worldsDir_);
        wi.name = newWorldName_.empty() ? "Svět" : newWorldName_;
        wi.seed = seedFromText(newWorldSeed_);
        wi.mode = newWorldMode_;
        wi.lastPlayed = unixNow();
        wi.showCoordinates = newWorldCoords_;
        wi.keepInventory = newWorldKeepInventory_;
        wi.allowCommands = newWorldCommands_;
        wi.expFlowingLiquids = newWorldExpLiquids_;
        wi.expExplorationMaps = newWorldExpMaps_;
        // The world keeps its own copy of the generator, so later pack edits do not reshape it.
        GenConfig config = packs_.buildConfig();
        config.flat = newWorldFlat_;
        saveGenerator(wi.folder, config);
        startSession(wi, PlayerState{});
        return;
    }
    if (ui_.button(x + w / 2 + 4, by, bw, 22, "Zrušit")) setScreen(Screen::WorldList);
}

void Game::confirmDeleteScreen() {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0.2f, 0, 0, 0.5f});
    if (deleteIndex_ < 0 || deleteIndex_ >= (int)worlds_.size()) {
        setScreen(Screen::WorldList);
        return;
    }
    const WorldInfo& wi = worlds_[deleteIndex_];
    float pw = std::min(320.0f, W - 20), ph = 90, px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.frame(px, py, pw, ph, kPanel, kBorder);
    ui_.textCentered(W / 2, py + 12, trf("Opravdu smazat svět „{0}“?", {wi.name}), kTextBright);
    ui_.textCentered(W / 2, py + 28, "Tuto akci nelze vrátit!", kError);
    float bw = (pw - 24) / 2;
    if (ui_.button(px + 8, py + ph - 30, bw, 22, "Smazat")) {
        deleteWorld(wi.folder);
        setScreen(Screen::WorldList);
        return;
    }
    if (ui_.button(px + 16 + bw, py + ph - 30, bw, 22, "Zrušit", true, true)) setScreen(Screen::WorldList);
}

void Game::settingsScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, true);
    // 1.5.7: graphics and shaders have a tab of their own, shown after Video.
    const char* tabs[] = {"Dotyk", "Video", "Zvuk", "Profil", "Globální zdroje", "Grafika"};
    const int order[] = {0, 1, 5, 2, 3, 4};
    const float tabW = std::min(150.0f, std::floor(w * 0.3f));
    for (int i = 0; i < 6; ++i)
        if (ui_.button(x, y + i * 26, tabW, 22, tabs[order[i]], true, settingsTab_ == order[i])) settingsTab_ = order[i];

    float px = x + tabW + 8, pw = w - tabW - 8;
    ui_.frame(px, y, pw, h - 4, kPanelInner, kSeparator);
    const char* titles[] = {"Nastavení dotyku", "Nastavení videa", "Zvuk", "Profil", "Globální zdroje", "Grafika a shadery"};
    ui_.text(px + 8, y + 7, titles[settingsTab_], kTitle, 1, false);

    float ry = y + 22;
    // The video tab has the most rows; they are a little tighter there.
    const float rowH = settingsTab_ == 1 ? 23 : settingsTab_ == 5 ? 26 : 22, cx = px + pw - 8;
    float subW = pw - 120;  // room for the control on the right
    auto row = [&](const std::string& label, const std::string& sub) {
        ui_.text(px + 8, ry + (sub.empty() ? rowH / 2 - 4 : 3), label, kText, 1, false, pw - 120);
        if (!sub.empty()) ui_.text(px + 8, ry + rowH - 11, sub, kTextDim, 1, false, subW);
    };
    auto endRow = [&] {
        ry += rowH;
        ui_.separator(px + 6, ry - 1, pw - 12);
    };
    auto check = [&](const std::string& label, const std::string& sub, bool& v) {
        subW = pw - 40;  // a checkbox is narrow
        row(label, sub);
        subW = pw - 120;
        ui_.checkbox(cx - 12, ry + 5, v);
        endRow();
    };
    auto slider = [&](const std::string& label, const std::string& value, float& v) {
        row(label, "");
        ui_.text(cx - 132 - ui_.textWidth(value), ry + 7, value, kTextDim, 1, false);
        bool changed = ui_.themedSlider(cx - 124, ry + 6, 124, v);
        endRow();
        return changed;
    };

    if (settingsTab_ == 0) {
        float v = (settings_.sensitivity - 0.25f) / 2.75f;
        if (slider("Citlivost", std::to_string((int)std::round(settings_.sensitivity * 100)) + " %", v))
            settings_.sensitivity = 0.25f + v * 2.75f;
        check("Invertovat osu Y", "", settings_.invertY);
        check("Automatický skok", "Vyskočí na schod sám", settings_.autoJump);
        check("Vibrace při rozbití", "", settings_.vibrate);
        check("Pro leváky", "D-pad vpravo, skok vlevo", settings_.lefty);
        v = (settings_.buttonScale - 0.6f) / 1.0f;
        if (slider("Velikost tlačítek", std::to_string((int)std::round(settings_.buttonScale * 100)) + " %", v))
            settings_.buttonScale = 0.6f + v;
    } else if (settingsTab_ == 1) {
        row("Velikost GUI", "");
        std::string scale = settings_.guiScale == 0 ? tr("Automaticky") : std::to_string(settings_.guiScale) + "x";
        if (ui_.dropdown(cx - 100, ry + 2, 100, 18, scale)) {
            settings_.guiScale = (settings_.guiScale + 1) % 7;
            applyGuiScale();
        }
        endRow();
        float v = (settings_.brightness - 0.6f) / 0.9f;
        if (slider("Jas", std::to_string((int)std::round(settings_.brightness * 100)) + " %", v))
            settings_.brightness = 0.6f + v * 0.9f;
        v = (settings_.fov - 50) / 60.0f;
        if (slider("Zorné pole", std::to_string((int)std::round(settings_.fov)) + "°", v)) settings_.fov = std::round(50 + v * 60);
        v = (settings_.renderDistance - 2) / 10.0f;
        if (slider("Dohled", trf("{0} chunků", {std::to_string(settings_.renderDistance)}), v))
            settings_.renderDistance = 2 + (int)std::round(v * 10);
        row("Pohled kamery", "");
        const char* cams[] = {"První osoba", "Zezadu", "Zepředu"};
        if (ui_.dropdown(cx - 100, ry + 2, 100, 18, cams[settings_.cameraMode])) settings_.cameraMode = (settings_.cameraMode + 1) % 3;
        endRow();
        check("Pohupování kamery", "", settings_.viewBobbing);
        check("Mapa", "Vpravo nahoře, ťuknutím se zvětší", settings_.showMap);
    } else if (settingsTab_ == 5) {
        // 1.5.5: Classic or Enhanced graphics, and the shader pack that tunes Enhanced.
        // 1.5.7: the battery saver.
        {
            const char* modes[3] = {"Vypnuto", "Automaticky", "Vždy"};
            row("Úspora baterie", saverActive() ? tr("Zapnuto: 30 FPS, kratší dohled, bez stínů") : tr("Při 35 % a méně, když se nenabíjí"));
            if (ui_.dropdown(cx - 100, ry + 4, 100, 18, modes[settings_.batterySaver])) settings_.batterySaver = (settings_.batterySaver + 1) % 3;
            endRow();
        }
        row("Grafika", settings_.graphics == GraphicsMode::Enhanced ? tr("Vibrant shadery, živější barvy") : tr("Původní vzhled"));
        if (ui_.dropdown(cx - 100, ry + 4, 100, 18, settings_.graphics == GraphicsMode::Enhanced ? "Vylepšená" : "Klasická")) {
            settings_.graphics = settings_.graphics == GraphicsMode::Enhanced ? GraphicsMode::Classic : GraphicsMode::Enhanced;
            shadersDirty_ = true;
        }
        endRow();
        if (settings_.graphics == GraphicsMode::Enhanced) {
            subW = pw - 16;
            row("Shader pack", packs_.shaderPackDir().empty() ? tr("Žádný (Vibrant). Zapni balíček se složkou shaders") : shaderProfile_.name);
            subW = pw - 120;
            endRow();
            check("Stíny", "Slunce a měsíc vrhají stíny", settings_.shadows);
            check("Pohyb listí a vody", "Listí a tráva se vlní ve větru, voda se čeří", settings_.waves);
        } else {
            for (const std::string& line : wrapText("Stíny, pohyb listí a shader packy jsou ve Vylepšené grafice.", pw - 16)) {
                ui_.text(px + 8, ry + 6, line, kTextDim, 1, false, pw - 16);
                ry += 10;
            }
            ry += 8;
        }
        check("Mraky", "", settings_.clouds);
        check("Skrýt GUI", "Jen svět, třeba na snímky", settings_.hideGui);
    } else if (settingsTab_ == 2) {
        float v = settings_.musicVolume;
        if (slider("Hudba", std::to_string((int)std::round(v * 100)) + " %", v)) {
            settings_.musicVolume = v;
            if (onMusicVolume) onMusicVolume(v);
        }
        v = settings_.soundVolume;
        if (slider("Zvuky", std::to_string((int)std::round(v * 100)) + " %", v)) settings_.soundVolume = v;
        check("Zvuky rozhraní", "Cvaknutí tlačítek", settings_.uiSounds);
        check("Klidný režim", "Bez příšer, tišší zvuky, bez rudého záblesku", settings_.calmMode);
        for (const std::string& line : wrapText("Hudba: témata VoxelSoftworks a skladby biomů. Vlastní písně dej do složky music.", pw - 16)) {
            ui_.text(px + 8, ry + 6, line, kTextDim, 1, false, pw - 16);
            ry += 10;
        }
    } else if (settingsTab_ == 3) {
        row("Jazyk", "Language · Idioma · Язык · Sprache");
        if (ui_.dropdown(cx - 120, ry + 2, 120, 18, languageName(language()))) {
            Language next = (Language)(((int)language() + 1) % (int)Language::Count);
            setLanguage(next);
            settings_.language = languageCode(next);
            if (onLanguage) onLanguage(settings_.language);
            saveSettings(settingsPath_, settings_);
        }
        endRow();
        row("Vývojářský přístup", profile_.isDeveloper() ? trf("Přihlášen: {0}", {profile_.name}) : tr("Pro tvůrce hry, chráněno heslem"));
        if (profile_.isDeveloper()) {
            const float outW = std::max(48.0f, ui_.textWidth("Odhlásit") + 12), menuW = std::max(40.0f, ui_.textWidth("Menu") + 12);
            if (ui_.button(cx - outW - menuW - 2, ry + 1, menuW, 20, "Menu")) {
                devReturn_ = Screen::Settings;
                setScreen(Screen::Developer);
            }
            if (ui_.button(cx - outW, ry + 1, outW, 20, "Odhlásit")) {
                profile_.developer = false;
                profile_.save(profilePath_);
            }
        } else if (ui_.button(cx - 100, ry + 1, 100, 20, "Přihlásit") && onTextPrompt) {
            onTextPrompt(PromptDevName, tr("Jméno vývojáře"), profile_.name);
        }
        endRow();
        if (now_ < toastUntil_ && !toast_.empty()) ui_.text(px + pw - 8 - ui_.textWidth(toast_), y + 7, toast_, kGold, 1, false);
        row("Úspěchy", trf("{0} z {1} splněno", {std::to_string(profile_.achievements.size()), std::to_string(kAchievementCount)}));
        if (ui_.button(cx - 100, ry + 1, 100, 20, "Otevřít")) {
            achievementsReturn_ = Screen::Settings;
            setScreen(Screen::Achievements);
        }
        endRow();
        row("Jméno hráče", "");
        if (ui_.textBox(cx - 120, ry + 2, 120, 18, profile_.name, "Hráč") && onTextPrompt)
            onTextPrompt(PromptPlayerName, "Jméno hráče", profile_.name);
        endRow();
        row("Skin", skinIndex(profile_.skin) >= 0 ? skins_[skinIndex(profile_.skin)].name : "");
        if (ui_.button(cx - 100, ry + 1, 100, 20, "Změnit")) setScreen(Screen::Skins);
        endRow();
        row("Mince", "Za hraní: 1 za 10 bloků, 50 denně");
        std::string coins = std::to_string(profile_.coins);
        ui_.icon(cx - ui_.textWidth(coins) - 12, ry + 7, Font::Coin, kGold);
        ui_.text(cx - ui_.textWidth(coins), ry + 7, coins, kGold, 1, false);
        endRow();
        row("Obchod VoxelCraftu", "Skiny, textury a světy");
        if (ui_.button(cx - 100, ry + 1, 100, 20, "Otevřít")) setScreen(Screen::Store);
        endRow();
    } else {
        row("Módy a balíčky", "Bloky, předměty, moby, biomy, recepty");
        if (ui_.button(cx - 100, ry + 1, 100, 20, "Spravovat")) setScreen(Screen::Packs);
        endRow();
        row("Balíček textur", "terrain.png, .zip, .mcpack");
        if (ui_.button(cx - 100, ry + 1, 100, 20, "Vybrat...") && onTexturePackButton) onTexturePackButton();
        endRow();
        int active = 0;
        for (const PackInfo& p : packList_) active += p.enabled;
        row("Aktivní balíčky", "");
        std::string count = std::to_string(active);
        ui_.text(cx - ui_.textWidth(count), ry + 7, count, kTextBright, 1, false);
        endRow();
    }

    ui_.text(px + 8, y + h - 18, std::string(kVersion), kTextDim, 1, false);
    if (ui_.button(px + pw - 68, y + h - 26, 60, 20, "Zpět")) {
        saveSettings(settingsPath_, settings_);
        setScreen(settingsReturn_);
    }
}

void Game::packsScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Módy a balíčky", kTitle, 1, false);
    ui_.text(x + 2, y + 14, "Zapnuté balíčky se použijí odshora dolů; pozdější přepisují dřívější.", kTextDim, 1, false, w);

    const float listY = y + 28, listH = h - 28 - 32, listW = std::floor(w * 0.45f), rowH = 22;
    ui_.frame(x, listY, listW, listH, kPanelInner, kSeparator);
    listScroll_ -= ui_.dragY(x, listY, listW, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, packList_.size() * rowH - (listH - 4)));
    for (size_t i = 0; i < packList_.size(); ++i) {
        float ry = listY + 2 + i * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        PackInfo& p = packList_[i];
        bool sel = (int)i == packSelected_;
        ui_.frame(x + 2, ry, listW - 4, rowH - 2, sel ? kGreen : kButton, sel ? kGreenBorder : kSeparator);
        bool on = p.enabled;
        if (ui_.checkbox(x + 6, ry + 4, on)) {
            packs_.setEnabled(p.id, on);
            rebuildFromPacks();
            return;
        }
        ui_.text(x + 24, ry + 6, p.name, sel ? kTextBright : kText, 1, false, listW - 32);
        if (ui_.tapped(x + 22, ry, listW - 26, rowH - 2)) packSelected_ = (int)i;
    }

    // Details of the selected pack
    float dx = x + listW + 8, dw = w - listW - 8;
    ui_.frame(dx, listY, dw, listH, kPanelInner, kSeparator);
    if (!packList_.empty()) {
        PackInfo& p = packList_[packSelected_];
        float ty = listY + 6;
        ui_.text(dx + 6, ty, p.name, kTextBright, 1, false, dw - 12);
        ty += 12;
        ui_.text(dx + 6, ty, tr(p.builtin ? "Vestavěný" : "Doplněk") + (p.version.empty() ? std::string() : trf(" · verze {0}", {p.version})),
                 kTextDim, 1, false, dw - 12);
        ty += 12;
        // What the mod adds, and for a Java import what came across and what did not.
        const ModRegistry::Summary sum = ModRegistry::get().summary(p.dir);
        if (sum.blocks + sum.items + sum.mobs > 0) {
            ui_.text(dx + 6, ty, trf("Bloky: {0} · předměty: {1} · moby: {2}",
                                     {std::to_string(sum.blocks), std::to_string(sum.items), std::to_string(sum.mobs)}),
                     kAccent, 1, false, dw - 12);
            ty += 12;
        }
        for (const std::string& line : readImportReport(p.dir)) {
            const size_t eq = line.find('=');
            if (eq == std::string::npos || ty > listY + listH - 84) continue;
            const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            std::string shown;
            if (k == "skipped_minecraft" && v != "0") shown = trf("Přeskočeno ze hry Minecraft: {0} souborů", {v});
            else if (k == "skipped_shapes" && v != "0") shown = trf("Bloky jiného tvaru než kostka: {0}", {v});
            else if (k == "skipped_recipes" && v != "0") shown = trf("Recepty se štítky nebo cizími věcmi: {0}", {v});
            else if (k == "classes" && v != "0") shown = trf("Kód v Javě se nespouští ({0} tříd)", {v});
            if (shown.empty()) continue;
            ui_.text(dx + 6, ty, shown, kTextDim, 1, false, dw - 12);
            ty += 10;
        }
        for (const std::string& line : wrapText(p.description, dw - 12)) {
            if (ty > listY + listH - 74) break;
            ui_.text(dx + 6, ty, line, kText, 1, false, dw - 12);
            ty += 10;
        }
        float by = listY + listH - 70, bw = dw - 12;
        if (ui_.button(dx + 6, by, bw, 20, "Upravit soubory JSON", true, true)) {
            filesPack_ = p.id;
            packFiles_ = packs_.files(p.id);
            setScreen(Screen::PackFiles);
            return;
        }
        float hw = (bw - 4) / 2;
        if (ui_.button(dx + 6, by + 22, hw, 20, "Výš", p.enabled)) {
            packs_.move(p.id, -1);
            rebuildFromPacks();
            packSelected_ = std::max(0, packSelected_ - 1);
        }
        if (ui_.button(dx + 10 + hw, by + 22, hw, 20, "Níž", p.enabled)) {
            packs_.move(p.id, 1);
            rebuildFromPacks();
            packSelected_ = std::min((int)packList_.size() - 1, packSelected_ + 1);
        }
        if (p.builtin) {
            if (ui_.button(dx + 6, by + 44, bw, 20, "Obnovit výchozí obsah")) {
                packs_.resetDefaultPack();
                rebuildFromPacks();
            }
        } else if (ui_.button(dx + 6, by + 44, bw, 20, "Smazat balíček")) {
            packs_.deletePack(p.id);
            rebuildFromPacks();
        }
    }

    // Problems found in the pack files
    std::string status = packWarnings_.empty() ? "Všechny soubory jsou v pořádku"
                                               : trf("{0} problém(ů): {1}", {std::to_string(packWarnings_.size()), packWarnings_[0]});
    ui_.text(x + 2, y + h - 40, status, packWarnings_.empty() ? kAccent : kError, 1, false, w - 4);

    float by = y + h - 26, bw = std::floor((w - 24) / 4);
    if (ui_.button(x, by, bw, 22, "Importovat .zip") && onImportPack) onImportPack();
    if (ui_.button(x + bw + 8, by, bw, 22, "Nový balíček") && onTextPrompt)
        onTextPrompt(PromptPackName, "Název nového balíčku", "Můj balíček");
    if (ui_.button(x + 2 * (bw + 8), by, bw, 22, "Nový shader pack") && onTextPrompt)
        onTextPrompt(PromptShaderPackName, "Název shader packu", "Můj shader");
    if (ui_.button(x + 3 * (bw + 8), by, bw, 22, "Zpět")) setScreen(Screen::Title);
}

void Game::packFilesScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    std::string name = filesPack_;
    for (const PackInfo& p : packList_)
        if (p.id == filesPack_) name = p.name;
    ui_.text(x + 2, y + 2, name, kTitle, 1, false, w);
    ui_.text(x + 2, y + 14, "packs/" + filesPack_ + "/", kAccent, 1, false, w);

    const float listY = y + 28, listH = h - 28 - 32, rowH = 18;
    ui_.frame(x, listY, w, listH, kField, kSeparator);
    listScroll_ -= ui_.dragY(x, listY, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, packFiles_.size() * rowH - (listH - 4)));
    for (size_t i = 0; i < packFiles_.size(); ++i) {
        float ry = listY + 2 + i * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        const std::string& f = packFiles_[i];
        bool down = ui_.held(x + 2, ry, w - 4, rowH - 1);
        if (down) ui_.fill(x + 2, ry, w - 4, rowH - 1, kButton);
        // Folder colour hints, like the file browser mock-up.
        Color c = f.rfind("biomes/", 0) == 0 ? kAccent : f.rfind("dimensions/", 0) == 0 ? kGold
                : f.rfind("recipes/", 0) == 0 ? Color{0.45f, 0.65f, 0.75f, 1} : kText;
        ui_.fill(x + 6, ry + 5, 6, 7, c);
        ui_.text(x + 18, ry + 5, f, kText, 1, false, w - 24);
        if (ui_.tapped(x + 2, ry, w - 4, rowH - 1)) {
            openEditor(filesPack_, f);
            return;
        }
    }

    float by = y + h - 26, bw = std::min(160.0f, (w - 8) / 2);
    if (ui_.button(x + w / 2 - bw - 4, by, bw, 22, "Nový biom")) {
        std::string path;
        for (int n = 1;; ++n) {
            path = "biomes/novy_biom_" + std::to_string(n) + ".json";
            if (std::find(packFiles_.begin(), packFiles_.end(), path) == packFiles_.end()) break;
        }
        std::string id = path.substr(7, path.size() - 12);
        packs_.writeFile(filesPack_, path,
                         "{\n  \"id\": \"" + id + "\",\n  \"name\": \"Nový biom\",\n"
                         "  \"climate\": {\"temperature\": 0.3, \"humidity\": 0.2},\n"
                         "  \"terrain\": {\"height\": 70, \"variation\": 12},\n"
                         "  \"blocks\": {\"surface\": \"grass\", \"filler\": \"dirt\", \"filler_depth\": 3, \"beach\": \"sand\", \"underwater\": \"gravel\"},\n"
                         "  \"trees\": {\"density\": 0.02, \"trunk\": \"log\", \"leaves\": \"leaves\"}\n}\n");
        packFiles_ = packs_.files(filesPack_);
        rebuildFromPacks();
        openEditor(filesPack_, path);
        return;
    }
    if (ui_.button(x + w / 2 + 4, by, bw, 22, "Zpět")) setScreen(Screen::Packs);
}

void Game::openEditor(const std::string& pack, const std::string& path) {
    editorPack_ = pack;
    editorPath_ = path;
    editorScroll_ = 0;
    editorApply(packs_.readFile(pack, path), false);
    editorSaved_ = true;
    setScreen(Screen::JsonEditor);
}

void Game::editorApply(const std::string& text, bool save) {
    editorText_ = text;
    Json j;
    std::string err;
    editorValid_ = Json::parse(text, j, err);
    if (!editorValid_) {
        editorStatus_ = trf("Chyba: {0}", {err});
        editorSaved_ = !save;
        return;
    }
    editorStatus_ = "Platný JSON";
    if (save) {
        editorSaved_ = packs_.writeFile(editorPack_, editorPath_, text);
        editorStatus_ = editorSaved_ ? "Uloženo · platný JSON" : "Soubor se nepodařilo uložit";
        rebuildFromPacks();
    }
}

void Game::jsonEditorScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, editorPath_, kTitle, 1, false, w);
    ui_.text(x + 2, y + 14, "packs/" + editorPack_ + "/", kTextDim, 1, false, w);

    // Toolbar
    float tx = x, ty = y + 26;
    if (ui_.button(tx, ty, 80, 20, "Upravit", true, true) && onEditText) onEditText(PromptJson, editorPath_, editorText_);
    if (ui_.button(tx + 84, ty, 70, 20, "Ověřit")) editorApply(editorText_, false);
    if (ui_.button(tx + 158, ty, 90, 20, "Formátovat", editorValid_)) {
        Json j;
        std::string err;
        if (Json::parse(editorText_, j, err)) editorApply(j.dump() + "\n", true);
    }
    ui_.text(tx + 256, ty + 6, editorStatus_, editorValid_ ? kAccent : kError, 1, false, w - 260);

    // Code view with line numbers
    const float codeY = ty + 26, codeH = h - (codeY - y) - 32, lineH = 10;
    ui_.frame(x, codeY, w, codeH, kField, kSeparator);
    std::vector<std::string> lines;
    {
        std::string line;
        for (char c : editorText_) {
            if (c == '\n') { lines.push_back(line); line.clear(); }
            else if (c == '\t') line += "  ";
            else if (c != '\r') line += c;
        }
        if (!line.empty()) lines.push_back(line);
    }
    editorScroll_ -= ui_.dragY(x, codeY, w, codeH);
    editorScroll_ = std::clamp(editorScroll_, 0.0f, std::max(0.0f, lines.size() * lineH - (codeH - 8)));
    const float gutter = 26;
    ui_.fill(x + 1, codeY + 1, gutter, codeH - 2, {0.12f, 0.105f, 0.09f, 1});
    for (size_t i = 0; i < lines.size(); ++i) {
        float ly = codeY + 4 + i * lineH - editorScroll_;
        if (ly < codeY + 2 || ly + 8 > codeY + codeH - 2) continue;
        std::string num = std::to_string(i + 1);
        ui_.text(x + gutter - 3 - ui_.textWidth(num), ly, num, kTextDim, 1, false);
        ui_.text(x + gutter + 4, ly, lines[i], kCode, 1, false, w - gutter - 8);
    }

    std::string info = trf("{0} znaků · {1} řádků", {std::to_string(editorText_.size()), std::to_string(lines.size())});
    ui_.text(x + 2, y + h - 18, info, kTextDim, 1, false);
    if (!editorSaved_) ui_.text(x + 140, y + h - 18, "Neuloženo, oprav chybu", kError, 1, false);
    if (ui_.button(x + w - 70, y + h - 26, 70, 22, "Zpět")) {
        packFiles_ = packs_.files(editorPack_);
        setScreen(Screen::PackFiles);
    }
}

void Game::pauseScreen() {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.45f});
    const bool creative = !session_->survival(), developer = profile_.isDeveloper();
    const bool client = lan_.role() == Multiplayer::Role::Client, hosting = lan_.role() == Multiplayer::Role::Host;
    const int buttons = 3 + (creative ? 1 : 0) + (developer ? 1 : 0) + (client ? 0 : 1);
    float pw = std::min(240.0f, W - 20), ph = 28 + buttons * 28, px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.fill(px + 2, py + 2, pw, ph, {0, 0, 0, 0.5f});
    ui_.frame(px, py, pw, ph, kPanel, kBorder);
    ui_.textCentered(W / 2, py + 8, "Hra pozastavena", kTitle);
    float bx = px + 8, bw = pw - 16, by = py + 24;
    if (ui_.button(bx, by, bw, 22, "Pokračovat ve hře", true, true)) setScreen(Screen::Playing);
    by += 28;
    if (ui_.button(bx, by, bw, 22, "Nastavení")) {
        settingsReturn_ = Screen::Pause;
        setScreen(Screen::Settings);
    }
    by += 28;
    if (creative) {
        if (ui_.button(bx, by, bw, 22, "Struktury")) setScreen(Screen::Structures);
        by += 28;
    }
    if (developer) {
        if (ui_.button(bx, by, bw, 22, "Vývojář")) {
            devReturn_ = Screen::Pause;
            setScreen(Screen::Developer);
        }
        by += 28;
    }
    if (hosting) {
        // Where others connect: the room code of the best address; tap for all codes and the rules.
        std::vector<net::LocalAddress> addresses = net::localInterfaces();
        std::string code = addresses.empty() ? std::string("?") : net::roomCode(addresses[0].ip, lan_.port());
        bool down = ui_.held(bx, by, bw, 22);
        ui_.frame(bx, by, bw, 22, down ? kButtonDown : kGreen, kGreenBorder);
        ui_.textCentered(bx + bw / 2, by + 2, trf("Pozvat hráče · hráčů: {0}", {std::to_string(lan_.playerCount())}), kTextBright);
        ui_.textCentered(bx + bw / 2, by + 12, code, {0.85f, 0.95f, 0.8f, 1});
        if (ui_.tapped(bx, by, bw, 22)) setScreen(Screen::Invite);
        by += 28;
    } else if (!client) {
        if (ui_.button(bx, by, bw, 22, "Otevřít pro LAN")) openToLan();
        by += 28;
    }
    if (lan_.active()) {
        // Who is playing: you and everyone connected.
        const float lw = 120, lx = px + pw + 8;
        if (lx + lw <= W - 4) {
            const auto& others = lan_.players();
            const float lh = 22 + 11 * (1 + others.size());
            ui_.fill(lx + 2, py + 2, lw, lh, {0, 0, 0, 0.5f});
            ui_.frame(lx, py, lw, lh, kPanel, kGreenBorder);
            ui_.text(lx + 6, py + 6, trf("Hráči ({0})", {std::to_string(lan_.playerCount())}), kTitle, 1, false);
            ui_.text(lx + 6, py + 19, hosting ? trf("{0} (hostitel)", {playerName()}) : playerName(), kGold, 1, false, lw - 12);
            for (size_t i = 0; i < others.size(); ++i)
                ui_.text(lx + 6, py + 30 + i * 11, others[i].name, kTextBright, 1, false, lw - 12);
        }
    }
    if (ui_.button(bx, by, bw, 22, client ? "Odpojit se" : "Uložit a odejít")) {
        endSession();
        setScreen(client ? Screen::WorldList : Screen::Title);
    }
}

void Game::tutorial(float dt) {
    // First steps for new players, one hint at a time, in survival worlds.
    if (settings_.tutorialStep >= kTutorialSteps || !session_->survival()) return;
    static const char* const kSteps[kTutorialSteps] = {
        "Táhni prstem po obrazovce a rozhlédni se.",
        "Choď šipkami vlevo dole.",
        "Podrž prst na bloku a vytěžíš ho.",
        "Krátce ťukni na blok a postavíš další.",
        "Otevři inventář tlačítkem … vedle lišty.",
        "Z kmene vyrob prkna a z nich pracovní stůl.",
    };
    const Player& p = session_->player();
    tutorialLook_ += std::fabs(std::remainder(p.yaw - tutorialYaw_, 2 * kPi));
    tutorialYaw_ = p.yaw;
    tutorialWalk_ += p.walked;
    (void)dt;
    bool done = false;
    switch (settings_.tutorialStep) {
        case 0: done = tutorialLook_ > 1.5f; break;
        case 1: done = tutorialWalk_ > 5; break;
        case 2: done = tutorialMined_ > 0; break;
        case 3: done = session_->blocksPlaced > 0; break;
        case 4: done = screen_ == Screen::Inventory; break;
        case 5: done = countInInventory(session_->inventory(), CRAFTING_TABLE) > 0; break;
        default: break;
    }
    if (done) {
        ++settings_.tutorialStep;
        saveSettings(settingsPath_, settings_);
        playSound(Sound::Click);
        if (settings_.tutorialStep >= kTutorialSteps) addChat(tr("Tutoriál je hotový. Hodně štěstí!"));
        return;
    }
    if (screen_ != Screen::Playing) return;
    const float W = ui_.width();
    const std::string line = trf("Tip {0}/{1}: {2}", {std::to_string(settings_.tutorialStep + 1), std::to_string(kTutorialSteps),
                                                       tr(kSteps[settings_.tutorialStep])});
    const float tw = ui_.textWidth(line) + 70, tx = std::floor((W - tw) / 2), ty = 30;
    ui_.fill(tx + 2, ty + 2, tw, 18, {0, 0, 0, 0.5f});
    ui_.frame(tx, ty, tw, 18, kPanel, kGreenBorder);
    ui_.text(tx + 6, ty + 5, line, kTextBright, 1, false);
    if (ui_.button(tx + tw - 58, ty + 2, 54, 14, "Přeskočit")) {
        settings_.tutorialStep = kTutorialSteps;
        saveSettings(settingsPath_, settings_);
    }
}

void Game::inviteScreen() {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.55f});
    const float pw = std::min(W - 16, 420.0f), ph = std::min(H - 12, 256.0f);
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.fill(px + 2, py + 2, pw, ph, {0, 0, 0, 0.5f});
    ui_.frame(px, py, pw, ph, kPanel, kBorder);
    ui_.textCentered(W / 2, py + 7, "Pozvat hráče", kTitle);
    // One room code per way in: the phone's own hotspot, the Wi-Fi, the internet.
    float y = py + 22;
    const std::vector<net::LocalAddress> addresses = net::localInterfaces();
    if (addresses.empty()) {
        ui_.text(px + 10, y, "Zapni Wi-Fi nebo osobní hotspot.", kAccent, 1, false, pw - 20);
        y += 14;
    }
    for (size_t i = 0; i < addresses.size() && i < 3; ++i) {
        const net::LocalAddress& a = addresses[i];
        const char* label = a.kind == net::AddressKind::Hotspot ? "Hotspot (bez Wi-Fi a bez dat)"
                            : a.kind == net::AddressKind::Wifi ? "Wi-Fi / LAN" : "Internet";
        ui_.frame(px + 8, y, pw - 16, 24, kPanelInner, a.kind == net::AddressKind::Hotspot ? kGreenBorder : kSeparator);
        ui_.text(px + 14, y + 3, label, kTextDim, 1, false, pw / 2);
        ui_.text(px + 14, y + 13, a.ip + ":" + std::to_string(lan_.port()), kTextDim, 1, false, pw / 2);
        const std::string code = net::roomCode(a.ip, lan_.port());
        ui_.text(px + pw - 14 - ui_.textWidth(code) * 2, y + 4, code, kGold, 2, true);
        y += 28;
    }
    if (!internetCode_.empty()) {
        ui_.frame(px + 8, y, pw - 16, 24, kPanelInner, kSeparator);
        ui_.text(px + 14, y + 8, "Internet", kTextDim, 1, false, pw / 2);
        ui_.text(px + pw - 14 - ui_.textWidth(internetCode_) * 2, y + 4, internetCode_, kGold, 2, true);
        y += 28;
    }
    // The host's rules.
    ui_.checkbox(px + 10, y + 2, settings_.mpPvp);
    ui_.text(px + 28, y + 4, "PvP (souboje hráčů)", kTextBright, 1, false, pw / 2 - 30);
    ui_.checkbox(px + pw / 2 + 4, y + 2, settings_.mpProtectSpawn);
    ui_.text(px + pw / 2 + 22, y + 4, "Chránit okolí spawnu", kTextBright, 1, false, pw / 2 - 30);
    y += 20;
    for (const std::string& line :
         wrapText(trf("Bez internetu: zapni osobní hotspot (data mohou být vypnutá), kamarád se k němu připojí a zadá kód. Přes internet: na routeru přesměruj port {0}.",
                      {std::to_string(lan_.port())}),
                  pw - 20)) {
        if (y > py + ph - 40) break;
        ui_.text(px + 10, y, line, kTextDim, 1, false);
        y += 10;
    }
    const float bw = (pw - 24) / 2, by = py + ph - 28;
    if (ui_.button(px + 8, by, bw, 22, "Kód pro internet") && onTextPrompt)
        onTextPrompt(PromptPublicIp, tr("Veřejná IP adresa (zjistíš ji na routeru)"), "");
    if (ui_.button(px + 16 + bw, by, bw, 22, "Zpět", true, true)) {
        saveSettings(settingsPath_, settings_);
        setScreen(Screen::Pause);
    }
}

void Game::offlineGuideScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, true);
    ui_.textCentered(x + w / 2, y + 2, "Hra bez internetu", kTitle);
    float ty = y + 20;
    const char* steps[] = {
        "1. Hostitel zapne na telefonu osobní hotspot. Mobilní data nejsou potřeba.",
        "2. Kamarád se v nastavení Wi-Fi připojí k tomuto hotspotu.",
        "3. Hostitel v Multiplayeru ťukne na svůj svět a v pauze na Pozvat hráče.",
        "4. Kamarád ťukne na Připojit kódem a opíše kód místnosti (nebo hru najde v seznamu).",
    };
    for (const char* step : steps)
        for (const std::string& line : wrapText(step, w - 16)) {
            ui_.text(x + 8, ty, line, kTextBright, 1, false);
            ty += 11;
        }
    ty += 4;
    for (const std::string& line : wrapText("Telefony spolu mluví přímo přes hotspot, takže hra nespotřebuje žádná data.", w - 16)) {
        ui_.text(x + 8, ty, line, kAccent, 1, false);
        ty += 11;
    }
    const float bw = std::min(150.0f, (w - 24) / 2), by = y + h - 26;
    if (ui_.button(x + w / 2 - bw - 4, by, bw, 22, "Připojit kódem", true, true) && onTextPrompt)
        onTextPrompt(PromptJoinAddress, tr("Kód místnosti nebo IP adresa"), "");
    if (ui_.button(x + w / 2 + 4, by, bw, 22, "Zpět")) setScreen(Screen::WorldList);
}

// The story of the forgotten powerstone kingdom, one page per old document.
const char* const kLorePages[kDocumentPages][2] = {
    {"Kronika powerstonového lidu",
     "Jsme lid powerstonu. Kámen, který svítí a dává sílu, jsme našli v suché zemi na jihu. Postavili jsme z něj domy, cesty "
     "i lampy, které nikdy nezhasnou. Naše vesnice jsou první, které noc nevidí."},
    {"Den ve vesnici",
     "Ráno se lampy ztlumí a kováři rozdělí powerstonový prach. Děti se učí kreslit kosočtverec s tečkou, znamení našeho "
     "království. Večer se všichni sejdou u sloupu na náměstí, kde svítí největší lampa."},
    {"Lampy a stroje",
     "Mistři zjistili, že powerstonový blok rozsvítí lampu přes kabel i na druhém konci vesnice. Postavili jsme vypínače a "
     "dveře, které se otvírají samy. Říká se, že síla kamene je větší, než si myslíme."},
    {"Brána do ohně",
     "Pod zemí jsme našli úhelný kámen z jiného světa. Učenci chtějí postavit bránu z obsidiánu a powerstonu, aby se mezi "
     "světy dalo cestovat bezpečně. Rada souhlasila. Stavíme první portál."},
    {"Portály",
     "Portály stojí na třech místech. Za nimi leží svět úhelného kamene, horký a rudý. Postavili jsme tam pevnosti z "
     "netherových cihel. Každý strážce brány dostal kompas, který k pevnostem vede."},
    {"Den, kdy se to pokazilo",
     "Brána u velkého jezera se rozzářila a pak praskla. Země se otřásla, voda zaplavila dolní čtvrti a lampy začaly "
     "blikat. Ti, kdo stáli u portálu, se už nevrátili stejní."},
    {"Huskingové",
     "V noci chodí mezi domy. Nemají ruce, hlavu nesou nakloněnou a z úst jim šlehá oheň. Někteří tvrdí, že v nich "
     "poznali své sousedy. Nikdo neví, odkud se vzali. Zabednili jsme dveře."},
    {"Poslední zápis",
     "Odcházíme. Necháváme tu domy, lampy i kroniku. Kdo ji najde, ať ví, že síla kamene není hračka. Portály jsou "
     "rozbité, ale pevnosti ve světě úhelného kamene stojí dál. Snad jednou někdo zjistí, co se s námi stalo."},
};

void Game::documentScreen() {
    // An old sheet of parchment: the page's title, its text, and how many pages were found.
    const int page = std::clamp(documentPage_, 0, kDocumentPages - 1);
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.5f});
    const float pw = std::min(W - 20, 340.0f), ph = std::min(H - 16, 200.0f);
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    const Color ink{0.28f, 0.18f, 0.1f, 1}, paper{0.89f, 0.83f, 0.66f, 1}, edge{0.55f, 0.42f, 0.26f, 1};
    ui_.fill(px + 3, py + 3, pw, ph, {0, 0, 0, 0.4f});
    ui_.fill(px, py, pw, ph, edge);
    ui_.fill(px + 2, py + 2, pw - 4, ph - 4, paper);
    ui_.textCentered(px + pw / 2, py + 8, itemName((ItemId)(OLD_DOCUMENT + page)), {0.5f, 0.32f, 0.16f, 1});
    ui_.textCentered(px + pw / 2, py + 22, kLorePages[page][0], ink);
    float ty = py + 40;
    for (const std::string& line : wrapText(kLorePages[page][1], pw - 24)) {
        ui_.text(px + 12, ty, line, ink, 1, false);
        ty += 11;
    }
    int read = 0;
    for (int k = 0; k < kDocumentPages; ++k) read += (session_->stats().documentsRead >> k) & 1;
    ui_.text(px + 12, py + ph - 20, trf("Nalezené stránky: {0} z {1}", {std::to_string(read), std::to_string(kDocumentPages)}),
             {0.5f, 0.32f, 0.16f, 1}, 1, false);
    if (ui_.button(px + pw - 82, py + ph - 26, 74, 22, "Zavřít", true, true)) setScreen(Screen::Playing);
}

void Game::developerScreen() {
    if (!profile_.isDeveloper()) {
        setScreen(devReturn_);
        return;
    }
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.55f});
    const float pw = std::min(W - 16, 460.0f), ph = std::min(H - 12, 250.0f);
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.fill(px + 2, py + 2, pw, ph, {0, 0, 0, 0.5f});
    ui_.frame(px, py, pw, ph, kPanel, kBorder);
    ui_.text(px + 8, py + 6, "Vývojářské menu", kTitle, 1, false);
    ui_.text(px + 8 + ui_.textWidth("Vývojářské menu") + 10, py + 6, profile_.name, kGold, 1, false);
    if (ui_.button(px + pw - 22, py + 3, 18, 16, "X")) {
        setScreen(devReturn_);
        return;
    }
    const float colW = std::floor((pw - 24) / 2), lx = px + 8, rx = lx + colW + 8;
    float ly = py + 24, ryy = py + 24;
    bool changed = false;
    auto toggle = [&](float x, float& y, const char* label, bool& v) {
        ui_.text(x + 18, y + 3, label, kText, 1, false, colW - 20);
        changed |= ui_.checkbox(x, y + 1, v);
        y += 17;
    };
    // Modes
    ui_.text(lx, ly, "Režimy", kTextBright, 1, false);
    ly += 13;
    toggle(lx, ly, "Nesmrtelnost", settings_.devGodMode);
    toggle(lx, ly, "Létání i v přežití", settings_.devFly);
    toggle(lx, ly, "Okamžité těžení", settings_.devInstantMine);
    toggle(lx, ly, "Průchod bloky při létání", settings_.devNoclip);
    toggle(lx, ly, "Ladicí přehled (FPS, chunky)", settings_.devDebug);
    ui_.text(lx, ly + 3, "Rychlost", kText, 1, false);
    if (ui_.dropdown(lx + 70, ly, 60, 15, "x" + std::to_string((int)settings_.devSpeed))) {
        settings_.devSpeed = settings_.devSpeed >= 4 ? 1 : settings_.devSpeed + 1;
        changed = true;
    }
    ly += 20;
    // Experiments
    ui_.text(rx, ryy, "Experimenty", kTextBright, 1, false);
    ryy += 13;
    toggle(rx, ryy, "Rychlý čas (den za minutu)", settings_.expDayNight);
    toggle(rx, ryy, "Pece 4x rychlejší", settings_.expFastFurnace);
    toggle(rx, ryy, "Rudy na mapě", settings_.expOreMap);
    toggle(rx, ryy, "Rychlý růst sazenic", settings_.expFastGrowth);
    toggle(rx, ryy, "Nízká gravitace", settings_.expLowGravity);
    toggle(rx, ryy, "Velké výbuchy", settings_.expBigBoom);
    if (changed) saveSettings(settingsPath_, settings_);
    // Actions in the open world
    float by = py + ph - 52;
    ui_.separator(px + 6, by - 6, pw - 12);
    const float bw = std::floor((pw - 16 - 12) / 4);
    const bool inWorld = session_ != nullptr;
    if (ui_.button(lx, by, bw, 20, inWorld && session_->survival() ? "Na Kreativní" : "Na Přežití", inWorld)) {
        session_->setMode(session_->survival() ? GameMode::Creative : GameMode::Survival);
        toast_ = tr("Herní režim změněn");
        toastUntil_ = now_ + 3;
    }
    if (ui_.button(lx + bw + 4, by, bw, 20, "Uzdravit", inWorld)) session_->healAndFeed();
    if (ui_.button(lx + 2 * (bw + 4), by, bw, 20, "Na spawn", inWorld)) {
        session_->teleportToSpawn();
        setScreen(Screen::Playing);
        return;
    }
    if (ui_.button(lx + 3 * (bw + 4), by, bw, 20, "Výbava", inWorld)) {
        // Ruby tools and a full ruby armour, for testing.
        for (ItemId id : {RUBY_PICKAXE, RUBY_AXE, RUBY_SHOVEL}) session_->giveItem({id, 1, 0});
        for (int piece = 0; piece < 4; ++piece) session_->armor()[piece] = {armorItem(4, piece), 1, 0};
        toast_ = tr("Rubínová výbava přidána");
        toastUntil_ = now_ + 3;
    }
    by += 24;
    if (ui_.button(lx, by, bw * 2 + 4, 20, "Všechny bloky do inventáře", inWorld)) {
        Inventory& inv = session_->inventory();
        for (int i = 0; i < kInventorySize && i < kCreativeItemCount; ++i)
            inv.slots[i] = {kCreativeItems[i], (uint8_t)maxStack(kCreativeItems[i]), 0};
    }
    if (ui_.button(lx + 2 * (bw + 4), by, bw * 2 + 4, 20, "Zpět")) {
        setScreen(devReturn_);
        return;
    }
    if (now_ < toastUntil_ && !toast_.empty()) ui_.textCentered(W / 2, py + ph + 3, toast_);
}

void Game::refreshStructures() {
    structures_.clear();
    structureErrors_.clear();
    for (Structure& b : builtinStructures()) {
        StructureEntry e;
        e.data = std::move(b);
        e.source = "vestavěná";
        structures_.push_back(std::move(e));
    }
    auto scan = [this](const std::string& dir, const std::string& source) {
        std::error_code ec;
        std::vector<std::string> files;
        for (const auto& f : std::filesystem::directory_iterator(dir, ec)) {
            std::string ext = f.path().extension().string();
            if (!f.is_regular_file() || (ext != ".vcstructure" && ext != ".mcstructure")) continue;
            if (f.file_size(ec) > 16u << 20) {
                structureErrors_.push_back(f.path().filename().string() + ": " + tr("soubor je příliš velký"));
                continue;
            }
            files.push_back(f.path().string());
        }
        std::sort(files.begin(), files.end());
        for (const std::string& path : files) {
            StructureEntry e;
            std::string err;
            if (!loadStructureFile(path, e.data, e.report, err)) {
                structureErrors_.push_back(path.substr(path.find_last_of('/') + 1) + ": " + tr(err));
                continue;
            }
            e.source = source;
            e.file = path;
            structures_.push_back(std::move(e));
        }
    };
    scan(structuresDir_, "importovaná");
    for (const PackInfo& p : packs_.list())
        if (p.enabled) scan(packs_.packDir(p.id) + "/structures", "balíček " + p.name);
    structureSelected_ = std::clamp(structureSelected_, 0, (int)structures_.size() - 1);
}

void Game::structuresScreen() {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.45f});
    const float pw = std::min(420.0f, W - 16), ph = std::min(250.0f, H - 12);
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.fill(px + 2, py + 2, pw, ph, {0, 0, 0, 0.5f});
    ui_.frame(px, py, pw, ph, kPanel, kBorder);
    ui_.text(px + 8, py + 6, "Struktury", kTitle, 1, false);
    const float titleW = ui_.textWidth("Struktury") + 12;
    ui_.text(px + 8 + titleW, py + 6, "VCStructures · .vcstructure / .mcstructure", kTextDim, 1, false, pw - titleW - 40);
    if (ui_.button(px + pw - 22, py + 3, 18, 16, "X")) {
        setScreen(Screen::Pause);
        return;
    }

    // List on the left.
    const float listX = px + 6, listY = py + 22, listW = std::floor(pw * 0.42f), listH = ph - 22 - 54, rowH = 20;
    ui_.frame(listX, listY, listW, listH, kPanelInner, kSeparator);
    listScroll_ -= ui_.dragY(listX, listY, listW, listH);
    const int rows = (int)structures_.size() + (int)structureErrors_.size();
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, rows * rowH - (listH - 4)));
    for (int i = 0; i < rows; ++i) {
        float ry = listY + 2 + i * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        if (i >= (int)structures_.size()) {
            ui_.text(listX + 6, ry + 5, structureErrors_[i - structures_.size()], {1, 0.45f, 0.4f, 1}, 1, false, listW - 12);
            continue;
        }
        bool sel = i == structureSelected_;
        ui_.frame(listX + 2, ry, listW - 4, rowH - 2, sel ? kGreen : kButton, sel ? kGreenBorder : kSeparator);
        ui_.text(listX + 6, ry + 5, structures_[i].data.name, sel ? kTextBright : kText, 1, false, listW - 12);
        if (ui_.tapped(listX + 2, ry, listW - 4, rowH - 2)) {
            structureSelected_ = i;
            structureStatus_.clear();
        }
    }

    // Details, preview and actions on the right.
    const float dx = listX + listW + 6, dw = px + pw - 6 - dx;
    ui_.frame(dx, listY, dw, listH, kPanelInner, kSeparator);
    if (!structures_.empty()) {
        const StructureEntry& e = structures_[structureSelected_];
        Structure s = structureMirror_ ? e.data.mirroredX() : e.data;
        s = s.rotated(structureRotation_);
        float ty = listY + 5;
        ui_.text(dx + 6, ty, e.data.name, kTextBright, 1, false, dw - 12);
        ty += 11;
        ui_.text(dx + 6, ty, std::to_string(s.sx) + "x" + std::to_string(s.sy) + "x" + std::to_string(s.sz) + " · " +
                     trf("{0} bloků", {std::to_string(s.blockCount())}) + " · " + tr(e.source), kTextDim, 1, false, dw - 12);
        ty += 11;
        std::vector<std::string> warnings;
        if (!e.report.unknown.empty()) {
            int n = 0;
            for (auto& [name, count] : e.report.unknown) n += count;
            warnings.push_back(trf("{0} neznámých druhů bloků ({1}x) vynecháno",
                                   {std::to_string(e.report.unknown.size()), std::to_string(n)}));
        }
        if (e.report.ignoredStates) warnings.push_back("Stavy bloků (natočení apod.) ignorovány");
        if (e.report.entities) warnings.push_back(trf("{0} entit nebylo importováno", {std::to_string(e.report.entities)}));
        for (const std::string& warn : warnings) {
            ui_.text(dx + 6, ty, warn, {1, 0.8f, 0.35f, 1}, 1, false, dw - 12);
            ty += 11;
        }
        // Front view of the side that will face the player (placeStructureAhead turns its z = 0 side
        // that way): the nearest block of every (x, y) line, so doors and windows show.
        ui_.text(dx + 6, ty, "Pohled zepředu:", kTextDim, 1, false);
        const float boxTop = ty + 11, boxH = listY + listH - 4 - boxTop;
        const float cell = std::max(1.0f, std::floor(std::min((dw - 12) / s.sx, boxH / s.sy)));
        const float ox = dx + std::floor((dw - cell * s.sx) / 2), oy = boxTop + std::floor((boxH - cell * s.sy) / 2);
        ui_.fill(ox - 1, oy - 1, cell * s.sx + 2, cell * s.sy + 2, {0, 0, 0, 0.35f});
        for (int y = 0; y < s.sy; ++y)
            for (int x = 0; x < s.sx; ++x)
                for (int z = 0; z < s.sz; ++z) {
                    ItemId id = s.at(x, y, z);
                    if (id == Structure::kVoid || id == AIR) continue;
                    // Deeper blocks are drawn darker; seen from the front, x runs right to left.
                    float shade = 1.0f - 0.5f * z / std::max(1, s.sz - 1);
                    ui_.tile(ox + (s.sx - 1 - x) * cell, oy + (s.sy - 1 - y) * cell, cell, tileFor(id, F_NORTH),
                             {shade, shade, shade, 1});
                    break;
                }
    } else {
        ui_.text(dx + 6, listY + 6, "Žádné struktury", kTextDim, 1, false, dw - 12);
    }

    // Actions: two rows of three.
    const float gap = 4, bw = std::floor((pw - 12 - gap * 2) / 3), row1 = py + ph - 48, row2 = py + ph - 24;
    const float c0 = px + 6, c1 = c0 + bw + gap, c2 = c1 + bw + gap;
    static const char* kTurns[] = {"Otočit: 0°", "Otočit: 90°", "Otočit: 180°", "Otočit: 270°"};
    if (ui_.button(c0, row1, bw, 20, kTurns[structureRotation_])) structureRotation_ = (structureRotation_ + 1) % 4;
    if (ui_.button(c1, row1, bw, 20, structureMirror_ ? "Zrcadlit: ano" : "Zrcadlit: ne")) structureMirror_ = !structureMirror_;
    if (ui_.button(c2, row1, bw, 20, "Umístit před sebe", !structures_.empty(), true)) {
        const StructureEntry& e = structures_[structureSelected_];
        Structure s = (structureMirror_ ? e.data.mirroredX() : e.data).rotated(structureRotation_);
        std::string msg;
        if (session_->placeStructureAhead(s, msg)) {
            playSound(Sound::Place);
            structureStatus_.clear();
            setScreen(Screen::Playing);
            return;
        }
        structureStatus_ = trf("Nelze umístit: {0}", {tr(msg)});
    }
    if (ui_.button(c0, row2, bw, 20, "Vrátit zpět", session_->canUndoStructure())) {
        session_->undoStructure();
        structureStatus_ = "Poslední umístění vráceno";
    }
    if (ui_.button(c1, row2, bw, 20, "Importovat soubor", (bool)onImportStructure)) onImportStructure();
    if (ui_.button(c2, row2, bw, 20, "Obnovit seznam")) refreshStructures();
    if (!structureStatus_.empty()) ui_.textCentered(W / 2, py + ph + 3, structureStatus_);
}

void Game::deathScreen() {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0.5f, 0, 0, 0.45f});
    ui_.textCentered(W / 2, H * 0.22f, "Zemřel jsi!", {1, 1, 1, 1}, 3);
    float bw = 200, bx = (W - bw) / 2, by = H * 0.5f;
    if (ui_.button(bx, by, bw, 22, "Oživit", true, true)) {
        session_->respawn();
        setScreen(Screen::Playing);
    }
    if (ui_.button(bx, by + 28, bw, 22, "Hlavní menu")) {
        session_->respawn();  // never save a dead player
        endSession();
        setScreen(Screen::Title);
    }
}

// ---------------------------------------------------------------- sound, skins, store

void Game::playSound(Sound s) {
    if (!onPlaySound || settings_.soundVolume <= 0) return;
    float volume = settings_.soundVolume;
    if (settings_.calmMode) volume *= (s == Sound::Explode || s == Sound::Hurt || s == Sound::Grunt) ? 0.35f : 0.6f;  // softer
    onPlaySound(soundName(s), volume);
}

void Game::loadSkins() {
    skins_ = builtinSkins();
    skins_.push_back(developerSkin());  // listed only for the developer (see skinsScreen)
    // The player's own skins: any valid PNG in skins/.
    std::error_code ec;
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(skinsDir_, ec))
        if (e.is_regular_file() && e.path().extension() == ".png") files.push_back(e.path().filename().string());
    std::sort(files.begin(), files.end());
    for (const std::string& f : files) {
        Skin s;
        std::string err;
        if (!Skin::loadPng(skinsDir_ + "/" + f, s, err)) continue;
        s.id = "custom:" + f;
        s.name = f.substr(0, f.size() - 4);
        skins_.push_back(std::move(s));
    }
    if (!hasSurface_) return;  // uploaded again in onSurfaceCreated
    // One texture with every skin, for the previews in menus.
    const int cols = skinAtlasCols_, rows = (int)(skins_.size() + cols - 1) / cols;
    const int aw = cols * Skin::kSize, ah = std::max(1, rows) * Skin::kSize;
    std::vector<uint8_t> atlas((size_t)aw * ah * 4, 0);
    for (size_t i = 0; i < skins_.size(); ++i) {
        int ox = (int)(i % cols) * Skin::kSize, oy = (int)(i / cols) * Skin::kSize;
        for (int y = 0; y < Skin::kSize; ++y)
            std::copy(&skins_[i].rgba[(size_t)y * Skin::kSize * 4], &skins_[i].rgba[(size_t)(y + 1) * Skin::kSize * 4],
                      &atlas[((size_t)(oy + y) * aw + ox) * 4]);
    }
    renderer_.setSkinAtlas(atlas, aw, ah);
    applySkin();
}

int Game::skinIndex(const std::string& id) const {
    if (id == "developer" && !profile_.isDeveloper()) return -1;  // nobody else may wear it
    if (id.rfind("vip_", 0) == 0 && !isVip(profile_, nowSeconds())) return -1;  // VIP skins while VIP lasts
    for (size_t i = 0; i < skins_.size(); ++i)
        if (skins_[i].id == id) return (int)i;
    return -1;
}

void Game::applySkin() {
    int i = skinIndex(profile_.skin);
    if (i < 0) i = 0;
    if (!skins_.empty()) renderer_.setPlayerSkin(skins_[i].rgba);
}

void Game::drawSkinFigure(float x, float y, float scale, int skin, bool back) {
    if (skin < 0 || skin >= (int)skins_.size()) return;
    const int cols = skinAtlasCols_, rows = (int)(skins_.size() + cols - 1) / cols;
    const float aw = (float)cols * Skin::kSize, ah = (float)rows * Skin::kSize;
    const float ox = (float)(skin % cols) * Skin::kSize, oy = (float)(skin / cols) * Skin::kSize;
    // Draws a w x h rectangle of the skin texture at figure position (fx, fy) in skin pixels.
    auto part = [&](float fx, float fy, int u, int v, int w, int h) {
        ui_.image(TexSkins, x + fx * scale, y + fy * scale, w * scale, h * scale, (ox + u) / aw, (oy + v) / ah,
                  (ox + u + w) / aw, (oy + v + h) / ah);
    };
    // Front faces sit at (u+d, v+d) of each part, back faces at (u+2d+w, v+d).
    // Seen from the front the player's right arm is on the viewer's left.
    for (int layer = 0; layer < 2; ++layer) {
        if (!back) {
            part(4, 0, layer ? 40 : 8, 8, 8, 8);                 // head / hat
            part(4, 8, 20, layer ? 36 : 20, 8, 12);              // body / jacket
            part(0, 8, 44, layer ? 36 : 20, 4, 12);              // right arm
            part(12, 8, layer ? 52 : 36, 52, 4, 12);             // left arm
            part(4, 20, 4, layer ? 36 : 20, 4, 12);              // right leg
            part(8, 20, layer ? 4 : 20, 52, 4, 12);              // left leg
        } else {
            part(4, 0, layer ? 56 : 24, 8, 8, 8);
            part(4, 8, 32, layer ? 36 : 20, 8, 12);
            part(12, 8, 52, layer ? 36 : 20, 4, 12);             // right arm is now on the viewer's right
            part(0, 8, layer ? 60 : 44, 52, 4, 12);
            part(8, 20, 12, layer ? 36 : 20, 4, 12);
            part(4, 20, layer ? 12 : 28, 52, 4, 12);
        }
    }
}

void Game::storeScreen() {
    const bool dialog = pendingBuy_ >= 0;
    if (dialog) ui_.holdTaps(true);  // only the purchase dialog takes taps
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Obchod VoxelCraftu", kTitle, 1, false);
    ui_.text(x + 2, y + 14, "Platí se mincemi z hraní, žádné skutečné peníze.", kTextDim, 1, false, w - 120);
    coinsPill(x + w, y);

    const char* cats[] = {"Vše", "Skiny", "Textury", "Světy a modpacky", "VIP a doplňky", "Odměny"};
    float cx = x, catsW = 0;
    for (const char* c : cats) catsW += ui_.textWidth(c) + 20;
    const float pad = catsW <= w ? 16 : 6;  // tighter tabs on narrow screens
    for (int i = 0; i < 6; ++i) {
        float bw = ui_.textWidth(cats[i]) + pad;
        if (ui_.button(cx, y + 26, bw, 18, cats[i], true, storeCategory_ == i)) {
            storeCategory_ = i;
            storeScroll_ = 0;
        }
        cx += bw + 4;
    }

    std::vector<int> shown;
    const auto& catalog = storeCatalog();
    const long long nowS = nowSeconds();
    for (size_t i = 0; i < catalog.size(); ++i) {
        StoreItem::Kind k = catalog[i].kind;
        if (storeCategory_ == 0 || (storeCategory_ == 1 && k == StoreItem::Kind::Skin) ||
            (storeCategory_ == 2 && k == StoreItem::Kind::Textures) || (storeCategory_ == 3 && k == StoreItem::Kind::ModPack) ||
            (storeCategory_ == 4 && (k == StoreItem::Kind::Vip || k == StoreItem::Kind::Addon)))
            shown.push_back((int)i);
    }
    if (storeCategory_ == 5) shown.clear();

    // Cards, two per row, scrolled by dragging.
    const float gridY = y + 50, gridH = h - 50 - 16, gap = 6;
    const float cardW = std::floor((w - gap) / 2), cardH = 86;
    const int rows = ((int)shown.size() + 1) / 2;
    if (pendingBuy_ < 0) storeScroll_ -= ui_.dragY(x, gridY, w, gridH);
    storeScroll_ = std::clamp(storeScroll_, 0.0f, std::max(0.0f, rows * (cardH + gap) - gridH));
    int worn = skinIndex(profile_.skin);
    for (size_t n = 0; n < shown.size(); ++n) {
        const StoreItem& it = catalog[shown[n]];
        float bx = x + (n % 2) * (cardW + gap), by = gridY + (n / 2) * (cardH + gap) - storeScroll_;
        if (by < gridY - 1 || by + cardH > gridY + gridH + 1) continue;  // only whole cards
        ui_.fill(bx + 2, by + 2, cardW, cardH, {0, 0, 0, 0.4f});
        ui_.frame(bx, by, cardW, cardH, kPanel, kBorder);
        // Banner
        Color bc{it.color[0], it.color[1], it.color[2], 1};
        ui_.fill(bx + 1, by + 1, cardW - 2, 34, bc);
        float bs = ui_.textWidth(it.banner, 2) < cardW - 70 ? 2 : 1;
        ui_.textCentered(bx + cardW / 2, by + 18 - 4 * bs, it.banner, kTitle, bs);
        if (it.kind == StoreItem::Kind::Skin) {
            int si = skinIndex(it.id.substr(5));
            if (si >= 0) drawSkinFigure(bx + cardW - 28, by + 2, 0.9f, si, false);
        }
        float tw = ui_.textWidth(it.tag) + 8;
        ui_.fill(bx + 4, by + 4, tw, 11, {0.06f, 0.05f, 0.04f, 0.9f});
        ui_.text(bx + 8, by + 6, it.tag, kGold, 1, false);
        char rating[8];
        std::snprintf(rating, sizeof rating, "%.1f", it.rating);
        float rw = ui_.textWidth(rating) + 18;
        ui_.fill(bx + 4, by + 21, rw, 11, {0.06f, 0.05f, 0.04f, 0.9f});
        ui_.icon(bx + 8, by + 23, Font::Star, kGold);
        ui_.text(bx + 18, by + 23, rating, kGold, 1, false);
        // Body
        ui_.text(bx + 6, by + 38, it.title, kTextBright, 1, false, cardW - 12);
        ui_.text(bx + ui_.textWidth(it.title) + 12, by + 38, "· VoxelCraft", kTextDim, 1, false,
                 cardW - ui_.textWidth(it.title) - 18);
        ui_.text(bx + 6, by + 50, it.description, kText, 1, false, cardW - 12);
        ui_.separator(bx + 4, by + cardH - 22, cardW - 8);
        // Footer: price and action
        bool owned = profile_.owns(it.id);
        // Won for a while on the wheel (or from the daily rewards): the time left.
        auto left = [&]() -> std::string {
            auto u = profile_.until.find(it.id);
            if (u == profile_.until.end() || u->second <= nowS) return "";
            return trf("zbývá {0} h", {std::to_string((u->second - nowS + 3599) / 3600)});
        };
        if (it.kind == StoreItem::Kind::Addon && (owned || addonOwned(profile_, it.id, nowS))) {
            bool on = !profile_.addonsOff.count(it.id);
            ui_.text(bx + 6, by + cardH - 15, owned ? tr("Vlastníš") : left(), kAccent, 1, false, cardW - 90);
            if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, on ? "Zapnuto" : "Vypnuto", true, on)) {
                if (on) profile_.addonsOff.insert(it.id);
                else profile_.addonsOff.erase(it.id);
                profile_.save(profilePath_);
            }
        } else if (it.id == "addon:anniversary") {
            float ew = ui_.textWidth("Zadat kód") + 14;
            ui_.text(bx + 6, by + cardH - 15, "Kód z kola štěstí", kGold, 1, false, cardW - ew - 18);
            if (ui_.button(bx + cardW - ew - 6, by + cardH - 19, ew, 16, "Zadat kód", true, true) && onTextPrompt)
                onTextPrompt(PromptCode, tr("Výroční kód (VC1Y-....-..)"), "");
        } else if (it.kind == StoreItem::Kind::Vip && !owned && isVip(profile_, nowS)) {
            ui_.text(bx + 6, by + cardH - 15, left(), kAccent, 1, false, cardW - 90);
            if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, "Koupit", true, true)) pendingBuy_ = shown[n];
        } else if (it.kind == StoreItem::Kind::Vip || it.kind == StoreItem::Kind::Addon) {
            if (owned) {
                ui_.text(bx + 6, by + cardH - 15, "Vlastníš", kAccent, 1, false);
            } else {
                ui_.icon(bx + 6, by + cardH - 15, Font::Coin, kGold);
                ui_.text(bx + 18, by + cardH - 15, std::to_string(it.price), profile_.coins >= it.price ? kGold : kError, 1, false);
                if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, "Koupit", true, true)) pendingBuy_ = shown[n];
            }
        } else if (!owned && developerFree(it, profile_)) {
            ui_.text(bx + 6, by + cardH - 15, "Zdarma pro vývojáře", kAccent, 1, false, cardW - 90);
            if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, "Vyzkoušet", true, true)) pendingBuy_ = shown[n];
        } else if (!owned) {
            std::string price = std::to_string(it.price);
            ui_.icon(bx + 6, by + cardH - 15, Font::Coin, kGold);
            ui_.text(bx + 18, by + cardH - 15, price, profile_.coins >= it.price ? kGold : kError, 1, false);
            if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, "Koupit", true, true)) pendingBuy_ = shown[n];
        } else if (it.kind == StoreItem::Kind::Skin) {
            int si = skinIndex(it.id.substr(5));
            bool wearing = si == worn;
            ui_.text(bx + 6, by + cardH - 15, "Vlastníš", kAccent, 1, false);
            if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, wearing ? "Nasazeno" : "Nasadit", !wearing)) {
                profile_.skin = it.id.substr(5);
                profile_.save(profilePath_);
                applySkin();
            }
        } else {
            ui_.text(bx + 6, by + cardH - 15, "Nainstalováno", kAccent, 1, false);
            if (ui_.button(bx + cardW - 76, by + cardH - 19, 70, 16, "Balíčky")) setScreen(Screen::Packs);
        }
    }

    if (storeCategory_ == 5) rewardsPage(x, gridY, w, gridH);
    else if (storeScroll_ < rows * (cardH + gap) - gridH - 1 && cx + ui_.textWidth("Táhni pro další") < x + w) {
        const char* hint = "Táhni pro další";
        ui_.text(x + w - ui_.textWidth(hint), y + 31, hint, kTextDim, 1, false);
    }
    ui_.text(x, y + h - 10, "Mince: 1 za 10 vytěžených bloků v Přežití a denní odměny.", kTextDim, 1, false, w - 70);
    if (ui_.button(x + w - 64, y + h - 14, 64, 16, "Zpět")) setScreen(Screen::Title);
    if (now_ < toastUntil_ && !toast_.empty()) ui_.textCentered(x + w / 2 + 40, y + 2, toast_, kAccent);

    // Purchase confirmation
    if (dialog) ui_.holdTaps(false);
    if (pendingBuy_ >= 0) {
        const StoreItem& it = catalog[pendingBuy_];
        const float W = ui_.width(), H = ui_.height();
        ui_.fill(0, 0, W, H, {0, 0, 0, 0.55f});
        float pw = std::min(330.0f, W - 20), ph = 92, px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
        ui_.frame(px, py, pw, ph, kPanel, kBorder);
        const bool devFree = developerFree(it, profile_);
        bool enough = devFree || profile_.coins >= it.price;
        ui_.textCentered(W / 2, py + 10, trf("Koupit „{0}“?", {tr(it.title)}), kTextBright);
        std::string cost = devFree ? tr("Vývojářský přístup: zdarma, mince se neodečtou")
                                   : trf("Cena {0} mincí, máš {1}", {std::to_string(it.price), std::to_string(profile_.coins)});
        ui_.textCentered(W / 2, py + 26, cost, kGold);
        if (!enough)
            ui_.textCentered(W / 2, py + 40, trf("Chybí {0} mincí. Hraj a těž!", {std::to_string(it.price - profile_.coins)}), kError);
        float bw = (pw - 24) / 2;
        if (ui_.button(px + 8, py + ph - 28, bw, 20, "Koupit", enough, true)) {
            std::string err;
            BuyResult r = buyStoreItem(it, profile_, packs_, err);
            if (r == BuyResult::Bought) {
                profile_.save(profilePath_);
                playSound(Sound::Coin);
                toast_ = trf("Koupeno: {0}", {tr(it.title)});
                if (it.kind == StoreItem::Kind::Skin) {
                    profile_.skin = it.id.substr(5);
                    profile_.save(profilePath_);
                    applySkin();
                } else if (it.kind == StoreItem::Kind::Vip || it.kind == StoreItem::Kind::Addon) {
                    profile_.addonsOff.erase(it.id);
                    profile_.save(profilePath_);
                } else {
                    rebuildFromPacks();
                }
            } else {
                toast_ = r == BuyResult::Failed ? trf("Instalace selhala: {0}", {tr(err)}) : tr("Nákup se nepovedl");
            }
            toastUntil_ = now_ + 4;
            pendingBuy_ = -1;
        }
        if (ui_.button(px + 16 + bw, py + ph - 28, bw, 20, "Zrušit")) pendingBuy_ = -1;
    }
}

namespace {
constexpr float kWheelSpinTime = 3.5f;
const char* wheelShortLabel(WheelPrize p) {
    switch (p) {
        case WheelPrize::Coins10: return "10";
        case WheelPrize::Coins50: return "50";
        case WheelPrize::Coins100: return "100";
        case WheelPrize::Coins250: return "250";
        case WheelPrize::Coins500: return "500";
        case WheelPrize::Vip: return "VIP";
        case WheelPrize::Skin: return "Skin";
        case WheelPrize::Addon: return "8 h";
        case WheelPrize::Code: return "Kód";
        default: return "";
    }
}
}  // namespace

void Game::rewardsPage(float x, float y, float w, float h) {
    const int today = todayYmd();
    const long long nowS = nowSeconds();
    const uint32_t seed = (uint32_t)std::time(nullptr) * 2654435761u ^ (uint32_t)(now_ * 1000);

    // Daily rewards: a track of 7 days; missing a day starts it again.
    const float leftW = std::floor(w * 0.56f);
    ui_.frame(x, y, leftW, h, kPanelInner, kSeparator);
    ui_.text(x + 6, y + 5, "Denní odměny", kTitle, 1, false);
    const int next = nextRewardDay(profile_, today);
    const int claimedUpTo = next ? next - 1 : profile_.rewardStreak;
    const float gap = 3, cw = std::floor((leftW - 12 - 6 * gap) / 7), ch = 50;
    for (int d = 1; d <= 7; ++d) {
        const DailyReward& r = dailyReward(d);
        float cx = x + 6 + (d - 1) * (cw + gap), cy = y + 18;
        bool claimed = d <= claimedUpTo, isToday = d == next;
        ui_.frame(cx, cy, cw, ch, isToday ? kGreen : claimed ? kButton : kField, isToday ? kGreenBorder : kSeparator);
        ui_.textCentered(cx + cw / 2, cy + 3, trf("Den {0}", {std::to_string(d)}), d == 7 ? kGold : kTextBright);
        ui_.icon(cx + cw / 2 - 10, cy + 16, Font::Coin, kGold);
        ui_.text(cx + cw / 2 - 1, cy + 16, std::to_string(r.coins), kGold, 1, false);
        // Extras: an item from the day, a spin of the wheel (+1), an addon for 8 hours.
        if (!r.items.empty()) ui_.item(cx + cw / 2 - 5, cy + 27, 10, (ItemId)r.items[0].first);
        else if (r.spins) ui_.textCentered(cx + cw / 2, cy + 28, "+1", kAccent);
        if (r.addon) ui_.textCentered(cx + cw / 2, cy + 28, "8 h", kAccent);
        if (claimed) ui_.fill(cx + 3, cy + ch - 6, cw - 6, 3, kAccent);  // taken
    }
    float by = y + 18 + ch + 6;
    if (ui_.button(x + 6, by, leftW - 12, 20, next ? "Vyzvednout odměnu" : "Zítra další odměna", next > 0, true)) {
        std::string text = claimDailyReward(profile_, today, nowS, seed);
        if (!text.empty()) {
            profile_.save(profilePath_);
            playSound(Sound::Coin);
            toast_ = text;
            toastUntil_ = now_ + 5;
        }
    }
    by += 26;
    ui_.text(x + 6, by, isVip(profile_, nowS) ? "VIP: 2× mince z odměn a točení navíc" : "Věci z odměn dostaneš v příštím světě.",
             isVip(profile_, nowS) ? kGold : kTextDim, 1, false, leftW - 12);
    by += 14;
    if (ui_.button(x + 6, by, 90, 18, "Zadat kód") && onTextPrompt)
        onTextPrompt(PromptCode, tr("Výroční kód (VC1Y-....-..)"), "");
    if (!profile_.codes.empty()) {
        ui_.text(x + 102, by + 5, trf("Tvůj kód: {0}", {profile_.codes.back()}), kGold, 1, false, leftW - 108);
    }

    // The wheel of fortune.
    const float rx = x + leftW + 6, rw = w - leftW - 6;
    ui_.frame(rx, y, rw, h, kPanelInner, kSeparator);
    ui_.text(rx + 6, y + 5, "Kolo štěstí", kTitle, 1, false);
    const int spins = spinsAvailable(profile_, today, nowS);
    const float R = std::floor(std::max(20.0f, std::min(rw / 2 - 10, (h - 56) / 2)));
    const float wx = std::floor(rx + rw / 2), wy = std::floor(y + 22 + R);
    const auto& segs = wheelSegments();
    const int n = (int)segs.size();
    const float seg = 2 * kPi / n;
    if (!wheelMessage_.empty()) {
        float t = std::min(1.0f, (float)(now_ - wheelStart_) / kWheelSpinTime);
        wheelAngle_ = wheelFrom_ + (wheelTo_ - wheelFrom_) * (1 - (1 - t) * (1 - t) * (1 - t));
        if (t >= 1) {
            toast_ = wheelMessage_;
            toastUntil_ = now_ + 6;
            wheelMessage_.clear();
            playSound(Sound::Coin);
        }
    }
    // The disc, one row at a time, in runs of the same segment.
    for (int py = (int)-R; py < (int)R; ++py) {
        float dy = py + 0.5f, half = std::sqrt(std::max(0.0f, R * R - dy * dy));
        int x0 = (int)std::floor(-half), x1 = (int)std::ceil(half);
        int runStart = x0, runSeg = -2;
        for (int px = x0; px <= x1; ++px) {
            int s = -1;
            if (px < x1) {
                float dx = px + 0.5f, r = std::sqrt(dx * dx + dy * dy);
                if (r > R) s = -3;
                else if (r > R - 2 || r < 5) s = -1;  // rim and hub
                else {
                    float a = std::atan2(dx, -dy) - wheelAngle_;
                    a = std::fmod(a, 2 * kPi);
                    if (a < 0) a += 2 * kPi;
                    s = std::min(n - 1, (int)(a / seg));
                }
            } else {
                s = -4;
            }
            if (s != runSeg) {
                if (runSeg != -2 && runSeg != -3 && px > runStart) {
                    Color c = runSeg == -1 ? Color{0.85f, 0.68f, 0.15f, 1}
                                           : Color{segs[runSeg].color[0], segs[runSeg].color[1], segs[runSeg].color[2], 1};
                    ui_.fill(wx + runStart, wy + py, (float)(px - runStart), 1, c);
                }
                runSeg = s;
                runStart = px;
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        float mid = (i + 0.5f) * seg + wheelAngle_;
        ui_.textCentered(wx + std::sin(mid) * R * 0.66f, wy - std::cos(mid) * R * 0.66f - 4, wheelShortLabel(segs[i].prize),
                         kTextBright);
    }
    // The pointer at the top.
    for (int k = 0; k < 7; ++k) ui_.fill(wx - 6 + k, wy - R - 6 + k, (float)(12 - 2 * k), 1, kError);
    float sy = wy + R + 6;
    bool spinning = !wheelMessage_.empty();
    if (ui_.button(rx + 6, sy, rw - 12, 20, spins > 0 ? trf("Roztočit ({0})", {std::to_string(spins)}) : tr("Zítra znovu"),
                   spins > 0 && !spinning, true)) {
        std::string code;
        int index = spinWheel(profile_, today, nowS, seed, wheelMessage_, code);
        if (index >= 0) {
            profile_.save(profilePath_);
            // Land the middle of the won segment under the pointer, after a few turns.
            float base = std::fmod(wheelAngle_, 2 * kPi);
            float want = -(index + 0.5f) * seg;
            float delta = std::fmod(want - base, 2 * kPi);
            if (delta < 0) delta += 2 * kPi;
            wheelFrom_ = base;
            wheelTo_ = base + 4 * 2 * kPi + delta;
            wheelStart_ = now_;
        }
    }
}

void Game::skinsScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Profil a skiny", kTitle, 1, false);
    coinsPill(x + w, y);

    // Owned skins
    std::vector<int> owned;
    for (size_t i = 0; i < skins_.size(); ++i) {
        const Skin& s = skins_[i];
        if (s.developerOnly ? profile_.isDeveloper()
            : s.vipOnly    ? isVip(profile_, nowSeconds())
                           : (s.id.rfind("custom:", 0) == 0 || profile_.owns("skin:" + s.id)))
            owned.push_back((int)i);
    }
    skinSelected_ = std::clamp(skinSelected_, 0, std::max(0, (int)skins_.size() - 1));
    if (std::find(owned.begin(), owned.end(), skinSelected_) == owned.end() && !owned.empty())
        skinSelected_ = skinIndex(profile_.skin) >= 0 ? skinIndex(profile_.skin) : owned[0];

    const float listY = y + 22, listH = h - 22 - 28, tile = 46, listW = std::floor(w * 0.58f);
    const float scaleCap = 4;
    ui_.frame(x, listY, listW, listH, kPanelInner, kSeparator);
    const int perRow = std::max(1, (int)((listW - 4) / (tile + 4)));
    listScroll_ -= ui_.dragY(x, listY, listW, listH);
    int rowsN = ((int)owned.size() + perRow - 1) / perRow;
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, rowsN * (tile + 14) - listH + 4));
    int worn = skinIndex(profile_.skin);
    for (size_t n = 0; n < owned.size(); ++n) {
        int si = owned[n];
        float tx = x + 4 + (n % perRow) * (tile + 4), ty = listY + 4 + (n / perRow) * (tile + 14) - listScroll_;
        if (ty < listY || ty + tile + 10 > listY + listH) continue;
        bool sel = si == skinSelected_;
        ui_.frame(tx, ty, tile, tile + 10, sel ? kGreen : kButton, sel ? kGreenBorder : kSeparator);
        drawSkinFigure(tx + tile / 2 - 10, ty + 3, 1.25f, si, false);
        std::string label = skins_[si].name;
        ui_.text(tx + 2, ty + tile, label, si == worn ? kGold : kText, 1, false, tile - 4);
        if (ui_.tapped(tx, ty, tile, tile + 10)) skinSelected_ = si;
    }

    // Preview of the selected skin
    float px = x + listW + 8, pw = w - listW - 8;
    ui_.frame(px, listY, pw, listH, kField, kSeparator);
    if (!skins_.empty()) {
        const float scale = std::min(scaleCap, std::floor((listH - 50) / 32));
        drawSkinFigure(px + (pw - 16 * scale) / 2, listY + 8, scale, skinSelected_, skinBack_);
        ui_.textCentered(px + pw / 2, listY + listH - 38, skins_[skinSelected_].name, kTextBright);
        float bw = std::floor((pw - 12) / 2);
        if (ui_.button(px + 4, listY + listH - 24, bw, 20, skinBack_ ? "Zepředu" : "Zezadu")) skinBack_ = !skinBack_;
        bool wearing = skinSelected_ == worn;
        if (ui_.button(px + 8 + bw, listY + listH - 24, bw, 20, wearing ? "Nasazeno" : "Nasadit", !wearing, true)) {
            profile_.skin = skins_[skinSelected_].id;
            profile_.save(profilePath_);
            applySkin();
        }
    }

    float by = y + h - 22, bw = std::floor((w - 16) / 3);
    if (ui_.button(x, by, bw, 20, "Nahrát skin (PNG)") && onImportSkin) onImportSkin();
    if (ui_.button(x + bw + 8, by, bw, 20, "Další v obchodě")) {
        storeCategory_ = 1;
        setScreen(Screen::Store);
    }
    if (ui_.button(x + 2 * (bw + 8), by, bw, 20, "Zpět")) setScreen(Screen::Title);
    if (now_ < toastUntil_ && !toast_.empty()) ui_.textCentered(x + w / 2, y + 2, toast_, kAccent);
}

void Game::editWorldScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Upravit svět", kTitle, 1, false);
    ui_.text(x + 2, y + 14, "Změny se projeví při příštím spuštění světa.", kTextDim, 1, false, w);
    float cy = y + 28;
    const float col = std::floor(w / 2) - 4;
    ui_.text(x, cy, "Název světa:", kText, 1, false);
    if (ui_.textBox(x, cy + 10, col, 18, editInfo_.name, "Svět") && onTextPrompt)
        onTextPrompt(PromptEditWorldName, "Název světa", editInfo_.name);
    ui_.text(x, cy + 34, "Herní režim:", kText, 1, false);
    float mw = (col - 4) / 2;
    if (ui_.button(x, cy + 44, mw, 20, "Přežití", true, editInfo_.mode == GameMode::Survival)) editInfo_.mode = GameMode::Survival;
    if (ui_.button(x + mw + 4, cy + 44, mw, 20, "Kreativní", true, editInfo_.mode == GameMode::Creative))
        editInfo_.mode = GameMode::Creative;
    const GenConfig& gen = editGen_;
    ui_.text(x, cy + 72, trf("Seed: {0}", {std::to_string((int32_t)editInfo_.seed)}), kTextDim, 1, false, col);
    ui_.text(x, cy + 84, trf("Typ: {0} · biomů: {1}", {tr(gen.flat ? "plochý" : "nekonečný"), std::to_string(gen.biomes.size())}),
             kTextDim, 1, false, col);
    ui_.text(x, cy + 96, trf("Naposledy hráno: {0}", {formatDate(editInfo_.lastPlayed)}), kTextDim, 1, false, col);

    float rx = x + col + 8, oy = cy + 10;
    ui_.frame(rx, oy, col, 92, kPanelInner, kSeparator);
    ui_.text(rx + 6, oy + 6, "Zobrazit souřadnice", kText, 1, false);
    ui_.text(rx + 6, oy + 16, "XYZ a biom vlevo nahoře", kTextDim, 1, false, col - 30);
    ui_.checkbox(rx + col - 18, oy + 8, editInfo_.showCoordinates);
    ui_.separator(rx + 4, oy + 30, col - 8);
    ui_.text(rx + 6, oy + 36, "Ponechat inventář", kText, 1, false);
    ui_.text(rx + 6, oy + 46, "Věci zůstanou po smrti", kTextDim, 1, false, col - 30);
    ui_.checkbox(rx + col - 18, oy + 38, editInfo_.keepInventory);
    ui_.separator(rx + 4, oy + 60, col - 8);
    ui_.text(rx + 6, oy + 66, "Povolit příkazy", kText, 1, false);
    ui_.text(rx + 6, oy + 76, "/gamemode, /tp, /give a další", kTextDim, 1, false, col - 30);
    ui_.checkbox(rx + col - 18, oy + 68, editInfo_.allowCommands);
    // 1.4.0: the world as one file to keep or send, and the automatic backups.
    const std::vector<std::string> backups = listBackups(editInfo_.folder, dataDir_ + "/backups");
    const float hb = (col - 4) / 2;
    if (ui_.button(rx, oy + 98, hb, 20, "Exportovat svět")) {
        const std::string file = dataDir_ + "/exports/" + editInfo_.name + ".vcworld";
        std::string err;
        saveLevel(editInfo_, editPlayer_);
        if (exportWorld(editInfo_.folder, file, err) && onExportWorld) onExportWorld(file, editInfo_.name + ".vcworld");
        else if (!err.empty()) toast_ = tr(err), toastUntil_ = now_ + 5;
    }
    if (ui_.button(rx + hb + 4, oy + 98, hb, 20, trf("Obnovit zálohu ({0})", {std::to_string(backups.size())}), !backups.empty())) {
        if (restoreBackup(editInfo_.folder, backups[0], dataDir_ + "/backups") && loadLevel(editInfo_.folder, editInfo_, editPlayer_)) {
            toast_ = tr("Svět je obnovený ze zálohy");
            toastUntil_ = now_ + 5;
        }
    }

    float by = y + h - 26, bw = std::floor((w - 16) / 3);
    if (ui_.button(x, by, bw, 22, "Uložit", true, true)) {
        saveLevel(editInfo_, editPlayer_);
        setScreen(Screen::WorldList);
        return;
    }
    if (ui_.button(x + bw + 8, by, bw, 22, "Uložit a hrát")) {
        saveLevel(editInfo_, editPlayer_);
        startSession(editInfo_, editPlayer_);
        return;
    }
    if (ui_.button(x + 2 * (bw + 8), by, bw, 22, "Zpět")) setScreen(Screen::WorldList);
}

// ---------------------------------------------------------------- containers

namespace {
constexpr float kSlot = 20;
const Color kLabel{0.25f, 0.25f, 0.25f, 1};
}  // namespace

void Game::inventoryScreen() {
    if (session_->survival()) survivalInventoryScreen();
    else creativeInventoryScreen();
}

ItemStack* Game::stackAt(SlotRef ref) {
    Inventory& inv = session_->inventory();
    Furnace* f = screen_ == Screen::Furnace ? session_->furnace(session_->openFurnace) : nullptr;
    switch (ref.area) {
        case SlotArea::Inventory: return &inv.slots[ref.index];
        case SlotArea::Grid: return &grid_[ref.index];
        case SlotArea::Armor: return &session_->armor()[ref.index];
        case SlotArea::Offhand: return &session_->offhand();
        case SlotArea::Chest: {
            ChestContents* c = screen_ == Screen::Chest ? session_->chest(session_->openChest) : nullptr;
            return c ? &(*c)[ref.index] : nullptr;
        }
        case SlotArea::FurnaceInput: return f ? &f->input : nullptr;
        case SlotArea::FurnaceFuel: return f ? &f->fuel : nullptr;
        case SlotArea::FurnaceOutput: return f ? &f->output : nullptr;
        case SlotArea::Result: return nullptr;
    }
    return nullptr;
}

void Game::drawStack(float x, float y, const ItemStack& s) {
    if (s.empty()) return;
    ui_.item(x + 2, y + 2, kSlot - 4, s.id);
    if (int max = toolDurability(s.id); max > 0 && s.damage > 0) {
        // Durability bar, green to red.
        float left = 1 - (float)s.damage / max;
        ui_.fill(x + 3, y + kSlot - 4, kSlot - 6, 2, {0, 0, 0, 1});
        ui_.fill(x + 3, y + kSlot - 4, (kSlot - 6) * left, 1, {1 - left, left, 0, 1});
    }
    if (s.count > 1) {
        std::string n = std::to_string(s.count);
        ui_.text(x + kSlot - 1 - ui_.textWidth(n), y + kSlot - 9, n);
    }
}

void Game::slot(float x, float y, SlotRef ref) {
    ItemStack* s = stackAt(ref);
    ui_.slotFrame(x, y, kSlot, held_.valid() && held_ == ref);
    if (s) drawStack(x, y, *s);
    if (ui_.tapped(x, y, kSlot, kSlot)) tapSlot(ref);
}

void Game::tapSlot(SlotRef ref) {
    Inventory& inv = session_->inventory();
    if (ref.area == SlotArea::Result) {
        craftOnce();
        return;
    }
    ItemStack* target = stackAt(ref);
    if (!target) return;
    if (ref.area == SlotArea::FurnaceOutput && !held_.valid()) {
        // Smelted items go straight into the inventory.
        if (!target->empty()) {
            int left = inv.add(*target);
            if (left == 0) *target = {};
            else target->count = (uint8_t)left;
        }
        return;
    }
    if (!held_.valid()) {
        if (!target->empty()) {
            held_ = ref;
            heldAmount_ = target->count;  // the whole stack unless the panel changes it
            slotHint_.clear();
        }
        if (ref.area == SlotArea::Inventory && ref.index < kHotbarSize) session_->selectedSlot() = ref.index;
        return;
    }
    if (held_ == ref) {  // tapping the picked slot again puts it back
        held_ = {};
        return;
    }
    ItemStack* from = stackAt(held_);
    // Nothing can be put into the furnace output.
    if (!from || from->empty() || ref.area == SlotArea::FurnaceOutput) {
        held_ = {};
        return;
    }
    // Armour slots take only their own piece; a piece taken off can go anywhere.
    if (ref.area == SlotArea::Armor && armorPiece(from->id) != ref.index) {
        slotHint_ = "Sem patří jen odpovídající kus brnění";
        held_ = {};
        return;
    }
    if (held_.area == SlotArea::Armor && !target->empty() && armorPiece(target->id) != held_.index) {
        slotHint_ = "Sem patří jen odpovídající kus brnění";
        held_ = {};
        return;
    }
    int amount = std::clamp(heldAmount_, 1, (int)from->count);
    bool sameItem = !target->empty() && target->id == from->id;
    int moved = transferItems(*from, *target, amount);
    if (moved == 0) {
        slotHint_ = sameItem ? "Cílová hromádka je plná" : "Část nejde vyměnit s jiným blokem";
        held_ = {};
        return;
    }
    slotHint_.clear();
    // After a split the rest stays picked, so it can be shared out into more slots.
    if (!from->empty() && target->id == from->id)  // (a swap leaves different items: done)
        heldAmount_ = std::min(heldAmount_, (int)from->count);
    else
        held_ = {};
}

CraftResult Game::craftResult(int size) const {
    // Items of the store addons can be crafted only while the addon is on.
    CraftResult r = matchRecipe(grid_.data(), size);
    Settings s = playSettings();
    if ((r.id == NUKE_TNT && !s.addonHalloween) || ((r.id == GIFT_BOX || r.id == CANDY_CANE) && !s.addonWinter) ||
        ((r.id == CAKE || isAnniversaryItem(r.id)) && !s.addonAnniversary))
        return {};
    return r;
}

void Game::craftOnce() {
    CraftResult r = craftResult(gridSize_);
    if (r.empty()) return;
    Inventory& inv = session_->inventory();
    Inventory trial = inv;  // only craft when the result fits
    if (trial.add(r.id, r.count) > 0) return;
    inv = trial;
    for (int i = 0; i < gridSize_ * gridSize_; ++i)
        if (!grid_[i].empty() && --grid_[i].count == 0) grid_[i] = {};
    held_ = {};
}

void Game::returnCraftingGrid() {
    if (!session_) return;
    for (ItemStack& s : grid_)
        if (!s.empty()) {
            session_->giveItem(s);  // what does not fit drops at the player's feet
            s = {};
        }
}

bool Game::containerWindow(const std::string& title, float contentH, float& gx, float& gy) {
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.45f});
    // The window and, beside it, the panel for the picked stack (amount, split, throw).
    const float pw = kSlot * 9 + 14, ph = 18 + contentH + 7, sw = 112, gap = 6;
    const float px = std::floor((W - pw - gap - sw) / 2), py = std::floor((H - ph) / 2);
    const float sx = px + pw + gap;
    ui_.panel(px, py, pw, ph);
    ui_.text(px + 7, py + 6, title, kLabel, 1, false);
    gx = px + 7;
    gy = py + 18;
    if (ui_.button(px + pw - 18, py + 3, 14, 14, "X")) {
        setScreen(Screen::Playing);
        return false;
    }
    // The recipe book (crafting screens): a button in the title bar swaps the side panel.
    const bool crafting = screen_ == Screen::Inventory || screen_ == Screen::Crafting;
    if (crafting && ui_.button(px + pw - 80, py + 3, 60, 14, "Recepty", true, recipeBook_)) recipeBook_ = !recipeBook_;
    if (crafting && recipeBook_) recipeBookPanel(sx, py, sw, ph);
    else selectionPanel(sx, py, sw, ph);
    // Tapping outside both closes the window.
    if (ui_.tapped(0, 0, W, py) || ui_.tapped(0, py + ph, W, H - py - ph) || ui_.tapped(0, 0, px, H) ||
        ui_.tapped(sx + sw, 0, W - sx - sw, H) || ui_.tapped(px + pw, py, gap, ph)) {
        setScreen(Screen::Playing);
        return false;
    }
    return true;
}

std::vector<Game::BookEntry> Game::bookEntries(int gridSize) const {
    // Every recipe that fits this grid (one per result), the ones the inventory can make first.
    std::vector<BookEntry> out;
    const RecipeBook& book = recipes();
    const Settings play = playSettings();
    auto allowed = [&](ItemId id) {
        if (id == NUKE_TNT) return play.addonHalloween;
        if (id == GIFT_BOX || id == CANDY_CANE) return play.addonWinter;
        if (id == CAKE || isAnniversaryItem(id)) return play.addonAnniversary;
        return true;
    };
    auto add = [&](const CraftResult& r, std::vector<ItemId> cells, int w) {
        if (r.empty() || !allowed(r.id)) return;
        for (const BookEntry& e : out)
            if (e.result.id == r.id) return;
        BookEntry e{r, std::move(cells), w, true};
        std::map<ItemId, int> need;
        for (ItemId c : e.cells)
            if (c != AIR) ++need[c];
        const Inventory& inv = session_->inventory();
        for (auto [id, n] : need) {
            int have = countInInventory(inv, id);
            for (const ItemStack& g : grid_)
                if (g.id == id) have += g.count;
            if (have < n) e.canMake = false;
        }
        out.push_back(std::move(e));
    };
    for (const ShapedRecipe& r : book.shaped)
        if (r.w <= gridSize && r.h <= gridSize) add(r.result, r.cells, r.w);
    for (const ShapelessRecipe& r : book.shapeless)
        if ((int)r.ingredients.size() <= gridSize * gridSize) add(r.result, r.ingredients, -1);
    std::stable_partition(out.begin(), out.end(), [](const BookEntry& e) { return e.canMake; });
    return out;
}

void Game::fillFromBook(const BookEntry& e) {
    // Back to the inventory with what is in the grid, then the recipe's items into their places.
    returnCraftingGrid();
    Inventory& inv = session_->inventory();
    for (size_t i = 0; i < e.cells.size(); ++i) {
        const ItemId id = e.cells[i];
        if (id == AIR) continue;
        const int cell = e.width > 0 ? (int)(i / (size_t)e.width) * gridSize_ + (int)(i % (size_t)e.width) : (int)i;
        if (cell >= (int)grid_.size()) continue;
        for (ItemStack& s : inv.slots)
            if (s.id == id && s.count > 0) {
                grid_[(size_t)cell] = {id, 1, s.damage};
                if (--s.count == 0) s = {};
                break;
            }
    }
}

void Game::recipeBookPanel(float x, float y, float w, float h) {
    ui_.panel(x, y, w, h);
    ui_.text(x + 6, y + 6, "Kniha receptů", kLabel, 1, false);
    const std::vector<BookEntry> entries = bookEntries(gridSize_);
    const int cols = std::max(1, (int)((w - 8) / kSlot));
    const float gy = y + 18, gh = h - 24;
    bookScroll_ -= ui_.dragY(x, gy, w, gh);
    const int rows = ((int)entries.size() + cols - 1) / cols;
    bookScroll_ = std::clamp(bookScroll_, 0.0f, std::max(0.0f, rows * kSlot - gh));
    for (size_t i = 0; i < entries.size(); ++i) {
        const float sx = x + 4 + (float)(i % (size_t)cols) * kSlot, sy = gy + (float)(i / (size_t)cols) * kSlot - bookScroll_;
        if (sy < gy - 1 || sy + kSlot > gy + gh + 1) continue;
        const BookEntry& e = entries[i];
        ui_.frame(sx, sy, kSlot - 1, kSlot - 1, e.canMake ? kGreen : kPanelInner, e.canMake ? kGreenBorder : kSeparator);
        ui_.item(sx + 2, sy + 2, kSlot - 5, e.result.id);
        if (!e.canMake) ui_.fill(sx, sy, kSlot - 1, kSlot - 1, {0, 0, 0, 0.45f});
        if (ui_.tapped(sx, sy, kSlot - 1, kSlot - 1)) {
            if (e.canMake) {
                fillFromBook(e);
            } else {
                toast_ = trf("Chybí suroviny: {0}", {tr(itemName(e.result.id))});
                toastUntil_ = now_ + 3;
            }
        }
    }
    if (entries.empty()) ui_.text(x + 6, gy + 4, "Žádné recepty", kTextDim, 1, false, w - 12);
}

void Game::selectionPanel(float x, float y, float w, float h) {
    ui_.panel(x, y, w, h);
    ItemStack* s = held_.valid() ? stackAt(held_) : nullptr;
    if (s && s->empty()) {  // used up meanwhile (e.g. smelted)
        held_ = {};
        s = nullptr;
    }
    const float ix = x + 6, iw = w - 12;
    if (!s) {
        heldAmount_ = 0;
        float ty = y + 8;
        for (const char* para : {"Ťukni na slot a vyber hromádku.", "Pak ťukni na jiný slot: přesune se zvolený počet.",
                                 "Stejné bloky se spojí, nejvýš 64 v jednom slotu."}) {
            for (const std::string& line : wrapText(para, iw)) ui_.text(ix, ty, line, kLabel, 1, false), ty += 10;
            ty += 6;
        }
        if (!slotHint_.empty()) ui_.text(ix, y + h - 14, slotHint_, {0.6f, 0.15f, 0.1f, 1}, 1, false, iw);
        return;
    }
    heldAmount_ = std::clamp(heldAmount_, 1, (int)s->count);
    // The picked stack: icon, name and the amount that will move.
    ui_.slotFrame(ix, y + 6, kSlot, true);
    drawStack(ix, y + 6, *s);
    ui_.text(ix + kSlot + 4, y + 8, "Vybráno:", kLabel, 1, false);
    ui_.text(ix + kSlot + 4, y + 18, trf("{0} z {1}", {std::to_string(heldAmount_), std::to_string(s->count)}), {0.1f, 0.1f, 0.1f, 1}, 1,
             false);
    ui_.text(ix, y + 30, itemName(s->id), kLabel, 1, false, iw);
    float by = y + 42;
    const float b3 = std::floor((iw - 4) / 3), b2 = std::floor((iw - 2) / 2);
    if (ui_.button(ix, by, b3, 16, "-", heldAmount_ > 1)) --heldAmount_;
    if (ui_.button(ix + b3 + 2, by, b3, 16, "1", heldAmount_ != 1)) heldAmount_ = 1;
    if (ui_.button(ix + 2 * b3 + 4, by, b3, 16, "+", heldAmount_ < s->count)) ++heldAmount_;
    by += 18;
    if (ui_.button(ix, by, b2, 16, "Půlka", s->count > 1)) heldAmount_ = std::max(1, (s->count + 1) / 2);
    if (ui_.button(ix + b2 + 2, by, b2, 16, "Vše", heldAmount_ != s->count)) heldAmount_ = s->count;
    by += 22;
    // Quick move: hotbar <-> main inventory, anything else -> the inventory.
    Inventory& inv = session_->inventory();
    if (ui_.button(ix, by, iw, 16, held_.area == SlotArea::Inventory && held_.index < kHotbarSize ? "Do inventáře" : "Do hotbaru")) {
        int moved;
        if (held_.area == SlotArea::Inventory && held_.index < kHotbarSize)
            moved = moveIntoRange(*s, heldAmount_, &inv.slots[kHotbarSize], inv.slots.data() + kInventorySize);
        else if (held_.area == SlotArea::Inventory)
            moved = moveIntoRange(*s, heldAmount_, inv.slots.data(), &inv.slots[kHotbarSize]);
        else
            moved = moveIntoRange(*s, heldAmount_, inv.slots.data(), inv.slots.data() + kInventorySize);
        slotHint_ = moved ? "" : "Není místo";
        held_ = {};
        return;
    }
    by += 18;
    if (ui_.button(ix, by, iw, 16, "Vyhodit")) {
        ItemStack thrown = *s;
        thrown.count = (uint8_t)heldAmount_;
        s->count = (uint8_t)(s->count - heldAmount_);
        if (s->count == 0) *s = {};
        session_->throwItem(thrown);
        held_ = {};
        return;
    }
    by += 18;
    if (ui_.button(ix, by, iw, 16, "Zrušit výběr")) held_ = {};
}

void Game::playerSlots(float gx, float gy) {
    for (int i = 0; i < 27; ++i) slot(gx + (i % 9) * kSlot, gy + (i / 9) * kSlot, {SlotArea::Inventory, kHotbarSize + i});
    for (int i = 0; i < kHotbarSize; ++i) slot(gx + i * kSlot, gy + 3 * kSlot + 4, {SlotArea::Inventory, i});
}

void Game::drawArrow(float x, float y, float progress) {
    // A right-pointing arrow, filled white as the progress grows.
    const Color dark{0.55f, 0.55f, 0.55f, 1};
    ui_.fill(x, y + 4, 14, 4, dark);
    for (int i = 0; i < 6; ++i) ui_.fill(x + 14 + i, y + i, 1, 12 - 2 * i, dark);
    if (progress > 0) {
        float w = 20 * std::min(progress, 1.0f);
        ui_.fill(x, y + 4, std::min(w, 14.0f), 4, {1, 1, 1, 1});
        for (int i = 0; i < 6 && 14 + i < w; ++i) ui_.fill(x + 14 + i, y + i, 1, 12 - 2 * i, {1, 1, 1, 1});
    }
}


void Game::survivalInventoryScreen() {
    float gx, gy;
    const float top = 4 * kSlot;
    if (!containerWindow("Inventář", top + 6 + 3 * kSlot + 4 + kSlot, gx, gy)) return;
    // Armour slots (helmet to boots) beside the player in their skin, like the classic inventory.
    for (int i = 0; i < 4; ++i) {
        float sy = gy + i * kSlot;
        slot(gx, sy, {SlotArea::Armor, i});
        if (session_->armor()[i].empty()) {
            // A faint outline of the piece that belongs here.
            static const int kHint[4] = {T_FIRST_ARMOR, T_FIRST_ARMOR + 1, T_FIRST_ARMOR + 2, T_FIRST_ARMOR + 3};
            ui_.tile(gx + 2, sy + 2, kSlot - 4, kHint[i], {0, 0, 0, 0.25f});
        }
    }
    ui_.frame(gx + kSlot + 2, gy, 50, top, {0.12f, 0.12f, 0.14f, 1}, {0, 0, 0, 1});
    drawSkinFigure(gx + kSlot + 2 + 25 - 12, gy + (top - 48) / 2, 1.5f, std::max(0, skinIndex(profile_.skin)), false);
    {
        // 1.5.7: the off hand, in the corner of the player's frame.
        const float ox = gx + kSlot + 2 + 50 - kSlot, oy = gy + top - kSlot;
        slot(ox, oy, {SlotArea::Offhand, 0});
        if (session_->offhand().empty()) ui_.tile(ox + 2, oy + 2, kSlot - 4, T_SHIELD, {0, 0, 0, 0.25f});
    }
    if (int points = session_->armorPoints(); points > 0) {
        ui_.icon(gx + kSlot + 5, gy + top - 11, Font::ArmorFull, {0.85f, 0.85f, 0.9f, 1});
        ui_.text(gx + kSlot + 15, gy + top - 11, std::to_string(points), kTextBright, 1, true);
    }
    // 2x2 crafting next to the player, like the classic inventory.
    float cx = gx + 80, cy = gy + 10;
    ui_.text(cx, gy, "Výroba", kLabel, 1, false);
    for (int i = 0; i < 4; ++i) slot(cx + (i % 2) * kSlot, cy + (i / 2) * kSlot, {SlotArea::Grid, i});
    CraftResult r = craftResult(2);
    drawArrow(cx + 2 * kSlot + 4, cy + kSlot - 6, r.empty() ? 0 : 1);
    float rx = cx + 2 * kSlot + 30, ry = cy + kSlot / 2;
    slot(rx, ry, {SlotArea::Result, 0});
    drawStack(rx, ry, {r.id, (uint8_t)r.count});
    playerSlots(gx, gy + top + 6);
}

void Game::chestScreen() {
    if (!session_->chest(session_->openChest)) {
        setScreen(Screen::Playing);
        return;
    }
    float gx, gy;
    const float top = 3 * kSlot;
    if (!containerWindow("Truhla", top + 6 + 3 * kSlot + 4 + kSlot, gx, gy)) return;
    for (int i = 0; i < kChestSize; ++i) slot(gx + (i % 9) * kSlot, gy + (i / 9) * kSlot, {SlotArea::Chest, i});
    playerSlots(gx, gy + top + 6);
}

void Game::tradeScreen() {
    // A villager's offers: what it wants, what it gives, and a button when you can pay.
    const Profession prof = (Profession)tradeProfession_;
    const auto& trades = tradesFor(prof);
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.45f});
    const float pw = 300, rowH = 26, ph = 40 + rowH * ((trades.size() + 1) / 2) + 8;
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.panel(px, py, pw, ph);
    ui_.text(px + 7, py + 6, trf("Vesničan: {0}", {tr(professionName(prof))}), kLabel, 1, false);
    Inventory& inv = session_->inventory();
    ui_.item(px + 7, py + 18, 10, RUBY);
    ui_.text(px + 20, py + 20, trf("Rubíny: {0}", {std::to_string(countInInventory(inv, RUBY))}), kLabel, 1, false);
    if (ui_.button(px + pw - 18, py + 3, 14, 14, "X")) {
        setScreen(Screen::Playing);
        return;
    }
    const float colW = (pw - 14) / 2;
    std::string hint;
    for (size_t i = 0; i < trades.size(); ++i) {
        const Trade& t = trades[i];
        float x = px + 7 + (i % 2) * colW, y = py + 34 + (i / 2) * rowH;
        ui_.slotFrame(x, y, kSlot, false);
        drawStack(x, y, t.cost);
        ui_.text(x + kSlot + 3, y + 6, ">", kLabel, 1, false);
        ui_.slotFrame(x + kSlot + 12, y, kSlot, false);
        drawStack(x + kSlot + 12, y, t.result);
        const bool can = countInInventory(inv, t.cost.id) >= t.cost.count;
        if (ui_.held(x, y, kSlot * 2 + 12, kSlot)) hint = itemName(t.result.id);
        if (ui_.button(x + kSlot * 2 + 18, y + 2, colW - kSlot * 2 - 24, 16, "Vyměnit", can, can)) {
            if (makeTrade(inv, t)) {
                playSound(Sound::Coin);
                if (session_->survival()) unlockAchievement("trader");
            }
        }
    }
    if (!hint.empty()) ui_.textCentered(W / 2, py + ph + 4, hint, kTextBright);
}

void Game::craftingScreen() {
    float gx, gy;
    const float top = 3 * kSlot;
    if (!containerWindow("Pracovní stůl", top + 6 + 3 * kSlot + 4 + kSlot, gx, gy)) return;
    float cx = gx + 22;
    for (int i = 0; i < 9; ++i) slot(cx + (i % 3) * kSlot, gy + (i / 3) * kSlot, {SlotArea::Grid, i});
    CraftResult r = craftResult(3);
    drawArrow(cx + 3 * kSlot + 10, gy + kSlot + 4, r.empty() ? 0 : 1);
    float rx = cx + 3 * kSlot + 40, ry = gy + kSlot;
    slot(rx, ry, {SlotArea::Result, 0});
    drawStack(rx, ry, {r.id, (uint8_t)r.count});
    playerSlots(gx, gy + top + 6);
}

void Game::furnaceScreen() {
    Furnace* f = session_->furnace(session_->openFurnace);
    if (!f) {
        setScreen(Screen::Playing);
        return;
    }
    float gx, gy;
    const float top = 3 * kSlot;
    if (!containerWindow("Pec", top + 6 + 3 * kSlot + 4 + kSlot, gx, gy)) return;
    if (!(f = session_->furnace(session_->openFurnace))) return;
    float ix = gx + 66;
    slot(ix, gy, {SlotArea::FurnaceInput, 0});
    // Flame: how much of the current fuel is left.
    float flame = f->burnTotal > 0 ? f->burnLeft / f->burnTotal : 0;
    ui_.fill(ix + 5, gy + kSlot + 2, 10, 16, {0.55f, 0.55f, 0.55f, 1});
    if (flame > 0) ui_.fill(ix + 5, gy + kSlot + 2 + 16 * (1 - flame), 10, 16 * flame, {1, 0.55f, 0.1f, 1});
    slot(ix, gy + 2 * kSlot, {SlotArea::FurnaceFuel, 0});
    drawArrow(ix + kSlot + 12, gy + kSlot + 4, f->cook / kSmeltSeconds);
    slot(ix + kSlot + 44, gy + kSlot, {SlotArea::FurnaceOutput, 0});
    ui_.text(gx, gy + 4, "Surovina", kLabel, 1, false);
    ui_.text(gx, gy + 2 * kSlot + 6, "Palivo", kLabel, 1, false);
    playerSlots(gx, gy + top + 6);
}

void Game::creativeInventoryScreen() {
    Inventory& inv = session_->inventory();
    int& selected = session_->selectedSlot();
    const float W = ui_.width(), H = ui_.height();
    ui_.fill(0, 0, W, H, {0, 0, 0, 0.45f});
    const float pw = std::min(W - 16, 560.0f), ph = H - 12;
    const float px = std::floor((W - pw) / 2), py = std::floor((H - ph) / 2);
    ui_.panel(px, py, pw, ph);
    if (ui_.button(px + pw - 18, py + 3, 14, 14, "X")) {
        setScreen(Screen::Playing);
        return;
    }
    // 1.5.9: categories as icon tabs on the left (two columns), the player on the right, and
    // groups of variants (stained glass, stairs...) behind one button in the middle.
    const Settings play = playSettings();
    std::vector<ItemId> addonItems;
    if (play.addonWinter) addonItems.insert(addonItems.end(), {GIFT_BOX, CANDY_CANE});
    if (play.addonHalloween) addonItems.insert(addonItems.end(), {NUKE_TNT, PUMPKIN});
    const std::vector<ItemId>& modItems = ModRegistry::get().creative();
    if (play.addonAnniversary)
        addonItems.insert(addonItems.end(), {CAKE, ANNIV_LOGO, CONFETTI_BLOCK, BALLOON, BALLOON + 1, BALLOON + 2, PARTY_LANTERN,
                                             FESTIVE_BRICKS, PAINTED_PLANKS, PAINTED_PLANKS + 1, PAINTED_PLANKS + 2, PAINTED_PLANKS + 3,
                                             ANNIV_TOKEN, ANNIV_MEDAL, PARTY_POPPER});
    struct Tab {
        const char* name;
        ItemId icon;
        const std::vector<ItemId>* items;
    };
    std::vector<Tab> tabs;
    for (int c = 0; c < CatCount; ++c) tabs.push_back({creativeCategoryName(c), creativeCategoryIcon(c), &creativeCategoryItems(c)});
    if (!addonItems.empty()) tabs.push_back({"Doplňky", addonItems[0], &addonItems});
    if (!modItems.empty()) tabs.push_back({"Módy", modItems[0], &modItems});
    if (creativeTab_ >= (int)tabs.size()) creativeTab_ = 0;
    const float tab = 24, tabX = px + 6, tabY = py + 20;
    for (int t = 0; t < (int)tabs.size(); ++t) {
        const float x = tabX + (t % 2) * (tab + 2), y = tabY + (t / 2) * (tab + 2);
        ui_.slotFrame(x, y, tab, t == creativeTab_);
        drawStack(x + (tab - kSlot) / 2, y + (tab - kSlot) / 2, {tabs[t].icon, 1});
        if (ui_.held(x, y, tab, tab)) creativeHint_ = tabs[t].name;
        if (ui_.tapped(x, y, tab, tab)) {
            creativeTab_ = t;
            creativeScroll_ = 0;
            creativeGroup_.clear();
        }
    }
    const float tabW = 2 * tab + 2;
    // The player, as the skin screen draws it.
    const float figW = 40, figX = px + pw - figW - 6, figY = py + 22;
    ui_.frame(figX, figY, figW, 84, kField, kSeparator);
    if (!skins_.empty()) drawSkinFigure(figX + 4, figY + 10, 2, std::max(0, skinIndex(profile_.skin)), false);
    // The catalogue: the category (variants folded into their group), or one open group.
    const std::vector<ItemId>& all = *tabs[(size_t)creativeTab_].items;
    std::vector<ItemId> items;
    std::vector<bool> folded;
    for (ItemId id : all) {
        const char* g = creativeTab_ < CatCount ? creativeGroup(id) : nullptr;
        if (!creativeGroup_.empty()) {
            if (g && creativeGroup_ == g) items.push_back(id), folded.push_back(false);
            continue;
        }
        if (g) {
            bool seen = false;
            for (ItemId o : items) seen = seen || (creativeGroup(o) && std::string(creativeGroup(o)) == g);
            if (seen) continue;
            int n = 0;
            for (ItemId o : all) n += creativeGroup(o) && std::string(creativeGroup(o)) == g;
            items.push_back(id), folded.push_back(n > 1);
        } else {
            items.push_back(id), folded.push_back(false);
        }
    }
    const float gx = px + tabW + 14;
    float gy = py + 20;
    ui_.text(gx, py + 6, creativeGroup_.empty() ? tabs[(size_t)creativeTab_].name : creativeGroup_.c_str(), kTextBright, 1, false,
             figX - gx - 60);
    if (!creativeGroup_.empty() && ui_.button(figX - 56, py + 3, 50, 14, "Zpět")) {
        creativeGroup_.clear();
        creativeScroll_ = 0;
    }
    const int cols = std::max(4, (int)((figX - gx - 10) / kSlot));
    const float gridH = ph - 20 - kSlot - 22;
    const int rows = ((int)items.size() + cols - 1) / cols;
    creativeScroll_ -= ui_.dragY(gx, gy, cols * kSlot, gridH);
    creativeScroll_ = std::clamp(creativeScroll_, 0.0f, std::max(0.0f, rows * kSlot - gridH));
    for (int i = 0; i < (int)items.size(); ++i) {
        float x = gx + (i % cols) * kSlot, y = gy + (i / cols) * kSlot - creativeScroll_;
        if (y < gy - 0.5f || y + kSlot > gy + gridH + 0.5f) continue;
        ItemId id = items[i];
        ui_.slotFrame(x, y, kSlot, false);
        drawStack(x, y, {id, 1});
        if (folded[i]) {
            // A group: a small "+" in the corner; a tap opens the variants.
            ui_.fill(x + kSlot - 9, y + 1, 8, 8, {0, 0, 0, 0.6f});
            ui_.text(x + kSlot - 8, y + 1, "+", kAccent, 1, false);
            if (ui_.held(x, y, kSlot, kSlot)) creativeHint_ = creativeGroup(id);
            if (ui_.tapped(x, y, kSlot, kSlot)) {
                creativeGroup_ = creativeGroup(id);
                creativeScroll_ = 0;
            }
            continue;
        }
        if (ui_.held(x, y, kSlot, kSlot)) creativeHint_ = itemName(id);
        if (ui_.tapped(x, y, kSlot, kSlot)) {
            // A full stack into the selected hotbar slot.
            inv.slots[selected] = {id, (uint8_t)maxStack(id), 0};
            creativeHint_ = itemName(id);
        }
    }
    if (rows * kSlot > gridH) ui_.icon(gx + cols * kSlot + 2, gy + gridH - 8, Font::ArrowDown, kLabel);
    // Hotbar and a bin for the selected slot.
    const float hy = py + ph - kSlot - 8;
    for (int i = 0; i < kHotbarSize; ++i) {
        float x = gx + i * kSlot;
        ui_.slotFrame(x, hy, kSlot, i == selected);
        drawStack(x, hy, inv.slots[i]);
        if (ui_.tapped(x, hy, kSlot, kSlot)) selected = i;
    }
    if (ui_.button(gx + kHotbarSize * kSlot + 6, hy, 76, kSlot, "Vyprázdnit", !inv.slots[selected].empty()))
        inv.slots[selected] = {};
    if (!creativeHint_.empty()) ui_.text(gx, hy - 11, creativeHint_, kLabel, 1, false, figX - gx);
}

}  // namespace vc
