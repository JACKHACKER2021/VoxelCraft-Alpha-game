#pragma once
// 2.8.0 preview: the VC language's model (VcModel): bones with a pivot, a rotation and a parent, each
// with cubes (origin, size, UV in a texture of any size, inflate, an own rotation). The same shape as a
// Bedrock geo.json, which it reads as it is; our own files are *.vcmodel.json in the same layout.
// A model becomes triangles (Vertex, as the rest of the engine draws) with buildVcModel.
#include <map>
#include <string>
#include <vector>

#include "Math.h"
#include "Mesher.h"

namespace vc {

class Json;

struct VcCube {
    float origin[3] = {0, 0, 0}, size[3] = {0, 0, 0};  // model pixels (1/16 block)
    float uv[2] = {0, 0};                               // the box layout's corner in the texture (pixels)
    float inflate = 0;
    bool rotated = false;
    float pivot[3] = {0, 0, 0}, rotation[3] = {0, 0, 0};  // degrees
};

struct VcBone {
    std::string name, parent;
    int parentIndex = -1;
    float pivot[3] = {0, 0, 0}, rotation[3] = {0, 0, 0};  // degrees
    std::vector<VcCube> cubes;
};

// 2.8.0: a free mesh part (from a GLB): triangles already in blocks (y up, the head towards -z, not
// mirrored), texture 0..1 of the model's own texture. Walking legs turn round `pivot` about x.
struct VcPart {
    std::string name;
    std::vector<float> tris;  // x, y, z, u, v per vertex, 3 vertices a triangle
    float pivot[3] = {0, 0, 0};
    float swing = 0;          // radians per unit of the walk's swing (legs +-1, arms the other way), 0: does not move
    float flap = 0;           // share of the wing beat, turning about z round `pivot` (wings), 0: none
    float attack = 0;         // radians per unit of the attack swing, about x round `pivot` (a slam)
    bool collar = false;      // drawn only while the creature wears a collar ("show": "collar")
};

struct VcModel {
    std::string id;
    int texW = 64, texH = 64;
    std::vector<VcBone> bones;
    std::vector<VcPart> parts;  // 2.8.0: GLB models
    bool animated = false;      // the GLB said how it moves ("extras": {"vc": ...})
    bool squash = false;        // squashes on landing and stretches in the air (the slime)
    float flapSpeed = 0, flapAmp = 0, flapRest = 0;  // the wing beat: sin(time * speed) * amp, rest on the ground
    int cubeCount() const;
    int bone(const std::string& name) const;  // -1 if none
};

// Extra rotations (degrees) per bone for one frame: walking legs, a turned head, sitting.
struct VcPose {
    std::map<std::string, Vec3> rotation;
    float walk = 0;  // radians: how far the legs of a GLB model swing now
    float fall = 0;  // radians: dying, the whole body tips over sideways round the feet
    float flap = 0;  // radians: the wing beat now
    float attack = 0;     // the attack swing now (Mob::attackAnim)
    float stretch = 1;    // squash models: height factor (the width shrinks to keep the volume)
    bool collar = false;  // show the collar parts
    std::vector<std::pair<int, int>> swaps;  // atlas tiles drawn as others (a villager's robe, a zombie face)
};

// Where the model's texture lies in the atlas (0..1).
struct VcTexRect {
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
};

// Reads every model of a geo.json (format 1.12+: "minecraft:geometry": [...], and the older
// "geometry.name": {...}) or a .vcmodel.json. Returns false with `error` set when nothing could be read.
bool parseVcModels(const Json& doc, std::vector<VcModel>& out, std::string& error);

// 2.8.0: reads a GLB (glTF 2.0 binary): every mesh of every node becomes a VcPart (node translation and
// scale applied); the embedded PNG (if any) is returned in `png`. Legs are found with guessVcLegs.
bool parseVcGlb(const uint8_t* data, size_t size, VcModel& out, std::vector<uint8_t>* png, std::string& error);
// Marks the low parts standing on the ground as legs (grouped into columns, each turning round its top).
void guessVcLegs(VcModel& model);
// How much of the texture's height the model uses (0..1), to keep only that part in the atlas.
float vcUsedTextureHeight(const VcModel& model);

// The model's triangles: model pixels to blocks, turned by `yaw` round the feet, at `feet`.
void buildVcModel(const VcModel& model, const VcPose& pose, const VcTexRect& tex, const Vec3& feet, float yaw, float light,
                  std::vector<Vertex>& out);

}  // namespace vc
