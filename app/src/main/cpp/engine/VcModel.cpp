#include "VcModel.h"

#include <cmath>

#include "Json.h"

namespace vc {

int VcModel::cubeCount() const {
    int n = 0;
    for (const VcBone& b : bones) n += (int)b.cubes.size();
    return n;
}

int VcModel::bone(const std::string& name) const {
    for (size_t i = 0; i < bones.size(); ++i)
        if (bones[i].name == name) return (int)i;
    return -1;
}

namespace {

void read3(const Json& a, float out[3]) {
    for (int k = 0; k < 3; ++k) out[k] = (float)a[(size_t)k].num(out[k]);
}

bool readModel(const Json& g, const std::string& id, VcModel& m) {
    m.id = id;
    const Json& d = g["description"];
    m.texW = (int)d["texture_width"].num(g["texturewidth"].num(64));
    m.texH = (int)d["texture_height"].num(g["textureheight"].num(64));
    const Json& bones = g["bones"];
    for (size_t i = 0; i < bones.size(); ++i) {
        const Json& b = bones[i];
        VcBone bone;
        bone.name = b["name"].str("bone" + std::to_string(i));
        bone.parent = b["parent"].str("");
        if (b["pivot"].isArray()) read3(b["pivot"], bone.pivot);
        if (b["rotation"].isArray()) read3(b["rotation"], bone.rotation);
        for (size_t c = 0; c < b["cubes"].size(); ++c) {
            const Json& cj = b["cubes"][c];
            VcCube cube;
            read3(cj["origin"], cube.origin);
            read3(cj["size"], cube.size);
            if (cj["uv"].isArray()) cube.uv[0] = (float)cj["uv"][0].num(0), cube.uv[1] = (float)cj["uv"][1].num(0);
            cube.inflate = (float)cj["inflate"].num(0);
            if (cj["rotation"].isArray()) {
                cube.rotated = true;
                read3(cj["rotation"], cube.rotation);
                if (cj["pivot"].isArray()) read3(cj["pivot"], cube.pivot);
            }
            bone.cubes.push_back(cube);
        }
        m.bones.push_back(bone);
    }
    for (VcBone& b : m.bones) b.parentIndex = b.parent.empty() ? -1 : m.bone(b.parent);
    return !m.bones.empty();
}

// Turns `p` round `pivot` by `deg` (x, then y, then z), in model pixels.
Vec3 turn(Vec3 p, const float pivot[3], const Vec3& deg) {
    p = p - Vec3(pivot[0], pivot[1], pivot[2]);
    const float rx = deg.x * 3.14159265f / 180, ry = deg.y * 3.14159265f / 180, rz = deg.z * 3.14159265f / 180;
    if (rx != 0) p = Vec3(p.x, p.y * std::cos(rx) - p.z * std::sin(rx), p.y * std::sin(rx) + p.z * std::cos(rx));
    if (ry != 0) p = Vec3(p.x * std::cos(ry) + p.z * std::sin(ry), p.y, -p.x * std::sin(ry) + p.z * std::cos(ry));
    if (rz != 0) p = Vec3(p.x * std::cos(rz) - p.y * std::sin(rz), p.x * std::sin(rz) + p.y * std::cos(rz), p.z);
    return p + Vec3(pivot[0], pivot[1], pivot[2]);
}

}  // namespace

bool parseVcModels(const Json& doc, std::vector<VcModel>& out, std::string& error) {
    const size_t before = out.size();
    const Json& list = doc["minecraft:geometry"].isArray() ? doc["minecraft:geometry"] : doc["vc:models"];
    for (size_t i = 0; i < list.size(); ++i) {
        VcModel m;
        if (readModel(list[i], list[i]["description"]["identifier"].str("model" + std::to_string(i)), m)) out.push_back(m);
    }
    for (const std::string& k : doc.keys())  // the older format: "geometry.name": {...}
        if (k.rfind("geometry.", 0) == 0) {
            VcModel m;
            if (readModel(doc[k], k.substr(0, k.find(':')), m)) out.push_back(m);
        }
    if (doc["bones"].isArray()) {  // a bare .vcmodel.json: {"id", "texture_size", "bones"}
        VcModel m;
        Json desc = Json::object();
        if (readModel(doc, doc["id"].str("model"), m)) {
            if (doc["texture_size"].isArray()) m.texW = (int)doc["texture_size"][0].num(64), m.texH = (int)doc["texture_size"][1].num(64);
            out.push_back(m);
        }
    }
    if (out.size() == before) error = "žádný model (chybí bones)";
    return out.size() > before;
}

void buildVcModel(const VcModel& model, const VcPose& pose, const VcTexRect& tex, const Vec3& feet, float yaw, float light,
                  std::vector<Vertex>& out) {
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    for (size_t bi = 0; bi < model.bones.size(); ++bi) {
        const VcBone& bone = model.bones[bi];
        for (const VcCube& c : bone.cubes) {
            if (c.size[0] == 0 && c.size[1] == 0 && c.size[2] == 0) continue;
            const float g = c.inflate;
            const float x0 = c.origin[0] - g, y0 = c.origin[1] - g, z0 = c.origin[2] - g;
            const float x1 = c.origin[0] + c.size[0] + g, y1 = c.origin[1] + c.size[1] + g, z1 = c.origin[2] + c.size[2] + g;
            // The box layout: w (x), h (y), d (z) from the uv corner.
            const float w = c.size[0], h = c.size[1], d = c.size[2], u = c.uv[0], v = c.uv[1];
            struct Face { Vec3 p[4]; float ru, rv, rw, rh, shade; };
            const Face faces[6] = {
                {{{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}}, u + d, v, w, d, 1.0f},           // top
                {{{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}}, u + d + w, v, w, d, 0.5f},       // bottom
                {{{x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {x0, y0, z0}}, u + d, v + d, w, h, 0.8f},       // north (the front)
                {{{x1, y1, z1}, {x0, y1, z1}, {x0, y0, z1}, {x1, y0, z1}}, u + 2 * d + w, v + d, w, h, 0.8f},  // south
                {{{x0, y1, z1}, {x0, y1, z0}, {x0, y0, z0}, {x0, y0, z1}}, u, v + d, d, h, 0.6f},           // west
                {{{x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {x1, y0, z0}}, u + d + w, v + d, d, h, 0.6f},   // east
            };
            for (const Face& f : faces) {
                Vec3 q[4];
                for (int k = 0; k < 4; ++k) {
                    Vec3 p = f.p[k];
                    if (c.rotated) p = turn(p, c.pivot, Vec3(c.rotation[0], c.rotation[1], c.rotation[2]));
                    // The bone and its parents, the innermost first.
                    for (int b = (int)bi; b >= 0; b = model.bones[(size_t)b].parentIndex) {
                        const VcBone& bb = model.bones[(size_t)b];
                        Vec3 r(bb.rotation[0], bb.rotation[1], bb.rotation[2]);
                        auto it = pose.rotation.find(bb.name);
                        if (it != pose.rotation.end()) r = r + it->second;
                        if (r.x != 0 || r.y != 0 || r.z != 0) p = turn(p, bb.pivot, r);
                    }
                    // Model pixels (x mirrored like Bedrock) to blocks, turned by yaw, at the feet.
                    const Vec3 m(-p.x / 16.0f, p.y / 16.0f, p.z / 16.0f);
                    q[k] = feet + Vec3(m.x * cy - m.z * sy, m.y, m.x * sy + m.z * cy);
                }
                const float uu[4] = {f.ru, f.ru + f.rw, f.ru + f.rw, f.ru}, vv[4] = {f.rv, f.rv, f.rv + f.rh, f.rv + f.rh};
                Vertex vtx[4];
                for (int k = 0; k < 4; ++k) {
                    const float tu = tex.u0 + (uu[k] / model.texW) * (tex.u1 - tex.u0), tv = tex.v0 + (vv[k] / model.texH) * (tex.v1 - tex.v0);
                    vtx[k] = Vertex{q[k].x, q[k].y, q[k].z, tu, tv, f.shade * light};
                }
                out.insert(out.end(), {vtx[0], vtx[1], vtx[2], vtx[0], vtx[2], vtx[3]});
            }
        }
    }
}

}  // namespace vc
