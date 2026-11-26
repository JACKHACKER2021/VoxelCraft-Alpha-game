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

struct VcModel {
    std::string id;
    int texW = 64, texH = 64;
    std::vector<VcBone> bones;
    int cubeCount() const;
    int bone(const std::string& name) const;  // -1 if none
};

// Extra rotations (degrees) per bone for one frame: walking legs, a turned head, sitting.
struct VcPose {
    std::map<std::string, Vec3> rotation;
};

// Where the model's texture lies in the atlas (0..1).
struct VcTexRect {
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
};

// Reads every model of a geo.json (format 1.12+: "minecraft:geometry": [...], and the older
// "geometry.name": {...}) or a .vcmodel.json. Returns false with `error` set when nothing could be read.
bool parseVcModels(const Json& doc, std::vector<VcModel>& out, std::string& error);

// The model's triangles: model pixels to blocks, turned by `yaw` round the feet, at `feet`.
void buildVcModel(const VcModel& model, const VcPose& pose, const VcTexRect& tex, const Vec3& feet, float yaw, float light,
                  std::vector<Vertex>& out);

}  // namespace vc
