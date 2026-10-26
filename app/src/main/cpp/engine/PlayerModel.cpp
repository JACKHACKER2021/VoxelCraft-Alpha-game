#include "PlayerModel.h"

#include <cmath>

#include "Skin.h"
#include "Blocks.h"
#include "Items.h"
#include "ItemModel.h"

namespace vc {

namespace {

constexpr float kPixel = 1.8f / 32.0f;  // a 32 px tall model is 1.8 blocks tall

struct Corner {
    float x, y, z, u, v;
};

class Builder {
public:
    Builder(const PlayerPose& pose, std::vector<Vertex>& out) : pose_(pose), out_(out) {
        cy_ = std::cos(pose.yaw);
        sy_ = std::sin(pose.yaw);
    }

    // A box in model pixels (x0..x1, y0..y1, z0..z1), rotated by `angle` about the X axis
    // through `pivot` (pitch for the head, swing for limbs). `grow` inflates overlay layers.
    void box(const SkinPart& p, float x0, float y0, float z0, float x1, float y1, float z1, Vec3 pivot, float angle,
             float grow = 0) {
        const float ca = std::cos(angle), sa = std::sin(angle);
        appendSkinBox(out_, p, {x0, y0, z0}, {x1, y1, z1}, grow, 1.0f, [&](const Vec3& q) {
            // Rotate about X through the pivot (limb swing / head pitch).
            Vec3 p = q;
            float ry = p.y - pivot.y, rz = p.z - pivot.z;
            p.y = pivot.y + ry * ca - rz * sa;
            p.z = pivot.z + ry * sa + rz * ca;
            // Model faces -Z; turn it to the player's yaw, then scale to blocks.
            float wx = (p.x * cy_ - p.z * sy_) * kPixel, wz = (p.x * sy_ + p.z * cy_) * kPixel;
            return Vec3(pose_.feet.x + wx, pose_.feet.y + p.y * kPixel, pose_.feet.z + wz);
        });
    }

private:
    const PlayerPose& pose_;
    std::vector<Vertex>& out_;
    float cy_, sy_;
};

}  // namespace

void appendSkinBox(std::vector<Vertex>& out, const SkinPart& p, Vec3 lo, Vec3 hi, float grow, float light,
                   const std::function<Vec3(const Vec3&)>& map) {
    const float x0 = lo.x - grow, y0 = lo.y - grow, z0 = lo.z - grow;
    const float x1 = hi.x + grow, y1 = hi.y + grow, z1 = hi.z + grow;
    const float u = (float)p.u, v = (float)p.v, w = (float)p.w, h = (float)p.h, d = (float)p.d;
    auto face = [&](Vec3 tl, Vec3 tr, Vec3 br, Vec3 bl, float fu, float fv, float fw, float fh, float shade) {
        auto vert = [&](const Vec3& q, float tu, float tv) {
            Vec3 m = map(q);
            return Vertex{m.x, m.y, m.z, tu / Skin::kSize, tv / Skin::kSize, shade * light};
        };
        Vertex a = vert(tl, fu, fv), b = vert(tr, fu + fw, fv), c = vert(br, fu + fw, fv + fh), e = vert(bl, fu, fv + fh);
        out.insert(out.end(), {a, b, c, a, c, e});
    };
    // Faces as seen from outside: top-left, top-right, bottom-right, bottom-left.
    face({x1, y1, z0}, {x0, y1, z0}, {x0, y0, z0}, {x1, y0, z0}, u + d, v + d, w, h, 0.8f);          // front
    face({x0, y1, z1}, {x1, y1, z1}, {x1, y0, z1}, {x0, y0, z1}, u + 2 * d + w, v + d, w, h, 0.8f);  // back
    face({x1, y1, z1}, {x1, y1, z0}, {x1, y0, z0}, {x1, y0, z1}, u, v + d, d, h, 0.6f);              // right
    face({x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}, {x0, y0, z0}, u + d + w, v + d, d, h, 0.6f);      // left
    face({x1, y1, z1}, {x0, y1, z1}, {x0, y1, z0}, {x1, y1, z0}, u + d, v, w, d, 1.0f);              // top
    face({x1, y0, z0}, {x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1}, u + d + w, v, w, d, 0.5f);          // bottom
}

namespace {

// The right arm's angle: walking swing, a strike and a short push when placing.
float rightArmAngle(const PlayerPose& pose) {
    // Positive angles swing the arm forward (towards -Z, where the model looks).
    float a = -pose.swing;
    if (pose.attack < 1) a += std::sin(std::sqrt(pose.attack) * 3.14159265f) * 1.5f;
    if (pose.place < 1) a += std::sin(pose.place * 3.14159265f) * 0.6f;
    if (pose.held && pose.attack >= 1) a += 0.25f;  // holding something: the arm a little forward
    return a;
}

// Model pixels -> world, with a rotation about X through `pivot` (like Builder::box).
Vec3 modelToWorld(const PlayerPose& pose, Vec3 p, Vec3 pivot, float angle) {
    const float ca = std::cos(angle), sa = std::sin(angle);
    float ry = p.y - pivot.y, rz = p.z - pivot.z;
    p.y = pivot.y + ry * ca - rz * sa;
    p.z = pivot.z + ry * sa + rz * ca;
    const float cy = std::cos(pose.yaw), sy = std::sin(pose.yaw);
    float wx = (p.x * cy - p.z * sy) * kPixel, wz = (p.x * sy + p.z * cy) * kPixel;
    return Vec3(pose.feet.x + wx, pose.feet.y + p.y * kPixel, pose.feet.z + wz);
}

}  // namespace

std::vector<Vertex> buildPlayerModel(const PlayerPose& pose) {
    using namespace skinparts;
    std::vector<Vertex> v;
    v.reserve(36 * 12);
    Builder b(pose, v);
    const float arm = pose.slim ? 3 : 4, s = pose.swing;
    // The player's right side is +X in model space (the model looks along -Z).
    const Vec3 neck{0, 24, 0}, rS{6, 22, 0}, lS{-6, 22, 0}, rH{2, 12, 0}, lH{-2, 12, 0};
    for (int layer = 0; layer < 2; ++layer) {
        float g = layer ? 0.5f : 0;   // overlay sits half a pixel outside the base layer
        float lg = layer ? 0.25f : 0;
        b.box(layer ? kHat : kHead, -4, 24, -4, 4, 32, 4, neck, pose.headPitch, g);
        b.box(layer ? kJacket : kBody, -4, 12, -2, 4, 24, 2, neck, 0, lg);
        b.box(layer ? kRightSleeve : kRightArm, 4, 12, -2, 4 + arm, 24, 2, rS, rightArmAngle(pose), lg);
        b.box(layer ? kLeftSleeve : kLeftArm, -4 - arm, 12, -2, -4, 24, 2, lS, s, lg);
        // In a boat the legs point forward.
        const float rl = pose.riding ? 1.4f : s, ll = pose.riding ? 1.4f : -s;
        b.box(layer ? kRightPants : kRightLeg, 0, 0, -2, 4, 12, 2, rH, rl, lg);
        b.box(layer ? kLeftPants : kLeftLeg, -4, 0, -2, 0, 12, 2, lH, ll, lg);
    }
    return v;
}

}  // namespace vc

namespace vc {

std::vector<Vertex> buildPlayerExtras(const PlayerPose& pose) {
    std::vector<Vertex> out;
    const float tileUv = kTileUv;
    // A box in model pixels with one atlas tile on every face, rotated like a body part.
    auto box = [&](int tile, Vec3 lo, Vec3 hi, Vec3 pivot, float angle, float light) {
        const float u0 = (tile % kAtlasCols) * tileUv, v0 = (tile / kAtlasCols) * tileUv;
        static const int faces[6][4][3] = {
            {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
            {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
            {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}};
        static const float shade[6] = {1.0f, 0.5f, 0.8f, 0.8f, 0.6f, 0.6f};
        const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
        for (int f = 0; f < 6; ++f) {
            Vertex q[4];
            for (int k = 0; k < 4; ++k) {
                Vec3 p(faces[f][k][0] ? hi.x : lo.x, faces[f][k][1] ? hi.y : lo.y, faces[f][k][2] ? hi.z : lo.z);
                Vec3 w = modelToWorld(pose, p, pivot, angle);
                q[k] = {w.x, w.y, w.z, u0 + uv[k][0] * tileUv, v0 + uv[k][1] * tileUv, shade[f] * light};
            }
            out.insert(out.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        }
    };
    const float arm = pose.slim ? 3 : 4, s = pose.swing, ra = rightArmAngle(pose);
    const Vec3 neck{0, 24, 0}, rS{6, 22, 0}, lS{-6, 22, 0}, rH{2, 12, 0}, lH{-2, 12, 0};
    const float rl = pose.riding ? 1.4f : s, ll = pose.riding ? 1.4f : -s;
    // Armour: the material's block texture, a little outside the skin.
    static const int kMaterialTile[5] = {T_PLANKS, T_COBBLE, T_BRONZE_BLOCK, T_STEEL_BLOCK, T_RUBY_BLOCK};
    for (int piece = 0; piece < 4; ++piece) {
        ItemId id = pose.armor[piece];
        if (!isArmor(id)) continue;
        const int tile = kMaterialTile[(id - FIRST_ARMOR) / 4];
        const float g = 1.0f;
        if (piece == 0) {
            // A helmet with an open face: crown, back, cheeks and a brow over the eyes.
            const float hp = pose.headPitch;
            box(tile, {-4 - g, 31, -4 - g}, {4 + g, 32 + g, 4 + g}, neck, hp, 0.95f);
            box(tile, {-4 - g, 24 - 0.2f, 2.5f}, {4 + g, 31, 4 + g}, neck, hp, 0.95f);
            box(tile, {-4 - g, 25, -4 - g}, {-3.2f, 31, 2.5f}, neck, hp, 0.95f);
            box(tile, {3.2f, 25, -4 - g}, {4 + g, 31, 2.5f}, neck, hp, 0.95f);
            box(tile, {-3.2f, 30, -4 - g}, {3.2f, 31, -3.2f}, neck, hp, 0.95f);
        } else if (piece == 1) {
            box(tile, {-4 - 0.8f, 12.5f, -2 - 0.8f}, {4 + 0.8f, 24 + 0.8f, 2 + 0.8f}, neck, 0, 0.95f);
            box(tile, {4 - 0.6f, 17, -2 - 0.8f}, {4 + arm + 0.8f, 24 + 0.8f, 2 + 0.8f}, rS, ra, 0.95f);
            box(tile, {-4 - arm - 0.8f, 17, -2 - 0.8f}, {-4 + 0.6f, 24 + 0.8f, 2 + 0.8f}, lS, s, 0.95f);
        } else if (piece == 2) {
            box(tile, {-4 - 0.5f, 10, -2 - 0.5f}, {4 + 0.5f, 13, 2 + 0.5f}, neck, 0, 0.9f);  // waist
            box(tile, {0 - 0.5f, 4, -2 - 0.5f}, {4 + 0.5f, 12, 2 + 0.5f}, rH, rl, 0.9f);
            box(tile, {-4 - 0.5f, 4, -2 - 0.5f}, {0 + 0.5f, 12, 2 + 0.5f}, lH, ll, 0.9f);
        } else {
            box(tile, {0 - 0.9f, -0.2f, -2 - 0.9f}, {4 + 0.9f, 4, 2 + 0.9f}, rH, rl, 0.9f);
            box(tile, {-4 - 0.9f, -0.2f, -2 - 0.9f}, {0 + 0.9f, 4, 2 + 0.9f}, lH, ll, 0.9f);
        }
    }
    // The item in the right hand: the 3D item model standing forward out of the fist, or a small block.
    if (pose.held) {
        const Vec3 hand{4 + arm / 2, 13, -1};  // in the fist at the bottom of the right arm
        if (isItemOnly(pose.held) || isPlant(pose.held) || isDoor(pose.held)) {
            // 1.5.1: a tool is gripped at the end of its handle (sprite pixel 2, 13) and points forward
            // and up out of the fist; other items sit in the hand by their lower middle.
            const ToolKind kind = toolKind(pose.held);
            const bool tool = kind != ToolKind::None || pose.held == HOE || pose.held == BRUSHER || pose.held == BOW;
            const Vec3 grip = tool ? Vec3(2.5f / 16 - 0.5f, 0.5f - 13.5f / 16, 0) : Vec3(0, -0.3f, 0);
            const float size = tool ? 11 : 8;  // model pixels across the sprite
            appendItemModel(tileFor(pose.held, F_SOUTH), 0.95f, 1 / 16.0f, [&](const Vec3& q) {
                // Sprite x runs forward (-z), sprite y up, its thickness across the hand (x).
                const Vec3 d = (q - grip) * size;
                Vec3 p(hand.x + d.z, hand.y + d.y, hand.z - d.x);
                return modelToWorld(pose, p, {6, 22, 0}, ra);
            }, out);
        } else {
            const float h = 2.6f;
            box(tileFor(pose.held, F_SOUTH), {hand.x - h, hand.y - h * 2, -1 - h}, {hand.x + h, hand.y, -1 + h}, rS, ra, 0.95f);
        }
    }
    return out;
}

}  // namespace vc
