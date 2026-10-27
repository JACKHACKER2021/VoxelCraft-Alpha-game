#pragma once
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "Audio.h"
#include "Boats.h"
#include "Mobs.h"
#include "Trees.h"
#include "Drops.h"
#include "FirstPerson.h"
#include "Loot.h"
#include "Items.h"
#include "Minimap.h"
#include "Particles.h"
#include "Player.h"
#include "Renderer.h"
#include "Save.h"
#include "Sky.h"
#include "Structure.h"
#include "Ui.h"
#include "World.h"
#include "Blocks.h"

namespace vc {

// Another player in a LAN game, as drawn here (see Multiplayer).
struct RemoteAvatar {
    std::string name;
    PlayerPose pose;
    int dim = 0;
    int skinSlot = -1;  // Renderer::setRemoteSkin slot, -1 = the default skin
};

// One world being played: both dimensions, the player, survival stats, the
// in-game touch controls and the HUD. All coordinates are GUI pixels.
class Session {
public:
    // Opens a saved world, or starts a fresh one when `player.valid` is false.
    Session(const WorldInfo& info, const PlayerState& player);

    void save();

    void touchDown(int id, float x, float y, double time);
    void touchMove(int id, float x, float y);
    void touchUp(int id, float x, float y, double time);
    void cancelTouches();

    void update(float dt, double time, float guiW, float guiH, const Settings& settings);
    // Draws the world and (optionally) the HUD.
    void render(Renderer& renderer, Ui& ui, const Settings& settings, bool hud);

    World& currentWorld() { return *world_; }
    const WorldInfo& info() const { return info_; }
    GameMode mode() const { return info_.mode; }
    bool survival() const { return info_.mode == GameMode::Survival; }
    Inventory& inventory() { return state_.inventory; }
    std::array<ItemStack, 4>& armor() { return state_.armor; }
    ItemStack& offhand() { return state_.offhand; }  // 1.5.7: the off hand (shield, torch...)
    void swapOffhand() { std::swap(state_.offhand, state_.inventory.slots[state_.selectedSlot]); }
    int armorPoints() const;
    // Puts on the armour piece from the selected slot (swapping with what is worn).
    bool equipArmor(int slot);
    int& selectedSlot() { return state_.selectedSlot; }
    bool dead() const { return dead_; }
    Player& player() { return player_; }
    const PlayerState& stats() const { return state_; }
    const ParticleSystem& particles() const { return particles_; }
    void respawn();
    // LAN games (see Multiplayer).
    PlayerPose pose() const;            // how this player looks to the others
    int dimension() const { return dimIndex(); }
    void recordEdits(bool on);          // collect changed blocks for takeEdits
    std::vector<BlockEdit> takeEdits(int dim);
    void applyRemoteBlock(int dim, int x, int y, int z, ItemId id);  // not recorded again
    void saveWorlds();                  // changed chunks to disk, so they can be sent
    std::vector<RemoteAvatar> remotePlayers;
    // Developer actions
    void setMode(GameMode mode) { info_.mode = mode; }
    void healAndFeed();
    void teleportToSpawn();
    // Chat commands
    void teleport(const Vec3& pos);
    void kill();
    // Time of day in ticks (see Sky.h); setting it keeps the day count.
    double timeOfDay() const { return state_.dayTime; }
    void setTimeOfDay(float ticks) {
        double day = std::floor(state_.dayTime / 24000.0);
        state_.dayTime = day * 24000.0 + ticks;
    }
    bool raining() const { return state_.raining; }
    void setWeather(bool raining);
    Precipitation precipitation() const;  // what falls where the player stands

    // Short vibration in milliseconds (when a block breaks and vibration is on).
    std::function<void(int)> onVibrate;
    std::function<void(Sound)> onSound;
    // A short message for the player (already translated), e.g. "You can only sleep at night".
    std::function<void(const std::string&)> onMessage;
    // A block was broken in survival (the profile turns these into coins).
    std::function<void()> onBlockMined;
    // An achievement was earned (survival only), see Achievements.h.
    std::function<void(const std::string& id)> onAchievement;

    // Requests raised by HUD buttons and by tapping blocks, cleared by the caller.
    // Movement that is not from the touch screen (demos and the trailer tool).
    struct Autopilot {
        float moveX = 0, moveZ = 0;
        bool jump = false;
    } autopilot;

    bool wantsPause = false;
    bool wantsChat = false;
    bool wantsInventory = false;
    bool wantsCrafting = false;  // tapped a crafting table
    bool wantsFurnace = false;   // tapped a furnace, see openFurnace
    bool wantsChest = false;     // tapped a chest, see openChest
    bool travelled = false;      // went through a portal (the game shows the loading screen)
    int wantsTrade = -1;         // a villager was tapped: index into mobs() for the trading screen
    int wantsDocument = -1;      // 1.5.0: an old document was read: its page (0..7) for the reading screen
    void readDocument(int page);
    FurnaceKey openChest;
    ChestContents* chest(const FurnaceKey& key);
    // Multiplayer: the chest's contents (structure loot is decided now if it was never opened),
    // and replacing a chest's or furnace's contents with what another player sees.
    ChestContents& chestFor(const FurnaceKey& key);
    Furnace& furnaceFor(const FurnaceKey& key);
    void setChest(const FurnaceKey& key, const ChestContents& c) { chests_[key] = c; }
    void setFurnace(const FurnaceKey& key, const Furnace& f) { furnaces_[key] = f; }
    Vec3 spawnPoint() const { return state_.spawn; }
    int dimIndex() const { return world_ == &nether_ ? 1 : world_ == &void_ ? 2 : 0; }
    World& worldAt(int dim) { return dim == 1 ? nether_ : dim == 2 ? void_ : overworld_; }
    // Opens the chest at the key; a structure chest is filled with its loot the first time.
    void openChestAt(const FurnaceKey& key);
    // Doors and trapdoors: opens or closes the one at (x, y, z) (both halves of a door).
    bool toggleDoor(int x, int y, int z);
    FurnaceKey openFurnace;
    Furnace* furnace(const FurnaceKey& key);
    // Opens (creating if needed) the furnace at `key`: sets openFurnace and wantsFurnace.
    void openFurnaceAt(const FurnaceKey& key);
    // Smelting runs even while a menu is open.
    void updateFurnaces(float dt);

    // Puts items into the inventory; what does not fit drops at the player's feet
    // (and is picked up again once there is room), so nothing is lost.
    void giveItem(const ItemStack& stack);
    // Throws items forward out of the inventory (the "Vyhodit" button).
    void throwItem(const ItemStack& stack);
    const DropSystem& drops() const { return drops_; }
    const HandAnimator& hand() const { return hand_; }
    const BoatSystem& boats() const { return boats_; }
    MobSystem& mobs() { return mobs_; }
    void summon(MobType type);  // in front of the player (commands, tests)
    // An explosion like TNT's (radius in blocks); public for commands and tests.
    void explodeAt(const Vec3& center, float radius) { explode(center, radius, radius > 6); }

    // 1.4.0 multiplayer: set by the game while the world is shared.
    std::function<bool(int x, int y, int z)> canEdit;  // spawn protection (null: anything goes)
    bool mobsFromHost = false;                          // a guest shows the host's mobs instead of its own
    std::function<void(uint32_t uid, float damage)> onHitRemoteMob;
    // The host: a guest's kill (killer = player id) gives the drops to that guest.
    std::function<void(int killer, const std::vector<std::pair<ItemId, int>>& drops)> onRemoteKill;
    bool pvp = false;
    int blocksPlaced = 0;  // this session (the tutorial watches it)
    std::function<int(const Vec3& eye, const Vec3& dir, float maxDist, float& dist)> pickRemotePlayer;
    std::function<void(int player, float damage)> onHitPlayer;
    void hurtByPlayer(float damage, const Vec3& knock);
    float heldDamage() const;
    size_t primedTnt() const { return tnt_.size(); }
    size_t growingSaplings() const { return saplings_.size(); }
    // 1.3.0: arrows in flight (or stuck in blocks) and the bow's draw (0..1).
    struct Arrow {
        int dim;
        Vec3 pos, vel;
        float life;
        bool stuck;
    };
    const std::vector<Arrow>& arrows() const { return arrows_; }
    float bowCharge() const { return bowCharge_; }
    void shootArrow(const Vec3& dir, float power);
    bool riding() const { return riding_ >= 0; }
    void exitBoat();
    const Minimap& minimap() const { return minimap_; }
    const PaperMaps& paperMaps() const { return paperMaps_; }
    bool showingPaperMap() const { return paperShown_; }
    std::vector<MapMarker> mapMarkers() const;

    // VCStructures (creative): places `s` in front of the player, on the face the
    // crosshair points at or two blocks ahead, never overlapping the player. The
    // structure is turned so that its z = 0 side faces the player.
    bool placeStructureAhead(const Structure& s, std::string& message);
    // Reverts the last placement in the current dimension.
    bool undoStructure();
    bool canUndoStructure() const;

private:
    struct Touch {
        int id = -1;
        enum Kind { None, DPad, Look, Jump, FlyDown, Hotbar, InventoryButton, Pause, Chat, Map, Sneak } kind = None;
        float startX = 0, startY = 0, x = 0, y = 0, lastX = 0, lastY = 0;
        double startTime = 0, lastBreak = 0;
        bool moved = false;
    };
    struct Layout {
        float slot, hotbarX, hotbarY;
        float jumpX, jumpY, flyDownX, buttonSize;
        float pauseX, pauseY, pauseSize;
        float chatX;  // chat button, left of the pause button (same size)
        float padX, padY, padButton;  // D-pad: 3x3 grid of padButton-sized cells
        float mapX, mapY, mapSize;    // minimap, top right (mapSize 0 when hidden)
    };
    // D-pad direction under a point: -1/0/1 for x (left/right) and z (back/forward).
    bool dpadDirection(const Layout& l, float x, float y, int& dx, int& dz) const;

    Layout layout() const;
    Touch* findTouch(int id);
    Vec3 rayThroughScreen(float sx, float sy) const;
    RayHit pick(float sx, float sy) const;
    void selectSlot(int slot);
    void useItem(const RayHit& hit);
    void breakBlock(const RayHit& hit);
    void dropAt(int x, int y, int z, const ItemStack& stack);
    void placeShape(const RayHit& hit, ItemStack& held, int x, int y, int z);  // stairs and slabs
    bool placeSpecial(const RayHit& hit, ItemStack& held, int x, int y, int z);  // doors, trapdoors, cactus
    void updateMining(float dt, double time);
    // 1.5.0 archaeology: brushing debris gravel takes this long (half in creative).
    static constexpr float kBrushTime = 2.0f;
    float brushProgress_ = 0;
    void uncoverFind(int x, int y, int z);
    void updateSurvival(float dt, bool jumped);
    // `armorHelps`: falls and lava are softened by armour (which wears down), drowning and hunger are not.
    void hurt(float amount, bool ignoreCooldown = false, bool armorHelps = false);
    void addExhaustion(float amount) { exhaustion_ += amount; }
    void updatePortal(float dt);
    void travelThroughPortal();
    void findSpawn();
    void drawHud(Ui& ui) const;
    void updateCamera(float dt, bool walking);
    void sound(Sound s) const { if (onSound) onSound(s); }
    void damageHeldTool();

    WorldInfo info_;
    PlayerState state_;
    std::vector<BlockEdit> edits_[3];
    GenConfig config_;  // must come before the worlds that copy it
    World overworld_, nether_, void_;
    World* world_;
    Player player_;
    Touch touches_[10];
    std::map<FurnaceKey, Furnace> furnaces_;
    std::map<FurnaceKey, ChestContents> chests_;
    float guiW_ = 1, guiH_ = 1, fov_ = 70, sensitivity_ = 1;
    Settings settings_;
    Player camera_;          // what the renderer looks through (third person, view bobbing)
    float cameraDistance_ = 0;
    float walkPhase_ = 0, swing_ = 0, stepDistance_ = 0;
    ParticleSystem particles_;
    DropSystem drops_;
    std::vector<Vertex> dropVerts_;
    float clock_ = 0;  // seconds since the world was opened (animations)
    HandAnimator hand_;
    std::vector<Vertex> handSkin_, handAtlas_;
    bool miningNow_ = false;  // really digging this frame (drives the hand's strokes)
    std::vector<Vertex> particleVerts_;
    float chipTimer_ = 0;
    float fps_ = 60;
    float achievementTimer_ = 0;
    void achieve(const char* id) const { if (survival() && onAchievement) onAchievement(id); }
    void checkAchievements();
    BoatSystem boats_;
    MobSystem mobs_;
    // 1.2.0: lit TNT, growing saplings, leaves about to fall.
    struct PrimedTnt {
        int dim;
        Vec3 pos, vel;
        float fuse;
        bool nuke;
    };
    std::vector<PrimedTnt> tnt_;
    struct Timed {
        int dim, x, y, z;
        float time;
    };
    std::vector<Timed> saplings_, fallingLeaves_;
    // 1.5.7: sand and gravel fall; with the Flowing Liquids experiment water and lava spread.
    // Flowing (non-source) liquid cells remember how far they are from their source.
    std::vector<std::array<int, 3>> changes_[3];
    std::vector<Timed> blockTicks_;
    std::unordered_set<uint64_t> ticking_[3];
    std::unordered_map<uint64_t, uint8_t> flow_[3];
    void updateBlockPhysics(float dt);
    void tickLiquid(int dim, int x, int y, int z);
    void saveFlow() const;
    void loadFlow();
    std::vector<Vertex> tntVerts_, arrowVerts_;
    std::vector<Arrow> arrows_;
    float bowCharge_ = 0;
    void updateArrows(float dt);
    // 1.5.0: fireballs spat by Huskings.
    struct Fireball {
        int dim;
        Vec3 pos, vel;
        float life, damage;
    };
    std::vector<Fireball> fireballs_;
    std::vector<Vertex> fireballVerts_;
    void updateFireballs(float dt);
public:
    size_t fireballCount() const { return fireballs_.size(); }
private:
    bool tillOrHarvest(const RayHit& hit, ItemStack& held);
    void igniteTnt(int x, int y, int z, float fuse);
    void explode(const Vec3& center, float radius, bool nuke);
    void spreadTar(const Vec3& where);  // the tar monster killed something there
    void spawnInCave();
    void updateShowcase();
    void updateSnowCover(float dt, bool snowing);
    float snowTimer_ = 0;  // Anniversary modpack: builds the showcase once
    void updateBlocksOverTime(float dt);
    void scheduleLeafDecay(int x, int y, int z);
    void powerChanged(int x, int y, int z);
    bool useBucket(ItemStack& held);
    void saveTimers() const;
    void loadTimers();
    ChestContents structureLoot(ItemId block, uint32_t seed) const;  // generateLoot plus experiment extras
    bool shieldBlocks(const Vec3& push);  // 1.5.7: true when a raised shield stopped the hit
    std::vector<Vertex> shadowCasters_;  // 1.5.7: models that cast sun shadows
    std::vector<Vertex> mobVerts_;
    double protectedToast_ = -10;  // when "the spawn is protected" was last shown
    bool tarWarned_ = false;  // "the tar spreads" was shown once
    float spawnTimer_ = 0, attackCooldown_ = 0;
    uint32_t spawnRng_ = 12345;
    float spawnRandom();
    void updateMobs(float dt);
    float confettiTimer_ = 0;
    int pickMob(float sx, float sy) const;
    void attackMob(int index);
    int pickOtherPlayer(float sx, float sy) const;
    void attackPlayer(int who);
    void protectedSpot();
    int riding_ = -1;  // index of the boat the player sits in
    std::vector<Vertex> boatVerts_, playerExtras_;
    int pickBoat(float sx, float sy) const;  // boat under the finger, closer than any block
    bool placeBoat();
    float dayLight_ = 1;  // how bright the sky makes the world
    float rain_ = 0, skyRain_ = 0;  // 0..1, fading in and out
    std::vector<Vertex> sunMoonVerts_, starVerts_, cloudVerts_, weatherVerts_;
    void updateWeather(float dt);
    void sleepInBed(int x, int y, int z);
    bool monstersNear(float radius) const;
    float sleepTimer_ = 0;  // > 0 while sleeping: the screen fades out, then it is morning
public:
    bool sleeping() const { return sleepTimer_ > 0; }
private:
    Minimap minimap_;
    PaperMaps paperMaps_;       // the map item
    bool paperShown_ = false;   // the map texture holds a paper sheet (map in hand)
    int paperOriginX_ = 0, paperOriginZ_ = 0;
    bool mapChanged_ = false, mapBig_ = false;
    struct Placement {
        World* world;
        std::vector<BlockChange> changes;
    };
    std::vector<Placement> structureUndo_;  // newest last, at most kMaxUndo
    static constexpr size_t kMaxUndo = 10;

    RayHit target_;
    float breakProgress_ = 0;
    double lastJumpTap_ = -1;
    float portalTime_ = 0;
    bool portalCooldown_ = false;
    bool portalVoid_ = false;
    float voidFog_[3] = {0.16f, 0.05f, 0.24f};  // standing in a void portal (Nether <-> Crimson Void)
    bool dead_ = false;
    float hurtCooldown_ = 0, hurtFlash_ = 0;
    float exhaustion_ = 0, regenTimer_ = 0, starveTimer_ = 0, drownTimer_ = 0;
    float autosaveTimer_ = 0, itemNameTimer_ = 0;
    uint32_t rng_ = 0x12345678;
};

}  // namespace vc
