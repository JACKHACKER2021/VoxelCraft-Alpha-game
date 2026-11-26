// Archived in 2.7.0: the add-on maker of VoxelCraft 1.4.0-2.6.0 (Mods and Packs, the pack files, the JSON editor).
// It still runs in the 2.6.0 edition (release/VoxelCraft-2.6.0.apk); in 2.7.0 the add-on manager (GameAddons.cpp) replaces it.

void Game::packsScreen() {
    if (kFeatures270) {  // 2.7.0: the new add-on manager; the old screen stays in the older editions
        addonManagerScreen();
        return;
    }
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    ui_.text(x + 2, y + 2, "Módy a balíčky", kTitle, 1, false);
    ui_.text(x + 2, y + 14, "Zapnuté balíčky se použijí odshora dolů; pozdější přepisují dřívější.", kTextDim, 1, false, w);

    const float listY = y + 28, listH = h - 28 - 32, listW = std::floor(w * 0.45f), rowH = 22;
    ui_.frame(x, listY, listW, listH, kPanelInner, kSeparator);
    listScroll_ -= ui_.dragY(x, listY, listW, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, packList_.size() * rowH - (listH - 4)));
    for (size_t i = 0; i < packList_.size(); ++i) {
        float ry = listY + 2 + i * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        PackInfo& p = packList_[i];
        bool sel = (int)i == packSelected_;
        ui_.frame(x + 2, ry, listW - 4, rowH - 2, sel ? kGreen : kButton, sel ? kGreenBorder : kSeparator);
        bool on = p.enabled;
        if (ui_.checkbox(x + 6, ry + 4, on)) {
            packs_.setEnabled(p.id, on);
            rebuildFromPacks();
            return;
        }
        ui_.text(x + 24, ry + 6, p.name, sel ? kTextBright : kText, 1, false, listW - 32);
        if (ui_.tapped(x + 22, ry, listW - 26, rowH - 2)) packSelected_ = (int)i;
    }

    // Details of the selected pack
    float dx = x + listW + 8, dw = w - listW - 8;
    ui_.frame(dx, listY, dw, listH, kPanelInner, kSeparator);
    if (!packList_.empty()) {
        PackInfo& p = packList_[packSelected_];
        float ty = listY + 6;
        ui_.text(dx + 6, ty, p.name, kTextBright, 1, false, dw - 12);
        ty += 12;
        ui_.text(dx + 6, ty, tr(p.builtin ? "Vestavěný" : "Doplněk") + (p.version.empty() ? std::string() : trf(" · verze {0}", {p.version})),
                 kTextDim, 1, false, dw - 12);
        ty += 12;
        // What the mod adds, and for a Java import what came across and what did not.
        const ModRegistry::Summary sum = ModRegistry::get().summary(p.dir);
        if (sum.blocks + sum.items + sum.mobs > 0) {
            ui_.text(dx + 6, ty, trf("Bloky: {0} · předměty: {1} · moby: {2}",
                                     {std::to_string(sum.blocks), std::to_string(sum.items), std::to_string(sum.mobs)}),
                     kAccent, 1, false, dw - 12);
            ty += 12;
        }
        for (const std::string& line : readImportReport(p.dir)) {
            const size_t eq = line.find('=');
            if (eq == std::string::npos || ty > listY + listH - 84) continue;
            const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            std::string shown;
            if (k == "skipped_minecraft" && v != "0") shown = trf("Přeskočeno ze hry Minecraft: {0} souborů", {v});
            else if (k == "skipped_shapes" && v != "0") shown = trf("Bloky jiného tvaru než kostka: {0}", {v});
            else if (k == "skipped_recipes" && v != "0") shown = trf("Recepty se štítky nebo cizími věcmi: {0}", {v});
            else if (k == "classes" && v != "0") shown = trf("Kód v Javě se nespouští ({0} tříd)", {v});
            if (shown.empty()) continue;
            ui_.text(dx + 6, ty, shown, kTextDim, 1, false, dw - 12);
            ty += 10;
        }
        for (const std::string& line : wrapText(p.description, dw - 12)) {
            if (ty > listY + listH - 74) break;
            ui_.text(dx + 6, ty, line, kText, 1, false, dw - 12);
            ty += 10;
        }
        float by = listY + listH - 70, bw = dw - 12;
        if (ui_.button(dx + 6, by, bw, 20, "Upravit soubory JSON", true, true)) {
            filesPack_ = p.id;
            packFiles_ = packs_.files(p.id);
            setScreen(Screen::PackFiles);
            return;
        }
        float hw = (bw - 4) / 2;
        if (ui_.button(dx + 6, by + 22, hw, 20, "Výš", p.enabled)) {
            packs_.move(p.id, -1);
            rebuildFromPacks();
            packSelected_ = std::max(0, packSelected_ - 1);
        }
        if (ui_.button(dx + 10 + hw, by + 22, hw, 20, "Níž", p.enabled)) {
            packs_.move(p.id, 1);
            rebuildFromPacks();
            packSelected_ = std::min((int)packList_.size() - 1, packSelected_ + 1);
        }
        if (p.builtin) {
            if (ui_.button(dx + 6, by + 44, bw, 20, "Obnovit výchozí obsah")) {
                packs_.resetDefaultPack();
                rebuildFromPacks();
            }
        } else if (ui_.button(dx + 6, by + 44, bw, 20, "Smazat balíček")) {
            packs_.deletePack(p.id);
            rebuildFromPacks();
        }
    }

    // Problems found in the pack files
    std::string status = packWarnings_.empty() ? "Všechny soubory jsou v pořádku"
                                               : trf("{0} problém(ů): {1}", {std::to_string(packWarnings_.size()), packWarnings_[0]});
    ui_.text(x + 2, y + h - 40, status, packWarnings_.empty() ? kAccent : kError, 1, false, w - 4);

    float by = y + h - 26, bw = std::floor((w - 24) / 4);
    if (ui_.button(x, by, bw, 22, "Importovat .zip") && onImportPack) onImportPack();
    if (ui_.button(x + bw + 8, by, bw, 22, "Nový balíček") && onTextPrompt)
        onTextPrompt(PromptPackName, "Název nového balíčku", "Můj balíček");
    if (ui_.button(x + 2 * (bw + 8), by, bw, 22, "Nový shader pack") && onTextPrompt)
        onTextPrompt(PromptShaderPackName, "Název shader packu", "Můj shader");
    if (ui_.button(x + 3 * (bw + 8), by, bw, 22, "Zpět")) setScreen(Screen::Title);
}

void Game::packFilesScreen() {
    float x, y, w, h;
    menuWindow(x, y, w, h, false);
    std::string name = filesPack_;
    for (const PackInfo& p : packList_)
        if (p.id == filesPack_) name = p.name;
    ui_.text(x + 2, y + 2, name, kTitle, 1, false, w);
    ui_.text(x + 2, y + 14, "packs/" + filesPack_ + "/", kAccent, 1, false, w);

    const float listY = y + 28, listH = h - 28 - 32, rowH = 18;
    ui_.frame(x, listY, w, listH, kField, kSeparator);
    listScroll_ -= ui_.dragY(x, listY, w, listH);
    listScroll_ = std::clamp(listScroll_, 0.0f, std::max(0.0f, packFiles_.size() * rowH - (listH - 4)));
    for (size_t i = 0; i < packFiles_.size(); ++i) {
        float ry = listY + 2 + i * rowH - listScroll_;
        if (ry < listY || ry + rowH > listY + listH + 1) continue;
        const std::string& f = packFiles_[i];
        bool down = ui_.held(x + 2, ry, w - 4, rowH - 1);
        if (down) ui_.fill(x + 2, ry, w - 4, rowH - 1, kButton);
        // Folder colour hints, like the file browser mock-up.
        Color c = f.rfind("biomes/", 0) == 0 ? kAccent : f.rfind("dimensions/", 0) == 0 ? kGold
                : f.rfind("recipes/", 0) == 0 ? Color{0.45f, 0.65f, 0.75f, 1} : kText;
        ui_.fill(x + 6, ry + 5, 6, 7, c);
        ui_.text(x + 18, ry + 5, f, kText, 1, false, w - 24);
        if (ui_.tapped(x + 2, ry, w - 4, rowH - 1)) {
            openEditor(filesPack_, f);
            return;
        }
    }

    float by = y + h - 26, bw = std::min(160.0f, (w - 8) / 2);
    if (ui_.button(x + w / 2 - bw - 4, by, bw, 22, "Nový biom")) {
        std::string path;
        for (int n = 1;; ++n) {
            path = "biomes/novy_biom_" + std::to_string(n) + ".json";
            if (std::find(packFiles_.begin(), packFiles_.end(), path) == packFiles_.end()) break;
        }
        std::string id = path.substr(7, path.size() - 12);
        packs_.writeFile(filesPack_, path,
                         "{\n  \"id\": \"" + id + "\",\n  \"name\": \"Nový biom\",\n"
                         "  \"climate\": {\"temperature\": 0.3, \"humidity\": 0.2},\n"
                         "  \"terrain\": {\"height\": 70, \"variation\": 12},\n"
                         "  \"blocks\": {\"surface\": \"grass\", \"filler\": \"dirt\", \"filler_depth\": 3, \"beach\": \"sand\", \"underwater\": \"gravel\"},\n"
                         "  \"trees\": {\"density\": 0.02, \"trunk\": \"log\", \"leaves\": \"leaves\"}\n}\n");
        packFiles_ = packs_.files(filesPack_);
        rebuildFromPacks();
        openEditor(filesPack_, path);
        return;
    }
    if (ui_.button(x + w / 2 + 4, by, bw, 22, "Zpět")) setScreen(Screen::Packs);
}

