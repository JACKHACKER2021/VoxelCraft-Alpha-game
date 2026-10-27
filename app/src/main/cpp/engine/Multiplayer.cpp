#include "Multiplayer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "Items.h"
#include "Mobs.h"
#include "Mods.h"
#include "Session.h"

namespace vc {

namespace fs = std::filesystem;

namespace {

constexpr float kPoseInterval = 0.1f;  // ten poses a second
constexpr float kAnnounceInterval = 1.5f;
const char* const kDimDirs[3] = {"overworld", "nether", "void"};

std::vector<uint8_t> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Only plain chunk and region file names are accepted from the network ("c.<x>.<z>.bin",
// 1.5.7: "r.<x>.<z>.vcr").
bool safeChunkName(const std::string& n) {
    const bool chunk = n.compare(0, 2, "c.") == 0 && n.size() > 4 && n.compare(n.size() - 4, 4, ".bin") == 0;
    const bool region = n.compare(0, 2, "r.") == 0 && n.size() > 4 && n.compare(n.size() - 4, 4, ".vcr") == 0;
    if (n.size() < 7 || n.size() > 40 || !(chunk || region)) return false;
    for (char ch : n)
        if (!(std::isdigit((unsigned char)ch) || ch == '.' || ch == '-' || ch == 'c' || ch == 'b' || ch == 'i' || ch == 'n' ||
              ch == 'r' || ch == 'v'))
            return false;
    return n.find("..") == std::string::npos;
}

float wrapAngle(float a) {
    while (a > kPi) a -= 2 * kPi;
    while (a < -kPi) a += 2 * kPi;
    return a;
}

}  // namespace

// ---------------------------------------------------------------- start / stop

bool Multiplayer::host(Session& session, const std::string& worldName, const std::string& name,
                       const std::vector<uint8_t>& skin, bool slim, std::string& error) {
    stop();
    if (!listener_.open(net::kGamePort)) {
        error = "Nelze otevřít síťový port";
        return false;
    }
    role_ = Role::Host;
    name_ = name;
    worldName_ = worldName;
    skin_ = skin;
    slim_ = slim;
    nextId_ = 1;
    announceTimer_ = 0;
    session.recordEdits(true);
    return true;
}

bool Multiplayer::join(const std::string& address, int port, const std::string& name, const std::vector<uint8_t>& skin,
                       bool slim, std::string& error) {
    stop();
    if (!server_.connect(address, port, error)) return false;
    role_ = Role::Client;
    joinState_ = JoinState::Connecting;
    name_ = name;
    skin_ = skin;
    slim_ = slim;
    helloSent_ = lost_ = false;
    welcome_ = {};
    chunkFiles_.clear();
    pendingEdits_.clear();
    return true;
}

void Multiplayer::stop() {
    if (attached_) {
        // The world goes on alone: no more rules or shared mobs.
        Session& s = *attached_;
        s.canEdit = nullptr;
        s.mobsFromHost = false;
        s.onHitRemoteMob = nullptr;
        s.onRemoteKill = nullptr;
        s.pickRemotePlayer = nullptr;
        s.onHitPlayer = nullptr;
        s.pvp = false;
        attached_ = nullptr;
    }
    pendingHits_.clear();
    mobHits_.clear();
    gifts_.clear();
    openKind_ = sentKind_ = -1;
    if (role_ == Role::Host)
        for (Peer& p : peers_) {
            p.conn->send(Kick, net::Writer().str("Hostitel zavřel svět"));
            std::vector<net::Packet> ignored;
            p.conn->poll(ignored);
        }
    peers_.clear();
    listener_.close();
    server_.close();
    players_.clear();
    role_ = Role::None;
    joinState_ = JoinState::Idle;
}

void Multiplayer::writeWorld(const std::string& folder) const {
    std::error_code ec;
    // The host's world replaces what an earlier visit left here (only level.dat stays).
    for (const char* d : kDimDirs) fs::remove_all(fs::path(folder) / d, ec);
    for (const char* d : kDimDirs) fs::create_directories(fs::path(folder) / d, ec);
    std::ofstream(fs::path(folder) / "generator.json", std::ios::binary) << welcome_.generator;
    for (auto& [path, data] : chunkFiles_) {
        std::ofstream out(fs::path(folder) / path, std::ios::binary);
        out.write((const char*)data.data(), (std::streamsize)data.size());
    }
}

// ---------------------------------------------------------------- shared

Multiplayer::Player* Multiplayer::player(int id) {
    for (Player& p : players_)
        if (p.id == id) return &p;
    return nullptr;
}

void Multiplayer::addPlayer(int id, const std::string& name, bool slim, const std::vector<uint8_t>& skin) {
    removePlayer(id);
    Player p;
    p.id = id;
    p.name = name;
    p.slim = slim;
    p.skin = skin.size() == 64 * 64 * 4 ? skin : std::vector<uint8_t>{};
    // The first renderer slot nobody else uses.
    for (int s = 0; s < kMaxPlayers; ++s)
        if (std::none_of(players_.begin(), players_.end(), [&](const Player& o) { return o.skinSlot == s; })) {
            p.skinSlot = s;
            break;
        }
    players_.push_back(std::move(p));
}

void Multiplayer::removePlayer(int id) {
    players_.erase(std::remove_if(players_.begin(), players_.end(), [&](const Player& p) { return p.id == id; }),
                   players_.end());
}

net::Writer Multiplayer::joinOf(int id, const std::string& name, bool slim, const std::vector<uint8_t>& skin) const {
    net::Writer w;
    w.i32(id).str(name).u8(slim).bytes(skin);
    return w;
}

net::Writer Multiplayer::poseOf(Session& session) const {
    PlayerPose p = session.pose();
    net::Writer w;
    w.u8((uint8_t)session.dimension()).f32(p.feet.x).f32(p.feet.y).f32(p.feet.z).f32(p.yaw).f32(p.headPitch);
    w.f32(p.attack).f32(p.place).u16(p.held);
    for (ItemId a : p.armor) w.u16(a);
    w.u8(p.riding);
    return w;
}

void Multiplayer::handlePose(net::Reader& r, int id) {
    int dim = r.u8();
    Vec3 pos{r.f32(), r.f32(), r.f32()};
    float yaw = r.f32(), pitch = r.f32(), attack = r.f32(), place = r.f32();
    ItemId held = r.u16();
    ItemId armor[4];
    for (ItemId& a : armor) a = r.u16();
    bool riding = r.u8() != 0;
    Player* p = player(id);
    if (!r.ok() || !p) return;
    if (!p->posed || p->dim != dim || (pos - p->pos).length() > 16) {
        p->pos = pos;  // first pose, a portal or a teleport: jump there
        p->yaw = yaw;
    }
    p->posed = true;
    p->dim = std::clamp(dim, 0, 2);
    p->target = pos;
    p->targetYaw = yaw;
    p->pitch = std::clamp(pitch, -1.6f, 1.6f);
    p->attack = attack;
    p->place = place;
    p->held = held;
    std::copy(armor, armor + 4, p->armor);
    p->riding = riding;
}

void Multiplayer::applyBlock(Session* session, int dim, int x, int y, int z, ItemId id) {
    if (y < 0 || y >= kChunkHeight) return;
    if (!session) {
        pendingEdits_.push_back({std::clamp(dim, 0, 2), x, y, z, id});
        return;
    }
    session->applyRemoteBlock(std::clamp(dim, 0, 2), x, y, z, id);
}

void Multiplayer::animate(float dt) {
    // Others move smoothly between their poses and swing their limbs while walking.
    float k = 1 - std::exp(-dt * 12);
    for (Player& p : players_) {
        Vec3 before = p.pos;
        p.pos = p.pos + (p.target - p.pos) * k;
        p.yaw += wrapAngle(p.targetYaw - p.yaw) * k;
        Vec3 d = p.pos - before;
        float speed = dt > 0 ? std::sqrt(d.x * d.x + d.z * d.z) / dt : 0;
        p.phase += dt * std::min(speed, 6.0f) * 2.2f;
        float amp = std::min(speed / 4.3f, 1.0f) * 0.9f;
        p.swing = p.riding ? 0 : std::sin(p.phase) * amp;
    }
}

void Multiplayer::update(Session* session, float dt, double now) {
    if (session && role_ != Role::None && attached_ != session) attach(*session);
    if (session) session->pvp = rules_.pvp;
    if (role_ == Role::Host && session) hostUpdate(*session, dt, now);
    else if (role_ == Role::Client) clientUpdate(session, dt, now);
    if (session) {
        animate(dt);
        session->remotePlayers.clear();
        for (const Player& p : players_) {
            if (!p.posed) continue;
            RemoteAvatar a;
            a.name = p.name;
            a.dim = p.dim;
            a.skinSlot = p.skin.empty() ? -1 : p.skinSlot;
            a.pose.feet = p.pos;
            a.pose.yaw = p.yaw;
            a.pose.headPitch = p.pitch;
            a.pose.swing = p.swing;
            a.pose.slim = p.slim;
            a.pose.attack = p.attack;
            a.pose.place = p.place;
            a.pose.held = p.held;
            std::copy(p.armor, p.armor + 4, a.pose.armor);
            a.pose.riding = p.riding;
            session->remotePlayers.push_back(std::move(a));
        }
    }
}

void Multiplayer::sendChat(const std::string& text) {
    std::string t = text.substr(0, 200);
    if (role_ == Role::Host) broadcast(ChatMsg, net::Writer().str("<" + name_ + "> " + t), -1);
    else if (role_ == Role::Client && joinState_ == JoinState::Ready) server_.send(ChatMsg, net::Writer().str(t));
}

// ---------------------------------------------------------------- host

void Multiplayer::broadcast(uint8_t type, const net::Writer& w, int exceptId) {
    for (Peer& p : peers_)
        if (p.joined && p.id != exceptId) p.conn->send(type, w);
}

void Multiplayer::sendWorld(Session& session, net::Connection& c, int id) {
    session.saveWorlds();  // changed chunks go out as the files a world folder holds
    net::Writer w;
    w.i32(id).str(name_).str(worldName_).u32(session.info().seed).u8((uint8_t)session.mode());
    w.str("");  // (reserved)
    c.send(WelcomeMsg, w);
    // generator.json can be longer than a short string: it goes as bytes.
    std::string gen = session.currentWorld().config().toJson().dump();
    c.send(Chunk, net::Writer().str("generator.json").bytes(std::vector<uint8_t>(gen.begin(), gen.end())));
    for (int d = 0; d < 3; ++d) {
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(fs::path(session.info().folder) / kDimDirs[d], ec)) {
            std::string n = e.path().filename().string();
            if (!safeChunkName(n)) continue;
            c.send(Chunk, net::Writer().str(std::string(kDimDirs[d]) + "/" + n).bytes(readFile(e.path())));
        }
    }
    c.send(Ready, net::Writer());
}

void Multiplayer::hostUpdate(Session& session, float dt, double now) {
    // New players.
    for (int fd; (fd = listener_.accept()) >= 0;) {
        if ((int)peers_.size() >= kMaxPlayers - 1) {
            net::Connection c(fd);
            c.send(Kick, net::Writer().str("Svět je plný"));
            std::vector<net::Packet> ignored;
            c.poll(ignored);
            continue;
        }
        Peer p;
        p.conn = std::make_unique<net::Connection>(fd);
        p.id = nextId_++;
        p.since = now;
        peers_.push_back(std::move(p));
    }
    // Messages.
    for (size_t i = 0; i < peers_.size(); ++i) {
        Peer& peer = peers_[i];
        std::vector<net::Packet> in;
        bool alive = peer.conn->poll(in);
        for (net::Packet& pk : in) {
            net::Reader r(pk.data);
            if (pk.type == Hello && !peer.joined) {
                int version = r.u16();
                std::string name = r.str().substr(0, 24);
                bool slim = r.u8() != 0;
                std::vector<uint8_t> skin = r.bytes();
                if (!r.ok() || version != kProtocol) {
                    peer.conn->send(Kick, net::Writer().str("Jiná verze hry"));
                    alive = false;
                    break;
                }
                if (name.empty()) name = "Hráč";
                sendWorld(session, *peer.conn, peer.id);
                // Who is already here: the host and the other players.
                peer.conn->send(Join, joinOf(0, name_, slim_, skin_));
                for (const Player& o : players_) peer.conn->send(Join, joinOf(o.id, o.name, o.slim, o.skin));
                broadcast(Join, joinOf(peer.id, name, slim, skin), peer.id);
                peer.conn->send(RulesMsg, rulesOf());
                peer.joined = true;
                addPlayer(peer.id, name, slim, skin);
                messages_.push_back(name);
                messages_.back().insert(0, "+");
            } else if (pk.type == Pose && peer.joined) {
                handlePose(r, peer.id);
                net::Writer w;
                w.i32(peer.id);
                std::vector<uint8_t> body = w.data();
                body.insert(body.end(), pk.data.begin(), pk.data.end());
                for (Peer& o : peers_)
                    if (o.joined && o.id != peer.id) o.conn->send(Pose, body);
            } else if (pk.type == MobHit && peer.joined) {
                const uint32_t uid = r.u32();
                const float damage = std::clamp(r.f32(), 0.0f, 40.0f);
                const int idx = session.mobs().findUid(uid);
                const Player* from = player(peer.id);
                if (r.ok() && idx >= 0 && std::isfinite(damage))
                    session.mobs().hurt(idx, damage, from ? from->pos : session.mobs().mobs()[(size_t)idx].pos, peer.id);
            } else if (pk.type == PlayerHit && peer.joined) {
                const int target = r.i32();
                const float damage = std::clamp(r.f32(), 0.0f, 20.0f);
                const Player* from = player(peer.id);
                if (!r.ok() || !rules_.pvp || !from || !std::isfinite(damage)) continue;
                const Vec3 at = target == 0 ? session.player().pos : (player(target) ? player(target)->pos : from->pos);
                const Vec3 knock = Vec3(at.x - from->pos.x, 0, at.z - from->pos.z).normalized() * 4.0f + Vec3(0, 3.5f, 0);
                if (target == 0) session.hurtByPlayer(damage, knock);
                else sendTo(target, Hurt, net::Writer().f32(damage).f32(knock.x).f32(knock.y).f32(knock.z));
            } else if (pk.type == ContainerGet && peer.joined) {
                int kind = r.u8();
                FurnaceKey key{std::clamp(r.i32(), 0, 2), r.i32(), r.i32(), r.i32()};
                if (!r.ok()) continue;
                net::Writer w;
                writeContainer(w, session, kind, key);
                peer.conn->send(ContainerData, w);
            } else if (pk.type == ContainerSet && peer.joined) {
                int kind;
                FurnaceKey key;
                if (!readContainer(r, session, kind, key)) continue;
                net::Writer w;
                writeContainer(w, session, kind, key);
                broadcast(ContainerData, w, peer.id);
            } else if (pk.type == ChatMsg && peer.joined) {
                std::string text = r.str().substr(0, 200);
                Player* from = player(peer.id);
                if (!r.ok() || text.empty() || !from) continue;
                std::string line = "<" + from->name + "> " + text;
                messages_.push_back("#" + line);
                broadcast(ChatMsg, net::Writer().str(line), peer.id);
            } else if (pk.type == Block && peer.joined) {
                int dim = r.u8(), x = r.i32(), y = r.u8(), z = r.i32();
                ItemId id = r.u16();
                if (!r.ok()) continue;
                if (dim == 0 && rules_.protects(x, z)) {
                    // Only the host builds around the spawn: put the block back for the guest.
                    const ItemId keep = session.worldAt(0).getBlock(x, y, z);
                    peer.conn->send(Block, net::Writer().u8(0).i32(x).u8((uint8_t)y).i32(z).u16(keep));
                    continue;
                }
                applyBlock(&session, dim, x, y, z, id);
                for (Peer& o : peers_)
                    if (o.joined && o.id != peer.id) o.conn->send(Block, pk.data);
            }
        }
        // A silent socket that never said hello is dropped after a while.
        if (!peer.joined && now - peer.since > 15) alive = false;
        if (!alive) {
            if (peer.joined) {
                Player* p = player(peer.id);
                if (p) messages_.push_back("-" + p->name);
                removePlayer(peer.id);
                broadcast(Leave, net::Writer().i32(peer.id), peer.id);
            }
            peers_.erase(peers_.begin() + (long)i);
            --i;
        }
    }
    // Everyone sees the same sky: the time of day and the weather, every two seconds.
    if ((timeTimer_ += dt) >= 2.0f) {
        timeTimer_ = 0;
        broadcast(WorldTime, net::Writer().f32((float)std::fmod(session.timeOfDay(), 24000.0)).u8(session.raining() ? 1 : 0), -1);
    }
    // The shared animals and monsters, five times a second.
    if ((mobTimer_ += dt) >= 0.2f) {
        mobTimer_ = 0;
        broadcast(MobSync, mobsOf(session), -1);
    }
    for (auto& [killer, drops] : gifts_) {
        net::Writer w;
        w.u16((uint16_t)drops.size());
        for (auto [id, n] : drops) w.u16(id).u8((uint8_t)std::clamp(n, 0, 64));
        sendTo(killer, GiveItems, w);
    }
    gifts_.clear();
    for (auto [target, damage] : pendingHits_) {
        const Player* p = player(target);
        if (!rules_.pvp || !p) continue;
        const Vec3 me = session.player().pos;
        const Vec3 knock = Vec3(p->pos.x - me.x, 0, p->pos.z - me.z).normalized() * 4.0f + Vec3(0, 3.5f, 0);
        sendTo(target, Hurt, net::Writer().f32(damage).f32(knock.x).f32(knock.y).f32(knock.z));
    }
    pendingHits_.clear();
    // Our own changes and pose.
    for (int d = 0; d < 3; ++d)
        for (const BlockEdit& e : session.takeEdits(d))
            broadcast(Block, net::Writer().u8((uint8_t)d).i32(e.x).u8((uint8_t)e.y).i32(e.z).u16(e.id), -1);
    if ((poseTimer_ += dt) >= kPoseInterval) {
        poseTimer_ = 0;
        net::Writer w;
        w.i32(0);
        std::vector<uint8_t> body = w.data(), pose = poseOf(session).data();
        body.insert(body.end(), pose.begin(), pose.end());
        for (Peer& o : peers_)
            if (o.joined) o.conn->send(Pose, body);
    }
    for (Peer& p : peers_) {
        std::vector<net::Packet> none;
        if (p.conn->queued()) p.conn->poll(none);  // flush what was queued this frame
    }
    if ((announceTimer_ -= dt) <= 0) {
        announceTimer_ = kAnnounceInterval;
        net::LanGame g;
        g.port = listener_.port();
        g.host = name_;
        g.world = worldName_;
        g.mode = (int)session.mode();
        g.players = playerCount();
        announcer_.announce(g);
    }
}

// ---------------------------------------------------------------- client

void Multiplayer::clientUpdate(Session* session, float dt, double now) {
    (void)now;
    if (joinState_ == JoinState::Failed) return;
    if (session) {
        session->recordEdits(true);
        for (const Edit& e : pendingEdits_) session->applyRemoteBlock(e.dim, e.x, e.y, e.z, e.id);
        pendingEdits_.clear();
        for (int d = 0; d < 3; ++d)
            for (const BlockEdit& e : session->takeEdits(d))
                server_.send(Block, net::Writer().u8((uint8_t)d).i32(e.x).u8((uint8_t)e.y).i32(e.z).u16(e.id));
        if ((poseTimer_ += dt) >= kPoseInterval) {
            poseTimer_ = 0;
            server_.send(Pose, poseOf(*session));
        }
        for (auto [uid, damage] : mobHits_) server_.send(MobHit, net::Writer().u32(uid).f32(damage));
        mobHits_.clear();
        for (auto [target, damage] : pendingHits_) server_.send(PlayerHit, net::Writer().i32(target).f32(damage));
        pendingHits_.clear();
        syncContainer(*session, dt);
    }
    if (server_.connected() && !helloSent_) {
        server_.send(Hello, net::Writer().u16(kProtocol).str(name_).u8(slim_).bytes(skin_));
        helloSent_ = true;
        joinState_ = JoinState::Downloading;
    }
    std::vector<net::Packet> in;
    bool alive = server_.poll(in);
    for (net::Packet& pk : in) {
        net::Reader r(pk.data);
        switch (pk.type) {
            case WelcomeMsg: {
                welcome_.yourId = r.i32();
                welcome_.host = r.str();
                welcome_.world = r.str();
                welcome_.seed = r.u32();
                welcome_.mode = r.u8() == (uint8_t)GameMode::Creative ? GameMode::Creative : GameMode::Survival;
                break;
            }
            case Chunk: {
                std::string path = r.str();
                std::vector<uint8_t> data = r.bytes();
                if (!r.ok()) break;
                if (path == "generator.json") {
                    welcome_.generator.assign(data.begin(), data.end());
                } else {
                    size_t slash = path.find('/');
                    std::string dir = path.substr(0, slash), file = slash == std::string::npos ? "" : path.substr(slash + 1);
                    if ((dir == kDimDirs[0] || dir == kDimDirs[1] || dir == kDimDirs[2]) && safeChunkName(file))
                        chunkFiles_.emplace_back(path, std::move(data));
                }
                break;
            }
            case Ready:
                joinState_ = JoinState::Ready;
                break;
            case Join: {
                int id = r.i32();
                std::string name = r.str().substr(0, 24);
                bool slim = r.u8() != 0;
                std::vector<uint8_t> skin = r.bytes();
                if (r.ok() && id != welcome_.yourId) {
                    bool known = player(id) != nullptr;
                    addPlayer(id, name, slim, skin);
                    if (!known && joinState_ == JoinState::Ready && id != 0) messages_.push_back("+" + name);
                }
                break;
            }
            case Leave: {
                Player* p = player(r.i32());
                if (p) {
                    messages_.push_back("-" + p->name);
                    removePlayer(p->id);
                }
                break;
            }
            case Pose: {
                int id = r.i32();
                handlePose(r, id);
                break;
            }
            case Block: {
                int dim = r.u8(), x = r.i32(), y = r.u8(), z = r.i32();
                ItemId id = r.u16();
                if (r.ok()) applyBlock(session, dim, x, y, z, id);
                break;
            }
            case ChatMsg: {
                std::string line = r.str();
                if (r.ok() && !line.empty()) messages_.push_back("#" + line.substr(0, 240));
                break;
            }
            case WorldTime: {
                float ticks = r.f32();
                bool rain = r.u8() != 0;
                if (r.ok() && session && std::isfinite(ticks)) {
                    // Small differences are left alone so the sky does not jump.
                    float mine = (float)std::fmod(session->timeOfDay(), 24000.0);
                    float diff = std::fabs(mine - ticks);
                    if (std::min(diff, 24000 - diff) > 40) session->setTimeOfDay(ticks);
                    if (session->raining() != rain) session->setWeather(rain);
                }
                break;
            }
            case RulesMsg: {
                Rules got;
                got.pvp = r.u8() != 0;
                got.protectSpawn = r.u8() != 0;
                got.radius = r.u8();
                got.spawnX = r.f32();
                got.spawnZ = r.f32();
                if (r.ok() && std::isfinite(got.spawnX) && std::isfinite(got.spawnZ)) rules_ = got;
                break;
            }
            case MobSync:
                if (session) applyMobs(*session, r);
                break;
            case GiveItems: {
                const int n = r.u16();
                for (int k = 0; k < n && k < 64; ++k) {
                    const ItemId id = r.u16();
                    const int count = r.u8();
                    if (r.ok() && session && id != AIR && id < kItemIdLimit && count > 0)
                        session->giveItem({id, (uint8_t)std::min(count, maxStack(id)), 0});
                }
                break;
            }
            case Hurt: {
                const float damage = r.f32();
                const Vec3 knock{r.f32(), r.f32(), r.f32()};
                if (r.ok() && session && std::isfinite(damage) && std::isfinite(knock.x) && std::isfinite(knock.y) &&
                    std::isfinite(knock.z))
                    session->hurtByPlayer(std::clamp(damage, 0.0f, 20.0f), knock);
                break;
            }
            case ContainerData: {
                int kind;
                FurnaceKey key;
                if (session && readContainer(r, *session, kind, key) && kind == openKind_ && key == openKey_) {
                    net::Writer w;
                    writeContainer(w, *session, kind, key);
                    containerSnapshot_ = w.data();  // what the host has is what we show
                }
                break;
            }
            case Kick:
                error_ = r.str();
                if (error_.empty()) error_ = "Odpojeno";
                alive = false;
                break;
            default:
                break;
        }
    }
    if (!alive) {
        if (error_.empty()) error_ = server_.error().empty() ? "Spojení ukončeno" : server_.error();
        server_.close();
        if (joinState_ == JoinState::Ready) lost_ = true;
        joinState_ = JoinState::Failed;
        players_.clear();
    }
}

// ---------------------------------------------------------------- 1.4.0: rules, mobs, containers

void Multiplayer::setRules(bool pvp, bool protectSpawn) {
    if (rules_.pvp == pvp && rules_.protectSpawn == protectSpawn) return;
    rules_.pvp = pvp;
    rules_.protectSpawn = protectSpawn;
    if (role_ == Role::Host) broadcast(RulesMsg, rulesOf(), -1);
}

net::Writer Multiplayer::rulesOf() const {
    net::Writer w;
    w.u8(rules_.pvp).u8(rules_.protectSpawn).u8((uint8_t)rules_.radius).f32(rules_.spawnX).f32(rules_.spawnZ);
    return w;
}

void Multiplayer::attach(Session& session) {
    attached_ = &session;
    if (role_ == Role::Host) {
        const Vec3 spawn = session.spawnPoint();
        rules_.spawnX = spawn.x;
        rules_.spawnZ = spawn.z;
        session.onRemoteKill = [this](int killer, const std::vector<std::pair<ItemId, int>>& drops) { gifts_.emplace_back(killer, drops); };
    } else {
        // A guest: the host's mobs, the host's rules around the spawn.
        session.mobsFromHost = true;
        session.mobs().mobs().clear();
        Session* s = &session;
        session.canEdit = [this, s](int x, int, int z) { return s->dimIndex() != 0 || !rules_.protects(x, z); };
        session.onHitRemoteMob = [this](uint32_t uid, float damage) { mobHits_.emplace_back(uid, damage); };
    }
    Session* s = &session;
    session.pickRemotePlayer = [this, s](const Vec3& eye, const Vec3& dir, float maxDist, float& dist) {
        return pickPlayer(s->dimIndex(), eye, dir, maxDist, dist);
    };
    session.onHitPlayer = [this](int who, float damage) { pendingHits_.emplace_back(who, damage); };
}

int Multiplayer::pickPlayer(int dim, const Vec3& eye, const Vec3& dir, float maxDist, float& dist) const {
    int best = -1;
    float bestT = maxDist;
    for (const Player& p : players_) {
        if (!p.posed || p.dim != dim) continue;
        const Vec3 lo = p.pos - Vec3(0.4f, 0, 0.4f), hi = p.pos + Vec3(0.4f, 1.85f, 0.4f);
        float t0 = 0, t1 = bestT;
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            const float o = a == 0 ? eye.x : a == 1 ? eye.y : eye.z, d = a == 0 ? dir.x : a == 1 ? dir.y : dir.z;
            const float l = a == 0 ? lo.x : a == 1 ? lo.y : lo.z, u = a == 0 ? hi.x : a == 1 ? hi.y : hi.z;
            if (std::fabs(d) < 1e-6f) {
                hit = o >= l && o <= u;
                continue;
            }
            float ta = (l - o) / d, tb = (u - o) / d;
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            hit = t0 <= t1;
        }
        if (hit && t0 < bestT) {
            bestT = t0;
            best = p.id;
        }
    }
    dist = bestT;
    return best;
}

void Multiplayer::sendTo(int id, uint8_t type, const net::Writer& w) {
    for (Peer& p : peers_)
        if (p.joined && p.id == id) p.conn->send(type, w);
}

net::Writer Multiplayer::mobsOf(Session& session) const {
    // Every living mob of the host: where it is, which way it looks, how it is doing.
    const auto& mobs = session.mobs().mobs();
    net::Writer w;
    w.u16((uint16_t)std::min<size_t>(mobs.size(), 400));
    for (size_t i = 0; i < mobs.size() && i < 400; ++i) {
        const Mob& m = mobs[i];
        const uint8_t flags = (m.alive() ? 0 : 1) | (m.hurtTime > 0 ? 2 : 0);
        w.u32(m.uid).u8((uint8_t)m.type).u8((uint8_t)m.dim).f32(m.pos.x).f32(m.pos.y).f32(m.pos.z).f32(m.yaw).u8(flags);
        w.u8((uint8_t)m.profession).f32(m.fuse).u16(m.variant);
    }
    return w;
}

void Multiplayer::applyMobs(Session& session, net::Reader& r) {
    std::vector<Mob>& mobs = session.mobs().mobs();
    std::vector<Mob> next;
    const int n = r.u16();
    for (int k = 0; k < n; ++k) {
        const uint32_t uid = r.u32();
        const int type = r.u8(), dim = r.u8();
        const Vec3 pos{r.f32(), r.f32(), r.f32()};
        const float yaw = r.f32();
        const int flags = r.u8(), profession = r.u8();
        const float fuse = r.f32();
        const int variant = r.u16();
        if (!r.ok()) return;
        if (type >= (int)MobType::Count || !std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) continue;
        Mob m;
        auto it = std::find_if(mobs.begin(), mobs.end(), [&](const Mob& o) { return o.uid == uid; });
        if (it != mobs.end()) m = *it;
        else m.pos = pos;  // new here: appears where it is
        m.uid = uid;
        m.type = (MobType)type;
        m.dim = dim;
        m.target = pos;
        m.yaw = yaw;
        m.fuse = fuse;
        m.variant = (uint16_t)(ModRegistry::get().mob(variant) ? variant : 0);
        m.profession = (Profession)std::min(profession, (int)Profession::Count - 1);
        if (flags & 2) m.hurtTime = std::max(m.hurtTime, 0.2f);
        if ((flags & 1) && m.alive()) m.deathTime = 0.001f;
        next.push_back(m);
    }
    mobs = std::move(next);
}

void Multiplayer::writeContainer(net::Writer& w, Session& session, int kind, const FurnaceKey& key) {
    w.u8((uint8_t)kind).i32(key.dim).i32(key.x).i32(key.y).i32(key.z);
    auto stack = [&](const ItemStack& s) { w.u16(s.id).u8(s.count).u16(s.damage); };
    if (kind == 0) {
        for (const ItemStack& s : session.chestFor(key)) stack(s);
    } else {
        const Furnace& f = session.furnaceFor(key);
        stack(f.input);
        stack(f.fuel);
        stack(f.output);
        w.f32(f.burnLeft).f32(f.burnTotal).f32(f.cook);
    }
}

bool Multiplayer::readContainer(net::Reader& r, Session& session, int& kind, FurnaceKey& key) {
    kind = r.u8();
    key.dim = std::clamp(r.i32(), 0, 2);
    key.x = r.i32();
    key.y = r.i32();
    key.z = r.i32();
    auto stack = [&]() {
        ItemStack s;
        s.id = r.u16();
        s.count = r.u8();
        s.damage = r.u16();
        if (s.id >= kItemIdLimit || s.count == 0) s = {};
        else s.count = (uint8_t)std::min<int>(s.count, maxStack(s.id));
        return s;
    };
    if (kind == 0) {
        ChestContents c;
        for (ItemStack& s : c) s = stack();
        if (!r.ok()) return false;
        session.setChest(key, c);
    } else if (kind == 1) {
        Furnace f = session.furnaceFor(key);
        f.input = stack();
        f.fuel = stack();
        f.output = stack();
        const float left = r.f32(), total = r.f32(), cook = r.f32();
        if (!r.ok() || !std::isfinite(left) || !std::isfinite(total) || !std::isfinite(cook)) return false;
        f.burnLeft = std::clamp(left, 0.0f, 1000.0f);
        f.burnTotal = std::clamp(total, 0.0f, 1000.0f);
        f.cook = std::clamp(cook, 0.0f, 100.0f);
        session.setFurnace(key, f);
    } else {
        return false;
    }
    return true;
}

void Multiplayer::setOpenContainer(int kind, const FurnaceKey& key) {
    openKind_ = kind;
    openKey_ = key;
}

void Multiplayer::syncContainer(Session& session, float dt) {
    // A guest's open chest or furnace: asked for when it opens and again now and then
    // (the host's furnace keeps smelting); every change made here goes to the host.
    if (openKind_ < 0) {
        sentKind_ = -1;
        return;
    }
    auto ask = [&] {
        server_.send(ContainerGet, net::Writer().u8((uint8_t)openKind_).i32(openKey_.dim).i32(openKey_.x).i32(openKey_.y).i32(openKey_.z));
    };
    net::Writer now;
    writeContainer(now, session, openKind_, openKey_);
    if (sentKind_ != openKind_ || !(sentKey_ == openKey_)) {
        sentKind_ = openKind_;
        sentKey_ = openKey_;
        containerSnapshot_ = now.data();
        containerTimer_ = 0;
        ask();
        return;
    }
    if (now.data() != containerSnapshot_) {
        server_.send(ContainerSet, now);
        containerSnapshot_ = now.data();
        containerTimer_ = 0;
        return;
    }
    if ((containerTimer_ += dt) >= (openKind_ == 1 ? 0.5f : 1.0f)) {
        containerTimer_ = 0;
        ask();
    }
}

}  // namespace vc
