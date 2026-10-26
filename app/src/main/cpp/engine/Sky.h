#pragma once
#include <vector>

#include "Math.h"
#include "Mesher.h"

namespace vc {

class World;

// Time of day in ticks like the classic game: 24000 per day, 20 per second (a 20 minute
// day). 0 is sunrise, 6000 noon, 12000 sunset and 18000 midnight.
constexpr float kDayTicks = 24000, kTicksPerSecond = 20;

enum class DayPhase { Morning, Day, Evening, Night, Midnight };
DayPhase dayPhase(float ticks);
const char* dayPhaseName(DayPhase phase);  // Czech key ("Ráno", "Den", ...)
inline bool isNight(float ticks) {
    DayPhase p = dayPhase(ticks);
    return p == DayPhase::Night || p == DayPhase::Midnight;
}

// Colours and light of the overworld sky at a time of day; `rain` (0..1) greys it.
struct SkyState {
    float sky[3];   // clear colour and fog
    float light;    // multiplies the world's brightness
    float stars;    // 0..1 how visible the stars are
    float clouds;   // brightness of the clouds
};
SkyState computeSky(float ticks, const float daySky[3], float rain);

// Sun, moon and stars around the camera (atlas tiles T_SUN, T_MOON, T_STAR), drawn behind everything.
void buildSkyObjects(const Vec3& eye, float ticks, std::vector<Vertex>& sunMoon, std::vector<Vertex>& stars);
// A layer of blocky clouds drifting east at y = kCloudHeight.
constexpr float kCloudHeight = 116;
void buildClouds(const Vec3& eye, float seconds, float brightness, std::vector<Vertex>& out);

// Rain and snow fall in the open columns around the camera.
enum class Precipitation { None, Rain, Snow };
void buildPrecipitation(const World& world, const Vec3& eye, Precipitation kind, float seconds, float intensity,
                        float light, std::vector<Vertex>& out);

}  // namespace vc
