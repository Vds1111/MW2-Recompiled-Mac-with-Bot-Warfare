# MW2 Macbook Recomp — Bot Warfare 2.4.0

MULTIPLAYER AND CONTROLLER ONLY!!!
## YOU MUST FIRST INSTALL HOMEBREW AND MOLTENVK
```bash
brew install cmake ninja molten-vk
```
Builds a standalone macOS `Modern Warfare 2.app` (Title Update 6 Multiplayer) with **Bot Warfare 2.4.0** from your own legal Xbox 360 copy. **No copyrighted game files are included.**
Added support for third party controllers like PowerA

## What You Need
1. **Apple Silicon Mac (macOS 15+)** with Xcode Command Line Tools (`xcode-select --install`), `cmake`, `ninja`, and MoltenVK.
2. **Base Game:** Your Xbox 360 **Modern Warfare 2** `.iso` (or unpacked disc folder containing `default_mp.xex`).
3. **Title Update 6:** Your Xbox 360 TU6 file (`TU_10LC20N_...`).

## If macOS blocks MW2 Bot Warfare Builder.app:
Right-click the app and choose Open, or run xattr -cr "MW2 Bot Warfare Builder.app" in Terminal.

## Easy GUI Build (Recommended)

**Step 0.** EXTRACT **`SRC.ZIP`** BEFORE RUNNING IT

**Step 1.** Double-click **`MW2 Bot Warfare Builder.app`**.

**Step 2.** Choose your MW2 `.iso` file or unpacked disc folder.

**Step 3.** Select your Title Update 6 (`TU_10LC20N_...`) file.

**Step 4.** Wait for Terminal to finish building, then double-click **`Modern Warfare 2.app`** on your Desktop and press **D-Pad Down** in a Private Match to open the Bot Warfare menu.

## CLI Build (Alternative)
```bash
./build-mac-app.sh /path/to/MW2.iso /path/to/TU_10LC20N_file 
```
## Managing settings
For Bot Warfare: Press DPad down, pretty easy. To manage other change like changing fps, anti-aliasing, etc, press RB+START. The Bot settings in the RB+START Menu will not work so don't be bothered to use it, use Bot Warfare instead ;)

## Why?
Because the Bots only did basic stuffs, like moving, ads, crouch. They lacked killstreaks or grenades or special grenades. For that, I had to add Bot Warfare to it (Thanks to Claude Ai). 

## Credits
* **GSC Loader & TU6 Hooks:** Ported from `kyokokxdev/mw2nx` (`mw2mp_gsc.cpp`).
* **Bot Warfare 2.4.0:** Created by the Bot Warfare team (see `mods/bot_warfare/CREDITS.md` and `LICENSE`).
* **Base Recompilation:** `vrcnx/mw2-recompiled-ios` (`ios-port` branch, forked from `PaulCombal/mw2-recompiled`).

