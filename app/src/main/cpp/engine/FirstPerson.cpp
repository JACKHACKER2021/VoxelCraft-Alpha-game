#include "FirstPerson.h"

#include <algorithm>
#include <cmath>

#include "BlockShapes.h"
#include "Blocks.h"
#include "Items.h"
#include "ItemModel.h"
#include "PlayerModel.h"
#include "Skin.h"

namespace vc {

namespace {

constexpr float kTile = kTileUv;

// Rotation about X, then Y, then Z (radians), then a translation.
struct Transform {
    float rx = 0, ry = 0, rz = 0;
    Vec3 t;
    Vec3 apply(Vec3 p) const {
        float c = std::cos(rx), s = std::sin(rx);
        p = {p.x, p.y * c - p.z * s, p.y * s + p.z * c};
        c = std::cos(ry), s = std::sin(ry);
        p = {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
        c = std::cos(rz), s = std::sin(rz);
        p = {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
        return p + t;
    }
};

// Offsets for the arm and held items: swing, place, walk bob, idle and equip.
// The arm strikes by lifting towards the middle of the view, a held block or item
// tips forward like a hit.
Transform animate(const HandAnimator& h, Transform base, bool arm, bool tool = false) {
    const float s = h.swingProgress(), p = h.placeProgress();
    if (s < 1 && tool) {
        // A tool chops: it tips forward and down about the hand and comes back.
        const float a = std::sin(std::sqrt(s) * kPi), b = std::sin(s * kPi);
        base.t = base.t + Vec3(-0.06f * a, 0.04f * a, -0.08f * b);
        base.rx += -0.55f * a;
        base.rz += 0.35f * a;
    } else if (s < 1) {
        float a = std::sin(std::sqrt(s) * kPi), b = std::sin(s * s * kPi), c = std::sin(std::sqrt(s) * 2 * kPi);
        if (arm) {
            base.t = base.t + Vec3(-0.2f * a, 0.07f * c, -0.14f * b);
            base.rx += 0.45f * a;
            base.ry += 0.4f * b;
        } else {
            base.t = base.t + Vec3(-0.24f * a, 0.1f * c, -0.15f * b);
            base.rx += -0.7f * a;
            base.ry += 0.3f * b;
            base.rz += 0.2f * a;
        }
    }
    if (p < 1) {
        float a = std::sin(p * kPi);
        base.t = base.t + Vec3(0, -0.08f * a, -0.12f * a);
        base.rx += (arm ? 0.25f : -0.35f) * a;
    }
    const float bob = h.bob();
    base.t = base.t + Vec3(std::cos(h.bobPhase()) * 0.025f * bob, -std::fabs(std::sin(h.bobPhase())) * 0.04f * bob, 0);
    base.t.y += std::sin(h.time() * 1.7f) * 0.006f;  // breathing
    base.t.y -= 0.65f * h.equip();
    return base;
}

void quad(std::vector<Vertex>& out, Vec3 a, Vec3 b, Vec3 c, Vec3 d, float u0, float v0, float u1, float v1, float light) {
    Vertex va{a.x, a.y, a.z, u0, v1, light}, vb{b.x, b.y, b.z, u1, v1, light};
    Vertex vc{c.x, c.y, c.z, u1, v0, light}, vd{d.x, d.y, d.z, u0, v0, light};
    out.insert(out.end(), {va, vb, vc, va, vc, vd});
}

void tileUv(int tile, float& u0, float& v0, float& u1, float& v1) {
    u0 = (tile % kAtlasCols) * kTile;
    v0 = (tile / kAtlasCols) * kTile;
    u1 = u0 + kTile;
    v1 = v0 + kTile;
}

void heldBlock(const Transform& tr, ItemId id, float light, std::vector<Vertex>& out) {
    const float h = 0.15f;
    Box boxes[kItemBoxes];
    const int n = itemBoxes(id, boxes);
    for (int i = 0; i < n; ++i) {
        const Box& bx = boxes[i];
        // The box in hand space (the full cube spans -h..h) and the matching part of each tile.
        const float x0 = -h + 2 * h * bx.x0, x1 = -h + 2 * h * bx.x1, y0 = -h + 2 * h * bx.y0, y1 = -h + 2 * h * bx.y1;
        const float z0 = -h + 2 * h * bx.z0, z1 = -h + 2 * h * bx.z1;
        auto P = [&](float x, float y, float z) { return tr.apply({x, y, z}); };
        auto face = [&](Face f, Vec3 a, Vec3 b, Vec3 c, Vec3 d, float s0, float s1, float t0, float t1, float shade) {
            float u0, v0, u1, v1;
            tileUv(tileFor(id, f), u0, v0, u1, v1);
            const float du = u1 - u0, dv = v1 - v0;
            quad(out, P(a.x, a.y, a.z), P(b.x, b.y, b.z), P(c.x, c.y, c.z), P(d.x, d.y, d.z), u0 + s0 * du, v0 + t0 * dv,
                 u0 + s1 * du, v0 + t1 * dv, shade * light);
        };
        const float ty0 = 1 - bx.y1, ty1 = 1 - bx.y0;
        face(F_TOP, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}, bx.x0, bx.x1, bx.z0, bx.z1, 1.0f);
        face(F_BOTTOM, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, bx.x0, bx.x1, bx.z0, bx.z1, 0.5f);
        face(F_NORTH, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, 1 - bx.x1, 1 - bx.x0, ty0, ty1, 0.8f);
        face(F_SOUTH, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, bx.x0, bx.x1, ty0, ty1, 0.8f);
        face(F_EAST, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, 1 - bx.z1, 1 - bx.z0, ty0, ty1, 0.6f);
        face(F_WEST, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, bx.z0, bx.z1, ty0, ty1, 0.6f);
    }
}

void heldFlat(const Transform& tr, ItemId id, float light, std::vector<Vertex>& out) {
    // A 3D item: the sprite extruded one pixel deep, held like a tool.
    const float s = 0.55f;
    appendItemModel(tileFor(id, F_SOUTH), light, 1 / 16.0f, [&](const Vec3& q) { return tr.apply(q * s); }, out);
}

}  // namespace

void HandAnimator::update(float dt, float walkSpeed, bool onGround, bool mining, ItemId held, int slot) {
    time_ += dt;
    swing_ = std::min(1.0f, swing_ + dt / kSwingTime);
    place_ = std::min(1.0f, place_ + dt / kPlaceTime);
    // Mining keeps striking for as long as the player really digs.
    if (mining && swing_ >= 1) swing_ = 0;
    // Walking bob follows the real speed and fades out when standing or in the air.
    float target = onGround ? std::min(1.0f, walkSpeed / 4.3f) : 0;
    bobAmount_ += (target - bobAmount_) * std::min(1.0f, dt * 8);
    bobPhase_ += walkSpeed * dt * 2.4f;
    // Switching items: lower the hand, swap what it holds at the bottom, raise it again.
    if (held != shown_ || slot != shownSlot_) {
        if (shownSlot_ < 0) {  // first frame: start raised with the right item
            shown_ = held;
            shownSlot_ = slot;
        } else {
            lowering_ = true;
        }
    }
    if (lowering_) {
        // Running out of a stack swaps at once (nothing left to lower); a different slot dips.
        bool sameSlotEmptied = held == AIR && slot == shownSlot_;
        equip_ = sameSlotEmptied ? 1.0f : std::min(1.0f, equip_ + dt * kEquipSpeed);
        if (equip_ >= 1) {
            shown_ = held;
            shownSlot_ = slot;
            lowering_ = held != shown_ || slot != shownSlot_;
        }
    } else {
        equip_ = std::max(0.0f, equip_ - dt * kEquipSpeed);
    }
}

void HandAnimator::swing() {
    if (swing_ >= 0.5f) swing_ = 0;
}

void HandAnimator::place() {
    place_ = 0;
    if (swing_ >= 1) swing_ = 0.35f;  // a short tail of the punch
}

void HandAnimator::use() { place_ = 0; }

void buildFirstPerson(const HandAnimator& hand, float light, std::vector<Vertex>& skin, std::vector<Vertex>& atlas) {
    skin.clear();
    atlas.clear();
    const ItemId id = hand.shown();
    if (id == AIR) {
        // The bare right arm: the shoulder sits below the lower right corner of the view
        // and the arm reaches towards the middle, so the forearm and hand show.
        const Vec3 shoulder{0.75f, -0.78f, -0.3f}, reach = Vec3(-0.4f, 0.4f, -0.55f).normalized();
        Transform base;
        base.t = shoulder;
        // Angles that turn the arm's own axis (towards the hand, -y) into `reach`.
        base.rx = std::acos(-reach.y);
        base.ry = std::atan2(-reach.x, -reach.z);
        Transform tr = animate(hand, base, true);
        const float px = 0.068f;  // one skin pixel in view space
        auto map = [&](const Vec3& q) { return tr.apply(q * px); };
        // Arm pixels as on the model: x -2..2, the shoulder at y = 0, the hand at y = -12.
        appendSkinBox(skin, skinparts::kRightArm, {-2, -12, -2}, {2, 0, 2}, 0, light, map);
        appendSkinBox(skin, skinparts::kRightSleeve, {-2, -12, -2}, {2, 0, 2}, 0.25f, light, map);
        return;
    }
    const ToolKind kind = toolKind(id);
    if (kind == ToolKind::Pickaxe || kind == ToolKind::Axe || kind == ToolKind::Shovel || kind == ToolKind::Sword || id == HOE ||
        id == BRUSHER) {
        // 1.5.1: tools are held by the end of the handle (sprite pixel 2, 13), big in the lower
        // right of the view and turned a little, and they swing about the hand.
        Transform base;
        base.t = {0.2f, -0.6f, -0.8f};
        base.rx = -0.1f;
        base.ry = -0.55f;
        base.rz = 0.12f;
        const Transform tr = animate(hand, base, false, true);
        const float size = 0.6f;
        const Vec3 grip(2.5f / 16 - 0.5f, 0.5f - 13.5f / 16, 0);
        appendItemModel(tileFor(id, F_SOUTH), light, 1 / 16.0f, [&](const Vec3& q) { return tr.apply((q - grip) * size); }, atlas);
        return;
    }
    if (isItemOnly(id) || isPlant(id)) {
        Transform base;
        base.t = {0.48f, -0.36f, -0.72f};
        base.ry = -0.45f;
        base.rz = 0.35f;
        heldFlat(animate(hand, base, false), id, light, atlas);
        return;
    }
    // A block: a small cube held in front of the lower right corner, turned to show three faces.
    Transform base;
    base.t = {0.52f, -0.46f, -0.95f};
    base.rx = 0.15f;
    base.ry = 0.75f;
    heldBlock(animate(hand, base, false), id, light, atlas);
}

}  // namespace vc
