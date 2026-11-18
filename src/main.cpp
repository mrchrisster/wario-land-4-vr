#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "mobile_platform.h"
#include "runtime.h"
#include "gba_bus.h"
#include "runtime_bus_bridge.h"

#include <cstdint>
#include <unordered_map>

#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

#ifndef GBARECOMP_BUILTIN_NAME
#define GBARECOMP_BUILTIN_NAME "GBA cartridge"
#endif
#ifndef GBARECOMP_BUILTIN_SHA1
#define GBARECOMP_BUILTIN_SHA1 ""
#endif
#ifndef GBARECOMP_BUILTIN_CRC32
#define GBARECOMP_BUILTIN_CRC32 0
#endif
#ifndef GBARECOMP_BUILTIN_REGION
#define GBARECOMP_BUILTIN_REGION ""
#endif
#ifndef GBARECOMP_DEFAULT_GAME_CONFIG
#define GBARECOMP_DEFAULT_GAME_CONFIG "game.toml"
#endif

// Shared Quest-VR runtime defaults to Mother 3's policy of clamping world sprites to
// Plane 1. Wario Land 4 keeps sprites on their own GBA priority plane.
extern "C" int g_layered_unify_objects;
// WL4 depth rules (shared runtime, off by default): split equal-priority BGs by BG index
// (cutscenes put every BG at priority 0) and push slower-scrolling BGs back by parallax.
extern "C" int g_layered_split_ties;
extern "C" int g_layered_obj_forward;
extern "C" int g_layered_obj_plane;
extern "C" int g_layered_flat_bitmap;
extern "C" int g_layered_sprite_layer;
extern "C" void (*g_layered_bg_split_row)(int bg, int y, const uint8_t* io, const uint8_t* vram,
                                          uint8_t* out, int width);
extern "C" int g_layered_fog_alpha;
extern "C" float g_layered_split_push;
extern "C" float g_layered_sprite_depth;
extern "C" float g_layered_parallax_depth;

namespace {

void set_environment_default(const char* name, const char* value) {
    if (std::getenv(name) != nullptr) return;
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 0);
#endif
}

void print_usage() {
    std::printf(
        "WarioLand4Recomp [--bios <path>] [--rom <path>] "
        "[game.toml]\n"
        "The BIOS and ROM must match the SHA-1 identities in game.toml.\n"
                "Windowed play opens the recomp-ui launcher first; --no-launcher "
        "skips it once.\n"
        "Static recompilation is the default; set GBARECOMP_FORCE_INTERP=1 "
        "for the reference interpreter diagnostic backend.\n");
}

}  // namespace

// Wario Land 4: a gameplay layer often has decoration painted into it (Wildflower Fields' pink cave
// wall around the mossy ledges). The game's own Map16 terrain table (tileset entry +24, one byte per
// 16x16 tile, 0 = no collision) tells them apart: decoration pixels of a row with terrain in it go
// one plane further back, solid terrain stays.
namespace {
struct Wl4Terrain {
    int tileset = -1;
    std::unordered_map<uint64_t, uint8_t> by_tiles;  // four 8x8 tilemap entries -> terrain id
};
Wl4Terrain g_wl4_terrain;

inline uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
inline uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

void wl4_split_row(int bg, int y, const uint8_t* io, const uint8_t* vram, uint8_t* out, int width) {
    gba::GbaBus* bus = gbarecomp::active_bus();
    if (!bus || !bus->rom_ptr()) return;
    const uint8_t* rom = bus->rom_ptr();
    const int ts = bus->iwram_ptr()[0x74];  // current room header copy: tileset id
    if (ts >= 0x60) return;
    if (ts != g_wl4_terrain.tileset) {
        g_wl4_terrain.tileset = ts;
        g_wl4_terrain.by_tiles.clear();
        const uint32_t tp = 0x3F2298u + ts * 36u;
        const uint32_t m16 = rd32(rom + tp + 0x14) & 0x1FFFFFFu, terr = rd32(rom + tp + 24) & 0x1FFFFFFu;
        if (m16 + 0x300u * 8u > 0x800000u || terr + 0x300u > 0x800000u) return;
        for (uint32_t i = 0; i < 0x300u; ++i) {
            const uint8_t* e = rom + m16 + i * 8u;
            const uint64_t key = rd16(e) | (uint64_t(rd16(e + 2)) << 16) | (uint64_t(rd16(e + 4)) << 32) |
                                 (uint64_t(rd16(e + 6)) << 48);
            g_wl4_terrain.by_tiles.emplace(key, rom[terr + i]);
        }
    }
    if (g_wl4_terrain.by_tiles.empty()) return;
    const uint16_t cnt = rd16(io + 0x08 + 2 * bg);
    const uint32_t sb = ((cnt >> 8) & 31u) * 0x800u, size = (cnt >> 14) & 3u;
    const uint32_t mw = (size & 1u) ? 512u : 256u, mh = (size & 2u) ? 512u : 256u;
    const uint32_t hofs = rd16(io + 0x10 + 4 * bg) & 0x1FFu, vofs = rd16(io + 0x12 + 4 * bg) & 0x1FFu;
    auto entry = [&](uint32_t px, uint32_t py) -> uint16_t {
        px &= mw - 1u; py &= mh - 1u;
        const uint32_t blk = (px >> 8) + (py >> 8) * (mw == 512u ? 2u : 1u);
        return rd16(vram + sb + blk * 0x800u + (((py & 255u) >> 3) * 32u + ((px & 255u) >> 3)) * 2u);
    };
    const uint32_t py = (static_cast<uint32_t>(y) + vofs) & (mh - 1u);
    const uint32_t cy = py & ~15u;
    const bool bpp8 = (cnt & 0x80u) != 0;
    const uint32_t cb = ((cnt >> 2) & 3u) * 0x4000u;
    // Colour statistics per BG (palette bank << 4 | colour index; 8bpp: the index): how often a
    // colour shows in terrain tiles vs decoration tiles. Collected this frame, used next frame.
    static uint32_t hist_now[4][2][256], hist_prev[4][2][256];
    static bool solid_now[4] = {}, solid_prev[4] = {};
    // Per colour: 1 = belongs to the painted background, 2 = belongs to the terrain art.
    // Clear-cut colours by their counts; in-between ones (a dark shade under a ledge) take the
    // class of the nearest clear-cut colour by RGB, so a wall shadow joins the wall.
    static uint8_t colour_class[4][256];
    static int stats_last_y[4] = {-1, -1, -1, -1};
    const bool new_frame = y <= stats_last_y[bg];
    stats_last_y[bg] = y;
    if (new_frame) {
        std::memcpy(hist_prev[bg], hist_now[bg], sizeof(hist_now[bg]));
        std::memset(hist_now[bg], 0, sizeof(hist_now[bg]));
        solid_prev[bg] = solid_now[bg]; solid_now[bg] = false;
        const uint8_t* pal = bus->pal_ptr();
        auto rgb = [&](int c, int* r, int* g, int* b2) {
            const uint16_t v = rd16(pal + (bpp8 ? c : c) * 2);
            *r = v & 31; *g = (v >> 5) & 31; *b2 = (v >> 10) & 31;
        };
        for (int c = 0; c < 256; ++c) {
            const uint32_t dc = hist_prev[bg][0][c], sc = hist_prev[bg][1][c];
            colour_class[bg][c] = dc > 4u * sc + 8u ? 1 : (sc > 4u * dc + 8u ? 2 : 0);
        }
        int seed_rgb[256][3]; uint8_t seed_cls[256]; int seeds = 0;
        for (int k = 0; k < 256; ++k) {
            if (!colour_class[bg][k] || !(hist_prev[bg][0][k] + hist_prev[bg][1][k])) continue;
            rgb(k, &seed_rgb[seeds][0], &seed_rgb[seeds][1], &seed_rgb[seeds][2]);
            seed_cls[seeds++] = colour_class[bg][k];
        }
        for (int c = 0; c < 256; ++c) {
            if (colour_class[bg][c] || !(hist_prev[bg][0][c] + hist_prev[bg][1][c])) continue;
            int r, g, b2; rgb(c, &r, &g, &b2);
            int best = 1 << 30; uint8_t cls = 0;
            for (int k = 0; k < seeds; ++k) {
                const int dr = r - seed_rgb[k][0], dg = g - seed_rgb[k][1], db = b2 - seed_rgb[k][2];
                const int dd = dr * dr + dg * dg + db * db;
                if (dd < best) { best = dd; cls = seed_cls[k]; }
            }
            colour_class[bg][c] = cls;
        }
    }
    // Per-frame cache of the terrain id of each 16x16 map cell (a 512x512 map has 32x32 cells).
    static int16_t cell_cache[4][32 * 32];
    static uint32_t cell_gen[4][32 * 32], frame_gen[4] = {1, 1, 1, 1};
    static int last_y[4] = {-1, -1, -1, -1};
    if (y <= last_y[bg]) ++frame_gen[bg];  // a new frame for this BG (rows only go down within one)
    last_y[bg] = y;
    auto terrain_at = [&](uint32_t cx, uint32_t cyy) -> int {  // -1 unknown, 0 decoration, >0 terrain
        const uint32_t ci = ((cyy >> 4) & 31u) * 32u + ((cx >> 4) & 31u);
        if (cell_gen[bg][ci] == frame_gen[bg]) return cell_cache[bg][ci];
        cell_gen[bg][ci] = frame_gen[bg];
        const uint64_t key = entry(cx, cyy) | (uint64_t(entry(cx + 8, cyy)) << 16) |
                             (uint64_t(entry(cx, cyy + 8)) << 32) | (uint64_t(entry(cx + 8, cyy + 8)) << 48);
        auto it = g_wl4_terrain.by_tiles.find(key);
        const int t = it == g_wl4_terrain.by_tiles.end() ? -1 : it->second;
        cell_cache[bg][ci] = static_cast<int16_t>(t);
        return t;
    };
    auto colour_at = [&](uint32_t px) -> int {  // -1 transparent
        const uint16_t e = entry(px, py);
        const uint32_t tx = (e & 0x400u) ? 7u - (px & 7u) : (px & 7u);
        const uint32_t ty = (e & 0x800u) ? 7u - (py & 7u) : (py & 7u);
        if (bpp8) {
            const uint32_t addr = cb + (e & 0x3FFu) * 64u + ty * 8u + tx;
            if (addr >= 0x10000u) return -1;
            const int c = vram[addr];
            return c ? c : -1;
        }
        const uint32_t addr = cb + (e & 0x3FFu) * 32u + ty * 4u + (tx >> 1);
        if (addr >= 0x10000u) return -1;
        const int c = (tx & 1u) ? (vram[addr] >> 4) : (vram[addr] & 15);
        return c ? static_cast<int>(((e >> 12) << 4) | c) : -1;
    };
    static uint8_t deco[1300];
    int cell_terrain = -1, above_terrain = -1;
    uint32_t cell_cx = 0xFFFFFFFFu;
    for (int x = 0; x < width; ++x) {
        const uint32_t px = (static_cast<uint32_t>(x) + hofs) & (mw - 1u);
        const uint32_t cx = px & ~15u;
        if (cx != cell_cx) {
            cell_cx = cx;
            cell_terrain = terrain_at(cx, cy);
            above_terrain = terrain_at(cx, (cy + mh - 16u) & (mh - 1u));
            if (cell_terrain > 0) solid_now[bg] = true;
        }
        deco[x] = 0;
        if (cell_terrain < 0) continue;  // not a tile of this tileset: leave it alone
        if (!solid_prev[bg] && (y & 7) != 0) continue;  // not split: sample colours on every 8th row only
        const int c = colour_at(px);
        if (c < 0) continue;
        hist_now[bg][cell_terrain > 0 ? 1 : 0][c]++;
        const uint8_t cls = colour_class[bg][c];
        if (cell_terrain > 0) {
            // Inside a terrain tile: background-coloured pixels (the pink wall and its shadow
            // behind the moss drips) go back with the background.
            deco[x] = cls == 1 ? 1 : 0;
        } else {
            // Decoration tile right under terrain: terrain-coloured pixels (drip tips hanging
            // into the next tile row) stay with the ledge.
            deco[x] = (above_terrain > 0 && cls == 2) ? 0 : 1;
        }
    }
    // Split a BG only when the previous frame showed terrain in it (pure scenery layers such as
    // the back wall or the sky stay whole; decided per frame, not per row, to avoid stripes).
    if (!solid_prev[bg]) return;
    for (int x = 0; x < width; ++x) out[x] = deco[x];
}
}  // namespace

int wl4_main(int argc, char** argv) {
    std::vector<std::string> args(argv, argv + argc);
    // Android: private-storage layout, log file, staged game TOML and no
    // pre-boot launcher (shared gbarecomp mobile setup). No-op on desktop.
    gbarecomp::MobileProcessOptions mobile;
    mobile.game_config = GBARECOMP_DEFAULT_GAME_CONFIG;
    mobile.program_name = "./WarioLand4Recomp";
    gbarecomp::mobile_prepare_process(args, mobile);
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }


    g_layered_unify_objects = 0;
#if defined(GBARECOMP_QUEST_XR)
    // Same as the Mother 3 Quest port: OpenSL ES instead of SDL's Java AudioTrack driver,
    // whose thread stalls (>40 ms) underrun the buffer and break up the sound.
    setenv("SDL_AUDIODRIVER", "openslES", 0);
#endif
    g_layered_split_ties = 1;
    // Sprites get their own plane (Christoph prefers this to moving them forward).
    // Wario and the other world sprites live on a separate sixth layer (not a GBA plane);
    // the GBA's own 5 planes keep the scenery. Depth in plane units (3 = terrain, 4 = front).
    g_layered_sprite_layer = 1;
    g_layered_sprite_depth = -1.0f;  // automatic: behind every BG the GBA draws in front of Wario, in front of the rest
    g_layered_fog_alpha = 1;
    // Split decoration from terrain inside a gameplay layer (game terrain table).
    g_layered_bg_split_row = wl4_split_row;
    g_layered_split_push = 0.0f;  // (0.12 tried 2026-11-17: Christoph asked to revert it)
    if (const char* e = std::getenv("WL4_SPLIT_PUSH")) g_layered_split_push = static_cast<float>(std::atof(e));
    if (const char* e = std::getenv("WL4_SPLIT_DECOR"); e && *e == '0') g_layered_bg_split_row = nullptr;  // fog/water stay their own see-through layer
    if (const char* e = std::getenv("WL4_FOG_ALPHA")) g_layered_fog_alpha = std::atoi(e);
    if (const char* e = std::getenv("WL4_SPRITE_LAYER")) g_layered_sprite_layer = std::atoi(e);
    if (const char* e = std::getenv("WL4_SPRITE_DEPTH")) g_layered_sprite_depth = static_cast<float>(std::atof(e));
    g_layered_obj_plane = g_layered_sprite_layer ? 0 : 2;  // older own-plane rule as fallback
    g_layered_obj_forward = 0;
    g_layered_flat_bitmap = 1;  // intro tunnel etc.: keep bitmap cutscenes whole
    if (const char* e = std::getenv("WL4_FLAT_BITMAP")) g_layered_flat_bitmap = std::atoi(e);
    if (const char* e = std::getenv("WL4_OBJ_PLANE")) g_layered_obj_plane = std::atoi(e);
    if (const char* e = std::getenv("WL4_OBJ_FORWARD")) g_layered_obj_forward = std::atoi(e);
    g_layered_parallax_depth = 1.0f;
    if (const char* e = std::getenv("WL4_PARALLAX_DEPTH")) g_layered_parallax_depth = static_cast<float>(std::atof(e));
    if (const char* e = std::getenv("WL4_SPLIT_TIES")) g_layered_split_ties = std::atoi(e);
    gbarecomp::RunOptions opts;
    opts.builtin_game_name = GBARECOMP_BUILTIN_NAME;
    opts.builtin_rom_sha1 = GBARECOMP_BUILTIN_SHA1;
    opts.builtin_rom_crc32 = GBARECOMP_BUILTIN_CRC32;
    opts.launcher_region = GBARECOMP_BUILTIN_REGION;
    opts.launcher_game_config = GBARECOMP_DEFAULT_GAME_CONFIG;
    opts.launcher_save_path =
        "saves/wario_land_4.sav";

#if defined(__ANDROID__)
    opts.ui_touch_friendly = true;
    // Quest: a session the OS suspended (or a state staged as the suspend state) resumes on launch.
    opts.resume_suspend_state_on_launch = true;
    opts.resume_newest_state_on_launch = true;
    opts.snapshot_on_battery_flush = true;  // in-game saves also refresh the resume state
    // Local debug observer (Quest-local 127.0.0.1:19880, reached via adb forward): lets
    // tools/quest_update.sh save the running game before installing a new build.
    setenv("GBARECOMP_TCP_OBSERVE", "19880", 0);
    // Busy scenes ran late on the Quest (music dropped in pitch/tempo): keep the game thread on
    // the performance cores, ask OpenXR for sustained-high CPU clocks, and skip the background
    // diagnostic replay recorder (state hashing + checkpoint copies every frame).
    setenv("GBARECOMP_BIG_CORES", "1", 0);
    setenv("GBARECOMP_XR_CPU_HIGH", "1", 0);
    setenv("GBARECOMP_DIAGNOSTICS", "0", 0);
    // Keep the screen distance / size the player last set with the grips.
    setenv("GBARECOMP_XR_REMEMBER_VIEW", "1", 0);
    // VR options menu on the Menu button (save/load state, 3D depth, background, screen); the stick
    // clicks no longer switch the 3D mode.
    setenv("GBARECOMP_XR_MENU", "1", 0);
    setenv("GBARECOMP_XR_TITLE", "WARIO LAND 4 VR", 0);
    // Layers get 24 GBA px of edge-extended margin; dark diorama walls run from the play layers
    // back to the far layers (the front frame of the first shadow box was dropped).
    setenv("GBARECOMP_XR_SHADOWBOX", "24", 0);
    setenv("GBARECOMP_XR_BOXWALLS", "0", 0);
    setenv("GBARECOMP_XR_WINDOW", "0", 0);
    setenv("GBARECOMP_XR_DARK", "1", 0);
#endif

#if defined(GBAGAME_RECOMP_UI)
    if (game_launcher_preboot(args, opts)) return 0;
#endif
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
}

#if defined(__ANDROID__)
extern "C" int SDL_main(int argc, char** argv) {
    // Desktop-sized host stack for recompiled code (see mobile_platform.h).
    return gbarecomp::mobile_run_with_stack(wl4_main, argc, argv);
}
#else
int main(int argc, char** argv) {
    return wl4_main(argc, argv);
}
#endif
