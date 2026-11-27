// 2.8.0 preview: the VC editor (the add-on manager's "Tvůrce" tab): everything a pack in the VC language can
// hold, made on the phone without writing JSON: blocks (a cube or a model of your own), items, mobs (with the
// body of any creature of the game, its GLB model), models (boxes, saved as a geo.json), crafting recipes,
// shader settings and the pack's own name. Everything goes to the pack "moje_tvorba" and loads at once.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Blocks.h"
#include "Game.h"
#include "Items.h"
#include "Json.h"
#include "Lang.h"
#include "Mods.h"
#include "Png.h"
#include "VcBuiltin.h"
#include "VcModel.h"

namespace vc {

namespace fs = std::filesystem;
using namespace theme;

namespace {

const char* const kPack = "moje_tvorba";
constexpr uint32_t kPal[12] = {0xF0F0F0, 0x202020, 0x8C5A32, 0x5AA046, 0xC83C37, 0x3C78C8,
                               0xE6C83C, 0x9696A0, 0xE68C3C, 0x8C50B4, 0xF0A0B4, 0x46B4AA};
const char* const kModes[7] = {"Blok", "Předmět", "Mob", "Model", "Recept", "Shader", "Balíček"};
// What recipes and drops can use, besides what the pack makes itself.
const ItemId kIngredients[] = {STONE, COBBLESTONE, PLANKS, LOG, STICK, COAL, STEEL_INGOT, BRONZE_INGOT, RUBY, GLASS, SAND, WOOL, STRING, FEATHER, WHEAT};
constexpr int kIngredientCount = (int)(sizeof(kIngredients) / sizeof(kIngredients[0]));
// The shader's numbers: label, lowest, highest (hundredths).
struct ShaderNum { const char* label; int lo, hi; };
const ShaderNum kShaderNums[6] = {{"Sytost", 50, 200}, {"Kontrast", 50, 160}, {"Teplo", -30, 40}, {"Vinětace", 0, 80}, {"Záře", 0, 100}, {"Mlha", 30, 200}};

Color rgb(uint32_t c) { return Color{((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, 1}; }

Json readJson(const std::string& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    Json doc;
    std::string err;
    if (!Json::parse(ss.str(), doc, err) || !doc.isObject()) doc = Json::object();
    return doc;
}

std::string idFrom(const std::string& name) {
    std::string id;
    for (char c : name) {
        const unsigned char u = (unsigned char)c;
        if (std::isalnum(u)) id += (char)std::tolower(u);
        else if (c == ' ' || c == '-' || c == '_') id += '_';
    }
    return id.empty() ? "vec" : id.substr(0, 30);
}

std::string hundredths(int v) {
    char b[16];
    std::snprintf(b, sizeof b, "%.2f", v / 100.0);
    return b;
}

// Where each box's part of the texture lies (the box layout: 2 * depth + 2 * width across, depth + height down).
struct ModelLayout {
    std::vector<std::pair<int, int>> at;
    int w = 64, h = 16;
};
ModelLayout modelLayout(const std::vector<std::array<int, 6>>& boxes) {
    ModelLayout l;
    for (const auto& b : boxes) l.w = std::max(l.w, 2 * b[5] + 2 * b[3]);
    int tx = 0, ty = 0, rowH = 0;
    for (const auto& b : boxes) {
        const int bw = 2 * b[5] + 2 * b[3], bh = b[5] + b[4];
        if (tx + bw > l.w) tx = 0, ty += rowH, rowH = 0;
        l.at.emplace_back(tx, ty);
        tx += bw, rowH = std::max(rowH, bh);
    }
    l.h = std::max(16, (ty + rowH + 15) / 16 * 16);
    return l;
}

}  // namespace

// 2.8.0 build 7: the model's own texture, painted right on its box layout. A new layout keeps what was
// painted where it still fits; new parts start from the Blok picture, repeated.
void Game::creatorModelTexture() {
    const ModelLayout l = modelLayout(creatorBoxes_);
    if (creatorModelW_ == l.w && creatorModelH_ == l.h && creatorModelPix_.size() == (size_t)l.w * l.h) return;
    std::vector<uint32_t> pix((size_t)l.w * l.h);
    for (int y = 0; y < l.h; ++y)
        for (int x = 0; x < l.w; ++x)
            pix[(size_t)y * l.w + x] = x < creatorModelW_ && y < creatorModelH_ && !creatorModelPix_.empty() ? creatorModelPix_[(size_t)y * creatorModelW_ + x]
                                                                                                             : creatorPixels_[(size_t)((y % 16) * 16 + x % 16)];
    creatorModelPix_ = std::move(pix), creatorModelW_ = l.w, creatorModelH_ = l.h;
}

std::vector<std::string> Game::creatorModels() const {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto e = fs::directory_iterator(packs_.packDir(kPack) + "/models", ec); !ec && e != fs::directory_iterator(); e.increment(ec)) {
        const std::string n = e->path().filename().string();
        if (n.size() > 9 && n.compare(n.size() - 9, 9, ".geo.json") == 0) out.push_back(n.substr(0, n.size() - 9));
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> Game::creatorItems() const {
    std::vector<std::string> out;
    const Json doc = readJson(packs_.packDir(kPack) + "/content.json");
    const std::string ns = doc["namespace"].str("tvorba");
    for (const char* list : {"blocks", "items"})
        for (size_t i = 0; i < doc[list].size(); ++i) out.push_back(ns + ":" + doc[list][i]["id"].str(""));
    return out;
}

void Game::creatorPainter(float px0, float y, float gw) {
    if (creatorPixels_.empty()) creatorPixels_.assign(256, 0xF0F0F0);
    const float cell = std::max(2.0f, std::floor(gw / 16));
    gw = cell * 16;
    ui_.frame(px0 - 2, y, gw + 4, gw + 4, kField, kSeparator);
    for (int i = 0; i < 256; ++i) {
        const float cx = px0 + (i % 16) * cell, cy = y + 2 + (i / 16) * cell;
        ui_.fill(cx, cy, cell - (cell > 4 ? 1 : 0), cell - (cell > 4 ? 1 : 0), rgb(creatorPixels_[i]));
        if (!creatorFill_ && ui_.held(cx, cy, cell, cell)) creatorPixels_[i] = kPal[creatorColor_];
        if (creatorFill_ && ui_.tapped(cx, cy, cell, cell)) {
            const uint32_t from = creatorPixels_[i], to = kPal[creatorColor_];
            std::vector<int> stack;
            if (from != to) stack.push_back(i);
            while (!stack.empty()) {
                const int k = stack.back();
                stack.pop_back();
                if (creatorPixels_[k] != from) continue;
                creatorPixels_[k] = to;
                if (k % 16 > 0) stack.push_back(k - 1);
                if (k % 16 < 15) stack.push_back(k + 1);
                if (k >= 16) stack.push_back(k - 16);
                if (k < 240) stack.push_back(k + 16);
            }
        }
    }
    // The palette and the tools beside the picture.
    const float palX = px0 + gw + 8;
    for (int k = 0; k < 12; ++k) {
        const float cx = palX + (k % 2) * 16, cy = y + 2 + (k / 2) * 14;
        ui_.frame(cx, cy, 14, 12, rgb(kPal[k]), k == creatorColor_ ? kTextBright : kSeparator);
        if (ui_.tapped(cx, cy, 14, 12)) creatorColor_ = k;
    }
    if (ui_.button(palX, y + 90, 64, 16, creatorFill_ ? "Výplň" : "Štětec")) creatorFill_ = !creatorFill_;
    if (ui_.button(palX, y + 110, 64, 16, "Vymazat")) creatorPixels_.assign(256, kPal[creatorColor_]);
}

void Game::vcCreatorTab(float x, float y, float w, float h) {
    if (creatorPixels_.empty()) creatorPixels_.assign(256, 0xF0F0F0);
    if (creatorBoxes_.empty()) creatorBoxes_ = {{-4, 0, -4, 8, 8, 8}};
    const float bw = std::floor((w - 6 * 3) / 7);
    for (int i = 0; i < 7; ++i)
        if (ui_.button(x + i * (bw + 3), y, bw, 16, kModes[i], true, creatorMode_ == i)) creatorMode_ = i;
    const float cy = y + 20, ch = h - 20, fw = std::floor(w * 0.36f), fx = x + 6, fw2 = fw - 12;
    ui_.frame(x, cy, fw, ch, kPanelInner, kSeparator);
    float ry = cy + 4;
    auto stepper = [&](const std::string& label, int& v, int lo, int hi, int st, const std::string& shown) {
        ui_.text(fx, ry, label, kTextDim, 1, false);
        if (ui_.button(fx, ry + 10, 22, 16, "-")) v = std::max(lo, v - st);
        ui_.textCentered(fx + fw2 / 2, ry + 14, shown, kTextBright);
        if (ui_.button(fx + fw2 - 22, ry + 10, 22, 16, "+")) v = std::min(hi, v + st);
        ry += 30;
    };
    auto inlineStepper = [&](const std::string& label, int& v, int lo, int hi, int st, const std::string& shown) {
        ui_.text(fx, ry + 4, label, kTextDim, 1, false, fw2 - 96);
        if (ui_.button(fx + fw2 - 92, ry, 20, 16, "-")) v = std::max(lo, v - st);
        ui_.textCentered(fx + fw2 - 46, ry + 4, shown, kTextBright);
        if (ui_.button(fx + fw2 - 20, ry, 20, 16, "+")) v = std::min(hi, v + st);
        ry += 20;
    };
    auto choice = [&](const std::string& label, const std::string& shown) {
        ui_.text(fx, ry, label, kTextDim, 1, false);
        const bool b = ui_.button(fx, ry + 10, fw2, 16, shown);
        ry += 30;
        return b;
    };
    auto nameField = [&](const char* fallback) {
        if (choice(tr("Název"), creatorName_.empty() ? tr("Klepni a napiš") : creatorName_) && onTextPrompt)
            onTextPrompt(PromptCreatorName, "Název", creatorName_.empty() ? fallback : creatorName_);
    };
    const float rx = x + fw + 8, rw = x + w - rx;
    auto note = [&](float ty, const std::string& s) {
        for (const std::string& l : wrapText(tr(s), rw)) ui_.text(rx, ty, l, kTextDim, 1, false, rw), ty += 10;
        return ty;
    };
    auto hint = [&](const std::string& s) {  // under the form
        ry += 2;
        for (const std::string& l : wrapText(tr(s), fw2)) ui_.text(fx, ry, l, kTextDim, 1, false, fw2), ry += 10;
    };
    bool canSave = !creatorName_.empty();
    const float painterW = std::min(rw - 90, ch - 4);
    switch (creatorMode_) {
        case 0: {  // a block: a cube with the painted texture, or one of your models
            nameField("Můj blok");
            static const char* const kHard[4] = {"Měkký", "Střední", "Tvrdý", "Velmi tvrdý"};
            if (choice(tr("Tvrdost"), tr(kHard[creatorHard_]))) creatorHard_ = (creatorHard_ + 1) % 4;
            if (choice(tr("Svítí"), tr(creatorLight_ ? "Ano" : "Ne"))) creatorLight_ = !creatorLight_;
            const std::vector<std::string> models = creatorModels();
            if (creatorShape_ >= (int)models.size()) creatorShape_ = -1;
            if (choice(tr("Tvar"), creatorShape_ < 0 ? tr("Kostka") : models[(size_t)creatorShape_]))
                creatorShape_ = creatorShape_ + 1 >= (int)models.size() ? -1 : creatorShape_ + 1;
            hint(creatorShape_ < 0 ? "Kostka s touto texturou na všech stranách." : "Blok má tvar tvého modelu (záložka Model).");
            creatorPainter(rx, cy, painterW);
            break;
        }
        case 1: {  // an item
            nameField("Můj předmět");
            stepper(tr("Jídlo (sytost)"), creatorFood_, 0, 20, 1, creatorFood_ ? std::to_string(creatorFood_) : tr("Nejí se"));
            static const int kStacks[3] = {1, 16, 64};
            int si = creatorStack_ == 1 ? 0 : creatorStack_ == 16 ? 1 : 2;
            if (choice(tr("Do jednoho políčka"), std::to_string(kStacks[si]))) creatorStack_ = kStacks[(si + 1) % 3];
            creatorPainter(rx, cy, painterW);
            break;
        }
        case 2: {  // a mob: any creature's body (its VC model), its behaviour from the form
            nameField("Můj tvor");
            creatorBase_ = std::clamp(creatorBase_, 0, (int)MobType::Count - 1);
            if (choice(tr("Tělo (VC model)"), tr(mobInfo((MobType)creatorBase_).name))) creatorBase_ = (creatorBase_ + 1) % (int)MobType::Count;
            stepper(tr("Životy"), creatorHealth_, 2, 200, 2, std::to_string(creatorHealth_));
            stepper(tr("Jak často se objeví"), creatorSpawn_, 0, 10, 1, std::to_string(creatorSpawn_ * 10) + " %");
            creatorDrop_ = std::clamp(creatorDrop_, 0, kIngredientCount);
            if (choice(tr("Co upustí"), creatorDrop_ == kIngredientCount ? tr("Nic") : tr(itemName(kIngredients[creatorDrop_]))))
                creatorDrop_ = (creatorDrop_ + 1) % (kIngredientCount + 1);
            float ty = cy + 4;
            ui_.text(rx, ty, std::string(vcMobModelId((MobType)creatorBase_)) + ".glb", kTitle, 1, false), ty += 14;
            ty = note(ty, "Tvor dostane tělo, texturu i pohyby (nohy, křídla) z GLB modelu hry. Model i textura se zkopírují do tvého doplňku (models/ a textures/mod/), takže je můžeš upravit v Blockbench nebo Blenderu.");
            break;
        }
        case 3: {  // a model: boxes
            nameField("Můj model");
            creatorBox_ = std::clamp(creatorBox_, 0, (int)creatorBoxes_.size() - 1);
            if (choice(tr("Kostka"), std::to_string(creatorBox_ + 1) + " / " + std::to_string(creatorBoxes_.size())))
                creatorBox_ = (creatorBox_ + 1) % (int)creatorBoxes_.size();
            if (ui_.button(fx, ry, fw2 / 2 - 2, 16, "+ Kostka", creatorBoxes_.size() < 16)) {
                std::array<int, 6> b = creatorBoxes_[(size_t)creatorBox_];
                b[1] += b[4];
                creatorBoxes_.push_back(b), creatorBox_ = (int)creatorBoxes_.size() - 1;
            }
            if (ui_.button(fx + fw2 / 2 + 2, ry, fw2 / 2 - 2, 16, "- Kostka", creatorBoxes_.size() > 1))
                creatorBoxes_.erase(creatorBoxes_.begin() + creatorBox_), creatorBox_ = 0;
            ry += 22;
            static const char* const kAxes[6] = {"Poloha X", "Poloha Y", "Poloha Z", "Šířka", "Výška", "Hloubka"};
            if (choice(tr("Mění se"), tr(kAxes[creatorAxis_]))) creatorAxis_ = (creatorAxis_ + 1) % 6;
            int& v = creatorBoxes_[(size_t)creatorBox_][(size_t)creatorAxis_];
            stepper(tr("Pixely (16 = blok)"), v, creatorAxis_ < 3 ? -16 : 1, 32, 1, std::to_string(v));
            // Two views on the right: the shape, or painting its texture.
            if (ui_.button(rx, cy, 80, 14, "Tvar", true, !creatorModelPaint_)) creatorModelPaint_ = false;
            if (ui_.button(rx + 84, cy, 130, 14, "Malování textury", true, creatorModelPaint_)) creatorModelPaint_ = true;
            const float top = cy + 18, room = ch - 18 - 26;
            creatorModelTexture();
            if (creatorModelPaint_) {
                // Painting right on the model's texture: each box's part is outlined (the selected one bright).
                const ModelLayout lay = modelLayout(creatorBoxes_);
                const float cell = std::max(1.0f, std::floor(std::min((rw - 80) / creatorModelW_, (room - 4) / creatorModelH_)));
                ui_.frame(rx - 2, top, creatorModelW_ * cell + 4, creatorModelH_ * cell + 4, kField, kSeparator);
                for (int i = 0; i < creatorModelW_ * creatorModelH_; ++i) {
                    const float px = rx + (i % creatorModelW_) * cell, py = top + 2 + (i / creatorModelW_) * cell;
                    ui_.fill(px, py, cell, cell, rgb(creatorModelPix_[(size_t)i]));
                    if (ui_.held(px, py, cell, cell)) creatorModelPix_[(size_t)i] = kPal[creatorColor_];
                }
                for (size_t i = 0; i < creatorBoxes_.size(); ++i) {
                    const auto& bx = creatorBoxes_[i];
                    const float ox = rx + lay.at[i].first * cell, oy = top + 2 + lay.at[i].second * cell, bw = (2 * bx[5] + 2 * bx[3]) * cell, bh = (bx[5] + bx[4]) * cell;
                    const Color c = (int)i == creatorBox_ ? kTextBright : kSeparator;
                    ui_.fill(ox, oy, bw, 1, c), ui_.fill(ox, oy + bh - 1, bw, 1, c), ui_.fill(ox, oy, 1, bh, c), ui_.fill(ox + bw - 1, oy, 1, bh, c);
                }
                const float palX = rx + creatorModelW_ * cell + 10;
                for (int k = 0; k < 12; ++k) {
                    const float cx = palX + (k % 2) * 16, cyy = top + 2 + (k / 2) * 14;
                    ui_.frame(cx, cyy, 14, 12, rgb(kPal[k]), k == creatorColor_ ? kTextBright : kSeparator);
                    if (ui_.tapped(cx, cyy, 14, 12)) creatorColor_ = k;
                }
                if (ui_.button(palX, top + 90, 60, 16, "Z Bloku")) creatorModelPix_.clear(), creatorModelW_ = 0, creatorModelTexture();
                break;
            }
            // The model from the front and from the side.
            const float pw = std::floor((rw - 8) / 2), ph = std::min(pw, room - 10), sc = ph / 28;
            for (int view = 0; view < 2; ++view) {
                const float vx = rx + view * (pw + 8), base = top + 14 + ph - 4;
                ui_.text(vx, top, tr(view ? "Z boku" : "Zepředu"), kTextDim, 1, false);
                ui_.frame(vx, top + 10, pw, ph, kField, kSeparator);
                ui_.fill(vx + 2, base, pw - 4, 1, kSeparator);
                for (size_t i = 0; i < creatorBoxes_.size(); ++i) {
                    const auto& bx = creatorBoxes_[i];
                    const float a0 = view ? bx[2] : bx[0], aw = view ? bx[5] : bx[3];
                    // Each box in the colour of the middle of its texture.
                    const ModelLayout lay = modelLayout(creatorBoxes_);
                    const int tx = std::min(creatorModelW_ - 1, lay.at[i].first + bx[5] + bx[3] / 2), ty = std::min(creatorModelH_ - 1, lay.at[i].second + bx[5] + bx[4] / 2);
                    ui_.frame(vx + pw / 2 + a0 * sc, base - (bx[1] + bx[4]) * sc, aw * sc, bx[4] * sc, rgb(creatorModelPix_[(size_t)ty * creatorModelW_ + tx]),
                              (int)i == creatorBox_ ? kTextBright : kSeparator);
                }
            }
            break;
        }
        case 4: {  // a crafting recipe
            const std::vector<std::string> items = creatorItems();
            creatorIngredient_ = std::clamp(creatorIngredient_, 0, kIngredientCount - 1);
            if (choice(tr("Surovina (klepni do mřížky)"), tr(itemName(kIngredients[creatorIngredient_]))))
                creatorIngredient_ = (creatorIngredient_ + 1) % kIngredientCount;
            if (items.empty()) {
                hint("Nejdřív ulož blok nebo předmět: recept vyrábí tvoje věci.");
                canSave = false;
            } else {
                creatorResult_ %= (int)items.size();
                const ItemId res = itemFromKey(items[(size_t)creatorResult_]);
                if (choice(tr("Výsledek"), res != AIR ? std::string(tr(itemName(res))) : items[(size_t)creatorResult_]))
                    creatorResult_ = (creatorResult_ + 1) % (int)items.size();
                stepper(tr("Kolik kusů"), creatorCount_, 1, 64, 1, std::to_string(creatorCount_));
                canSave = std::any_of(std::begin(creatorGrid_), std::end(creatorGrid_), [](int g) { return g >= 0; });
            }
            const float cell = std::floor(std::min(rw, ch - 20) / 3) - 4;
            for (int i = 0; i < 9; ++i) {
                const float gx = rx + (i % 3) * (cell + 4), gy = cy + (i / 3) * (cell + 4);
                const int g = creatorGrid_[i];
                ui_.frame(gx, gy, cell, cell, kField, g >= 0 ? kAccent : kSeparator);
                if (g >= 0)
                    for (const std::string& l : wrapText(tr(itemName(kIngredients[g])), cell - 4)) ui_.text(gx + 2, gy + 2, l, kTextBright, 1, false, cell - 4);
                if (ui_.tapped(gx, gy, cell, cell)) creatorGrid_[i] = g == creatorIngredient_ ? -1 : creatorIngredient_;
            }
            break;
        }
        case 5: {  // shader numbers
            if (choice(tr("Název shaderu"), creatorShaderName_) && onTextPrompt) onTextPrompt(PromptShaderName, "Název shaderu", creatorShaderName_);
            for (int i = 0; i < 6; ++i) inlineStepper(tr(kShaderNums[i].label), creatorShader_[i], kShaderNums[i].lo, kShaderNums[i].hi, 5, hundredths(creatorShader_[i]));
            // A preview: grass, sky, stone, sand and lava with the numbers applied, darker towards the corners.
            static const uint32_t kSample[5] = {0x5AA046, 0x86B4F0, 0x8C8C8C, 0xDCC88C, 0xF0782A};
            const float sat = creatorShader_[0] / 100.0f, con = creatorShader_[1] / 100.0f, warm = creatorShader_[2] / 100.0f,
                        vig = creatorShader_[3] / 100.0f, glow = creatorShader_[4] / 100.0f;
            const float sw = std::floor(rw / 5), sh = std::min(ch - 60, 100.0f);
            for (int k = 0; k < 5; ++k) {
                Color c = rgb(kSample[k]);
                const float l = (c.r + c.g + c.b) / 3;
                c.r = l + (c.r - l) * sat, c.g = l + (c.g - l) * sat, c.b = l + (c.b - l) * sat;
                c.r = (c.r - 0.5f) * con + 0.5f + warm * 0.3f, c.g = (c.g - 0.5f) * con + 0.5f, c.b = (c.b - 0.5f) * con + 0.5f - warm * 0.3f;
                if (k == 4) c.r += glow * 0.2f, c.g += glow * 0.1f;
                const float edge = 1 - vig * std::fabs(k - 2) / 2.0f * 0.8f;
                c.r = std::clamp(c.r * edge, 0.0f, 1.0f), c.g = std::clamp(c.g * edge, 0.0f, 1.0f), c.b = std::clamp(c.b * edge, 0.0f, 1.0f);
                ui_.fill(rx + k * sw, cy, sw - 2, sh, c);
            }
            note(cy + sh + 6, "Uloží se jako shaders/shader.json doplňku. Zapneš ho v Nastavení > Grafika > Shader.");
            (void)hint;
            break;
        }
        default: {  // the pack itself
            if (choice(tr("Název doplňku"), creatorPackName_) && onTextPrompt) onTextPrompt(PromptAddonName, "Název doplňku", creatorPackName_);
            if (choice(tr("Popis"), creatorPackText_) && onTextPrompt) onTextPrompt(PromptAddonText, "Popis", creatorPackText_);
            const ModRegistry::Summary sum = ModRegistry::get().summary(packs_.packDir(kPack));
            float ty = cy + 4;
            ui_.text(rx, ty, creatorPackName_, kTitle, 1, false, rw), ty += 14;
            ty = note(ty, trf("Bloky {0}, předměty {1}, moby {2}, modely {3}", {std::to_string(sum.blocks), std::to_string(sum.items), std::to_string(sum.mobs),
                                                                               std::to_string(creatorModels().size())}));
            note(ty + 6, "Doplněk najdeš v záložce Doplňky; sdílet ho můžeš jako .vcaddon.");
            canSave = true;
            break;
        }
    }
    if (ui_.button(x + w - 140, cy + ch - 20, 140, 20, "Uložit do .vcaddon", canSave, true)) saveVcCreation();
}

void Game::saveVcCreation() {
    std::error_code ec;
    const std::string dir = packs_.packDir(kPack);
    fs::create_directories(dir + "/textures/mod", ec);
    fs::create_directories(dir + "/models", ec);
    if (creatorMode_ == 6 || !fs::exists(dir + "/manifest.json")) {
        Json man = Json::object();
        man.set("name", Json::string(creatorPackName_));
        man.set("description", Json::string(creatorPackText_));
        man.set("version", Json::string("1.0"));
        std::ofstream(dir + "/manifest.json") << man.dump();
    }
    std::string shown = creatorName_;
    const std::string id = idFrom(creatorName_);
    auto paintedPng = [&](const std::string& file) {
        std::vector<uint8_t> rgba(256 * 4);
        for (int i = 0; i < 256; ++i) {
            const uint32_t c = creatorPixels_[i];
            rgba[i * 4] = (c >> 16) & 255, rgba[i * 4 + 1] = (c >> 8) & 255, rgba[i * 4 + 2] = c & 255, rgba[i * 4 + 3] = 255;
        }
        writePng(dir + "/textures/mod/" + file, 16, 16, rgba);
    };
    Json doc = readJson(dir + "/content.json");
    if (doc["namespace"].isNull()) doc.set("namespace", Json::string("tvorba"));
    auto putEntry = [&](const char* listName, const Json& e) {
        Json list = Json::array();
        for (size_t i = 0; i < doc[listName].size(); ++i)
            if (doc[listName][i]["id"].str("") != e["id"].str("")) list.push(doc[listName][i]);
        list.push(e);
        doc.set(listName, list);
        for (const char* other : {"blocks", "items", "mobs"})
            if (doc[other].isNull()) doc.set(other, Json::array());
        std::ofstream(dir + "/content.json") << doc.dump();
    };
    Json e = Json::object();
    e.set("id", Json::string(id));
    e.set("name", Json::string(creatorName_));
    e.set("name_cs", Json::string(creatorName_));
    switch (creatorMode_) {
        case 0: {
            paintedPng(id + ".png");
            static const float kHard[4] = {0.5f, 1.5f, 3.0f, 6.0f};
            e.set("texture", Json::string(id + ".png"));
            e.set("hardness", Json::number(kHard[creatorHard_]));
            e.set("tool", Json::string(creatorHard_ >= 2 ? "pickaxe" : ""));
            if (creatorLight_) e.set("light", Json::fromBool(true));
            const std::vector<std::string> models = creatorModels();
            if (creatorShape_ >= 0 && creatorShape_ < (int)models.size()) {
                e.set("model", Json::string(models[(size_t)creatorShape_] + ".geo.json"));
                e.set("model_texture", Json::string(models[(size_t)creatorShape_] + "_model.png"));
            }
            putEntry("blocks", e);
            break;
        }
        case 1:
            paintedPng(id + ".png");
            e.set("texture", Json::string(id + ".png"));
            if (creatorFood_) e.set("food", Json::number(creatorFood_));
            e.set("stack", Json::number(creatorStack_));
            putEntry("items", e);
            break;
        case 2: {
            const MobType body = (MobType)std::clamp(creatorBase_, 0, (int)MobType::Count - 1);
            const std::string mid = id + "_mob", glb = dataDir_ + "/vc/models/mobs/" + vcMobModelId(body) + ".glb";
            std::ifstream in(glb, std::ios::binary);
            const std::string bytes{std::istreambuf_iterator<char>(in), {}};
            VcModel m;
            std::vector<uint8_t> png;
            std::string err;
            e.set("id", Json::string(mid));
            e.set("base", Json::string(vcMobModelId(body)));
            if (!bytes.empty() && parseVcGlb((const uint8_t*)bytes.data(), bytes.size(), m, &png, err) && !png.empty()) {
                std::ofstream(dir + "/models/" + mid + ".glb", std::ios::binary) << bytes;
                std::ofstream(dir + "/textures/mod/" + mid + ".png", std::ios::binary).write((const char*)png.data(), (std::streamsize)png.size());
                e.set("model", Json::string(mid + ".glb"));
                e.set("texture", Json::string(mid + ".png"));
            }
            e.set("health", Json::number(creatorHealth_));
            e.set("spawn", Json::number(creatorSpawn_ / 10.0));
            if (creatorDrop_ >= 0 && creatorDrop_ < kIngredientCount) {
                Json drops = Json::array(), d = Json::object();
                d.set("item", Json::string(itemKey(kIngredients[creatorDrop_])));
                d.set("count", Json::number(1));
                drops.push(d);
                e.set("drops", drops);
            }
            putEntry("mobs", e);
            break;
        }
        case 3: {
            // The boxes as a geo.json (each its own bone; the ones standing on the ground are legs that walk),
            // the texture as painted on the box layout.
            creatorModelTexture();
            const ModelLayout lay = modelLayout(creatorBoxes_);
            std::vector<std::pair<int, int>> places = lay.at;
            const int texW = lay.w, texH = lay.h;
            std::vector<uint8_t> rgba((size_t)texW * texH * 4, 255);
            for (int i = 0; i < texW * texH; ++i) {
                const uint32_t c = creatorModelPix_[(size_t)i];
                rgba[(size_t)i * 4] = (c >> 16) & 255, rgba[(size_t)i * 4 + 1] = (c >> 8) & 255, rgba[(size_t)i * 4 + 2] = c & 255;
            }
            writePng(dir + "/textures/mod/" + id + "_model.png", texW, texH, rgba);
            Json bones = Json::array();
            for (size_t i = 0; i < creatorBoxes_.size(); ++i) {
                const auto& b = creatorBoxes_[i];
                const bool leg = b[1] == 0 && creatorBoxes_.size() > 1;
                auto arr = [](float a, float c, float d) {
                    Json j = Json::array();
                    j.push(Json::number(a)), j.push(Json::number(c)), j.push(Json::number(d));
                    return j;
                };
                Json bone = Json::object(), cubes = Json::array(), cube = Json::object(), uv = Json::array();
                bone.set("name", Json::string((leg ? "leg" : "box") + std::to_string(i)));
                bone.set("pivot", arr(b[0] + b[3] / 2.0f, (float)(b[1] + b[4]), b[2] + b[5] / 2.0f));
                cube.set("origin", arr((float)b[0], (float)b[1], (float)b[2]));
                cube.set("size", arr((float)b[3], (float)b[4], (float)b[5]));
                uv.push(Json::number(places[i].first)), uv.push(Json::number(places[i].second));
                cube.set("uv", uv);
                cubes.push(cube);
                bone.set("cubes", cubes);
                bones.push(bone);
            }
            Json desc = Json::object(), geo = Json::object(), list = Json::array(), root = Json::object();
            desc.set("identifier", Json::string("geometry." + id));
            desc.set("texture_width", Json::number(texW));
            desc.set("texture_height", Json::number(texH));
            geo.set("description", desc);
            geo.set("bones", bones);
            list.push(geo);
            root.set("format_version", Json::string("1.12.0"));
            root.set("minecraft:geometry", list);
            std::ofstream(dir + "/models/" + id + ".geo.json") << root.dump();
            break;
        }
        case 4: {
            const std::vector<std::string> items = creatorItems();
            if (items.empty()) return;
            int x0 = 3, y0 = 3, x1 = -1, y1 = -1;
            for (int i = 0; i < 9; ++i)
                if (creatorGrid_[i] >= 0) x0 = std::min(x0, i % 3), x1 = std::max(x1, i % 3), y0 = std::min(y0, i / 3), y1 = std::max(y1, i / 3);
            if (x1 < 0) return;
            Json pattern = Json::array(), key = Json::object();
            std::string letters;
            for (int yy = y0; yy <= y1; ++yy) {
                std::string row;
                for (int xx = x0; xx <= x1; ++xx) {
                    const int g = creatorGrid_[yy * 3 + xx];
                    if (g < 0) {
                        row += ' ';
                        continue;
                    }
                    const char l = (char)('A' + g);
                    if (letters.find(l) == std::string::npos) letters += l, key.set(std::string(1, l), Json::string(itemKey(kIngredients[g])));
                    row += l;
                }
                pattern.push(Json::string(row));
            }
            Json r = Json::object();
            r.set("type", Json::string("shaped"));
            r.set("pattern", pattern);
            r.set("key", key);
            r.set("result", Json::string(items[(size_t)(creatorResult_ % (int)items.size())]));
            r.set("count", Json::number(creatorCount_));
            fs::create_directories(dir + "/recipes", ec);
            Json book = readJson(dir + "/recipes/tvorba.json"), list = Json::array();
            for (size_t i = 0; i < book["recipes"].size(); ++i) list.push(book["recipes"][i]);
            list.push(r);
            book.set("recipes", list);
            std::ofstream(dir + "/recipes/tvorba.json") << book.dump();
            shown = tr("Recept");
            break;
        }
        case 5: {
            fs::create_directories(dir + "/shaders", ec);
            static const char* const kKeys[6] = {"saturation", "contrast", "warmth", "vignette", "glow", "fog_strength"};
            Json s = Json::object();
            s.set("name", Json::string(creatorShaderName_));
            for (int i = 0; i < 6; ++i) s.set(kKeys[i], Json::number(creatorShader_[i] / 100.0));
            std::ofstream(dir + "/shaders/shader.json") << s.dump();
            shown = creatorShaderName_;
            break;
        }
        default:
            shown = creatorPackName_;
            break;
    }
    packs_.setEnabled(kPack, true);
    rebuildFromPacks();
    toast_ = trf("Uloženo do doplňku Moje tvorba: {0}", {shown});
    toastUntil_ = now_ + 5;
}

}  // namespace vc
