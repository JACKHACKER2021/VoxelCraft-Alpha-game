#pragma once
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Font.h"
#include "Multiplayer.h"
#include "Net.h"
#include "Packs.h"
#include "Profile.h"
#include "Skin.h"
#include "Store.h"
#include "Structure.h"
#include "Player.h"
#include "Renderer.h"
#include "Save.h"
#include "Session.h"
#include "Ui.h"
#include "World.h"

namespace vc {

enum class TouchAction { Down, Move, Up };

// Platform-independent top level: menus, settings and the running Session.
class Game {
public:
    // dataDir holds worlds/, options.txt and texturepack/.
    explicit Game(const std::string& dataDir);
    ~Game();

    // Called whenever a GL context and surface are (re)created.
    bool onSurfaceCreated();
    void onSurfaceDestroyed();
    void onResize(int width, int height);
    void onTouch(int pointerId, TouchAction action, float x, float y, double time);
    void frame(double time);
    // The app goes to the background: save and pause.
    void onAppPause();
    // System back gesture. Returns false when the app should close.
    bool onBack();

    // Thread-safe: the atlas is rebuilt on the next frame.
    void requestTextureReload() { textureReload_ = true; }
    // Thread-safe: a pack was imported, rescan packs on the next frame.
    void requestPacksReload() { packsReload_ = true; }
    // Thread-safe: answer to onTextPrompt.
    void onTextResult(int requestId, const std::string& text);

    Session* session() { return session_.get(); }  // the open world, if any
    // The VoxelSoftworks intro before the menu; tests and tools skip it.
    void skipSplash() { splashTime_ = kSplashLength; }
    // For tools (the trailer): open a world straight away and reach the profile and settings.
    void openWorld(const WorldInfo& info, const PlayerState& player) { startSession(info, player); }
    Profile& toolProfile() { return profile_; }
    Settings& toolSettings() { return settings_; }
    PackManager& toolPacks() { return packs_; }
    void toolRebuildPacks() { rebuildFromPacks(); }
    void toolApplySkin() { applySkin(); }
    void toolOpenCredits() { setScreen(Screen::Credits); }

    // Platform hooks, all called on the game thread.
    std::function<void()> onTexturePackButton;
    std::function<void(int requestId, const std::string& title, const std::string& initial)> onTextPrompt;
    std::function<void(int requestId, const std::string& title, const std::string& text)> onEditText;  // multi-line
    std::function<void()> onImportPack;
    std::function<void(int ms)> onVibrate;
    std::function<void()> onQuit;
    std::function<void(const std::string& sound, float volume)> onPlaySound;
    std::function<void(float volume)> onMusicVolume;
    // Plays a music track ("asset:music/x.mp3" or a file path, "" = silence), crossfading.
    std::function<void(const std::string& source, bool loop)> onPlayMusic;
    // 1.5.7: plays a video from the app's assets full screen (the trailer gallery).
    std::function<void(const std::string& asset)> onPlayVideo;
    // 1.5.7 battery saver: the phone's battery (percent, -1 unknown) and whether it charges.
    void onBattery(int percent, bool charging) { batteryLevel_ = percent; charging_ = charging; }
    bool saverActive() const;
    bool wantsLowFps() const { return saverActive(); }  // 30 frames a second instead of 60
    std::function<void()> onImportSkin;
    std::function<void()> onImportStructure;
    std::function<void()> onImportWorld;                                      // pick a .vcworld
    std::function<void(const std::string& file, const std::string& name)> onExportWorld;  // save a .vcworld somewhere
    std::function<void(int requestId, const std::string& title)> onPasswordPrompt;  // masked input
    // LAN games are being hosted or looked for (Android keeps a Wi-Fi multicast lock meanwhile).
    std::function<void(bool active)> onLanActive;
    std::function<void(const std::string& code)> onLanguage;  // the platform's own dialogs follow the game language  // file picker; answers with PromptStructureImported

    const Profile& profile() const { return profile_; }
    // What plays now (for tests): the source last given to onPlayMusic.
    const std::string& musicSource() const { return musicSource_; }

private:
    enum class Screen {
        Title, WorldList, CreateWorld, EditWorld, ConfirmDelete, Settings, Packs, PackFiles, JsonEditor, Store, Skins,
        Playing, Pause, Structures, Developer, Achievements, Joining, Loading, Inventory, Crafting, Furnace, Chest, Death, Trade,
        Invite, OfflineGuide,  // 1.4.0
        Document,              // 1.5.0: reading a page of the old documents
        Credits, Trailers,     // 1.5.7: the credits (tap VoxelSoftworks on the title) and the trailer gallery
        WorldMap, Chat,        // 1.5.7: the big map of explored land, the chat window
        Book                   // 1.5.7: book and quill
    };

    // A slot in any open container, so items can be moved between them.
    enum class SlotArea { Inventory, Grid, Result, FurnaceInput, FurnaceFuel, FurnaceOutput, Armor, Chest, Offhand };
    struct SlotRef {
        SlotArea area = SlotArea::Inventory;
        int index = -1;
        bool valid() const { return index >= 0; }
        bool operator==(const SlotRef& o) const { return area == o.area && index == o.index; }
    };
    enum PromptId { PromptWorldName = 1, PromptSeed = 2, PromptJson = 3, PromptPackName = 4, PromptPackImported = 5,
                    PromptPlayerName = 6, PromptSkinImported = 7, PromptEditWorldName = 8,
                    PromptStructureImported = 9, PromptDevName = 10, PromptDevPassword = 11, PromptMusicEnded = 12,
                    PromptJoinAddress = 13, PromptChat = 14, PromptCode = 15, PromptPublicIp = 16, PromptWorldImported = 17,
                    PromptShaderPackName = 18, PromptWaypoint = 19, PromptBookPage = 20, PromptBookTitle = 21 };

    void setScreen(Screen s);
    Settings playSettings() const;  // settings_ without developer options for normal players
    void playSound(Sound s);
    void loadSkins();
    void applySkin();
    int skinIndex(const std::string& id) const;
    // Draws a skin from the front (or back) as a flat figure 16 x 32 skin pixels times `scale`.
    void drawSkinFigure(float x, float y, float scale, int skin, bool back);
    void storeScreen();
    void splashScreen(float dt);
    static constexpr float kSplashLength = 3.4f;
    float splashTime_ = 0;  // seconds of the intro shown so far
    int splashLanded_ = 0;  // blocks that have dropped into place
    void rewardsPage(float x, float y, float w, float h);
    CraftResult craftResult(int size) const;  // the grid's recipe, if its addon is on
    void skinsScreen();
    void editWorldScreen();
    void coinsPill(float right, float y);
    void applyGuiScale();
    void rebuildFromPacks();
    void openEditor(const std::string& pack, const std::string& path);
    void editorApply(const std::string& text, bool save);
    // Full-screen window of the retro theme; returns the content area below the logo.
    void menuWindow(float& x, float& y, float& w, float& h, bool logo);
    std::vector<std::string> wrapText(const std::string& text, float width) const;
    void loadTextures();
    void drawWorldOnce(World& world);
    void updateMenuBackground(float dt);
    void startSession(const WorldInfo& info, const PlayerState& player);
    void endSession();
    void refreshWorldList();

    void titleScreen();
    void worldListScreen();
    void createWorldScreen();
    void confirmDeleteScreen();
    void settingsScreen();
    void packsScreen();
    void packFilesScreen();
    void jsonEditorScreen();
    void pauseScreen();
    void tutorial(float dt);   // 1.4.0 first-steps hints while playing
    static constexpr int kTutorialSteps = 6;
    int tutorialMined_ = 0;
    float tutorialYaw_ = 0, tutorialLook_ = 0, tutorialWalk_ = 0;
    void inviteScreen();        // host: room codes, PvP and spawn protection
    void offlineGuideScreen();  // how to play with a hotspot, without Wi-Fi or data
    void documentScreen();      // 1.5.0: a page of the forgotten kingdom's story
    void creditsScreen();       // 1.5.7
    void trailersScreen();      // 1.5.7
    void worldMapScreen();
    void chatScreen();
    void bookScreen();
    int bookId_ = 0, bookPage_ = 0;
    float mapCx_ = 0, mapCz_ = 0, mapScale_ = 1;
    int mapSelected_ = -1;
    bool mapDirty_ = true;
    std::vector<uint8_t> mapImage_;
    float chatScroll_ = 0;
    double creditsStart_ = 0;
    int documentPage_ = 0;
    void inventoryScreen();
    void creativeInventoryScreen();
    void survivalInventoryScreen();
    void craftingScreen();
    void furnaceScreen();
    void chestScreen();
    void tradeScreen();
    int tradeMob_ = -1;
    int tradeProfession_ = 0;
    bool containerWindow(const std::string& title, float contentH, float& gx, float& gy);
    void playerSlots(float gx, float gy);
    void slot(float x, float y, SlotRef ref);
    // 1.4.0 recipe book beside the crafting grid.
    struct BookEntry {
        CraftResult result;
        std::vector<ItemId> cells;  // shaped: w*h row-major; shapeless: the ingredients
        int width = -1;             // -1 for shapeless
        bool canMake = false;
    };
    std::vector<BookEntry> bookEntries(int gridSize) const;
    void fillFromBook(const BookEntry& e);
    void recipeBookPanel(float x, float y, float w, float h);
    bool recipeBook_ = false;
    float bookScroll_ = 0;
    void tapSlot(SlotRef ref);
    ItemStack* stackAt(SlotRef ref);
    void drawStack(float x, float y, const ItemStack& s);
    void craftOnce();
    void returnCraftingGrid();
    void drawArrow(float x, float y, float progress);
    void deathScreen();
    void structuresScreen();
    void developerScreen();
    void achievementsScreen();
    void joiningScreen();
    // Chat: a line typed by the player (a message, or a command starting with '/').
    void submitChat(const std::string& text);
    void addChat(const std::string& line);
    std::string playerName() const;  // as others see it in chat and LAN games
    void drawChat();
    // Loading world / building terrain / travelling between dimensions, until the chunks around the player are ready.
    void startLoading(const std::string& title, bool dimension);
    void loadingScreen(float dt);
    // LAN: open the running world to others, or join someone's game.
    void openToLan();
    void joinLan(const std::string& address, int port, const std::string& label);
    void updateLan(float dt);
    void setLanActive(bool active);
    const Skin* currentSkin() const;
    void unlockAchievement(const std::string& id);
    void updateMusic(float dt);
    std::string findMusic(const std::string& name) const;  // the player's own track, if any
    void refreshStructures();

    std::string dataDir_, worldsDir_, settingsPath_, texturePackDir_;
    Font font_;
    Ui ui_;
    Renderer renderer_;
    Settings settings_;
    PackManager packs_;
    Profile profile_;
    std::string profilePath_, skinsDir_, soundsDir_;
    std::vector<Skin> skins_;
    int skinAtlasCols_ = 8;
    bool musicStarted_ = false;
    std::string internetCode_;  // a room code made from the public address the host typed
    std::string toast_;   // short message on the title / store screens
    double toastUntil_ = 0;

    // Store
    int storeCategory_ = 0;
    float storeScroll_ = 0;
    int pendingBuy_ = -1;
    // The wheel of fortune turns from wheelFrom_ to wheelTo_ (radians), started at wheelStart_.
    float wheelAngle_ = 0, wheelFrom_ = 0, wheelTo_ = 0;
    double wheelStart_ = 0;
    std::string wheelMessage_;  // the prize, told when the wheel stops (empty: not turning)

    // Skins screen
    int skinSelected_ = 0;
    bool skinBack_ = false;

    // VCStructures: built-in ones, then <dataDir>/structures, then structures/ of enabled packs.
    struct StructureEntry {
        Structure data;
        ImportReport report;
        std::string source;  // "vestavěná", "importovaná" or the pack id
        std::string file;    // empty for built-in structures
    };
    std::string structuresDir_;
    std::vector<StructureEntry> structures_;
    std::vector<std::string> structureErrors_;  // files that failed to load
    int structureSelected_ = 0, structureRotation_ = 0;
    bool structureMirror_ = false;
    std::string structureStatus_;

    // Editing an existing world
    WorldInfo editInfo_;
    PlayerState editPlayer_;
    GenConfig editGen_;
    std::unique_ptr<Session> session_;
    const World* meshedWorld_ = nullptr;  // whose chunks the renderer currently holds

    // Rotating world behind the title screen.
    std::unique_ptr<World> menuWorld_;
    Player menuCamera_;

    Screen screen_ = Screen::Title;
    Screen settingsReturn_ = Screen::Title;
    bool hasSurface_ = false;
    int width_ = 1, height_ = 1;
    float guiScale_ = 1;
    double lastTime_ = 0, now_ = 0;

    std::vector<WorldInfo> worlds_;
    float worldScroll_ = 0;
    int playTab_ = 0;           // the Play screen: 0 Singleplayer, 1 Multiplayer
    bool hostAfterLoad_ = false;  // open the world to the LAN once it has loaded
    int deleteIndex_ = -1;
    std::string newWorldName_, newWorldSeed_;
    GameMode newWorldMode_ = GameMode::Creative;
    bool newWorldFlat_ = false, newWorldCoords_ = false, newWorldKeepInventory_ = false, newWorldCommands_ = false;
    bool newWorldExpLiquids_ = false, newWorldExpMaps_ = false, newWorldExpOpen_ = false;  // 1.5.1 Experimental Features
    int settingsTab_ = 0;

    // Packs screen and JSON editor
    std::vector<PackInfo> packList_;
    std::vector<std::string> packWarnings_, packFiles_;
    int packSelected_ = 0;
    std::string filesPack_;
    float listScroll_ = 0;
    std::string editorPack_, editorPath_, editorText_, editorStatus_;
    bool editorValid_ = true, editorSaved_ = true;
    float editorScroll_ = 0;
    SlotRef held_;  // slot picked up for moving (tap one slot, then another)
    int heldAmount_ = 0;  // how many of the held stack move with the next tap (split stacks)
    std::string slotHint_;
    void selectionPanel(float x, float y, float w, float h);
    std::array<ItemStack, 9> grid_{};  // crafting grid, row-major, gridSize_ x gridSize_
    int gridSize_ = 2;
    int creativeTab_ = 0;
    bool texturesWinter_ = false;  // the atlas has the Winter addon's snow
    // The ore tiles of the loaded atlas (16x16 each), put back when a world without the
    // Ore Redesign experiment is played.
    std::vector<uint8_t> oreBase_;
    void applyOreRedesign(bool on);
    // 1.5.5: the world shader follows the graphics setting and the active shader pack.
    bool shadersDirty_ = true;
    ShaderProfile shaderProfile_;
    GraphicsMode appliedGraphics_ = GraphicsMode::Enhanced;
    std::atomic<int> batteryLevel_{-1};
    std::atomic<bool> charging_{false};
    bool saverWasOn_ = false;
    void applyShaders();
    int splash_ = 0;
    bool languageSent_ = false;
    std::string achievementBanner_;  // shown over the game for a few seconds
    int achievementCoins_ = 0;
    double achievementUntil_ = 0;
    Screen achievementsReturn_ = Screen::Settings;
    // Music: the context decides the playlist (menu theme, overworld + biome, Nether).
    std::string musicContext_, musicSource_;
    float musicGap_ = 0;       // silence before the next game track
    bool musicEnded_ = false;
    // Developer login
    std::string devLoginName_;
    int devFailures_ = 0;
    double devLockedUntil_ = 0;
    Screen devReturn_ = Screen::Settings;
    uint32_t splashSeed_ = 12345;
    float creativeScroll_ = 0;
    std::string creativeGroup_;  // 1.5.9: the open group of variants (empty: the whole category)
    std::string creativeHint_;

    // Loading screen
    std::string loadingTitle_;
    bool loadingDimension_ = false;  // portal travel: purple swirl instead of dirt
    float loadingTime_ = 0, loadingProgress_ = 0;
    int loadingTip_ = 0;

    // LAN games
    Multiplayer lan_;
    net::Discovery discovery_;
    bool lanActive_ = false;
    std::string joinLabel_;
    // Chat lines (messages, command replies, joins and departures), newest last.
    struct ChatLine {
        std::string text;
        double time;
    };
    std::vector<ChatLine> chat_;

    std::atomic<bool> textureReload_{false}, packsReload_{false};
    std::mutex textMutex_;
    std::vector<std::pair<int, std::string>> textResults_;
};

}  // namespace vc
