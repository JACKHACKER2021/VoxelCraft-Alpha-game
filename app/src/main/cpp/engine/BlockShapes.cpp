#include "BlockShapes.h"

#include "World.h"

namespace vc {

namespace {

constexpr float kThin = 3 / 16.0f;

// A slab along one side of the cell: 0 north (z = 0), 1 east (x = 1), 2 south (z = 1), 3 west (x = 0).
Box sideSlab(int side, float y0, float y1) {
    switch (side & 3) {
        case 0: return {0, y0, 0, 1, y1, kThin};
        case 1: return {1 - kThin, y0, 0, 1, y1, 1};
        case 2: return {0, y0, 1 - kThin, 1, y1, 1};
        default: return {0, y0, 0, kThin, y1, 1};
    }
}

}  // namespace

DoorState doorState(ItemId id) {
    int v = isDoor(id) ? (id - (id >= IRON_DOOR ? IRON_DOOR : OAK_DOOR)) : id - OAK_TRAPDOOR;
    return {v & 3, (v & 4) != 0, (v & 8) != 0};
}

ItemId doorId(ItemId base, DoorState s) { return (ItemId)(base + (s.facing & 3) + (s.open ? 4 : 0) + (s.upper ? 8 : 0)); }

bool fenceConnects(ItemId n) { return isFence(n) || isOpaque(n); }

bool cableConnects(ItemId n) {
    return isCable(n) || isSwitch(n) || isLamp(n) || isTnt(n) || n == POWERSTONE_BLOCK || (isDoor(n) && n >= IRON_DOOR) || isPowerstoneFurnace(n);
}

// 1.5.7: a torch is a thin stick; on a wall it leans against the block it hangs on.
static int torchBoxes(ItemId id, Box out[]) {
    const float lo = 7 / 16.0f, hi = 9 / 16.0f;
    if (id == TORCH) {
        out[0] = {lo, 0, lo, hi, 10 / 16.0f, hi};
        return 1;
    }
    // A 2 x 2 stick whose foot touches the wall; the mesher leans its top out from the wall.
    const float y0 = 3 / 16.0f, y1 = 13 / 16.0f, d = 2 / 16.0f;
    switch (id - TORCH_WALL) {
        case 0: out[0] = {lo, y0, 0, hi, y1, d}; break;          // on the wall to the north
        case 1: out[0] = {1 - d, y0, lo, 1, y1, hi}; break;      // east
        case 2: out[0] = {lo, y0, 1 - d, hi, y1, 1}; break;      // south
        default: out[0] = {0, y0, lo, d, y1, hi}; break;         // west
    }
    return 1;
}

int itemBoxes(ItemId id, Box out[kItemBoxes]) {
    if (isTorch(id)) return torchBoxes(TORCH, out);
    if (isFence(id)) {
        // Two posts joined by two rails, so a fence looks like a fence in the inventory.
        out[0] = {2 / 16.0f, 0, 6 / 16.0f, 6 / 16.0f, 1, 10 / 16.0f};
        out[1] = {0, 12 / 16.0f, 7 / 16.0f, 1, 15 / 16.0f, 9 / 16.0f};
        out[2] = {0, 6 / 16.0f, 7 / 16.0f, 1, 9 / 16.0f, 9 / 16.0f};
        out[3] = {10 / 16.0f, 0, 6 / 16.0f, 14 / 16.0f, 1, 10 / 16.0f};
        return 4;
    }
    if (isSkeletonHead(id)) {
        out[0] = {3 / 16.0f, 0, 3 / 16.0f, 13 / 16.0f, 10 / 16.0f, 13 / 16.0f};
        return 1;
    }
    if (isBalloon(id)) {
        out[0] = {3 / 16.0f, 4 / 16.0f, 3 / 16.0f, 13 / 16.0f, 1, 13 / 16.0f};
        return 1;
    }
    if (id == SNOW_LAYER) {
        out[0] = {0, 0, 0, 1, 2 / 16.0f, 1};
        return 1;
    }
    if (id == PARTY_LANTERN) {
        out[0] = {3 / 16.0f, 0, 3 / 16.0f, 13 / 16.0f, 13 / 16.0f, 13 / 16.0f};
        return 1;
    }
    if (isSlab(id) && !isDoubleSlab(id)) {
        out[0] = {0, 0, 0, 1, 0.5f, 1};
        return 1;
    }
    if (isStairs(id)) {
        out[0] = {0, 0, 0, 1, 0.5f, 1};
        out[1] = {0, 0.5f, 0, 1, 1, 0.5f};  // the tall side away from the viewer
        return 2;
    }
    out[0] = {0, 0, 0, 1, 1, 1};
    return 1;
}

int blockBoxes(const World& world, int x, int y, int z, ItemId id, bool collision, Box out[10]) {
    if (isTorch(id)) return collision ? 0 : torchBoxes(id, out);  // nothing to bump into
    if (isDoor(id)) {
        DoorState s = doorState(id);
        // Closed: a slab on the `facing` side. Open: swung a quarter turn on its hinge.
        out[0] = sideSlab(s.open ? s.facing + 1 : s.facing, 0, 1);
        return 1;
    }
    if (isTrapdoor(id)) {
        DoorState s = doorState(id);
        if (s.open) out[0] = sideSlab(s.facing, 0, 1);
        else out[0] = s.upper ? Box{0, 1 - kThin, 0, 1, 1, 1} : Box{0, 0, 0, 1, kThin, 1};
        return 1;
    }
    if (isChest(id)) {
        out[0] = {1 / 16.0f, 0, 1 / 16.0f, 15 / 16.0f, 14 / 16.0f, 15 / 16.0f};
        return 1;
    }
    if (isWallCable(id) || isWallSwitch(id)) {
        // On a wall: `side` is the wall (0 north z = 0, 1 east x = 1, 2 south z = 1, 3 west x = 0).
        const int side = wallSide(id);
        const bool alongX = side == 0 || side == 2;
        const float depth = isWallSwitch(id) ? 3 / 16.0f : (collision ? 0.01f : 1 / 16.0f);
        auto box = [&](float t0, float t1, float y0, float y1) {
            const float d0 = (side == 0 || side == 3) ? 0 : 1 - depth, d1 = d0 + depth;
            return alongX ? Box{t0, y0, d0, t1, y1, d1} : Box{d0, y0, t0, d1, y1, t1};
        };
        if (isWallSwitch(id)) {
            out[0] = box(4 / 16.0f, 12 / 16.0f, 3 / 16.0f, 13 / 16.0f);
            return 1;
        }
        if (collision) {
            out[0] = box(0, 1, 0, 1);
            return 1;
        }
        // The wire runs up, down and along the wall towards whatever it connects to.
        const float lo = 6.5f / 16, hi = 9.5f / 16;
        const int tx = alongX ? 1 : 0, tz = alongX ? 0 : 1;
        bool up = cableConnects(world.getBlock(x, y + 1, z)), down = cableConnects(world.getBlock(x, y - 1, z));
        bool minus = cableConnects(world.getBlock(x - tx, y, z - tz)), plus = cableConnects(world.getBlock(x + tx, y, z + tz));
        if (!up && !down && !minus && !plus) up = down = true;  // alone: a straight piece up the wall
        if ((up || down) && !minus && !plus) up = down = true;
        if ((minus || plus) && !up && !down) minus = plus = true;
        int count = 0;
        out[count++] = box(lo, hi, lo, hi);
        if (up) out[count++] = box(lo, hi, hi, 1);
        if (down) out[count++] = box(lo, hi, 0, lo);
        if (minus) out[count++] = box(0, lo, lo, hi);
        if (plus) out[count++] = box(hi, 1, lo, hi);
        return count;
    }
    if (isCeilingCable(id)) {
        // 1.5.7: the floor wire turned upside down, under a ceiling.
        const float h = collision ? 0.01f : 1 / 16.0f, lo = 6.5f / 16, hi = 9.5f / 16, t = 1 - h;
        if (collision) {
            out[0] = {0, t, 0, 1, 1, 1};
            return 1;
        }
        bool n = cableConnects(world.getBlock(x, y, z - 1)), s = cableConnects(world.getBlock(x, y, z + 1));
        bool w = cableConnects(world.getBlock(x - 1, y, z)), e = cableConnects(world.getBlock(x + 1, y, z));
        if ((n || s) && !w && !e) n = s = true;
        if ((w || e) && !n && !s) w = e = true;
        int count = 0;
        out[count++] = {lo, t, lo, hi, 1, hi};
        if (n) out[count++] = {lo, t, 0, hi, 1, lo};
        if (s) out[count++] = {lo, t, hi, hi, 1, 1};
        if (w) out[count++] = {0, t, lo, lo, 1, hi};
        if (e) out[count++] = {hi, t, lo, 1, 1, hi};
        if (count == 1) out[count++] = {4 / 16.0f, t, lo, 12 / 16.0f, 1, hi};
        return count;
    }
    if (isCable(id)) {
        // A thin wire on the floor that runs towards whatever it connects to: straight,
        // round a corner, a T or a cross. Alone it is a short piece.
        const float h = collision ? 0.01f : 1 / 16.0f, lo = 6.5f / 16, hi = 9.5f / 16;
        if (collision) {
            out[0] = {0, 0, 0, 1, h, 1};
            return 1;
        }
        bool n = cableConnects(world.getBlock(x, y, z - 1)), s = cableConnects(world.getBlock(x, y, z + 1));
        bool w = cableConnects(world.getBlock(x - 1, y, z)), e = cableConnects(world.getBlock(x + 1, y, z));
        // One neighbour: the wire still goes all the way through the cell.
        if ((n || s) && !w && !e) n = s = true;
        if ((w || e) && !n && !s) w = e = true;
        int count = 0;
        out[count++] = {lo, 0, lo, hi, h, hi};
        if (n) out[count++] = {lo, 0, 0, hi, h, lo};
        if (s) out[count++] = {lo, 0, hi, hi, h, 1};
        if (w) out[count++] = {0, 0, lo, lo, h, hi};
        if (e) out[count++] = {hi, 0, lo, 1, h, hi};
        if (count == 1) out[count++] = {4 / 16.0f, 0, lo, 12 / 16.0f, h, hi};  // a lone piece
        // 1.5.7: a wall cable just above: the wire bends up the wall to meet it.
        const ItemId above = world.getBlock(x, y + 1, z);
        if (isWallCable(above) && count < 10) {
            const int side = wallSide(above);
            const float d = 1 / 16.0f;
            if (side == 0) out[count++] = {lo, 0, 0, hi, 1, d};
            else if (side == 1) out[count++] = {1 - d, 0, lo, 1, 1, hi};
            else if (side == 2) out[count++] = {lo, 0, 1 - d, hi, 1, 1};
            else out[count++] = {0, 0, lo, d, 1, hi};
            if (side == 0) out[count++] = {lo, 0, 0, hi, h, lo};
            else if (side == 1) out[count++] = {hi, 0, lo, 1, h, hi};
            else if (side == 2) out[count++] = {lo, 0, hi, hi, h, 1};
            else out[count++] = {0, 0, lo, lo, h, hi};
        }
        return count;
    }
    if (isBalloon(id)) {
        // A round-ish balloon floating on its string.
        out[0] = {3 / 16.0f, 6 / 16.0f, 3 / 16.0f, 13 / 16.0f, 1, 13 / 16.0f};
        if (collision) return 1;
        out[1] = {7.5f / 16, 0, 7.5f / 16, 8.5f / 16, 6 / 16.0f, 8.5f / 16};
        return 2;
    }
    if (id == SNOW_LAYER) {
        out[0] = {0, 0, 0, 1, collision ? 0.01f : 2 / 16.0f, 1};
        return 1;
    }
    if (id == PARTY_LANTERN) {
        out[0] = {4 / 16.0f, 0, 4 / 16.0f, 12 / 16.0f, 10 / 16.0f, 12 / 16.0f};
        return 1;
    }
    if (isSlab(id)) {
        out[0] = slabHalf(id) == 1 ? Box{0, 0.5f, 0, 1, 1, 1} : Box{0, 0, 0, 1, 0.5f, 1};
        return 1;
    }
    if (isStairs(id)) {
        // A half slab plus a quarter block along the back (tall) side; upside down both flip.
        const int v = stairsVariant(id), facing = v & 3;
        const bool flip = (v & 4) != 0;
        out[0] = flip ? Box{0, 0.5f, 0, 1, 1, 1} : Box{0, 0, 0, 1, 0.5f, 1};
        const float y0 = flip ? 0 : 0.5f, y1 = flip ? 0.5f : 1;
        switch (facing) {
            case 0: out[1] = {0, y0, 0, 1, y1, 0.5f}; break;
            case 1: out[1] = {0.5f, y0, 0, 1, y1, 1}; break;
            case 2: out[1] = {0, y0, 0.5f, 1, y1, 1}; break;
            default: out[1] = {0, y0, 0, 0.5f, y1, 1}; break;
        }
        return 2;
    }
    if (id == FARMLAND) {
        out[0] = {0, 0, 0, 1, 15 / 16.0f, 1};
        return 1;
    }
    if (isSwitch(id)) {
        // A small stone plate on the floor (the lever is drawn on its top texture).
        out[0] = {4 / 16.0f, 0, 3 / 16.0f, 12 / 16.0f, 3 / 16.0f, 13 / 16.0f};
        return 1;
    }
    if (id == CAKE) {
        out[0] = {1 / 16.0f, 0, 1 / 16.0f, 15 / 16.0f, 8 / 16.0f, 15 / 16.0f};
        return 1;
    }
    if (isBed(id)) {
        out[0] = {0, 0, 0, 1, 9 / 16.0f, 1};
        return 1;
    }
    if (id == CACTUS) {
        out[0] = {1 / 16.0f, 0, 1 / 16.0f, 15 / 16.0f, 1, 15 / 16.0f};
        return 1;
    }
    if (isSkeletonHead(id)) {
        out[0] = {4 / 16.0f, 0, 4 / 16.0f, 12 / 16.0f, 8 / 16.0f, 12 / 16.0f};
        return 1;
    }
    if (isFence(id)) {
        const float lo = 6 / 16.0f, hi = 10 / 16.0f, top = collision ? 1.5f : 1.0f;
        const bool n = fenceConnects(world.getBlock(x, y, z - 1)), s = fenceConnects(world.getBlock(x, y, z + 1));
        const bool w = fenceConnects(world.getBlock(x - 1, y, z)), e = fenceConnects(world.getBlock(x + 1, y, z));
        int count = 0;
        out[count++] = {lo, 0, lo, hi, top, hi};  // the post
        if (collision) {
            // One tall box per direction the fence runs.
            if (n) out[count++] = {lo, 0, 0, hi, top, lo};
            if (s) out[count++] = {lo, 0, hi, hi, top, 1};
            if (w) out[count++] = {0, 0, lo, lo, top, hi};
            if (e) out[count++] = {hi, 0, lo, 1, top, hi};
            return count;
        }
        // Drawn: two rails towards each connected side.
        const float a0 = 7 / 16.0f, a1 = 9 / 16.0f;
        auto rails = [&](float x0, float z0, float x1, float z1) {
            out[count++] = {x0, 6 / 16.0f, z0, x1, 9 / 16.0f, z1};
            out[count++] = {x0, 12 / 16.0f, z0, x1, 15 / 16.0f, z1};
        };
        if (n) rails(a0, 0, a1, lo);
        if (s) rails(a0, hi, a1, 1);
        if (w) rails(0, a0, lo, a1);
        if (e) rails(hi, a0, 1, a1);
        return count;
    }
    return 0;
}

}  // namespace vc
