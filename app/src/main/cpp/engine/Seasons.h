#pragma once

namespace vc {

// 1.9.0 seasonal events, by the date on the phone: autumn Halloween (1 October - 30 November),
// Christmas (20 - 29 December) and New Year's Eve (30 December - 2 January).
enum class Season { None, Halloween, Christmas, NewYear };

// `ymd` as YYYYMMDD, like todayYmd().
inline Season seasonOn(int ymd) {
    const int md = ymd % 10000;
    if (md >= 1001 && md <= 1130) return Season::Halloween;
    if (md >= 1220 && md <= 1229) return Season::Christmas;
    if (md >= 1230 || md <= 102) return Season::NewYear;
    return Season::None;
}

// Snow everywhere and white grass: Christmas and New Year's Eve.
inline bool snowySeason(Season s) { return s == Season::Christmas || s == Season::NewYear; }

}  // namespace vc
