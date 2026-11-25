# MW2 Mac Recomp — Bot Warfare 2.4.0 + Optional DLC Builder

MULTIPLAYER ONLY!!!

Builds a standalone macOS `Modern Warfare 2.app` (Title Update 6 Multiplayer) with **Bot Warfare 2.4.0** and **optional Stimulus / Resurgence DLC map support** from your own legal Xbox 360 copy. **No copyrighted game files are included.**

## What You Need
1. **Apple Silicon Mac (macOS 15+)** with Xcode Command Line Tools (`xcode-select --install`), `cmake`, `ninja`, and MoltenVK.
2. **Base Game:** Your Xbox 360 **Modern Warfare 2** `.iso` (or unpacked disc folder containing `default_mp.xex`).
3. **Title Update 6:** Your Xbox 360 TU6 file (`TU_10LC20N_...`).
4. **DLC Map Packs (Optional):** Either:
   * The raw Xbox 360 Marketplace DLC containers (`53660E2B83E8E56D42073C826BED197261DA4EBB41` for Stimulus, `510551C98861C9E4379FF46F2D8ABA76F28521E341` for Resurgence, or the `41560617/00000002/` folder), **OR**
   * Already-extracted DLC map `.ff` files (`mp_complex.ff`, `mp_compact.ff`, `mp_crash.ff`, `mp_overgrown.ff`, `mp_storm.ff`, `mp_abandon.ff`, `mp_fuel2.ff`, `mp_strike.ff`, `mp_trailerpark.ff`, `mp_vacant.ff`).

## Easy GUI Build (Recommended)
1. Double-click **`MW2 Bot Warfare Builder.app`**.
2. **Step 1:** Choose your MW2 `.iso` file or unpacked disc folder.
3. **Step 2:** Select your Title Update 6 (`TU_10LC20N_...`) file.
4. **Step 3:** Choose **No DLC**, or click **Select DLC Folder** and point it at your Stimulus/Resurgence folder (raw Xbox 360 DLC packages are unpacked automatically).
5. Wait for Terminal to finish building, then double-click **`Modern Warfare 2.app`** on your Desktop and press **D-Pad Down** in a Private Match to open the Bot Warfare menu.

## CLI Build (Alternative)
```bash
./build-mac-app.sh /path/to/MW2.iso /path/to/TU_10LC20N_file /path/to/optional_dlc_folder
```

## Credits
* **GSC Loader & TU6 Hooks:** Ported from `kyokokxdev/mw2nx` (`mw2mp_gsc.cpp`).
* **Bot Warfare 2.4.0:** Created by the Bot Warfare team (see `mods/bot_warfare/CREDITS.md` and `LICENSE`).
* **Base Recompilation:** `vrcnx/mw2-recompiled-ios` (`ios-port` branch, forked from `PaulCombal/mw2-recompiled`).

