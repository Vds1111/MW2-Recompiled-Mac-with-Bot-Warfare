# MW2 Macbook Recomp With Bot Warfare 2.4.0

Builds a standalone macOS `Modern Warfare 2.app` (Title Update 6 Multiplayer) with **Bot Warfare 2.4.0** from your own legal Xbox 360 copy. **No copyrighted game files are included.**
Added support for third party controllers like PowerA. **THIS IS MULTIPLAYER AND CONTROLLERS ONLY! NO SINGLEPLAYER AND KEYBOARD SUPPORT!**

## YOUR SAVE NOW GOES INTO Document/MW2 Save so it doesn't get erased

## Status: Beta — Expect occasional bugs or crashes. Core multiplayer, Bot Warfare 2.4.0, and controller support are fully playable on Apple Silicon Macs, but occasional visual or stability quirks may still occur.
## INSTALLATION

## What You Need
1. **Apple Silicon Mac (macOS 15+)** with Xcode Command Line Tools (`xcode-select --install`), `cmake`, `ninja`, and MoltenVK.
2. **Base Game:** Your Xbox 360 **Modern Warfare 2** `.iso` (or unpacked disc folder containing `default_mp.xex`).
3. **Title Update 6:** Your Xbox 360 TU6 file (`TU_10LC20N_...`).

## You must first install Homebrew and MoltenVK
```bash
brew install cmake ninja molten-vk vulkan-loader
```

## Easy GUI Build (Recommended)

**Step 1.** First extract **`src.zip`**.

**Step 2.** Extract **`MW2 Bot Warfare Builder.app.zip`**, then double-click **`MW2 Bot Warfare Builder.app`**.

(If macOS blocks MW2 Bot Warfare Builder.app:
Right-click the app and choose Open, or run xattr -cr "MW2 Bot Warfare Builder.app" in Terminal.)

**Step 3.** Choose your MW2 `.iso` file or unpacked disc folder.

**Step 4.** Select your Title Update 6 (`TU_10LC20N_...`) file.

**Step 5.** Wait for Terminal to finish building, then double-click **`Modern Warfare 2.app`** on your Desktop and press **D-Pad Down** in a Private Match to open the Bot Warfare menu.

## CLI Build (Alternative)
```
bash ./build-mac-app.sh /path/to/MW2.iso /path/to/TU_10LC20N_file
```

## Loading a map
Loading a map for the first time can be quite laggy when adding bots, to fix this, just restart and it should be back to normal!

## Managing settings
For Bot Warfare: Press DPad down, pretty easy. To manage other change like changing fps, anti-aliasing, etc, press RB+START. The Bot settings in the RB+START Menu will not work so don't be bothered to use it, use Bot Warfare instead ;)

## Tip: Keep the FPS cap at 60 FPS (default) for maximum stability — 90/120 FPS can cause GPU ring-buffer crashes during heavy or long sessions.

## Why add Bot Warfare?
The Bots in settings RB+START only do basic stuffs, like moving, ads, crouch. They could not do killstreaks or throw grenades or special grenades. On Domination and Headquarter, they would not grab or defend the flags. So for that, I had to add Bot Warfare to it (Thanks to Claude Ai). 

## Credits
* **GSC Loader & TU6 Hooks:** Ported from `kyokokxdev/mw2nx` (`mw2mp_gsc.cpp`).
* **Bot Warfare 2.4.0:** Created by the Bot Warfare team (see `mods/bot_warfare/CREDITS.md` and `LICENSE`).
* **Base Recompilation:** `vrcnx/mw2-recompiled-ios` (`ios-port` branch, forked from `PaulCombal/mw2-recompiled`).

## License & Usage
You are free to use, modify, and share this project as long as you give proper credit to this repository and me and the original authors listed above (`kyokokxdev/mw2nx`, for the GSC Loader and TU6, the Bot Warfare team, and `vrcnx/mw2-recompiled-ios`, forked from `PaulCombal/mw2-recompiled`).
