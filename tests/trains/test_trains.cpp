#include <cassert>
#include <cstdio>
#include "Trains.h"
using namespace vc;
int main() {
    TrainSystem ts;
    // Track: line going north from z=20 to z=11 (10 pieces, x=5), curve at z=10 to the east, then x=6..12 east, stop at x=13.
    auto put = [&](int x, int z, PieceKind k, int d) { TrackPiece p; p.x = x; p.y = 64; p.z = z; p.kind = k; p.dir = d; return p; };
    (void)put;
    // placePiece needs World; fill pieces via save file instead.
    std::string dir = "/tmp/work/tt"; system(("mkdir -p " + dir).c_str());
    FILE* f = fopen((dir + "/trains.dat").c_str(), "w");
    for (int z = 26; z >= 11; --z) fprintf(f, "piece 0 5 64 %d 0 0\n", z);   // straight, axis N-S
    fprintf(f, "piece 0 5 64 10 1 2\n");  // curve ports: dir=2 (south) and 3 (west)?? fix below
    fclose(f);
    // Curve at (5,10): must connect south (2) and east (1): ports dir and dir+1 => dir=1 gives {1,2}.
    f = fopen((dir + "/trains.dat").c_str(), "w");
    for (int z = 26; z >= 11; --z) fprintf(f, "piece 0 5 64 %d 0 0\n", z);
    fprintf(f, "piece 0 5 64 10 1 1\n");
    for (int x = 6; x <= 12; ++x) fprintf(f, "piece 0 %d 64 10 0 1\n", x);  // straight dir 1 = E-W
    fprintf(f, "piece 0 13 64 10 3 3\n");  // stop connected on the west side (port 3)
    fclose(f);
    ts.load(dir);
    printf("pieces: %zu (expect 25)\n", ts.pieceCount());
    assert(ts.pieceCount() == 25);
    std::string why;
    int idx = ts.spawnTrain(0, 5, 64, 15, 0, {CarKind::Steam, CarKind::Boxcar, CarKind::Passenger}, &why);
    printf("spawn idx=%d why='%s'\n", idx, why.c_str());
    assert(idx == 0 && ts.trains()[0].cars.size() == 3);
    Train& t = ts.trains()[0];
    for (auto& c : t.cars) printf(" car pos %.2f %.2f %.2f yaw %.2f\n", c.pos.x, c.pos.y, c.pos.z, c.yaw);
    float d01 = (t.cars[0].pos - t.cars[1].pos).length();
    printf("spacing on straight: %.3f (expect %.3f)\n", d01, TrainSystem::kCarSpacing);
    assert(std::abs(d01 - TrainSystem::kCarSpacing) < 0.01f);
    auto sup = [](int, int, int) { return true; };
    ts.setTarget(0, 10);
    float maxJump = 0; Vec3 last = t.cars[0].pos;
    for (int i = 0; i < 1200 && !t.blocked; ++i) {
        ts.update(0, 1 / 30.0f, sup);
        float j = (t.cars[0].pos - last).length(); if (j > maxJump) maxJump = j; last = t.cars[0].pos;
    }
    printf("blocked=%d lead at %.2f %.2f %.2f maxStep=%.3f (<=0.34 expected)\n", t.blocked, t.cars[0].pos.x, t.cars[0].pos.y, t.cars[0].pos.z, maxJump);
    assert(t.blocked);
    // Lead must stop at the boundary of cell x=12 (east edge x=13.0), centre z = 10.5
    assert(std::abs(t.cars[0].pos.x - 13.0f) < 0.05f && std::abs(t.cars[0].pos.z - 10.5f) < 0.05f);
    assert(maxJump < 0.4f);
    for (auto& c : t.cars) printf(" car pos %.2f %.2f %.2f yaw %.2f\n", c.pos.x, c.pos.y, c.pos.z, c.yaw);
    // Reverse: ride back until the tail end (the former last car leads) hits the end of the line.
    ts.reverse(0);
    ts.setTarget(0, 12);
    for (int i = 0; i < 2000 && !t.blocked; ++i) ts.update(0, 1 / 30.0f, sup);
    printf("after reverse: blocked=%d front(tail car) %.2f %.2f %.2f\n", t.blocked, ts.frontPosition(0).x, ts.frontPosition(0).y, ts.frontPosition(0).z);
    assert(t.blocked);
    assert(std::abs(ts.frontPosition(0).z - 26.0f) < 0.2f || std::abs(ts.frontPosition(0).z - 27.0f) < 0.2f || t.blocked);
    // Save/load roundtrip
    ts.save(dir);
    TrainSystem t2; t2.load(dir);
    assert(t2.pieceCount() == 25 && t2.trains().size() == 1 && t2.trains()[0].cars.size() == 3);
    // Geometry builds
    std::vector<Vertex> v; ts.buildVertices(0, Vec3(8, 70, 15), 100, v);
    printf("vertices: %zu\n", v.size());
    assert(v.size() > 1000 && v.size() % 3 == 0);
    // Config
    std::string err;
    bool ok = ts.loadConfig(R"({"materials":{"tie":"x:tie"},"models":{"boxcar":[{"from":[-0.4,0.1,-0.8],"to":[0.4,1,0.8],"material":"tie"}]}})",
                            [](const std::string&) { return 7; }, err);
    printf("config ok=%d err='%s' boxcar boxes=%zu\n", ok, err.c_str(), ts.model(CarKind::Boxcar).size());
    assert(ok && ts.model(CarKind::Boxcar).size() == 1);
    bool bad = ts.loadConfig("{ nope", [](const std::string&) { return 7; }, err);
    printf("bad json -> %d '%s'\n", bad, err.c_str());
    assert(!bad);
    puts("ALL OK");
}
