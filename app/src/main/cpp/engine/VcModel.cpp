#include "VcModel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Json.h"
#include "Blocks.h"

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

bool parseVcGlb(const uint8_t* data, size_t size, VcModel& out, std::vector<uint8_t>* png, std::string& error) {
    auto u32 = [&](size_t at) { uint32_t v = 0; std::memcpy(&v, data + at, 4); return v; };
    if (size < 28 || std::memcmp(data, "glTF", 4) != 0 || u32(4) != 2) return error = "není GLB 2.0", false;
    const size_t jsonLen = u32(12);
    if (20 + jsonLen + 8 > size || u32(16) != 0x4E4F534Au) return error = "poškozený GLB", false;
    Json j;
    if (!Json::parse(std::string((const char*)data + 20, jsonLen), j, error)) return false;
    const size_t binAt = 20 + jsonLen + 8, binLen = u32(20 + jsonLen);
    if (binAt + binLen > size) return error = "poškozený GLB", false;
    const uint8_t* bin = data + binAt;
    // The bytes of an accessor: (pointer, count, components, component type).
    auto view = [&](const Json& acc, const uint8_t*& p, size_t& count, size_t& stride) {
        const Json& bv = j["bufferViews"][(size_t)acc["bufferView"].num(0)];
        const size_t off = (size_t)bv["byteOffset"].num(0) + (size_t)acc["byteOffset"].num(0);
        count = (size_t)acc["count"].num(0);
        const int type = (int)acc["componentType"].num(5126);
        const size_t comps = acc["type"].str("") == "VEC3" ? 3 : acc["type"].str("") == "VEC2" ? 2 : 1;
        const size_t natural = comps * (type == 5126 || type == 5125 ? 4 : type == 5123 ? 2 : 1);
        stride = (size_t)bv["byteStride"].num((double)natural);
        p = bin + off;
        return off + (count ? (count - 1) * stride + natural : 0) <= binLen;
    };
    const Json& nodes = j["nodes"];
    for (size_t n = 0; n < nodes.size(); ++n) {
        const Json& node = nodes[n];
        if (!node["mesh"].isNumber()) continue;
        float t[3] = {0, 0, 0}, s[3] = {1, 1, 1};
        const Json& ex = node["extras"]["vc"];
        if (node["translation"].isArray()) read3(node["translation"], t);
        if (node["scale"].isArray()) read3(node["scale"], s);
        const Json& prims = j["meshes"][(size_t)node["mesh"].num(0)]["primitives"];
        for (size_t pi = 0; pi < prims.size(); ++pi) {
            const Json& pr = prims[pi];
            const uint8_t *pp = nullptr, *tp = nullptr, *ip = nullptr;
            size_t pc = 0, ps = 0, tc = 0, ts = 0, ic = 0, is = 0;
            if (!view(j["accessors"][(size_t)pr["attributes"]["POSITION"].num(0)], pp, pc, ps)) return error = "poškozený GLB", false;
            const bool hasUv = pr["attributes"]["TEXCOORD_0"].isNumber();
            if (hasUv && (!view(j["accessors"][(size_t)pr["attributes"]["TEXCOORD_0"].num(0)], tp, tc, ts) || tc != pc)) return error = "poškozený GLB", false;
            const bool indexed = pr["indices"].isNumber();
            int itype = 5123;
            if (indexed) {
                const Json& ia = j["accessors"][(size_t)pr["indices"].num(0)];
                itype = (int)ia["componentType"].num(5123);
                if (!view(ia, ip, ic, is)) return error = "poškozený GLB", false;
            }
            VcPart part;
            part.name = node["name"].str("part" + std::to_string(out.parts.size()));
            if (ex.isObject()) {
                part.swing = (float)ex["swing"].num(0), part.flap = (float)ex["flap"].num(0);
                if (ex["pivot"].isArray()) read3(ex["pivot"], part.pivot);
                out.animated = true;
            }
            const size_t total = indexed ? ic : pc;
            for (size_t k = 0; k + 2 < total + 2 && k < total; ++k) {
                size_t idx = k;
                if (indexed) {
                    if (itype == 5125) { uint32_t v; std::memcpy(&v, ip + k * is, 4); idx = v; }
                    else if (itype == 5123) { uint16_t v; std::memcpy(&v, ip + k * is, 2); idx = v; }
                    else idx = ip[k * is];
                }
                if (idx >= pc) return error = "poškozený GLB", false;
                float p[3], uv[2] = {0, 0};
                std::memcpy(p, pp + idx * ps, 12);
                if (hasUv) std::memcpy(uv, tp + idx * ts, 8);
                part.tris.insert(part.tris.end(), {p[0] * s[0] + t[0], p[1] * s[1] + t[1], p[2] * s[2] + t[2], uv[0], uv[1]});
            }
            part.tris.resize(part.tris.size() / 15 * 15);
            if (!part.tris.empty()) out.parts.push_back(std::move(part));
        }
    }
    if (out.parts.empty()) return error = "GLB nemá žádnou síť", false;
    if (png && j["images"].isArray() && j["images"].size() > 0 && j["images"][0]["bufferView"].isNumber()) {
        const Json& bv = j["bufferViews"][(size_t)j["images"][0]["bufferView"].num(0)];
        const size_t off = (size_t)bv["byteOffset"].num(0), len = (size_t)bv["byteLength"].num(0);
        if (off + len <= binLen) png->assign(bin + off, bin + off + len);
    }
    out.id = j["nodes"][0]["name"].str("model");
    for (size_t n = 0; n < nodes.size(); ++n) {
        const Json& ex = nodes[n]["extras"]["vc"];
        if (ex.isObject()) out.animated = true;  // its moves are given (none is an answer too)
        if (!ex["flapSpeed"].isNumber()) continue;
        out.flapSpeed = (float)ex["flapSpeed"].num(0), out.flapAmp = (float)ex["flapAmp"].num(0), out.flapRest = (float)ex["flapRest"].num(out.flapAmp);
        out.animated = true;
    }
    return true;
}

void guessVcLegs(VcModel& model) {
    float top = 0;
    struct Box { float lo[3], hi[3]; };
    std::vector<Box> boxes;
    for (const VcPart& p : model.parts) {
        Box b{{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
        for (size_t i = 0; i < p.tris.size(); i += 5)
            for (int k = 0; k < 3; ++k) b.lo[k] = std::min(b.lo[k], p.tris[i + k]), b.hi[k] = std::max(b.hi[k], p.tris[i + k]);
        boxes.push_back(b);
        top = std::max(top, b.hi[1]);
    }
    // The legs: columns of low parts (a leg, its hoof, toes) standing on the ground under the body.
    struct Column { float x, z, lo = 1e9f, hi = -1e9f; std::vector<size_t> parts; };
    std::vector<Column> cols;
    for (size_t i = 0; i < boxes.size(); ++i) {
        const Box& b = boxes[i];
        if (b.hi[1] >= top * 0.5f || b.lo[1] > top * 0.3f) continue;
        const float cx = (b.lo[0] + b.hi[0]) / 2, cz = (b.lo[2] + b.hi[2]) / 2;
        Column* col = nullptr;
        for (Column& c : cols)
            if ((c.x - cx) * (c.x - cx) + (c.z - cz) * (c.z - cz) < 0.012f) col = &c;
        if (!col) cols.push_back(Column{cx, cz}), col = &cols.back();
        col->lo = std::min(col->lo, b.lo[1]), col->hi = std::max(col->hi, b.hi[1]);
        col->parts.push_back(i);
    }
    cols.erase(std::remove_if(cols.begin(), cols.end(), [](const Column& c) { return c.lo > 0.03f || c.hi - c.lo < 0.12f; }), cols.end());
    for (const Column& c : cols)
        for (size_t i : c.parts) {
            VcPart& p = model.parts[i];
            p.pivot[0] = c.x, p.pivot[1] = c.hi, p.pivot[2] = c.z;
            // Front left and back right swing together, as the old models did; two legs: left against right.
            p.swing = (cols.size() <= 2 ? c.x < 0 : (c.x < 0) == (c.z < 0)) ? 1.0f : -1.0f;
        }
}

float vcUsedTextureHeight(const VcModel& model) {
    float v = 0;
    for (const VcPart& p : model.parts)
        for (size_t i = 4; i < p.tris.size(); i += 5) v = std::max(v, p.tris[i]);
    return std::min(1.0f, v);
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
    // 2.8.0: GLB parts (already in blocks, not mirrored).
    const float cw = std::cos(pose.walk), sw = std::sin(pose.walk), cf = std::cos(pose.fall), sf = std::sin(pose.fall);
    for (const VcPart& part : model.parts) {
        const float a = part.swing * pose.walk, ca = part.swing ? std::cos(a) : 1, sa = part.swing ? std::sin(a) : 0;
        const float fa = part.flap * pose.flap, cfa = std::cos(fa), sfa = std::sin(fa);
        (void)cw, (void)sw;
        for (size_t i = 0; i + 15 <= part.tris.size(); i += 15) {
            Vec3 q[3];
            float uv[3][2];
            for (int k = 0; k < 3; ++k) {
                const float* f = &part.tris[i + (size_t)k * 5];
                Vec3 p(f[0], f[1], f[2]);
                if (sa != 0) {
                    const float y = p.y - part.pivot[1], z = p.z - part.pivot[2];
                    p = Vec3(p.x, part.pivot[1] + y * ca - z * sa, part.pivot[2] + y * sa + z * ca);
                }
                if (fa != 0) {
                    const float x = p.x - part.pivot[0], y = p.y - part.pivot[1];
                    p = Vec3(part.pivot[0] + x * cfa - y * sfa, part.pivot[1] + x * sfa + y * cfa, p.z);
                }
                if (pose.fall != 0) p = Vec3(p.x * cf - p.y * sf, p.x * sf + p.y * cf + sf * 0.3f, p.z);
                q[k] = p, uv[k][0] = f[3], uv[k][1] = f[4];
            }
            // The shade from the face's direction, as the other models: top bright, bottom dark, sides between.
            const Vec3 e1 = q[1] - q[0], e2 = q[2] - q[0];
            const Vec3 nrm(e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x);
            const float ax = std::fabs(nrm.x), ay = std::fabs(nrm.y), az = std::fabs(nrm.z);
            const float shade = ay >= ax && ay >= az ? (nrm.y > 0 ? 1.0f : 0.5f) : az >= ax ? 0.8f : 0.6f;
            float du = 0, dv = 0;
            if (!pose.swaps.empty()) {
                const float cu = (uv[0][0] + uv[1][0] + uv[2][0]) / 3, cv = (uv[0][1] + uv[1][1] + uv[2][1]) / 3;
                const int tile = (int)(cv * kAtlasCols) * kAtlasCols + (int)(cu * kAtlasCols);
                for (const auto& [from, to] : pose.swaps)
                    if (tile == from) du = (float)(to % kAtlasCols - from % kAtlasCols) / kAtlasCols, dv = (float)(to / kAtlasCols - from / kAtlasCols) / kAtlasCols;
            }
            for (int k = 0; k < 3; ++k) uv[k][0] += du, uv[k][1] += dv;
            for (int k = 0; k < 3; ++k) {
                const Vec3 w = feet + Vec3(q[k].x * cy - q[k].z * sy, q[k].y, q[k].x * sy + q[k].z * cy);
                out.push_back(Vertex{w.x, w.y, w.z, tex.u0 + uv[k][0] * (tex.u1 - tex.u0), tex.v0 + uv[k][1] * (tex.v1 - tex.v0), shade * light});
            }
        }
    }
}

}  // namespace vc
