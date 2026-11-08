// VoxelCraft Rails & Trains mod  -  public header
// Build as libvoxelcraft_rails.so  (see CMakeLists.txt)
//
// The host (VoxelCraft) calls  vc_mod_init(&hostApi)  once after dlopen().
// Everything the mod needs from the engine goes through the small C-ABI table
// VcHostApi below - if your engine's registration API looks different, only
// the 4 adapter calls in voxelcraft_rails.cpp (registerAll) need to be touched.
#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#if defined(_WIN32)
  #define VC_EXPORT extern "C" __declspec(dllexport)
#else
  #define VC_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ------------------------------------------------------------------ C ABI
extern "C" {

#define VC_MOD_ABI_VERSION 1

typedef struct VcBlockDef {
    const char* id;            // "vcrails:rail_straight"
    const char* display_name;
    const char* model_path;    // GLB, relative to the mod's assets dir
    uint32_t    flags;         // VC_BLOCK_*
    float       hardness;
} VcBlockDef;

enum { VC_BLOCK_NON_SOLID = 1, VC_BLOCK_ROTATABLE = 2, VC_BLOCK_ATTACH_RAIL = 4,
       VC_BLOCK_USE_TOGGLE = 8 /* right-click calls vc_rails_toggle_switch */ };

typedef struct VcItemDef {
    const char* id;
    const char* display_name;
    int         max_stack;
} VcItemDef;

typedef struct VcEntityDef {
    const char* id;            // "vcrails:train_locomotive"
    const char* model_path;
    float       width, height, length;   // collision box (m)
    int         seat_count;
    const char* wheel_node_prefix;       // "wheel_" -> engine spins these nodes
} VcEntityDef;

typedef struct VcRecipeDef {
    const char* id;
    const char* output;
    int         output_count;
    const char* rows[3];       // shaped pattern, '.' = empty; NULL row = unused
    const char* keys;          // "S=vc:steel_ingot;P=vc:spruce_planks"
} VcRecipeDef;

typedef struct VcHostApi {
    uint32_t abi_version;
    void (*log)(int level, const char* msg);
    int  (*register_item)  (const VcItemDef*);
    int  (*register_block) (const VcBlockDef*);
    int  (*register_entity)(const VcEntityDef*);
    int  (*register_recipe)(const VcRecipeDef*);
} VcHostApi;

VC_EXPORT int         vc_mod_init(const VcHostApi* host);   // 0 = ok
VC_EXPORT void        vc_mod_shutdown(void);
VC_EXPORT const char* vc_mod_name(void);
VC_EXPORT uint32_t    vc_mod_abi_version(void);
// Right-click on a switch block: flips *diverge (0/1). shape = RailShape value. Returns 1 if toggled.
VC_EXPORT int         vc_rails_toggle_switch(int shape, int* diverge);

} // extern "C"

// ------------------------------------------------------------------ C++ API
namespace vcrails {

// Constants shared with tools/gen_models.py - keep in sync.
constexpr float kRailBed     = 0.08f;
constexpr float kRailTop     = 0.247f;   // rail head height above block bottom
constexpr float kGauge       = 0.6f;
constexpr float kCarLength   = 4.0f;
constexpr float kCarWidth    = 1.4f;
constexpr float kCarPitch    = 4.45f;    // centre-to-centre distance in a consist
constexpr float kFloorY      = 0.52f;    // walkable floor above rail head
constexpr float kWheelRadius = 0.18f;

struct Vec3 { float x = 0, y = 0, z = 0; };

// North = -Z, East = +X, South = +Z, West = -X
enum class Heading : uint8_t { North = 0, East = 1, South = 2, West = 3 };
Heading opposite(Heading h);
void    headingStep(Heading h, int& dx, int& dz);

enum class RailShape : uint8_t { Straight, CurveLeft, CurveRight, Stop, SlopeUp, SlopeDown, SwitchRight, SwitchLeft };
enum class CarType   : uint8_t { Locomotive, Passenger, Freight };

struct RailBlock { RailShape shape = RailShape::Straight; uint8_t rot = 0; bool diverge = false; };   // rot: 0..3 (x90deg about Y); diverge = switch state

inline bool isSwitch(RailShape s) { return s == RailShape::SwitchRight || s == RailShape::SwitchLeft; }
inline bool toggleSwitch(RailBlock& b) { if (!isSwitch(b.shape)) return false; b.diverge = !b.diverge; return true; }
constexpr float kLeverAngleDeg = 35.0f;   // node "switch_lever": rotate about Z, -35 = straight, +35 = diverging
// Shape a train actually follows through this block (switch -> straight or curve)
RailShape effectiveShape(const RailBlock& b, bool branch);

struct RailPort { Heading edge; int height; };    // height: 0 low edge, 1 raised edge (+1 block)
struct RailPorts { RailPort in, out; bool deadEnd; RailPort alt{Heading::North, 0}; bool hasAlt = false; };   // alt = diverging exit of a switch

// Where a rail model sits for rot=0: enters on North edge, runs towards South.
RailPorts railPorts(const RailBlock& b);
float     railLength(RailShape s);
float     railMaxT(RailShape s);                           // Stop ends at 0.80
// Position of the rail head (where wheels touch) + unit direction, in block-local space
// (origin = block centre, y=0 block bottom). t in [0, railMaxT].
void      sampleRail(const RailBlock& b, float t, Vec3& pos, Vec3& dir, bool branch = false);

struct IRailWorld {
    virtual ~IRailWorld() = default;
    virtual bool getRail(int x, int y, int z, RailBlock& out) const = 0;
};

// ---- recipes / content tables (also used by tests) ------------------------
struct RecipeSpec { const char* id; const char* out; int count; const char* rows[3]; const char* keys; };
const std::vector<RecipeSpec>& recipeTable();

// ---- train simulation -------------------------------------------------------
struct SeatSlot { Vec3 local; };
const std::vector<SeatSlot>& seatsFor(CarType t);

struct Car {
    CarType type;
    Vec3    pos;          // rail-head level, car centre
    float   yaw = 0;      // radians, 0 = facing +Z
    float   pitch = 0;    // radians, + = nose up
    float   wheelAngle = 0;
    std::vector<uint32_t> passengers;   // entity id per seat (0 = free)
};

class Train {
public:
    explicit Train(std::vector<CarType> consist);

    // Put the head of the train at block (x,y,z), moving along block 'b' forwards.
    bool place(const IRailWorld& w, int x, int y, int z, bool reversed = false);
    void setThrottle(float v)  { throttle_ = v < 0 ? 0 : (v > 1 ? 1 : v); }   // 0..1 (reverse = place(..., reversed=true))
    void setBrake(bool on)     { brake_ = on; }
    void update(float dt, const IRailWorld& w);

    int  board(uint32_t entityId, CarType preferred = CarType::Passenger);  // car index or -1
    bool alight(uint32_t entityId, Vec3* exitPos = nullptr);                // exit next to the car
    bool seatWorldPos(uint32_t entityId, Vec3& out) const;

    float speed() const { return speed_; }
    const std::vector<Car>& cars() const { return cars_; }
    bool  atBuffer() const { return atBuffer_; }

private:
    bool advance(float dist, const IRailWorld& w);
    bool findNext(const IRailWorld& w, bool& ok);
    void recordHead();
    void layoutCars();

    std::vector<Car> cars_;
    // head state
    int   cx_ = 0, cy_ = 0, cz_ = 0;
    RailBlock blk_{};
    float t_ = 0;
    bool  branch_ = false;            // current block: following the diverging path of a switch
    bool  rev_ = false;               // travelling against the block's own direction
    float speed_ = 0, throttle_ = 0;
    bool  brake_ = false, atBuffer_ = false;
    std::deque<Vec3> trail_;          // head positions, newest at front
    static constexpr float kMaxSpeed = 8.0f;   // m/s
    static constexpr float kAccel    = 2.0f;
};

} // namespace vcrails
