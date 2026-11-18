# Wario Land 4 VR

Wario Land 4 for the Game Boy Advance, played in layered stereoscopic depth on Meta Quest, the way Virtual Boy Wario Land looked in 1995.

[![Wario Land 4 VR gameplay on Meta Quest](https://img.youtube.com/vi/fdBW9pHtGyo/maxresdefault.jpg)](https://youtu.be/fdBW9pHtGyo)

*Click to watch the gameplay video on YouTube.*

This is an unofficial fan project. No ROM, game code, graphics or save data is included. You need your own legally owned copy of **Wario Land 4 (USA, Europe)** (`sha1 b9fe05a8080e124b67bce6a623234ee3b518a2c1`) and you build everything yourself.

## Why this exists

The Virtual Boy is remembered as Nintendo's big flop, and most of its library deserves that. Virtual Boy Wario Land is the exception. It is the one game on the system that really works, and it works because of the 3D: Wario runs along a foreground, jumps into the background, and the stage is a stack of flat layers at different distances. The depth isn't a gimmick on top of the platforming, it is how you read the level.

That game never got a sequel in the same style. Wario Land 4 is the best 2D Wario game, and its levels are already drawn as separate hardware layers: a far parallax sky, a back wall, the terrain Wario walks on, and see-through foreground covers like water, fog and hidden-wall overlays. This port takes those layers apart and puts each one at its own distance in a headset, so Wario Land 4 plays like the Virtual Boy game could have looked with a full colour palette and a proper screen.

If you know Wario Land 4 well, it's a new way to see levels you already know. Secret walls read as a layer floating in front of the path, the parallax skies sit far behind the room, platforms stand out from the decoration around them, and Wario has his own depth between the back wall and the scenery the GBA draws over him.

The same idea was applied earlier to another Wario title: **[WarioWare: Twisted! VR](https://github.com/mrchrisster/WarioWareTwistedRecomp)**.

## How it works

The game is not emulated. It is statically recompiled with [gbarecomp](https://github.com/mstan/gbarecomp): the ARM code from your ROM is translated into C++ once and compiled into a native Mac or Quest app. The GBA picture unit (PPU) is reimplemented in that runtime, and that is where the depth comes from. Instead of mixing the four backgrounds and the sprites into one 240x160 image, the renderer writes each piece onto its own plane, and the Quest app draws those planes as separate OpenXR layers at different distances.

Every plane recomposites to the exact GBA frame (0 pixel difference at all 18 level starts), so viewed straight on the game looks like the original. The depth only appears when you look with two eyes or move your head.

## What's changed compared with the stock recomp

The stock gbarecomp Quest layer mode puts each GBA priority on a plane. Wario Land 4 needed more than that:

- **Wario's own depth layer.** Wario and the other world sprites are routed out of the background planes into a separate sprite layer. Its depth is chosen per scanline: in front of the back wall and parallax, behind whatever terrain, fog or foreground the GBA draws over him. If scenery behind him cuts through a sprite, the whole sprite moves back together, so he's never split across two layers.
- **Parallax depth from scroll speed.** A background that scrolls at half speed is pushed back about twice as far, so the sky sits well behind the room instead of on the next plane.
- **See-through fog and water.** Translucent layers (mist, water, rain, hidden-wall covers) become their own premultiplied see-through plane instead of being baked into what's behind them, so Wario's feet and the doors behind the fog stay on their own layers.
- **Platforms lifted off the wall.** A Wario Land 4–specific hook reads the room's tileset and terrain table and separates walkable platforms from decoration pixel by pixel, so ledges stand in front of the wall they're drawn on.
- **Equal-priority ties split, bitmap cutscenes kept whole.** Backgrounds with the same priority are ranked by index onto separate planes; bitmap scenes like the intro tunnel stay on one plane so they don't fall apart.
- **Per-eye window clipping or shadow-box edges.** Pick in the menu. *Shadow box* (default) extends the far layers past the screen edge behind a dark frame, like looking into a box. *Window* clips every layer per eye to the front plane's rectangle, like looking through a window into the level.
- **VR options menu.** The left Menu button pauses and opens a panel with the game's logo (extracted from your ROM at setup, never shipped): resume, three save-state slots, 3D depth (normal / deep / flat), dark room or passthrough, recenter and screen-size reset. Screen distance and size are remembered between sessions.
- **Mac layer view.** A Mac build plays the game with the layers drawn as tilted planes you can orbit with the mouse, which is how the depth rules were tested without a headset (F10 cycles tilted / colour-coded / flat, Shift+F1–F9 save states).
- **Performance and audio on Quest.** Big cores and the XR sustained-high CPU level are requested, diagnostics are off, and audio uses OpenSL ES to avoid stalls. All 18 levels, including the HBlank raster effects, run as native code.

## Repository layout

| Path | What it is |
|---|---|
| `src/main.cpp` | Wario Land 4 policy: which depth rules are on, the platform/decoration split hook, Quest settings |
| `variants/wario_land_4/game.toml` | Recompiler config (code-copy blocks, extra functions found by self-heal) |
| `android/` | Quest app (Gradle, OpenXR) |
| `tools/` | Regenerate/build scripts, Mac layer view, headless runs, level tour, Quest update script, ROM analysis helpers |
| `external/gbarecomp`, `external/recomp-ui` | The runtime with the VR layer code (submodules, branch `wl4-vr`) |

`variants/wario_land_4/generated/` is created from your ROM when you build and is never committed.

## Building

You need macOS with Xcode command line tools, CMake, SDL2 (Homebrew), and for Quest, Android Studio with the NDK and an OpenXR loader.

```bash
git clone --recursive https://github.com/mrchrisster/wario-land-4-vr.git
cd wario-land-4-vr
mkdir -p variants/wario_land_4/roms
cp "/path/to/Wario Land 4 (USA, Europe).gba" variants/wario_land_4/roms/wario_land_4.gba
```

Configure the Mac build and generate the C++ from your ROM:

```bash
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release && tools/regen.sh
```

Play with the tilted layer view (point `WL4_ROM` at your ROM):

```bash
WL4_ROM=variants/wario_land_4/roms/wario_land_4.gba tools/mac_layer_view.sh
```

Build the Quest APK (your ROM is bundled into a private APK; don't share it):

```bash
cd android && ./gradlew :quest:assembleRelease -PprivateRom="/path/to/Wario Land 4 (USA, Europe).gba"
```

Install it with `adb install -r` and launch **Wario Land 4 VR** from Unknown Sources.

## Controls on Quest

The GBA buttons are on the Touch controllers. Left Menu opens the options menu (stick to select and change, A to confirm, B to close). Hold both grips and move to reposition the screen; right stick click recenters.

## Legal

Wario Land 4 is © Nintendo. This project is not affiliated with or endorsed by Nintendo. It contains no Nintendo code or assets: the ROM, the code generated from it, save files, save states and the menu logo all stay on your own machine. The gbarecomp runtime is under the PolyForm Noncommercial 1.0.0 license and recomp-ui under MIT; see their repositories.
