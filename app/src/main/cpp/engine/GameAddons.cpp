// 2.7.0: the add-on manager: Add-ons, Behavior Voxel, Resource Voxel, Creator and Store tabs, the import
// report of every converted add-on (nothing fails silently) and the creator of blocks and mobs.
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "BedrockImport.h"
#include "Game.h"
#include "JavaImport.h"
#include "Json.h"
#include "Lang.h"
#include "Mods.h"
#include "Png.h"
#include "Store.h"
#include "WorldFiles.h"
#include "third_party/stb/stb_image.h"  // implementation lives in TexturePack.cpp

namespace vc {

namespace fs = std::filesystem;
using namespace theme;

namespace {

constexpr Color kOriginVc{0.45f, 0.8f, 0.47f, 1};
constexpr Color kOriginJava{0.9f, 0.67f, 0.31f, 1};
constexpr Color kOriginBedrock{0.47f, 0.67f, 1.0f, 1};
constexpr Color kOriginStore{1.0f, 0.82f, 0.43f, 1};
constexpr Color kOriginError{0.86f, 0.35f, 0.31f, 1};

const char* const kCreatorPack = "moje_tvorba";

// The creator's palette: 12 colours to paint with.
constexpr uint32_t kPalette[12] = {0xF0F0F0, 0x202020, 0x8C5A32, 0x5AA046, 0xC83C37, 0x3C78C8,
                                   0xE6C83C, 0x9696A0, 0xE68C3C, 0x8C50B4, 0xF0A0B4, 0x46B4AA};

}  // namespace

Game::AddonInfo Game::addonInfo(const PackInfo& p) {
    auto it = addonInfo_.find(p.id);
    if (it != addonInfo_.end()) return it->second;
    AddonInfo a;
    a.lines = readImportLines(p.dir);
    for (const ImportLine& l : a.lines) a.errors += l.status == 'x', a.warnings += l.status == '!';
    const ModRegistry::Summary sum = ModRegistry::get().summary(p.dir);
    a.blocks = sum.blocks, a.items = sum.items, a.mobs = sum.mobs;
    std::error_code ec;
    if (p.builtin) a.origin = "Vestavěný", a.kind = 0;
    else if (fs::is_directory(p.dir + "/bedrock", ec)) a.origin = "Bedrock > VC · un-UI", a.kind = 2;
    else if (fs::is_directory(p.dir + "/java", ec)) a.origin = "Java > VC · un-UI", a.kind = 1;
    else if (p.id.rfind("store_", 0) == 0 || p.id == "pre_alpha_style") a.origin = "VC · .vcpack (Obchod)", a.kind = 3;
    else a.origin = "VC · .vcaddon", a.kind = 0;
    if (a.errors > 0 && a.blocks + a.items + a.mobs == 0 && a.kind == 2) a.kind = 4;
    for (auto e = fs::recursive_directory_iterator(p.dir + "/textures", ec); e != fs::recursive_directory_iterator(); e.increment(ec)) {
        if (ec) break;
        if (e->is_regular_file()) ++a.textures;
    }
    addonInfo_[p.id] = a;
    return a;
}

void Game::addonTabs(float x, float y, float w) {
    static const char* const kTabs[5] = {"Doplňky", "Behavior", "Resource", "Tvůrce", "Obchod"};
    const float importW = 92, tw = std::floor((w - importW - 6 * 4) / 5);
    for (int i = 0; i < 5; ++i)
        if (ui_.button(x + i * (tw + 4), y, tw, 18, kTabs[i], true, addonTab_ == i)) addonTab_ = i, listScroll_ = 0;
    if (ui_.button(x + w - importW, y, importW, 18, "+ Importovat", true, true) && onImportPack) onImportPack();
}

void Game::addonManagerScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    static const char* const kTitles[5] = {"Správce doplňků", "Behavior Voxel", "Resource Voxel", "Tvůrce", "Obchod"};
    static const char* const kSubs[5] = {"Všechny doplňky na jednom místě: původ, typ, obsah, stav",
                                         "Chování: co věci dělají (bloky, předměty, moby, recepty, svět)",
                                         "Vzhled: textury, atlasy bloků, předmětů a mobů, shadery",
                                         "Bez psaní JSON: formulář, malování textury, uložení do doplňku",
                                         "Textury a balíčky za Voxel Bucks"};
    if (!addonReportPack_.empty()) {
        addonReportScreen(x, y, w, h);
        return;
    }
    ui_.text(x + 2, y + 2, kTitles[addonTab_], kTitle, 1, false);
    ui_.text(x + 2, y + 13, kSubs[addonTab_], kTextDim, 1, false, w);
    addonTabs(x, y + 24, w);
    const float cy = y + 46, ch = h - 46 - 28;
    packSelected_ = std::clamp(packSelected_, 0, std::max(0, (int)packList_.size() - 1));
    switch (addonTab_) {
        case 0: addonListTab(x, cy, w, ch); break;
        case 1: case 2: addonFilesTab(x, cy, w, ch, addonTab_ == 2); break;
        case 3: addonCreatorTab(x, cy, w, ch); break;
        default: addonStoreTab(x, cy, w, ch); break;
    }
    const float by = y + h - 24;
    if (ui_.button(x + w - 80, by, 80, 20, "Zpět")) setScreen(Screen::Title);
    if (addonTab_ == 0) {
        const std::string status = packWarnings_.empty() ? tr("Všechny soubory jsou v pořádku")
                                                         : trf("{0} problém(ů): {1}", {std::to_string(packWarnings_.size()), packWarnings_[0]});
        ui_.text(x + 2, by + 6, status, packWarnings_.empty() ? kAccent : kError, 1, false, w - 90);
    }
}

void Game::addonListTab(float x, float y, float w, float h) {
    const float rowH = 30, listH = h - 26;
    ui_.frame(x, y, w, listH, kPanelInner, kSeparator);
    listScroll_ -= ui_.dragY(x, y, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, packList_.size() * rowH - (listH - 4)));
    const bool preAlphaOwned = profile_.owns("textures:pre_alpha") || profile_.isDeveloper();
    for (size_t i = 0; i < packList_.size(); ++i) {
        const float ry = y + 2 + i * rowH - listScroll_;
        if (ry < y || ry + rowH > y + listH + 1) continue;
        PackInfo& p = packList_[i];
        const AddonInfo a = addonInfo(p);
        const bool sel = (int)i == packSelected_;
        static const Color kStripe[5] = {kOriginVc, kOriginJava, kOriginBedrock, kOriginStore, kOriginError};
        ui_.frame(x + 2, ry, w - 4, rowH - 2, sel ? kButtonDown : kButton, sel ? kGreenBorder : kSeparator);
        ui_.fill(x + 3, ry + 1, 3, rowH - 4, kStripe[a.kind]);
        // A little cube in the origin's colour.
        ui_.fill(x + 12, ry + 8, 11, 11, kStripe[a.kind]);
        ui_.fill(x + 12, ry + 8, 11, 3, Color{1, 1, 1, 0.35f});
        ui_.fill(x + 20, ry + 8, 3, 11, Color{0, 0, 0, 0.3f});
        ui_.text(x + 30, ry + 4, p.name, kTextBright, 1, false, w * 0.4f);
        ui_.text(x + 30, ry + 16, tr(a.origin), kTextDim, 1, false, w * 0.4f);
        std::string what;
        if (a.kind == 4) what = trf("{0} chyb > zobrazit chyby", {std::to_string(a.errors)});
        else if (a.blocks + a.items + a.mobs > 0)
            what = trf("{0} bloků · {1} předmětů · {2} mobů", {std::to_string(a.blocks), std::to_string(a.items), std::to_string(a.mobs)});
        else if (a.textures > 0) what = trf("{0} textur", {std::to_string(a.textures)});
        else if (!p.description.empty()) what = p.description;
        if (a.errors + a.warnings > 0 && a.kind != 4) what += trf("  ({0} chyb, {1} varování)", {std::to_string(a.errors), std::to_string(a.warnings)});
        ui_.text(x + w * 0.44f, ry + 10, what, a.kind == 4 ? kError : kText, 1, false, w * 0.34f);
        const bool locked = kFeatures270 && p.id == "pre_alpha_style" && !preAlphaOwned && !p.enabled;
        if (locked) {
            if (ui_.button(x + w - 118, ry + 5, 80, 18, "V obchodě")) addonTab_ = 4, listScroll_ = 0;
        } else if (ui_.button(x + w - 118, ry + 5, 80, 18, p.enabled ? "Zapnuto" : "Vypnuto", true, p.enabled)) {
            packs_.setEnabled(p.id, !p.enabled);
            rebuildFromPacks();
            return;
        }
        if (ui_.button(x + w - 34, ry + 5, 28, 18, "...")) packSelected_ = (int)i;
        if (ui_.tapped(x + 2, ry, w - 124, rowH - 2)) packSelected_ = (int)i;
    }
    if (packList_.empty()) return;
    // The selected add-on's actions.
    PackInfo& p = packList_[packSelected_];
    const int cols = kFeatures280 ? 7 : 6;
    const float by = y + listH + 4, bw = std::floor((w - (cols - 1) * 4) / cols);
    ui_.text(x, by - 0, "", kText, 1, false);
    if (ui_.button(x, by, bw, 20, "Chyby", !addonInfo(p).lines.empty())) addonReportPack_ = p.id;
    if (ui_.button(x + (bw + 4), by, bw, 20, "Výš", p.enabled)) {
        packs_.move(p.id, -1);
        rebuildFromPacks();
        packSelected_ = std::max(0, packSelected_ - 1);
    }
    if (ui_.button(x + 2 * (bw + 4), by, bw, 20, "Níž", p.enabled)) {
        packs_.move(p.id, 1);
        rebuildFromPacks();
        packSelected_ = std::min((int)packList_.size() - 1, packSelected_ + 1);
    }
    if (ui_.button(x + 3 * (bw + 4), by, bw, 20, "Behavior")) addonTab_ = 1, listScroll_ = 0;
    if (ui_.button(x + 4 * (bw + 4), by, bw, 20, "Resource")) addonTab_ = 2, listScroll_ = 0;
    if (p.builtin) {
        if (ui_.button(x + 5 * (bw + 4), by, bw, 20, "Obnovit")) {
            packs_.resetDefaultPack();
            rebuildFromPacks();
        }
    } else if (ui_.button(x + 5 * (bw + 4), by, bw, 20, "Smazat")) {
        packs_.deletePack(p.id);
        rebuildFromPacks();
    }
    if (kFeatures280 && ui_.button(x + 6 * (bw + 4), by, bw, 20, "Exportovat", true, true)) {
        // 2.8.0: the add-on as a .vcaddon (a zip of its folder) to Downloads/VoxelCraft: edit it and import it back.
        const std::string name = (p.builtin ? std::string("voxelcraft_default") : p.id) + ".vcaddon", file = dataDir_ + "/exports/" + name;
        std::string err;
        if (exportFolderZip(p.dir, file, err) && onExportWorld) onExportWorld(file, name), toast_ = trf("Doplněk exportován: {0}", {name});
        else toast_ = err.empty() ? tr("Export se nepovedl") : err;
        toastUntil_ = now_ + 5;
    }
}

void Game::addonReportScreen(float x, float y, float w, float h) {
    std::string name = addonReportPack_;
    std::string dir;
    for (const PackInfo& p : packList_)
        if (p.id == addonReportPack_) name = p.name, dir = p.dir;
    const std::vector<ImportLine> lines = dir.empty() ? std::vector<ImportLine>{} : readImportLines(dir);
    int errors = 0, warnings = 0;
    for (const ImportLine& l : lines) errors += l.status == 'x', warnings += l.status == '!';
    ui_.text(x + 2, y + 2, trf("Převod do VC: {0}", {name}), kTitle, 1, false, w);
    ui_.text(x + 2, y + 13, trf("+ převedeno   ! částečně   x nejde – chyby {0}, varování {1}", {std::to_string(errors), std::to_string(warnings)}),
             kTextDim, 1, false, w);
    const float listY = y + 26, listH = h - 26 - 28, rowH = 22;
    ui_.frame(x, listY, w, listH, kField, kSeparator);
    listScroll_ -= ui_.dragY(x, listY, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, lines.size() * rowH - (listH - 4)));
    for (size_t i = 0; i < lines.size(); ++i) {
        const float ry = listY + 2 + i * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        const ImportLine& l = lines[i];
        const Color c = l.status == '+' ? kOriginVc : l.status == '!' ? kGold : kError;
        ui_.text(x + 6, ry + 6, std::string(1, l.status == 'x' ? 'X' : l.status), c, 1, false);
        ui_.text(x + 18, ry + 2, l.file, c, 1, false, w - 24);
        ui_.text(x + 18, ry + 12, trf(l.text, l.args), kText, 1, false, w - 24);
    }
    if (lines.empty()) ui_.textCentered(x + w / 2, listY + listH / 2 - 4, tr("Tento doplněk nemá zprávu o převodu"), kTextDim);
    const float by = y + h - 24;
    if (ui_.button(x + w - 170, by, 80, 20, "Behavior")) addonReportPack_.clear(), addonTab_ = 1, listScroll_ = 0;
    if (ui_.button(x + w - 84, by, 84, 20, "Zavřít", true, true)) addonReportPack_.clear(), listScroll_ = 0;
}

void Game::addonFilesTab(float x, float y, float w, float h, bool resource) {
    if (packList_.empty()) return;
    if (resource && !texEditFile_.empty()) {
        textureEditor(x, y, w, h);
        return;
    }
    // Which add-on: a row of arrows above the tree.
    PackInfo& p = packList_[packSelected_];
    if (ui_.button(x, y, 18, 16, "<", packSelected_ > 0)) --packSelected_, listScroll_ = 0;
    if (ui_.button(x + 22, y, 18, 16, ">", packSelected_ + 1 < (int)packList_.size())) ++packSelected_, listScroll_ = 0;
    ui_.text(x + 46, y + 4, p.name + "  /  " + (resource ? "resource" : "behavior"), kGold, 1, false, w - 50);
    std::vector<std::string> files;
    std::error_code ec;
    if (resource) {
        for (auto e = fs::recursive_directory_iterator(p.dir, ec); e != fs::recursive_directory_iterator(); e.increment(ec)) {
            if (ec) break;
            if (!e->is_regular_file()) continue;
            const std::string rel = fs::relative(e->path(), p.dir, ec).generic_string();
            if (rel.size() > 7 && rel.compare(rel.size() - 7, 7, ".edited") == 0) continue;
            if ((rel.rfind("textures/", 0) == 0 || rel.rfind("shaders/", 0) == 0 || rel.rfind("theme/", 0) == 0 || rel.rfind("vc/", 0) == 0 ||
                 rel.rfind("bedrock/", 0) == 0 && rel.find("/textures/") != std::string::npos) &&
                files.size() < 1500)
                files.push_back(rel);
        }
        std::sort(files.begin(), files.end());
    } else {
        for (const std::string& f : packs_.files(p.id))
            if (f != "manifest.json") files.push_back(f);
    }
    const float listY = y + 20, listH = h - 20, rowH = 16;
    ui_.frame(x, listY, w, listH, kField, kSeparator);
    listScroll_ -= ui_.dragY(x, listY, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, files.size() * rowH - (listH - 4)));
    std::string folder;
    for (size_t i = 0; i < files.size(); ++i) {
        const float ry = listY + 2 + i * rowH - listScroll_;
        const std::string& f = files[i];
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        const size_t slash = f.find('/');
        const std::string top = slash == std::string::npos ? "" : f.substr(0, slash);
        const Color c = top == "textures" ? kOriginBedrock : top == "recipes" ? kOriginJava : top == "biomes" ? kOriginVc
                      : top == "dimensions" ? kGold : top == "shaders" ? Color{0.75f, 0.55f, 0.9f, 1} : kText;
        ui_.fill(x + 6, ry + 4, 6, 7, c);
        ui_.text(x + 18, ry + 4, f, kText, 1, false, w - 24);
        if (ui_.tapped(x + 2, ry, w - 4, rowH - 1)) {
            const auto ends = [&](const char* e) { return f.size() > std::strlen(e) && f.compare(f.size() - std::strlen(e), std::strlen(e), e) == 0; };
            if (!resource || ends(".json") || ends(".fsh") || ends(".vsh") || ends(".txt") || ends(".example")) {
                openEditor(p.id, f);  // 2.8.0: shader.json, theme.json, vc/ files and shaders open as text too
                return;
            }
            if (ends(".png")) {
                // 2.8.0: a texture opens in the painter.
                int iw = 0, ih = 0, n = 0;
                if (stbi_uc* px = stbi_load((p.dir + "/" + f).c_str(), &iw, &ih, &n, 4)) {
                    if (iw >= 1 && ih >= 1 && iw <= 64 && ih <= 64) {
                        texEditPix_.assign((size_t)iw * ih, 0);
                        for (int k = 0; k < iw * ih; ++k)
                            texEditPix_[(size_t)k] = (uint32_t)px[k * 4] << 24 | (uint32_t)px[k * 4 + 1] << 16 | (uint32_t)px[k * 4 + 2] << 8 | px[k * 4 + 3];
                        texEditW_ = iw, texEditH_ = ih, texEditPack_ = p.id, texEditFile_ = f;
                    } else {
                        toast_ = tr("Obrázek je větší než 64 × 64: uprav ho v počítači"), toastUntil_ = now_ + 4;
                    }
                    stbi_image_free(px);
                }
                return;
            }
        }
    }
    if (files.empty()) ui_.textCentered(x + w / 2, listY + listH / 2 - 4, tr(resource ? "Žádné textury ani shadery" : "Žádné soubory chování"), kTextDim);
    if (!resource && ui_.button(x + w - 110, y, 110, 16, "+ Nový biom", true, true)) {
        // 2.8.0: a new biome right here (it used to be only in the old file list).
        const std::vector<std::string> have = packs_.files(p.id);
        std::string path;
        for (int n = 1;; ++n) {
            path = "biomes/novy_biom_" + std::to_string(n) + ".json";
            if (std::find(have.begin(), have.end(), path) == have.end()) break;
        }
        const std::string id = path.substr(7, path.size() - 12);
        packs_.writeFile(p.id, path,
                         "{\n  \"id\": \"" + id + "\",\n  \"name\": \"Nový biom\",\n"
                         "  \"climate\": {\"temperature\": 0.3, \"humidity\": 0.2},\n"
                         "  \"terrain\": {\"height\": 70, \"variation\": 12},\n"
                         "  \"blocks\": {\"surface\": \"grass\", \"filler\": \"dirt\", \"filler_depth\": 3, \"beach\": \"sand\", \"underwater\": \"gravel\"},\n"
                         "  \"trees\": {\"density\": 0.02, \"trunk\": \"log\", \"leaves\": \"leaves\"}\n}\n");
        rebuildFromPacks();
        openEditor(p.id, path);
    }
}

void Game::textureEditor(float x, float y, float w, float h) {
    // The picture, the palette beside it, and Uložit / Zpět. Colours are RGBA; the eraser paints transparent.
    static const uint32_t kColors[14] = {0xF0F0F0FF, 0x202020FF, 0x8C5A32FF, 0x5AA046FF, 0xC83C37FF, 0x3C78C8FF, 0xE6C83CFF,
                                         0x9696A0FF, 0xE68C3CFF, 0x8C50B4FF, 0xF0A0B4FF, 0x46B4AAFF, 0x606060FF, 0x00000000};
    ui_.text(x, y + 2, texEditPack_ + " / " + texEditFile_, kGold, 1, false, w);
    const float top = y + 16, cell = std::max(1.0f, std::floor(std::min((w - 100) / texEditW_, (h - 20) / texEditH_)));
    ui_.frame(x - 1, top - 1, texEditW_ * cell + 2, texEditH_ * cell + 2, kField, kSeparator);
    for (int i = 0; i < texEditW_ * texEditH_; ++i) {
        const float px = x + (i % texEditW_) * cell, py = top + (i / texEditW_) * cell;
        const uint32_t c = texEditPix_[(size_t)i];
        if ((c & 255) < 128) ui_.fill(px, py, cell, cell, ((i % texEditW_) + (i / texEditW_)) % 2 ? Color{0.3f, 0.3f, 0.3f, 1} : Color{0.4f, 0.4f, 0.4f, 1});
        else ui_.fill(px, py, cell, cell, Color{(c >> 24) / 255.0f, ((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, 1});
        if (ui_.held(px, py, cell, cell)) texEditPix_[(size_t)i] = kColors[creatorColor_ % 14];
    }
    const float palX = x + texEditW_ * cell + 10;
    for (int k = 0; k < 14; ++k) {
        const uint32_t c = kColors[k];
        const float cx = palX + (k % 2) * 16, cy = top + (k / 2) * 14;
        ui_.frame(cx, cy, 14, 12, k == 13 ? Color{0.35f, 0.35f, 0.35f, 1} : Color{(c >> 24) / 255.0f, ((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, 1},
                  k == creatorColor_ % 14 ? kTextBright : kSeparator);
        if (ui_.tapped(cx, cy, 14, 12)) creatorColor_ = k;
    }
    if (ui_.button(palX, top + 104, 70, 18, "Uložit", true, true)) {
        std::vector<uint8_t> rgba((size_t)texEditW_ * texEditH_ * 4);
        for (size_t k = 0; k < texEditPix_.size(); ++k)
            rgba[k * 4] = texEditPix_[k] >> 24, rgba[k * 4 + 1] = (texEditPix_[k] >> 16) & 255, rgba[k * 4 + 2] = (texEditPix_[k] >> 8) & 255, rgba[k * 4 + 3] = texEditPix_[k] & 255;
        writePng(packs_.packDir(texEditPack_) + "/" + texEditFile_, texEditW_, texEditH_, rgba);
        packs_.markEdited(texEditPack_, texEditFile_);
        packs_.setEnabled(texEditPack_, true);
        rebuildFromPacks();
        textureReload_ = true;
        toast_ = trf("Uloženo: {0}", {texEditFile_}), toastUntil_ = now_ + 4;
    }
    if (ui_.button(palX, top + 126, 70, 18, "Zpět")) texEditFile_.clear();
}

void Game::addonStoreTab(float x, float y, float w, float h) {
    ui_.text(x, y, trf("Voxel Bucks: {0}", {std::to_string(profile_.coins)}), kGold, 1, false);
    std::vector<const StoreItem*> shown;
    for (int pass = 0; pass < 2; ++pass)  // the textures first (the Pre-Alpha Style among them), then the packs
        for (const StoreItem& it : storeCatalog())
            if (it.kind == (pass == 0 ? StoreItem::Kind::Textures : StoreItem::Kind::ModPack)) shown.push_back(&it);
    const float listY = y + 14, listH = h - 14;
    listScroll_ -= ui_.dragY(x, listY, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, shown.size() * 31.0f - listH));
    float ry = listY - listScroll_;
    for (const StoreItem* ip : shown) {
        const StoreItem& it = *ip;
        if (ry < listY - 1 || ry + 28 > listY + listH) {
            ry += 31;
            continue;
        }
        ui_.frame(x, ry, w, 28, kButton, kSeparator);
        ui_.fill(x + 1, ry + 1, 3, 26, it.kind == StoreItem::Kind::Textures ? kOriginStore : kOriginVc);
        ui_.text(x + 10, ry + 4, it.title, kTextBright, 1, false, w * 0.5f);
        ui_.text(x + 10, ry + 15, tr(it.description), kTextDim, 1, false, w * 0.6f);
        const bool owned = profile_.owns(it.id);
        const std::string label = owned ? tr("Vlastníš") : trf("{0} VB", {std::to_string(it.price)});
        if (ui_.button(x + w - 90, ry + 5, 84, 18, label, !owned, !owned)) {
            std::string err;
            const BuyResult r = buyStoreItem(it, profile_, packs_, err);
            toast_ = r == BuyResult::Bought ? trf("Koupeno: {0}", {it.title}) : r == BuyResult::NotEnoughCoins ? tr("Málo Voxel Bucks") : tr("Nákup se nepovedl");
            toastUntil_ = now_ + 4;
            profile_.save(profilePath_);
            rebuildFromPacks();
        }
        ry += 31;
    }
}

void Game::addonCreatorTab(float x, float y, float w, float h) {
    if (kFeatures280) {
        vcCreatorTab(x, y, w, h);  // 2.8.0: the VC editor with everything (GameCreator.cpp)
        return;
    }
    // Left: the form; middle: the 16x16 picture; right: the preview and saving.
    if (creatorPixels_.empty()) creatorPixels_.assign(256, 0xF0F0F0);
    const float fw = std::floor(w * 0.3f);
    ui_.frame(x, y, fw, h, kPanelInner, kSeparator);
    if (ui_.button(x + 4, y + 4, fw / 2 - 6, 16, "Blok", true, !creatorMob_)) creatorMob_ = false;
    if (ui_.button(x + fw / 2 + 2, y + 4, fw / 2 - 6, 16, "Mob", true, creatorMob_)) creatorMob_ = true;
    float ry = y + 26;
    ui_.text(x + 6, ry, "Název", kTextDim, 1, false);
    if (ui_.button(x + 6, ry + 10, fw - 12, 16, creatorName_.empty() ? tr("Klepni a napiš") : creatorName_) && onTextPrompt)
        onTextPrompt(PromptCreatorName, "Název", creatorName_.empty() ? "Můj blok" : creatorName_);
    ry += 32;
    if (!creatorMob_) {
        static const char* const kHard[4] = {"Měkký", "Střední", "Tvrdý", "Velmi tvrdý"};
        ui_.text(x + 6, ry, "Tvrdost", kTextDim, 1, false);
        if (ui_.button(x + 6, ry + 10, fw - 12, 16, kHard[creatorHard_])) creatorHard_ = (creatorHard_ + 1) % 4;
        ry += 32;
        ui_.text(x + 6, ry, "Svítí", kTextDim, 1, false);
        if (ui_.button(x + 6, ry + 10, fw - 12, 16, creatorLight_ ? "Ano" : "Ne")) creatorLight_ = !creatorLight_;
    } else {
        static const char* const kBase[7] = {"pig", "cow", "sheep", "chicken", "spider", "zombie", "villager"};
        static const char* const kBaseCs[7] = {"Prase", "Kráva", "Ovce", "Slepice", "Pavouk", "Zombie", "Vesničan"};
        ui_.text(x + 6, ry, "Tělo", kTextDim, 1, false);
        if (ui_.button(x + 6, ry + 10, fw - 12, 16, kBaseCs[creatorBase_ % 7])) creatorBase_ = (creatorBase_ + 1) % 7;
        ry += 32;
        ui_.text(x + 6, ry, "Životy", kTextDim, 1, false);
        if (ui_.button(x + 6, ry + 10, fw - 12, 16, std::to_string(creatorHealth_))) creatorHealth_ = creatorHealth_ >= 40 ? 4 : creatorHealth_ + 4;
        (void)kBase;
    }
    // The picture.
    const float px0 = x + fw + 6, cell = std::floor(std::min((w - fw - 12) * 0.55f, h - 30) / 16), gw = cell * 16;
    ui_.frame(px0 - 2, y, gw + 4, gw + 4, kField, kSeparator);
    for (int i = 0; i < 256; ++i) {
        const uint32_t c = creatorPixels_[i];
        const float cx = px0 + (i % 16) * cell, cy = y + 2 + (i / 16) * cell;
        ui_.fill(cx, cy, cell - (cell > 4 ? 1 : 0), cell - (cell > 4 ? 1 : 0), Color{((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, 1});
        if (ui_.held(cx, cy, cell, cell)) creatorPixels_[i] = creatorFill_ ? creatorPixels_[i] : kPalette[creatorColor_];
        if (creatorFill_ && ui_.tapped(cx, cy, cell, cell)) {
            const uint32_t from = creatorPixels_[i], to = kPalette[creatorColor_];
            if (from != to) {
                std::vector<int> stack{i};
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
    }
    const float palY = y + gw + 8;
    for (int k = 0; k < 12; ++k) {
        const uint32_t c = kPalette[k];
        const float cx = px0 + (k % 6) * (gw / 6), cy = palY + (k / 6) * 14;
        ui_.frame(cx, cy, gw / 6 - 2, 12, Color{((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, 1},
                  k == creatorColor_ ? kTextBright : kSeparator);
        if (ui_.tapped(cx, cy, gw / 6 - 2, 12)) creatorColor_ = k;
    }
    // Tools and saving.
    const float rx = px0 + gw + 10, rw = x + w - rx;
    if (ui_.button(rx, y, rw, 18, creatorFill_ ? "Výplň" : "Štětec")) creatorFill_ = !creatorFill_;
    if (ui_.button(rx, y + 22, rw, 18, "Vymazat")) creatorPixels_.assign(256, kPalette[creatorColor_]);
    float ty = y + 48;
    for (const std::string& l : wrapText(tr(creatorMob_ ? "Mob dostane tělo zvoleného tvora a tuto kůži." : "Blok má tuto texturu na všech stranách."), rw))
        ui_.text(rx, ty, l, kTextDim, 1, false, rw), ty += 10;
    if (ui_.button(rx, y + h - 20, rw, 20, "Uložit do .vcaddon", !creatorName_.empty(), true)) saveCreation();
}

void Game::saveCreation() {
    // Into the pack "moje_tvorba": its texture in textures/mod and an entry in content.json.
    std::error_code ec;
    const std::string dir = packs_.packDir(kCreatorPack);
    fs::create_directories(dir + "/textures/mod", ec);
    if (!fs::exists(dir + "/manifest.json"))
        std::ofstream(dir + "/manifest.json") << R"({"name": "Moje tvorba", "description": "Vytvořeno v Tvůrci", "version": "1.0"})";
    std::string id;
    for (char c : creatorName_) {
        const unsigned char u = (unsigned char)c;
        if (std::isalnum(u)) id += (char)std::tolower(u);
        else if (c == ' ' || c == '-') id += '_';
    }
    if (id.empty()) id = "vec";
    id = id.substr(0, 30) + (creatorMob_ ? "_mob" : "");
    std::vector<uint8_t> rgba(256 * 4);
    for (int i = 0; i < 256; ++i) {
        const uint32_t c = creatorPixels_[i];
        rgba[i * 4] = (c >> 16) & 255, rgba[i * 4 + 1] = (c >> 8) & 255, rgba[i * 4 + 2] = c & 255, rgba[i * 4 + 3] = 255;
    }
    writePng(dir + "/textures/mod/" + id + ".png", 16, 16, rgba);
    Json doc;
    std::string err;
    {
        std::ifstream in(dir + "/content.json");
        std::stringstream ss;
        ss << in.rdbuf();
        if (!Json::parse(ss.str(), doc, err) || !doc.isObject()) doc = Json::object(), doc.set("namespace", Json::string("tvorba"));
    }
    if (doc["namespace"].isNull()) doc.set("namespace", Json::string("tvorba"));
    const char* listName = creatorMob_ ? "mobs" : "blocks";
    Json list = Json::array();
    for (size_t i = 0; i < doc[listName].size(); ++i)
        if (doc[listName][i]["id"].str("") != id) list.push(doc[listName][i]);
    Json e = Json::object();
    e.set("id", Json::string(id));
    e.set("name", Json::string(creatorName_));
    e.set("name_cs", Json::string(creatorName_));
    e.set("texture", Json::string(id + ".png"));
    if (creatorMob_) {
        static const char* const kBase[7] = {"pig", "cow", "sheep", "chicken", "spider", "zombie", "villager"};
        e.set("base", Json::string(kBase[creatorBase_ % 7]));
        e.set("face", Json::string(id + ".png"));
        e.set("health", Json::number(creatorHealth_));
        e.set("spawn", Json::number(0.1));
    } else {
        static const float kHard[4] = {0.5f, 1.5f, 3.0f, 6.0f};
        e.set("hardness", Json::number(kHard[creatorHard_]));
        e.set("tool", Json::string(creatorHard_ >= 2 ? "pickaxe" : ""));
        if (creatorLight_) e.set("light", Json::fromBool(true));
    }
    list.push(e);
    doc.set(listName, list);
    for (const char* other : {"blocks", "items", "mobs"})
        if (doc[other].isNull()) doc.set(other, Json::array());
    std::ofstream(dir + "/content.json") << doc.dump();
    packs_.setEnabled(kCreatorPack, true);
    rebuildFromPacks();
    toast_ = trf("Uloženo do doplňku Moje tvorba: {0}", {creatorName_});
    toastUntil_ = now_ + 5;
}

}  // namespace vc
