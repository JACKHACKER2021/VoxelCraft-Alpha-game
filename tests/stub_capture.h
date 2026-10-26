#pragma once
#include <cstdint>
#include <vector>

#include "engine/Mesher.h"
#include "engine/Ui.h"

// What the no-op renderer was last asked to draw, for UI snapshots.
namespace vc::capture {
extern UiList ui;
extern float guiW, guiH;
extern std::vector<uint8_t> atlas, gui;
extern int atlasSize, guiW_px, guiH_px;
extern std::vector<uint8_t> skins, playerSkin;
extern int skinsW, skinsH, modelVertices, particleVertices, remoteModels;
extern std::vector<Vertex> handSkin, handAtlas;
extern std::vector<uint8_t> map;  // map image (minimap or a paper sheet)
extern int mapW, mapH;  // last first-person hand
}  // namespace vc::capture
