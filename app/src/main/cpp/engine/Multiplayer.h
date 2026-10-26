#pragma once
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Net.h"
#include "PlayerModel.h"
#include "Save.h"
#include "Blocks.h"

namespace vc {

class Session;

// Playing one world together over the local network, a phone's hotspot or the internet
// (a room code is just the host's address). The host's game is the authority: a joining
// player downloads the seed, generator.json and every changed chunk, then block changes
// and player poses flow both ways. Since 1.4.0 the host's animals and monsters are shared
// (guests see and hit the same mobs), so are chests and furnaces, and the host decides
// about PvP and the protected area around the spawn.
class Multiplayer {
public:
    static constexpr int kProtocol = 5;  // 1.5.0: Huskings and the forgotten kingdom's blocks (1.4.0 was 4)
    static constexpr int kMaxPlayers = 8;

    enum class Role { None, Host, Client };
    enum class JoinState { Idle, Connecting, Downloading, Ready, Failed };

    // Someone else in the game, as drawn in this one.
    struct Player {
        int id = 0;
        std::string name;
        bool slim = false;
        std::vector<uint8_t> skin;  // 64x64 RGBA
        int skinSlot = 0;           // renderer slot (see Renderer::setRemoteSkin)
        bool skinDirty = true;      // the renderer has not got this skin yet
        int dim = 0;
        Vec3 pos, target;
        float yaw = 0, pitch = 0, targetYaw = 0, phase = 0, swing = 0;
        float attack = 1, place = 1;
        ItemId held = 0, armor[4] = {};
        bool riding = false;
        bool posed = false;  // got at least one pose
    };

    // The host's rules for everyone.
    struct Rules {
        bool pvp = false;
        bool protectSpawn = true;
        int radius = 8;          // blocks around the spawn only the host may change
        float spawnX = 0, spawnZ = 0;
        bool protects(int x, int z) const {
            return protectSpawn && std::fabs(x + 0.5f - spawnX) <= radius && std::fabs(z + 0.5f - spawnZ) <= radius;
        }
    };

    // What a joining player learns about the world.
    struct Welcome {
        int yourId = 0;
        std::string host, world, generator;
        uint32_t seed = 0;
        GameMode mode = GameMode::Survival;
    };

    ~Multiplayer() { stop(); }

    // Opens the session's world to the LAN. `name` and `skin` are the host's.
    bool host(Session& session, const std::string& worldName, const std::string& name,
              const std::vector<uint8_t>& skin, bool slim, std::string& error);
    // Starts joining a game; follow joinState() while update(nullptr, ...) runs.
    bool join(const std::string& address, int port, const std::string& name, const std::vector<uint8_t>& skin, bool slim,
              std::string& error);
    // Writes the downloaded world into `folder` (generator.json and chunk files), keeping level.dat.
    void writeWorld(const std::string& folder) const;
    void stop();

    // Every frame. `session` is the open world (null while a client downloads).
    void update(Session* session, float dt, double now);

    Role role() const { return role_; }
    bool active() const { return role_ != Role::None; }
    JoinState joinState() const { return joinState_; }
    const std::string& error() const { return error_; }
    const Welcome& welcome() const { return welcome_; }
    int chunksReceived() const { return (int)chunkFiles_.size(); }
    int port() const { return listener_.port(); }
    std::vector<Player>& players() { return players_; }
    int playerCount() const { return 1 + (int)players_.size(); }
    // Sends a chat line to everyone else ("<name> text" on their screens).
    void sendChat(const std::string& text);
    // Joins ("+name"), departures ("-name") and chat lines ("#<name> text") to show in the game.
    std::vector<std::string> takeMessages() { return std::move(messages_); }
    // The connection to the host was lost (client) after the game had started.
    bool lost() const { return lost_; }

    // The host sets the rules (sent to everyone); guests learn them on joining.
    void setRules(bool pvp, bool protectSpawn);
    const Rules& rules() const { return rules_; }
    // Wires the session's multiplayer hooks (spawn protection, mobs, PvP) to this game.
    void attach(Session& session);
    // The chest (kind 0) or furnace (kind 1) open on this screen, or kind -1.
    void setOpenContainer(int kind, const FurnaceKey& key);
    // The remote player under a ray, for PvP: their id, or -1.
    int pickPlayer(int dim, const Vec3& eye, const Vec3& dir, float maxDist, float& dist) const;

private:
    enum Msg : uint8_t {
        Hello = 1, WelcomeMsg, Chunk, Ready, Join, Leave, Pose, Block, Kick, ChatMsg, WorldTime,
        // 1.4.0
        RulesMsg, MobSync, MobHit, GiveItems, ContainerGet, ContainerData, ContainerSet, PlayerHit, Hurt
    };
    struct Peer {
        std::unique_ptr<net::Connection> conn;
        int id = 0;
        bool joined = false;
        double since = 0;
    };

    void hostUpdate(Session& session, float dt, double now);
    void clientUpdate(Session* session, float dt, double now);
    void handlePose(net::Reader& r, int id);
    void applyBlock(Session* session, int dim, int x, int y, int z, ItemId id);
    net::Writer poseOf(Session& session) const;
    net::Writer joinOf(int id, const std::string& name, bool slim, const std::vector<uint8_t>& skin) const;
    Player* player(int id);
    void addPlayer(int id, const std::string& name, bool slim, const std::vector<uint8_t>& skin);
    void removePlayer(int id);
    void sendWorld(Session& session, net::Connection& c, int id);
    void broadcast(uint8_t type, const net::Writer& w, int exceptId);
    void animate(float dt);
    net::Writer mobsOf(Session& session) const;
    void applyMobs(Session& session, net::Reader& r);
    net::Writer rulesOf() const;
    static void writeContainer(net::Writer& w, Session& session, int kind, const FurnaceKey& key);
    static bool readContainer(net::Reader& r, Session& session, int& kind, FurnaceKey& key);
    void sendTo(int id, uint8_t type, const net::Writer& w);
    void syncContainer(Session& session, float dt);

    Role role_ = Role::None;
    JoinState joinState_ = JoinState::Idle;
    std::string error_;
    std::string name_, worldName_;
    std::vector<uint8_t> skin_;
    bool slim_ = false;
    // Host
    net::Listener listener_;
    net::Announcer announcer_;
    std::vector<Peer> peers_;
    int nextId_ = 1;
    float announceTimer_ = 0;
    // Client
    net::Connection server_;
    bool helloSent_ = false, lost_ = false;
    Welcome welcome_;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> chunkFiles_;  // relative path, file contents
    struct Edit { int dim, x, y, z; ItemId id; };
    std::vector<Edit> pendingEdits_;  // arrived before the session was ready
    // Both
    std::vector<Player> players_;
    std::vector<std::string> messages_;
    float poseTimer_ = 0, timeTimer_ = 0, mobTimer_ = 0;
    Rules rules_;
    Session* attached_ = nullptr;
    std::vector<std::pair<int, float>> pendingHits_;    // guest: (player id, damage) to send
    std::vector<std::pair<uint32_t, float>> mobHits_;   // guest: (mob uid, damage) to send
    std::vector<std::pair<int, std::vector<std::pair<ItemId, int>>>> gifts_;  // host: drops for guests
    int openKind_ = -1, sentKind_ = -1;
    FurnaceKey openKey_, sentKey_;
    std::vector<uint8_t> containerSnapshot_;
    float containerTimer_ = 0;
};

}  // namespace vc
