// Renders the menus and HUD to PNG files with a tiny software rasterizer,
// so the UI can be reviewed without a phone. Output: build/snapshots/*.png
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "engine/Game.h"
#include "engine/Multiplayer.h"
#include "engine/Session.h"
#include "engine/Save.h"
#include "engine/Lang.h"
#include "stub_capture.h"

using namespace vc;

void writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);  // core_test.cpp helper copy below

namespace {

std::vector<uint8_t> rasterize(int scale) {
    const auto& L = capture::ui;
    int W = (int)(capture::guiW * scale), H = (int)(capture::guiH * scale);
    std::vector<float> img(W * H * 3);
    // Stand-in for the 3D world: sky gradient over a grassy horizon.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float* p = &img[(y * W + x) * 3];
            if (y < H * 0.55f) { p[0] = 0.53f; p[1] = 0.74f; p[2] = 1.0f; }
            else { p[0] = 0.32f; p[1] = 0.55f; p[2] = 0.2f; }
        }
    auto sample = [&](int tex, float u, float v, float out[4]) {
        const std::vector<uint8_t>& t = tex == TexAtlas ? capture::atlas : tex == TexSkins ? capture::skins : tex == TexMap ? capture::map : capture::gui;
        int tw = tex == TexAtlas ? capture::atlasSize : tex == TexSkins ? capture::skinsW : tex == TexMap ? capture::mapW : capture::guiW_px;
        int th = tex == TexAtlas ? capture::atlasSize : tex == TexSkins ? capture::skinsH : tex == TexMap ? capture::mapH : capture::guiH_px;
        if (t.size() < (size_t)tw * th * 4) {
            out[0] = out[1] = out[2] = out[3] = 0;
            return;
        }
        int x = std::clamp((int)(u * tw), 0, tw - 1), y = std::clamp((int)(v * th), 0, th - 1);
        for (int i = 0; i < 4; ++i) out[i] = t[(y * tw + x) * 4 + i] / 255.0f;
    };
    size_t first = 0;
    for (auto [tex, count] : L.runs) {
        for (size_t i = first; i < first + count; i += 3) {
            const UiVert* v[3] = {&L.verts[i], &L.verts[i + 1], &L.verts[i + 2]};
            float x0 = std::min({v[0]->x, v[1]->x, v[2]->x}) * scale, x1 = std::max({v[0]->x, v[1]->x, v[2]->x}) * scale;
            float y0 = std::min({v[0]->y, v[1]->y, v[2]->y}) * scale, y1 = std::max({v[0]->y, v[1]->y, v[2]->y}) * scale;
            float ax = v[0]->x * scale, ay = v[0]->y * scale, bx = v[1]->x * scale, by = v[1]->y * scale;
            float cx = v[2]->x * scale, cy = v[2]->y * scale;
            float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (std::fabs(area) < 1e-6f) continue;
            for (int py = std::max(0, (int)y0); py < std::min(H, (int)std::ceil(y1)); ++py)
                for (int px = std::max(0, (int)x0); px < std::min(W, (int)std::ceil(x1)); ++px) {
                    float sx = px + 0.5f, sy = py + 0.5f;
                    float w0 = ((bx - sx) * (cy - sy) - (by - sy) * (cx - sx)) / area;
                    float w1 = ((cx - sx) * (ay - sy) - (cy - sy) * (ax - sx)) / area;
                    float w2 = 1 - w0 - w1;
                    if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                    float c[4] = {1, 1, 1, 1};
                    if (tex != TexNone) {
                        float u = w0 * v[0]->u + w1 * v[1]->u + w2 * v[2]->u, vv = w0 * v[0]->v + w1 * v[1]->v + w2 * v[2]->v;
                        sample(tex, u, vv, c);
                    }
                    float r = c[0] * v[0]->r, g = c[1] * v[0]->g, b = c[2] * v[0]->b, a = c[3] * v[0]->a;
                    float* p = &img[(py * W + px) * 3];
                    p[0] = p[0] * (1 - a) + r * a;
                    p[1] = p[1] * (1 - a) + g * a;
                    p[2] = p[2] * (1 - a) + b * a;
                }
        }
        first += count;
    }
    std::vector<uint8_t> out(W * H * 4);
    for (int i = 0; i < W * H; ++i) {
        for (int k = 0; k < 3; ++k) out[i * 4 + k] = (uint8_t)std::clamp(img[i * 3 + k] * 255.0f, 0.0f, 255.0f);
        out[i * 4 + 3] = 255;
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    fs::path out = argc > 1 ? argv[1] : "build/snapshots";
    fs::create_directories(out);
    fs::path data = fs::temp_directory_path() / "vc_snapshots";
    fs::remove_all(data);
    fs::create_directories(data);

    Game game(data.string());

    game.skipSplash();
    game.onSurfaceCreated();
    game.onResize(2400, 1080);  // GUI 600 x 270
    double t = 1;
    auto frames = [&](int n) { for (int i = 0; i < n; ++i) game.frame(t += 1 / 60.0); };
    auto tap = [&](float gx, float gy) {
        game.onTouch(0, TouchAction::Down, gx * 4, gy * 4, t);
        game.onTouch(0, TouchAction::Up, gx * 4, gy * 4, t);
        frames(2);
    };
    auto shot = [&](const char* name) {
        int w = (int)(capture::guiW * 2), h = (int)(capture::guiH * 2);
        writePng((out / (std::string(name) + ".png")).string(), w, h, rasterize(2));
        std::printf("wrote %s\n", (out / (std::string(name) + ".png")).c_str());
    };

    frames(3);
    shot("01_title");
    tap(575, 265);  // VoxelSoftworks: the credits
    frames(200);
    shot("01b_credits");
    tap(570, 255);  // Skip
    frames(2);
    shot("01c_trailers");
    game.onBack();
    frames(2);
    tap(378, 121);  // Nastavení
    shot("02_settings_touch");
    tap(103, 81);
    shot("03_settings_video");
    tap(103, 107);
    shot("03a_settings_graphics");
    tap(103, 133);
    shot("03b_settings_audio");
    tap(103, 159);
    shot("03c_settings_profile");
    tap(514, 120);  // Úspěchy: Otevřít
    shot("03d_achievements");
    game.onBack();
    frames(1);
    tap(103, 185);
    shot("04_settings_resources");
    tap(539, 244);  // Zpět
    tap(222, 149);  // Obchod
    shot("16_store");
    tap(261, 135);  // Koupit (first card)
    shot("17_store_confirm");
    tap(219, 163);  // confirm
    frames(2);
    shot("18_store_bought");
    game.onBack();  // closes the dialog (not enough coins for the knight)
    frames(1);
    tap(390, 44);  // VIP a doplňky
    shot("18b_store_vip");
    tap(490, 44);  // Odměny
    shot("18c_store_rewards");
    tap(150, 140);  // Vyzvednout odměnu
    tap(455, 225);  // Roztočit
    frames(90);
    shot("18d_store_wheel");
    frames(240);
    shot("18e_store_wheel_done");
    tap(30, 44);  // Vše
    game.onBack();
    frames(1);
    tap(378, 149);  // Profil a skiny
    shot("19_skins");
    game.onBack();
    frames(1);
    tap(300, 177);  // Balíčky a doplňky
    shot("05_packs");
    tap(426, 168);  // Upravit soubory JSON
    shot("06_pack_files");
    tap(200, 156);  // dimensions/nether.json
    shot("07_json_editor");
    game.onBack();
    game.onBack();
    game.onBack();
    frames(1);
    tap(222, 121);  // Hrát
    shot("08_worlds_empty");
    tap(164, 245);  // Nový svět
    game.onTextResult(1, "Můj první svět");
    frames(1);
    tap(560, 86);   // show coordinates
    shot("09_create_world");
    tap(216, 245);  // Vytvořit a hrát
    frames(4);
    shot("09b_loading_world");
    frames(30);
    // Portal travel shows the purple loading screen.
    game.session()->travelled = true;
    frames(3);
    shot("09c_loading_dimension");
    frames(70);
    shot("10_survival_hud");
    game.onTextResult(14, "Ahoj, kdo jde těžit?");
    game.onTextResult(14, "/seed");
    game.onTextResult(14, "/give ruby 3");
    game.onTextResult(14, "/help");
    frames(2);
    shot("10b_chat");
    {
        Inventory& inv = game.session()->inventory();
        inv.slots[0] = {WOOD_PICKAXE, 1, 20};
        inv.slots[1] = {LOG, 12};
        inv.slots[2] = {COBBLESTONE, 30};
        inv.slots[3] = {COAL, 7};
        inv.slots[10] = {IRON_ORE, 5};
        inv.slots[11] = {STICK, 8};
        inv.slots[20] = {PLANKS, 20};
    }
    tap(390, 258);  // "..." inventory button
    frames(2);
    shot("11_survival_inventory");
    tap(181, 215);  // pick the 12 logs in hotbar slot 2
    tap(374, 105);  // Půlka
    shot("11b_inventory_split");
    tap(181, 171);  // put 6 into an empty slot
    tap(400, 163);  // Zrušit výběr
    game.onBack();
    frames(1);
    {
        Session* s = game.session();
        s->wantsCrafting = true;
        frames(2);
        shot("12_crafting_table");
        tap(287, 60);  // Recepty: the recipe book beside the grid
        frames(2);
        shot("12b_recipe_book");
        tap(287, 60);
        game.onBack();
        frames(1);
        Vec3 p = s->player().pos;
        int x = (int)p.x + 2, y = (int)p.y, z = (int)p.z;
        s->currentWorld().setBlock(x, y, z, FURNACE);
        s->openFurnaceAt({0, x, y, z});
        Furnace* f = s->furnace(s->openFurnace);
        f->input = {IRON_ORE, 4};
        f->fuel = {COAL, 3};
        frames(1);
        for (int i = 0; i < 300; ++i) game.frame(t += 1 / 60.0);
        shot("13_furnace");
        game.onBack();
        frames(1);
        // 1.5.0: reading a page of the old documents, and the fortress compass in hand.
        s->readDocument(6);
        frames(2);
        shot("13b_document");
        game.onBack();
        frames(1);
        const ItemStack keep = s->inventory().slots[s->selectedSlot()];
        s->inventory().slots[s->selectedSlot()] = {FORTRESS_COMPASS, 1};
        frames(2);
        shot("13c_fortress_compass");
        s->inventory().slots[s->selectedSlot()] = keep;
        frames(1);
    }
    game.onBack();  // pause
    frames(2);
    shot("14_pause");
    tap(300, 184);  // save & quit
    tap(222, 121);
    shot("15_worlds_list");
    tap(530, 44);   // E: edit world
    shot("20_edit_world");
    // A creative world: pause gets "Struktury", the Structures screen places a built-in house.
    game.onBack();
    frames(1);
    tap(164, 245);  // Nový svět
    game.onTextResult(1, "Stavby");
    frames(1);
    tap(230, 92);   // Kreativní
    tap(216, 245);  // Vytvořit a hrát
    frames(30);
    game.onBack();
    frames(2);
    shot("21_pause_creative");
    tap(300, 142);  // Struktury
    tap(164, 222);  // Otočit: 90°
    shot("22_structures");
    tap(436, 222);  // Umístit před sebe: a house on the map
    frames(70);
    shot("23_hud_minimap");
    {
        // 1.4.0: the map item in hand shows the paper sheet drawn so far.
        Session* ms = game.session();
        ItemStack keep = ms->inventory().slots[ms->selectedSlot()];
        ms->inventory().slots[ms->selectedSlot()] = {MAP, 1};
        frames(90);
        shot("23c_hud_map_item");
        ms->inventory().slots[ms->selectedSlot()] = keep;
        frames(2);
    }
    game.onTextResult(14, "Hi! Let's build a castle here");
    game.onTextResult(14, "/give ruby 5");
    game.onTextResult(14, "/time noon");
    game.onTextResult(14, "/seed");
    frames(2);
    shot("23b_chat_commands");
    tap(390, 258);  // "..." creative inventory
    frames(2);
    shot("24_creative_building");
    tap(65, 39);    // 1.5.9 icon tab "Sklo"
    shot("24b_creative_glass");
    for (int i = 0; i < 3; ++i) tap(114 + i * 20, 36);  // the groups open on a tap
    shot("24c_creative_glass_group");
    tap(39, 117);   // tab "Nástroje a zbraně"
    tap(94, 36);    // first tool into the hotbar
    shot("25_creative_tools");
    tap(65, 117);   // tab "Brnění"
    shot("26_creative_armor");
    game.onBack();
    frames(2);
    // Developer login and menu.
    game.onTextResult(10, "Hoppy_Hopsy21");
    frames(1);
    game.onTextResult(11, std::getenv("VC_DEV_PASSWORD") ? std::getenv("VC_DEV_PASSWORD") : "");
    frames(2);
    game.onBack();  // pause
    frames(2);
    shot("27_pause_developer");
    tap(300, 65 + 3 * 28);  // Vývojář (4th of 6 buttons in creative)
    frames(2);
    shot("28_developer_menu");
    game.onBack();
    frames(2);
    tap(300, 65 + 4 * 28);  // Otevřít pro LAN
    frames(2);
    shot("29_pause_lan_host");
    tap(300, 65 + 4 * 28);  // the room code button: invite players
    frames(2);
    shot("29b_invite");
    game.onBack();
    frames(2);
    tap(300, 65 + 5 * 28);  // Uložit a odejít
    frames(2);
    {
        // LAN: someone on the network opens a world; it shows up in the world list.
        WorldInfo hostInfo;
        hostInfo.folder = createWorldFolder((data / "host_worlds").string());
        hostInfo.name = "Ostrov";
        hostInfo.seed = 77;
        hostInfo.mode = GameMode::Survival;
        Session hostWorld(hostInfo, PlayerState{});
        Multiplayer host;
        std::string err;
        std::vector<uint8_t> hostSkin(64 * 64 * 4, 0);
        for (size_t i = 0; i < hostSkin.size(); i += 4) {
            hostSkin[i] = 40; hostSkin[i + 1] = 90; hostSkin[i + 2] = 200; hostSkin[i + 3] = 255;
        }
        host.host(hostWorld, "Ostrov", "Kamarád", hostSkin, false, err);
        Settings hs;
        hs.renderDistance = 2;
        auto lanFrames = [&](int n) {
            for (int i = 0; i < n; ++i) {
                t += 1 / 60.0;
                hostWorld.update(1 / 60.0f, t, 600, 270, hs);
                host.update(&hostWorld, 1 / 60.0f, t);
                game.frame(t);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        tap(222, 121);  // Hrát
        lanFrames(120);
        tap(182, 15);  // the Multiplayer tab
        lanFrames(4);
        shot("33_worlds_lan");
        tap(508, 70);  // "No internet?": the hotspot guide
        lanFrames(2);
        shot("33b_offline_guide");
        game.onBack();
        lanFrames(2);
        tap(300, 45);  // the LAN game (first row)
        lanFrames(2);
        shot("34_joining");
        for (int i = 0; i < 300 && !game.session(); ++i) lanFrames(1);
        lanFrames(60);
        if (game.session()) {
            // The host stands a few steps in front of us.
            Player& me = game.session()->player();
            me.yaw = 0;
            me.pitch = -0.1f;
            hostWorld.player().pos = me.pos + Vec3(0.5f, 0, -5);
            hostWorld.player().yaw = kPi;
            lanFrames(60);
        }
        shot("35_lan_client");
        game.onBack();
        frames(2);
        shot("36_pause_lan_client");
        tap(300, 89 + 3 * 28 + 4);  // Odpojit se (last of 4 buttons with developer access)
        frames(2);
        host.stop();
        game.onBack();  // world list -> title
        frames(2);
    }
    // The title screen and settings in every language.
    const char* codes[] = {"en", "cs", "es", "ru", "de"};
    for (int i = 0; i < 5; ++i) {
        setLanguage((Language)i);
        frames(2);
        shot((std::string("30_title_") + codes[i]).c_str());
    }
    setLanguage(Language::German);
    tap(378, 121);  // Settings
    tap(103, 169);  // Profile tab
    shot("31_settings_profile_de");
    setLanguage(Language::Russian);
    tap(103, 91);   // Video tab
    shot("32_settings_video_ru");
    setLanguage(Language::Czech);
    fs::remove_all(data);
    return 0;
}
